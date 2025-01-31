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

    std::vector<DataInstance> scenarios;
    int scenario_id = -1;

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
        for (int s=0; s<S ;s++){
            scenarios.push_back(DataInstance(*this, s));
        }
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
        scenario_id = s; // mark the id of the scenario
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
        if (scenario_id==-1){ // normal call on a full instance
            return scenarios[s];
        }
        else if (s==0){ //instance is already single, so we can only extract scen 0. Note that scenario ID might be different from 0
            return *this;
        }
        else {
            throw std::runtime_error("trying to extract sub scenario but only one is available.");
        }
    }

    size_t hash() const {// dirty hash for quick lookup (doesn't consider the data that's supposedly equal)
        std::size_t seed = 0; 
            
        seed += std::hash<int>()(S); 
        for (size_t i; i<releaseDates.size(); i++){
            std::vector<int> vec = releaseDates[i];
            for (int data : vec){
                std::size_t datahash= std::hash<int>()(data); 
                seed ^= datahash + 0x9e3779b9 + (i << 6) + (i >> 2);
            }
        }
        return seed;
    }

    bool operator==(const DataInstance& other) const {// == operator to compare when hash collides. (comparing data that is always equal in the end for lazy eval, but could remove entirely)
        return (N==other.N) && 
               (S==other.S) &&
               (releaseDates==other.releaseDates) &&
               (durations==other.durations) &&
               (dueDates==other.dueDates) &&
               (precedenceConstraints==other.precedenceConstraints) ;

    } 
};

#endif // DATAINSTANCE_H