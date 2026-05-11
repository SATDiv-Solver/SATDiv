#ifndef _ARGUMENT_H_
#define _ARGUMENT_H_

#include <cstring>
#include <iostream>
#include <string>

struct Argument {
    int seed = 1, k = 100;
    std::string input_cnf_path;
	std::string reduced_cnf_file_path;
    std::string output_path;

    int candidate_set_size = 1;
    std::string generate_strategy = "cadical"; // "cadical", "contextsat"
    int cadical_randec = 30; // percentage: [0, 100]
    
    int stop_length = 1'000'000'000;

    int just_optimize = 0;
    std::string init_solution_set_path = "";

    bool premise_break = false;

    int mx_fail_times = 100;
    int mx_continue_times = 20;
    int mx_cell_times = 10000;

    bool use_cell_mode = false;
    bool use_greedy_step_var = true;
    bool use_sol_tabu = true;
    bool use_var_tabu = true;

    bool use_pair = false;
    int mu = 10, mu_var = 10;

};

#endif  /* _ARGUMENT_H_ */
