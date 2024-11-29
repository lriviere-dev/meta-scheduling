#include <iostream>
#include "MetaSolutions.h"
#include "ListMetaSolutions.h"
#include "Policy.h"
#include "Instance.h"
#include "Sequence.h"
#include "Schedule.h"
#include "Algorithms.h"


int main() {
    std::cout << "==== INSTANCE SCENARIO TEST ===" << std::endl;
    //Loading up instance/scenario
    DataInstance instance("instances/test.data");
    DataInstance single_scenario(instance,0);
    single_scenario.print();

    FIFOPolicy fifo; //defining policy

    std::cout << std::endl << std::endl;

    std::cout << "==== JSEQ TEST ===" << std::endl;
    // Example group sequence
    std::vector<int> taskseq = {
            2,0,1   
        };
    Sequence taskseqbis = Sequence(taskseq);
    // Test GroupMetaSolution
    SequenceMetaSolution myseq(taskseqbis);
    //SequenceMetaSolution myseq(taskseq); //both versions should work
    std::cout << "MetaSolution (JSEQ) :";
    myseq.print();
    std::cout << std::endl;

    //extract sequence from meta solution
    Sequence selected_sequence = fifo.extract_sequence(myseq, single_scenario);
    std::cout << "Selected sequence :";
    selected_sequence.print();
    std::cout << std::endl << std::endl;


    std::cout << "==== GROUP SEQUENCE TEST ===" << std::endl;
        // Example group sequence
    std::vector<std::vector<int>> taskGroups = {
            {1},    // Group 1:
            {0,2},       // Group 2:
        };

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
    std::vector<int> taskseq2 = {
            1,0,2   
        };
    Sequence taskseqbis2 = Sequence(taskseq2);
    std::vector<SequenceMetaSolution> vectorseq = {taskseqbis, taskseqbis2};

    // another group sequence
    std::vector<std::vector<int>> taskGroups2 = {
            {0,1},    // Group 1:
            {2},       // Group 2:
        };
    GroupMetaSolution mygroup2(taskGroups);
    std::vector<GroupMetaSolution> vectorgroup = {taskGroups, taskGroups2};

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

    PureFiFoSolver PureFifo(&fifo);
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

    //evaluate 
    overall_score = fifo.evaluate_meta(*jseq_optim, instance);
    std::cout << "Score after optimisation :" << overall_score << std::endl;

    return 0;
}