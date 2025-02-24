#ifndef POLICY_H
#define POLICY_H


#include "MetaSolutions.h"
#include "ListMetaSolutions.h"
#include "Sequence.h"
#include "Schedule.h"
#include "Instance.h"
#include <vector>
#include <deque>
#include <ilcp/cp.h>

#include <tuple>
#include <functional>

#include <functional>


//The Policy handles the second decision stage. From Meta solution to Sequence to Schedule. It opperates within a scenario.
// WARNING : because of the current scope, some functions should be exclusive to "MAX-policies" (extract_sub_metasolution_index for example). small refactor is in order.
class Policy {
public:
    virtual ~Policy() = default;

    // Pure virtual function to be implemented by derived policies
    virtual Sequence extract_sequence(MetaSolution& metaSolution, const DataInstance& instance, int scenario_id) const = 0;
    virtual int extract_sub_metasolution_index(const MetaSolution& metaSolution, const DataInstance& instance, int scenario_id) const = 0;    
    
    //functions virtual but with default implementation
    //ERD will be shared by probably all policies
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

            // Iterate over all scenarios in the DataInstance
            int maxCost = 0; //could use int-min aswell depends on if we are ok with negative values . sumci can't be negative.
            metasol.scores.resize(instance.S);
            metasol.front_sequences.resize(instance.S);
            for (int i=0; i<instance.S; i++) { 
                Sequence seq = this->extract_sequence(metasol, instance, i);
                Schedule schedule = this->transform_to_schedule(seq, instance, i);
                // Evaluate the schedule for the current scenario
                int cost = schedule.evaluate(instance);
                //set metasol data
                metasol.scores[i]=cost;
                metasol.front_sequences[i] = seq; 
                // Update the maximum cost
                if (cost > maxCost) {
                    maxCost = cost;
                }
            }
            metasol.score = maxCost; //set metasol score
            metasol.scored_by = this;
            metasol.scored_for = &instance;
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
            return this->evaluate_meta(metasol, instance);
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

//We will only use FIFOpolicy, but we could imagine other policies.
class FIFOPolicy : public Policy {
public:
    ~FIFOPolicy() = default;

    // Override extract_sequence for GroupMetaSolution
    Sequence extract_sequence(MetaSolution& metaSolution, const DataInstance& instance, int scenario_id) const override{
        if(metaSolution.scored_by){throw std::runtime_error("Extracting sequence but metasol is already scored.");} //if already scored, why don't we just get the stored result?

        Sequence output; 
        bool set_output = false;
        // Try to cast to GroupMetaSolution
        if (auto* groupMeta = dynamic_cast<GroupMetaSolution*>(&metaSolution)) {
            // Handle GroupMetaSolution

            std::vector<int> sequence(instance.N);
            int c = 0; // counter for index
            const auto& releaseDates = instance.releaseDates[scenario_id];
            const auto& prec = instance.precedenceConstraints;
            std::deque<int> free_nodes; //sorted list of available nodes (re-used for each group, is emptied every loop)
            std::vector<int> incoming_edges_nb; 
            std::vector<std::vector<int>> outgoing_edges; 


        // Iterate over each group of tasks
            for (auto& group : groupMeta->get_task_groups_modifiable()) {
                // First, sort the tasks by their release date (or lex order if tie)
                std::sort(group.begin(), group.end(), [&releaseDates](int t1, int t2) {
                    //check release dates
                    if (releaseDates[t1] != releaseDates[t2]) {
                        return releaseDates[t1] < releaseDates[t2];
                    }
                    return t1 < t2; // Lexicographical order as tie-breaker
                }); //group shouldn't be further modified, as we will use it as a key

                //precompute a graph-like node structure for toposort
                incoming_edges_nb.resize(group.size(), 0);
                for (auto& vec : outgoing_edges) vec.clear(); // Clears contents without deallocating
                outgoing_edges.resize(group.size()); // Ensures correct size without reallocating inner vectors                
                for (size_t i=0; i<group.size();  i++) { //!!! We use index "0" for example to refer to the 0th task in group vector!
                    //for each tasks list nodes with an incoming edge
                    for (size_t j =0; j<group.size(); j++){
                        if (prec[group[j] * instance.N + group[i]]){ //if the task at index j should be before task at index i,
                            incoming_edges_nb[i]++;
                        }
                        if (prec[group[i] * instance.N + group[j]]){ 
                            outgoing_edges[i].push_back(j); //keeps sorted order from group
                        }                        
                    }
                    if (incoming_edges_nb[i]==0){free_nodes.push_back(i);}
                }

                //toposort, but free nodes are selected in the sorted order.
                while (!free_nodes.empty()){
                    int selected_task = free_nodes.front();
                    free_nodes.pop_front();
                    sequence[c++]=group[selected_task]; //post-increment
                    for (auto & node : outgoing_edges[selected_task]){//remove edges with this task
                        incoming_edges_nb[node]--;
                        if (incoming_edges_nb[node]==0){
                            free_nodes.push_back(node);//because nodes in outgoing edges are sorted, free nodes is sorted.
                        }
                    }
                }
            }

            output = Sequence(std::move(sequence));
            /*if (!output.check_precedence_constraints(instance)){
                throw std::runtime_error("bug detected");
            }*/
            set_output = true;
        }
        else if (auto* SeqMeta = dynamic_cast< SequenceMetaSolution*>(&metaSolution)) {
            output = Sequence(SeqMeta->get_sequence().get_tasks());
            set_output = true;
        }
        // Handling all ListMetaSolution types via their underlying metasolution type (recursive)
        //we assume the underlying metasolutions have already been scored appropriately ( we make sure of that in evaluate_meta)
        else if (auto* listMeta = dynamic_cast< ListMetaSolutionBase*>(&metaSolution)) {
            const auto& metaSolutions = listMeta->get_meta_solutions();
            Sequence minSeq = metaSolutions[0]->front_sequences[scenario_id]; // Initialize with the first sequence/metasol
            listMeta->front_indexes[scenario_id] = 0;

            for (size_t i = 1; i < metaSolutions.size(); ++i) {  // Iterate from second element
                auto& seq = metaSolutions[i]->front_sequences[scenario_id];
                if (seq.isLexicographicallySmaller(minSeq, instance, scenario_id)) {
                    minSeq = seq;
                    listMeta->front_indexes[scenario_id] = i;//update index of metasol used in that scenario
                }
            }
            output = std::move(minSeq); 
            set_output = true;
        }
        // Add other MetaSolution type checks here if necessary
        if (!set_output){
            throw std::invalid_argument("Unsupported MetaSolution type in FIFOPolicy::extract_sequence.");
        }
        return output;
    }
    
    int extract_sub_metasolution_index(const MetaSolution& metaSolution, const DataInstance& instance, int scenario_id) const override{
        //assert list solution
        const ListMetaSolutionBase* listMetaSolution = dynamic_cast<const ListMetaSolutionBase*>(&metaSolution);
        if (!listMetaSolution) {
            throw std::runtime_error("MetaSolution must be of type ListMetaSolutionBase.");
        }
        //assert metasol was scored
        if (!metaSolution.scored_by) {
            throw std::runtime_error("metasol should already be scored by a policy");
        }    
        return listMetaSolution->front_indexes[scenario_id]; // Return the index of the metasolution in the list that outputs the min sequence    
    }

    };

#endif // POLICY_H
