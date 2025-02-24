#include <iostream>
#include "Schedule.h"
#include "Instance.h"
#include "Sequence.h"
#include "Policy.h"
#include "MetaSolutions.h"
#include "ListMetaSolutions.h"
#include "Algorithms.h"
#include "BestOfAlgorithm.h"
#include "Ideal.h"

#include <iostream>
#include <string>
#include <stdexcept> 

template class ListMetaSolution<SequenceMetaSolution>;
template class ListMetaSolution<GroupMetaSolution>;
template class BestOfAlgorithm<SequenceMetaSolution>;
template class BestOfAlgorithm<GroupMetaSolution>;

std::vector<SequenceMetaSolution> diversify_step (std::vector<SequenceMetaSolution>& input_solutions, const DataInstance& instance) {
    int steps = 1; //TODO : could be a parameter.
    int neighborhood_size = 1; //TODO : could be a parameter.
    std::vector<SequenceMetaSolution> output_solutions;
    output_solutions.insert(output_solutions.end(), input_solutions.begin(), input_solutions.end()); //insert the original solutions

    for (int step = 0; step < steps; step++) //repeat process depending on diversification strenghth (this will create duplicate solutions as inverse step can be taken. Could be done smarter)
    {
        for (size_t i = 0; i < input_solutions.size(); i++)
        {
            SequenceMetaSolution current = input_solutions[i];
            std::vector<SequenceMetaSolution> neighbors = current.gen_neighbors(neighborhood_size, instance); //instance is passed down to check precedence constraints
            output_solutions.insert(output_solutions.end(), neighbors.begin(), neighbors.end());
        }
    }
    return output_solutions;
}

int main(int argc, char* argv[]) {
    // Default parameter values
    std::string file_name = "instances/test.data";  // Default file for tests
    int jseq_time = 10;                     // Time allocated to jseq solver (seconds)
    int nb_training_scenarios = 5;  //this is the number of training scenarios : S

    // Command-line arguments override defaults:
    // Usage: ./program <intParam> <fileName> <doubleParam>

    if (argc > 1) { // first argument: file name
        file_name = argv[1];
    }

    if (argc > 2) { // second argument
        try {
            jseq_time = std::stoi(argv[2]);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Invalid integer parameter. Using default: " << jseq_time << "\n";
        }
    }

    if (argc > 3) { // sethirdcond argument
        try {
            nb_training_scenarios = std::stoi(argv[3]);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Invalid integer parameter. Using default: " << nb_training_scenarios<< "\n";
        }
    }

    // Display the parameters used
    std::cout << std:: endl << "==== Input (or default) parameters ===" << std::endl;

    std::cout << "file_name: " << file_name << "\n"
              << "jseq_time: " << jseq_time << "\n";


    // Start of actual process
    std::mt19937 rng(42);  // Fixed seed for reproducibility, pass down to everything that requires randomness. Note that due to varying cpo performances, results may differ still. To ensure full reproducibility, start from the same output of CPO.

    std::cout << std:: endl << "==== Instance Summary ===" << std::endl;

    DataInstance instance(file_name);
    instance.print_summary();

    std::cout << std:: endl << "==== Experiment Start ===" << std::endl;

    //ideal policy and solvers
    IdealPolicy ideal; //ideal policy for bounds
    IdealSolver ideal_solver(&ideal, jseq_time);

    //fifo policy and solvers
    FIFOPolicy fifo; //fifo policy
    PureFiFoSolver FifoSolver(&fifo);
    JSEQSolver JseqSolver(&fifo, jseq_time);
    EssweinAlgorithm EWSolver(&fifo);
    BestOfAlgorithm<SequenceMetaSolution> bestof_jseq(&fifo);
    BestOfAlgorithm<GroupMetaSolution> bestof_gseq(&fifo);

    //Output solutions declaration
    MetaSolution* ideal_train_solution, *ideal_test_solution, *fifo_solution, *jseq_solution, *gseq_solution, *sjseq_solution, *sgseq_solution, *clean_sjseq_solution, *clean_sgseq_solution;
    std::unordered_set<GroupMetaSolution> metaSet; //the map allows to not repeat identical EW steps between te different JSEQ. (results kept even for each scenario sampling)


    int sampling_iterations = 1; //number of times to repeat the sampling / solve / evaluation process (HIgher number is more significant)
    
    for (int i =0; i< sampling_iterations; i++){
        //Splitting instance randomly into training and testing.
        DataInstance trainInstance, testInstance;
        std::tie(trainInstance, testInstance) = instance.SampleSplitScenarios(nb_training_scenarios, rng);

        //Ideal bounds for score for both training and test instances
        ideal_train_solution = ideal_solver.solve(trainInstance);
        ideal_test_solution = ideal_solver.solve(testInstance);
        std::cout<<"Ideal training score : " << ideal.evaluate_meta(*ideal_train_solution, trainInstance) << std::endl; 
        std::cout<<"Ideal testing score : " << ideal.evaluate_meta(*ideal_test_solution, testInstance) << std::endl; 

        //FIFO -> Fully reactive solution
        fifo_solution = FifoSolver.solve(trainInstance); //resolving isn't necessary as the solution is identical no matter the input training scenarios, however, solve time is negligeable
        std::cout<<"Fifo training score : " << fifo.evaluate_meta(*fifo_solution, trainInstance) << std::endl; 
        std::cout<<"Fifo testing score : " << fifo.evaluate_meta(*fifo_solution, testInstance) << std::endl; 

        //SLALGO PROCESS:
        //CPO -> JSEQ solution
        jseq_solution = JseqSolver.solve(trainInstance);
        std::cout<<"JSEQ training score : " << fifo.evaluate_meta(*jseq_solution, trainInstance) << std::endl; 
        std::cout<<"JSEQ testing score : " << fifo.evaluate_meta(*jseq_solution, testInstance) << std::endl; 

        //EW -> GSEQ solution
        EWSolver.set_initial_solution(*jseq_solution);
        gseq_solution = EWSolver.solve(trainInstance);
        std::cout<<"GSEQ training score : " << fifo.evaluate_meta(*gseq_solution, trainInstance) << std::endl; 
        std::cout<<"GSEQ testing score : " << fifo.evaluate_meta(*gseq_solution, testInstance) << std::endl; 

        //Diversify JSEQ solution
        std::vector<SequenceMetaSolution> AllSolutionsSeq;
        AllSolutionsSeq.push_back(*(dynamic_cast<SequenceMetaSolution*>(jseq_solution)));
        std::vector<SequenceMetaSolution> diversifiedSeq = diversify_step(AllSolutionsSeq, trainInstance);

        //BO(JSEQ) -> SJSEQ solution
        ListMetaSolution<SequenceMetaSolution> listseqmetasol(diversifiedSeq);
        bestof_jseq.set_initial_solution(listseqmetasol);
        sjseq_solution = bestof_jseq.solve(trainInstance); 
        std::cout<<"SJSEQ training score : " << fifo.evaluate_meta(*sjseq_solution, trainInstance) << std::endl;                 
        std::cout<<"SJSEQ testing score : " << fifo.evaluate_meta(*sjseq_solution, testInstance) << std::endl; 


        //SJSEQ front evaluation
        clean_sjseq_solution = (dynamic_cast<ListMetaSolution<SequenceMetaSolution>*>(sjseq_solution))->front_sub_metasolutions(&fifo, trainInstance);
        std::cout<<"SJSEQ (Front) training score : " << fifo.evaluate_meta(*clean_sjseq_solution, trainInstance) << std::endl; 
        std::cout<<"SJSEQ (Front) testing score : " << fifo.evaluate_meta(*clean_sjseq_solution, testInstance) << std::endl; 


        //EW diversification 
        std::vector<GroupMetaSolution> AllSolutionsGroup;
        for (size_t i=0; i<diversifiedSeq.size();i++){ 
            EWSolver.set_initial_solution(diversifiedSeq[i]);
            EWSolver.solve_savesteps(trainInstance, metaSet);
        }
        AllSolutionsGroup.assign(metaSet.begin(), metaSet.end());

        //BO(GSEQ) -> SGSEQ solution
        ListMetaSolution<GroupMetaSolution> listgroupmetasol(AllSolutionsGroup);
        bestof_gseq.set_initial_solution(listgroupmetasol);
        sgseq_solution = bestof_gseq.solve(trainInstance); 
        std::cout<<"SGSEQ training score : " << fifo.evaluate_meta(*sgseq_solution, trainInstance) << std::endl; 
        std::cout<<"SGSEQ testing score : " << fifo.evaluate_meta(*sgseq_solution, testInstance) << std::endl; 


        //SGSEQ front evaluation
        clean_sgseq_solution = (dynamic_cast<ListMetaSolution<GroupMetaSolution>*>(sgseq_solution))->front_sub_metasolutions(&fifo, trainInstance);
        std::cout<<"SGSEQ (Front) training score : " << fifo.evaluate_meta(*clean_sgseq_solution, trainInstance) << std::endl; 
        std::cout<<"SGSEQ (Front) testing score : " << fifo.evaluate_meta(*clean_sgseq_solution, testInstance) << std::endl; //note that the "front" of a SGSEQ also should have the same training score, however, there can exists several different fronts, that can behave differently in testing. The front isn't unique
    }

    //Average results etc... TODO


    std::cout << std:: endl << "==== Solutions dump ===" << std::endl;
    //debug solution dumps
    std::vector<MetaSolution*> outputs = {ideal_train_solution, ideal_test_solution, fifo_solution, jseq_solution, gseq_solution, clean_sjseq_solution, clean_sgseq_solution};
    for (size_t i = 0; i<outputs.size(); i++){
        std::cout << "Output " << i << ": " << typeid(*outputs[i]).name() << std::endl;
        std::cout << "Solution: ";
        outputs[i]->print();
        std::cout << std::endl;
    }

    return 0;
}

