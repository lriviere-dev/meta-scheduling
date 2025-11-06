#ifndef FIFO_POLICY_H
#define FIFO_POLICY_H


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


//We will mostly use FIFOpolicy, but we could imagine other policies.
class FIFOPolicy : public Policy {
public:
    ~FIFOPolicy() = default;

    std::string name = "fifo_policy";

    // Override extract_sequence for GroupMetaSolution
    Sequence extract_sequence(MetaSolution& metaSolution, const DataInstance& instance, int scenario_id) const override{
        if(metaSolution.scored_by){throw std::runtime_error("Extracting sequence but metasol is already scored.");} //if already scored, why don't we just get the stored result?

        Sequence output; 
        bool set_output = false;
        // Try to cast to GroupMetaSolution
        if (auto* groupMeta = dynamic_cast<GroupMetaSolution*>(&metaSolution)) {
            // Handle GroupMetaSolution

            std::vector<int> sequence(instance.N);
            int c = 0; // counter for index
            const auto& releaseDates = instance.releaseDates[scenario_id];
            const auto& prec = instance.precedenceConstraints;
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

    //compares two sequences to find the preffered one by FIFO in a given scenario 
    bool isLexicographicallySmaller(const Sequence& seq1, const Sequence& seq2, const DataInstance& instance, int scenario_id) const {
        const auto& releaseDates = instance.releaseDates[scenario_id]; 
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

    };

#endif // FIFO_POLICY_H
