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

enum class InstanceType { SINGLE_MACHINE, RCPSP };

// 1. THE INTERFACE (Pure Virtual)
// No data members here. Only definitions of what an instance must provide.
class DataInstance {
public:
    virtual ~DataInstance() {}
    InstanceType type;
    virtual std::string get_file_name() const = 0; //file from which the data comes, helps for debug
    virtual int getS() const = 0; //number of scenarios
    virtual int getN() const = 0; //number of jobs (used as an instance size indicator mostly)
    virtual bool get_prec(int task1, int task2) const = 0; 
    // Splitting function for scenarios
    virtual void extractScenarios(const DataInstance* original, const std::vector<int>& indices) = 0;
    virtual DataInstance* clone() const = 0;
    virtual void print() const = 0;
    virtual void print_summary() const = 0;

    //split logic common to all instances
    std::pair<DataInstance*, DataInstance*> SampleSplitScenarios(int k, std::mt19937 &rng) const {
        if (k >= this->getS()) throw std::runtime_error("k >= S");

        // 1. Logic: Shuffling indices
        std::vector<int> indices(getS());
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), rng);

        // 2. Creation: Create copies of the correct concrete type
        DataInstance* train = this->clone();
        DataInstance* test = this->clone();

        // 3. Partition: Split indices into two sets
        std::vector<int> train_indices(indices.begin(), indices.begin() + k);
        std::vector<int> test_indices(indices.begin() + k, indices.end());

        // 4. Delegation: Let the subclass handle the actual data plucking
        train->extractScenarios(this, train_indices);
        test->extractScenarios(this, test_indices);

        return {train, test};
    }

};


// 2. THE CONCRETE SINGLE MACHINE CLASS
class SingleMachineInstance : public DataInstance {
public:
    std::string file_name;
    int N; //number of tasks
    int S; // number of scenarios in data
    std::vector<uint8_t> precedenceConstraints;
    std::vector<int> durations;
    std::vector<std::vector<int>> releaseDates;
    std::vector<int> dueDates; //unused, but read from file for completeness (artefact from older project versions, could be useful for lateness-based objectives)

    inline bool get_prec(int task1, int task2) const override{
        return precedenceConstraints[task1 * N + task2];
    }


    SingleMachineInstance() {this->type = InstanceType::SINGLE_MACHINE;}

    SingleMachineInstance(const std::string& filename) { //constructor : reads from data file
        this->type = InstanceType::SINGLE_MACHINE;
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

    // Implementing the Interface
    int getS() const override { return S; }
    int getN() const override { return N; }
    std::string get_file_name() const override { return file_name; }
    void extractScenarios(const DataInstance* original, const std::vector<int>& indices) override {
        const SingleMachineInstance* orig = dynamic_cast<const SingleMachineInstance*>(original); //cast to correct type
        if (!orig) {
            throw std::runtime_error("Invalid instance type for scenario extraction");
        }

        S = indices.size();
        N = orig->N;
        file_name = orig->file_name + "_virtual_split";

        precedenceConstraints = orig->precedenceConstraints; //same for all scenarios
        durations = orig->durations; //same for all scenarios
        dueDates = orig->dueDates; //same for all scenarios

        releaseDates.resize(S, std::vector<int>(N));
        for (size_t i = 0; i < indices.size(); ++i) {
            releaseDates[i] = orig->releaseDates[indices[i]];
        }
    }
 
    DataInstance* clone() const override {
        return new SingleMachineInstance(*this);
    }

    void print() const override {
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

    void print_summary() const override{
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

};


class RCPSPInstance : public SingleMachineInstance {
public:
    int num_resources;
    std::vector<int> capacities;
    // usages[task_index][resource_index]
    std::vector<std::vector<int>> usages; 

    RCPSPInstance(const std::string& filename) {
        this->type = InstanceType::RCPSP;
        this->file_name = filename;
        this->S = 1;

        std::ifstream file(filename);
        if (!file.is_open()) throw std::runtime_error("Could not open file: " + filename);

        std::string line;
        while (std::getline(file, line)) {
            // Robust Job Count Parsing
            if (line.find("jobs (incl. supersource/sink )") != std::string::npos) {
                size_t pos = line.find_last_of(':');
                if (pos != std::string::npos) {
                    N = std::stoi(line.substr(pos + 1));
                }
                precedenceConstraints.assign(N * N, 0);
                durations.resize(N);
                usages.resize(N);
            }
            // Robust Resource Count Parsing
            else if (line.find("- renewable") != std::string::npos) {
                std::stringstream ss(line);
                std::string temp;
                while (ss >> temp && temp != ":"); // Skip until colon
                ss >> num_resources;
                capacities.resize(num_resources);
            }
            else if (line.find("PRECEDENCE RELATIONS:") != std::string::npos) {
                std::getline(file, line); // Skip header row
                for (int i = 0; i < N; ++i) {
                    if (!std::getline(file, line)) break;
                    std::stringstream ss(line);
                    int jobIdx, mode, numSuccessors;
                    if (!(ss >> jobIdx >> mode >> numSuccessors)) continue;
                    for (int j = 0; j < numSuccessors; ++j) {
                        int successor;
                        ss >> successor;
                        precedenceConstraints[(jobIdx - 1) * N + (successor - 1)] = 1;
                    }
                }
            }
            else if (line.find("REQUESTS/DURATIONS:") != std::string::npos) {
                std::getline(file, line); // Skip header
                std::getline(file, line); // Skip separator (-------)
                for (int i = 0; i < N; ++i) {
                    if (!std::getline(file, line)) break;
                    std::stringstream ss(line);
                    int jobIdx, mode;
                    ss >> jobIdx >> mode >> durations[i];
                    usages[i].resize(num_resources);
                    for (int r = 0; r < num_resources; ++r) {
                        ss >> usages[i][r];
                    }
                }
            }
            else if (line.find("RESOURCEAVAILABILITIES:") != std::string::npos) {
                std::getline(file, line); // Skip header (R1 R2...)
                std::getline(file, line); // Read the capacity values
                std::stringstream ss(line);
                for (int r = 0; r < num_resources; ++r) {
                    ss >> capacities[r];
                }
            }
        }
        releaseDates.assign(S, std::vector<int>(N, 0));
        dueDates.assign(N, 0);
        file.close();
    }   

    DataInstance* clone() const override {
        return new RCPSPInstance(*this);
    }

    void print_summary() const override {
        SingleMachineInstance::print_summary();
        std::cout << "Resources: " << num_resources << std::endl;
        std::cout << "Capacities: ";
        for (int c : capacities) std::cout << c << " ";
        std::cout << std::endl;
    }
};

#endif // DATAINSTANCE_H