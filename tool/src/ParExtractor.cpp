// ----------------------------------------------------------------------------
// Copyright (c) 2013-2014 by Graz University of Technology and
//                            Johannes Kepler University Linz
//
// This is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, see
// <http://www.gnu.org/licenses/>.
//
// For more information about this software see
//   <http://www.iaik.tugraz.at/content/research/design_verification/others/>
// or email the authors directly.
//
// ----------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------
/// @file ParExtractor.cpp
/// @brief Contains the definition of the class ParExtractor.
// -------------------------------------------------------------------------------------------


#include "ParExtractor.h"
#include "Options.h"
#include "AIG2CNF.h"
#include "VarManager.h"
#include "SatSolver.h"
#include "DepQBFApi.h"
#include "Logger.h"

#include <thread>
#include <mutex>
#include "unistd.h"
#include <pthread.h>

extern "C" {
  #include "aiger.h"
}

// -------------------------------------------------------------------------------------------
///
/// @def EXTR_GO
/// @brief A constant for the circuit extractor command 'GO'.
#define EXTR_GO 0

// -------------------------------------------------------------------------------------------
///
/// @def EXTR_STOP
/// @brief A constant for the circuit extractor command 'STOP'.
#define EXTR_STOP 1

// -------------------------------------------------------------------------------------------
ParExtractor::ParExtractor(size_t nr_of_threads) :
    extr_command_(0),
    nr_of_threads_(nr_of_threads),
    nr_of_new_ands_(0)
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
ParExtractor::~ParExtractor()
{
  for(size_t cnt = 0; cnt < extractors_.size(); ++cnt)
  {
    // This is a hack: thread nr 1 may get cancelled. In this case, its memory may be in
    // a corrupted state. Hence, we do not free the memory to trade a memory corruption
    // problem with a memory leak.
    if(cnt != 1 || extractors_[cnt]->done_)
      delete extractors_[cnt];
  }
  extractors_.clear();
}

// -------------------------------------------------------------------------------------------
void ParExtractor::run(const CNF &win, const CNF &neg_win)
{
  for(size_t cnt = 0; cnt < extractors_.size(); ++cnt)
    delete extractors_[cnt];
  extractors_.clear();
  if(nr_of_threads_ >= 1)
    extractors_.push_back(new ParExtractorWorker(*this, win, neg_win));
  if(nr_of_threads_ >= 2)
    extractors_.push_back(new ParExtractorQBFWorker(*this, win, neg_win));
  if(nr_of_threads_ >= 3)
    extractors_.push_back(new ParExtractorWorker(*this, win, neg_win));

  // Start the threads:
  vector<thread> extractor_threads;
  if(nr_of_threads_ >= 2)
  {
    pthread_t thread_handle;
    int ret = pthread_create(&thread_handle, 0, &ParExtractorQBFWorker::startQBF, extractors_[1]);
    MASSERT(ret == 0, "Could not start thread with pthread_create");
    extractor_pthreads_.push_back(thread_handle);
  }
    //extractor_threads.push_back(thread(&ParExtractorWorker::runQBF, extractors_[1]));
  if(nr_of_threads_ >= 3)
    extractor_threads.push_back(thread(&ParExtractorWorker::runSAT, extractors_[2]));

  //The main thread executes the first explorer:
  extractors_[0]->runSATDep();

  // Wait until the threads are finished:
  for(size_t cnt = 0; cnt < extractor_threads.size(); ++cnt)
    extractor_threads[cnt].join();
  for(size_t cnt = 0; cnt < extractor_pthreads_.size(); ++cnt)
  {
    int ret = pthread_join((extractor_pthreads_[cnt]), 0);
    MASSERT(ret == 0, "Could not join thread with pthread_join");
  }

  // Find best circuit:
  size_t best = extractors_.size();
  for(size_t cnt = 0; cnt < extractors_.size(); ++cnt)
  {
    if(extractors_[cnt]->done_ && best == extractors_.size())
      best = cnt;
    else if(extractors_[cnt]->done_ && best < extractors_.size() &&
            extractors_[cnt]->standalone_circuit_->num_ands <
            extractors_[best]->standalone_circuit_->num_ands)
      best = cnt;
  }
  MASSERT(best < extractors_.size(), "Something is wrong with the done_ flag.");
  nr_of_new_ands_ = insertIntoSpec(extractors_[best]->standalone_circuit_);
  extractors_[best]->statistics.notifyFinalSize(nr_of_new_ands_);
}

// -------------------------------------------------------------------------------------------
void ParExtractor::killThemAll()
{
  // All C++ thread objects are nicely asked to terminate:
  extr_command_ = EXTR_STOP;

  // All pthread objects are killed:
  for(size_t cnt = 0; cnt < extractor_pthreads_.size(); ++cnt)
  {
    pthread_cancel(extractor_pthreads_[cnt]);
    //MASSERT(ret == 0, "Could not cancel thread with pthread_cancel");
  }

}

// -------------------------------------------------------------------------------------------
bool ParExtractor::allWaiting()
{
  bool all_waiting = true;
  for(size_t cnt = 0; cnt < extractors_.size(); ++cnt)
  {
    if(extractors_[cnt]->waiting_ == false)
      all_waiting = false;
  }
  return all_waiting;
}

// -------------------------------------------------------------------------------------------
void ParExtractor::logDetailedStatistics()
{
  L_LOG("Final circuit size: " << nr_of_new_ands_ << " new AND gates." );
  string done;
  for(size_t cnt = 0; cnt < extractors_.size(); ++cnt)
  {
    if(extractors_[cnt]->done_)
      done += "true, ";
    else
      done += "false, ";
  }
  L_LOG("Done: " + done);
  string second;
  for(size_t cnt = 0; cnt < extractors_.size(); ++cnt)
  {
    if(extractors_[cnt]->did_second_run_)
      second += "true, ";
    else
      second += "false, ";
  }
  L_LOG("Second run: " + second);
  for(size_t cnt = 0; cnt < extractors_.size(); ++cnt)
    if(extractors_[cnt]->done_)
      extractors_[cnt]->statistics.logStatistics();
}


// -------------------------------------------------------------------------------------------
ParExtractorWorker::ParExtractorWorker(ParExtractor &coordinator,
                                       const CNF &win_region,
                                       const CNF &neg_win_region) :
  done_(false),
  waiting_(false),
  did_second_run_(false),
  standalone_circuit_(aiger_init()),
  coordinator_(coordinator),
  check_(Options::instance().getSATSolverExtr(false, false)),
  gen_(Options::instance().getSATSolverExtr(false, true)),
  win_region_(win_region),
  neg_win_region_(neg_win_region),
  next_free_cnf_lit_(VarManager::instance().getMaxCNFVar() + 1),
  standalone_circuit2_(aiger_init())
{
  // compute some lists that are used often:
  const vector<int> &in = VarManager::instance().getVarsOfType(VarInfo::INPUT);
  const vector<int> &pres = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &ctrl = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  ip_.reserve(in.size() + pres.size());
  ip_.insert(ip_.end(), in.begin(), in.end());
  ip_.insert(ip_.end(), pres.begin(), pres.end());
  ipc_.reserve(ip_.size() + ctrl.size());
  ipc_.insert(ipc_.end(), ip_.begin(), ip_.end());
  ipc_.insert(ipc_.end(), ctrl.begin(), ctrl.end());

  // next, we would like to compute a map from CNF variables to the corresponding AIGER
  // literals in the standalone_circuit_. We want to use a vector instead of a map for
  // performance reasons. Hence, we first of all need to find out the highest CNF var
  // we may want to want to transform into the corresponding AIGER variable:
  int max_cnf_var_of_interest = 0;
  for(size_t cnt = 0; cnt < ipc_.size(); ++cnt)
    if(ipc_[cnt] > max_cnf_var_of_interest)
      max_cnf_var_of_interest = ipc_[cnt];
  const map<int, set<VarInfo> > &trans_deps = AIG2CNF::instance().getTmpDepsTrans();
  for(AIG2CNF::DepConstIter it = trans_deps.begin(); it != trans_deps.end(); ++it)
    if(it->first > max_cnf_var_of_interest)
      max_cnf_var_of_interest = it->first;

  // Now we fill the map from CNF variables to AIGER variables. While doing this, we also
  // initialize the standalone_circuit_:
  next_free_aig_lit_ = 2;
  cnf_var_to_standalone_aig_var_.resize(max_cnf_var_of_interest + 1, 0);
  for(size_t cnt = 0; cnt < ip_.size(); ++cnt)
  {
    if(ip_[cnt] != VarManager::instance().getPresErrorStateVar())
    {
      cnf_var_to_standalone_aig_var_[ip_[cnt]] = next_free_aig_lit_;
      ostringstream name;
      VarInfo info = VarManager::instance().getInfo(ip_[cnt]);
      name << "Original AIG literal: " << info.getLitInAIG();
      if(info.getName() != "")
        name << " (" << info.getName() << ")";
      aiger_add_input(standalone_circuit_, next_free_aig_lit_, name.str().c_str());
      aiger_add_input(standalone_circuit2_, next_free_aig_lit_, name.str().c_str());
      next_free_aig_lit_ += 2;
    }
  }

  for(size_t cnt = 0; cnt < ctrl.size(); ++cnt)
  {
    cnf_var_to_standalone_aig_var_[ctrl[cnt]] = next_free_aig_lit_;
    ostringstream name;
    VarInfo info = VarManager::instance().getInfo(ctrl[cnt]);
    name << "Original AIG literal: " << info.getLitInAIG();
    if(info.getName() != "")
      name << " (" << info.getName() << ")";
    aiger_add_output(standalone_circuit_, next_free_aig_lit_, name.str().c_str());
    aiger_add_output(standalone_circuit2_, next_free_aig_lit_, name.str().c_str());
    next_free_aig_lit_ += 2;
  }


  for(AIG2CNF::DepConstIter it = trans_deps.begin(); it != trans_deps.end(); ++it)
  {
    if(AIG2CNF::instance().isTrueInTrans() && it->first == 1) // CNF var 1 is constant TRUE
      cnf_var_to_standalone_aig_var_[1] = 1;
    else
    {
      cnf_var_to_standalone_aig_var_[it->first] = next_free_aig_lit_;
      next_free_aig_lit_ += 2;
    }
  }
  next_free_aig_lit2_ = next_free_aig_lit_;
}


// -------------------------------------------------------------------------------------------
ParExtractorWorker::~ParExtractorWorker()
{
  aiger_reset(standalone_circuit_);
  standalone_circuit_ = NULL;
  aiger_reset(standalone_circuit2_);
  standalone_circuit2_ = NULL;
  delete check_;
  check_ = NULL;
  delete gen_;
  gen_ = NULL;
}

// -------------------------------------------------------------------------------------------
void ParExtractorWorker::runSAT()
{
  PointInTime start_time = Stopwatch::start();

  const vector<int> &ctrl = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  vector<int> none;

  vector<CNF> c_eq_impl;
  c_eq_impl.reserve(ctrl.size());
  vector<CNF> impl;
  impl.reserve(ctrl.size());

  // we need to rename all variables except for the states and inputs:
  int nr_of_vars = next_free_cnf_lit_;
  vector<int> rename_map(nr_of_vars, 0);
  for(int cnt = 1; cnt < nr_of_vars; ++cnt)
    rename_map[cnt] = next_free_cnf_lit_++;
  for(size_t cnt = 0; cnt < ip_.size(); ++cnt)
    rename_map[ip_[cnt]] = ip_[cnt];

  CNF mustBe(neg_win_region_);
  mustBe.swapPresentToNext();
  mustBe.addCNF(win_region_);
  mustBe.addCNF(AIG2CNF::instance().getTrans());
  CNF goto_win(win_region_);
  goto_win.swapPresentToNext();
  goto_win.addCNF(AIG2CNF::instance().getTrans());
  goto_win.renameVars(rename_map);
  mustBe.addCNF(goto_win);

  // We need one activation variable per control signal. This activation variable
  // activates all control-signal-dependent constraints:
  vector<int> act_vars(ctrl.size(), 0);
  // If eq_vars[cnt] is set then ctrl[i] is equal to its copy for all cnt<i<ctrl.size()
  vector<int> eq_vars(ctrl.size(), 0);
  for(size_t cnt = 0; cnt < ctrl.size(); ++cnt)
  {
    act_vars[cnt] = next_free_cnf_lit_++;
    eq_vars[cnt] = next_free_cnf_lit_++;
  }
  for(size_t cnt = 0; cnt < ctrl.size() - 1; ++cnt)
  {
    // ctrl[cnt + 1] must be equal to its copy:
    mustBe.add3LitClause(-eq_vars[cnt], ctrl[cnt+1], -rename_map[ctrl[cnt+1]]);
    mustBe.add3LitClause(-eq_vars[cnt], -ctrl[cnt+1], rename_map[ctrl[cnt+1]]);
    // eq_vars[cnt + 1] must be set:
    mustBe.add2LitClause(-eq_vars[cnt], eq_vars[cnt+1]);
    mustBe.add2LitClause(-act_vars[cnt], eq_vars[cnt]);
  }
  CNF mustBe0(mustBe);
  CNF mustBe1(mustBe);
  for(size_t cnt = 0; cnt < ctrl.size(); ++cnt)
  {
    mustBe0.add2LitClause(-act_vars[cnt], ctrl[cnt]);
    mustBe0.add2LitClause(-act_vars[cnt], -rename_map[ctrl[cnt]]);
    mustBe1.add2LitClause(-act_vars[cnt], -ctrl[cnt]);
    mustBe1.add2LitClause(-act_vars[cnt], rename_map[ctrl[cnt]]);
  }

  // now we initialize the solvers:
  vector<int> vars_to_keep;
  vars_to_keep.reserve(ipc_.size() + ctrl.size() + ctrl.size());
  vars_to_keep.insert(vars_to_keep.end(), ipc_.begin(), ipc_.end());
  for(size_t cnt = 0; cnt < ctrl.size(); ++cnt)
  {
    vars_to_keep.push_back(act_vars[cnt]);
    vars_to_keep.push_back(rename_map[ctrl[cnt]]);
  }
  check_->startIncrementalSession(vars_to_keep, false);
  check_->incAddCNF(mustBe0);
  gen_->startIncrementalSession(vars_to_keep, false);
  gen_->incAddCNF(mustBe1);

  vector<int> dep_vars(ipc_);
  for(size_t ctrl_cnt = 0; ctrl_cnt < ctrl.size(); ++ctrl_cnt)
  {
    statistics.notifyBeforeCtrlSignal();
    int current_ctrl = ctrl[ctrl_cnt];
    Utils::remove(dep_vars, current_ctrl);
    vector<int> fixed(1, act_vars[ctrl_cnt]);

    // now we can do the actual learning loop:
    CNF solution;
    while(true)
    {
      if(coordinator_.extr_command_ == EXTR_STOP)
        return;

      vector<int> false_pos;
      // compute a false-positives (ctrl-signal is 1 but must not be):
      statistics.notifyBeforeClauseComp();
      bool false_pos_exists = check_->incIsSatModelOrCore(none, fixed, dep_vars, false_pos);
      statistics.notifyAfterClauseComp();
      if(!false_pos_exists)
        break;

      if(coordinator_.extr_command_ == EXTR_STOP)
        return;

      // generalize the false-positives:
      // (compute a larger set of situations (including the false-positive) for which
      // setting the control signal to 0 is allowed:
      vector<int> gen_false_pos;
      statistics.notifyBeforeClauseMin();
      bool sat = gen_->incIsSatModelOrCore(false_pos, fixed, none, gen_false_pos);
      MASSERT(!sat, "Impossible.");

      statistics.notifyAfterClauseMin(false_pos.size(), gen_false_pos.size());
      solution.addNegCubeAsClause(gen_false_pos);
      gen_false_pos.push_back(act_vars[ctrl_cnt]);
      check_->incAddNegCubeAsClause(gen_false_pos);
    }

    if(coordinator_.extr_command_ == EXTR_STOP)
      return;

    // dump solution in AIGER format:
    addToStandAloneAiger(current_ctrl, solution);
    // store it for future optimization or dumping:
    impl.push_back(solution);

    if(coordinator_.extr_command_ == EXTR_STOP)
      return;

    // re-substitution:
    CNF orig_resub = makeEq(current_ctrl, solution);
    c_eq_impl.push_back(orig_resub);
    check_->incAddCNF(orig_resub);
    gen_->incAddCNF(orig_resub);
    solution.renameVars(rename_map);
    CNF renamed_resub = makeEq(rename_map[current_ctrl], solution);
    check_->incAddCNF(renamed_resub);
    gen_->incAddCNF(renamed_resub);
    statistics.notifyAfterCtrlSignal();
  }

  if(coordinator_.extr_command_ == EXTR_STOP)
    return;
  statistics.notifyBeforeABC(standalone_circuit_->num_ands);
  aiger *opt = CNFImplExtractor::optimizeWithABC(standalone_circuit_);
  aiger_reset(standalone_circuit_);
  standalone_circuit_ = opt;
  statistics.notifyAfterABC(standalone_circuit_->num_ands);
  done_ = true;

  // Now we are done in principle. Let's now decide if we have time for a second minimization
  // round:
  bool second_run = true;
  if(standalone_circuit_->num_ands <= 1)
    second_run = false;
  size_t elapsed_in_first_round = Stopwatch::getRealTimeSec(start_time);
  size_t remaining = Options::instance().getEstimatedTimeRemaining();
  if(remaining == 0 || remaining <  10 * elapsed_in_first_round)
    second_run = false;

  if(second_run)
  {
    // Now the second step: try to reduce the implementations further
    gen_->doMinCores(true);
    CNF mustBe(neg_win_region_);
    mustBe.swapPresentToNext();
    mustBe.addCNF(win_region_);
    mustBe.addCNF(AIG2CNF::instance().getTrans());
    mustBe.setVarValue(VarManager::instance().getPresErrorStateVar(), false);
    for(size_t cnt = 0; cnt < ctrl.size() - 1; ++cnt)
    {
      // eq_vars[cnt] implies that ctrl[cnt + 1] is set to the old solution
      const list<vector<int> > &clauses = c_eq_impl[cnt+1].getClauses();
      for(CNF::ClauseConstIter it = clauses.begin(); it != clauses.end(); ++it)
      {
        vector<int> cl(*it);
        cl.push_back(-eq_vars[cnt]);
        mustBe.addClause(cl);
      }
      // eq_vars[cnt] implies that eq_vars[cnt+1] is set
      mustBe.add2LitClause(-eq_vars[cnt], eq_vars[cnt+1]);
      mustBe.add2LitClause(-act_vars[cnt], eq_vars[cnt]);
    }
    CNF mustBe0(mustBe);
    CNF mustBe1(mustBe);
    for(size_t cnt = 0; cnt < ctrl.size(); ++cnt)
    {
      mustBe0.add2LitClause(-act_vars[cnt], ctrl[cnt]);
      mustBe1.add2LitClause(-act_vars[cnt], -ctrl[cnt]);
    }

    // now we initialize the solvers:
    vector<int> vars_to_keep;
    vars_to_keep.reserve(ipc_.size() + act_vars.size());
    vars_to_keep.insert(vars_to_keep.end(), ipc_.begin(), ipc_.end());
    vars_to_keep.insert(vars_to_keep.end(), act_vars.begin(), act_vars.end());
    check_->startIncrementalSession(vars_to_keep, false);
    check_->incAddCNF(mustBe0);
    gen_->startIncrementalSession(vars_to_keep, false);
    gen_->incAddCNF(mustBe1);

    for(size_t ctrl_cnt = 0; ctrl_cnt < ctrl.size(); ++ctrl_cnt)
    {
      statistics.notifyBeforeCtrlSignal();
      int current_ctrl = ctrl[ctrl_cnt];
      vector<int> fixed(1, act_vars[ctrl_cnt]);

      CNF solution;
      while(impl[ctrl_cnt].getNrOfClauses() > 0)
      {
        vector<int> neg_clause = impl[ctrl_cnt].removeSmallest();
        Utils::negateLiterals(neg_clause);
        vector<int> smaller_cube;
        statistics.notifyBeforeClauseMin();

        if(coordinator_.extr_command_ == EXTR_STOP)
          return;

        bool sat = gen_->incIsSatModelOrCore(neg_clause, fixed, none, smaller_cube);
        statistics.notifyAfterClauseMin(neg_clause.size(), smaller_cube.size());
        MASSERT(!sat, "Impossible");
        solution.addNegCubeAsClause(smaller_cube);
        smaller_cube.push_back(act_vars[ctrl_cnt]);
        check_->incAddNegCubeAsClause(smaller_cube);
        statistics.notifyBeforeClauseComp();

        if(coordinator_.extr_command_ == EXTR_STOP)
          return;

        sat = check_->incIsSat(fixed);
        statistics.notifyAfterClauseComp();
        if(!sat)
          break;
      }

      if(coordinator_.extr_command_ == EXTR_STOP)
        return;

      addToStandAloneAiger2(current_ctrl, solution);
      c_eq_impl[ctrl_cnt] = makeEq(current_ctrl, solution);
      check_->incAddCNF(c_eq_impl[ctrl_cnt]);
      gen_->incAddCNF(c_eq_impl[ctrl_cnt]);
    }

    if(coordinator_.extr_command_ == EXTR_STOP)
      return;
    statistics.notifyBeforeABC(standalone_circuit2_->num_ands);
    aiger *opt = CNFImplExtractor::optimizeWithABC(standalone_circuit2_);
    aiger_reset(standalone_circuit2_);
    standalone_circuit2_ = opt;
    statistics.notifyAfterABC(standalone_circuit2_->num_ands);

    // swap:
    aiger *tmp = standalone_circuit_;
    standalone_circuit_ = standalone_circuit2_;
    standalone_circuit2_ = tmp;
    did_second_run_ = true;
  }

  size_t elapsed_until_done = Stopwatch::getRealTimeSec(start_time);
  waitForOthersToFinish(elapsed_until_done);
}

// -------------------------------------------------------------------------------------------
void ParExtractorWorker::runSATDep()
{
  PointInTime start_time = Stopwatch::start();

  const vector<int> &ctrl = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  typedef map<int, set<int> >::const_iterator MapConstIter;
  CNF trans = AIG2CNF::instance().getTrans();
  vector<int> none;

  CNF next_win(win_region_);
  next_win.swapPresentToNext();
  CNF leave_win(neg_win_region_);
  leave_win.swapPresentToNext();
  leave_win.addCNF(win_region_);

  vector<CNF> c_eq_impl;
  c_eq_impl.reserve(ctrl.size());
  vector<CNF> impl;
  impl.reserve(ctrl.size());

  vector<int> dep_vars(ipc_);
  map<int, set<int> > ctrl_dep_on;
  for(size_t ctrl_cnt = 0; ctrl_cnt < ctrl.size(); ++ctrl_cnt)
  {
    statistics.notifyBeforeCtrlSignal();
    int current_ctrl = ctrl[ctrl_cnt];
    Utils::remove(dep_vars, current_ctrl);
    const map<int, set<VarInfo> > &trans_deps = AIG2CNF::instance().getTmpDepsTrans();
    vector<int> real_dep_vars;
    real_dep_vars.reserve(dep_vars.size() + trans_deps.size() + ctrl_dep_on.size());
    real_dep_vars.insert(real_dep_vars.end(), dep_vars.begin(), dep_vars.end());
    // check out on which of the already computed ctrl signals we can depend:
    for(map<int, set<int> >::iterator it = ctrl_dep_on.begin(); it != ctrl_dep_on.end(); ++it)
      if(it->second.count(current_ctrl) == 0)
        real_dep_vars.push_back(it->first);
    // check out on which temporary variables of the transition relation we can depend:
    for(AIG2CNF::DepConstIter it = trans_deps.begin(); it != trans_deps.end(); ++it)
    {
      bool can_dep_on_this_tmp = true;
      for(set<VarInfo>::const_iterator i2 = it->second.begin(); i2 != it->second.end(); ++i2)
      {
        if(i2->getKind() == VarInfo::CTRL && !Utils::contains(real_dep_vars, i2->getLitInCNF()))
        {
          can_dep_on_this_tmp = false;
          break;
        }
      }
      if(can_dep_on_this_tmp)
        real_dep_vars.push_back(it->first);
    }

    // we need to rename all variables except for the dep_vars:
    int nr_of_vars = next_free_cnf_lit_;
    int local_next_free_var = nr_of_vars;
    vector<int> rename_map(nr_of_vars, 0);
    for(int cnt = 1; cnt < nr_of_vars; ++cnt)
      rename_map[cnt] = local_next_free_var++;
    for(size_t cnt = 0; cnt < real_dep_vars.size(); ++cnt)
      rename_map[real_dep_vars[cnt]] = real_dep_vars[cnt];

    CNF canBe1(trans);
    canBe1.setVarValue(current_ctrl, true);
    canBe1.addCNF(next_win);
    canBe1.renameVars(rename_map);
    CNF mustBe1(trans);
    mustBe1.setVarValue(current_ctrl, false);
    mustBe1.addCNF(leave_win);
    mustBe1.addCNF(canBe1);
    // just to be sure that the solution does not depend on our artificial state variable
    // (this could otherwise happen if we do not use minimal cores):
    mustBe1.setVarValue(VarManager::instance().getPresErrorStateVar(), false);

    CNF canBe0(trans);
    canBe0.setVarValue(current_ctrl, false);
    canBe0.addCNF(next_win);
    canBe0.renameVars(rename_map);
    CNF mustBe0(trans);
    mustBe0.setVarValue(current_ctrl, true);
    mustBe0.addCNF(leave_win);
    mustBe0.addCNF(canBe0);
    // just to be sure that the solution does not depend on our artificial state variable
    // (this could otherwise happen if we do not use minimal cores):
    mustBe0.setVarValue(VarManager::instance().getPresErrorStateVar(), false);

    // now we can do the actual learning loop:
    check_->startIncrementalSession(real_dep_vars, false);
    check_->incAddCNF(mustBe0);
    gen_->startIncrementalSession(real_dep_vars, false);
    gen_->incAddCNF(mustBe1);
    CNF solution;
    while(true)
    {

      if(coordinator_.extr_command_ == EXTR_STOP)
        return;

      vector<int> false_pos;
      // compute a false-positives (ctrl-signal is 1 but must not be):
      statistics.notifyBeforeClauseComp();
      bool false_pos_exists = check_->incIsSatModelOrCore(none, real_dep_vars, false_pos);

      statistics.notifyAfterClauseComp();
      if(!false_pos_exists)
        break;

      if(coordinator_.extr_command_ == EXTR_STOP)
        return;

      // generalize the false-positives:
      // (compute a larger set of situations (including the false-positive) for which
      // setting the control signal to 0 is allowed:
      vector<int> gen_false_pos;
      statistics.notifyBeforeClauseMin();
      Utils::randomize(false_pos);
      bool sat = gen_->incIsSatModelOrCore(false_pos, none, gen_false_pos);
      MASSERT(!sat, "Impossible.");
      statistics.notifyAfterClauseMin(false_pos.size(), gen_false_pos.size());
      solution.addNegCubeAsClause(gen_false_pos);
      check_->incAddNegCubeAsClause(gen_false_pos);
    }

    if(coordinator_.extr_command_ == EXTR_STOP)
      return;

    // dump solution in AIGER format:
    addToStandAloneAiger(current_ctrl, solution);
    // store solution for future optimization:
    impl.push_back(solution);
    // re-substitution:
    CNF resub = makeEq(current_ctrl, solution);
    trans.addCNF(resub);
    c_eq_impl.push_back(resub);

    // update the dependencies:
    // 1: find out on which of the remaining ctrl-signals current_ctrl depends on
    // 1a: other control signals:
    set<int> ctrl_dep;
    for(size_t cnt = 0; cnt < ctrl.size(); ++cnt)
      if(solution.contains(ctrl[cnt]))
        ctrl_dep.insert(ctrl[cnt]);
    // 1b: temporary signals which, in turn, may depend on ctrl-signals:
    for(AIG2CNF::DepConstIter it = trans_deps.begin(); it != trans_deps.end(); ++it)
      if(solution.contains(it->first))
        for(set<VarInfo>::const_iterator i2 = it->second.begin(); i2 != it->second.end(); ++i2)
          if(i2->getKind() == VarInfo::CTRL)
            ctrl_dep.insert(i2->getLitInCNF());
    // 1c: some ctrl-signals may already be defined; we need to take their deps:
    for(MapConstIter it = ctrl_dep_on.begin(); it != ctrl_dep_on.end(); ++it)
    {
      if(ctrl_dep.count(it->first))
      {
        ctrl_dep.erase(it->first);
        ctrl_dep.insert(it->second.begin(), it->second.end());
      }
    }
    // 2: other defined ctrl-signals may depend on current_ctrl; we need to update their deps:
    for(map<int, set<int> >::iterator it = ctrl_dep_on.begin(); it != ctrl_dep_on.end(); ++it)
    {
      if(it->second.count(current_ctrl))
      {
        it->second.erase(current_ctrl);
        it->second.insert(ctrl_dep.begin(), ctrl_dep.end());
      }
    }
    ctrl_dep_on[current_ctrl] = ctrl_dep;
    // 3: check for cyclic dependencies (just for debugging purposes)
    for(MapConstIter it = ctrl_dep_on.begin(); it != ctrl_dep_on.end(); ++it)
    {
      DASSERT(it->second.count(it->first) == 0, "Cyclic dependency");
    }
    statistics.notifyAfterCtrlSignal();
  }

  if(coordinator_.extr_command_ == EXTR_STOP)
    return;
  // finally, we have to copy AND gates from the transition relation:
  insertMissingAndFromTrans(standalone_circuit_);
  if(coordinator_.extr_command_ == EXTR_STOP)
    return;

  // and do the optimization with ABC
  statistics.notifyBeforeABC(standalone_circuit_->num_ands);
  aiger *opt = CNFImplExtractor::optimizeWithABC(standalone_circuit_);
  aiger_reset(standalone_circuit_);
  standalone_circuit_ = opt;
  statistics.notifyAfterABC(standalone_circuit_->num_ands);
  done_ = true;

  // Now we are done in principle. Let's now decide if we have time for a second minimization
  // round:
  bool second_run = true;
  if(standalone_circuit_->num_ands <= 1)
    second_run = false;
  size_t elapsed_in_first_round = Stopwatch::getRealTimeSec(start_time);
  size_t remaining = Options::instance().getEstimatedTimeRemaining();
  if(remaining == 0 || remaining <  10 * elapsed_in_first_round)
    second_run = false;

  if(second_run)
  {
    // Now the second step: try to reduce the implementations further
    gen_->doMinCores(true);
    CNF neg_rel(leave_win);
    neg_rel.addCNF(AIG2CNF::instance().getTrans());
    neg_rel.setVarValue(VarManager::instance().getPresErrorStateVar(), false);

    for(size_t ctrl_cnt = 0; ctrl_cnt < ctrl.size(); ++ctrl_cnt)
    {
      statistics.notifyBeforeCtrlSignal();
      int current_ctrl = ctrl[ctrl_cnt];
      CNF neg_rel_fixed(neg_rel);
      for(size_t ctrl_cnt2 = 0; ctrl_cnt2 < ctrl.size(); ++ctrl_cnt2)
        if(ctrl_cnt2 != ctrl_cnt)
          neg_rel_fixed.addCNF(c_eq_impl[ctrl_cnt2]);
      dep_vars = impl[ctrl_cnt].getVars();
      check_->startIncrementalSession(dep_vars, false);
      check_->incAddCNF(neg_rel_fixed);
      check_->incAddUnitClause(current_ctrl);
      gen_->startIncrementalSession(dep_vars, false);
      gen_->incAddCNF(neg_rel_fixed);
      gen_->incAddUnitClause(-current_ctrl);

      CNF solution;
      while(impl[ctrl_cnt].getNrOfClauses() > 0)
      {
        vector<int> neg_clause = impl[ctrl_cnt].removeSmallest();
        Utils::negateLiterals(neg_clause);
        vector<int> smaller_cube;

        if(coordinator_.extr_command_ == EXTR_STOP)
          return;

        statistics.notifyBeforeClauseMin();
        bool sat = gen_->incIsSatModelOrCore(neg_clause, none, smaller_cube);
        statistics.notifyAfterClauseMin(neg_clause.size(), smaller_cube.size());
        MASSERT(!sat, "Impossible");
        solution.addNegCubeAsClause(smaller_cube);

        if(coordinator_.extr_command_ == EXTR_STOP)
          return;

        check_->incAddNegCubeAsClause(smaller_cube);
        statistics.notifyBeforeClauseComp();
        sat = check_->incIsSat();
        statistics.notifyAfterClauseComp();
        if(!sat)
          break;
      }
      if(coordinator_.extr_command_ == EXTR_STOP)
        return;
      impl[ctrl_cnt] = solution;
      addToStandAloneAiger2(current_ctrl, solution);
      c_eq_impl[ctrl_cnt] = makeEq(current_ctrl, solution);
      statistics.notifyAfterCtrlSignal();
    }

    if(coordinator_.extr_command_ == EXTR_STOP)
      return;
    // finally, we have to copy AND gates from the transition relation:
    insertMissingAndFromTrans(standalone_circuit2_);

    if(coordinator_.extr_command_ == EXTR_STOP)
      return;
    statistics.notifyBeforeABC(standalone_circuit2_->num_ands);
    aiger *opt = CNFImplExtractor::optimizeWithABC(standalone_circuit2_);
    aiger_reset(standalone_circuit2_);
    standalone_circuit2_ = opt;
    statistics.notifyAfterABC(standalone_circuit2_->num_ands);

    // swap:
    aiger *tmp = standalone_circuit_;
    standalone_circuit_ = standalone_circuit2_;
    standalone_circuit2_ = tmp;
    did_second_run_ = true;

  }
  size_t elapsed_until_done = Stopwatch::getRealTimeSec(start_time);
  waitForOthersToFinish(elapsed_until_done);
}

// -------------------------------------------------------------------------------------------
void ParExtractorWorker::waitForOthersToFinish(size_t elapsed_until_done)
{
  waiting_ = true;
  size_t remaining_time = Options::instance().getEstimatedTimeRemaining();
  if(standalone_circuit_->num_ands <= 1 || remaining_time < 5)
  {
    coordinator_.killThemAll();
    return;
  }
  PointInTime wait_start = Stopwatch::start();
  while(true)
  {
    usleep(200000); // microseconds
    if(coordinator_.extr_command_ == EXTR_STOP)
      return;
    if(coordinator_.allWaiting())
    {
      coordinator_.killThemAll();
      return;
    }
    size_t elapsed_wait = Stopwatch::getRealTimeSec(wait_start);
    // if we have already waited 25% of our working time, it is time to kill:
    if(elapsed_wait + elapsed_wait + elapsed_wait + elapsed_wait > elapsed_until_done + 10)
    {
      coordinator_.killThemAll();
      return;
    }
    // if we have already waited 25% of the time until the timeout, we also kill:
    if(elapsed_wait + elapsed_wait + elapsed_wait + elapsed_wait > remaining_time)
    {
      coordinator_.killThemAll();
      return;
    }
  }
}

// -------------------------------------------------------------------------------------------
CNF ParExtractorWorker::makeEq(int var, CNF impl, vector<int> *new_temps)
{
  CNF res;
  // re-substitution:
  // var -> impl
  const list<vector<int> > &impl_clauses = impl.getClauses();
  for(CNF::ClauseConstIter it = impl_clauses.begin(); it != impl_clauses.end(); ++it)
  {
    vector<int> clause(*it);
    clause.push_back(-var);
    res.addClause(clause);
  }
  // impl -> var
  // begin negation of impl
  const list<vector<int> > &original = impl.getClauses();
  list<vector<int> > neg;
  vector<int> one_clause_false;
  one_clause_false.reserve(original.size() + 1);
  for(CNF::ClauseConstIter it = original.begin(); it != original.end(); ++it)
  {
    if(it->size() == 1)
      one_clause_false.push_back(-(*it)[0]);
    else
    {
      int clause_false_lit = next_free_cnf_lit_++;
      if(new_temps != NULL)
        new_temps->push_back(clause_false_lit);
      one_clause_false.push_back(clause_false_lit);
      for(size_t lit_cnt = 0; lit_cnt < it->size(); ++lit_cnt)
      {
        vector<int> twolitclause(2,0);
        twolitclause[0] = -clause_false_lit;
        twolitclause[1] = -((*it)[lit_cnt]);
        neg.push_back(twolitclause);
      }
    }
  }
  // here comes the implication:
  one_clause_false.push_back(var);
  neg.push_back(one_clause_false);
  // end negation of impl

  CNF neg_impl;
  neg_impl.swapWith(neg);
  res.addCNF(neg_impl);

  return res;
}

// -------------------------------------------------------------------------------------------
void ParExtractorWorker::addToStandAloneAiger(int ctrl_var, const CNF &solution)
{
  const list<vector<int> > &solution_clauses = solution.getClauses();

  // if we do not have any clauses, the result is true:
  if(solution_clauses.size() == 0)
  {
    aiger_add_and(standalone_circuit_, cnfToAig(ctrl_var), 1, 1);
    return;
  }

  // if one clause is empty, the result is false:
  for(CNF::ClauseConstIter it = solution_clauses.begin(); it != solution_clauses.end(); ++it)
  {
    if(it->size() == 0)
    {
      aiger_add_and(standalone_circuit_, cnfToAig(ctrl_var), 0, 0);
      return;
    }
  }

  int and_res = 0;
  for(CNF::ClauseConstIter it = solution_clauses.begin(); it != solution_clauses.end(); ++it)
  {
    int or_res = cnfToAig((*it)[0]);
    for(size_t lit_cnt = 1; lit_cnt < it->size(); ++lit_cnt)
      or_res = makeOr(or_res, cnfToAig((*it)[lit_cnt]));
    if(it == solution_clauses.begin())
      and_res = or_res;
    else
      and_res = makeAnd(and_res, or_res);
  }
  aiger_add_and(standalone_circuit_, cnfToAig(ctrl_var), and_res, 1);
}

// -------------------------------------------------------------------------------------------
void ParExtractorWorker::addToStandAloneAiger2(int ctrl_var, const CNF &solution)
{
  const list<vector<int> > &solution_clauses = solution.getClauses();

  // if we do not have any clauses, the result is true:
  if(solution_clauses.size() == 0)
  {
    aiger_add_and(standalone_circuit2_, cnfToAig(ctrl_var), 1, 1);
    return;
  }

  // if one clause is empty, the result is false:
  for(CNF::ClauseConstIter it = solution_clauses.begin(); it != solution_clauses.end(); ++it)
  {
    if(it->size() == 0)
    {
      aiger_add_and(standalone_circuit2_, cnfToAig(ctrl_var), 0, 0);
      return;
    }
  }

  int and_res = 0;
  for(CNF::ClauseConstIter it = solution_clauses.begin(); it != solution_clauses.end(); ++it)
  {
    int or_res = cnfToAig((*it)[0]);
    for(size_t lit_cnt = 1; lit_cnt < it->size(); ++lit_cnt)
      or_res = makeOr2(or_res, cnfToAig((*it)[lit_cnt]));
    if(it == solution_clauses.begin())
      and_res = or_res;
    else
      and_res = makeAnd2(and_res, or_res);
  }
  aiger_add_and(standalone_circuit2_, cnfToAig(ctrl_var), and_res, 1);
}

// -------------------------------------------------------------------------------------------
int ParExtractorWorker::cnfToAig(int cnf_lit)
{
  int cnf_var = (cnf_lit < 0) ? -cnf_lit : cnf_lit;
  if(cnf_lit < 0)
    return aiger_not(cnf_var_to_standalone_aig_var_[cnf_var]);
  else
    return cnf_var_to_standalone_aig_var_[cnf_var];
}

// -------------------------------------------------------------------------------------------
int ParExtractorWorker::makeAnd(int in1, int in2)
{
  int res = next_free_aig_lit_;
  aiger_add_and(standalone_circuit_, next_free_aig_lit_, in1, in2);
  next_free_aig_lit_ += 2;
  return res;
}

// -------------------------------------------------------------------------------------------
int ParExtractorWorker::makeAnd2(int in1, int in2)
{
  int res = next_free_aig_lit2_;
  aiger_add_and(standalone_circuit2_, next_free_aig_lit2_, in1, in2);
  next_free_aig_lit2_ += 2;
  return res;
}

// -------------------------------------------------------------------------------------------
int ParExtractorWorker::makeOr(int in1, int in2)
{
  int res = aiger_not(next_free_aig_lit_);
  aiger_add_and(standalone_circuit_, next_free_aig_lit_, aiger_not(in1), aiger_not(in2));
  next_free_aig_lit_ += 2;
  return res;
}

// -------------------------------------------------------------------------------------------
int ParExtractorWorker::makeOr2(int in1, int in2)
{
  int res = aiger_not(next_free_aig_lit2_);
  aiger_add_and(standalone_circuit2_, next_free_aig_lit2_, aiger_not(in1), aiger_not(in2));
  next_free_aig_lit2_ += 2;
  return res;
}

// -------------------------------------------------------------------------------------------
void ParExtractorWorker::insertMissingAndFromTrans(aiger *standalone_circuit)
{
  VarManager &VM = VarManager::instance();

  // Step 1: find out which variables are not yet defined in the standalone_circuit_:
  set<int> ref_aiger_vars;
  for(unsigned and_cnt = 0; and_cnt < standalone_circuit->num_ands; ++and_cnt)
  {
    ref_aiger_vars.insert(aiger_strip(standalone_circuit->ands[and_cnt].rhs0));
    ref_aiger_vars.insert(aiger_strip(standalone_circuit->ands[and_cnt].rhs1));
  }
  set<int> miss_cnf_vars;
  list<int> analyze_further_queue;
  const map<int, set<VarInfo> > &deps = AIG2CNF::instance().getTmpDeps();
  for(AIG2CNF::DepConstIter it = deps.begin(); it != deps.end(); ++it)
  {
    if(ref_aiger_vars.count(cnf_var_to_standalone_aig_var_[it->first]))
    {
      miss_cnf_vars.insert(it->first);
      analyze_further_queue.push_back(it->first);
    }
  }
  // Step 2: build the transitive closure of the missing variables:
  while(!analyze_further_queue.empty())
  {
    int analyze = analyze_further_queue.front();
    analyze_further_queue.pop_front();
    AIG2CNF::DepConstIter d = deps.find(analyze);
    MASSERT(d != deps.end(), "Impossible.");
    for(set<VarInfo>::const_iterator it = d->second.begin(); it != d->second.end(); ++it)
    {
      if(it->getKind() == VarInfo::TMP && miss_cnf_vars.count(it->getLitInCNF()) == 0)
      {
        miss_cnf_vars.insert(it->getLitInCNF());
        analyze_further_queue.push_back(it->getLitInCNF());
      }
    }
  }

  // Step 3: add missing variables by copying them from the transition relation
  const char *error = NULL;
  aiger *aig = aiger_init();
  const string &file = Options::instance().getAigInFileName();
  error = aiger_open_and_read_from_file (aig, file.c_str());
  MASSERT(error == NULL, "Could not open AIGER file " << file << " (" << error << ").");
  for(unsigned and_cnt = 0; and_cnt < aig->num_ands; ++and_cnt)
  {
    int lhs_cnf = VM.aigLitToCnfLit(aig->ands[and_cnt].lhs);
    if(miss_cnf_vars.count(lhs_cnf))
    {
      unsigned r0 = cnfToAig(VM.aigLitToCnfLit(aig->ands[and_cnt].rhs0));
      unsigned r1 = cnfToAig(VM.aigLitToCnfLit(aig->ands[and_cnt].rhs1));
      aiger_add_and(standalone_circuit, cnfToAig(lhs_cnf), r0, r1);
    }
  }
  aiger_reset(aig);
}

// -------------------------------------------------------------------------------------------
ParExtractorQBFWorker::ParExtractorQBFWorker(ParExtractor &coordinator,
                                             const CNF &win_region,
                                             const CNF &neg_win_region) :
  ParExtractorWorker(coordinator, win_region, neg_win_region),
  check_solver_(new DepQBFApi()),
  gen_solver_(new DepQBFApi())
{

}

// ----------------------------------------------------------------------------------------
ParExtractorQBFWorker::~ParExtractorQBFWorker()
{
  delete check_solver_;
  check_solver_ = NULL;
  delete gen_solver_;
  gen_solver_ = NULL;
}




// -------------------------------------------------------------------------------------------
void* ParExtractorQBFWorker::startQBF(void *object)
{
  reinterpret_cast<ParExtractorQBFWorker *>(object)->runQBF();
  return NULL;
}

// -------------------------------------------------------------------------------------------
void ParExtractorQBFWorker::runQBF()
{
  int old_state;
  PointInTime start_time = Stopwatch::start();

  const vector<int> &ctrl = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  const vector<int> &next = VarManager::instance().getVarsOfType(VarInfo::NEXT_STATE);
  tmp_ = VarManager::instance().getVarsOfType(VarInfo::TMP);

  neg_rel_ = neg_win_region_;
  neg_rel_.swapPresentToNext();
  neg_rel_.addCNF(win_region_);
  neg_rel_.addCNF(AIG2CNF::instance().getTrans());

  // DepQBF can be very memory hungry. In the parallel mode we have to be careful here:
  // if one thread goes out of memory, everything dies. So let's have a safety net here:
  if(neg_rel_.getNrOfLits() > 100000)
  {
    waiting_ = true;
    return;
  }

  // neg_rel will have a clause that sets the error state variable to false (it must be
  // part of win_region. However, we also replace it by false so that this variable is gone
  // from the CNF. This makes one state variable less to consider:
  neg_rel_.setVarValue(VarManager::instance().getPresErrorStateVar(), false);

  ctrl_univ_ = ctrl;
  ctrl_exist_.clear();
  ctrl_exist_.reserve(ctrl.size());
  for(size_t ctrl_cnt = 0; ctrl_cnt < ctrl.size(); ++ctrl_cnt)
  {
    statistics.notifyBeforeCtrlSignal();
    int current_ctrl = ctrl[ctrl_cnt];
    Utils::remove(ctrl_univ_, current_ctrl);
    ctrl_exist_.push_back(current_ctrl);

    // Build the quantifier prefix:
    quant_.clear();
    quant_.push_back(make_pair(ip_, QBFSolver::E));
    quant_.push_back(make_pair(ctrl_univ_, QBFSolver::A));
    quant_.push_back(make_pair(ctrl_exist_, QBFSolver::E));
    quant_.push_back(make_pair(next, QBFSolver::E));
    quant_.push_back(make_pair(tmp_, QBFSolver::E));

    // Build the CNF for computing false-positives (ctrl-signal is 1 but must be 0):
    check_ = neg_rel_;
    check_.setVarValue(current_ctrl, true);
    check_solver_->startIncrementalSession(quant_);
    check_solver_->incAddCNF(check_);

    // Build the CNF for generalizing false-positives (ctrl-signal can be 0)
    gen_ = neg_rel_;
    gen_.setVarValue(current_ctrl, false);
    gen_solver_->startIncrementalSession(quant_);
    gen_solver_->incAddCNF(gen_);

    // do the learning loop:
    solution_.clear();
    while(true)
    {
      if(coordinator_.extr_command_ == EXTR_STOP)
        return;

      // compute a false-positives (ctrl-signal is 1 but must not be):
      false_pos_.clear();
      statistics.notifyBeforeClauseComp();
      pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
      pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old_state);
      bool false_pos_exists = check_solver_->incIsSatModel(none_, false_pos_);
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);
      pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &old_state);
      statistics.notifyAfterClauseComp();
      if(!false_pos_exists)
        break;

      if(coordinator_.extr_command_ == EXTR_STOP)
        return;
      // generalize the false-positives:
      // (compute a larger set of situations (including the false-positive) for which
      // setting the control signal to 0 is allowed:
      statistics.notifyBeforeClauseMin();
      gen_false_pos_.clear();
      pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
      pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old_state);
      bool sat = gen_solver_->incIsSatCore(false_pos_, gen_false_pos_);
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);
      pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &old_state);
      MASSERT(!sat, "Impossible");
      statistics.notifyAfterClauseMin(false_pos_.size(), gen_false_pos_.size());
      check_solver_->incAddNegCubeAsClause(gen_false_pos_);
      solution_.addNegCubeAsClause(gen_false_pos_);
    }

    if(coordinator_.extr_command_ == EXTR_STOP)
      return;

    // dump solution in AIGER format:
    addToStandAloneAiger(current_ctrl, solution_);

    if(coordinator_.extr_command_ == EXTR_STOP)
      return;

    // re-substitution:
    neg_rel_.addCNF(makeEq(current_ctrl, solution_, &tmp_));
    statistics.notifyAfterCtrlSignal();
  }

  if(coordinator_.extr_command_ == EXTR_STOP)
    return;

  statistics.notifyBeforeABC(standalone_circuit_->num_ands);
  aiger *opt = CNFImplExtractor::optimizeWithABC(standalone_circuit_);
  aiger_reset(standalone_circuit_);
  standalone_circuit_ = opt;
  statistics.notifyAfterABC(standalone_circuit_->num_ands);
  done_ = true;

  size_t elapsed_until_done = Stopwatch::getRealTimeSec(start_time);
  waitForOthersToFinish(elapsed_until_done);

}

