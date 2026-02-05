#ifndef POLICY_RCPSP_H
#define POLICY_RCPSP_H


#include "MetaSolutions.h"
#include "Policy.h"
#include "ListMetaSolutions.h"
#include "Sequence.h"
#include "Schedule.h"
#include "Instance.h"
#include <vector>
#include <set>
#include <deque>
#include <ilcp/cp.h>
#include <tuple>
#include <functional>

//TODO : implement RCPSP-specific policies here. (Use SGS to override transform_to_schedule, and implement extract_sequence using resource-based heuristics.)
// Currently, we just use an ill-fitting fifo policy for RCPSP as a placeholder.

class RCPSPPolicy : public Policy {
public:
    ~RCPSPPolicy() = default;

    std::string name = "rcpsp_policy";

    Schedule transform_to_schedule(const Sequence& sequence, const DataInstance& instance, int scenario_id) const override {
        const RCPSPInstance& rcpsp_instance = static_cast<const RCPSPInstance&>(instance);
        const std::vector<int>& tasks = sequence.get_tasks();
        const size_t numTasks = tasks.size();
        const size_t numRes = rcpsp_instance.capacities.size();

        // 1. Setup Horizon and 1D Resource Profile
        int horizon = 0;
        int max_r = 0;
        for (int d : rcpsp_instance.durations) horizon += d;
        for (int r : rcpsp_instance.releaseDates[scenario_id]) max_r = std::max(max_r, r);
        horizon += max_r;
        
        // Contiguous memory: Resource 0 [0...H], Resource 1 [0...H], etc.
        std::vector<int> resourceUsage(numRes * (horizon + 1), 0);
        std::vector<int> startTimes(numTasks);
        std::vector<int> finishTimes(numTasks);

        int lastTaskStart = 0;

        for (size_t i = 0; i < numTasks; ++i) {
            int task = tasks[i];
            int duration = rcpsp_instance.durations[task];

            // 2. Initial Earliest Start Time (EST)
            // Respects sequence order (start_i >= start_{i-1}) and release date
            int t = std::max(rcpsp_instance.releaseDates[scenario_id][task], lastTaskStart);

            // 3. Precedence: Must finish after ALL predecessors
            // We check every task previously scheduled in the sequence
            for (size_t j = 0; j < i; ++j) {
                int prevTask = tasks[j];
                if (rcpsp_instance.get_prec(prevTask, task)) {//if prevTask must precede task
                    if (finishTimes[prevTask] > t) {//and it finishes after current t
                        t = finishTimes[prevTask]; //push t forward
                    }
                }
            }

            // 4. Resource Feasibility (The Leapfrog Search) : push time forward until no resource conflict
            bool conflict = true;
            while (conflict) {
                conflict = false;
                for (size_t r = 0; r < numRes; ++r) { //for every resource
                    int demand = rcpsp_instance.usages[task][r];
                    if (demand <= 0) continue;

                    int cap = rcpsp_instance.capacities[r];
                    int offset = r * (horizon + 1); //offset in 1D profile for quicker accesss

                    for (int time = t; time < t + duration; ++time) {
                        if (resourceUsage[offset + time] + demand > cap) {
                            // Conflict found: Jump t forward and restart resource check
                            t = time + 1;
                            conflict = true;
                            break; // Breaks out of time loop
                        }
                    }
                    if (conflict) break; //break out of ressource loop : restart from first ressource at new t
                }
            }

            // 5. Commit Timing
            startTimes[task] = t;
            finishTimes[task] = t + duration;
            lastTaskStart = t; // Anchors the next task in the sequence

            // 6. Update Resource Profile
            for (size_t r = 0; r < numRes; ++r) {
                int demand = rcpsp_instance.usages[task][r];
                if (demand <= 0) continue;
                int offset = r * (horizon + 1);
                for (int time = t; time < t + duration; ++time) {
                    resourceUsage[offset + time] += demand;
                }
            }
        }
        return Schedule(startTimes);
    }

    // Override extract_sequence for GroupMetaSolution
    Sequence extract_sequence(MetaSolution& metaSolution, const DataInstance& instance, int scenario_id) const override{
        if(metaSolution.scored_by){throw std::runtime_error("Extracting sequence but metasol is already scored.");} //if already scored, why don't we just get the stored result?

        if (! (instance.type == InstanceType::RCPSP)) {
            throw std::runtime_error("RCPSPPolicy does not support SINGLE MACHINE instances.");
        }
        const RCPSPInstance& rcpsp_instance = static_cast<const RCPSPInstance&>(instance); //ensure correct type

        Sequence output; 
        bool set_output = false;
        // Try to cast to GroupMetaSolution
        if (auto* groupMeta = dynamic_cast<GroupMetaSolution*>(&metaSolution)) {
            // Handle GroupMetaSolution

            std::vector<int> sequence(rcpsp_instance.N);
            int c = 0; // counter for index
            const auto& releaseDates = rcpsp_instance.releaseDates[scenario_id];
            const auto& prec = rcpsp_instance.precedenceConstraints;
            std::set<int, std::less<int>> free_nodes; // sorted set of available nodes (default comparison by index)  std::vector<int> incoming_edges_nb; 
            std::vector<int> incoming_edges_nb; 
            std::vector<std::vector<int>> outgoing_edges; 


            // Iterate over each group of tasks
            for (auto& group : groupMeta->get_task_groups_modifiable()) { 
                // First, sort the tasks by their release date (or lex order if tie)
                std::sort(group.begin(), group.end(), [&releaseDates](int t1, int t2) {
                    //check release dates
                    if (releaseDates[t1] != releaseDates[t2]) {
                        return releaseDates[t1] < releaseDates[t2];
                    }
                    return t1 < t2; // Lexicographical order as tie-breaker
                }); //group shouldn't be further modified, as we will use it as a key

                //precompute a graph-like node structure for toposort
                incoming_edges_nb.resize(group.size(), 0);
                for (auto& vec : outgoing_edges) vec.clear(); // Clears contents without deallocating
                outgoing_edges.resize(group.size()); // Ensures correct size without reallocating inner vectors                
                for (size_t i=0; i<group.size();  i++) { //!!! We use index "0" for example to refer to the 0th task in group vector!
                    //for each tasks list nodes with an incoming edge
                    for (size_t j =0; j<group.size(); j++){
                        if (prec[group[j] * instance.N + group[i]]){ //if the task at index j should be before task at index i,
                            incoming_edges_nb[i]++;
                        }
                        if (prec[group[i] * instance.N + group[j]]){ 
                            outgoing_edges[i].push_back(j); //keeps sorted order from group
                        }                        
                    }
                    if (incoming_edges_nb[i]==0){free_nodes.insert(i);}
                }

                //toposort, but free nodes are selected in the sorted order.
                while (!free_nodes.empty()){
                    int selected_task = *free_nodes.begin();  // Get the first (smallest) element
                    free_nodes.erase(free_nodes.begin());    // Remove it from the set
                    sequence[c++] = group[selected_task];    // Post-increment
                    for (auto& node : outgoing_edges[selected_task]) {  // Remove edges with this task
                        incoming_edges_nb[node]--;
                        if (incoming_edges_nb[node] == 0) {
                            free_nodes.insert(node);  // Insert node into the set, which keeps it sorted by index
                        }
                    }
                }
            }

            output = Sequence(std::move(sequence));
            /*if (!output.check_precedence_constraints(instance)){
                throw std::runtime_error("bug detected");
            }*/
            set_output = true;
        }
        else if (auto* SeqMeta = dynamic_cast< SequenceMetaSolution*>(&metaSolution)) {
            output = Sequence(SeqMeta->get_sequence().get_tasks());
            set_output = true;
        }
        // Handling all ListMetaSolution types via their underlying metasolution type (recursive)
        //we assume the underlying metasolutions have already been scored appropriately ( we make sure of that in evaluate_meta)
        else if (auto* listMeta = dynamic_cast< ListMetaSolutionBase*>(&metaSolution)) {
            const auto& metaSolutions = listMeta->get_meta_solutions();
            Sequence minSeq = metaSolutions[0]->front_sequences[scenario_id]; // Initialize with the first sequence/metasol
            listMeta->front_indexes[scenario_id] = 0;

            for (size_t i = 1; i < metaSolutions.size(); ++i) {  // Iterate from second element
                auto& seq = metaSolutions[i]->front_sequences[scenario_id];
                if (isLexicographicallySmaller(seq, minSeq, instance, scenario_id)) {
                    minSeq = seq;
                    listMeta->front_indexes[scenario_id] = i;//update index of metasol used in that scenario
                }
            }
            output = std::move(minSeq); 
            set_output = true;
        }
        // Add other MetaSolution type checks here if necessary
        if (!set_output){
            throw std::invalid_argument("Unsupported MetaSolution type in FIFOPolicy::extract_sequence.");
        }
        return output;
    }
    
    int extract_sub_metasolution_index(const MetaSolution& metaSolution, const DataInstance& instance, int scenario_id) const override{
        //assert list solution
        const ListMetaSolutionBase* listMetaSolution = dynamic_cast<const ListMetaSolutionBase*>(&metaSolution);
        if (!listMetaSolution) {
            throw std::runtime_error("MetaSolution must be of type ListMetaSolutionBase.");
        }
        //assert metasol was scored
        if (!metaSolution.scored_by) {
            throw std::runtime_error("metasol should already be scored by a policy");
        }    
        return listMetaSolution->front_indexes[scenario_id]; // Return the index of the metasolution in the list that outputs the min sequence  
        (void)instance; //legacy, in theory requires instanc 
    }

    //compares two sequences to find the preffered one by the policy in a given scenario 
    bool isLexicographicallySmaller(const Sequence& seq1, const Sequence& seq2, const DataInstance& instance, int scenario_id) const {
        if (! (instance.type == InstanceType::RCPSP)) {
            throw std::runtime_error("RCPSPPolicy does not support SINGLE MACHINE instances.");
        }
        const RCPSPInstance& rcpsp_instance = static_cast<const RCPSPInstance&>(instance); //ensure correct type

        const auto& releaseDates = rcpsp_instance.releaseDates[scenario_id]; 
        const auto& tasks1 = seq1.get_tasks(); // Cache tasks of seq1
        const auto& tasks2 = seq2.get_tasks(); // Cache tasks of seq2
        size_t size = tasks1.size(); // Assuming both sequences have the same size

        // Compare task order in sequences
        for (size_t i = 0; i < size; ++i) { 
            int taskA = tasks1[i];
            int taskB = tasks2[i];
            if (releaseDates[taskA] != releaseDates[taskB]) {
                // Earlier release date wins
                return releaseDates[taskA] < releaseDates[taskB];
            }

            // If release dates are equal, compare task indices lexicographically
            if (taskA != taskB) {
                return taskA < taskB;
            }
        }

        // At this point, sequences should be equal. So not strictly smaller
        return false;
    }

    virtual void define_objective(IloEnv env, IloModel& model, 
                    IloIntervalVarArray2& jobs, const DataInstance& instance, 
                    IloIntExprArray& scenario_scores,  IloIntVar& aggregated_objective) const {
        RCPSPInstance const& rcpsp_instance = static_cast<const RCPSPInstance&>(instance);
        //objective here is max sumci. Could be other.
        int nbScenarios = rcpsp_instance.S;
        int nbJobs = rcpsp_instance.N;
        IloIntExprArray completions(env, nbJobs);
        for (int s = 0; s < nbScenarios; s++) {
            for (int i = 0; i < nbJobs; i++) {
                completions[i] = IloEndOf(jobs[s][i]);
            }
            scenario_scores[s] = IloSum(completions);
        }

        // Aggregate objectives across scenarios
        for (int s = 0; s < nbScenarios; s++)
            model.add(aggregated_objective >= scenario_scores[s]); //max score among scenarios
        model.add(IloMinimize(env, aggregated_objective));
    }

    };

#endif // POLICY_RCPSP_H
