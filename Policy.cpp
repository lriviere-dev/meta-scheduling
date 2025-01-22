#include "Policy.h"
#include "Sequence.h"
#include "Schedule.h"
#include "Instance.h"
#include "MetaSolutions.h" 
#include "ListMetaSolutions.h" 
#include <algorithm>

//extracts a sequence from a metasolution for a given scenario.
Sequence FIFOPolicy::extract_sequence(const MetaSolution& metaSolution, DataInstance& scenario) const {
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
        return Sequence(sequence);
    }
    else if (const auto* SeqMeta = dynamic_cast<const SequenceMetaSolution*>(&metaSolution)) {
        return Sequence((*SeqMeta).get_sequence().get_tasks());
    }
    // Handling all ListMetaSolution types via their underlying metasolution type (recursive)
    else if (const auto* listMeta = dynamic_cast<const ListMetaSolutionBase*>(&metaSolution)) {
        Sequence minSeq = extract_sequence(*listMeta->get_meta_solutions().front(), scenario); // Initialize with the first sequence

        for (const auto& meta_sol : listMeta->get_meta_solutions()) {
            Sequence seq = extract_sequence(*meta_sol, scenario);

            if (seq.isLexicographicallySmaller(minSeq, scenario)) {
                minSeq = seq; // Copy the Sequence
            }
        }
        return minSeq; // Return the valid Sequence    
    }
    // Add other MetaSolution type checks here if necessary

    throw std::invalid_argument("Unsupported MetaSolution type in FIFOPolicy::extract_sequence.");
}

int FIFOPolicy::extract_sub_metasolution_index(const MetaSolution& metasol, DataInstance& scenario) const {
    
    //assert list solution
    const ListMetaSolutionBase* listMetaSolution = dynamic_cast<const ListMetaSolutionBase*>(&metasol);
    if (!listMetaSolution) {
        throw std::runtime_error("MetaSolution must be of type ListMetaSolutionBase.");
    }
    Sequence minSeq = extract_sequence(*listMetaSolution->get_meta_solutions().front(), scenario); // Initialize with the first sequence
    int minIndex = 0;

    for (int i=0 ; i < listMetaSolution->get_meta_solutions().size(); i++) {
        Sequence seq = extract_sequence(*listMetaSolution->get_meta_solutions()[i], scenario);
        if (seq.isLexicographicallySmaller(minSeq, scenario)) {
            minSeq = seq; 
            minIndex = i; 
        }
    }
    return minIndex; // Return the valid Sequence    
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
    /* Backup code for other objectives :  
    // model.add(IloMinimize(env, IloSum(objectives) / nbScenarios));
    IloIntExprArray L(env,nbScenarios);// array of obj for each scenario
    for (int s = 0; s < nbScenarios; s++) {
        IloIntExprArray L2(env, nbJobs); // array of obj for each tas of a scenario

        if (scenario_obj == 0) {
            //Compute objective : Maximum lateness over tasks
            for (int i=0;i<nbJobs;i++) {
                L2[i] = IloMax(0, IloEndOf(Jobs[s][i]) - dueDate[(dVar)?s:0][i]);
            }
            L[s] = IloMax(L2); //max lateness
        }
        else { //sumCi
            for (int i=0;i<nbJobs;i++) {
                L2[i] = IloEndOf(Jobs[s][i]);
            }
            L[s] = IloSum(L2); // max completion time
        }
    }
    */

}