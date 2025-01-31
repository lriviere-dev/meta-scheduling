#ifndef POLICY_H
#define POLICY_H

#include "MetaSolutions.h"
#include "ListMetaSolutions.h"
#include "Sequence.h"
#include "Schedule.h"
#include "Instance.h"
#include <vector>
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
    virtual Schedule transform_to_schedule(const Sequence& sequence, const DataInstance& instance, int scenario_id) const = 0;
    virtual void define_objective(IloEnv env, IloModel& model, 
                        IloIntervalVarArray2& jobs, const DataInstance& instance, 
                        IloIntExprArray& scenario_scores,  IloIntVar& aggregated_objective) const = 0;
    
    //Functions common to all Policies (i.e not virtual)
    int evaluate_meta(MetaSolution& metasol, const DataInstance& instance) {
        // same for all policies. just extract a schedule and evaluate it for all scenarios.
        // assume aggregator : max

        if (!metasol.scored_by){//metasol was not already scored -> score it and set front/scores for each scenario

            //special case if metasol is a list of metasol, we recursively have to make sure to evaluate the underlying before
            if (ListMetaSolutionBase* listMeta = dynamic_cast<ListMetaSolutionBase*>(&metasol)) {
                for (auto submeta : listMeta->get_meta_solutions()){
                    if (!submeta->scored_by){ //sub metasolution wasn't scored : evaluate it
                        this->evaluate_meta(*submeta,instance);
                    }
                    else if (submeta->scored_by!=this){ // else if it is scored but not by this policy, throw
                        throw std::runtime_error("sub metasolution was scored by a different policy");
                    }
                    //else do nothing, it is already scored, we can proceed
                }
            }

            // Iterate over all scenarios in the DataInstance
            int maxCost = 0; //could use int-min aswell depends on if we are ok with negative values . sumci can't be negative.
            for (int i=0; i<instance.S; i++) { 
                Sequence seq = this->extract_sequence(metasol, instance, i);
                Schedule schedule = this->transform_to_schedule(seq, instance, i);
                // Evaluate the schedule for the current scenario
                int cost = schedule.evaluate(instance);
                //set metasol data
                metasol.scores.push_back(cost);
                metasol.front_sequences.push_back(seq);                
                // Update the maximum cost
                if (cost > maxCost) {
                    maxCost = cost;
                }
            }
            metasol.score = maxCost; //set metasol score
            metasol.scored_by = this;
            return maxCost; // Return the aggregated value (max)
        }
        else if (metasol.scored_by!=this){ // else if it is scored but not by this policy, throw
            throw std::runtime_error("metasolution was already scored by different policy"); //should not happen while we have only one policy.
        }
        else{ //metasol was scored and by us already, just send result.
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
    Sequence extract_sequence(MetaSolution& metaSolution, const DataInstance& instance, int scenario_id) const override;
    int extract_sub_metasolution_index(const MetaSolution& metaSolution, const DataInstance& instance, int scenario_id) const override;
    Schedule transform_to_schedule(const Sequence& sequence, const DataInstance& instance, int scenario_id) const override;
    void define_objective(IloEnv env, IloModel& model, 
                        IloIntervalVarArray2& jobs, const DataInstance& instance, 
                        IloIntExprArray& scenario_scores,  IloIntVar& aggregated_objective) const override;
    
    };

#endif // POLICY_H
