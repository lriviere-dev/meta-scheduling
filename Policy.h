#ifndef POLICY_H
#define POLICY_H


#include "MetaSolutions.h"
#include "ListMetaSolutions.h"
#include "Sequence.h"
#include "Schedule.h"
#include "Instance.h"
#include <vector>
#include <optional>
#include <ilcp/cp.h>

#include <tuple>
#include <functional>

class EvaluationBoundExceeded : public std::exception {
public:
    int bound_value;

    explicit EvaluationBoundExceeded(int value) : bound_value(value) {}

    const char* what() const noexcept override {
        return "Evaluation exceeded the given bound";
    }
};

// The Policy handles the second decision stage. From Meta solution to Sequence to Schedule. It opperates within a scenario.
// WARNING : because of the current scope, some functions should be exclusive to "MAX-policies" (extract_sub_metasolution_index for example). small refactor is in order.
// WARNING : similarly, we require lexicographical order to be defined for the policy
class Policy {
public:
    virtual ~Policy() = default;
    std::string name = "undefined_policy";

    // Pure virtual function to be implemented by derived policies
    virtual Sequence extract_sequence(MetaSolution& metaSolution, const DataInstance& instance, int scenario_id) const = 0;
    virtual bool isLexicographicallySmaller(const Sequence& seq1, const Sequence& seq2, const DataInstance& instance, int scenario_id) const = 0;
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
    int evaluate_meta(MetaSolution& metasol, const DataInstance& instance, std::optional<int> exit_bound = std::nullopt) {
        // same for all policies. just extract a schedule and evaluate it for all scenarios.
        // assume aggregator : max (hence why we can potentially use a bound to stop scenario exploration)

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
            for (int i=0; i<instance.S; i++) { //for each scenario, get sequence and score, to aggregate
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
                    if (exit_bound.has_value() && maxCost > exit_bound.value()){ //given max aggregator, if the score in a scenario gets bigger than the bound, we know eval will return something bigger than bound
                        throw EvaluationBoundExceeded(maxCost);
                    }
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
            
    int find_limiting_scenario(const MetaSolution& metasol, const DataInstance& instance) const{ //finds the limiting scenario of a listMetaSOlution
        // same for all policies. Similar to evaluate_meata but keep the scenario culprit.
        // assume aggregator : max 

        //assert metasolution type is listmeta.
        if (!dynamic_cast<const ListMetaSolutionBase*>(&metasol)) {
            throw std::runtime_error("MetaSolution must be of type ListMetaSolutionBase.");
        } 

        // Iterate over all scenarios in the DataInstance to find the worst one
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
        return limiting_scenario;
        //obsolete limiting_index = this->extract_sub_metasolution_index(metasol, instance, limiting_scenario);
        //return limiting_index; 
    }

};


#endif // POLICY_H
