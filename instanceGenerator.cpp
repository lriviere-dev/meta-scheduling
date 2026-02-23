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

    void generate_and_save(int N, int S, double prec_density, int cluster_number, double intra_cluster_var, double inter_cluster_var, const std::string& path) {
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
        std::uniform_int_distribution<int> r_dist(0, half_sum);//possible base release dates.
        std::vector<std::vector<int>> cluster_scenarios(cluster_number, std::vector<int>(N));
        std::vector<int> global_base_R(N);//used when inter_cluster_var is not maximal (value 1)


        for (int i = 0; i < N; ++i) {
            global_base_R[i] = r_dist(rng);
        }

        for (int s = 0; s < cluster_number; ++s) {
            for (int i = 0; i < N; ++i) {
                cluster_scenarios[s][i] = inter_cluster_var == 1 ? r_dist(rng) : global_base_R[i] + static_cast<int>(inter_cluster_var * r_dist(rng));
                }
        }

        // 2bis. Scenario Sampling 
        std::vector<std::vector<int>> scenarios(S, std::vector<int>(N));
        std::uniform_int_distribution<int> sample_pick(0, cluster_number-1);
        std::normal_distribution<double> intra_var_gen(0, half_sum * intra_cluster_var);

        //Setting the first listed scenarios as the clusters center themselves (so the can be retrieved if needed)
        for (int s = 0; s < std::min(S, cluster_number); ++s) {
            for (int j = 0; j < N; ++j) {
                scenarios[s][j] = cluster_scenarios[s][j];
            }
        }

        //fill rest with random scenarios around the cluster centers
        for (int s = cluster_number; s < S; ++s) {
            int cluster = sample_pick(rng);//choose which cluster this scenario is from
            for (int j = 0; j < N; ++j) { //draw each task around cluster scenario values with intra_cluster variance (this creates the "modes" around the cluster centers)
                scenarios[s][j] = std::max(0, cluster_scenarios[cluster][j] + static_cast<int>(intra_var_gen(rng)));
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
    std::string bench_name = "given_clusters"; //name of the folder where generated instances will be stored (inside instances/)

    std::vector<int> ns = {100};
    std::vector<double> prec_densities = {0.01};
    std::vector<double> inter_var = {1};
    std::vector<double> intra_var = {0.005, 0.01, 0.03, 0.05 };
    std::vector<int> number_cluster = {5, 25};
    int nb_base_variants = 3;

    std::cout << "Generating instances..." << std::endl;
    int count = 0;

    for (int N : ns) {
        for (double density : prec_densities) {
            for (double inter_var : inter_var) {
                for (double intra_var : intra_var) {
                    for (int cluster : number_cluster) {    
                        for (int i = 0; i < nb_base_variants; ++i) {
                                // Generate Test Instance (1000 scenarios)
                                std::string test_file = "instances/" + bench_name + "/" + 
                                                        bench_name + "_N" + std::to_string(N) + 
                                                        "_prec" + format_num(density) + 
                                                        "_I" + std::to_string(i) + 
                                                    "_S1000_inter" + format_num(inter_var) + 
                                                    "_intra" + format_num(intra_var) + 
                                                    "_cluster" + std::to_string(cluster) + 
                                                    ".data";
                            gen.generate_and_save(N, 1000, density, cluster, intra_var, inter_var, test_file);
                            std::cout << "."; std::cout.flush();
                            count++;
                        }
                    }
                }
            }
        }
    }

    std::cout << "\nGeneration complete : " << count << " instances generated." << std::endl;
    return 0;
}