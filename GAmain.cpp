#include <iostream>
#include <string>
#include <random>

#include "Instance.h"
#include "Policy.h"
#include "PolicyFifo.h"
#include "MetaSolutions.h"
#include "ListMetaSolutions.h"
#include "GeneticAlgorithm.h"
#include "Ideal.h"
#include "Timer.h"
#include "Diversity.h"

int main(int argc, char* argv[]) {
    // --- Parameters ---
    std::string file_name          = "instances/test.data";
    int         jseq_time          = 10;
    int         nb_train_scenarios = 1;
    int         epochs             = 10;
    int         mutations_per_epoch = 100;

    if (argc > 1) file_name           = argv[1];
    if (argc > 2) jseq_time           = std::stoi(argv[2]);
    if (argc > 3) nb_train_scenarios  = std::stoi(argv[3]);
    if (argc > 4) epochs              = std::stoi(argv[4]);
    if (argc > 5) mutations_per_epoch = std::stoi(argv[5]);

    std::cout << "=== GA Experiment ===" << std::endl;
    std::cout << "file: "               << file_name           << "\n"
              << "jseq_time: "          << jseq_time           << "\n"
              << "train_scenarios: "    << nb_train_scenarios  << "\n"
              << "epochs: "             << epochs              << "\n"
              << "mutations_per_epoch: "<< mutations_per_epoch << "\n";

    std::mt19937 rng(42);

    // --- Instance ---
    SingleMachineInstance instance(file_name);
    instance.print_summary();

    // --- Train/test split ---
    DataInstance *trainInstance, *testInstance;
    std::tie(trainInstance, testInstance) =
        instance.SampleSplitScenarios(nb_train_scenarios, rng, true);

    // --- Policy ---
    FIFOPolicy policy;

    // --- Run GA ---
    GeneticAlgorithm ga(&policy, epochs, mutations_per_epoch, rng);

    std::cout << "\nBuilding initial pool...\n" << std::endl;
    ListMetaSolution* pool = ga.build_initial_pool(*trainInstance, jseq_time);
    policy.evaluate_meta(*pool, *trainInstance); //evaluate the initial pool 

    std::cout << "\nInitial pool built. Starting GA...\n" << std::endl;
    ListMetaSolution* result = ga.solve(*pool, *trainInstance, *testInstance);

    // --- Log ---
    std::cout << "\nLogging results...\n" << std::endl;
    ga.get_logger().print();

    return 0;
}