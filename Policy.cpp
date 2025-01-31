#include "Policy.h"
#include "Sequence.h"
#include "Schedule.h"
#include "Instance.h"
#include "MetaSolutions.h" 
#include "ListMetaSolutions.h" 
#include <algorithm>

//extracts a sequence from a metasolution for a given scenario.
Sequence FIFOPolicy::extract_sequence(const MetaSolution& metaSolution, DataInstance& scenario) const {

    if(metaSolution.scored_by){throw std::runtime_error("Extracting sequence but metasol is already scored.");}

    std::vector<int> emptyTasks;
    Sequence output(emptyTasks); 
    bool set_output = false;
    // Try to cast to GroupMetaSolution
    if (const auto* groupMeta = dynamic_cast<const GroupMetaSolution*>(&metaSolution)) {
        // Handle GroupMetaSolution

        std::vector<int> sequence;

        // Iterate over groups in order
        for (const auto& group : groupMeta->get_task_groups()) {
            // Sort tasks in the group by release date, with ties going to lexicographical order
            std::vector<int> sortedGroup = group;
            std::sort(sortedGroup.begin(), sortedGroup.end(), [&scenario](int t1, int t2) {
                //first, check precedence constraints
                if (scenario.precedenceConstraints[t1][t2]){
                    return true;
                }
                if (scenario.precedenceConstraints[t2][t1]){
                    return false;
                }
                //then, release dates
                if (scenario.releaseDates[0][t1] != scenario.releaseDates[0][t2]) {
                    return scenario.releaseDates[0][t1] < scenario.releaseDates[0][t2];
                }
                return t1 < t2; // Lexicographical order as tie-breaker
            });

            // Add sorted tasks to the final sequence
            sequence.insert(sequence.end(), sortedGroup.begin(), sortedGroup.end());
        }
        output = Sequence(sequence);
        set_output = true;
    }
    else if (const auto* SeqMeta = dynamic_cast<const SequenceMetaSolution*>(&metaSolution)) {
        output = Sequence((*SeqMeta).get_sequence().get_tasks());
        set_output = true;
    }
    // Handling all ListMetaSolution types via their underlying metasolution type (recursive)
    //we assume the underlying metasolutions have already been scored by the policy
    else if (const auto* listMeta = dynamic_cast<const ListMetaSolutionBase*>(&metaSolution)) {
        Sequence minSeq = listMeta->get_meta_solutions()[0]->front_sequences[scenario.scenario_id]; // Initialize with the first sequence

        for (const auto& meta_sol : listMeta->get_meta_solutions()) { //find the leximin sequence
            Sequence seq = meta_sol->front_sequences[scenario.scenario_id];

            if (seq.isLexicographicallySmaller(minSeq, scenario)) {
                minSeq = seq; // Copy the Sequence
            }
        }
        output = minSeq; 
        set_output = true;
    }
    // Add other MetaSolution type checks here if necessary
    if (!set_output){
        throw std::invalid_argument("Unsupported MetaSolution type in FIFOPolicy::extract_sequence.");
    }
    return output;
}

//finds the index of the metasolution (in the list of metasolution) that is used in a given scenario.
//TODO work could be avoided by saving it along front. (would also have to update when removing a sequence)
int FIFOPolicy::extract_sub_metasolution_index(const MetaSolution& metasol, DataInstance& scenario) const {
    
    //assert list solution
    const ListMetaSolutionBase* listMetaSolution = dynamic_cast<const ListMetaSolutionBase*>(&metasol);
    if (!listMetaSolution) {
        throw std::runtime_error("MetaSolution must be of type ListMetaSolutionBase.");
    }
    //asssert unique scenario
    if (scenario.S>1) {
        throw std::runtime_error("Scenario should be unique but S>1.");
    }
    //asssert metasol was scored
    if (!metasol.scored_by) {
        throw std::runtime_error("metasol should already be scored by a policy");
    }
    Sequence minSeq = listMetaSolution->get_meta_solutions()[0]->front_sequences[scenario.scenario_id]; // Initialize with the first sequence
    int minIndex = 0;

    for (size_t i=0 ; i < listMetaSolution->get_meta_solutions().size(); i++) {
        Sequence seq = listMetaSolution->front_sequences[scenario.scenario_id];
        if (seq.isLexicographicallySmaller(minSeq, scenario)) {
            minSeq = seq; 
            minIndex = i; 
        }
    }
    
    return minIndex; // Return the index of the metasolution in the list that outputs the min sequence    
}


Schedule FIFOPolicy::transform_to_schedule(const Sequence& sequence, const DataInstance& scenario) const {
        const std::vector<int>& tasks = sequence.get_tasks(); // Get tasks in sequence order
        const std::vector<int>& releaseDates = scenario.releaseDates[0]; // Access release dates for tasks
        const std::vector<int>& durations = scenario.durations; // Access task durations
       
    std::vector<int> startTimes(tasks.size(), 0);

    // Variable to track the current time
    int currentTime = 0;

    for (size_t i = 0; i < tasks.size(); ++i) {
        int task = tasks[i];
        // A task can only start after its release date and after the previous task finishes
        currentTime = std::max(currentTime, releaseDates[task]);
        startTimes[task] = currentTime; // Record the start time
        currentTime += durations[task]; // Advance current time by the task duration
    }

    return Schedule(startTimes); 
    }

void FIFOPolicy::define_objective(IloEnv env, IloModel& model, 
                        IloIntervalVarArray2& jobs, const DataInstance& instance, 
                        IloIntExprArray& scenario_scores,  IloIntVar& aggregated_objective) const {
    //objective here is max sumci. Could be other.
    int nbScenarios = instance.S;
    int nbJobs = instance.N;
    IloIntExprArray completions(env, nbJobs);
    for (int s = 0; s < nbScenarios; s++) {
        for (int i = 0; i < nbJobs; i++) {
            completions[i] = IloEndOf(jobs[s][i]);
        }
        scenario_scores[s] = IloSum(completions);

    }

    // Aggregate objectives across scenarios
    for (int s = 0; s < nbScenarios; s++)
        model.add(aggregated_objective >= scenario_scores[s]);
    model.add(IloMinimize(env, aggregated_objective));
}
