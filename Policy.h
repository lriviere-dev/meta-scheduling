#ifndef POLICY_H
#define POLICY_H

#include "MetaSolutions.h"
#include "Sequence.h"
#include "Schedule.h"
#include "Instance.h"
#include <vector>
#include <ilcp/cp.h>


//The Policy handles the second decision stage. From Meta solution to Sequence to Schedule. It opperates within a scenario.
class Policy {
public:
    virtual ~Policy() = default;

    // Pure virtual function to be implemented by derived policies
    virtual Sequence extract_sequence(const MetaSolution& metaSolution, DataInstance& scenario) const = 0;
    virtual Schedule transform_to_schedule(const Sequence& sequence, const DataInstance& scenario) const = 0;
    virtual void define_objective(IloEnv env, IloModel& model, 
                        IloIntervalVarArray2& jobs, const DataInstance& instance, 
                        IloIntExprArray& scenario_scores,  IloIntVar& aggregated_objective) const = 0;
    
    //Functions common to all Policies
    int evaluate_meta(const MetaSolution& metasol, const DataInstance& instance) const{
        // same for all policies. just extract a schedule and evaluate it for all scenarios.
        // assume aggregator : max

        // Iterate over all scenarios in the DataInstance
        int maxCost = 0; //could use int-min aswell depends on if we are ok with negative values for lateness. sumci can't be negative.
        for (int i=0; i<instance.S; i++) { 
            DataInstance scenario = instance.single_instance(i);
            Sequence seq = extract_sequence(metasol, scenario);
            Schedule schedule = transform_to_schedule(seq, scenario);

            // Evaluate the schedule for the current scenario
            int cost = schedule.evaluate(scenario);
            std::cout << "s" << i << ":";
            seq.print();
            std::cout <<":" <<cost<<std::endl;
            // Update the maximum cost
            if (cost > maxCost) {
                maxCost = cost;
            }
        }

        return maxCost; // Return the aggregated value (max)
        };
    };

//We will only use FIFOpolicy, but we could imagine other policies.
class FIFOPolicy : public Policy {
public:
    ~FIFOPolicy() = default;

    // Override extract_sequence for GroupMetaSolution
    Sequence extract_sequence(const MetaSolution& metaSolution, DataInstance& scenario) const override;
    Schedule transform_to_schedule(const Sequence& sequence, const DataInstance& scenario) const override;
    void define_objective(IloEnv env, IloModel& model, 
                        IloIntervalVarArray2& jobs, const DataInstance& instance, 
                        IloIntExprArray& scenario_scores,  IloIntVar& aggregated_objective) const override;
    };

#endif // POLICY_H