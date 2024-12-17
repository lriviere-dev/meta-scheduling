#ifndef ALGO_H
#define ALGO_H


#include "Policy.h"
#include "MetaSolutions.h"
#include "Instance.h"
#include <ilcp/cp.h>
#include <regex>

//Algorithm take an instance and converti it to a meta solution. Only for instances of 1P.

class Algorithm {
public:
    virtual ~Algorithm() {}

    // Pure virtual function to solve a problem instance
    virtual MetaSolution* solve(const DataInstance& instance) = 0;

    // Virtual method to set a policy for the algorithm (in case it's needed later)
    void setPolicy(Policy* policy) {
        this->policy = policy;
    }

protected:
    Policy* policy; // Reference to a Policy object for evaluating solutions
};

//subclass of algorithm require a starting solution
class SecondStageAlgorithm : public Algorithm{
public:
    virtual ~SecondStageAlgorithm() {}

    // Pure virtual function to solve a problem instance
    void set_initial_solution(MetaSolution& initial_solution){
        this->initial_solution = &initial_solution;
    };

    MetaSolution* initial_solution;
};

//specific Algorithms

//FIFO : a bit of a workaround but outputs the fully permutable group.
//Takes a policy as input but obv. requires FIFO policy.
class PureFiFoSolver : public Algorithm {
public:
    PureFiFoSolver(Policy* policy) {
        this->policy = policy; // Use the policy provided during initialization
    }

    // Implementation of solve method specific to this algorithm
    MetaSolution* solve(const DataInstance& instance) override {
        std::vector<std::vector<int>> groupsVector = {{}};
        groupsVector[0].resize(instance.N);
        for (int i=0; i<instance.N; i++){
            groupsVector[0][i]=i;
        }
        GroupMetaSolution* solution = new GroupMetaSolution(groupsVector);
        return solution;
    }
};

class JSEQSolver : public Algorithm {
public:
    JSEQSolver(Policy* policy) {
        this->policy = policy; // Use the policy provided during initialization
    }

    MetaSolution* solve(const DataInstance& instance) override{

        // Problem parameters declaration
        int nbJobs = instance.N;
        int nbScenarios = instance.S;
        std::vector<std::vector<int>> prec = instance.precedenceConstraints;
        std::vector<int> dueDate = instance.dueDates;
        std::vector<std::vector<int>> releaseDate= instance.releaseDates;
        std::vector<int> duration = instance.durations;

        char name[32]; // dummy variable to temporarily store name of elements
        std::vector<int> final_solution;
        int limit_time = 1; // max solve time in minutes

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
                    if(prec[i][j] == 1) { // if precedence constraint
                        for (int s=0;s<nbScenarios;s++) { //enforce for each scenario, but it would suffice to do it for one scenario.;
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
                if (s > 0) {
                    model.add(IloSameSequence(env, sequences[s], sequences[0])); // force sequence to be the same
                }
            }

            //delegate objective definition to policy
            IloIntExprArray scenario_scores(env, nbScenarios);
            IloIntVar aggregated_objective(env);
            policy->define_objective(env, model, Jobs, instance, scenario_scores, aggregated_objective);

            //Create an instance of IloCP
            IloCP cp(model);
            cp.setParameter(IloCP::LogVerbosity,IloCP::Quiet);
            cp.setParameter(IloCP::TimeLimit, limit_time*60); //limit time to 1 hour
            cp.setParameter(IloCP::Workers, 1); //set workers to 1
            //std::cout  << "Solving... \n" ;

            //Search for a solution
            if (cp.solve()) {
                cp.out() << "Obj Value: " << cp.getObjValue() << std::endl;
                cp.out() << "Solve status: " << cp.getStatus() << std::endl;
                cp.out() << "Solution: ";

                for(IloIntervalVar a = cp.getFirst(sequences[0]); a.getImpl()!=0; a = cp.getNext(sequences[0], a)){
                    cp.out() <<  a.getName() << "*"; //cp.domain(a)
                    std::string temp(a.getName());
                    std::string task_number_str= temp.substr(4, temp.find(',') - 4); // Output: 2                   
                    final_solution.push_back(std::atoi(task_number_str.c_str()));
                    //cp.out() << cp.domain(a) << std::endl;
                    }
                cp.out() <<  std::endl; //cp.domain(a)
            } else {
                cp.out() << "No solution found. " << std::endl;
            }
            cp.printInformation();
            cp.out() << "Optimality Tolerance    : " << cp.getInfo(IloCP::EffectiveOptimalityTolerance) << std::endl;
        } catch (IloException& ex) {
            env.out() << "Error: " << ex << std::endl;
        }
        env.end();

    return new SequenceMetaSolution(final_solution); //return pointer to best reached meta solution (JSEQ)
    }
};


class EssweinAlgorithm : public SecondStageAlgorithm {
public:
    EssweinAlgorithm() {
        //timeLimitSeconds=60*60; //Time limit should never be reached but for fairness it should be there anyways.
    }


    //there is probably a memory leak in there. But at least I'm not deleting things I want to keep.
    MetaSolution* solve(const DataInstance& instance) override {
        // Ensure the poicy is set
        if (!policy) {
            throw std::runtime_error("Policy must be set before running the algorithm.");
        }
        // Ensure the initial sol is set
        if (!initial_solution) {
            throw std::runtime_error("Initial solution must be set before running the algorithm.");
        }
        GroupMetaSolution* currentSolution = nullptr;

        // Initialize the meta solution as a GroupMetaSolution based on initial JSEQ solution
        SequenceMetaSolution* sequenceSolution = dynamic_cast<SequenceMetaSolution*>(initial_solution);

        if (sequenceSolution) {
            // If the cast is successful, call to_gseq
            currentSolution = sequenceSolution->to_gseq();
        } else {
            // Handle the case where the cast fails (optional)
            throw std::runtime_error("Initial solution is not of type SequenceMetaSolution!");
        }

        // Main loop: merge groups while improvement exists
        bool improvement = true;
        while (improvement) { //&& !timeLimitExceeded(startTime)
            improvement = false;
            int bestCandidateScore = policy->evaluate_meta(*currentSolution, instance);
            int bestCandidatelargestGroupSize = currentSolution->largest_group_size();
            int bestCandidateMergeId =-1;

            for (int i = 0; i < currentSolution->nb_groups()-1; ++i){
                GroupMetaSolution* candidateSolution = currentSolution->merge_groups(i);
                int CandidateScore = policy->evaluate_meta(*candidateSolution, instance); // Note that Esswein's algorithm conserves precedence constraints compliance.
                int CandidatelargestGroupSize = candidateSolution->largest_group_size();
                //std::cout <<"considering :";
                //candidateSolution->print();
                //std::cout <<"\n";
                
                if ((CandidateScore<bestCandidateScore) || 
                    ((CandidateScore==bestCandidateScore) && (CandidatelargestGroupSize < bestCandidatelargestGroupSize))){
                    std::cout << CandidateScore << "*:";
                    candidateSolution->print();
                    std::cout << "\n\n";
                    bestCandidateMergeId = i;
                    bestCandidateScore = CandidateScore;
                    bestCandidatelargestGroupSize = CandidatelargestGroupSize;
                    improvement=true;
                }
                delete candidateSolution;
            }
            if (improvement && bestCandidateMergeId != -1) {
                GroupMetaSolution* oldSolution = currentSolution; // Save the old pointer
                currentSolution = currentSolution->merge_groups(bestCandidateMergeId); // Get the new solution
                delete oldSolution; // Delete the old solution            
            }
        }


        return currentSolution; // Return the final solution
    }

private:

};

#endif //ALGO_H