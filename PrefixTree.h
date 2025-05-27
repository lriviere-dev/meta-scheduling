#ifndef PREFIX_TREE_H
#define PREFIX_TREE_H

#include <iostream>
#include <vector>
#include <list>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <algorithm> // For std::max
#include <tuple>
#include "MetaSolutions.h"



// Forward declaration
class PrefixTreeNode;


// Define a custom hash function for std::vector<int> to use with std::unordered_set
namespace std {
    template <>
    struct hash<std::vector<int>> {
        size_t operator()(const std::vector<int>& lst) const {
            size_t h = 0;
            for (int val : lst) {
                    h ^= std::hash<int>{}(val) + 0x9e3779b9;; //commutative operation
            }
            return h;
        }
    };
}

class PrefixTreeNode {
public:
    using ChildMap = std::unordered_map<std::vector<int>, std::unique_ptr<PrefixTreeNode>>;
    std::vector<std::vector<int>> sequences; // partial sequence for each scenario up to this node
    std::vector<int> times; // Times for each scenario ending at this node
    ChildMap children;

    PrefixTreeNode() = default;
};

class PrefixTree {
private:
    std::unique_ptr<PrefixTreeNode> root_;

public:
    PrefixTree() : root_(std::make_unique<PrefixTreeNode>()) {}

    void add(const std::vector<std::vector<int>>& partial_groups,
             const std::vector<std::vector<int>>& partial_sequences,
             const std::vector<std::vector<int>>& times,
             PrefixTreeNode* starting_point = nullptr) {

        PrefixTreeNode* current_node;
        // This vector will store the growing partial sequence for each scenario as we traverse.
        // It will be assigned to `current_node->sequence` at each step.
        std::vector<std::vector<int>> current_path_sequences;
        size_t num_scenarios = 0;
        int sequence_index = 0;

        // Determine the starting node and initialize current_path_sequences
        if (starting_point != nullptr) {
            current_node = starting_point;
            current_path_sequences = current_node->sequences; // Start with the sequence of the starting node
            num_scenarios = current_path_sequences.size();
            sequence_index = current_path_sequences[0].size();
        } else {
            current_node = root_.get();
            // If partial_sequences is empty, it implies 0 scenarios.
            num_scenarios = partial_sequences.empty() ? 0 : partial_sequences.size();
            current_path_sequences.resize(num_scenarios); // Initialize with empty vectors for each scenario
        }

        size_t group_segment_idx = 0; // Index for current group in `partial_groups`
        auto time_it = times.begin(); // Iterator for times (per group)

        for (const auto& group : partial_groups) {
            // Find or create the child node for this group
            if (current_node->children.find(group) == current_node->children.end()) {
                current_node->children[group] = std::make_unique<PrefixTreeNode>();
            }
            current_node = current_node->children[group].get();

            // Set the completion times for this group at this node for all scenarios
            current_node->times = *time_it;
            ++time_it;

            // Build the cumulative sequence for each scenario at this node
            if (num_scenarios > 0) {
                for (size_t s_idx = 0; s_idx < num_scenarios; ++s_idx) {
                    // Append the current group's sequence element for this scenario
                    current_path_sequences[s_idx].insert(current_path_sequences[s_idx].end(), partial_sequences[s_idx].begin()+ sequence_index , partial_sequences[s_idx].begin()+ sequence_index + group.size());
                }
            }
            // Assign the newly built cumulative sequence to the current node
            current_node->sequences = current_path_sequences;
            sequence_index+=group.size();
            group_segment_idx++;
        }
    }


    //Search for existing prefix
    //returns index of group in common, sequences for each scenarios for prefix, time for each scenarioso after prefix (could be recomputed from sequence)
    std::tuple<int, const std::vector<std::vector<int>>*, const std::vector<int>*> search(const GroupMetaSolution& gms) const {
        const PrefixTreeNode* current = root_.get();
        int matching_groups = 0;
        const std::vector<std::vector<int>>* prefix_sequences = nullptr; // Pointer to the sequence at the last matched node
        const std::vector<int>* times_at_prefix = nullptr;               // Pointer to the times at the last matched node

        // if no groups match, `matching_groups` is 0, and pointers should be null.

        for (const auto& search_group : gms.get_task_groups()) {
            auto it = current->children.find(search_group);
            if (it != current->children.end()) {
                // Match found
                current = it->second.get();
                matching_groups++;
                prefix_sequences = &current->sequences; // Update pointer to the sequence of the newly matched node
                times_at_prefix = &current->times;     // Update pointer to the times of the newly matched node
            } else {
                // No match for this group, so similarity stops here
                break;
            }
        }
        // Return the tuple
        return std::make_tuple(matching_groups, prefix_sequences, times_at_prefix);
    }
};

#endif // PREFIX_TREE_H