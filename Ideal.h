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
            if(instance.S != sequences.size()){
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
        return instance.S;
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
            if (instance.S != idealmeta->instance_S()) { //assert coherence
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

class IdealSolver : public Algorithm {
public:
    IdealSolver(Policy* policy) {
        this->policy = policy; // Use the policy provided during initialization
        //assert it's Idealpolicy
        if (! dynamic_cast<IdealPolicy*>(policy)) {
            throw std::runtime_error("IdealSolver requires an IdealPolicy");
        }
    }

    IdealSolver(Policy* policy, int max_time) {
        this->policy = policy; // Use the policy provided during initialization
        //assert it's Idealpolicy
        if (! dynamic_cast<IdealPolicy*>(policy)) {
            throw std::runtime_error("IdealSolver requires an IdealPolicy");
        }
        this->max_time = max_time;
    }

    //basically a copy of JSEQSolver.solve
    MetaSolution* solve(const DataInstance& instance) override{

        // Problem parameters declaration
        int nbJobs = instance.N;
        int nbScenarios = instance.S;
        std::vector<uint8_t> prec = instance.precedenceConstraints;
        std::vector<int> dueDate = instance.dueDates;
        std::vector<std::vector<int>> releaseDate= instance.releaseDates;
        std::vector<int> duration = instance.durations;

        char name[32]; // dummy variable to temporarily store name of elements
        std::vector<Sequence> final_solution; //a vector of vector of int i.e a vector of sequences
        int limit_time = max_time; // max solve time in seconds

        //Create environment, model problem
        IloEnv env;
        try {
            IloModel model(env);
            //std::cout  << "Modeling... \n" ;

            //Declaring the interval variables
            IloIntervalVarArray2 Jobs(env,nbScenarios);
            //std::cout  << "   Declaring interval variables... \n" ;
            for (int s=0;s<nbScenarios;s++) { // for each scenario
                Jobs[s] = IloIntervalVarArray(env,nbJobs);
                for (int i=0;i<nbJobs;i++) { //for each Jobs
                    sprintf(name,"Job_%d,Scen_%d",i,s);
                    Jobs[s][i] = IloIntervalVar(env, duration[i], name); //creating var with duration
                    Jobs[s][i].setStartMin(releaseDate[s][i]); //setting release date
    //                std::cout << "Set min start date for task"<< i << " in scenario " << s << ":" << releaseDate[(rVar)?s:0][i] << std::endl;
                }
            }

            //std::cout  << "   Precedence constraints... \n" ;
            //Setting precedence constraints
            for (int i=0;i<nbJobs;i++) { //for each pair of Jobs
                for (int j = 0; j < nbJobs; j++) {
                    if(prec[i*instance.N+j] == 1) { // if precedence constraint
                        for (int s=0;s<nbScenarios;s++) { //enforce for each scenario, as they are not linked anymore
                            model.add(IloEndBeforeStart(env, Jobs[s][i], Jobs[s][j])); //add precedence constranit
                        }
                    }
                }
            }

            //std::cout  << "   Parallel constraints... \n" ;
            //Limit parallel execution (one machine)
            IloIntervalSequenceVarArray sequences(env, nbScenarios);
            for (int s = 0; s < nbScenarios; s++) {//for each scenario
                sequences[s] = IloIntervalSequenceVar(env, Jobs[s]); //Jobs associated with scenario s cannot overlap
                model.add(IloNoOverlap(env, sequences[s]));
                /* if (s > 0) {
                    model.add(IloSameSequence(env, sequences[s], sequences[0])); // DO NOT force sequence to be the same (hence this is commented)
                }*/
            }

            //delegate objective definition to policy
            IloIntExprArray scenario_scores(env, nbScenarios);
            IloIntVar aggregated_objective(env);
            policy->define_objective(env, model, Jobs, instance, scenario_scores, aggregated_objective);

            //Create an instance of IloCP
            IloCP cp(model);
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
                for (int s = 0; s<instance.S; s++){
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