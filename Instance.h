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
#include <regex>
#include <string>

enum class InstanceType { SINGLE_MACHINE, RCPSP };

// 1. THE INTERFACE (Pure Virtual)
// Definitions of what an instance must provide.
// Data common to all Sequence representing instances : S, precedence constraints, 
class DataInstance {
public:
    std::string file_name;
    int N; //number of tasks
    int S; // number of scenarios in data
    std::vector<uint8_t> precedenceConstraints;

    virtual ~DataInstance() {}
    InstanceType type;
    virtual std::string get_file_name() const { return file_name; }; //file from which the data comes, helps for debug
    virtual int getS() const { return S; }; //number of scenarios
    virtual int getN() const { return N; }; //number of jobs (used as an instance size indicator mostly)
    virtual bool get_prec(int task1, int task2) const { return precedenceConstraints[task1 * N + task2]; } 
    // Splitting function for scenarios
    virtual void extractScenarios(const DataInstance* original, const std::vector<int>& indices) = 0;
    virtual DataInstance* clone() const = 0;
virtual void print_precedences() const {
        std::cout << "\n--- PRECEDENCE RELATIONSHIPS ---" << std::endl;
        bool found = false;
        for (int i = 0; i < N; ++i) {
            bool first = true;
            for (int j = 0; j < N; ++j) {
                if (precedenceConstraints[i * N + j] == 1) {
                    if (first) { std::cout << "Job " << i << " -> ["; first = false; }
                    else { std::cout << ", "; }
                    std::cout << j;
                    found = true;
                }
            }
            if (!first) std::cout << "]" << std::endl;
        }
        if (!found) std::cout << "(None)" << std::endl;
    }

    virtual void print_data() const = 0;

    // Comprehensive print
    virtual void print() const {
        print_summary();
        print_precedences();
        print_data();
        std::cout << "========================\n" << std::endl;
    }

    virtual void print_summary() const {    
        std::cout << "File name:" << file_name << std::endl;
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
    };

    int getClusterCount() const {
        // Regex looks for "cluster" followed by one or more digits (\d+)
        std::regex cluster_regex("_cluster(\\d+)");
        std::smatch match;

        if (std::regex_search(file_name, match, cluster_regex)) {
            return std::stoi(match[1].str());
        }
        std::cout << "Warning: No cluster information found in file name. Defaulting to 0 clusters (random sampling).\n";
        // Default fallback if "cluster" is not found in the name
        return 0; 
    }

    //split logic common to all instances
    std::pair<DataInstance*, DataInstance*> SampleSplitScenarios(int k, std::mt19937 &rng, bool use_clusters = false) const {
        if (k >= this->getS()) throw std::runtime_error("k >= S : cannot split scenarios, not enough scenarios for training");

        std::vector<int> train_indices;
        std::vector<int> test_indices;
        
        if (use_clusters) { 
            // 1a. Identify cluster centers (the first C scenarios)
            int C = this->getClusterCount(); //default return is 0 if no cluster info in name, so it falls back to pure random sampling if not specified
            int centers_to_take = std::min(k, C);

            // Take the centers first
            for (int i = 0; i < centers_to_take; ++i) {
                train_indices.push_back(i);
            }

            // 1b. Shuffle the remaining scenarios (those not used as centers)
            std::vector<int> remaining_indices;
            for (int i = centers_to_take; i < S; ++i) {
                remaining_indices.push_back(i);
            }
            std::shuffle(remaining_indices.begin(), remaining_indices.end(), rng);

            // 1c. Fill the rest of the training set 'k' and the test set
            int needed_for_train = k - centers_to_take;
            train_indices.insert(train_indices.end(), remaining_indices.begin(), remaining_indices.begin() + needed_for_train);
            test_indices.insert(test_indices.end(), remaining_indices.begin() + needed_for_train, remaining_indices.end());
            
        } else {
            // Original logic: Pure random shuffle
            std::vector<int> indices(S);
            std::iota(indices.begin(), indices.end(), 0);
            std::shuffle(indices.begin(), indices.end(), rng);
            
            train_indices.assign(indices.begin(), indices.begin() + k);
            test_indices.assign(indices.begin() + k, indices.end());
        }

        // 2. Creation: Create copies
        DataInstance* train = this->clone();
        DataInstance* test = this->clone();
        
        // //print training scenarios IDs (could put a debug arg for it)
        // std::cout << "Training scenarios IDs: ";
        // for (size_t i = 0; i < train_indices.size(); ++i) {
        //     std::cout << train_indices[i];
        //     if (i != train_indices.size() - 1) {
        //         std::cout << ", ";
        //     }
        // }
        // std::cout << std::endl;

        // 4. Delegation: Let the subclass handle the actual data plucking
        train->extractScenarios(this, train_indices);
        test->extractScenarios(this, test_indices);

        return {train, test};
    }

};


// 2. THE CONCRETE SINGLE MACHINE CLASS
class SingleMachineInstance : public DataInstance {
public:
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

 
    void print_data() const override {
        std::cout << "\n--- TASK DATA (Single Machine) ---" << std::endl;
        
        std::cout << "Durations: ";
        for (int i = 0; i < N; ++i) {
            std::cout << i << ":" << durations[i] << (i == N - 1 ? "" : ", ");
        }
        
        std::cout << "\nDue Dates: ";
        for (int i = 0; i < N; ++i) {
            std::cout << i << ":" << dueDates[i] << (i == N - 1 ? "" : ", ");
        }

        std::cout << "\n\n--- SCENARIO RELEASE DATES ---" << std::endl;
        for (int s = 0; s < S; ++s) {
            std::cout << "Scenario " << s << ": ";
            for (int i = 0; i < N; ++i) {
                std::cout << i << ":" << releaseDates[s][i] << (i == N - 1 ? "" : ", ");
            }
            std::cout << std::endl;
        }
    }
   // Implementing the Interface and the default methods
    void extractScenarios(const DataInstance* original, const std::vector<int>& indices) override {//use input instance to extract scenarios at given indices
        const SingleMachineInstance* orig = dynamic_cast<const SingleMachineInstance*>(original); //cast to correct the input instance
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

};


class RCPSPInstance : public DataInstance {
public:
    std::vector<int> durations;
    std::vector<std::vector<int>> releaseDates;
    std::vector<int> dueDates; //unused, but read from file for completeness (artefact from older project versions, could be useful for lateness-based objectives)
    int num_resources;
    std::vector<int> capacities;
    std::vector<std::vector<int>> usages; 


    RCPSPInstance(const std::string& filename) {
        this->type = InstanceType::RCPSP;
        this->file_name = filename;
        this->S = 1; // Default to 1 scenario
        this->N = 0;

        std::ifstream file(filename);
        if (!file.is_open()) throw std::runtime_error("Could not open file: " + filename);

        std::string line;
        while (std::getline(file, line)) {
            // 1. Robust Job Count Parsing
            if (line.find("jobs (incl. supersource/sink )") != std::string::npos) {
                size_t pos = line.find_last_of(':');
                if (pos != std::string::npos) {
                    N = std::stoi(line.substr(pos + 1));
                }
                precedenceConstraints.assign(N * N, 0);
                durations.resize(N);
                usages.resize(N);
                // Initialize default release dates (1 scenario of zeros)
                releaseDates.assign(1, std::vector<int>(N, 0));
            }
            // 2. Robust Resource Count Parsing
            else if (line.find("- renewable") != std::string::npos) {
                std::stringstream ss(line);
                std::string temp;
                while (ss >> temp && temp != ":"); 
                ss >> num_resources;
                capacities.resize(num_resources);
            }
            // 3. Precedence Relations
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
            // 4. Requests/Durations
            else if (line.find("REQUESTS/DURATIONS:") != std::string::npos) {
                std::getline(file, line); // Skip header
                std::getline(file, line); // Skip separator
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
            // 5. Resource Availabilities
            else if (line.find("RESOURCEAVAILABILITIES:") != std::string::npos) {
                std::getline(file, line); 
                std::getline(file, line); 
                std::stringstream ss(line);
                for (int r = 0; r < num_resources; ++r) {
                    ss >> capacities[r];
                }
            }
            // 6. NEW: Uncertainty Data Section
            else if (line.find("UNCERTAINTY DATA:") != std::string::npos) {
                while (std::getline(file, line)) {
                    if (line.find("scenarios") != std::string::npos) {
                        size_t pos = line.find(':');
                        this->S = std::stoi(line.substr(pos + 1));
                        releaseDates.assign(S, std::vector<int>(N, 0));
                    }
                    else if (line.find("RELEASE DATES SCENARIOS:") != std::string::npos) {
                        for (int s = 0; s < S; ++s) {
                            for (int j = 0; j < N; ++j) {
                                if (!(file >> releaseDates[s][j])) break;
                            }
                        }
                        break; 
                    }
                }
            }
        }
        
        dueDates.assign(N, 0); //no due dates in RCPSP instances, but keep just in case
        file.close();
    }

    void print_data() const override {
        std::cout << "\n--- TASK DATA (RCPSP) ---" << std::endl;
        
        std::cout << "Durations: ";
        for (int i = 0; i < N; ++i) {
            std::cout << i << ":" << durations[i] << (i == N - 1 ? "" : ", ");
        }

        std::cout << "\nResource Usages (Task: [r1, r2, ...]):" << std::endl;
        for (int i = 0; i < N; ++i) {
            std::cout << "  " << i << ": [";
            for (int r = 0; r < num_resources; ++r) {
                std::cout << usages[i][r] << (r == num_resources - 1 ? "" : ", ");
            }
            std::cout << "]" << std::endl;
        }

        std::cout << "\n--- SCENARIO RELEASE DATES ---" << std::endl;
        for (int s = 0; s < S; ++s) {
            std::cout << "Scenario " << s << ": ";
            for (int i = 0; i < N; ++i) {
                std::cout << i << ":" << releaseDates[s][i] << (i == N - 1 ? "" : ", ");
            }
            std::cout << std::endl;
        }
    }

    void extractScenarios(const DataInstance* original, const std::vector<int>& indices) override {//use input instance to extract scenarios at given indices
        const RCPSPInstance* orig = dynamic_cast<const RCPSPInstance*>(original); //cast to correct the input instance
        if (!orig) {
            throw std::runtime_error("Invalid instance type for scenario extraction");
        }

        S = indices.size();
        N = orig->N;
        file_name = orig->file_name + "_virtual_split";
        num_resources = orig->num_resources;
        precedenceConstraints = orig->precedenceConstraints; //same for all scenarios
        durations = orig->durations; //same for all scenarios
        dueDates = orig->dueDates; //same for all scenarios
        capacities = orig->capacities;
        usages = orig->usages;
        releaseDates.resize(S, std::vector<int>(N));
        for (size_t i = 0; i < indices.size(); ++i) {
            releaseDates[i] = orig->releaseDates[indices[i]];
        }
    }
 

    DataInstance* clone() const override {
        return new RCPSPInstance(*this);
    }

    void print_summary() const override {
        DataInstance::print_summary();
        std::cout << "Resources: " << num_resources << std::endl;
        std::cout << "Capacities: ";
        for (int c : capacities) std::cout << c << " ";
        std::cout << std::endl;
    }
};

#endif // DATAINSTANCE_H