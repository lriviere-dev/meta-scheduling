#include <iostream>
#include "Schedule.h"
#include "Instance.h"
#include "Sequence.h"
#include "Policy.h"
#include "PolicyFifo.h"
#include "PolicySPT.h"
#include "MetaSolutions.h"
#include "ListMetaSolutions.h"
#include "Algorithms.h"
#include "BestOfAlgorithm.h"
#include "Ideal.h"
#include "Timer.h"

#include <iostream>
#include <string>
#include <stdexcept> 

template class ListMetaSolution<SequenceMetaSolution>;
template class ListMetaSolution<GroupMetaSolution>;
template class BestOfAlgorithm<SequenceMetaSolution>;
template class BestOfAlgorithm<GroupMetaSolution>;

//array to string helper
template<typename T>
std::string vec_to_string(const std::vector<T>& vec) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        oss << vec[i];
        if (i + 1 < vec.size()) oss << ", ";
    }
    oss << "]";
    return oss.str();
}

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

std::vector<SequenceMetaSolution> diversify_step_random (std::vector<SequenceMetaSolution>& input_solutions, const DataInstance& instance, Policy & policy,  std::mt19937 &rng) {
    std::vector<SequenceMetaSolution> output_solutions;
    output_solutions.insert(output_solutions.end(), input_solutions.begin(), input_solutions.end()); //insert the original solutions
    
    SwapDescent descent(&policy);
    size_t nb_new = instance.N/2; //simulating similar nb of solution than swap (/2 cause we insert starting random and descended)
    for (size_t i = 0; i < nb_new; i++)
    {
        Sequence rand_seq = Sequence(instance.N, rng);//gen random sequence
        rand_seq.fix_precedence_constraints(instance); //fix it for prec constraints
        SequenceMetaSolution rand_seq_meta = SequenceMetaSolution(rand_seq);
        descent.set_initial_solution(rand_seq_meta);
        MetaSolution* descended_seq_meta = descent.solve(instance);//descent on it
        output_solutions.push_back(rand_seq_meta);//add to list
        output_solutions.push_back(*(dynamic_cast<SequenceMetaSolution*>(descended_seq_meta)));
    }
    
    return output_solutions;
}

std::vector<SequenceMetaSolution> diversify_step_jseq (std::vector<SequenceMetaSolution>& input_solutions, const DataInstance& instance, JSEQSolver & jseqsolver) {
    
    int max_size = 100;
    
    std::vector<SequenceMetaSolution> output_solutions;
    output_solutions.insert(output_solutions.end(), input_solutions.begin(), input_solutions.end()); //insert the original solutions
    
    //solve again using solve_savestep (redundant of course, just for testing purposes)
    for (MetaSolution* metasol : jseqsolver.solve_steps(instance)){
        output_solutions.push_back(*(dynamic_cast<SequenceMetaSolution*>(metasol)));
    }

    // Keep only the last k elements
    if (output_solutions.size() > max_size) {
        output_solutions.erase(output_solutions.begin(), output_solutions.begin() + (output_solutions.size() - max_size));
    }
    return output_solutions;
}

std::vector<SequenceMetaSolution> diversify_step_ideal (std::vector<SequenceMetaSolution>& input_solutions, const DataInstance& instance, MetaSolution * ideal_sol) {
    std::vector<SequenceMetaSolution> output_solutions;
    output_solutions.insert(output_solutions.end(), input_solutions.begin(), input_solutions.end()); //insert the original solutions
    
    //solve again using solve_savestep (redundant of course, just for testing purposes)
    for (Sequence seq : (dynamic_cast<IdealMetaSolution*> (ideal_sol))->get_sequences()){
        output_solutions.push_back(SequenceMetaSolution(seq));
    }
    return output_solutions;
}

int main(int argc, char* argv[]) {
    // Default parameter values
    std::string file_name = "instances/test.data";  // Default file for tests
    int jseq_time = 10;                     // Time allocated to jseq solver (seconds)
    int nb_training_scenarios = 2;  //this is the number of training scenarios : S
    int sampling_iterations = 1; //number of times to repeat the sampling / solve / evaluation process (HIgher number is more significant)

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
              << "jseq_time: " << jseq_time << "\n"
              << "nb_training_scenarios: " << nb_training_scenarios << "\n"
              << "sampling_iterations: " << sampling_iterations << "\n";


    // Start of actual process
    std::mt19937 rng(42);  // Fixed seed for reproducibility, pass down to everything that requires randomness. Note that due to varying cpo performances, results may differ still. To ensure full reproducibility, start from the same output of CPO.

    std::cout << std:: endl << "==== Instance Summary ===" << std::endl;

    DataInstance instance(file_name);
    instance.print_summary();

    std::cout << std:: endl << "==== Experiment Start ===" << std::endl;

    //ideal policy and solvers
    IdealPolicy ideal; //ideal policy for bounds
    IdealSolver ideal_solver(&ideal, (jseq_time>10*60) ? 10*60 : jseq_time); //limiting time spent computing bounds because we don't even use them much.

    //fifo policy and solvers
    FIFOPolicy used_policy; //fifo policy
    //SPTPolicy used_policy; //spt policy
    PurePolicySolver PolicySolver(&used_policy);
    JSEQSolver JseqSolver(&used_policy, jseq_time);
    EssweinAlgorithm EWSolver(&used_policy);
    BestOfAlgorithm<SequenceMetaSolution> bestof_jseq(&used_policy);
    BestOfAlgorithm<GroupMetaSolution> bestof_gseq(&used_policy);

    //Output solutions declaration
    MetaSolution* ideal_train_solution, *ideal_test_solution, *pure_policy_solution, *jseq_solution, *gseq_solution, *sjseq_solution, *sgseq_solution, *clean_sjseq_solution, *clean_sgseq_solution;
    std::unordered_set<GroupMetaSolution> metaSet; //the map allows to not repeat identical EW steps between te different JSEQ. (results kept even for each scenario sampling)

    
    for (int i =0; i< sampling_iterations; i++){
        std::cout << std:: endl << "\tIteration " << i << std::endl;
        //Splitting instance randomly into training and testing.
        DataInstance trainInstance, testInstance;
        std::tie(trainInstance, testInstance) = instance.SampleSplitScenarios(nb_training_scenarios, rng);

        //Ideal bounds for score for both training and test instances
        ideal_train_solution = ideal_solver.solve(trainInstance);
        ideal_test_solution = ideal_solver.solve(testInstance);
        std::cout<<"Ideal training score : " << ideal.evaluate_meta(*ideal_train_solution, trainInstance) << std::endl; 
        std::cout<<"Ideal testing score : " << ideal.evaluate_meta(*ideal_test_solution, testInstance) << std::endl; 
        std::cout<<"Ideal testing scenario scores : " << vec_to_string(ideal_test_solution->get_scores(ideal, testInstance)) << std::endl; 
        std::cout<<"Ideal testing 90q : " << ideal_test_solution->get_quantile(0.9, ideal, testInstance) << std::endl; 

        //Pure policy -> Fully reactive solution
        pure_policy_solution = PolicySolver.solve(trainInstance); //resolving isn't necessary as the solution is identical no matter the input training scenarios, however, solve time is negligeable
        std::cout<<"Pure policy training score : " << used_policy.evaluate_meta(*pure_policy_solution, trainInstance) << std::endl; 
        std::cout<<"Pure policy testing score : " << used_policy.evaluate_meta(*pure_policy_solution, testInstance) << std::endl; 
        std::cout<<"Pure policy testing scenario scores : " << vec_to_string(pure_policy_solution->get_scores(used_policy, testInstance)) << std::endl; 
        std::cout<<"Pure policy testing 90q : " << pure_policy_solution->get_quantile(0.9, used_policy, testInstance) << std::endl; 

        //SLALGO PROCESS:
        //CPO -> JSEQ solution
        jseq_solution = JseqSolver.solve(trainInstance);
        std::cout<<"JSEQ training score : " << used_policy.evaluate_meta(*jseq_solution, trainInstance) << std::endl; 
        std::cout<<"JSEQ testing score : " << used_policy.evaluate_meta(*jseq_solution, testInstance) << std::endl; 
        std::cout<<"JSEQ testing scenario scores : " << vec_to_string(jseq_solution->get_scores(used_policy, testInstance)) << std::endl; 
        std::cout<<"JSEQ testing 90q : " << jseq_solution->get_quantile(0.9, used_policy, testInstance) << std::endl; 

        //EW -> GSEQ solution
        EWSolver.set_initial_solution(*jseq_solution);
        gseq_solution = EWSolver.solve(trainInstance);
        std::cout<<"GSEQ training score : " << used_policy.evaluate_meta(*gseq_solution, trainInstance) << std::endl; 
        std::cout<<"GSEQ testing score : " << used_policy.evaluate_meta(*gseq_solution, testInstance) << std::endl; 
        std::cout<<"GSEQ testing scenario scores : " << vec_to_string(gseq_solution->get_scores(used_policy, testInstance)) << std::endl; 
        std::cout<<"GSEQ testing 90q : " << gseq_solution->get_quantile(0.9, used_policy, testInstance) << std::endl; 


        //Diversify JSEQ solution
        std::vector<SequenceMetaSolution> AllSolutionsSeq;
        AllSolutionsSeq.push_back(*(dynamic_cast<SequenceMetaSolution*>(jseq_solution)));
        std::vector<SequenceMetaSolution> diversifiedSeq = diversify_step(AllSolutionsSeq, trainInstance);
        //std::vector<SequenceMetaSolution> diversifiedSeq = diversify_step_jseq(AllSolutionsSeq, trainInstance, JseqSolver);
        //std::vector<SequenceMetaSolution> diversifiedSeq = diversify_step_random(AllSolutionsSeq, trainInstance, fifo, rng);
        //std::vector<SequenceMetaSolution> diversifiedSeq = diversify_step_ideal(AllSolutionsSeq, trainInstance, ideal_train_solution);
        std::cout << "number of diversifiedsol jseq :" <<diversifiedSeq.size()<<std::endl;
        /*for (auto & sol : diversifiedSeq){
            std::cout << "score :" << fifo.evaluate_meta(sol, trainInstance) <<std::endl;
            sol.print();
            std::cout << std::endl;
            if (!sol.get_sequence().check_precedence_constraints(instance)){throw std::runtime_error("invalid");}
        }*/

        //BO(JSEQ) -> SJSEQ solution
        ListMetaSolution<SequenceMetaSolution> listseqmetasol(diversifiedSeq);
        bestof_jseq.set_initial_solution(listseqmetasol);
        {
        Timer timer("BO SJSEQ timer");
        sjseq_solution = bestof_jseq.solve(trainInstance); 
        }
        std::cout<<"SJSEQ training score : " << used_policy.evaluate_meta(*sjseq_solution, trainInstance) << std::endl;                 
        std::cout<<"SJSEQ testing score : " << used_policy.evaluate_meta(*sjseq_solution, testInstance) << std::endl; 
        std::cout<<"SJSEQ testing scenario scores : " << vec_to_string(sjseq_solution->get_scores(used_policy, testInstance)) << std::endl; 
        std::cout<<"SJSEQ testing 90q : " << sjseq_solution->get_quantile(0.9, used_policy, testInstance) << std::endl; 


        //SJSEQ front evaluation
        clean_sjseq_solution = (dynamic_cast<ListMetaSolution<SequenceMetaSolution>*>(sjseq_solution))->front_sub_metasolutions(&used_policy, trainInstance);
        std::cout<<"SJSEQ Front training score : " << used_policy.evaluate_meta(*clean_sjseq_solution, trainInstance) << std::endl; 
        std::cout<<"SJSEQ Front testing score : " << used_policy.evaluate_meta(*clean_sjseq_solution, testInstance) << std::endl; 
        std::cout<<"SJSEQ Front testing scenario scores : " << vec_to_string(clean_sjseq_solution->get_scores(used_policy, testInstance)) << std::endl; 
        std::cout<<"SJSEQ Front testing 90q : " << clean_sjseq_solution->get_quantile(0.9, used_policy, testInstance) << std::endl; 


        //EW diversification 
        std::vector<GroupMetaSolution> AllSolutionsGroup;
        {
        Timer timer("EW step timer");
        for (size_t i=0; i<diversifiedSeq.size();i++){ 
            EWSolver.set_initial_solution(diversifiedSeq[i]);
            EWSolver.solve_savesteps(trainInstance, metaSet);
        }
        }
        std::cout << "number of diversifiedsol gseq :" <<metaSet.size()<<std::endl;

        AllSolutionsGroup.assign(metaSet.begin(), metaSet.end());
        AllSolutionsGroup.push_back(*dynamic_cast<GroupMetaSolution*>(pure_policy_solution)); //inserting the fifo fully permutable solution to make sure score is at least as good (very likely to be removed by GSEQ)
        AllSolutionsGroup[AllSolutionsGroup.size()-1].reset_evaluation();

        int best_GSEQ_sofar = 0;
        for (int k = 0; k<AllSolutionsGroup.size(); k++){
            if (!AllSolutionsGroup[k].scored_by){used_policy.evaluate_meta( AllSolutionsGroup[k], trainInstance);}//not checking if scored for correct set here. COuld do but should be fine
            if (AllSolutionsGroup[k].score <  AllSolutionsGroup[best_GSEQ_sofar].score){best_GSEQ_sofar = k;}
        }
        std::cout<<"Best GSEQ training score : " << used_policy.evaluate_meta(AllSolutionsGroup[best_GSEQ_sofar], trainInstance) << std::endl; //check to see if best-of is usefulll
        std::cout<<"Best GSEQ testing score : " << used_policy.evaluate_meta(AllSolutionsGroup[best_GSEQ_sofar], testInstance) << std::endl; 


        //BO(GSEQ) -> SGSEQ solution
        ListMetaSolution<GroupMetaSolution> listgroupmetasol(AllSolutionsGroup);
        bestof_gseq.set_initial_solution(listgroupmetasol);
        {
        Timer timer("BO SGSEQ timer");
        sgseq_solution = bestof_gseq.solve(trainInstance); 
        }
        std::cout<<"SGSEQ training score : " << used_policy.evaluate_meta(*sgseq_solution, trainInstance) << std::endl; 
        std::cout<<"SGSEQ testing score : " << used_policy.evaluate_meta(*sgseq_solution, testInstance) << std::endl; 
        std::cout<<"SGSEQ testing scenario scores : " << vec_to_string(sgseq_solution->get_scores(used_policy, testInstance)) << std::endl; 
        std::cout<<"SGSEQ testing 90q : " << sgseq_solution->get_quantile(0.9, used_policy, testInstance) << std::endl; 


        //SGSEQ front evaluation
        clean_sgseq_solution = (dynamic_cast<ListMetaSolution<GroupMetaSolution>*>(sgseq_solution))->front_sub_metasolutions(&used_policy, trainInstance);
        std::cout<<"SGSEQ Front training score : " << used_policy.evaluate_meta(*clean_sgseq_solution, trainInstance) << std::endl; 
        std::cout<<"SGSEQ Front testing score : " << used_policy.evaluate_meta(*clean_sgseq_solution, testInstance) << std::endl; //note that the "front" of a SGSEQ also should have the same training score, however, there can exists several different fronts, that can behave differently in testing. The front isn't unique
        std::cout<<"SGSEQ Front testing scenario scores : " << vec_to_string(clean_sgseq_solution->get_scores(used_policy, testInstance)) << std::endl; 
        std::cout<<"SGSEQ Front testing 90q : " << clean_sgseq_solution->get_quantile(0.9, used_policy, testInstance) << std::endl; 



        std::cout << std:: endl << "==== Solutions dump ===" << std::endl;
        //debug solution dumps
        std::vector<MetaSolution*> outputs = {ideal_train_solution, ideal_test_solution, pure_policy_solution, jseq_solution, gseq_solution, clean_sjseq_solution, clean_sgseq_solution};
        for (size_t i = 0; i<outputs.size(); i++){
            std::cout << "Output " << typeid(*outputs[i]).name() << ": " ;
            outputs[i]->print();
            std::cout << std::endl;
        }
    }


    std::cout << std:: endl << "==== Experiment end ===" << std::endl;

    return 0;
}

