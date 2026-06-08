#ifndef GENETIC_ALGORITHM_H
#define GENETIC_ALGORITHM_H

#include "MetaSolutions.h"
#include "ListMetaSolutions.h"
#include "BestOfAlgorithm.h"
#include "GALogger.h"
#include "Policy.h"
#include "Instance.h"
#include "Algorithms.h"
#include "Ideal.h"
#include "Diversity.h"
#include <memory>
#include <random>

class GeneticAlgorithm {
public:
    // mutations_per_epoch: how many random mutations to attempt each epoch
    GeneticAlgorithm(Policy* policy, int epochs, int mutations_per_epoch, std::mt19937& rng)
        : policy(policy)
        , epochs(epochs)
        , mutations_per_epoch(mutations_per_epoch)
        , rng(rng)
        , bestof(policy)
    {}


    ListMetaSolution* build_initial_pool(
        const DataInstance& trainInstance,
        int jseq_time)
        {
            // Ideal solver
            IdealPolicy ideal_policy;
            IdealSolver ideal_solver(&ideal_policy);
            ideal_solver.setMaxTime(jseq_time > 10*60 ? 10*60 : jseq_time); //capping ideal solver time to 10 minutes 
            MetaSolution* ideal_solution = ideal_solver.solve(trainInstance);
    
            // CP solver JSEQ
            JSEQSolver jseq_solver(policy, jseq_time);
            MetaSolution* jseq_solution = jseq_solver.solve(trainInstance);
    
            // Diversification
            std::vector<SequenceMetaSolution> seeds;
            seeds.push_back(*dynamic_cast<SequenceMetaSolution*>(jseq_solution)); //addd JSEQ solution as a seed with the idealsolutions
            //add a seed for each idealsolution:
            for (Sequence seq : (dynamic_cast<IdealMetaSolution*> (ideal_solution))->get_sequences()){
                SequenceMetaSolution x = SequenceMetaSolution(seq);
                if(std::find(seeds.begin(), seeds.end(), x) == seeds.end()) {//not adding doubles
                    seeds.push_back(x);
                }
            }

            std::cout << "Number of seeds: " << seeds.size() << std::endl;
            std::vector<SequenceMetaSolution> diversified = diversify_step_neighbours(seeds, trainInstance);
            std::cout << "Initial pool size: " << diversified.size() << std::endl;
            return new ListMetaSolution(diversified);
        }


    // Returns newly allocated ListMetaSolution — caller takes ownership.
    ListMetaSolution* solve(const ListMetaSolution& initial_pool,
                            const DataInstance& train_instance,
                            const DataInstance& test_instance) //test instance is only used for logging test scores during the process, it is not used for selection or anything else.
    {
        logger.reset();
        logger.print_header();
        std::unique_ptr<ListMetaSolution> pool(initial_pool.clone());

        for (int epoch = 0; epoch < epochs; ++epoch) {

            // --- Mutation ---
            std::vector<MetaSolution*> members = pool->get_meta_solutions(); // view only
            for (int m = 0; m < mutations_per_epoch; ++m) {
                if (members.empty()) break;
                std::uniform_int_distribution<size_t> dist(0, members.size() - 1);
                MetaSolution* target = members[dist(rng)];
                auto candidate = target->mutate(train_instance, rng);
                if (!candidate) continue; // mutation failed (e.g. no valid neighbors) — skip
                policy->evaluate_meta(*candidate, train_instance);
                pool->add_meta_solution(*candidate);
            }

            // Pressure selection : take a subset of the test scenarios.
            if (train_instance.getS() < 2) throw std::runtime_error("Training instance must have at least 2 scenarios for train/train split.");
            DataInstance * train_train_instance = train_instance.SampleSplitScenarios(train_instance.getS()/2, rng).first; //todo : replace with virtual scenario eval in evaluate meta! (currently leaks a bit too)

            // --- Selection: Best-Subset prunes harmful/redundant members ---
            bestof.set_initial_solution(*pool);
            pool.reset(dynamic_cast<ListMetaSolution*>(bestof.solve(*train_train_instance)));
            policy->evaluate_meta(*pool, *train_train_instance);

            // --- Log ---
            EpochStats stats;
            stats.epoch       = epoch;
            stats.pool_size   = pool->get_meta_solutions_size();
            stats.front_size  = static_cast<int>(pool->get_front_size());
            stats.train_score = policy->evaluate_meta(*pool, train_instance);
            stats.test_score = policy->evaluate_meta(*pool, test_instance);
            logger.record(stats);
            logger.print_last();
        }

        return pool.release();
    }

    const GALogger& get_logger() const { return logger; }

private:
    Policy*         policy;
    int             epochs;
    int             mutations_per_epoch;
    std::mt19937&   rng;
    BestOfAlgorithm bestof;
    GALogger        logger;
};

#endif // GENETIC_ALGORITHM_H