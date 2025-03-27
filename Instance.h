#ifndef DATAINSTANCE_H
#define DATAINSTANCE_H

#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <random>
#include <algorithm>
#include <numeric>

class DataInstance { // Instance data : bunch of scenarios
public:
    std::string file_name;
    int N; //number of tasks
    int S; // number of scenarios in data
    std::vector<uint8_t> precedenceConstraints;
    std::vector<int> durations;
    std::vector<std::vector<int>> releaseDates;
    std::vector<int> dueDates;

    DataInstance() {} //empty constructor


    DataInstance(const std::string& filename) { //constructor : reads from data file
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Unable to open file" << std::endl;
            return;
        }

        file_name = filename;

        std::string line;
        std::getline(file, line);
        std::stringstream ss(line);
        ss >> N >> S;

        std::getline(file, line); //eat spacing line

        // Read precedence constraints
        precedenceConstraints.resize(N * N);
        for (int i = 0; i < N; ++i) {
            std::getline(file, line);
            ss.clear();
            ss.str(line);
            for (int j = 0; j < N; ++j) {
                //ss >> precedenceConstraints[i*N+j]; //DOES NOT work with the uint8_t type, conversion fucks up
                char c;
                ss >> c;  // Read as char ('0' or '1')
                precedenceConstraints[i * N + j] = c - '0'; // Convert to binary 0 or 1
            }
        }

        std::getline(file, line); //eat spacing line

        // Read durations
        std::getline(file, line);
        ss.clear();
        ss.str(line);
        durations.resize(N);
        for (int i = 0; i < N; ++i) {
            ss >> durations[i];
        }

        std::getline(file, line); //eat spacing line

        // Read release dates
        releaseDates.resize(S, std::vector<int>(N));
        for (int i = 0; i < S; ++i) {
            std::getline(file, line);
            ss.clear();
            ss.str(line);
            for (int j = 0; j < N; ++j) {
                ss >> releaseDates[i][j];
            }
        }

        std::getline(file, line); //eat spacing line

        // Read due dates
        std::getline(file, line);
        ss.clear();
        ss.str(line);
        dueDates.resize(N);
        for (int i = 0; i < N; ++i) {
            ss >> dueDates[i];
        }
        file.close();

    }

    //more convenient getter for prec constraints
    inline bool get_prec(int task1, int task2) const {
        return precedenceConstraints[task1 * N + task2];
    }

    void print() const {
        std::cout << "Number of tasks: " << N << std::endl;
        std::cout << "Number of scenarios: " << S << std::endl;

        std::cout << "Precedence constraints:" << std::endl;
        for (int i=0;i<N;i++) {
            for (int j=0;j<N;j++) {
                if(precedenceConstraints[i*N+j] == 1) {
                    std::cout << i << "->" << j << std::endl;
                }
            }
        }

        std::cout << "Durations: ";
        for (int duration : durations) {
            std::cout << duration << " ";
        }
        std::cout << std::endl;

        std::cout << "Release dates:" << std::endl;
        for (const auto& row : releaseDates) {
            for (int val : row) {
                std::cout << val << " ";
            }
            std::cout << std::endl;
        }

        std::cout << "Due dates: ";
        for (int dueDate : dueDates) {
            std::cout << dueDate << " ";
        }
        std::cout << std::endl;
    }

    void print_summary() const {
        std::cout << "Number of tasks: " << N << std::endl;
        std::cout << "Number of scenarios: " << S << std::endl;

        int c= 0;
        for (int i=0;i<N;i++) {
            for (int j=0;j<N;j++) {
                if(precedenceConstraints[i*N+j] == 1) {
                    c++; //;)
                }
            }
        }
        std::cout << "Precedence constraints: " << c << std::endl;
        std::cout << "Precedence constraints ratio: " << static_cast<double>(c)*2/(N*N) << std::endl;
    }

    std::pair<DataInstance, DataInstance> SampleSplitScenarios(int k, std::mt19937 &rng) {
        // Check that k is not larger than the available number of scenarios.
        if (k > S) {
            throw std::runtime_error("Cannot sample more scenarios than are available in the instance.");
        }

        // Create a vector with indices 0, 1, ..., instance.S - 1.
        std::vector<int> indices(S);
        std::iota(indices.begin(), indices.end(), 0);

        // Shuffle the indices using a random number generator.
        std::shuffle(indices.begin(), indices.end(), rng);

        //print scenarios used for training
        std::cout << "Training scenarios : ";
        for (int i =0; i<k-1; i++){std::cout << indices[i] << ", ";}
        std::cout << indices[k-1] << std::endl;


        // Create copies for training and testing instances.
        DataInstance trainInstance = *this;
        DataInstance testInstance = *this;

        // Set the number of scenarios in each instance.
        trainInstance.S = k;
        testInstance.S = S - k;

        // Clear and resize releaseDates for both instances.
        trainInstance.releaseDates.clear();
        trainInstance.releaseDates.reserve(k);
        testInstance.releaseDates.clear();
        testInstance.releaseDates.reserve(S - k);

        // Assign scenarios based on shuffled indices.
        for (int i = 0; i < k; ++i) {
            trainInstance.releaseDates.push_back(releaseDates[indices[i]]);
        }
        for (int i = k; i < S; ++i) {
            testInstance.releaseDates.push_back(releaseDates[indices[i]]);
        }

        return {trainInstance, testInstance};
    }
};

#endif // DATAINSTANCE_H