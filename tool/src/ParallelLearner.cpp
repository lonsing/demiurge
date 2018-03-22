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
//   <http://www.iaik.tugraz.at/content/research/design_verification/demiurge/>
// or email the authors directly.
//
// ----------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------
/// @file ParallelLearner.cpp
/// @brief Contains the definition of many classes, all related to a parallelization
/// @todo Split this file into several files, one per class, and combine them in a namespace.
// -------------------------------------------------------------------------------------------

#include "ParallelLearner.h"
#include "AIG2CNF.h"
#include "VarManager.h"
#include "Logger.h"
#include "Utils.h"
#include "Options.h"
#include "LingelingApi.h"
#include "MiniSatApi.h"
#include "PicoSatApi.h"
#include "QBFCertImplExtractor.h"
#include "QBFSolver.h"
#include "unistd.h"
#include "IFMProofObligation.h"

// -------------------------------------------------------------------------------------------
///
/// @def UNKNOWN
/// @brief A constant for the realizability verdict 'not yet known'.
#define UNKNOWN 0

// -------------------------------------------------------------------------------------------
///
/// @def REALIZABLE
/// @brief A constant for the realizability verdict 'realizable'.
#define REALIZABLE 1

// -------------------------------------------------------------------------------------------
///
/// @def UNREALIZABLE
/// @brief A constant for the realizability verdict 'unrealizable'.
#define UNREALIZABLE 2

// -------------------------------------------------------------------------------------------
///
/// @def IS_LOSE
/// @brief A constant for the result: 'the state is losing'.
#define IS_LOSE false

// -------------------------------------------------------------------------------------------
///
/// @def IS_GREATER
/// @brief A constant for the result: 'the rank of the state is greater'.
#define IS_GREATER true

// -------------------------------------------------------------------------------------------
///
/// @def EXPL
/// @brief A constant for the source info: 'comes from a ClauseExplorerSAT instance'.
#define EXPL 0

// -------------------------------------------------------------------------------------------
///
/// @def CE_GEN
/// @brief A constant for the source info: 'comes from a CounterGenSAT instance'.
#define CE_GEN 1

// -------------------------------------------------------------------------------------------
///
/// @def MIN
/// @brief A constant for the source info: 'comes from a ClauseMinimizerQBF instance'.
#define MIN 2

// -------------------------------------------------------------------------------------------
///
/// @def IFM
/// @brief A constant for the source info: 'comes from a IFM13Explorer instance'.
#define IFM 3


mutex ParallelLearner::print_lock_;

// -------------------------------------------------------------------------------------------
///
/// @def PLOG(message)
/// @brief Prints a debug message (protected by a lock).
///
/// @param message The message to print.
#define PLOG(message)                                                     \
{                                                                         \
  ParallelLearner::print_lock_.lock();                                    \
  cout << "[DBG] " << message << endl;                                    \
  ParallelLearner::print_lock_.unlock();                                  \
}

// -------------------------------------------------------------------------------------------
ParallelLearner::ParallelLearner(size_t nr_of_threads) :
                 BackEnd(),
                 result_(0),
                 nr_of_threads_(nr_of_threads)
{
  use_ind_ = (Options::instance().getBackEndMode() == 1);

  MASSERT(nr_of_threads != 0, "Must have at least one thread");
  size_t nr_of_clause_explorers = nr_of_threads;
  size_t nr_of_ifm_explorers = 0;
  size_t nr_of_clause_minimizers = 0;
  size_t nr_of_ce_generalizers = 0;

  if(nr_of_threads == 1)
  {
    nr_of_clause_explorers = 1;
    nr_of_ifm_explorers = 0;
    nr_of_clause_minimizers = 0;
    nr_of_ce_generalizers = 0;
  }
  if(nr_of_threads == 2)
  {
    nr_of_clause_explorers = 2;
    nr_of_ifm_explorers = 0;
    nr_of_clause_minimizers = 0;
    nr_of_ce_generalizers = 0;
  }
  if(nr_of_threads == 3)
  {
    nr_of_clause_explorers = 2;
    nr_of_ifm_explorers = 0;
    nr_of_clause_minimizers = 0;
    nr_of_ce_generalizers = 1;
  }
  if(nr_of_threads == 4)
  {
    nr_of_clause_explorers = 2;
    nr_of_ifm_explorers = 0;
    nr_of_clause_minimizers = 1;
    nr_of_ce_generalizers = 1;
  }
  if(nr_of_threads == 5)
  {
    nr_of_clause_explorers = 3;
    nr_of_ifm_explorers = 0;
    nr_of_clause_minimizers = 1;
    nr_of_ce_generalizers = 1;
  }
  if(nr_of_threads == 6)
  {
    nr_of_clause_explorers = 3;
    nr_of_ifm_explorers = 1;
    nr_of_clause_minimizers = 1;
    nr_of_ce_generalizers = 1;
  }
  if(nr_of_threads == 7)
  {
    nr_of_clause_explorers = 3;
    nr_of_ifm_explorers = 1;
    nr_of_clause_minimizers = 1;
    nr_of_ce_generalizers = 2;
  }
  if(nr_of_threads == 8)
  {
    nr_of_clause_explorers = 4;
    nr_of_ifm_explorers = 1;
    nr_of_clause_minimizers = 1;
    nr_of_ce_generalizers = 2;
  }

  if(use_ind_)
    computePreviousTrans();

  clause_explorers_.reserve(nr_of_clause_explorers);
  for(size_t cnt = 0; cnt < nr_of_clause_explorers; ++cnt)
  {
    if(use_ind_)
      clause_explorers_.push_back(new ClauseExplorerSAT(cnt,
                                                        *this,
                                                        prev_trans_or_initial_,
                                                        current_to_previous_map_,
                                                        current_state_is_initial_));
    else
      clause_explorers_.push_back(new ClauseExplorerSAT(cnt, *this));
  }

  ce_generalizers_.reserve(nr_of_ce_generalizers);
  for(size_t cnt = 0; cnt < nr_of_ce_generalizers; ++cnt)
  {
    if(use_ind_)
      ce_generalizers_.push_back(new CounterGenSAT(*this,
                                                   prev_trans_or_initial_,
                                                   current_to_previous_map_,
                                                   current_state_is_initial_));
    else
      ce_generalizers_.push_back(new CounterGenSAT(*this));
  }

  ifm_explorers_.reserve(nr_of_ifm_explorers);
  for(size_t cnt = 0; cnt < nr_of_ifm_explorers; ++cnt)
    ifm_explorers_.push_back(new IFM13Explorer(*this));

  clause_minimizers_.reserve(nr_of_clause_minimizers);
  for(size_t cnt = 0; cnt < nr_of_clause_minimizers; ++cnt)
    clause_minimizers_.push_back(new ClauseMinimizerQBF(*this));

}

// -------------------------------------------------------------------------------------------
ParallelLearner::~ParallelLearner()
{
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    delete clause_explorers_[cnt];
  clause_explorers_.clear();

  for(size_t cnt = 0; cnt < ce_generalizers_.size(); ++cnt)
    delete ce_generalizers_[cnt];
  ce_generalizers_.clear();

  for(size_t cnt = 0; cnt < ifm_explorers_.size(); ++cnt)
    delete ifm_explorers_[cnt];
  ifm_explorers_.clear();

  for(size_t cnt = 0; cnt < clause_minimizers_.size(); ++cnt)
    delete clause_minimizers_[cnt];
  clause_minimizers_.clear();
}

// -------------------------------------------------------------------------------------------
bool ParallelLearner::run()
{
  VarManager::instance().push();
  L_INF("Starting to compute a winning region using " << nr_of_threads_ << " threads ...");
  statistics_.notifyWinRegStart();
  winning_region_.clear();
  winning_region_.addCNF(AIG2CNF::instance().getSafeStates());

  // Start the threads:
  vector<thread> explorer_threads;
  explorer_threads.reserve(clause_explorers_.size() - 1);
  for(size_t cnt = 1; cnt < clause_explorers_.size(); ++cnt)
    explorer_threads.push_back(thread(&ClauseExplorerSAT::exploreClauses,
                                      clause_explorers_[cnt]));

  vector<thread> gen_threads;
  gen_threads.reserve(ce_generalizers_.size());
  for(size_t cnt = 0; cnt < ce_generalizers_.size(); ++cnt)
    gen_threads.push_back(thread(&CounterGenSAT::generalizeCounterexamples,
                                 ce_generalizers_[cnt]));

  vector<thread> minimizer_threads;
  minimizer_threads.reserve(clause_minimizers_.size());
  for(size_t cnt = 0; cnt < clause_minimizers_.size(); ++cnt)
    minimizer_threads.push_back(thread(&ClauseMinimizerQBF::minimizeClauses,
                                       clause_minimizers_[cnt]));

  vector<thread> ifm_threads;
  ifm_threads.reserve(ifm_explorers_.size());
  for(size_t cnt = 0; cnt < ifm_explorers_.size(); ++cnt)
    ifm_threads.push_back(thread(&IFM13Explorer::exploreClauses,
                                 ifm_explorers_[cnt]));

  //The main thread executes the first explorer:
  MASSERT(clause_explorers_.size() > 0, "There must be at least one explorer thread");
  clause_explorers_[0]->exploreClauses();

  // Wait until the threads are finished:
  for(size_t cnt = 0; cnt < explorer_threads.size(); ++cnt)
    explorer_threads[cnt].join();
  for(size_t cnt = 0; cnt < gen_threads.size(); ++cnt)
    gen_threads[cnt].join();
  for(size_t cnt = 0; cnt < minimizer_threads.size(); ++cnt)
    minimizer_threads[cnt].join();
  for(size_t cnt = 0; cnt < ifm_threads.size(); ++cnt)
    ifm_threads[cnt].join();

  // Merge statistics:
  statistics_.notifyWinRegEnd();
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    statistics_.mergeWith(clause_explorers_[cnt]->getStatistics());
  for(size_t cnt = 0; cnt < ce_generalizers_.size(); ++cnt)
    statistics_.mergeWith(ce_generalizers_[cnt]->getStatistics());

  // Extract a circuit:
  if(result_ == UNREALIZABLE)
  {
    L_RES("The specification is unrealizable.");
    statistics_.logStatistics();
    return false;
  }
  L_RES("The specification is realizable.");
  Utils::debugCheckWinReg(winning_region_);
  if(Options::instance().doRealizabilityOnly())
  {
    statistics_.logStatistics();
    return true;
  }

  L_INF("Starting to extract a circuit ...");
  statistics_.notifyRelDetStart();
  QBFCertImplExtractor extractor;
  extractor.extractCircuit(winning_region_);
  statistics_.notifyRelDetEnd();
  L_INF("Synthesis done.");
  statistics_.logStatistics();
  return true;

}

// -------------------------------------------------------------------------------------------
void ParallelLearner::notifyNewWinRegClause(const vector<int> &clause, int src)
{

  // we notify the clause_explorers_ synchronously:
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    clause_explorers_[cnt]->notifyBeforeNewInfo();
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    clause_explorers_[cnt]->notifyNewWinRegClause(clause, src);
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    clause_explorers_[cnt]->notifyAfterNewInfo();

  winning_region_lock_.lock();
  winning_region_.addClauseAndSimplify(clause);
  winning_region_lock_.unlock();

  for(size_t cnt = 0; cnt < ce_generalizers_.size(); ++cnt)
    ce_generalizers_[cnt]->notifyNewWinRegClause(clause, src);

  for(size_t cnt = 0; cnt < ifm_explorers_.size(); ++cnt)
    ifm_explorers_[cnt]->notifyNewWinRegClause(clause, src);

  if(clause_minimizers_.size() > 0 && src != MIN)
  {
    unminimized_clauses_lock_.lock();
    unminimized_clauses_.addClause(clause);
    unminimized_clauses_lock_.unlock();
  }
}

// -------------------------------------------------------------------------------------------
void ParallelLearner::notifyNewUselessInputClause(const vector<int> &clause, int level)
{
  // we notify the clause_explorers_ synchronously:
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    clause_explorers_[cnt]->notifyBeforeNewInfo();
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    clause_explorers_[cnt]->notifyNewUselessInputClause(clause, level);
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    clause_explorers_[cnt]->notifyAfterNewInfo();
}

// -------------------------------------------------------------------------------------------
void ParallelLearner::notifyNewCounterexample(const vector<int> &ce, const vector<int> &gen)
{
  if(ce_generalizers_.size() > 0)
  {
    counterexamples_lock_.lock();
    counterexamples_.push_back(make_pair(ce, gen));
    counterexamples_lock_.unlock();
  }
}

// -------------------------------------------------------------------------------------------
void ParallelLearner::triggerExplorerRestart()
{
  var_man_lock_.lock();
  winning_region_lock_.lock();

  VarManager::instance().resetToLastPush();
  Utils::compressStateCNF(winning_region_);
  CNF leave_win(winning_region_);
  leave_win.swapPresentToNext();
  leave_win.negate();
  leave_win.addCNF(AIG2CNF::instance().getTrans());
  leave_win.addCNF(winning_region_);

  // we notify the clause_explorers_ synchronously:
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    clause_explorers_[cnt]->notifyBeforeNewInfo();
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    clause_explorers_[cnt]->notifyRestart(leave_win);
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    clause_explorers_[cnt]->notifyAfterNewInfo();

  winning_region_lock_.unlock();
  var_man_lock_.unlock();
}

// -------------------------------------------------------------------------------------------
void ParallelLearner::computePreviousTrans()
{
  // build previous-state copy of the transition relation:
  AIG2CNF& A2C = AIG2CNF::instance();
  VarManager &VM = VarManager::instance();
  current_to_previous_map_.resize(VM.getMaxCNFVar()+1, 0);
  const vector<int> &s = VM.getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &s_next = VM.getVarsOfType(VarInfo::NEXT_STATE);
  const vector<int> &c = VM.getVarsOfType(VarInfo::CTRL);
  const vector<int> &i = VM.getVarsOfType(VarInfo::INPUT);
  vector<int> t = VM.getVarsOfType(VarInfo::TMP);

  for(size_t v_cnt = 0; v_cnt < s.size(); ++v_cnt)
    current_to_previous_map_[s[v_cnt]] = VM.createFreshPrevVar();
  for(size_t v_cnt = 0; v_cnt < c.size(); ++v_cnt)
    current_to_previous_map_[c[v_cnt]] = VM.createFreshPrevVar();
  for(size_t v_cnt = 0; v_cnt < i.size(); ++v_cnt)
    current_to_previous_map_[i[v_cnt]] = VM.createFreshPrevVar();
  for(size_t v_cnt = 0; v_cnt < t.size(); ++v_cnt)
    current_to_previous_map_[t[v_cnt]] = VM.createFreshTmpVar();
  for(size_t v_cnt = 0; v_cnt < s_next.size(); ++v_cnt)
    current_to_previous_map_[s_next[v_cnt]] = s[v_cnt];

  current_state_is_initial_ = VM.createFreshTmpVar();
  list<vector<int> > prev_trans_clauses = A2C.getTrans().getClauses();
  for(CNF::ClauseIter it = prev_trans_clauses.begin(); it != prev_trans_clauses.end(); ++it)
  {
    presentToPrevious(*it);
    it->push_back(current_state_is_initial_);
  }
  prev_trans_or_initial_.swapWith(prev_trans_clauses);
  // if one of the state variables is true, then current_state_is_initial must be false:
  for(size_t cnt = 0; cnt < s.size(); ++cnt)
    prev_trans_or_initial_.add2LitClause(-s[cnt], -current_state_is_initial_);
}

// -------------------------------------------------------------------------------------------
int ParallelLearner::presentToPrevious(int literal) const
{
  int var = (literal < 0) ? -literal : literal;
  if(literal < 0)
    return -current_to_previous_map_[var];
  else
    return current_to_previous_map_[var];
}

// -------------------------------------------------------------------------------------------
void ParallelLearner::presentToPrevious(vector<int> &cube_or_clause) const
{
  for(size_t cnt = 0; cnt < cube_or_clause.size(); ++cnt)
    cube_or_clause[cnt] = presentToPrevious(cube_or_clause[cnt]);
}

// -------------------------------------------------------------------------------------------
void ParallelLearner::presentToPrevious(CNF &cnf) const
{
  list<vector<int> > orig_clauses = cnf.getClauses();
  cnf.clear();
  for(CNF::ClauseConstIter it = orig_clauses.begin(); it != orig_clauses.end(); ++it)
  {
    vector<int> clause(*it);
    presentToPrevious(clause);
    cnf.addClause(clause);
  }
}

// -------------------------------------------------------------------------------------------
ClauseExplorerSAT::ClauseExplorerSAT(size_t instance_nr, ParallelLearner &coordinator):
                   instance_nr_(instance_nr),
                   use_ind_(false),
                   coordinator_(coordinator),
                   precise_(true),
                   solver_i_(NULL),
                   solver_ctrl_(NULL),
                   vars_to_keep_(VarManager::instance().getAllNonTempVars()),
                   new_useless_input_clauses_level_(0),
                   restart_level_(0),
                   new_restart_level_(0),
                   solver_ctrl_ind_(NULL),
                   prev_safe_(0)

{
  // introduce some asymmetry to prevent the explorers from doing exactly the same thing:
  if(instance_nr == 0)
  {
    solver_i_ = new LingelingApi(false, true);
    solver_ctrl_ = new LingelingApi(false, true);
  }
  else if(instance_nr == 1)
  {
    solver_i_ = new MiniSatApi(false, true);
    solver_ctrl_ = new MiniSatApi(false, true);
  }
  else if((instance_nr & 1) == 0)
  {
    solver_i_ = new LingelingApi(false, true);
    solver_ctrl_ = new LingelingApi(false, true);
  }
  else
  {
    solver_i_ = new MiniSatApi(false, true);
    solver_ctrl_ = new MiniSatApi(false, true);
  }
}

// -------------------------------------------------------------------------------------------
ClauseExplorerSAT::ClauseExplorerSAT(size_t instance_nr,
                                     ParallelLearner &coordinator,
                                     const CNF &prev_trans_or_initial,
                                     const vector<int> &current_to_previous_map,
                                     int current_state_is_initial):
                   instance_nr_(instance_nr),
                   use_ind_(true),
                   coordinator_(coordinator),
                   precise_(true),
                   solver_i_(NULL),
                   solver_ctrl_(NULL),
                   vars_to_keep_(VarManager::instance().getAllNonTempVars()),
                   new_useless_input_clauses_level_(0),
                   restart_level_(0),
                   new_restart_level_(0),
                   solver_ctrl_ind_(NULL),
                   prev_trans_or_initial_(prev_trans_or_initial),
                   current_to_previous_map_(current_to_previous_map),
                   current_state_is_initial_(current_state_is_initial),
                   prev_safe_(-current_to_previous_map[VarManager::instance().getPresErrorStateVar()])

{
  // introduce some asymmetry to prevent the explorers from doing exactly the same thing:
  if(instance_nr == 0)
  {
    solver_i_ = new LingelingApi(false, true);
    solver_ctrl_ = new LingelingApi(false, false);
    solver_ctrl_ind_ = new LingelingApi(false, true);
  }
  else if(instance_nr == 1)
  {
    solver_i_ = new MiniSatApi(false, true);
    solver_ctrl_ = new MiniSatApi(false, false);
    solver_ctrl_ind_ = new LingelingApi(false, true);
  }
  else if(instance_nr == 2)
  {
    solver_i_ = new PicoSatApi(false, true);
    solver_ctrl_ = new MiniSatApi(false, false);
    solver_ctrl_ind_ = new LingelingApi(false, true);
  }
  else if((instance_nr & 1) == 0)
  {
    solver_i_ = new LingelingApi(false, true);
    solver_ctrl_ = new LingelingApi(false, false);
    solver_ctrl_ind_ = new LingelingApi(false, true);
  }
  else
  {
    solver_i_ = new MiniSatApi(false, true);
    solver_ctrl_ = new MiniSatApi(false, true);
    solver_ctrl_ind_ = new LingelingApi(false, true);
  }
}

// -------------------------------------------------------------------------------------------
ClauseExplorerSAT::~ClauseExplorerSAT()
{
  delete solver_i_;
  solver_i_ = NULL;
  delete solver_ctrl_;
  solver_ctrl_ = NULL;
  delete solver_ctrl_ind_;
  solver_ctrl_ind_ = NULL;
}

// -------------------------------------------------------------------------------------------
void ClauseExplorerSAT::exploreClauses()
{
  const AIG2CNF& A2C = AIG2CNF::instance();

  solver_i_->startIncrementalSession(vars_to_keep_, false);
  solver_i_->incAddCNF(A2C.getNextUnsafeStates());
  solver_i_->incAddCNF(A2C.getTrans());
  solver_i_->incAddCNF(A2C.getSafeStates());

  solver_ctrl_->startIncrementalSession(vars_to_keep_, false);
  solver_ctrl_->incAddCNF(A2C.getNextSafeStates());
  solver_ctrl_->incAddCNF(A2C.getTrans());
  solver_ctrl_->incAddCNF(A2C.getSafeStates());

  if(use_ind_)
  {
    solver_ctrl_ind_->startIncrementalSession(vars_to_keep_, true);
    solver_ctrl_ind_->incAddCNF(A2C.getNextSafeStates());
    solver_ctrl_ind_->incAddCNF(A2C.getTrans());
    solver_ctrl_ind_->incAddCNF(A2C.getSafeStates());
    solver_ctrl_ind_->incAddCNF(prev_trans_or_initial_);
    solver_ctrl_ind_->incAddUnitClause(prev_safe_);
  }

  precise_ = true;

  const vector<int> &s = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &c = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  const vector<int> &i = VarManager::instance().getVarsOfType(VarInfo::INPUT);
  vector<int> si;
  si.reserve(s.size() + i.size());
  si.insert(si.end(), s.begin(), s.end());
  si.insert(si.end(), i.begin(), i.end());
  size_t it_cnt = 0;
  vector<int> model_or_core;
  while(true)
  {
    ++it_cnt;
    if(coordinator_.result_ != UNKNOWN) // should be atomic
      return;
    considerNewInfoFromOthers();
    statistics_.notifyBeforeComputeCandidate();
    bool sat = solver_i_->incIsSatModelOrCore(vector<int>(), si, model_or_core);
    statistics_.notifyAfterComputeCandidate();
    if(!sat)
    {
      if(precise_)
      {
        coordinator_.result_ = REALIZABLE;  // should be atomic
        return;
      }
      if(coordinator_.result_ != UNKNOWN) // should be atomic
        return;
      L_DBG("Explorer " << instance_nr_ << " restarts (iteration " << it_cnt << ").");

      // minor performance optimization:
      // if a restart is ongoing, we do not trigger another one but wait here until it is
      // done. This is not a correctness issue: doing multiple restarts triggered by
      // several threads is also safe (but a waste of resources).
      bool restart_available = waitUntilOngoingRestartDone();
      if(restart_available == false)
        coordinator_.triggerExplorerRestart();
      continue;
    }

    vector<int> state_input = model_or_core;
    vector<int> state = Utils::extract(state_input, VarInfo::PRES_STATE);
    vector<int> input = Utils::extract(state_input, VarInfo::INPUT);

    if(coordinator_.result_ != UNKNOWN) // should be atomic
      return;
    statistics_.notifyBeforeCheckCandidate();
    Utils::randomize(state);
    sat = solver_ctrl_->incIsSatModelOrCore(state, input, c, model_or_core);
    if(!sat)
    {
      if(use_ind_)
      {
        // we now try to minimize the core further using reachability information:
        vector<int> orig_core(model_or_core);
        for(size_t lit_cnt = 0; lit_cnt < orig_core.size(); ++lit_cnt)
        {
          vector<int> tmp(model_or_core);
          Utils::remove(tmp, orig_core[lit_cnt]);
          solver_ctrl_ind_->incPush();

          // use what we already know immediately:
          vector<int> prev_core(model_or_core);
          presentToPrevious(prev_core);
          vector<int> next_core(model_or_core);
          Utils::swapPresentToNext(next_core);
          solver_ctrl_ind_->incAddNegCubeAsClause(model_or_core);
          solver_ctrl_ind_->incAddNegCubeAsClause(prev_core);
          solver_ctrl_ind_->incAddNegCubeAsClause(next_core);

          solver_ctrl_ind_->incAddCube(input);
          solver_ctrl_ind_->incAddCube(tmp);
          vector<int> prev_tmp(tmp);
          presentToPrevious(prev_tmp);
          prev_tmp.push_back(-current_state_is_initial_);
          solver_ctrl_ind_->incAddNegCubeAsClause(prev_tmp);
          sat = solver_ctrl_ind_->incIsSat();
          solver_ctrl_ind_->incPop();
          if(!sat)
            model_or_core = tmp;
        }
      }

      if(Utils::containsInit(model_or_core))
      {
        coordinator_.result_ = UNREALIZABLE;  // should be atomic
        return;
      }

      vector<int> blocking_clause(model_or_core);
      for(size_t cnt = 0; cnt < blocking_clause.size(); ++cnt)
        blocking_clause[cnt] = -blocking_clause[cnt];
      statistics_.notifyAfterCheckCandidateFound(s.size(), blocking_clause.size());
      coordinator_.notifyNewWinRegClause(blocking_clause, EXPL);
      coordinator_.notifyNewCounterexample(state_input, model_or_core);
      precise_ = false;
    }
    else
    {
      statistics_.notifyAfterCheckCandidateFailed();
      vector<int> ctrl_cube = model_or_core;
      if(coordinator_.result_ != UNKNOWN) // should be atomic
        return;
      statistics_.notifyBeforeRefine();
      sat = solver_i_->incIsSatModelOrCore(state_input, ctrl_cube, vector<int>(), model_or_core);
      MASSERT(sat == false, "Impossible " << instance_nr_);
      for(size_t cnt = 0; cnt < model_or_core.size(); ++cnt)
        model_or_core[cnt] = -model_or_core[cnt];
      coordinator_.notifyNewUselessInputClause(model_or_core, restart_level_);
      statistics_.notifyAfterRefine(si.size(), model_or_core.size());
    }
  }
}

// -------------------------------------------------------------------------------------------
void ClauseExplorerSAT::notifyBeforeNewInfo()
{
  new_info_lock_.lock();
}

// -------------------------------------------------------------------------------------------
void ClauseExplorerSAT::notifyAfterNewInfo()
{
  new_info_lock_.unlock();
}

// -------------------------------------------------------------------------------------------
void ClauseExplorerSAT::notifyNewWinRegClause(const vector<int> &clause, int src)
{
  if(src == EXPL)
  {
    new_win_reg_clauses_for_solver_i_.addClause(clause);
    new_win_reg_clauses_for_solver_ctrl_.addClause(clause);
  }
  else
  {
    new_foreign_win_reg_clauses_for_solver_i_.addClause(clause);
    new_win_reg_clauses_for_solver_ctrl_.addClause(clause);
  }
}

// -------------------------------------------------------------------------------------------
void ClauseExplorerSAT::notifyNewUselessInputClause(const vector<int> &clause, int level)
{
  if(new_useless_input_clauses_level_ < level)
  {
    new_useless_input_clauses_.clear();
    new_useless_input_clauses_level_ = level;
  }
  if(new_useless_input_clauses_level_ == level)
    new_useless_input_clauses_.addClause(clause);
}

// -------------------------------------------------------------------------------------------
void ClauseExplorerSAT::notifyRestart(const CNF &restart_with_cnf)
{
  // if there have been several restarts in the meantime, we only care about the last one:
  restart_with_cnf_ = restart_with_cnf;
  new_win_reg_clauses_for_solver_i_.clear();
  new_foreign_win_reg_clauses_for_solver_i_.clear();
  ++new_restart_level_;
}

// -------------------------------------------------------------------------------------------
const LearnStatisticsSAT& ClauseExplorerSAT::getStatistics() const
{
  return statistics_;
}

// -------------------------------------------------------------------------------------------
void ClauseExplorerSAT::considerNewInfoFromOthers()
{
  new_info_lock_.lock();
  if(restart_with_cnf_.getNrOfClauses() != 0)
  {
    statistics_.notifyRestart();
    solver_i_->startIncrementalSession(vars_to_keep_, false);
    solver_i_->incAddCNF(restart_with_cnf_);
    restart_with_cnf_.clear();
    restart_level_ = new_restart_level_;
    precise_ = true;
  }
  if(new_win_reg_clauses_for_solver_ctrl_.getNrOfClauses() > 0)
  {
    CNF next_win_reg_clauses(new_win_reg_clauses_for_solver_ctrl_);
    next_win_reg_clauses.swapPresentToNext();

    solver_ctrl_->incAddCNF(new_win_reg_clauses_for_solver_ctrl_);
    solver_ctrl_->incAddCNF(next_win_reg_clauses);
    if(use_ind_)
    {
      CNF prev_win_reg_clauses(new_win_reg_clauses_for_solver_ctrl_);
      presentToPrevious(prev_win_reg_clauses);
      solver_ctrl_ind_->incAddCNF(new_win_reg_clauses_for_solver_ctrl_);
      solver_ctrl_ind_->incAddCNF(next_win_reg_clauses);
      solver_ctrl_ind_->incAddCNF(prev_win_reg_clauses);
    }
    new_win_reg_clauses_for_solver_ctrl_.clear();
  }
  if(new_win_reg_clauses_for_solver_i_.getNrOfClauses() > 0)
  {
    solver_i_->incAddCNF(new_win_reg_clauses_for_solver_i_);
    precise_ = false;
    new_win_reg_clauses_for_solver_i_.clear();
  }
  if(!precise_ && new_foreign_win_reg_clauses_for_solver_i_.getNrOfClauses() > 0)
  {
    // We only consider foreign clauses for solver_i_ if we are already imprecise. Otherwise,
    // the foreign clauses could prevent the explorers from staying in a precise state long
    // enough to proof realizability:
    solver_i_->incAddCNF(new_foreign_win_reg_clauses_for_solver_i_);
    precise_ = false;
    new_foreign_win_reg_clauses_for_solver_i_.clear();
  }

  if(restart_level_ == new_useless_input_clauses_level_)
  {
    solver_i_->incAddCNF(new_useless_input_clauses_);
    new_useless_input_clauses_.clear();
  }
  new_info_lock_.unlock();
}

// -------------------------------------------------------------------------------------------
bool ClauseExplorerSAT::waitUntilOngoingRestartDone()
{
  // if a restart is ongoing, the coordinator_.var_man_lock_.lock() is held. Locking it
  // has the effect that we wait until the restart is done.
  coordinator_.var_man_lock_.lock();
  coordinator_.var_man_lock_.unlock();

  new_info_lock_.lock();
  bool restart_available = restart_with_cnf_.getNrOfClauses() != 0;
  new_info_lock_.unlock();
  return restart_available;
}

// -------------------------------------------------------------------------------------------
int ClauseExplorerSAT::presentToPrevious(int literal) const
{
  int var = (literal < 0) ? -literal : literal;
  if(literal < 0)
    return -current_to_previous_map_[var];
  else
    return current_to_previous_map_[var];
}

// -------------------------------------------------------------------------------------------
void ClauseExplorerSAT::presentToPrevious(vector<int> &cube_or_clause) const
{
  for(size_t cnt = 0; cnt < cube_or_clause.size(); ++cnt)
    cube_or_clause[cnt] = presentToPrevious(cube_or_clause[cnt]);
}

// -------------------------------------------------------------------------------------------
void ClauseExplorerSAT::presentToPrevious(CNF &cnf) const
{
  list<vector<int> > orig_clauses = cnf.getClauses();
  cnf.clear();
  for(CNF::ClauseConstIter it = orig_clauses.begin(); it != orig_clauses.end(); ++it)
  {
    vector<int> clause(*it);
    presentToPrevious(clause);
    cnf.addClause(clause);
  }
}

// -------------------------------------------------------------------------------------------
CounterGenSAT::CounterGenSAT(ParallelLearner &coordinator):
                             use_ind_(false),
                             coordinator_(coordinator),
                             vars_to_keep_(VarManager::instance().getAllNonTempVars()),
                             solver_ctrl_(Options::instance().getSATSolver(false, true)),
                             solver_win_(Options::instance().getSATSolver(false, true)),
                             current_state_is_initial_(0),
                             solver_ctrl_ind_(NULL),
                             prev_safe_(0),
                             next_bored_index_(0),
                             last_bored_compress_size_(1)
{
  do_if_bored_.reserve(1000000);
}

// -------------------------------------------------------------------------------------------
CounterGenSAT::CounterGenSAT(ParallelLearner &coordinator,
         const CNF &prev_trans_or_initial,
         const vector<int> &current_to_previous_map,
         int current_state_is_initial):
         use_ind_(true),
         coordinator_(coordinator),
         vars_to_keep_(VarManager::instance().getAllNonTempVars()),
         solver_ctrl_(Options::instance().getSATSolver(false, false)),
         solver_win_(Options::instance().getSATSolver(false, true)),
         prev_trans_or_initial_(prev_trans_or_initial),
         current_to_previous_map_(current_to_previous_map),
         current_state_is_initial_(current_state_is_initial),
         solver_ctrl_ind_(Options::instance().getSATSolver(false, true)),
         prev_safe_(-current_to_previous_map[VarManager::instance().getPresErrorStateVar()]),
         next_bored_index_(0),
         last_bored_compress_size_(1)
{
  do_if_bored_.reserve(30000);
}

// -------------------------------------------------------------------------------------------
CounterGenSAT::~CounterGenSAT()
{
  delete solver_win_;
  solver_win_ = NULL;
  delete solver_ctrl_;
  solver_ctrl_ = NULL;
  delete solver_ctrl_ind_;
  solver_ctrl_ind_ = NULL;
}

// -------------------------------------------------------------------------------------------
void CounterGenSAT::generalizeCounterexamples()
{
  const AIG2CNF& A2C = AIG2CNF::instance();

  solver_ctrl_->startIncrementalSession(vars_to_keep_, false);
  solver_ctrl_->incAddCNF(A2C.getNextSafeStates());
  solver_ctrl_->incAddCNF(A2C.getTrans());
  solver_ctrl_->incAddCNF(A2C.getSafeStates());

  const vector<int> &ps_vars = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE);
  solver_win_->startIncrementalSession(ps_vars, false);
  solver_win_->incAddCNF(A2C.getSafeStates());

  if(use_ind_)
  {
    solver_ctrl_ind_->startIncrementalSession(vars_to_keep_, true);
    solver_ctrl_ind_->incAddCNF(A2C.getNextSafeStates());
    solver_ctrl_ind_->incAddCNF(A2C.getTrans());
    solver_ctrl_ind_->incAddCNF(A2C.getSafeStates());
    solver_ctrl_ind_->incAddCNF(prev_trans_or_initial_);
    solver_ctrl_ind_->incAddUnitClause(prev_safe_);
  }

  pair<vector<int>, vector<int> > task;
  while(true)
  {
    if(coordinator_.result_ != UNKNOWN) // should be atomic
      return;
    coordinator_.counterexamples_lock_.lock();
    bool empty = coordinator_.counterexamples_.empty();
    if(!empty)
    {
      task = coordinator_.counterexamples_.front();
      coordinator_.counterexamples_.pop_front();
    }
    coordinator_.counterexamples_lock_.unlock();
    if(empty)
    {
      bored();
      continue;
    }

    vector<int> in = Utils::extract(task.first, VarInfo::INPUT);
    vector<int> orig_ce_state = Utils::extract(task.first, VarInfo::PRES_STATE);
    vector<int> first_gen = task.second;
    size_t orig_gen_size = first_gen.size();
    generalizeCeFuther(first_gen, in);
    if(first_gen.size() < orig_gen_size)
    {
      if(solver_win_->incIsSat(first_gen))
      {
        vector<int> blocking_clause(first_gen);
        Utils::negateLiterals(blocking_clause);
        coordinator_.notifyNewWinRegClause(blocking_clause, CE_GEN);
      }
    }

    do_if_bored_.push_back(make_pair(first_gen, in));

    list<set<int> > must_not_contain_queue;
    list<vector<int> > gen_ces;
    gen_ces.push_back(first_gen);
    for(size_t cnt = 0; cnt < first_gen.size(); ++cnt)
    {
      set<int> singleton_set;
      singleton_set.insert(first_gen[cnt]);
      must_not_contain_queue.push_back(singleton_set);
    }

    while(!must_not_contain_queue.empty())
    {
      set<int> must_not_contain = must_not_contain_queue.front();
      must_not_contain_queue.pop_front();

      // check if we already have a ce in gen_ces which satisfies our needs:
      bool found = false;
      for(list<vector<int> >::const_iterator it = gen_ces.begin(); it != gen_ces.end(); ++it)
      {
        if(Utils::intersectionEmpty(*it, must_not_contain))
        {
          found = true;
          break;
        }
      }
      if(found)
        continue;

      // if not: we compute one
      vector<int> new_ce(orig_ce_state);
      for(set<int>::const_iterator it = must_not_contain.begin(); it != must_not_contain.end(); ++it)
        Utils::remove(new_ce, *it);
      size_t orig_size = new_ce.size();
      if(coordinator_.result_ != UNKNOWN) // should be atomic
        return;
      statistics_.notifyBeforeCheckCandidate();
      bool is_ok = generalizeCounterexample(new_ce, in);
      if(is_ok)
      {
        statistics_.notifyAfterCheckCandidateFound(orig_size, new_ce.size());

        if(Utils::containsInit(new_ce))
        {
          coordinator_.result_ = UNREALIZABLE;  // should be atomic
          return;
        }

        gen_ces.push_back(new_ce);
        vector<int> blocking_clause(new_ce);
        Utils::negateLiterals(blocking_clause);
        for(size_t cnt = 0; cnt < new_ce.size(); ++cnt)
        {
          set<int> new_must_not_contain(must_not_contain);
          new_must_not_contain.insert(new_ce[cnt]);
          must_not_contain_queue.push_back(new_must_not_contain);
        }

        // check if blocking_clause is implied by existing clauses:
        if(solver_win_->incIsSat(new_ce))
        {
          do_if_bored_.push_back(make_pair(new_ce, in));
          coordinator_.notifyNewWinRegClause(blocking_clause, CE_GEN);
        }
      }
      else
        statistics_.notifyAfterCheckCandidateFailed();
    }
  }
}

// -------------------------------------------------------------------------------------------
void CounterGenSAT::notifyNewWinRegClause(const vector<int> &clause, int src)
{
  new_win_reg_clauses_lock_.lock();
  new_win_reg_clauses_.addClause(clause);
  new_win_reg_clauses_lock_.unlock();
}

// -------------------------------------------------------------------------------------------
const LearnStatisticsSAT& CounterGenSAT::getStatistics() const
{
  return statistics_;
}

// -------------------------------------------------------------------------------------------
bool CounterGenSAT::generalizeCounterexample(vector<int> &state_cube,
                                             const vector<int> &in_cube)
{
  vector<int> core;
  considerNewInfoFromOthers();
  bool sat = solver_ctrl_->incIsSatModelOrCore(state_cube, in_cube, vector<int>(), core);
  if(sat)
    return false;
  if(use_ind_)
    generalizeCeFuther(core, in_cube);
  state_cube = core;
  return true;
}

// -------------------------------------------------------------------------------------------
void CounterGenSAT::generalizeCeFuther(vector<int> &core, const vector<int> &in_cube)
{
  // minimize further using reachability:
  // we now try to minimize the core further using reachability information:
  considerNewInfoFromOthers();
  vector<int> orig_core(core);
  if(use_ind_)
  {
    for(size_t lit_cnt = 0; lit_cnt < orig_core.size(); ++lit_cnt)
    {
      vector<int> tmp(core);
      Utils::remove(tmp, orig_core[lit_cnt]);
      solver_ctrl_ind_->incPush();

      // use what we already know immediately:
      vector<int> prev_core(core);
      presentToPrevious(prev_core);
      vector<int> next_core(core);
      Utils::swapPresentToNext(next_core);
      solver_ctrl_ind_->incAddNegCubeAsClause(core);
      solver_ctrl_ind_->incAddNegCubeAsClause(prev_core);
      solver_ctrl_ind_->incAddNegCubeAsClause(next_core);

      solver_ctrl_ind_->incAddCube(in_cube);
      solver_ctrl_ind_->incAddCube(tmp);
      vector<int> prev_tmp(tmp);
      presentToPrevious(prev_tmp);
      prev_tmp.push_back(-current_state_is_initial_);
      solver_ctrl_ind_->incAddNegCubeAsClause(prev_tmp);
      bool sat = solver_ctrl_ind_->incIsSat();
      solver_ctrl_ind_->incPop();
      if(!sat)
        core = tmp;
    }
  }
  else
  {
    bool sat = solver_ctrl_->incIsSatModelOrCore(orig_core, in_cube, vector<int>(), core);
    MASSERT(sat == false, "Impossible.");
  }
}

// -------------------------------------------------------------------------------------------
void CounterGenSAT::bored()
{
  if(!do_if_bored_.empty())
  {
    if(do_if_bored_.size() > 20 && 2 * last_bored_compress_size_ < do_if_bored_.size())
      compressBored();
    if(coordinator_.result_ != UNKNOWN) // should be atomic
      return;

    vector<int> ce = do_if_bored_[next_bored_index_].first;
    vector<int> in = do_if_bored_[next_bored_index_].second;
    size_t org_size = ce.size();
    generalizeCeFuther(ce, in);
    if(ce.size() < org_size)
    {
      do_if_bored_[next_bored_index_] = make_pair(ce, in);
      if(solver_win_->incIsSat(ce))
      {
        Utils::negateLiterals(ce);
        coordinator_.notifyNewWinRegClause(ce, CE_GEN);
      }
    }
    ++next_bored_index_;
    if(next_bored_index_ >= do_if_bored_.size())
      next_bored_index_ = 0;
  }
}

// -------------------------------------------------------------------------------------------
void CounterGenSAT::compressBored()
{
  if(!do_if_bored_.empty())
  {
    vector<pair<vector<int>, vector<int> > > still_to_process(do_if_bored_);
    do_if_bored_.clear();
    do_if_bored_.reserve(30000);
    SatSolver *solver = Options::instance().getSATSolver();
    solver->startIncrementalSession(VarManager::instance().getVarsOfType(VarInfo::PRES_STATE),
                                    false);
    solver->incAddCNF(AIG2CNF::instance().getSafeStates());

    // find smallest:
    size_t smallest_idx = 0;
    size_t smallest_size = still_to_process[0].first.size();
    for(size_t cnt = 1; cnt < still_to_process.size(); ++cnt)
    {
      if(still_to_process[cnt].first.size() < smallest_size)
      {
        smallest_idx = cnt;
        smallest_size = still_to_process[cnt].first.size();
      }
    }
    // take smallest
    do_if_bored_.push_back(still_to_process[smallest_idx]);
    still_to_process[smallest_idx] = still_to_process.back();
    still_to_process.pop_back();
    solver->incAddNegCubeAsClause(do_if_bored_.back().first);
    while(!still_to_process.empty())
    {
      size_t smallest_idx = 0;
      size_t smallest_size = still_to_process[0].first.size();
      for(size_t cnt = 1; cnt < still_to_process.size(); ++cnt)
      {
        if(still_to_process[cnt].first.size() < smallest_size)
        {
          smallest_idx = cnt;
          smallest_size = still_to_process[cnt].first.size();
        }
      }
      if(coordinator_.result_ != UNKNOWN) // should be atomic
        break;
      if(solver->incIsSat(still_to_process[smallest_idx].first))
      {
        do_if_bored_.push_back(still_to_process[smallest_idx]);
        solver->incAddNegCubeAsClause(still_to_process[smallest_idx].first);
      }
      still_to_process[smallest_idx] = still_to_process.back();
      still_to_process.pop_back();
    }
    delete solver;
    solver = NULL;
    last_bored_compress_size_ = do_if_bored_.size();
    next_bored_index_ = 0;
  }
}

// -------------------------------------------------------------------------------------------
void CounterGenSAT::considerNewInfoFromOthers()
{
  new_win_reg_clauses_lock_.lock();
  if(new_win_reg_clauses_.getNrOfClauses() > 0)
  {
    CNF next_win_reg_clauses(new_win_reg_clauses_);
    next_win_reg_clauses.swapPresentToNext();

    solver_ctrl_->incAddCNF(new_win_reg_clauses_);
    solver_ctrl_->incAddCNF(next_win_reg_clauses);
    solver_win_->incAddCNF(new_win_reg_clauses_);
    if(use_ind_)
    {
      CNF prev_win_reg_clauses(new_win_reg_clauses_);
      presentToPrevious(prev_win_reg_clauses);
      solver_ctrl_ind_->incAddCNF(new_win_reg_clauses_);
      solver_ctrl_ind_->incAddCNF(next_win_reg_clauses);
      solver_ctrl_ind_->incAddCNF(prev_win_reg_clauses);
    }
    new_win_reg_clauses_.clear();
  }
  new_win_reg_clauses_lock_.unlock();
}

// -------------------------------------------------------------------------------------------
int CounterGenSAT::presentToPrevious(int literal) const
{
  int var = (literal < 0) ? -literal : literal;
  if(literal < 0)
    return -current_to_previous_map_[var];
  else
    return current_to_previous_map_[var];
}

// -------------------------------------------------------------------------------------------
void CounterGenSAT::presentToPrevious(vector<int> &cube_or_clause) const
{
  for(size_t cnt = 0; cnt < cube_or_clause.size(); ++cnt)
    cube_or_clause[cnt] = presentToPrevious(cube_or_clause[cnt]);
}

// -------------------------------------------------------------------------------------------
void CounterGenSAT::presentToPrevious(CNF &cnf) const
{
  list<vector<int> > orig_clauses = cnf.getClauses();
  cnf.clear();
  for(CNF::ClauseConstIter it = orig_clauses.begin(); it != orig_clauses.end(); ++it)
  {
    vector<int> clause(*it);
    presentToPrevious(clause);
    cnf.addClause(clause);
  }
}

// -------------------------------------------------------------------------------------------
ClauseMinimizerQBF::ClauseMinimizerQBF(ParallelLearner &coordinator):
                   coordinator_(coordinator),
                   qbf_solver_(Options::instance().getQBFSolver()),
                   sat_solver_(Options::instance().getSATSolver(false, false))

{
  const vector<int> &ps = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &i = VarManager::instance().getVarsOfType(VarInfo::INPUT);
  const vector<int> &c = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  const vector<int> &ns = VarManager::instance().getVarsOfType(VarInfo::NEXT_STATE);
  const vector<int> &t = VarManager::instance().getVarsOfType(VarInfo::TMP);

  gen_quant_.push_back(make_pair(ps, QBFSolver::E));
  gen_quant_.push_back(make_pair(i, QBFSolver::A));
  gen_quant_.push_back(make_pair(c, QBFSolver::E));
  gen_quant_.push_back(make_pair(ns, QBFSolver::E));
  gen_quant_.push_back(make_pair(t, QBFSolver::E));
}

// -------------------------------------------------------------------------------------------
ClauseMinimizerQBF::~ClauseMinimizerQBF()
{
  delete qbf_solver_;
  qbf_solver_ = NULL;

  delete sat_solver_;
  sat_solver_ = NULL;
}

// -------------------------------------------------------------------------------------------
void ClauseMinimizerQBF::minimizeClauses()
{
  vector<int> orig;

  while(true)
  {
    if(coordinator_.result_ != UNKNOWN) // should be atomic
      return;
    coordinator_.unminimized_clauses_lock_.lock();
    bool empty = coordinator_.unminimized_clauses_.getNrOfClauses() == 0;
    if(!empty)
      orig = coordinator_.unminimized_clauses_.removeSmallest();
    coordinator_.unminimized_clauses_lock_.unlock();
    if(empty)
    {
      usleep(100000); // microseconds
      continue;
    }

    Utils::randomize(orig);
    coordinator_.winning_region_lock_.lock();
    CNF win_reg = coordinator_.winning_region_;
    coordinator_.winning_region_lock_.unlock();
    CNF generalize_clause_cnf(win_reg);
    generalize_clause_cnf.swapPresentToNext();
    generalize_clause_cnf.addCNF(win_reg);
    generalize_clause_cnf.addCNF(AIG2CNF::instance().getTrans());

    vector<int> smallest_so_far(orig);
    for(size_t cnt = 0; cnt < orig.size(); ++cnt)
    {
      vector<int> tmp(smallest_so_far);
      Utils::remove(tmp, orig[cnt]);
      CNF check_cnf(generalize_clause_cnf);
      check_cnf.addNegClauseAsCube(tmp);
      if(coordinator_.result_ != UNKNOWN) // should be atomic
        return;
      if(!qbf_solver_->isSat(gen_quant_, check_cnf))
        smallest_so_far = tmp;
    }
    if(orig.size() > smallest_so_far.size())
    {
      vector<int> state_cube(smallest_so_far);
      Utils::negateLiterals(state_cube);
      if(Utils::containsInit(state_cube))
      {
        coordinator_.result_ = UNREALIZABLE;  // should be atomic
        return;
      }
      // check if the smaller clause is already implied by what we have:
      CNF check(win_reg);
      check.addNegClauseAsCube(smallest_so_far);
      if(sat_solver_->isSat(check))
      {
        coordinator_.notifyNewWinRegClause(smallest_so_far, MIN);
      }
    }
  }
}

// -------------------------------------------------------------------------------------------
IFM13Explorer::IFM13Explorer(ParallelLearner &coordinator) :
               coordinator_(coordinator),
               error_var_(VarManager::instance().getPresErrorStateVar())
{
  const vector<int> &s = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &n = VarManager::instance().getVarsOfType(VarInfo::NEXT_STATE);
  const vector<int> &c = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  const vector<int> &i = VarManager::instance().getVarsOfType(VarInfo::INPUT);
  sin_.reserve(s.size() + i.size() + n.size());
  sin_.insert(sin_.end(), s.begin(), s.end());
  sin_.insert(sin_.end(), i.begin(), i.end());
  sin_.insert(sin_.end(), n.begin(), n.end());

  sicn_.reserve(s.size() + i.size() + c.size() + n.size());
  sicn_.insert(sicn_.end(), s.begin(), s.end());
  sicn_.insert(sicn_.end(), i.begin(), i.end());
  sicn_.insert(sicn_.end(), c.begin(), c.end());
  sicn_.insert(sicn_.end(), n.begin(), n.end());

  r_.reserve(10000);
  r_.push_back(AIG2CNF::instance().getUnsafeStates()); // R[0] = !P
  goto_next_lower_solvers_.reserve(10000);
  goto_next_lower_solvers_.push_back(NULL);
  goto_next_lower_solvers_.push_back(Options::instance().getSATSolver());
  goto_next_lower_solvers_.back()->startIncrementalSession(sicn_, false);
  goto_next_lower_solvers_.back()->incAddCNF(AIG2CNF::instance().getTrans());
  goto_next_lower_solvers_.back()->incAddCNF(AIG2CNF::instance().getNextUnsafeStates());
  gen_block_trans_solvers_.reserve(10000);
  gen_block_trans_solvers_.push_back(NULL);
  gen_block_trans_solvers_.push_back(Options::instance().getSATSolver());
  gen_block_trans_solvers_.back()->startIncrementalSession(sicn_, false);
  gen_block_trans_solvers_.back()->incAddCNF(AIG2CNF::instance().getTrans());
  gen_block_trans_solvers_.back()->incAddCNF(AIG2CNF::instance().getNextUnsafeStates());

#ifndef NDEBUG
  u_.reserve(10000);
  u_.push_back(AIG2CNF::instance().getUnsafeStates());
#endif

  win_.addCNF(AIG2CNF::instance().getSafeStates());
  goto_win_solver_ = Options::instance().getSATSolver();
  goto_win_solver_->startIncrementalSession(sicn_, false);
  goto_win_solver_->incAddCNF(AIG2CNF::instance().getTrans());
  goto_win_solver_->incAddCNF(AIG2CNF::instance().getNextSafeStates());

  initial_state_cube_.reserve(s.size());
  for(size_t cnt = 0; cnt < s.size(); ++cnt)
    initial_state_cube_.push_back(-s[cnt]);
}


// -------------------------------------------------------------------------------------------
IFM13Explorer::~IFM13Explorer()
{
  delete goto_win_solver_;
  goto_win_solver_ = NULL;

  for(size_t cnt = 0; cnt < goto_next_lower_solvers_.size(); ++cnt)
    delete goto_next_lower_solvers_[cnt];
  goto_next_lower_solvers_.clear();

  for(size_t cnt = 0; cnt < gen_block_trans_solvers_.size(); ++cnt)
    delete gen_block_trans_solvers_[cnt];
  gen_block_trans_solvers_.clear();
}

// -------------------------------------------------------------------------------------------
void IFM13Explorer::exploreClauses()
{
  size_t k = 1;
  while(true)
  {
    L_DBG("------ Iteration k=" << k);
    if(coordinator_.result_ != UNKNOWN) // should be atomic
      return;
    if(recBlockCube(initial_state_cube_, k) == IS_LOSE)
    {
      coordinator_.result_ = UNREALIZABLE;  // should be atomic
      return;
    }
    if(coordinator_.result_ != UNKNOWN) // should be atomic
      return;

    //debugCheckInvariants(k);
    size_t equal = propagateBlockedStates(k);
    if(coordinator_.result_ != UNKNOWN) // should be atomic
      return;
    //debugCheckInvariants(k);
    if(equal != 0)
    {
      L_LOG("Found two equal clause sets: R" << (equal-1) << " and R" << equal);
      //L_LOG("Nr of iterations: " << k);
      return;
    }
    ++k;
  }
}

// -------------------------------------------------------------------------------------------
void IFM13Explorer::notifyNewWinRegClause(const vector<int> &clause, int src)
{
  new_win_reg_clauses_lock_.lock();
  new_win_reg_clauses_.addClause(clause);
  new_win_reg_clauses_lock_.unlock();
}

// -------------------------------------------------------------------------------------------
void IFM13Explorer::considerNewInfoFromOthers()
{
  new_win_reg_clauses_lock_.lock();
  win_.addCNF(new_win_reg_clauses_);
  new_win_reg_clauses_.swapPresentToNext();
  goto_win_solver_->incAddCNF(new_win_reg_clauses_);
  new_win_reg_clauses_.clear();
  new_win_reg_clauses_lock_.unlock();
}

// -------------------------------------------------------------------------------------------
size_t IFM13Explorer::propagateBlockedStates(size_t max_level)
{
  for(size_t i = 0; i <= max_level; ++i)
    getR(i).removeDuplicates();

  if(coordinator_.result_ != UNKNOWN) // should be atomic
    return 0;

  for(size_t i = 1; i <= max_level; ++i)
  {
    bool equal = true;
    const list<vector<int> > &r1clauses = getR(i).getClauses();
    CNF& r2cnf = getR(i+1);
    list<vector<int> > r2clauses = r2cnf.getClauses();
    list<vector<int> >::const_iterator it1 = r1clauses.begin();
    list<vector<int> >::iterator it2 = r2clauses.begin();

    for(; it1 != r1clauses.end(); )
    {
      if(coordinator_.result_ != UNKNOWN) // should be atomic
        return 0;
      if(it2 != r2clauses.end() && *it1 == *it2)
      {
        ++it1;
        ++it2;
        continue;
      }
      vector<int> neg_clause(*it1);
      for(size_t lit_cnt = 0; lit_cnt < neg_clause.size(); ++lit_cnt)
        neg_clause[lit_cnt] = -neg_clause[lit_cnt];
      if(!getGotoNextLowerSolver(i+1)->incIsSat(neg_clause))
      {
        // There is no edge from s (the negated clause) to Ri
        // --> no state in s can be part of Ri+1
        it2 = r2clauses.insert(it2,*it1);
        vector<int> propagated(*it1);
        Utils::swapPresentToNext(propagated);
        getGotoNextLowerSolver(i+2)->incAddClause(propagated);
        getGenBlockTransSolver(i+2)->incAddClause(propagated);
        ++it1;
        ++it2;
      }
      else
      {
        // Propagation failed
        ++it1;
        equal = false;
      }
    }
    r2cnf.swapWith(r2clauses);
    if(equal)
      return i + 1;
  }
  return 0;
}

// -------------------------------------------------------------------------------------------
bool IFM13Explorer::recBlockCube(const vector<int> &state_cube, size_t level)
{
  list<IFMProofObligation> queue;
  queue.push_back(IFMProofObligation(state_cube, level));

  vector<int> model_or_core;
  while(!queue.empty())
  {
    if(coordinator_.result_ != UNKNOWN) // should be atomic
      return IS_GREATER;
    IFMProofObligation proof_obligation = popMin(queue);
    const vector<int> &s = proof_obligation.getState();
    size_t s_level = proof_obligation.getLevel();

    if(isLose(s))
      continue;
    if(coordinator_.result_ != UNKNOWN) // should be atomic
      return IS_GREATER;
    if(isBlocked(s, s_level))
    {
      // BEGIN optimization A
      // Performance optimization not mentioned in the IFM'13 paper, but exploited in
      // Andreas Morgenstern's implementation:
      // We block the transition of the predecessor to s
      if(coordinator_.result_ != UNKNOWN) // should be atomic
        return IS_GREATER;
      if(proof_obligation.hasPre())
      {
        const vector<int> &si = proof_obligation.getPreStateInCube();
        const vector<int> &c = proof_obligation.getPreCtrlCube();
        genAndBlockTrans(si, c, s_level + 1);
      }
      // END optimization A
      continue;
    }
    if(coordinator_.result_ != UNKNOWN) // should be atomic
      return IS_GREATER;
    SatSolver *goto_next_lower = getGotoNextLowerSolver(s_level);
    bool isSat = goto_next_lower->incIsSatModelOrCore(s, sin_, model_or_core);
    if(isSat)
    {
      //L_DBG("Found transition from R" << s_level << " to R" << (s_level - 1));
      vector<int> succ = Utils::extractNextAsPresent(model_or_core);
      if(coordinator_.result_ != UNKNOWN) // should be atomic
        return IS_GREATER;
      if(s_level == 1 || isLose(succ))
      {
        //L_DBG(" Successor is losing");
        if(coordinator_.result_ != UNKNOWN) // should be atomic
          return IS_GREATER;
        vector<int> in_cube = Utils::extract(model_or_core, VarInfo::INPUT);
        vector<int> state_cube = Utils::extract(model_or_core, VarInfo::PRES_STATE);
        considerNewInfoFromOthers();
        isSat = goto_win_solver_->incIsSatModelOrCore(state_cube, in_cube, sicn_, model_or_core);
        if(isSat)
        {
          //L_DBG(" We can also go to a winning state");
          succ = Utils::extractNextAsPresent(model_or_core);
          vector<int> si = Utils::extractPresIn(model_or_core);
          vector<int> c = Utils::extract(model_or_core, VarInfo::CTRL);
          if(coordinator_.result_ != UNKNOWN) // should be atomic
            return IS_GREATER;
          if(s_level == 1 || isBlocked(succ, s_level - 1))
          {
            // generalize the state-input pair (s,u):
            // find subset of s,i literals such that c input does not lead to Ri-1
            // A QBF call could also be a good option here
            genAndBlockTrans(si, c, s_level);
          }
          else
            queue.push_back(IFMProofObligation(succ, s_level - 1, si, c));
          queue.push_back(proof_obligation);
        }
        else
        {
          //L_DBG(" We cannot go to a winning state");
          if(Utils::containsInit(model_or_core))
            return IS_LOSE;
          addLose(model_or_core);
        }
      }
      else
      {
        //L_DBG(" Successor is winning");
        vector<int> si = Utils::extractPresIn(model_or_core);
        vector<int> c = Utils::extract(model_or_core, VarInfo::CTRL);
        queue.push_back(IFMProofObligation(succ, s_level - 1, si, c));
        queue.push_back(proof_obligation);
      }
    }
    else
    {
      //L_DBG(" No transition from R" << s_level << " to R" << (s_level - 1));
      addBlockedState(model_or_core, s_level);
      // BEGIN optimization A
      // Performance optimization not mentioned in the IFM13 paper, but exploited in
      // Andreas Morgenstern's implementation:
      // We block the transition of the predecessor to s
      if(coordinator_.result_ != UNKNOWN) // should be atomic
        return IS_GREATER;
      if(proof_obligation.hasPre())
      {
        const vector<int> &si = proof_obligation.getPreStateInCube();
        const vector<int> &c = proof_obligation.getPreCtrlCube();
        genAndBlockTrans(si, c, s_level + 1);
      }
      // END optimization A

      // BEGIN optimization B
      // Performance optimization not mentioned in the IFM13 paper, but exploited in
      // Andreas Morgenstern's implementation:
      // We aggressively decide s also on later levels:
      //if(s_level < r_.size() - 1)
      //{
      //  queue.push_back(IFMProofObligation(s, s_level+1, proof_obligation.getPreStateInCube(),
      //                                  proof_obligation.getPreCtrlCube()));
      //}
      // END optimization B
    }
  }
  return IS_GREATER;
}

// -------------------------------------------------------------------------------------------
void IFM13Explorer::addBlockedTransition(const vector<int> &state_in_cube, size_t level)
{
  vector<int> blocking_clause(state_in_cube);
  for(size_t cnt = 0; cnt < blocking_clause.size(); ++cnt)
    blocking_clause[cnt] = -blocking_clause[cnt];
  for(size_t l_cnt = 1; l_cnt <= level; ++l_cnt)
  {
#ifndef NDEBUG
    getU(l_cnt).addClauseAndSimplify(blocking_clause);
#endif
    getGotoNextLowerSolver(l_cnt)->incAddClause(blocking_clause);
  }
}

// -------------------------------------------------------------------------------------------
void IFM13Explorer::addBlockedState(const vector<int> &state_cube, size_t level)
{
  // We require that the cube does not intersect with R0, i.e., the bad states.
  // The intuitive reason is that the bad states do not have to have a successor (in
  // R[level-1]) order to be problematic.
  // Often there are bad states, which do not have a successor in the bad states.
  // Before minimization with unsat cores, the blocked state is actually completely outside
  // of the bad states. Now, instead of coming up with a complex generalization procedure
  // which ensures that the cube does not intersect with the bad states, we can simply add
  // this property later, because the bad states are characterized by one state bit in our
  // setting. This saves time.

  vector<int> blocking_clause;
  blocking_clause.reserve(state_cube.size() + 1);
  for(size_t cnt = 0; cnt < state_cube.size(); ++cnt)
    blocking_clause.push_back(-state_cube[cnt]);
  if(!Utils::contains(blocking_clause, error_var_))
    blocking_clause.push_back(error_var_);
  vector<int> next_blocking_clause(blocking_clause);
  Utils::swapPresentToNext(next_blocking_clause);
  for(size_t l_cnt = 0; l_cnt <= level; ++l_cnt)
  {
    getR(l_cnt).addClause(blocking_clause);
    getGotoNextLowerSolver(l_cnt+1)->incAddClause(next_blocking_clause);
    getGenBlockTransSolver(l_cnt+1)->incAddClause(next_blocking_clause);
  }

  // BEGIN optimization C
  // Performance optimization not mentioned in the IFM13 paper, but exploited in
  // Andreas Morgenstern's implementation:
  // We try to push the blocking_clause forward as far as possible:
  vector<int> neg_clause(state_cube);
  if(!Utils::contains(neg_clause, -error_var_))
    neg_clause.push_back(-error_var_);
  for(size_t l_cnt = level + 1; l_cnt < r_.size(); ++l_cnt)
  {
    if(!getGotoNextLowerSolver(l_cnt)->incIsSat(neg_clause))
    {
      getR(l_cnt).addClause(blocking_clause);
      getGotoNextLowerSolver(l_cnt + 1)->incAddClause(next_blocking_clause);
      getGenBlockTransSolver(l_cnt + 1)->incAddClause(next_blocking_clause);
    }
    else
      break;
  }
  // END optimization C

}

// -------------------------------------------------------------------------------------------
bool IFM13Explorer::isBlocked(const vector<int> &state_cube, size_t level)
{
  return !getR(level).isSatBy(state_cube);
}

// -------------------------------------------------------------------------------------------
void IFM13Explorer::addLose(const vector<int> &state_cube)
{
  vector<int> blocking_clause(state_cube);
  for(size_t cnt = 0; cnt < blocking_clause.size(); ++cnt)
    blocking_clause[cnt] = -blocking_clause[cnt];
  coordinator_.notifyNewWinRegClause(blocking_clause, IFM);
  // this thread will get and consider the new clause via the notification mechanism also.
}

// -------------------------------------------------------------------------------------------
bool IFM13Explorer::isLose(const vector<int> &state_cube)
{
  considerNewInfoFromOthers();
  return !win_.isSatBy(state_cube);
}

// -------------------------------------------------------------------------------------------
void IFM13Explorer::genAndBlockTrans(const vector<int> &state_in_cube,
                                  const vector<int> &ctrl_cube,
                                  size_t level)
{
  SatSolver *gen_solver = getGenBlockTransSolver(level);
  vector<int> model_or_core;
  bool isSat = gen_solver->incIsSatModelOrCore(state_in_cube, ctrl_cube,
                                               state_in_cube, model_or_core);
  MASSERT(isSat == false, "Impossible.");
  addBlockedTransition(model_or_core, level);
}

// -------------------------------------------------------------------------------------------
CNF& IFM13Explorer::getR(size_t index)
{
  for(size_t i = r_.size(); i <= index; ++i)
    r_.push_back(CNF());
  return r_[index];
}

// -------------------------------------------------------------------------------------------
CNF& IFM13Explorer::getU(size_t index)
{
  for(size_t i = u_.size(); i <= index; ++i)
    u_.push_back(CNF());
  return u_[index];
}

// -------------------------------------------------------------------------------------------
SatSolver* IFM13Explorer::getGotoNextLowerSolver(size_t index)
{
  for(size_t i = goto_next_lower_solvers_.size(); i <= index; ++i)
  {
    goto_next_lower_solvers_.push_back(Options::instance().getSATSolver());
    goto_next_lower_solvers_.back()->startIncrementalSession(sicn_, false);
    goto_next_lower_solvers_.back()->incAddCNF(AIG2CNF::instance().getTrans());
  }
  return goto_next_lower_solvers_[index];
}

// -------------------------------------------------------------------------------------------
SatSolver* IFM13Explorer::getGenBlockTransSolver(size_t index)
{
  for(size_t i = gen_block_trans_solvers_.size(); i <= index; ++i)
  {
    gen_block_trans_solvers_.push_back(Options::instance().getSATSolver());
    gen_block_trans_solvers_.back()->startIncrementalSession(sicn_, false);
    gen_block_trans_solvers_.back()->incAddCNF(AIG2CNF::instance().getTrans());
  }
  return gen_block_trans_solvers_[index];
}

// -------------------------------------------------------------------------------------------
IFMProofObligation IFM13Explorer::popMin(list<IFMProofObligation> &queue)
{
  MASSERT(!queue.empty(), "cannot pop from empty queue");
  list<IFMProofObligation>::iterator min = queue.begin();
  list<IFMProofObligation>::iterator check = queue.begin();
  ++check;
  for(;check != queue.end(); ++check)
  {
    if(check->getLevel() < min->getLevel())
      min = check;
  }
  IFMProofObligation res = *min;
  queue.erase(min);
  return res;
}


