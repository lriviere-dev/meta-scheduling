#ifndef LIST_META_H
#define LIST_META_H

class Policy;

#include "MetaSolutions.h"
#include <vector>
#include <iostream>
#include <queue>
#include <memory>
#include <type_traits>

// Abstract base — unchanged interface so Policy.h and BestOfAlgorithm.h compile against it
class ListMetaSolutionBase : public MetaSolution {
public:
    virtual ~ListMetaSolutionBase() {}
    virtual std::vector<MetaSolution*> get_meta_solutions() const = 0;
    virtual int get_meta_solutions_size() const = 0;
    virtual void add_meta_solution(const MetaSolution& sol) = 0;
    virtual void remove_meta_solution_index(size_t index) = 0;
    virtual void remove_meta_solution_index_update(
        size_t index,
        std::vector<std::queue<size_t>>& scenarios_priority_indexes,
        std::vector<int>& scenarios_position_of_indexes,
        std::vector<size_t>& scenarios_reverse_positions) = 0;

    // Covariant clone — callers that know they have a ListMetaSolutionBase get a ListMetaSolutionBase* back
    virtual ListMetaSolutionBase* clone() const override = 0;

    void reset_evaluation() override {
        scored_by = nullptr;
        scored_for = nullptr;
        score = -1;
        scores.clear();
        front_sequences.clear();
        front_indexes.clear();
    }

    std::vector<int> front_indexes;
};


// Concrete type-erased list. Replaces ListMetaSolution<T>.
// Sub-metasolutions are owned via unique_ptr — no aliasing, automatic cleanup.
class ListMetaSolution : public ListMetaSolutionBase {
public:

    // --- Constructors ---

    // Default: empty list
    ListMetaSolution() = default;

    // From a typed vector — clones each element so the list fully owns its content.
    // Works for any T that derives from MetaSolution.
    template<typename T>
    explicit ListMetaSolution(const std::vector<T>& solutions) {
        static_assert(std::is_base_of<MetaSolution, T>::value, "T must derive from MetaSolution");
        metaSolutions.reserve(solutions.size());
        for (const auto& sol : solutions)
            metaSolutions.push_back(std::unique_ptr<MetaSolution>(sol.clone()));
    }

    // Copy constructor — deep clones all sub-metasolutions AND evaluation data
    ListMetaSolution(const ListMetaSolution& other) {
        metaSolutions.reserve(other.metaSolutions.size());
        for (const auto& sol : other.metaSolutions)
            metaSolutions.push_back(std::unique_ptr<MetaSolution>(sol->clone()));
        // copy cached evaluation data so re-evaluation is not needed after a copy
        scored_by       = other.scored_by;
        scored_for      = other.scored_for;
        score           = other.score;
        scores          = other.scores;
        front_sequences = other.front_sequences;
        front_indexes   = other.front_indexes;
    }

    ListMetaSolution& operator=(const ListMetaSolution& other) {
        if (this == &other) return *this;
        metaSolutions.clear();
        metaSolutions.reserve(other.metaSolutions.size());
        for (const auto& sol : other.metaSolutions)
            metaSolutions.push_back(std::unique_ptr<MetaSolution>(sol->clone()));
        scored_by       = other.scored_by;
        scored_for      = other.scored_for;
        score           = other.score;
        scores          = other.scores;
        front_sequences = other.front_sequences;
        front_indexes   = other.front_indexes;
        return *this;
    }

    // Move constructor and assignment — free, unique_ptr handles it
    ListMetaSolution(ListMetaSolution&&) = default;
    ListMetaSolution& operator=(ListMetaSolution&&) = default;

    // --- clone ---
    ListMetaSolution* clone() const override {
        return new ListMetaSolution(*this);
    }

    // --- Core interface ---

    // Returns raw view-only pointers into internal storage.
    // WARNING: never hold these across mutations or insertions (vector may reallocate).
    std::vector<MetaSolution*> get_meta_solutions() const override {
        std::vector<MetaSolution*> ptrs;
        ptrs.reserve(metaSolutions.size());
        for (const auto& sol : metaSolutions)
            ptrs.push_back(sol.get());
        return ptrs;
    }

    int get_meta_solutions_size() const override {
        return static_cast<int>(metaSolutions.size());
    }

    // Clones sol and takes ownership of the copy
    void add_meta_solution(const MetaSolution& sol) override {
        metaSolutions.push_back(std::unique_ptr<MetaSolution>(sol.clone()));
        reset_evaluation();
    }

    // Typed bulk access — downcast filter, O(n). Use for EW/diversification steps.
    // Returns raw view-only pointers, same warning as get_meta_solutions().
    template<typename T>
    std::vector<T*> get_typed() const {
        std::vector<T*> out;
        for (const auto& sol : metaSolutions)
            if (auto* p = dynamic_cast<T*>(sol.get()))
                out.push_back(p);
        return out;
    }

    // Typed value copy — use when you need owned copies (e.g. to fill a std::vector<T>)
    template<typename T>
    std::vector<T> get_typed_copies() const {
        std::vector<T> out;
        for (const auto& sol : metaSolutions)
            if (auto* p = dynamic_cast<T*>(sol.get()))
                out.push_back(*p);
        return out;
    }

    // --- Removal ---

    // Swap-pop: O(1) but reorders the vector. Resets evaluation.
    void remove_meta_solution_index(size_t index) override {
        if (index >= metaSolutions.size()) throw std::out_of_range("Index out of range");
        metaSolutions[index] = std::move(metaSolutions.back());
        metaSolutions.pop_back();
        reset_evaluation();
    }

    // In-place update variant used by BestOfAlgorithm.
    // Maintains score/front data without full re-evaluation.
    void remove_meta_solution_index_update(
        size_t index,
        std::vector<std::queue<size_t>>& scenarios_priority_indexes,
        std::vector<int>& position_of_indexes,
        std::vector<size_t>& reverse_positions) override
    {
        if (index >= metaSolutions.size()) throw std::out_of_range("Index to remove is out of range");

        // Swap-pop (move semantics, no copy)
        metaSolutions[index] = std::move(metaSolutions.back());

        int maxScore = -1;

        for (size_t s = 0; s < scenarios_priority_indexes.size(); ++s) {
            if (this->front_indexes[s] == static_cast<int>(index)) {
                // The active sub-metasolution for this scenario was removed: find next
                scenarios_priority_indexes[s].pop();
                int position = -1;
                size_t next_id = 0;
                while (position < 0) {
                    next_id = scenarios_priority_indexes[s].front();
                    position = position_of_indexes[next_id];
                    if (position < 0) scenarios_priority_indexes[s].pop();
                }
                size_t safe_pos = (static_cast<size_t>(position) == metaSolutions.size() - 1) 
                                ? index 
                                : static_cast<size_t>(position);
                this->front_indexes[s]   = safe_pos;
                this->scores[s]          = metaSolutions[safe_pos]->scores[s];
                this->front_sequences[s] = metaSolutions[safe_pos]->front_sequences[s];
                }
            // If the next active element is the one we just moved (was at back, now at index)
            if (this->front_indexes[s] == static_cast<int>(metaSolutions.size() - 1)) {
                this->front_indexes[s] = static_cast<int>(index);
            }
            if (this->scores[s] > maxScore) maxScore = this->scores[s];
        }

        // Update index tracking arrays
        size_t moved_id = reverse_positions.back();
        position_of_indexes[moved_id] = static_cast<int>(index);
        position_of_indexes[reverse_positions[index]] = -1;
        reverse_positions[index] = moved_id;
        reverse_positions.pop_back();

        this->score = maxScore;
        metaSolutions.pop_back();
    }

    // --- Front extraction ---

    size_t get_front_size() const {
        if (!scored_by) throw std::runtime_error("metasolution must be scored to get front size");
        std::vector<int> seen;
        for (int idx : front_indexes)
            if (std::find(seen.begin(), seen.end(), idx) == seen.end())
                seen.push_back(idx);
        return seen.size();
    }

    ListMetaSolution* front_sub_metasolutions(Policy* policy, const DataInstance& instance);

    // --- Print ---

    void print() const override {
        std::cout << "{";
        for (size_t i = 0; i < metaSolutions.size(); ++i) {
            metaSolutions[i]->print();
            if (i != metaSolutions.size() - 1) std::cout << ", ";
        }
        std::cout << "}";
    }

    // --- Equality ---

    MetaSolution* clone_as_meta() const { return new ListMetaSolution(*this); }

    bool equals(const MetaSolution& other) const override {
        const ListMetaSolution* o = dynamic_cast<const ListMetaSolution*>(&other);
        return o && compareLists(*o);
    }

    bool operator==(const MetaSolution& other) const {
        return equals(other);
    }

    bool compareLists(const ListMetaSolution& other) const {
        if (metaSolutions.size() != other.metaSolutions.size()) return false;
        for (const auto& sol : metaSolutions) {
            bool found = false;
            for (const auto& otherSol : other.metaSolutions)
                if (sol->equals(*otherSol)) { found = true; break; }
            if (!found) return false;
        }
        for (const auto& otherSol : other.metaSolutions) {
            bool found = false;
            for (const auto& sol : metaSolutions)
                if (otherSol->equals(*sol)) { found = true; break; }
            if (!found) return false;
        }
        return true;
    }

private:
    std::vector<std::unique_ptr<MetaSolution>> metaSolutions;
};

#endif // LIST_META_H