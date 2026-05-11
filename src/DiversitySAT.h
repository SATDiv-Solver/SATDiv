#ifndef LOCALSEARCH_OPTIMIZER_INCLUDE_H
#define LOCALSEARCH_OPTIMIZER_INCLUDE_H

#include "../cadical/src/cadical.hpp"

#include "cnfinfo.h"
#include "MyBitSet.h"
#include "Argument.h"

#include <vector>
#include <numeric>
#include <random>
#include <utility>
#include <algorithm>
#include <set>
#include <map>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <thread>

#if !defined (NDEBUG) and __has_include(<dbg.h>)
#define DBG_MACRO_NO_WARNING
#include <dbg.h>
#else
namespace dbg {
template <typename T, typename... Args>
decltype(auto) identity(T&& t, Args&&... args) {
    if constexpr (sizeof...(args) == 0)
        return std::forward<T>(t);
    else
        return (void) t, identity(std::forward<Args>(args)...);
}
}
#define dbg(...) dbg::identity(__VA_ARGS__)
#endif

using std::vector;
using std::pair;
using std::set;
using std::map;
using std::string;
using std::mt19937_64;
using std::thread;

class DiversitySAT {

public:
    DiversitySAT(const Argument &params);
    ~DiversitySAT();
    void run();
    void SaveResult(string output_path);
    vector<vector<int>> get_sol_set () const {
        return sol_set;
    }
    void dump(int &_nvar, int &_nclauses, vector<vector<int>> &_clauses) const {
        _nvar = nvar;
        _nclauses = nclauses;
        _clauses = clauses;
    }

    void optimize ();

    void set_solution_set (string init_solution_set_path);

    void check_cadical () {
        auto solver = std::make_unique<CaDiCaL::Solver>();
        // solver->set("shrink", 0);
        solver->set("quiet", 1);
        // solver->set("phase", 1);
        // solver->set("forcephase", 1);
        // solver->set("lucky", 0);
        solver->read_dimacs(cnf_path.c_str(), nvar);

        for (int i = 1; i <= nvar; i ++)
            solver->phase(i);
        dbg(solver->solve());
        for (int i = 1; i <= nvar; i ++)
            std::cout << (solver->val(i) > 0 ? 1 : 0);
        std::cout << '\n';

        for (int i = 1; i <= nvar; i ++)
            solver->phase(-i);
        dbg(solver->solve());
        for (int i = 1; i <= nvar; i ++)
            std::cout << (solver->val(i) > 0 ? 1 : 0);
        std::cout << '\n';
    }

private:

    bool use_cadical = true;

    int seed = 1;
    int k = 10;
    int nvar, nclauses;
    int sol_num = 0;
    u_int64_t SumDis = 0;

    string cnf_path;
    std::mt19937_64 gen;

    CaDiCaL::Solver *cadical_solver;

    vector<vector<int>> clauses;
    vector<vector<int>> pos_in_cls, neg_in_cls;
    vector<vector<int>> clauses_cov;


    vector<vector<int>> sol_set;
    vector<int> count_each_var_[2];

    int candidate_set_size = 2;
    vector<vector<int>> candidate_set_;

    void GenerateCandidateSet (bool use_cache);
    int get_gain (const vector<int> &sol);
    int SelectFromCandidateSet ();
    int GenerateSol ();
    void UpdateInfo (const vector<int> &sol);

    int stop_length;
    vector<int> last_greedy_time_sol, last_greedy_time_var;
    int taboo = 10, greedy_limit = 0;

    void gradient_descent ();
    pair<int,int> get_gain_flip (int sol_id, int v);
    void flip(int sol_id, int v);
    bool check_for_flip(int sol_id, int v);
    bool backtrack_row (bool must);
    void random_replace ();
    int get_gain_replace(int sol_id, const vector<int> &new_sol);
    void replace(int sol_id, const vector<int> &new_sol, bool update_clauses = true);
    int greedy_step_var ();

    vector<vector<int>> best_sol_set;
    vector<bool> disabled;

    bool premise_break = false;

    int change_worst_solution ();

    int mx_fail_times = 100;
    int mx_continue_times = 20;
    int mx_cell_times = 10000;

    u_int64_t Mx_SumDis;

    bool use_cell_mode = true;
    bool use_greedy_step_var = true;
    bool use_sol_tabu = true;
    bool use_var_tabu = true;

    bool use_pair = true;
    int mu = 10, mu_var = 10;
    int pair_change ();
    void get_op2_candidates (   int sol_id1, const vector<int> &new_sol,
                                int &op2_sol_id, vector<int> &op2_sol, int &opt2_gain );


    int meow_change ();
    void get_a_solution_by_cadical (vector<int> &new_sol);
    void get_a_solution_by_cadical_force_lit (vector<int> &new_sol, int lit);
};
#endif
