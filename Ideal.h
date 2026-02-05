#ifndef IDEAL_H
#define IDEAL_H

#include "Sequence.h"
#include "MetaSolutions.h"
#include "Instance.h"
#include "Policy.h"
#include "Algorithms.h"
#include <vector>
#include <iostream>
#include <algorithm>


// All special classes needed for the Ideal solver to compute a bound. Note that it's a bit of a special case as it cannot handle being evaluated against unknown scenario sets.

class IdealMetaSolution : public MetaSolution {
public:

    IdealMetaSolution(const std::vector<Sequence>& sequences, const DataInstance &instance) //requires instance right away
        : sequences(sequences) , instance(instance) {
            if(instance.getS() != sequences.size()){
                throw std::runtime_error("initializing an IdealMetaSolution with mismatched number of sequences");
            }
        }

    Sequence get_sequence(int scenario){
        return sequences[scenario];
    }

    std::vector<Sequence> get_sequences(){
        return sequences;
    }

    int instance_S(){
        return instance.getS();
    }

    void print() const override{
        if (sequences.size()>=25) {std::cout << "Troncating output (too large)\n";}
        for (size_t i = 0; i < std::min(static_cast<size_t>(25), sequences.size()); ++i) {
            std::cout << "Sequence for scenario " << i << " : ";
            sequences[i].print();
            std::cout << std::endl;
        }
    }
 
private:
    std::vector<Sequence> sequences; // A bunch of sequences (one for each scenario)
    const DataInstance & instance;

};

//The policy is Ideal as in it handles IdealMetaSOlutions, it is not Ideal in other contexts
class IdealPolicy : public Policy {
public:
    ~IdealPolicy() = default;
    std::string name = "ideal_policy";

    // Override extract_sequence for GroupMetaSolution
    Sequence extract_sequence(MetaSolution& metaSolution, const DataInstance& instance, int scenario_id) const override {
        if (auto* idealmeta = dynamic_cast<IdealMetaSolution*>(&metaSolution)) {
            if (instance.getS() != idealmeta->instance_S()) { //assert coherence
                throw std::runtime_error("Incoherence between IdealMetaSolutions and instance");
            }
            return idealmeta->get_sequence(scenario_id); // just return the ideal sequence for the scenario
        }
        else{
            throw std::runtime_error("IdealPolicy only handles IdealMetaSoluions");
        }
    }

    int extract_sub_metasolution_index(const MetaSolution& metaSolution, const DataInstance& instance, int scenario_id) const override{
        throw std::runtime_error("Should not be called with the IdealPolicy : IdealPolicy only handles IdealMetaSOlutions, not LIstMetaSolutions");
        (void)metaSolution; //warning removal
        (void)instance; //warning removal
        (void)scenario_id; //warning removal
    }

    bool isLexicographicallySmaller(const Sequence& seq1, const Sequence& seq2, const DataInstance& instance, int scenario_id)  const override{
        throw std::runtime_error("Should not be called with the IdealPolicy : IdealPolicy only handles IdealMetaSOlutions, not LIstMetaSolutions");
        (void)seq1; //warning removal
        (void)seq2; //warning removal
        (void)instance; //warning removal
        (void)scenario_id; //warning removal
    }

};

class IdealRCPSPPolicy : public Policy {
public:
    ~IdealRCPSPPolicy() = default;
    std::string name = "ideal_rcpsp_policy";

    // Override extract_sequence for GroupMetaSolution
    Sequence extract_sequence(MetaSolution& metaSolution, const DataInstance& instance, int scenario_id) const override {
        if (auto* idealmeta = dynamic_cast<IdealMetaSolution*>(&metaSolution)) {
            if (instance.getS() != idealmeta->instance_S()) { //assert coherence
                throw std::runtime_error("Incoherence between IdealMetaSolutions and instance");
            }
            return idealmeta->get_sequence(scenario_id); // just return the ideal sequence for the scenario
        }
        else{
            throw std::runtime_error("IdealPolicy only handles IdealMetaSoluions");
        }
    }

    Schedule transform_to_schedule(const Sequence& sequence, const DataInstance& instance, int scenario_id) const override {
        const RCPSPInstance& rcpsp_instance = static_cast<const RCPSPInstance&>(instance);
        const std::vector<int>& tasks = sequence.get_tasks();
        const size_t numTasks = tasks.size();
        const size_t numRes = rcpsp_instance.capacities.size();

        // 1. Setup Horizon and 1D Resource Profile
        int horizon = 0;
        int max_r = 0;
        for (int d : rcpsp_instance.durations) horizon += d;
        for (int r : rcpsp_instance.releaseDates[scenario_id]) max_r = std::max(max_r, r);
        horizon += max_r;
        
        // Contiguous memory: Resource 0 [0...H], Resource 1 [0...H], etc.
        std::vector<int> resourceUsage(numRes * (horizon + 1), 0);
        std::vector<int> startTimes(numTasks);
        std::vector<int> finishTimes(numTasks);

        int lastTaskStart = 0;

        for (size_t i = 0; i < numTasks; ++i) {
            int task = tasks[i];
            int duration = rcpsp_instance.durations[task];

            // 2. Initial Earliest Start Time (EST)
            // Respects sequence order (start_i >= start_{i-1}) and release date
            int t = std::max(rcpsp_instance.releaseDates[scenario_id][task], lastTaskStart);

            // 3. Precedence: Must finish after ALL predecessors
            // We check every task previously scheduled in the sequence
            for (size_t j = 0; j < i; ++j) {
                int prevTask = tasks[j];
                if (rcpsp_instance.get_prec(prevTask, task)) {//if prevTask must precede task
                    if (finishTimes[prevTask] > t) {//and it finishes after current t
                        t = finishTimes[prevTask]; //push t forward
                    }
                }
            }

            // 4. Resource Feasibility (The Leapfrog Search) : push time forward until no resource conflict
            bool conflict = true;
            while (conflict) {
                conflict = false;
                for (size_t r = 0; r < numRes; ++r) { //for every resource
                    int demand = rcpsp_instance.usages[task][r];
                    if (demand <= 0) continue;

                    int cap = rcpsp_instance.capacities[r];
                    int offset = r * (horizon + 1); //offset in 1D profile for quicker accesss

                    for (int time = t; time < t + duration; ++time) {
                        if (resourceUsage[offset + time] + demand > cap) {
                            // Conflict found: Jump t forward and restart resource check
                            t = time + 1;
                            conflict = true;
                            break; // Breaks out of time loop
                        }
                    }
                    if (conflict) break; //break out of ressource loop : restart from first ressource at new t
                }
            }

            // 5. Commit Timing
            startTimes[task] = t;
            finishTimes[task] = t + duration;
            lastTaskStart = t; // Anchors the next task in the sequence

            // 6. Update Resource Profile
            for (size_t r = 0; r < numRes; ++r) {
                int demand = rcpsp_instance.usages[task][r];
                if (demand <= 0) continue;
                int offset = r * (horizon + 1);
                for (int time = t; time < t + duration; ++time) {
                    resourceUsage[offset + time] += demand;
                }
            }
        }
        return Schedule(startTimes);
    }

    int extract_sub_metasolution_index(const MetaSolution& metaSolution, const DataInstance& instance, int scenario_id) const override{
        throw std::runtime_error("Should not be called with the IdealPolicy : IdealPolicy only handles IdealMetaSOlutions, not LIstMetaSolutions");
        (void)metaSolution; //warning removal
        (void)instance; //warning removal
        (void)scenario_id; //warning removal
    }

    bool isLexicographicallySmaller(const Sequence& seq1, const Sequence& seq2, const DataInstance& instance, int scenario_id)  const override{
        throw std::runtime_error("Should not be called with the IdealPolicy : IdealPolicy only handles IdealMetaSOlutions, not LIstMetaSolutions");
        (void)seq1; //warning removal
        (void)seq2; //warning removal
        (void)instance; //warning removal
        (void)scenario_id; //warning removal
    }

};


class IdealSolver : public Algorithm { 
public:
    IdealSolver(Policy* policy) {
        this->policy = policy; // Use the policy provided during initialization
        //assert it's Idealpolicy
        if ((! dynamic_cast<IdealPolicy*>(policy) && (! dynamic_cast<IdealRCPSPPolicy*>(policy)))) {
            throw std::runtime_error("IdealSolver requires an IdealPolicy");
        }
    }

    IdealSolver(Policy* policy, int max_time) {
        this->policy = policy; // Use the policy provided during initialization
        //assert it's Idealpolicy
        if ((! dynamic_cast<IdealPolicy*>(policy) && (! dynamic_cast<IdealRCPSPPolicy*>(policy)))) {
            throw std::runtime_error("IdealSolver requires an IdealPolicy");
        }
        this->max_time = max_time;
    }

    //basically a copy of JSEQSolver.solve
    MetaSolution* solve(const DataInstance& instance) override{
        //variables common to all instance types
        int nbJobs = instance.getN();
        int nbScenarios = instance.getS();
        std::vector<uint8_t> prec = instance.precedenceConstraints;

        char name[32]; // dummy variable to temporarily store name of elements
        std::vector<Sequence> final_solution; //a vector of vector of int i.e a vector of sequences
        int limit_time = max_time; // max solve time in seconds
        IloEnv env; //environment variabel
        IloIntExprArray scenario_scores(env, instance.getS());//scores in each scenario
        IloIntVar aggregated_objective(env); //score aggregated over scenarios
        IloModel model(env);
        // The sequence variable will simply record the relative ordering of tasks.
        IloIntervalSequenceVarArray sequences(env, nbScenarios);

        try {
            if (instance.type == InstanceType::SINGLE_MACHINE){
                
                const SingleMachineInstance& sm_instance = static_cast<const SingleMachineInstance&>(instance); //cast to specific type to access recquired data
                //model variables/constraints specific to single machine instances

                // instance specific parameters declaration (even if common to several instances)
                std::vector<int> dueDate = sm_instance.dueDates;
                std::vector<std::vector<int>> releaseDate = sm_instance.releaseDates;
                std::vector<int> duration = sm_instance.durations;

                //Declaring the interval variables
                IloIntervalVarArray2 Jobs(env,nbScenarios);
                for (int s=0;s<nbScenarios;s++) { // for each scenario
                    Jobs[s] = IloIntervalVarArray(env,nbJobs);
                    for (int i=0;i<nbJobs;i++) { //for each Jobs
                        sprintf(name,"Job_%d,Scen_%d",i,s);
                        Jobs[s][i] = IloIntervalVar(env, duration[i], name); //creating var with duration
                        Jobs[s][i].setStartMin(releaseDate[s][i]); //setting release date
                    }
                }

                //Setting precedence constraints
                for (int i=0;i<nbJobs;i++) { //for each pair of Jobs
                    for (int j = 0; j < nbJobs; j++) {
                        if(prec[i*instance.getN()+j] == 1) { // if precedence constraint
                            for (int s=0;s<nbScenarios;s++) { //enforce for each scenario, but it would suffice to do it for one scenario.;
                                model.add(IloEndBeforeStart(env, Jobs[s][i], Jobs[s][j])); //add precedence constranit
                            }
                        }
                    }
                }

                //std::cout  << "   Parallel constraints... \n" ;
                //Limit parallel execution (one machine)
                for (int s = 0; s < nbScenarios; s++) {//for each scenario
                    sequences[s] = IloIntervalSequenceVar(env, Jobs[s]); //Jobs associated with scenario s cannot overlap
                    model.add(IloNoOverlap(env, sequences[s]));
                    // if (s > 0) {
                    //     model.add(IloSameSequence(env, sequences[s], sequences[0])); // force sequence to be the same => Not in ideal solver!!
                    // }
                }
                //delegate objective definition to policy
                policy->define_objective(env, model, Jobs, instance, scenario_scores, aggregated_objective);
            }
            else if (instance.type == InstanceType::RCPSP) { //written by AI using single machine and CPO RCPSP examples as basis
                const RCPSPInstance& rcpsp_instance = static_cast<const RCPSPInstance&>(instance);
                
                // 1. Parameters extraction
                int nbResources = rcpsp_instance.num_resources;
                                
                // 2. Resource Setup (Cumul Functions)
                // We create resources per scenario because capacities or demands might vary by scenario
                std::vector<IloCumulFunctionExprArray> resources(nbScenarios);
                IloIntArray capacities(env, nbResources);
                for (int j = 0; j < nbResources; j++) {
                    capacities[j] = rcpsp_instance.capacities[j];
                }

                // 3. Declare Interval Variables
                IloIntervalVarArray2 Jobs(env, nbScenarios);
                for (int s = 0; s < nbScenarios; s++) { //for each scenario
                    Jobs[s] = IloIntervalVarArray(env, nbJobs); //make  jobs arrays
                    resources[s] = IloCumulFunctionExprArray(env, nbResources); 
                    
                    for (int j = 0; j < nbResources; j++) {
                        resources[s][j] = IloCumulFunctionExpr(env);
                    }

                    for (int i = 0; i < nbJobs; i++) {
                        sprintf(name, "Job_%d,Scen_%d", i, s);
                        // RCPSP tasks usually have fixed duration from instance
                        Jobs[s][i] = IloIntervalVar(env, rcpsp_instance.durations[i], name);
                        Jobs[s][i].setStartMin(rcpsp_instance.releaseDates[s][i]);//release date varies by scenario

                        // Resource Requirements (Pulse)
                        for (int r = 0; r < nbResources; r++) {
                            int demand = rcpsp_instance.usages[i][r];
                            if (demand > 0) {
                                resources[s][r] += IloPulse(Jobs[s][i], demand);
                            }
                        }
                    }

                    // Add Resource Capacity Constraints for this scenario
                    for (int r = 0; r < nbResources; r++) {
                        model.add(resources[s][r] <= capacities[r]);
                    }
                }

                // 4. Precedence Constraints
                for (int i = 0; i < nbJobs; i++) {
                    for (int j = 0; j < nbJobs; j++) {
                        // Using the same precedence matrix logic as your Single Machine snippet
                        if (rcpsp_instance.precedenceConstraints[i * nbJobs + j] == 1) {
                            for (int s = 0; s < nbScenarios; s++) {
                                model.add(IloEndBeforeStart(env, Jobs[s][i], Jobs[s][j]));
                            }
                        }
                    }
                }

                // 5. Sequence Variables (For Solution Extraction)
                // IMPORTANT: We do NOT add IloNoOverlap here. 
                for (int s = 0; s < nbScenarios; s++) {
                    sequences[s] = IloIntervalSequenceVar(env, Jobs[s]);
                    model.add(sequences[s]);//Recquired to extract the variable

                    // 4. Ensuring Sequencesand jobs share order : for each job, eitheh it's successor is null or its start time is after its own start time 
                    for (int i = 0; i < nbJobs; i++) {
                        model.add(IloStartOfPrevious(sequences[s], Jobs[s][i], 0) <= IloStartOf(Jobs[s][i])); //when prev is nul, we take 0 so constraint holds
                    }
                }

                // 6. Objective definition delegated to policy
                policy->define_objective(env, model, Jobs, instance, scenario_scores, aggregated_objective);
            }
            else {
                throw std::runtime_error("JSEQSolver does not support this instance type.");
            }

            //Create an instance of IloCP
            IloCP cp(model);
            cp.setParameter(IloCP::WarningLevel, instance.getN()>=20 ? 0 : 3); //suppress warnings for large instances, keep them for small ones for debug
            cp.setParameter(IloCP::LogVerbosity,IloCP::Quiet);
            cp.setParameter(IloCP::TimeLimit, limit_time); 
            cp.setParameter(IloCP::Workers, 1); //set workers to 1
            //std::cout  << "Solving... \n" ;

            //Search for a solution
            cp.out() << "Starting IdealSolver solve. Max time = " << max_time << "s" << std::endl;
            if (cp.solve()) {
                cp.out() << "IdealSolver Obj Value: " << cp.getObjValue() << std::endl;
                cp.out() << "IdealSolver Gap Value: " << cp.getObjGap() << std::endl;
                cp.out() << "IdealSolver Solve status: " << cp.getStatus() << std::endl;
                //cp.out() << "Solution: ";
                for (int s = 0; s<instance.getS(); s++){
                    std::vector<int> final_s;
                    for(IloIntervalVar a = cp.getFirst(sequences[s]); a.getImpl()!=0; a = cp.getNext(sequences[s], a)){
                        //cp.out() <<  a.getName() << "*"; //cp.domain(a)
                        std::string temp(a.getName());
                        std::string task_number_str= temp.substr(4, temp.find(',') - 4); // Output: 2                   
                        final_s.push_back(std::atoi(task_number_str.c_str()));
                        //cp.out() << cp.domain(a) << std::endl;
                        }
                        final_solution.push_back(Sequence(final_s));
                }
                //debug print start times of each interval in each scenario
                // for (int s = 0; s < std::min(nbScenarios, 10); s++) {
                //     cp.out() << "\nScenario " << s << " Start Times: ";
                //     for (IloIntervalVar a = cp.getFirst(sequences[s]); a.getImpl() != 0; a = cp.getNext(sequences[s], a)) {
                //         std::string temp(a.getName());
                //         std::string task_number_str= temp.substr(4, temp.find(',') - 4); // Output: 2    
                //         cp.out() << task_number_str << "(" << cp.getStart(a) << ") ";
                //     }
                //     cp.out() << std::endl;
                // }

                //cp.out() <<  std::endl; //cp.domain(a)
            } else {
                cp.out() << "No solution found. " << std::endl;
            }
            cp.printInformation();
            cp.out() << "Optimality Tolerance    : " << cp.getInfo(IloCP::EffectiveOptimalityTolerance) << std::endl;
        } catch (IloException& ex) {
            env.out() << "Error: " << ex << std::endl;
        }
        env.end();

    return new IdealMetaSolution(final_solution, instance); //return pointer to best reached meta solution (IdealSolution)
    }
};

#endif 