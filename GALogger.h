#ifndef GA_LOGGER_H
#define GA_LOGGER_H

#include <vector>
#include <iostream>

struct EpochStats {
    int epoch       = 0;
    int pool_size   = 0;
    int front_size  = 0;
    int train_score = 0;
    int test_score = 0;
    double elapsed_seconds = 0.0;

    // V3+: add fields here, e.g.:
    // double slack = 0.0;
    // int validate_score = 0;
};

class GALogger {
public:
    void reset() { 
        log.clear(); 
        start_time = std::chrono::steady_clock::now();
    }
    void record(const EpochStats& stats) {
        EpochStats s = stats;
        s.elapsed_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start_time).count();
        log.push_back(s);
    }
    const std::vector<EpochStats>& get_log() const { return log; }

    void print_last() const {
    if (log.empty()) return;
    const auto& s = log.back();
    std::cout << s.epoch           << "\t"
              << s.pool_size       << "\t"
              << s.front_size      << "\t"
              << s.train_score     << "\t"
              << s.test_score      << "\t"
              << s.elapsed_seconds << "\n";
    }

    void print_header() const {
        std::cout << "epoch\tpool\tfront\tscore\ttest_score\ttime(s)\n";
    }
    void print() const {
        print_header();
        for (const auto& s : log)
            std::cout << s.epoch           << "\t"
                      << s.pool_size       << "\t"
                      << s.front_size      << "\t"
                      << s.train_score     << "\t"
                      << s.test_score      << "\t"
                      << s.elapsed_seconds << "\n";
    }

private:
    std::vector<EpochStats> log;
    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
};

#endif // GA_LOGGER_H