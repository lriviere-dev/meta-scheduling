#include <iostream>
#include "Schedule.h"
#include "Instance.h"
#include "Sequence.h"
#include "Policy.h"
#include "PolicySPT.h"
#include "MetaSolutions.h"
#include "ListMetaSolutions.h"
#include "Algorithms.h"
#include "BestOfAlgorithm.h"
#include "Ideal.h"


int main2() {

    std::cout << "=== TESTING SPTPolicy on Manual GroupMetaSolution ===" << std::endl;
    
    std::string file_name = "instances/test_spt.data";  // Default file for tests
    DataInstance instance(file_name);
    instance.print_summary();

    // Create policy
    SPTPolicy spt_policy;
    
    // Create a manual GroupMetaSolution
    std::vector<std::vector<int>> sol = {{0},{1,2}};
    GroupMetaSolution manual_solution(sol);
    manual_solution.print();
    std::cout << std::endl;

    // Evaluate on trainInstance (or any DataInstance you have ready)
    double score = spt_policy.evaluate_meta(manual_solution, instance);
    
    spt_policy.transform_to_schedule(manual_solution.front_sequences[0], instance, 0).print();
     std::cout << "SPTPolicy score on manual solution s0: " << manual_solution.scores[0] << std::endl;
   spt_policy.transform_to_schedule(manual_solution.front_sequences[1], instance, 1).print();
    std::cout << "SPTPolicy score on manual solution s1: " << manual_solution.scores[1] << std::endl;

    std::cout << "SPTPolicy score on manual solution: " << score << std::endl;

    return 0;
}


    //Some backup code if needed. (probably out of date)
    /*
    std::cout << "==== INSTANCE SCENARIO TEST ===" << std::endl;
    //Loading up instance/scenario
    //DataInstance instance("instances/test.data");
    DataInstance instance("instances/test2.data");
    DataInstance single_scenario(instance,0);
    single_scenario.print();

    FIFOPolicy fifo; //defining policy

    std::cout << std::endl << std::endl;

    std::cout << "==== JSEQ TEST ===" << std::endl;
    std::vector<int> taskseq(instance.N);
    for (int i = 0; i < instance.N; ++i) {
        taskseq[i] = i; // Example: Filling with sequential integers
    }

    Sequence taskseqbis = Sequence(taskseq);
    // Test GroupMetaSolution
    SequenceMetaSolution myseq(taskseqbis);
    std::cout << "MetaSolution (JSEQ) :";
    myseq.print();
    std::cout << std::endl;

    //extract sequence from meta solution
    Sequence selected_sequence = fifo.extract_sequence(myseq, single_scenario);
    std::cout << "Selected sequence :";
    selected_sequence.print();
    std::cout << std::endl << std::endl;


    std::cout << "==== GROUP SEQUENCE TEST ===" << std::endl;
        // Example group sequence with half the tasks in the first group, half in the second one
    std::vector<std::vector<int>> taskGroups(2);
    for (int i = 0; i < instance.N; ++i) {
        if (i<instance.N/2){
            taskGroups[0].push_back(i); 
        }
        else{
            taskGroups[1].push_back(i); 
        }
    }

    // Test GroupMetaSolution
    GroupMetaSolution mygroup(taskGroups);
    std::cout << "MetaSolution (GSEQ) :";
    mygroup.print();
    std::cout << std::endl;

    //extract sequence from meta solution
    selected_sequence = fifo.extract_sequence(mygroup, single_scenario);
    std::cout << "Selected sequence :";
    selected_sequence.print();
    std::cout << std::endl << std::endl;

    std::cout << "==== LIST OF X TEST ===" << std::endl;
    // another sequence
    std::vector<int> taskseq2(instance.N);
    for (int i = 0; i < instance.N; ++i) {
        taskseq2[i] = instance.N-i; // Example: Filling with sequential integers
    }
    Sequence taskseqbis2 = Sequence(taskseq2);
    std::vector<SequenceMetaSolution> vectorseq = {taskseqbis, taskseqbis2};

    // another group sequence
    std::vector<std::vector<int>> taskGroups2(2);
    for (int i = 0; i < instance.N; ++i) {
        if (i % 2 == 0){
            taskGroups2[0].push_back(i); 
        }
        else{
            taskGroups2[1].push_back(i); 
        }
    }
    GroupMetaSolution mygroup2(taskGroups2);

    std::vector<std::vector<int>> taskGroups3(1);
    for (int i = 0; i < instance.N; ++i) {
        taskGroups3[0].push_back(i); //fully permutable gruop
    }
    GroupMetaSolution mygroup3(taskGroups3);
    std::vector<GroupMetaSolution> vectorgroup = {taskGroups, taskGroups2, taskGroups3};

    // Test seq set sol
    ListMetaSolution<SequenceMetaSolution> listseq(vectorseq);
    std::cout << "MetaSolution (S-SEQ) :";
    listseq.print();
    std::cout << std::endl;

    //extract sequence from meta solution
    selected_sequence = fifo.extract_sequence(listseq, single_scenario);
    std::cout << "Selected sequence :";
    selected_sequence.print();
    std::cout << std::endl << std::endl;


    // Test GroupMetaSolution
    ListMetaSolution<GroupMetaSolution> listgroup(vectorgroup);
    std::cout << "MetaSolution (S-GSEQ) :";
    listgroup.print();
    std::cout << std::endl;

    //extract sequence from meta solution
    selected_sequence = fifo.extract_sequence(listgroup, single_scenario);
    std::cout << "Selected sequence :";
    selected_sequence.print();
    std::cout << std::endl << std::endl;

    std::cout << "==== LEFT SHIFT TEST ===" << std::endl;
    Schedule resulting_schedule = fifo.transform_to_schedule(selected_sequence, single_scenario);
    std::cout << "Resulting schedule start times : ";
    resulting_schedule.print();
    std::cout << "Score : " <<resulting_schedule.evaluate(single_scenario);
    std::cout << std::endl << std::endl;



    std::cout << "==== ALGO / META EVAL TEST ===" << std::endl;

    PurePolicySolver PureFifo(&fifo);
    MetaSolution* purefifosolution = PureFifo.solve(instance);

    std::cout << "Solution reached:"; 
    purefifosolution->print();
    std::cout << std::endl;

    //evaluate 
    int overall_score = fifo.evaluate_meta(*purefifosolution, instance);
    std::cout << "Score :" << overall_score << std::endl;

    std::cout << "==== JSEQ SOLVER TEST ===" << std::endl;

    JSEQSolver JseqSolver(&fifo);
    MetaSolution* jseq_optim = JseqSolver.solve(instance);

    std::cout << "Solution reached:"; 
    jseq_optim->print();
    std::cout << std::endl;

    overall_score = fifo.evaluate_meta(*jseq_optim, instance);
    std::cout << "Score after optimisation :" << overall_score << std::endl;

    std::cout << "==== ESSWEIN WARM-SOLVER TEST ===" << std::endl;

    EssweinAlgorithm EWSolver(&fifo);
    EWSolver.set_initial_solution(*jseq_optim);
    MetaSolution* ew_sol_output = EWSolver.solve(instance);

    std::cout << "Solution reached:"; 
    ew_sol_output->print();
    std::cout << std::endl;

    overall_score = fifo.evaluate_meta(*ew_sol_output, instance);
    std::cout << "Score after optimisation :" << overall_score << std::endl;

    std::cout << "==== BEST-OF TEST ===" << std::endl;

    BestOfAlgorithm<SequenceMetaSolution> bestof(&fifo);
    bestof.set_initial_solution(listseq);
    std::cout<<"initial_list : ";
    listseq.print();
    std::cout<< "=>" << fifo.evaluate_meta(listseq, instance) <<"\n";
    MetaSolution* bestof_output = bestof.solve(instance);
    std::cout<<"bestof output : ";
    bestof_output->print();

    BestOfAlgorithm<GroupMetaSolution> bestof2(&fifo);
    bestof2.set_initial_solution(listgroup);
    std::cout<<"\n\ninitial_list : ";
    listgroup.print();
    std::cout<< "=>" << fifo.evaluate_meta(listgroup, instance) <<"\n";
    bestof_output = bestof2.solve(instance);
    std::cout<<"bestof output : ";  
    bestof_output->print();

    */
