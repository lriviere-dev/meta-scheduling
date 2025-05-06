#ifndef SPT_POLICY_H
#define SPT_POLICY_H


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


//The Shortest Processing Time policy : schedules the task that has the shortest processing time among ready tasks at each decision points.
class SPTPolicy : public Policy {
public:
    ~SPTPolicy() = default;

    // How to get a sequence from the meta solutions
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
            const auto& durations = instance.durations;
            const auto& prec = instance.precedenceConstraints;
            std::set<int, std::less<int>> prec_free_nodes; // sorted set of prec_available nodes; 
            std::set<int, std::less<int>> free_nodes; // sorted set of available (both release and precednece wise) nodes (default comparison by index)  std::vector<int> incoming_edges_nb; 
            int time  = 0; //tracks time to see if tasks are ready
            std::vector<int> incoming_edges_nb; 
            std::vector<std::vector<int>> outgoing_edges; 


            // Iterate over each group of tasks
            for (auto& group : groupMeta->get_task_groups_modifiable()) { 
                // First, sort the tasks by their durations (or lex order if tie)
                std::sort(group.begin(), group.end(), [&durations](int t1, int t2) {
                    //check release dates
                    if (durations[t1] != durations[t2]) {
                        return durations[t1] < durations[t2];
                    }
                    return t1 < t2; // Lexicographical order as tie-breaker
                }); //group shouldn't be further modified, as we will use it as a key!

                //precompute a graph-like node structure for toposort (ensures precedence constraints satisfactions)
                incoming_edges_nb.resize(group.size(), 0);
                for (auto& vec : outgoing_edges) vec.clear(); // Clears contents without deallocating
                outgoing_edges.resize(group.size()); // Ensures correct size without reallocating inner vectors                
                for (size_t i=0; i<group.size();  i++) { //!!! We use index "0" for example to refer to the 0th task in group vector (it also indicates it has the smallest duration)!
                    //for each tasks list nodes with an incoming edge
                    for (size_t j =0; j<group.size(); j++){
                        if (prec[group[j] * instance.N + group[i]]){ //if the task at index j should be before task at index i,
                            incoming_edges_nb[i]++;
                        }
                        if (prec[group[i] * instance.N + group[j]]){ 
                            outgoing_edges[i].push_back(j); 
                        }                        
                    }
                    if (incoming_edges_nb[i]==0){prec_free_nodes.insert(i);}//remember tasks without incoming edge (prec-wise ready) //keeps sorted order from group
                }
                //find first decision moment : min time when a prec_free task is realeased
                int prec_free_nodes_min_release = releaseDates[group[*prec_free_nodes.begin()]]; // Initial case using the first element
                for (auto node : prec_free_nodes){
                    prec_free_nodes_min_release = std::min(prec_free_nodes_min_release, releaseDates[group[node]]);
                }
                time = std::max(time, prec_free_nodes_min_release); //jumping to next decision moment if necessary
                //init done, now looping till group fully treated
                while (!free_nodes.empty() || !prec_free_nodes.empty()){
                    //update free_nodes at that time
                    std::vector<int> to_erase;
                    for (int node : prec_free_nodes) {
                        if (releaseDates[group[node]] <= time) {
                            free_nodes.insert(node);
                            to_erase.push_back(node);
                        }
                    }
                    for (int node : to_erase) {//erasing separately to not fuck with the iterations
                        prec_free_nodes.erase(node);
                    }
                    //find task to schedule and schedule it
                    int selected_task = *free_nodes.begin();  // Get the first (smallest) element (there must be one)
                    free_nodes.erase(selected_task);    // Remove it from the set
                    sequence[c++] = group[selected_task];    // Post-increment
                    for (auto& node : outgoing_edges[selected_task]) {  // Remove edges with this task
                        incoming_edges_nb[node]--;
                        if (incoming_edges_nb[node] == 0) {
                            prec_free_nodes.insert(node);  // Insert node into the set, which keeps it sorted by index
                        }
                    }
                    time+=durations[group[selected_task]]; //update time
                    //find new decision time (skip time if no task ready yet). also check it's not the end yet.
                    if (free_nodes.empty() && !prec_free_nodes.empty()){
                        prec_free_nodes_min_release = releaseDates[group[*prec_free_nodes.begin()]]; // Initial case using the first element
                        for (auto node : prec_free_nodes){
                            prec_free_nodes_min_release = std::min(prec_free_nodes_min_release, releaseDates[group[node]]);
                        }
                        time = std::max(time, prec_free_nodes_min_release); //jumping to next decision moment if necessary
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

    //compares two sequences to find the preffered one by SPT in a given scenario 
    bool isLexicographicallySmaller(const Sequence& seq1, const Sequence& seq2, const DataInstance& instance, int scenario_id) const {
        const auto& releaseDates = instance.releaseDates[scenario_id]; 
        const auto& durations = instance.durations; 
        const auto& tasks1 = seq1.get_tasks(); // Cache tasks of seq1
        const auto& tasks2 = seq2.get_tasks(); // Cache tasks of seq2
        size_t size = tasks1.size(); // Assuming both sequences have the same size
        int time = 0; // tracks time 

        // Compare task order in sequences (to find dissimilarity)
        for (size_t i = 0; i < size; ++i) { 
            int taskA = tasks1[i];
            int taskB = tasks2[i];

            //check if a task is ready before the other and it matters (not both ready at the current time)
            if ((releaseDates[taskA] != releaseDates[taskB]) && ((releaseDates[taskB]>time) || (releaseDates[taskA]>time))){
                // Earlier release date wins
                return releaseDates[taskA] < releaseDates[taskB];
            }
            //They have the same effective release date, so if they have different durations, we prefer smalller tasks
            if (durations[taskA] != durations[taskB]) {
                // smaller durations wins
                return durations[taskA] < durations[taskB];
            }

            // If effective release dates are equal, and durations are equal, compare task indices lexicographically if they're different
            if (taskA != taskB) {
                return taskA < taskB;
            }
            time = std::max(time, releaseDates[taskA]) + durations[taskA]; //same task, tracking time
        }

        // At this point, sequences should be equal. So not strictly smaller
        return false;
    }
    };

#endif // SPT_POLICY_H
