#ifndef DATAINSTANCE_H
#define DATAINSTANCE_H

#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>

class DataInstance { // Instance data : bunch of scenarios
public:
    int N; //number of tasks
    int S; // number of scenarios in data
    std::vector<std::vector<int>> precedenceConstraints;
    std::vector<int> durations;
    std::vector<std::vector<int>> releaseDates;
    std::vector<int> dueDates;

    DataInstance(const std::string& filename) { //constructor : reads from data file
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Unable to open file" << std::endl;
            return;
        }

        std::string line;
        std::getline(file, line);
        std::stringstream ss(line);
        ss >> N >> S;

        std::getline(file, line); //eat spacing line

        // Read precedence constraints
        precedenceConstraints.resize(N, std::vector<int>(N, 0));
        for (int i = 0; i < N; ++i) {
            std::getline(file, line);
            ss.clear();
            ss.str(line);
            for (int j = 0; j < N; ++j) {
                ss >> precedenceConstraints[i][j];
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

    DataInstance(const DataInstance& originalInstance, int s) {
        if (s < 0 || s >= originalInstance.S) {
            throw std::invalid_argument("Scenario number out of range");
        }
        N = originalInstance.N;
        S = 1; // Only one scenario in the new instance
        precedenceConstraints = originalInstance.precedenceConstraints;
        durations = originalInstance.durations;
        releaseDates = {{originalInstance.releaseDates[s]}}; // Copy only one scenario's release dates
        dueDates = {originalInstance.dueDates}; // Copy only one scenario's due dates
    }

    void print() const {
        std::cout << "Number of tasks: " << N << std::endl;
        std::cout << "Number of scenarios: " << S << std::endl;

        std::cout << "Precedence constraints:" << std::endl;
        for (int i=0;i<N;i++) {
            for (int j=0;j<N;j++) {
                if(precedenceConstraints[i][j] == 1) {
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

    DataInstance single_instance(int s) const { //extracts the single instance of scenario s
        //make an instance by ref ?
        return DataInstance(*this, s);
    }
};

#endif // DATAINSTANCE_H