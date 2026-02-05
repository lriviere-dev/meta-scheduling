#include "Instance.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

int main() {
    try {
        // Define the target file from the PSPLIB dataset
        std::string filename = "instances/j120.sm.tgz/j1201_1.sm";
        
        // Initialize the RCPSP instance
        // This invokes the constructor logic to parse the .sm file format
        RCPSPInstance instance(filename);

        std::cout << "========================================" << std::endl;
        std::cout << "   RCPSP Instance Reading Test" << std::endl;
        std::cout << "========================================" << std::endl;

        // 1. Basic Summary Info
        std::cout << "Filename: " << instance.get_file_name() << std::endl;
        instance.print_summary();

        // 2. Resource Capacity Verification
        // Expected from file: R1: 14, R2: 12, R3: 13, R4: 9
        std::cout << "\n--- Resource Capacities ---" << std::endl;
        const std::vector<int> expected_caps = {14, 12, 13, 9};
        for (int r = 0; r < instance.num_resources; ++r) {
            std::cout << "Resource R" << (r + 1) << ": " << instance.capacities[r] 
                      << " (Expected: " << expected_caps[r] << ")" << std::endl;
        }

        // 3. Specific Job Data Verification
        // Testing Job 2 (Index 1) 
        // Expected from file: Duration 6, R1 usage 9, others 0
        std::cout << "\n--- Job 2 (Index 1) Verification ---" << std::endl;
        std::cout << "Duration: " << instance.durations[1] << " (Expected: 6)" << std::endl;
        std::cout << "Resource Usages: ";
        for (int r = 0; r < instance.num_resources; ++r) {
            std::cout << "R" << (r + 1) << ":" << instance.usages[1][r] << " ";
        }
        std::cout << std::endl;

        // 4. Precedence Relation Verification
        // Job 1 (Index 0) should have successors 2, 3, and 4
        std::cout << "\n--- Precedence Verification (Job 1) ---" << std::endl;
        std::cout << "Successors found in data: ";
        int successor_count = 0;
        for (int j = 0; j < instance.getN(); ++j) {
            if (instance.get_prec(0, j)) {
                std::cout << (j + 1) << " ";
                successor_count++;
            }
        }
        std::cout << "\nTotal Successors: " << successor_count << " (Expected: 3)" << std::endl;

        std::cout << "\n========================================" << std::endl;
        std::cout << "Test Finished Successfully" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\nFATAL ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}