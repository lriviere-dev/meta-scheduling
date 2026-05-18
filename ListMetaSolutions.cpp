#include "ListMetaSolutions.h"
#include "Policy.h"
 
ListMetaSolution* ListMetaSolution::front_sub_metasolutions(Policy* policy, const DataInstance& instance) {
    if (!scored_by)
        policy->evaluate_meta(*this, instance);
    else if (scored_by != policy || scored_for != &instance) {
        reset_evaluation();
        policy->evaluate_meta(*this, instance);
    }
 
    std::vector<int> unique_indexes;
    for (int idx : front_indexes)
        if (std::find(unique_indexes.begin(), unique_indexes.end(), idx) == unique_indexes.end())
            unique_indexes.push_back(idx);
    std::sort(unique_indexes.begin(), unique_indexes.end());
 
    ListMetaSolution* front = new ListMetaSolution();
    for (int i : unique_indexes)
        front->add_meta_solution(*metaSolutions[i]); // clones via add_meta_solution
    return front;
}
