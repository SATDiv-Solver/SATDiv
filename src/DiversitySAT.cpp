#include "DiversitySAT.h"

#include <cassert>
#include <iostream>
#include <unistd.h>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <cstdlib>
#include <numeric>
#include <cstdio>

#include "core/Dimacs.h"
using std::make_pair;
using std::cout, std::endl;

DiversitySAT:: DiversitySAT(const Argument &params) {
    
    srand(seed);
    cnf_path = params.input_cnf_path;
    // output_path = params.output_path;
    k = params.k;
    candidate_set_size = params.candidate_set_size;
    stop_length = params.stop_length;
    premise_break = params.premise_break;

    mx_fail_times = params.mx_fail_times;
    mx_continue_times = params.mx_continue_times;
    mx_cell_times = params.mx_cell_times;

    use_cell_mode = params.use_cell_mode;
    use_greedy_step_var = params.use_greedy_step_var;
    use_sol_tabu = params.use_sol_tabu;
    use_var_tabu = params.use_var_tabu;

    use_pair = params.use_pair;
    mu = params.mu;
    mu_var = params.mu_var;

    gen.seed(seed = params.seed);

    string real_input_cnf_path = cnf_path;
    char reduced_cnf_template[] = "/tmp/satdiv_reduced_XXXXXX";
    int reduced_cnf_fd = mkstemp(reduced_cnf_template);
    assert(reduced_cnf_fd != -1);
    close(reduced_cnf_fd);
    string reduced_cnf_path = reduced_cnf_template;
    string cmd = "./bin/coprocessor -enabled_cp3 -up -subsimp -no-bve -no-bce -no-dense -dimacs=" + 
        reduced_cnf_path + " " + cnf_path;
    std::ignore = system(cmd.c_str());
    real_input_cnf_path = reduced_cnf_path;

    CNFInfo cnf(real_input_cnf_path);
    cnf.dump(nvar, nclauses, clauses, pos_in_cls, neg_in_cls);
    std::remove(reduced_cnf_path.c_str());
    dbg(nvar, nclauses);

    if (params.generate_strategy == "maple") {
        use_simp = false;
        solver.eliminate(true);
    } else {
        use_simp = true;
    }
    gzFile in = gzopen(real_input_cnf_path.c_str(), "rb");
    if (in == NULL)
        printf("c ERROR! Could not open file: %s\n", real_input_cnf_path.c_str()), exit(1);
    Minisat::parse_DIMACS(in, solver);
    gzclose(in);

    count_each_var_[0].resize(nvar + 2, 0);
    count_each_var_[1].resize(nvar + 2, 0);

    if (! params.just_optimize) {
        vector<int> first_sol(nvar);
        assert(solve());
        for (int v = 0; v < nvar; v ++)
            first_sol[v] = solver.modelValue(v) == l_True;

        UpdateInfo(first_sol);
        sol_set.emplace_back(first_sol);
    }
    solver.no_use_progate_diver = false;

    candidate_set_.resize(candidate_set_size * 2);

    std::cout << "init end\n";
}

void DiversitySAT::set_solution_set (string init_solution_set_path) {

    /*FILE *in_sol = fopen(init_solution_set_path.c_str(), "r");

    vector<int> sol(nvar, 0);
    for (int c, p = 0; (c = fgetc(in_sol)) != EOF; ) {
        if (c == '\n') {
            UpdateInfo(sol);
            sol_set.emplace_back(sol);
            // dbg(sol);
            p = 0;

            std::cout << "\033[;32mc current solution num: " << sol_set.size()
            << ", current SumDis: " << SumDis << " \033[0m" << std::endl;
        }
        else if (isdigit(c)){
            if(c == '1')
                sol[p] = 1;
            else
                sol[p] = 0;
            p ++;
        }
    }*/

    std::ifstream infile(init_solution_set_path.c_str());
    if (!infile) {
        printf("c Error opening %s for read.\n", init_solution_set_path.c_str());
        exit(1);
    }
    std::string buffer;
    while (std::getline(infile, buffer)) {
        size_t tab_pos = buffer.find('\t');
        if (tab_pos != buffer.npos) buffer = buffer.substr(tab_pos + 1);
        if (buffer.empty() || buffer.front() != 'v') continue;
        std::vector<int> solution(nvar, -1);
        size_t pos = 1;
        for (auto &v : solution) {
            pos = buffer.find_first_not_of(' ', pos);
            if (pos == buffer.npos) break;
            if (buffer[pos] != '0' && buffer[pos] != '1') {
                printf("c Error in initial solution line: %s: expected '0' or '1', got '%c'.\n", buffer.c_str(), buffer[pos]);
                exit(1);
            }
            v = (buffer[pos] == '1') ? 1 : 0;
            ++pos;
        }
        if (solution.back() == -1) {
            printf("c Warning: incomplete solution in initial solution file %s, ignored.\n", init_solution_set_path.c_str());
            continue;
        }
        UpdateInfo(solution);
        sol_set.emplace_back(solution);
    }

    cout << "load " << sol_set.size() << " solutions, SumDis = " << SumDis << endl;
    run ();
}

DiversitySAT:: ~DiversitySAT() {}

int DiversitySAT::get_gain (const vector<int> &sol) {
    int gain = 0;
    for (int i = 0; i < nvar; i ++)
        gain += count_each_var_[sol[i] ^ 1][i];
    return gain;
}

int DiversitySAT::SelectFromCandidateSet () {
    vector<pair<int, int>> gain;
    gain.resize(2 * candidate_set_size);
    for(int i = 0; i < 2 * candidate_set_size; i ++) {
        if (sol_set.size() == 1 && i < candidate_set_size)
            gain[i].first = -1;
        else
            gain[i].first = get_gain(candidate_set_[i]);
        gain[i].second = i;
    }
    sort(gain.begin(), gain.end());

    if(gain[2 * candidate_set_size - 1].first <= 0)
        return -1;

    vector<vector<int>> tmp;
    tmp.resize(candidate_set_size);
    for(int i = candidate_set_size; i < 2 * candidate_set_size; i ++)
        tmp[i - candidate_set_size] = candidate_set_[gain[i].second];
    for(int i = 0; i < candidate_set_size; i ++)
        candidate_set_[i] = tmp[i];
    return candidate_set_size - 1;
}

int DiversitySAT::GenerateSol () {
    GenerateCandidateSet(true);
    return SelectFromCandidateSet();
}

void DiversitySAT::GenerateCandidateSet (bool use_cache) {
    vector<int> phase;

    for(int i = use_cache ? candidate_set_size : 0; 
        i < (use_cache ? 2 * candidate_set_size : candidate_set_size);
        i ++) {
        
        phase.clear();
        for(int v = 0; v < nvar; v ++) {
            int v0 = count_each_var_[0][v];
            int v1 = count_each_var_[1][v];
            
            assert(v0 + v1 > 0);
            if (v0 < v1)
                phase.push_back(- v - 1);
            else if (v1 < v0)
                phase.push_back(v + 1);
            else {
                if (gen() % 2 == 0)
                    phase.push_back(v + 1);
                else
                    phase.push_back(- v - 1);
            }
        }

        for (int lit : phase)
            solver.setPolarity(abs(lit) - 1, lit > 0);
        assert(solve());
        candidate_set_[i].resize(nvar);
        for (int v = 0; v < nvar; v ++)
            candidate_set_[i][v] = solver.modelValue(v) == l_True;
        // for (int lit : phase)
        //     solver.unphase(abs(lit) - 1);
    }
}

void DiversitySAT::UpdateInfo (const vector<int> &sol) {
    for (int i = 0; i < nvar; i ++) {
        SumDis += count_each_var_[sol[i] ^ 1][i];
        count_each_var_[sol[i]][i] ++;
    }
}

void DiversitySAT::run () {
    for (sol_num = sol_set.size() + 1; sol_num <= k; sol_num ++) {
        
        int idx = GenerateSol ();
        if(idx == -1)
            break ;
        
        // if (sol_num == 2)
        //     cout << "SumDis = " << SumDis << '\n';

        sol_set.emplace_back(candidate_set_[idx]);
        UpdateInfo(candidate_set_[idx]);

        // if (sol_num == 2) {
        //     int tmp = 0;
        //     for (int i = 0; i < nvar; i ++)
        //         tmp += (sol_set[0][i] ^ sol_set[1][i]);
        //     cout << tmp << '\n';
        //     exit(0);
        // }

        std::cout << sol_num
            << ", SumDis: " << SumDis << std::endl;
    }
}

void DiversitySAT::SaveResult(string output_path) {

    cout << "SumDis: " << SumDis << ", Mx_SumDis: " << Mx_SumDis << endl;

    if (! best_sol_set.empty())
        sol_set = best_sol_set;

    std::ofstream res_file(output_path);

    for (const vector<int>& sol: sol_set) {
        for (int v = 0; v < nvar; v++)
            res_file << sol[v] << " ";
        res_file << "\n";
    }
    res_file.close();

    std::cout << "c Solution set saved in " << output_path << std::endl;
}

void DiversitySAT::gradient_descent () {

    int maxi = -0x3f3f3f3f;
    vector<pair<int,int>> choices, first_best;

    vector<int> ord(k);
    std::iota(ord.begin(), ord.end(), 0);
    std::shuffle(ord.begin(), ord.end(), gen);
    bool first = 1;

    for (int i : ord) {
        if (use_sol_tabu && greedy_limit - last_greedy_time_sol[i] <= taboo)
            continue ;

        for (int v = 0; v < nvar; v ++) {
            if ((use_var_tabu && greedy_limit - last_greedy_time_var[v] <= taboo) || disabled[v])
                continue ;

            auto [valid, gain] = get_gain_flip(i, v);
            if (valid) {
                if (gain > maxi || choices.empty()) {
                    maxi = gain;
                    choices.clear(), choices.emplace_back(i, v);
                    if (first)
                        first_best.clear(), first_best.emplace_back(i, v);
                }
                else if (gain == maxi) {
                    choices.emplace_back(i, v);
                    if (first)
                        first_best.emplace_back(i, v);
                }
            }
            if (premise_break && maxi > 0 && gen() % 100 < 10)
                break ;
        }

        if (premise_break && maxi > 0 && gen() % 100 < 10)
            break ;

        if (first)
            first = 0;
    }

    if (choices.empty()) {
        if (greedy_step_var () > 0)
            return ;
        
        if (gen () % 100 < 90) {
            int gain = meow_change ();
            if (gain > 0)
                return ;
        }

        if (gen() % 100 < 10 && backtrack_row (false))
            return ;
        // random_replace ();
        return ;
    }

    if (maxi > 0) {
        auto [i, v] = choices[gen() % choices.size()];
        flip(i, v);
        return ;
    }

    // 局部最优
    // TODO. weight
    if (gen() % 100 < 80 && greedy_step_var () > 0)
        return ;
    
    if (! first_best.empty()) {
        auto [i, v] = first_best[gen() % first_best.size()];
        flip(i, v);
        return ;
    }
    
    if (! choices.empty()) {
        auto [i, v] = choices[gen() % choices.size()];
        flip(i, v);
        return ;
    }

    if (gen() % 100 < 20) {
        backtrack_row (true);
        return ;
    }
    
    if (greedy_step_var () <= 0)
        random_replace ();
    return ;
}

bool DiversitySAT::check_for_flip(int sol_id, int v) {

    const vector<int>& sol = sol_set[sol_id];
    int curbit = sol[v];

    const vector<int>& var_cov_old = (curbit ? pos_in_cls[v + 1]: neg_in_cls[v + 1]);
    const vector<int>& var_cov_new = (curbit ? neg_in_cls[v + 1]: pos_in_cls[v + 1]);
    vector<int>& cur_clauses_cov = clauses_cov[sol_id];

    bool has0 = true;
    for (int cid: var_cov_new)
        cur_clauses_cov[cid] ++;
    for (int cid: var_cov_old){
        cur_clauses_cov[cid] --;
        if (cur_clauses_cov[cid] == 0)
            has0 = false;
    }

    for (int cid: var_cov_new) -- cur_clauses_cov[cid];
    for (int cid: var_cov_old) ++ cur_clauses_cov[cid];

    return has0;

}

void DiversitySAT::flip(int sol_id, int v) {
    
    const vector<int>& sol = sol_set[sol_id];
    int curbit = sol[v];

    int count_here[2] = {count_each_var_[0][v], count_each_var_[1][v]};
    count_here[curbit] --;

    int cur_score = count_here[curbit ^ 1];
    int new_score = count_here[curbit ^ 1 ^ 1];
    SumDis += (new_score - cur_score);

    const vector<int>& var_cov_old = (curbit ? pos_in_cls[v + 1]: neg_in_cls[v + 1]);
    const vector<int>& var_cov_new = (curbit ? neg_in_cls[v + 1]: pos_in_cls[v + 1]);
    vector<int>& cur_clauses_cov = clauses_cov[sol_id];

    for (int cid: var_cov_new)
        assert(cid >= 0 && cid < (int)cur_clauses_cov.size()), cur_clauses_cov[cid] ++;
    for (int cid: var_cov_old)
        assert(cid >= 0 && cid < (int)cur_clauses_cov.size()), cur_clauses_cov[cid] --;
    
    sol_set[sol_id][v] ^= 1;
    count_each_var_[curbit][v] --, count_each_var_[curbit ^ 1][v] ++;

    ++ greedy_limit;
    last_greedy_time_sol[sol_id] = greedy_limit;
    last_greedy_time_var[v] = greedy_limit;
}

pair<int,int> DiversitySAT::get_gain_flip (int sol_id, int v) {
    
    if (! check_for_flip(sol_id, v))
        return make_pair(false, -0x3f3f3f3f);
    
    const vector<int>& sol = sol_set[sol_id];
    int curbit = sol[v];

    int count_here[2] = {count_each_var_[0][v], count_each_var_[1][v]};
    count_here[curbit] --;

    int cur_score = count_here[curbit ^ 1];
    int new_score = count_here[curbit ^ 1 ^ 1];

    return {true, new_score - cur_score};
}

int DiversitySAT::get_gain_replace(int sol_id, const vector<int> &new_sol) {

    const vector<int>& cur_sol = sol_set[sol_id];
    
    int score = 0;
    for (int v = 0; v < nvar; v ++) {
        count_each_var_[cur_sol[v]][v] --;
     
        score += count_each_var_[new_sol[v] ^ 1][v];
        score -= count_each_var_[cur_sol[v] ^ 1][v];
    }
    
    for (int v = 0; v < nvar; v ++)
        count_each_var_[cur_sol[v]][v] ++;
    
    return score;
}

void DiversitySAT::replace(int sol_id, const vector<int> &new_sol, bool update_clauses) {

    vector<int>& cur_sol = sol_set[sol_id];
    for (int v = 0; v < nvar; v ++) {
        count_each_var_[cur_sol[v]][v] --;
     
        SumDis += count_each_var_[new_sol[v] ^ 1][v];
        SumDis -= count_each_var_[cur_sol[v] ^ 1][v];

        count_each_var_[new_sol[v]][v] ++;
    }

    if (update_clauses) {
        vector<int>& cur_clauses_cov = clauses_cov[sol_id];
        cur_clauses_cov = vector<int>(nclauses, 0);
        for(int i = 0; i < nvar; i ++) {
            const vector<int>& var = (new_sol[i] ? pos_in_cls[i + 1]: neg_in_cls[i + 1]);
            for (int x: var) cur_clauses_cov[x] ++;
        }
    }
    

    sol_set[sol_id] = new_sol;
}

bool DiversitySAT::backtrack_row (bool must) {

    int sol_id = gen() % k;

    vector<pair<int,int>> choices, all;

    for (int i = 0; i < k; i ++) {
        int gain = get_gain_replace (sol_id, best_sol_set[i]);
        if (gain > 0)
            choices.emplace_back(-gain, i);
        all.emplace_back(-gain, i);
    }
    sort(choices.begin(), choices.end());
    
    if (! choices.empty()) {
        auto [g, i] = choices[gen() % std::min(10, (int)choices.size())];
        replace(sol_id, best_sol_set[i]);
        return true;
    }
    else if (must || gen() % 100 < 10) {
        auto [g, i] = all[gen() % std::min(10, (int)all.size())];
        replace(sol_id, best_sol_set[i]);
        return true;
    }

    return false;
}

void DiversitySAT::random_replace () {

    int sol_id = gen() % k;
    vector<int> new_sol; new_sol.resize(nvar);

    vector<int> phase;
    for(int v = 0; v < nvar; v ++) {
        int v0 = count_each_var_[0][v];
        int v1 = count_each_var_[1][v];

        assert(v0 + v1 > 0);
        if (gen() % (v0 + v1) < v0)
            phase.push_back(v + 1);
        else
            phase.push_back(- v - 1);
    }

    for (int lit : phase)
        solver.setPolarity(abs(lit) - 1, lit > 0);
    assert(solve());
    for (int v = 0; v < nvar; v ++)
        new_sol[v] = solver.modelValue(v) == l_True;
    // for (int lit : phase)
    //     solver.unphase(abs(lit) - 1);

    replace(sol_id, new_sol);
}

int DiversitySAT::greedy_step_var () {

    if (! use_greedy_step_var)
        return -1;

    int Mx = -1;
    std::vector<int> choices;
    
    for (int x = 0; x < nvar; x ++) {
        if ((use_var_tabu && greedy_limit - last_greedy_time_var[x] <= taboo) || disabled[x])
            continue ;

        if (choices.empty() || abs(count_each_var_[0][x] - count_each_var_[1][x]) > Mx) {
            Mx = abs(count_each_var_[0][x] - count_each_var_[1][x]);
            choices.clear(), choices.push_back(x);
        }
        else if (abs(count_each_var_[0][x] - count_each_var_[1][x]) == Mx)
            choices.push_back(x);
    }

    if (choices.empty())
        return -1;

    // cout << "Mx = " << Mx << endl;
    int x = choices[gen() % choices.size()];
    greedy_limit ++;
    last_greedy_time_var[x] = greedy_limit;

    int lit = count_each_var_[0][x] > count_each_var_[1][x] ? (x + 1) : (- x - 1);
    
    vector<int> ord(k);
    std::iota(ord.begin(), ord.end(), 0);
    std::shuffle(ord.begin(), ord.end(), gen);
    int sample_num = k;

    int mx_gain = -1;
    vector<int> choices_sol_id;
    vector<vector<int>> choices_new_sol;

    for (int o = 0; o < sample_num; o ++) {

        int sol_id = ord[o];

        if (use_sol_tabu && greedy_limit - last_greedy_time_sol[sol_id] <= taboo)
            continue ;

        vector<int> phase, new_sol(nvar);
        auto assume_lit = Minisat::mkLit(abs(lit - 1), lit < 0);
        for (int v = 0; v < nvar; v ++) {
            int v0 = count_each_var_[0][v];
            int v1 = count_each_var_[1][v];

            assert(v0 + v1 > 0);
            /*if (gen() % (v0 + v1) < v0)
                phase.push_back(v + 1);
            else
                phase.push_back(- v - 1);*/
            if (v0 < v1)
                phase.push_back(- v - 1);
            else if (v1 < v0)
                phase.push_back(v + 1);
            else {
                if (gen() % 2 == 0)
                    phase.push_back(v + 1);
                else
                    phase.push_back(- v - 1);
            }
        }
        for (int lit : phase)
            solver.setPolarity(abs(lit) - 1, lit > 0);
        auto ret = solve(assume_lit);
        if (ret)
            for (int v = 0; v < nvar; v ++)
                new_sol[v] = solver.modelValue(v) == l_True;
        // for (int lit : phase)
        //     solver.unphase(abs(lit) - 1);

        if (!ret) {
            disabled[x] = true;
            return false;
        }

        int gain = get_gain_replace(sol_id, new_sol);
        if (gain > mx_gain || choices_sol_id.empty()) {
            mx_gain = gain;
            choices_sol_id.clear(), choices_sol_id.push_back(sol_id);
            choices_new_sol.clear(), choices_new_sol.push_back(new_sol);
        }
        else if (gain == mx_gain) {
            choices_sol_id.push_back(sol_id);
            choices_new_sol.push_back(new_sol);
        }
    }

    if (choices_sol_id.empty())
        return -1;

    if (mx_gain > 0 || gen() % 100 < 10) {
        int c = gen() % choices_sol_id.size();
        replace(choices_sol_id[c], choices_new_sol[c]);
        last_greedy_time_sol[choices_sol_id[c]] = greedy_limit;
        return std::max(mx_gain, 1);
    }
    
    return -1;
}

int DiversitySAT::change_worst_solution () {
    
    long long mn = -1;
    std::vector<int> choices;

    if (gen () % 100 < 95) {
        for (int sol_id = 0; sol_id < k; sol_id ++) {
            if (use_sol_tabu && greedy_limit - last_greedy_time_sol[sol_id] <= taboo)
                continue ;
            
            const vector<int>& cur_sol = sol_set[sol_id];
            long long cur_score = 0;
            for (int v = 0; v < nvar; v ++)
                cur_score += count_each_var_[cur_sol[v] ^ 1][v];
            
            if (choices.empty() || cur_score < mn) {
                mn = cur_score;
                choices.clear(), choices.push_back(sol_id);
            }
            else if (cur_score == mn)
                choices.push_back(sol_id);
        }
    }

    // std::cout << mn << std::endl;
    // assert(! choices.empty());
    
    int sol_id = choices.empty() ? gen() % k : choices[gen() % choices.size()];
    
    int gain = -1;
    choices.clear();

    GenerateCandidateSet(false);
    for(int i = 0; i < candidate_set_size; i ++) {
        int cur_gain = get_gain_replace(sol_id, candidate_set_[i]);
        if (choices.empty() || cur_gain > gain) {
            gain = cur_gain;
            choices.clear(), choices.push_back(i);
        }
        else if (cur_gain == gain)
            choices.push_back(i);
    }
    assert(! choices.empty());

    if (gain > 0 /*|| (gen() % 100 < 1)*/) {
        int i = choices[gen() % choices.size()];
        replace(sol_id, candidate_set_[i]);

        greedy_limit ++;
        last_greedy_time_sol[sol_id] = greedy_limit;
    }
    return gain;
}

void DiversitySAT::get_a_solution_by_cadical (vector<int> &new_sol) {
    new_sol.resize(nvar);
    vector<int> phase;
    for(int v = 0; v < nvar; v ++) {
        int v0 = count_each_var_[0][v];
        int v1 = count_each_var_[1][v];    
        assert(v0 + v1 > 0);
        if (v0 < v1)
            phase.push_back(- v - 1);
        else if (v1 < v0)
            phase.push_back(v + 1);
        else {
            if (gen() % 2 == 0)
                phase.push_back(v + 1);
            else
                phase.push_back(- v - 1);
        }
    }
    for (int lit : phase)
        solver.setPolarity(abs(lit) - 1, lit > 0);
    assert(solve());
    for (int v = 0; v < nvar; v ++)
        new_sol[v] = solver.modelValue(v) == l_True;
}

void DiversitySAT::get_a_solution_by_cadical_force_lit (vector<int> &new_sol, int lit) {
    new_sol.resize(nvar);

    vector<int> phase;
    for(int v = 0; v < nvar; v ++) {
        int v0 = count_each_var_[0][v];
        int v1 = count_each_var_[1][v];    
        assert(v0 + v1 > 0);
        if (v0 < v1)
            phase.push_back(- v - 1);
        else if (v1 < v0)
            phase.push_back(v + 1);
        else {
            if (gen() % 2 == 0)
                phase.push_back(v + 1);
            else
                phase.push_back(- v - 1);
        }
    }

    auto assume_lit = Minisat::mkLit(abs(lit) - 1, lit < 0);
    for (int lit : phase)
        solver.setPolarity(abs(lit) - 1, lit > 0);
    auto ret = solve(assume_lit);
    if (ret)
        for (int v = 0; v < nvar; v ++)
            new_sol[v] = solver.modelValue(v) == l_True;
    // for (int lit : phase)
    //     solver.unphase(abs(lit) - 1);

    if (!ret) {
        disabled[abs(lit) - 1] = 1;
        get_a_solution_by_cadical(new_sol);
    }
}

void DiversitySAT::get_op2_candidates ( int sol_id1, const vector<int> &new_sol,
                                        int &op2_sol_id, vector<int> &op2_sol, int &opt2_gain ) {
    
    const vector<int> &cur_sol = sol_set[sol_id1];
    for (int v = 0; v < nvar; v ++)
        count_each_var_[cur_sol[v]][v] --;

    vector<std::tuple<int, int, int>> var_choices;
    for (int v = 0; v < nvar; v ++) {
        if (disabled[v])
            continue ;

        int delta = count_each_var_[cur_sol[v] ^ 1][v] - count_each_var_[new_sol[v] ^ 1][v];
        if (delta <= 0)
            continue ;
        var_choices.emplace_back(delta, v, gen());
    }
    std::sort(var_choices.begin(), var_choices.end(),
            [](const auto& a, const auto& b) {
                return std::get<0>(a) == std::get<0>(b) ? 
                    std::get<2>(a) < std::get<2>(b) :
                    std::get<0>(a) > std::get<0>(b);
            });
    
    dbg(var_choices.size());
    if (var_choices.empty()) {
        for (int v = 0; v < nvar; v ++)
            cout << cur_sol[v] << ' ';
        cout << endl;
        for (int v = 0; v < nvar; v ++)
            cout << new_sol[v] << ' ';
        exit(0);
    }
    
    // 修改 sol_id1
    for (int v = 0; v < nvar; v ++)
        count_each_var_[new_sol[v]][v] ++;
    

    vector<int> ord(k);
    std::iota(ord.begin(), ord.end(), 0);
    
    opt2_gain = -1;
    for (int i = 0; i < mu_var; i ++) {
        auto [delta, v, _] = var_choices[i];
        int lit = cur_sol[v] ? (v + 1) : (- v - 1);

        std::shuffle(ord.begin(), ord.end(), gen);
        for (int sol_id2 : ord) {
            if (use_sol_tabu && greedy_limit - last_greedy_time_sol[sol_id2] <= taboo)
                continue ;

            get_a_solution_by_cadical_force_lit (op2_sol, lit);

            int cur_gain = get_gain_replace (sol_id2, op2_sol);
            if (cur_gain > 0) {
                op2_sol_id = sol_id2;
                opt2_gain = cur_gain;
                break ;
            }

            if (disabled[v])
                break ;
        }

        if (opt2_gain > 0)
            break ;
    }
    
    for (int v = 0; v < nvar; v ++) {
        count_each_var_[cur_sol[v]][v] ++;
        count_each_var_[new_sol[v]][v] --;
    }
}

int DiversitySAT::pair_change () {

    vector<int> not_tabu_sol;
    for (int sol_id = 0; sol_id < k; sol_id ++) {
        if (use_sol_tabu && greedy_limit - last_greedy_time_sol[sol_id] <= taboo)
            continue ;
        not_tabu_sol.push_back(sol_id);
    }
    std::shuffle(not_tabu_sol.begin(), not_tabu_sol.end(), gen);
    vector<vector<int>> op1_solutions(not_tabu_sol.size());

    int mx_gain = -1;
    vector<int> choices;
    for (uint i = 0; i < not_tabu_sol.size(); i ++) {
        int sol_id1 = not_tabu_sol[i];
        vector<int> &new_sol = op1_solutions[i];
        get_a_solution_by_cadical(new_sol);
        int cur_gain = get_gain_replace(sol_id1, new_sol);
        if (cur_gain <= 0)
            continue ;
        
        if (choices.empty() || cur_gain > mx_gain) {
            mx_gain = cur_gain;
            choices.clear(), choices.push_back(i);
        }
        else if (cur_gain == mx_gain)
            choices.push_back(i);
        
        if ((! choices.empty()) && gen() % 10 == 0)
            break ;
    }
    if ((! choices.empty()) && (! use_pair)) {
        int i = choices[gen() % choices.size()];
        int sol_id1 = not_tabu_sol[i];
        replace(sol_id1, op1_solutions[i]);

        greedy_limit ++;
        last_greedy_time_sol[sol_id1] = greedy_limit;
        return mx_gain;
    }

    if (! use_pair)
        return -1;
    
    // cout << "pair\n";
    int op1_nums = std::min((int) not_tabu_sol.size(), mu);
    int sol_id2, gain2;
    vector<int> op2_solution;
    for (int i = 0; i < op1_nums; i ++) {
        int sol_id1 = not_tabu_sol[i];
        get_op2_candidates (sol_id1, op1_solutions[i], sol_id2, op2_solution, gain2);
        if (gain2 > 0) {
            replace(sol_id1, op1_solutions[i]);
            replace(sol_id2, op2_solution);

            greedy_limit ++;
            last_greedy_time_sol[sol_id1] = greedy_limit;
            last_greedy_time_sol[sol_id2] = greedy_limit;
            return gain2;
        }
    }
    return -1;
}

int DiversitySAT::meow_change () {

    std::vector<int> new_sol;
    get_a_solution_by_cadical(new_sol);

    int mx_gain = -1;
    vector<int> choices;
    for (int sol_id = 0; sol_id < k; sol_id ++) {
        if (use_sol_tabu && greedy_limit - last_greedy_time_sol[sol_id] <= taboo)
            continue ;
        
        int gain = get_gain_replace(sol_id, new_sol);
        if (choices.empty() || gain > mx_gain) {
            mx_gain = gain;
            choices.clear(), choices.emplace_back(sol_id);
        }
        else if (gain == mx_gain)
            choices.emplace_back(sol_id);
    }

    if (choices.empty()) {
        // return -1;
        choices.emplace_back(gen() % k);
    }
    // cout << "gain0: " << mx_gain << endl;
    
    int sol_id = choices[gen() % choices.size()];
    std::vector<int> opt_new_sol = new_sol;
    int gain = mx_gain;

    if (gain < -(nvar / 5)) {
        vector<int> tmp_sol;
        sol_id = gen() % k;
        get_a_solution_by_cadical(tmp_sol);
        int tmp_gain = get_gain_replace(sol_id, tmp_sol);
        // cout << "QAQ_gain: " << tmp_gain << endl;

        if (tmp_gain > gain) {
            gain = tmp_gain;
            opt_new_sol = tmp_sol;
        }
    }

    if (gain > 0) {
        replace(sol_id, opt_new_sol);
        greedy_limit ++;
        last_greedy_time_sol[sol_id] = greedy_limit;
        return gain;
    }

    if (gain == 0 || (! use_pair)) {
        gradient_descent ();
        return -1;
    }
    
    // cout << "pair\n";
    int sol_id2, gain2;
    vector<int> op2_solution;
    get_op2_candidates (sol_id, opt_new_sol, sol_id2, op2_solution, gain2);
    gain2 += gain;
    // cout << "gain2: " << gain2 << endl;
    if (gain2 > 0 || gen() % 100 < 1) {
        replace(sol_id, opt_new_sol);
        replace(sol_id2, op2_solution);

        greedy_limit ++;
        last_greedy_time_sol[sol_id] = greedy_limit;
        last_greedy_time_sol[sol_id2] = greedy_limit;
        return gain2;
    }

    return -1;
}

void DiversitySAT::optimize () {

    last_greedy_time_sol = vector<int>(k + 1, -taboo - 1);
    last_greedy_time_var = vector<int>(nvar + 1, -taboo - 1);

    disabled = vector<bool>(nvar, 0);

    clauses_cov = vector<vector<int> >(k, vector<int>(nclauses, 0));
    for (int i = 0; i < k; i ++) {
        const vector<int>& sol = sol_set[i];
        vector<int>& cur_clauses_cov = clauses_cov[i];
        for (int j = 0; j < nvar; j ++) {
            const vector<int>& vec = (sol[j] ? pos_in_cls[j + 1]: neg_in_cls[j + 1]);
            for (int x: vec) ++ cur_clauses_cov[x];
        }
    }

    int cur_step = 0;

    best_sol_set = sol_set;
    Mx_SumDis = SumDis;

    [[maybe_unused]] int last_update_step = 0;
    dbg(use_pair, use_cell_mode);

    int small_delta_times = 0;
    int no_delte_times = 0;
    int mode = 0, swicth_times = 0, switch_bar = 100;
    bool updated = false;
    for ( ; cur_step < stop_length; cur_step ++) {

        if (! use_cell_mode)
            mode = 0;

        if (mode == 1)
            gradient_descent ();
        else {
            assert(mode == 0);
            meow_change ();
        }

        if (SumDis > Mx_SumDis) {
            updated = true;
            no_delte_times = 0;

            if (use_cell_mode)
                cout << "mode" << mode << " ";

            auto offset = SumDis - Mx_SumDis;
            if (offset < 10) {
                small_delta_times ++;
                if (use_cell_mode)
                    cout << "t" << small_delta_times << " ";
            }
            else
                small_delta_times = 0;
            
            if (small_delta_times >= 50)
                mode = 1;

            Mx_SumDis = SumDis;
            best_sol_set = sol_set;
            cout << "dis: " << Mx_SumDis << endl;

            last_update_step = 0;
        }
        else if (use_cell_mode) {
            // cout << "curdis: " << SumDis << endl;
            no_delte_times ++;
            if (no_delte_times >= switch_bar) {
                mode ^= 1;
                cout << "switch to mode" << mode << endl;

                if (! updated)
                    swicth_times ++;
                if (swicth_times > 10) {
                    switch_bar = std::min(switch_bar * 10, 10000000);
                }

                updated = false;
            }
        }
        last_update_step ++;
    }

    sol_set = best_sol_set;
    SumDis = Mx_SumDis;
}
