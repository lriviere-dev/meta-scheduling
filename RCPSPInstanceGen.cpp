#include "Instance.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <random>
#include <cmath>

namespace fs = std::filesystem;
std::string format_num(double val) {
    std::ostringstream oss;
    oss << std::noshowpoint << val; 
    return oss.str();
}
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

// 1. Updated signature to accept the output directory
void generateRCPSPScenarios(const fs::path& inputPath, const fs::path& outputDir, int numScenarios, int gamma) {
    RCPSPInstance base(inputPath.string());
    std::mt19937 gen(12345);

    // 2. Build the filename
    std::string baseName = inputPath.stem().string();
    std::string outFileName = baseName + "_N" + std::to_string(base.N) + 
                              "_S" + std::to_string(numScenarios) + 
                              "_gamma" + format_num(gamma) + ".data";

    // 3. Combine outputDir with the new filename
    fs::path finalPath = outputDir / outFileName;

    // Generate base release dates (1.2 to 1.5 times earliest start, as in the paper)
    std::vector<int> earliestStarts(base.N); //theoretical earliest start base on release and precedences
    std::vector<int> nominalReleaseDates(base.N); //actual release date of tasks before variations
    std::vector<int> offsets(base.N); //Offset added to the release dates if it is selected for variation
    std::uniform_real_distribution<double> rand_release(1.2, 1.5);
    std::uniform_real_distribution<double> rand_offset(5, 20);

    for (int j = 0; j < base.N; ++j) {
        earliestStarts[j] = 0;
        for (int i = 0; i < j; ++i) {
            if (base.get_prec(i, j)) {
                earliestStarts[j] = std::max(earliestStarts[j], nominalReleaseDates[i]+ base.durations[i]); //assumes precedence constraints only go from lower to higher lexicographically
            }
        }
        if (j == 0 || j == base.N - 1) { //making sure first and last tasks (dummy) have 0 release dates
            nominalReleaseDates[j] = 0;
        } else {
            nominalReleaseDates[j] = (int)(earliestStarts[j] * rand_release(gen));
        }
        offsets[j] = rand_offset(gen);
    }

    // //print for debug : base release dates of tasks and variation values
    // std::cout << "Base release dates: " << vec_to_string(nominalReleaseDates) << std::endl;
    // std::cout << "Offsets: " << vec_to_string(offsets) << std::endl;

    // ... [Scenario generation logic] ...
    std::vector<std::vector<int>> scenarioReleaseDates(numScenarios, std::vector<int>(base.N));
    for (int s = 0; s < numScenarios; ++s) {
        // Selection of indices to be "flipped" (the budget)
        std::vector<int> indices(base.N-2); // Exclude first and last task
        std::iota(indices.begin(), indices.end(), 1);
        std::shuffle(indices.begin(), indices.end(), gen);

        // Initial pass: Set all jobs to nominal base values
        for (int i = 0; i < base.N; ++i) {
            scenarioReleaseDates[s][i] = nominalReleaseDates[i];
        }

        // Apply the offset to exactly 'gamma' random jobs (or all if gamma > N-2)
        int effective_gamma = std::min(gamma, base.N-2);
        for (int k = 0; k < effective_gamma; ++k) {
            int job_idx = indices[k];
            scenarioReleaseDates[s][job_idx] += offsets[job_idx];
        }
    }

    // 4. Use finalPath for the destination stream
    std::ifstream src(inputPath, std::ios::binary);
    std::ofstream dst(finalPath, std::ios::binary); 
    if (!dst) {
        std::cerr << "Error: Could not create file " << finalPath << std::endl;
        return;
    }
    dst << src.rdbuf(); 
    src.close();

    // 5. Append Uncertainty Section
    dst << "\n************************************************************************\n";
    dst << "UNCERTAINTY DATA:\n";
    dst << "original file     : " << inputPath.filename().string() << "\n";
    dst << "scenarios         : " << numScenarios << "\n";
    dst << "gamma             : " << gamma << "\n";
    dst << "RELEASE DATES SCENARIOS:\n";
    
    for (int s = 0; s < numScenarios; ++s) {
        for (int j = 0; j < base.N; ++j) {
            dst << scenarioReleaseDates[s][j] << (j == base.N - 1 ? "" : " ");
        }
        dst << "\n";
    }
    dst.close();
    
    std::cout << "Generated: " << finalPath.string() << std::endl;
}

int main() {
    std::string inputFolder = "./instances/test_folder";
    fs::path sourcePath(inputFolder);

    if (!fs::exists(sourcePath) || !fs::is_directory(sourcePath)) {
        std::cerr << "Error: Input folder does not exist." << std::endl;
        return 1;
    }

    // Create the output directory path
    std::string outputFolderName = "sample_" + sourcePath.filename().string();
    fs::path outputDir = sourcePath.parent_path() / outputFolderName;

    if (!fs::exists(outputDir)) {
        fs::create_directories(outputDir); // Changed to create_directories for safety
    }

    const int S = 1000;
    const std::vector<int> gamma = {3,10};

    for (const auto& entry : fs::directory_iterator(sourcePath)) {
        if (entry.is_regular_file()) {
            for (int gamma : gamma) {
                // Pass the outputDir here
                generateRCPSPScenarios(entry.path(), outputDir, S, gamma);
            }
        }
    }

    return 0;
}