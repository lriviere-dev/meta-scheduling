#ifndef POLICY_H
#define POLICY_H


#include "MetaSolutions.h"
#include "ListMetaSolutions.h"
#include "Sequence.h"
#include "Schedule.h"
#include "Instance.h"
#include "PrefixTree.h"
#include <vector>
#include <deque>
#include <ilcp/cp.h>

#include <tuple>
#include <functional>


//struct to more easily group informations, used for trie (Info for a single scenario !)
struct ExtractedSequenceInfo { 
    Sequence sequence;
    std::vector<std::vector<int>> partial_groups; // The remaining groups after 'match_count'
    std::vector<int> partial_sequence_flat; // The flattened sequence of tasks from the *processed* groups
    std::vector<int> group_end_times; // Times at which each *processed* group ended
    PrefixTreeNode* last_node; // pointer to last common node
};


// The Policy handles the second decision stage. From Meta solution to Sequence to Schedule. It opperates within a scenario.
// WARNING : because of the current scope, some functions should be exclusive to "MAX-policies" (extract_sub_metasolution_index for example). small refactor is in order.
// WARNING : similarly, we require lexicographical order to be defined for the policy
class Policy {
public:

    mutable std::unordered_map<const DataInstance*, PrefixTree> prefix_trees_by_instance;

    virtual ~Policy() = default;

    // Pure virtual function to be implemented by derived policies
    virtual Sequence extract_sequence(MetaSolution& metaSolution, const DataInstance& instance, int scenario_id) const = 0;
    virtual ExtractedSequenceInfo  extract_sequence(MetaSolution& metaSolution, const DataInstance& instance, int scenario_id, int match_count, const std::vector<std::vector<int>>* seq_ptr, const std::vector<int>* times_ptr) const = 0;

    virtual bool isLexicographicallySmaller(const Sequence& seq1, const Sequence& seq2, const DataInstance& instance, int scenario_id) const = 0;
    virtual int extract_sub_metasolution_index(const MetaSolution& metaSolution, const DataInstance& instance, int scenario_id) const = 0;    
    
    //functions virtual but with default implementation
    //ERD will be shared by probably all policies at the sequence to schedule level (just scheduling asap while respecting sequence)
    virtual Schedule transform_to_schedule(const Sequence& sequence, const DataInstance& instance, int scenario_id) const {
        const std::vector<int>& tasks = sequence.get_tasks(); // Get tasks in sequence order
        const std::vector<int>& releaseDates = instance.releaseDates[scenario_id]; // Access release dates for tasks
        const std::vector<int>& durations = instance.durations; // Access task durations
       
        std::vector<int> startTimes(tasks.size());

        // Variable to track the current time
        int currentTime = 0;

        for (size_t i = 0; i < tasks.size(); ++i) {
            int task = tasks[i];
            // A task can only start after its release date and after the previous task finishes
            currentTime = std::max(currentTime, releaseDates[task]);
            startTimes[task] = currentTime; // Record the start time
            currentTime += durations[task]; // Advance current time by the task duration
        }

        return Schedule(startTimes); 
    }

    // also the way objective is computed ( for each scenario, the sum of end times)
    virtual void define_objective(IloEnv env, IloModel& model, 
                        IloIntervalVarArray2& jobs, const DataInstance& instance, 
                        IloIntExprArray& scenario_scores,  IloIntVar& aggregated_objective) const {
        //objective here is max sumci. Could be other.
        int nbScenarios = instance.S;
        int nbJobs = instance.N;
        IloIntExprArray completions(env, nbJobs);
        for (int s = 0; s < nbScenarios; s++) {
            for (int i = 0; i < nbJobs; i++) {
                completions[i] = IloEndOf(jobs[s][i]);
            }
            scenario_scores[s] = IloSum(completions);

        }

        // Aggregate objectives across scenarios
        for (int s = 0; s < nbScenarios; s++)
            model.add(aggregated_objective >= scenario_scores[s]);
        model.add(IloMinimize(env, aggregated_objective));
    }


    //Functions common to all Policies (i.e not virtual)
    int evaluate_meta(MetaSolution& metasol, const DataInstance& instance) {
        // same for all policies. just extract a schedule and evaluate it for all scenarios.
        // assume aggregator : max

        // Get or create the prefix tree for the current instance
        auto it = prefix_trees_by_instance.find(&instance);
        if (it == prefix_trees_by_instance.end()) {
            // Create a new PrefixTree for this instance if it doesn't exist
            it = prefix_trees_by_instance.emplace(&instance, PrefixTree()).first;
        }
        PrefixTree& tree = it->second; //fetch the prefix tree for this instance


        if (!metasol.scored_by){//metasol was not already scored -> score it and set front/scores for each scenario
            //special case if metasol is a list of metasol, we recursively have to make sure to evaluate the underlying before
            if (ListMetaSolutionBase* listMeta = dynamic_cast<ListMetaSolutionBase*>(&metasol)) {
                listMeta->front_indexes.resize(instance.S); //instanciate the indexes of front, is filled in "extract sequence"
                for (auto submeta : listMeta->get_meta_solutions()){
                    if (!submeta->scored_by){ //sub metasolution wasn't scored : evaluate it
                        this->evaluate_meta(*submeta,instance);
                    }
                    else if (submeta->scored_by!=this || submeta->scored_for!=&instance){ // else if it is scored but not by this policy, or not for this instance
                        submeta->reset_evaluation(); //reset it and then score again (via recursive call))
                        this->evaluate_meta(*submeta, instance);
                    }
                    //else do nothing, it is already scored appropriately, we can proceed
                }
            }

            //We have to score it, but maybe we can expediate the work (depending on the metasolution type)
            int match_count;
            const std::vector<std::vector<int>>* seq_ptr = nullptr;
            const std::vector<int>* times_ptr = nullptr;
            PrefixTreeNode* last_node = nullptr;

            //ALso declaring variables to store and cache
            std::vector<std::vector<std::vector<int>>> all_partial_groups_per_scenario;
            std::vector<std::vector<int>> all_partial_sequences_flat_per_scenario;
            std::vector<std::vector<int>> all_group_end_times_per_scenario;

            if (GroupMetaSolution* groupmetasol = dynamic_cast<GroupMetaSolution*>(&metasol)){
                std::tie(match_count, seq_ptr, times_ptr, last_node) = tree.search(*groupmetasol); //returns number of matching group, partial sequences until then (or none), times after those sequences (or none)  For each scenario
            }
            else { //unhandled shortcuts for these other metasol types. Use default values
                match_count = 0;
                seq_ptr = nullptr;
                times_ptr = nullptr;
                last_node = nullptr;
            }

            // Iterate over all scenarios in the DataInstance
            int maxCost = 0; //could use int-min aswell depends on if we are ok with negative values . sumci can't be negative.
            metasol.scores.resize(instance.S);
            metasol.front_sequences.resize(instance.S);
            for (int i=0; i<instance.S; i++) { //for each scenario, get sequence and score, to aggregate
                //Sequence seq = this->extract_sequence(metasol, instance, i, match_count, seq_ptr, times_ptr);
                ExtractedSequenceInfo extracted_info = this->extract_sequence(metasol, instance, i, match_count, seq_ptr, times_ptr, last_node);
                
                Schedule schedule = this->transform_to_schedule(extracted_info.sequence, instance, i);
                // Evaluate the schedule for the current scenario
                int cost = schedule.evaluate(instance);
                //set metasol data
                metasol.scores[i]=cost;
                metasol.front_sequences[i] = extracted_info.sequence; 
                // Update the maximum cost
                if (cost > maxCost) {
                    maxCost = cost;
                }
                //Collect scenario-specific data for caching *if* it's a GroupMetaSolution
                // These vectors hold the data for *this* specific scenario 'i'
                all_partial_groups_per_scenario.push_back(extracted_info.partial_groups);
                all_partial_sequences_flat_per_scenario.push_back(extracted_info.partial_sequence_flat);
                all_group_end_times_per_scenario.push_back(extracted_info.group_end_times);
            }
            metasol.score = maxCost; //set metasol score
            metasol.scored_by = this;
            metasol.scored_for = &instance;
            //Cache info if possible to cache that type
            if (GroupMetaSolution* groupmetasol = dynamic_cast<GroupMetaSolution*>(&metasol)){
                    tree.add(all_partial_groups_per_scenario, all_partial_sequences_flat_per_scenario, all_group_end_times_per_scenario, last_node);
            }
            return maxCost; // Return the aggregated value (max)
        }
        else if (metasol.scored_by!=this || metasol.scored_for!=&instance){ // else if it is scored but not by this policy, or not for this instance
            //reset evaluation (carefull, if it's a list, have to call list_specific_reset.)
            if (ListMetaSolutionBase* listMeta = dynamic_cast<ListMetaSolutionBase*>(&metasol)) {
                listMeta->reset_evaluation();
            }
            else {
                metasol.reset_evaluation(); //reset it and then score (via recursive call))
            }
            return this->evaluate_meta(metasol, instance); //call evaluation again, it will take care of everything.
        }
        else{ //metasol was scored by this policy for this instance already, just send result.
            return metasol.score;
        }
    };
            
    int find_limiting_element(const MetaSolution& metasol, const DataInstance& instance) const{ //finds the limiting element of a listMetaSOlution
        // same for all policies. Similar to evaluate_meata but keep the scenario culprit.
        // assume aggregator : max 

        //assert metasolution type is listmeta.
        if (!dynamic_cast<const ListMetaSolutionBase*>(&metasol)) {
            throw std::runtime_error("MetaSolution must be of type ListMetaSolutionBase.");
        } 

        // Iterate over all scenarios in the DataInstance
        int maxCost = 0; //could use int-min aswell depends on if we are ok with negative values . sumci can't be negative.
        int limiting_index = 0; 
        int limiting_scenario = 0; 
        for (int i=0; i<instance.S; i++) { 
            int cost = metasol.scores[i];
            // Update the maximum cost
            if (cost > maxCost) {
                maxCost = cost;
                limiting_scenario = i;
            }
        }
        limiting_index = this->extract_sub_metasolution_index(metasol, instance, limiting_scenario);
        return limiting_index; 
    }

};


#endif // POLICY_H
