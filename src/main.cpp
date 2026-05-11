#include "DiversitySAT.h"
#include "Argument.h"
#include <iostream>
#include <signal.h>
#include <cstring>
using namespace std;

#include <clipp.h>


Argument argument;
DiversitySAT* g_diversitySAT = nullptr;

void HandleInterrupt(int sig){

    if (g_diversitySAT) {
        g_diversitySAT->SaveResult(argument.output_path);
    }

    cout << "c" << endl;
    cout << "c caught signal... exiting" << endl;

    exit(-1);
}

void SetupSignalHandler(){
    signal(SIGTERM, HandleInterrupt);
    signal(SIGINT, HandleInterrupt);
    signal(SIGQUIT, HandleInterrupt);
    signal(SIGKILL, HandleInterrupt);
}

Argument ParseArgument(int argc, char *argv[]) {
    Argument argument;
    using namespace clipp;
    auto cli = ( //
        option("-i", "-input_cnf_path").required(true) &
            value("input file", argument.input_cnf_path),
        option("-o", "-output_path").required(true) &
            value("output file", argument.output_path),
        (option("-seed") & value("random seed", argument.seed))
            % "change random seed (default 1)",
        (option("-k") & value("samples count", argument.k))
            % "set target solutions count (default 100)",
        (option("-cadical_randec") & value("percentage", argument.cadical_randec))
            % "percentage of cadical for random decision"
    );

    bool help_all = false;
    auto cli_lagacy = (
        (option("--help-all").set(help_all, true)),

        (option("-candidate_set_size") & value("size", argument.candidate_set_size))
            % "set lambda (default 1)",
        (option("-maxsat_cutoff_time") & value("maxsat_cutoff_time", argument.maxsat_cutoff_time))
            % "maxsat cutoff time (default 3)",
        (option("-generate_strategy") & value("generate strategy", argument.generate_strategy))
            % "generate strategy (default cadical)",

        (option("-stop_length") & value("stop_length", argument.stop_length))
            % "stop length (default 1,000,000,000)",
        (option("-just_optimize") & value("just_optimize", argument.just_optimize))
            % "just optimize (default 0)",
        (option("-init_solution_set_path") & value("init_solution_set_path", argument.init_solution_set_path))
            % "initial solution set path (default "")",

        option("-premise_break").set(argument.premise_break, true) |
            option("-not_use_premise_break").set(argument.premise_break, false),
        
        (option("-mx_fail_times") & value("mx_fail_times", argument.mx_fail_times))
            % "max fail times (default 100)",
        (option("-mx_continue_times") & value("mx_continue_times", argument.mx_continue_times))
            % "max continue times (default 20)",
        (option("-mx_cell_times") & value("mx_cell_times", argument.mx_cell_times))
            % "max cell times (default 10000)",
        
        option("-use_cell_mode").set(argument.use_cell_mode, true) |
            option("-not_use_cell_mode").set(argument.use_cell_mode, false),
        option("-use_greedy_step_var").set(argument.use_greedy_step_var, true) |
            option("-not_use_greedy_step_var").set(argument.use_greedy_step_var, false),
        option("-use_sol_tabu").set(argument.use_sol_tabu, true) |
            option("-not_use_sol_tabu").set(argument.use_sol_tabu, false),
        option("-use_var_tabu").set(argument.use_var_tabu, true) |
            option("-not_use_var_tabu").set(argument.use_var_tabu, false),
        
        option("-use_pair").set(argument.use_pair, true) |
            option("-not_use_pair").set(argument.use_pair, false),
        (option("-mu") & value("mu", argument.mu))
            % "mu (default 10)",
        (option("-mu_var") & value("mu_var", argument.mu_var))
            % "mu_var (default 10)",
        
        option("-use_local_search_solver").set(argument.use_pair, true) |
            option("-not_use_local_search_solver").set(argument.use_pair, false),
        (option("-maxsat_max_tries") & value("maxsat_max_tries", argument.maxsat_max_tries))
            % "maxsat_max_tries (default 10)",
        (option("-maxsat_max_non_improve_flip") & value("maxsat_max_non_improve_flip", argument.maxsat_max_non_improve_flip))
            % "maxsat_max_non_improve_flip (default 10000)",
        
        option("-use_local_search_postopt").set(argument.use_local_search_postopt, true) |
            option("-not_use_local_search_postopt").set(argument.use_local_search_postopt, false)
    );

    if (!parse(argc, argv, (cli, cli_lagacy))) {
        std::cerr << make_man_page(cli, argv[0]);
        exit(1);
    }
    return argument;
}

int main(int argc, char **argv) {

    SetupSignalHandler();

    argument = ParseArgument(argc, argv);
    
    DiversitySAT diversitySAT(argument);
    g_diversitySAT = &diversitySAT;

    // diversitySAT.check_cadical ();
    // return 0;
    
    if (argument.just_optimize)
        diversitySAT.set_solution_set(argument.init_solution_set_path);

    else {    
        if (argument.generate_strategy == "contextsat" || argument.generate_strategy == "cadical")
            diversitySAT.run();
        else if (argument.generate_strategy == "maxsat")
        diversitySAT.run_maxsat();
    }
    
    
    diversitySAT.optimize();
    diversitySAT.SaveResult(argument.output_path);

    std::cout << "End" << std::endl;
    return 0;
}