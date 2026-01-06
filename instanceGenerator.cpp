#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <random>
#include <algorithm>
#include <numeric>
#include <filesystem>
#include <cmath>
#include <iomanip>
#include <sstream>

std::string format_num(double val) {
    std::ostringstream oss;
    oss << std::noshowpoint << val; 
    return oss.str();
}
namespace fs = std::filesystem;

class Generator {
private:
    std::mt19937 rng;

    // Helper to perform transitive closure to maintain the DAG properties
    void transitive_closure(int N, std::vector<uint8_t>& prec) {
        // Floyd-Warshall style transitive closure is simple and effective for these densities
        for (int k = 0; k < N; ++k) {
            for (int i = 0; i < N; ++i) {
                for (int j = 0; j < N; ++j) {
                    if (prec[i * N + k] && prec[k * N + j]) {
                        prec[i * N + j] = 1;
                    }
                }
            }
        }
    }

    void add_random_prec(int N, std::vector<uint8_t>& prec) {
        std::uniform_int_distribution<int> dist(0, N - 1);
        bool found = false;
        while (!found) {
            int a = dist(rng);
            int b = dist(rng);

            if (a == b) continue;
            // Enforce lexicographical order (a < b) to ensure no cycles (DAG)
            if (a > b) std::swap(a, b); 

            if (prec[a * N + b]) continue;

            prec[a * N + b] = 1;
            found = true;
        }
        transitive_closure(N, prec);
    }

public:
    Generator(unsigned int seed) : rng(seed) {}

    void generate_and_save(int N, int S, double prec_density, double var, double mode_dist_factor, const std::string& path) {
        // 1. Durations (P)
        std::uniform_int_distribution<int> p_dist(1, 49);
        std::vector<int> durations(N);
        long long sum_p = 0;
        for (int i = 0; i < N; ++i) {
            durations[i] = p_dist(rng);
            sum_p += durations[i];
        }
        int half_sum = static_cast<int>(sum_p / 2);

        // 2. Hierarchical Base Release Date Generation
        std::uniform_int_distribution<int> r_dist(0, half_sum);
        std::vector<int> global_base_R(N);
        std::vector<int> mode_A_centers(N);
        std::vector<int> mode_B_centers(N);

        for (int i = 0; i < N; ++i) {
            global_base_R[i] = r_dist(rng);
            
            // mode_dist_factor determines how far the two modes are from each other
            int offset = static_cast<int>(global_base_R[i] * mode_dist_factor);
            mode_A_centers[i] = global_base_R[i];
            mode_B_centers[i] = global_base_R[i] + offset;
        }

        // 2bis. Scenario Sampling (Equiprobable between modes)
        std::vector<std::vector<int>> scenarios(S, std::vector<int>(N));
        std::bernoulli_distribution coin_flip(0.5); // 50/50 chance for each mode

        for (int j = 0; j < N; ++j) {
            // Local variance for the bell curves around the centers
            double sigma_A = std::max(1.0, var * mode_A_centers[j]);
            double sigma_B = std::max(1.0, var * mode_B_centers[j]);
            
            std::normal_distribution<double> dist_A(mode_A_centers[j], sigma_A);
            std::normal_distribution<double> dist_B(mode_B_centers[j], sigma_B);
            
            for (int i = 0; i < S; ++i) {
                double val = coin_flip(rng) ? dist_A(rng) : dist_B(rng);
                scenarios[i][j] = std::max(0, static_cast<int>(std::round(val)));
            }
        }

        // 3. Precedence Constraints (E)
        std::vector<uint8_t> prec(N * N, 0);
        double max_prec = N * (N - 1) / 2.0;

        // Calculate ratio directly in the loop condition
        while (max_prec > 0 && (std::accumulate(prec.begin(), prec.end(), 0) / max_prec) < prec_density) {
            add_random_prec(N, prec);
        }

        // 4. Due Dates (Base R) and Due Dates (D)
        std::uniform_int_distribution<int> d_dist(half_sum, 2 * half_sum);
        std::vector<int> dueDates(N);
        for (int i = 0; i < N; ++i) {
            dueDates[i] = d_dist(rng);
        }

        // 5. Write to File (Matching DataInstance.h format)
        fs::create_directories(fs::path(path).parent_path());
        std::ofstream ofs(path);
        if (!ofs) return;

        ofs << N << " " << S << "\n\n";

        // Write Precedence Matrix
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N; ++j) {
                ofs << (int)prec[i * N + j] << " ";
            }
            ofs << "\n";
        }
        ofs << "\n";

        // Write Durations
        for (int i = 0; i < N; ++i) ofs << durations[i] << " ";
        ofs << "\n\n";

        // Write Scenarios (Release Dates)
        for (int i = 0; i < S; ++i) {
            for (int j = 0; j < N; ++j) {
                ofs << scenarios[i][j] << " ";
            }
            ofs << "\n";
        }
        ofs << "\n";

        // Write Due Dates
        for (int i = 0; i < N; ++i) ofs << dueDates[i] << " ";
        ofs << "\n";

        ofs.close();
    }
};

int main() {
    int seed = 42;
    Generator gen(seed);
    std::string bench_name = "bimodal_r";

    std::vector<int> ns = {100};
    std::vector<double> prec_densities = {0.01};
    std::vector<double> variances = { 0.1, 0.3};
    std::vector<double> mode_dist_factors = {0, 0.5, 1.0};
    int nb_base_variants = 5;

    std::cout << "Generating instances..." << std::endl;

    for (int N : ns) {
        for (double density : prec_densities) {
            for (double var : variances) {
                for (double mode_dist_factor : mode_dist_factors) {
                    for (int i = 0; i < nb_base_variants; ++i) {
                        // Generate Test Instance (1000 scenarios)
                        std::string test_file = "instances/" + bench_name + "/" + 
                                                bench_name + "_N" + std::to_string(N) + 
                                                "_prec" + format_num(density) + 
                                                "_I" + std::to_string(i) + 
                                            "_S1000_var" + format_num(var) + 
                                            "_mode" + format_num(mode_dist_factor) + 
                                            ".data";
                    gen.generate_and_save(N, 1000, density, var, mode_dist_factor, test_file);
                    std::cout << "."; std::cout.flush();
                    }
                }
            }
        }
    }

    std::cout << "\nGeneration complete." << std::endl;
    return 0;
}