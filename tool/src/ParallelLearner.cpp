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
#include "CNFImplExtractor.h"
#include "QBFSolver.h"
#include "unistd.h"
#include "IFMProofObligation.h"
#include "DepQBFApi.h"
#include "DepQBFExt.h"

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
PrevStateInfo::PrevStateInfo(bool use_ind):
    use_ind_(use_ind),
    current_state_is_initial_(0),
    prev_safe_(0)
{
  if(use_ind_)
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
      current_to_previous_map_[c[v_cnt]] = VM.createFreshTmpVar();
    for(size_t v_cnt = 0; v_cnt < i.size(); ++v_cnt)
      current_to_previous_map_[i[v_cnt]] = VM.createFreshTmpVar();
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
    prev_safe_ = -current_to_previous_map_[VM.getPresErrorStateVar()];

    // begin activation variables for a previous state clause:
    px_unused_.reserve(s.size());
    px_neg_.reserve(s.size());
    px_act_.reserve(s.size() + 1);
    for(size_t cnt = 0; cnt < s.size(); ++cnt)
    {
      px_unused_.push_back(VM.createFreshPrevVar());
      px_neg_.push_back(VM.createFreshPrevVar());
      px_act_.push_back(VM.createFreshPrevVar());
    }
    px_act_.push_back(current_state_is_initial_);
    prev_trans_or_initial_.addClause(px_act_);
    for(size_t cnt = 0; cnt < s.size(); ++cnt)
    {
      int prev = current_to_previous_map_[s[cnt]];
      prev_trans_or_initial_.add2LitClause(-px_unused_[cnt], -px_act_[cnt]);
      prev_trans_or_initial_.add4LitClause(px_unused_[cnt], px_neg_[cnt], prev, -px_act_[cnt]);
      prev_trans_or_initial_.add4LitClause(px_unused_[cnt], px_neg_[cnt], -prev, px_act_[cnt]);
      prev_trans_or_initial_.add4LitClause(px_unused_[cnt], -px_neg_[cnt], prev, px_act_[cnt]);
      prev_trans_or_initial_.add4LitClause(px_unused_[cnt], -px_neg_[cnt], -prev, -px_act_[cnt]);
    }
    // end activation variables for a previous state clause
    prev_vars_ = VM.getVarsOfType(VarInfo::PREV);
  }
}

// -------------------------------------------------------------------------------------------
PrevStateInfo::PrevStateInfo(const PrevStateInfo &other):
    use_ind_(other.use_ind_),
    prev_trans_or_initial_(other.prev_trans_or_initial_),
    current_to_previous_map_(other.current_to_previous_map_),
    current_state_is_initial_(other.current_state_is_initial_),
    prev_safe_(other.prev_safe_),
    px_unused_(other.px_unused_),
    px_neg_(other.px_neg_),
    px_act_(other.px_act_),
    prev_vars_(other.prev_vars_)
{
  // nothing else to do
}

// -------------------------------------------------------------------------------------------
PrevStateInfo& PrevStateInfo::operator=(const PrevStateInfo &other)
{
  use_ind_ = other.use_ind_;
  prev_trans_or_initial_ = other.prev_trans_or_initial_;
  current_to_previous_map_ = other.current_to_previous_map_;
  current_state_is_initial_ = other.current_state_is_initial_;
  prev_safe_ = other.prev_safe_;
  px_unused_ = other.px_unused_;
  px_neg_ = other.px_neg_;
  px_act_ = other.px_act_;
  prev_vars_ = other.prev_vars_;
  return *this;
}

// -------------------------------------------------------------------------------------------
int PrevStateInfo::presentToPrevious(int literal) const
{
  int var = (literal < 0) ? -literal : literal;
  if(literal < 0)
    return -current_to_previous_map_[var];
  else
    return current_to_previous_map_[var];
}

// -------------------------------------------------------------------------------------------
void PrevStateInfo::presentToPrevious(vector<int> &cube_or_clause) const
{
  for(size_t cnt = 0; cnt < cube_or_clause.size(); ++cnt)
    cube_or_clause[cnt] = presentToPrevious(cube_or_clause[cnt]);
}

// -------------------------------------------------------------------------------------------
void PrevStateInfo::presentToPrevious(CNF &cnf) const
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
ParallelLearner::ParallelLearner(size_t nr_of_threads, CNFImplExtractor *impl_extractor) :
                 BackEnd(),
                 psi_(Options::instance().getBackEndMode() == 0),
                 result_(0),
                 nr_of_threads_(nr_of_threads),
                 impl_extractor_(impl_extractor)
{

  MASSERT(nr_of_threads != 0, "Must have at least one thread");
  size_t nr_of_clause_explorers = nr_of_threads;
  size_t nr_of_ifm_explorers = 0;
  size_t nr_of_templ_explorers = 0;
  size_t nr_of_clause_minimizers = 0;
  size_t nr_of_ce_generalizers = 0;

  if(nr_of_threads == 1)
  {
    nr_of_clause_explorers = 1;
    nr_of_ifm_explorers = 0;
    nr_of_templ_explorers = 0;
    nr_of_clause_minimizers = 0;
    nr_of_ce_generalizers = 0;
  }
  if(nr_of_threads == 2)
  {
    nr_of_clause_explorers = 1;
    nr_of_ifm_explorers = 0;
    nr_of_templ_explorers = 1;
    nr_of_clause_minimizers = 0;
    nr_of_ce_generalizers = 0;
  }
  if(nr_of_threads == 3)
  {
    nr_of_clause_explorers = 1;
    nr_of_ifm_explorers = 1;
    nr_of_templ_explorers = 1;
    nr_of_clause_minimizers = 0;
    nr_of_ce_generalizers = 0;
  }
  if(nr_of_threads == 4)
  {
    nr_of_clause_explorers = 2;
    nr_of_ifm_explorers = 1;
    nr_of_templ_explorers = 1;
    nr_of_clause_minimizers = 0;
    nr_of_ce_generalizers = 0;
  }
  if(nr_of_threads == 5)
  {
    nr_of_clause_explorers = 2;
    nr_of_ifm_explorers = 1;
    nr_of_templ_explorers = 1;
    nr_of_clause_minimizers = 0;
    nr_of_ce_generalizers = 1;
  }
  if(nr_of_threads == 6)
  {
    nr_of_clause_explorers = 2;
    nr_of_ifm_explorers = 1;
    nr_of_templ_explorers = 1;
    nr_of_clause_minimizers = 1;
    nr_of_ce_generalizers = 1;
  }
  if(nr_of_threads == 7)
  {
    nr_of_clause_explorers = 3;
    nr_of_ifm_explorers = 1;
    nr_of_templ_explorers = 1;
    nr_of_clause_minimizers = 1;
    nr_of_ce_generalizers = 1;
  }
  if(nr_of_threads == 8)
  {
    nr_of_clause_explorers = 3;
    nr_of_ifm_explorers = 1;
    nr_of_templ_explorers = 1;
    nr_of_clause_minimizers = 2;
    nr_of_ce_generalizers = 1;
  }

  templ_explorers_.reserve(nr_of_templ_explorers);
  for(size_t cnt = 0; cnt < nr_of_templ_explorers; ++cnt)
    templ_explorers_.push_back(new TemplExplorer(*this));

  clause_explorers_.reserve(nr_of_clause_explorers);
  for(size_t cnt = 0; cnt < nr_of_clause_explorers; ++cnt)
    clause_explorers_.push_back(new ClauseExplorerSAT(cnt, *this, psi_));

  ce_generalizers_.reserve(nr_of_ce_generalizers);
  for(size_t cnt = 0; cnt < nr_of_ce_generalizers; ++cnt)
    ce_generalizers_.push_back(new CounterGenSAT(*this, psi_));

  ifm_explorers_.reserve(nr_of_ifm_explorers);
  for(size_t cnt = 0; cnt < nr_of_ifm_explorers; ++cnt)
    ifm_explorers_.push_back(new IFM13Explorer(*this));

  clause_minimizers_.reserve(nr_of_clause_minimizers);
  for(size_t cnt = 0; cnt < nr_of_clause_minimizers; ++cnt)
    clause_minimizers_.push_back(new ClauseMinimizerQBF(cnt, *this, psi_));


  const vector<int> &s = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &i = VarManager::instance().getVarsOfType(VarInfo::INPUT);
  const vector<int> &c = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  vars_to_keep_i_.reserve(s.size() + i.size() + c.size());
  vars_to_keep_i_.insert(vars_to_keep_i_.end(), s.begin(), s.end());
  vars_to_keep_i_.insert(vars_to_keep_i_.end(), i.begin(), i.end());
  vars_to_keep_i_.insert(vars_to_keep_i_.end(), c.begin(), c.end());

  expander_.setAbortCondition(&result_);
}

// -------------------------------------------------------------------------------------------
ParallelLearner::~ParallelLearner()
{
  for(size_t cnt = 0; cnt < templ_explorers_.size(); ++cnt)
    delete templ_explorers_[cnt];
  templ_explorers_.clear();

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

  delete impl_extractor_;
  impl_extractor_ = NULL;
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

  vector<thread> templ_threads;
  templ_threads.reserve(templ_explorers_.size());
  for(size_t cnt = 0; cnt < templ_explorers_.size(); ++cnt)
    templ_threads.push_back(thread(&TemplExplorer::computeWinningRegion,
                                    templ_explorers_[cnt]));

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
  for(size_t cnt = 0; cnt < templ_threads.size(); ++cnt)
    templ_threads[cnt].join();

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

  // cleaning up the winning region explorers to save memory:
  for(size_t cnt = 0; cnt < templ_explorers_.size(); ++cnt)
    delete templ_explorers_[cnt];
  templ_explorers_.clear();
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
  unminimized_clauses_.clear();
  counterexamples_.clear();
  expander_.cleanup();

  L_INF("Starting to extract a circuit ...");
  impl_extractor_->extractCircuit(winning_region_);
  L_INF("Synthesis done.");
  statistics_.logStatistics();
  impl_extractor_->logStatistics();
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

  for(size_t cnt = 0; cnt < clause_minimizers_.size(); ++cnt)
    clause_minimizers_[cnt]->notifyNewWinRegClause(clause, src);

  for(size_t cnt = 0; cnt < ifm_explorers_.size(); ++cnt)
    ifm_explorers_[cnt]->notifyNewWinRegClause(clause, src);

  for(size_t cnt = 0; cnt < templ_explorers_.size(); ++cnt)
    templ_explorers_[cnt]->notifyNewWinRegClause(clause, src);

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
    if(clause_explorers_[cnt]->mode_ == 0)
      clause_explorers_[cnt]->notifyBeforeNewInfo();
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    if(clause_explorers_[cnt]->mode_ == 0)
      clause_explorers_[cnt]->notifyNewUselessInputClause(clause, level);
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    if(clause_explorers_[cnt]->mode_ == 0)
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

  bool some_in_mode0 = false;
  bool some_in_mode1 = false;
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    if(clause_explorers_[cnt]->mode_ == 0)
      some_in_mode0 = true;
    else
      some_in_mode1 = true;

  Utils::compressStateCNF(winning_region_);
  CNF win(winning_region_);
  winning_region_lock_.unlock();

  CNF leave_win;
  if(some_in_mode0)
  {
    VarManager::instance().resetToLastPush();
    leave_win = win;
    leave_win.swapPresentToNext();
    leave_win.negate();
  }

  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
  {
    if(clause_explorers_[cnt]->mode_ == 0)
    {
      SatSolver *next_solver = clause_explorers_[cnt]->getFreshISolver();
      next_solver->startIncrementalSession(vars_to_keep_i_, false);
      next_solver->incAddCNF(win);
      next_solver->incAddCNF(leave_win);
      next_solver->incAddCNF(AIG2CNF::instance().getTrans());
      clause_explorers_[cnt]->notifyRestart(next_solver);
    }
  }
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    if(clause_explorers_[cnt]->mode_ == 0)
      clause_explorers_[cnt]->notifyAfterNewInfo();

  if(some_in_mode1 == false)
  {
    var_man_lock_.unlock();
    return;
  }

  // now we compute the mode1 restarts, which is more expensive:
  // the mode0 explorers can already work in the meantime
  vector<SatSolver*> solvers;
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    if(clause_explorers_[cnt]->mode_ != 0)
      solvers.push_back(clause_explorers_[cnt]->getFreshISolver());
  bool limit_exceeded = expander_.resetSolverIExp(win, solvers, true);
  if(limit_exceeded)
  {
    L_LOG("Memory exceeded: Falling back from mode 1 to 0");
    for(size_t scnt = 0; scnt < solvers.size(); ++scnt)
      delete solvers[scnt];
    if(some_in_mode0 == false)
    {
      VarManager::instance().resetToLastPush();
      leave_win = win;
      leave_win.swapPresentToNext();
      leave_win.negate();
    }
    for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    {
      if(clause_explorers_[cnt]->mode_ != 0)
      {
        SatSolver *next_solver = clause_explorers_[cnt]->getFreshISolver();
        next_solver->startIncrementalSession(vars_to_keep_i_, false);
        next_solver->incAddCNF(win);
        next_solver->incAddCNF(leave_win);
        next_solver->incAddCNF(AIG2CNF::instance().getTrans());
        clause_explorers_[cnt]->notifyRestart(next_solver);
        clause_explorers_[cnt]->mode_ = 0;
        clause_explorers_[cnt]->notifyAfterNewInfo();
      }
    }
  }
  else
  {
    size_t next_solver_idx = 0;
    for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    {
      if(clause_explorers_[cnt]->mode_ != 0)
      {
        clause_explorers_[cnt]->notifyRestart(solvers[next_solver_idx++]);
        clause_explorers_[cnt]->notifyAfterNewInfo();
      }
    }
  }
  var_man_lock_.unlock();
}

// -------------------------------------------------------------------------------------------
void ParallelLearner::triggerInitialMode1Restart()
{
  var_man_lock_.lock();
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    if(clause_explorers_[cnt]->mode_ != 0)
      clause_explorers_[cnt]->notifyBeforeNewInfo();
  vector<SatSolver*> solvers;
  for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    if(clause_explorers_[cnt]->mode_ != 0)
      solvers.push_back(clause_explorers_[cnt]->getFreshISolver());
  CNF win(AIG2CNF::instance().getSafeStates());
  bool limit_exceeded = expander_.resetSolverIExp(win, solvers, true);
  if(limit_exceeded)
  {
    L_LOG("Memory exceeded during initialization: Falling back from mode 1 to 0");
    for(size_t scnt = 0; scnt < solvers.size(); ++scnt)
      delete solvers[scnt];
    for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    {
      if(clause_explorers_[cnt]->mode_ != 0)
      {
        clause_explorers_[cnt]->mode_ = 0;
        clause_explorers_[cnt]->notifyAfterNewInfo();
      }
    }
  }
  else
  {
    size_t next_solver_idx = 0;
    for(size_t cnt = 0; cnt < clause_explorers_.size(); ++cnt)
    {
      if(clause_explorers_[cnt]->mode_ != 0)
      {
        clause_explorers_[cnt]->notifyRestart(solvers[next_solver_idx++]);
        clause_explorers_[cnt]->notifyAfterNewInfo();
      }
    }
  }
  var_man_lock_.unlock();
}


// -------------------------------------------------------------------------------------------
ClauseExplorerSAT::ClauseExplorerSAT(size_t instance_nr,
                                     ParallelLearner &coordinator,
                                     PrevStateInfo &psi):
                   mode_(0),
                   instance_nr_(instance_nr),
                   coordinator_(coordinator),
                   precise_(true),
                   solver_i_(NULL),
                   next_solver_i_(NULL),
                   solver_ctrl_(NULL),
                   vars_to_keep_(VarManager::instance().getAllNonTempVars()),
                   new_useless_input_clauses_level_(0),
                   restart_level_(0),
                   new_restart_level_(0),
                   solver_ctrl_ind_(NULL),
                   psi_(psi),
                   reset_c_cnt_(0),
                   clauses_added_(0)


{
  // introduce some asymmetry to prevent the explorers from doing exactly the same thing:
  if(instance_nr == 0)
  {
    solver_i_name_ = "min_api";
    solver_ctrl_ = new MiniSatApi(false, false);
    solver_ctrl_ind_ = new MiniSatApi(false, false);
    mode_ = 1;
  }
  else if(instance_nr == 1)
  {
    solver_i_name_ = "min_api";
    solver_ctrl_ = new MiniSatApi(false, false);
    solver_ctrl_ind_ = new MiniSatApi(false, false);
    mode_ = 0;
  }
  else if(instance_nr == 2)
  {
    solver_i_name_ = "lin_api";
    solver_ctrl_ = new MiniSatApi(false, false);
    solver_ctrl_ind_ = new MiniSatApi(false, false);
    mode_ = 1;
  }
  else if(instance_nr == 3)
  {
    solver_i_name_ = "pic_api";
    solver_ctrl_ = new MiniSatApi(false, false);
    solver_ctrl_ind_ = new LingelingApi(false, false);
    mode_ = 1;
  }
  else if((instance_nr & 1) == 0)
  {
    solver_i_name_ = "lin_api";
    solver_ctrl_ = new LingelingApi(false, false);
    solver_ctrl_ind_ = new LingelingApi(false, false);
    mode_ = 1;
  }
  else
  {
    solver_i_name_ = "lin_api";
    solver_ctrl_ = new MiniSatApi(false, true);
    solver_ctrl_ind_ = new LingelingApi(false, false);
    mode_ = 1;
  }
  win_ = AIG2CNF::instance().getSafeStates();
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
  delete next_solver_i_;
  next_solver_i_ = NULL;
}

// -------------------------------------------------------------------------------------------
void ClauseExplorerSAT::exploreClauses()
{
  const AIG2CNF& A2C = AIG2CNF::instance();
  const vector<int> &s = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &c = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  const vector<int> &i = VarManager::instance().getVarsOfType(VarInfo::INPUT);
  vector<int> si;
  si.reserve(s.size() + i.size());
  si.insert(si.end(), s.begin(), s.end());
  si.insert(si.end(), i.begin(), i.end());
  vector<int> sic;
  sic.reserve(si.size() + c.size());
  sic.insert(sic.end(), si.begin(), si.end());
  sic.insert(sic.end(), c.begin(), c.end());

  solver_ctrl_->startIncrementalSession(vars_to_keep_, false);
  solver_ctrl_->incAddCNF(A2C.getNextSafeStates());
  solver_ctrl_->incAddCNF(A2C.getTrans());
  solver_ctrl_->incAddCNF(A2C.getSafeStates());

  if(psi_.use_ind_)
  {
    exp_.initSolverCExp(solver_ctrl_ind_, psi_.prev_vars_);
    solver_ctrl_ind_->incAddCNF(A2C.getSafeStates());
    vector<int> safe;
    safe.push_back(-VarManager::instance().getPresErrorStateVar());
    exp_.addExpNxtClauseToC(safe, solver_ctrl_ind_);
    solver_ctrl_ind_->incAddCNF(psi_.prev_trans_or_initial_);
    solver_ctrl_ind_->incAddUnitClause(psi_.prev_safe_);
  }

  // mode 0 can start without waiting for the initialization of the mode1 explorers
  solver_i_ = getFreshISolver();
  solver_i_->startIncrementalSession(sic, false);
  solver_i_->incAddCNF(A2C.getNextUnsafeStates());
  solver_i_->incAddCNF(A2C.getTrans());
  solver_i_->incAddCNF(A2C.getSafeStates());

  if(mode_ == 1)
  {
    // in order to initialize solver_i_, we perform a restart
    bool first_restart_available = waitUntilOngoingRestartDone();
    if(coordinator_.result_ != UNKNOWN)
      return;
    if(first_restart_available == false)
      coordinator_.triggerInitialMode1Restart(); //does not block mode 0 explorers
  }

  precise_ = true;

  size_t it_cnt = 0;
  vector<int> model_or_core;
  while(true)
  {
    ++it_cnt;
    if(coordinator_.result_ != UNKNOWN)
      return;
    considerNewInfoFromOthers();
    statistics_.notifyBeforeComputeCandidate();
    if(it_cnt == 1)
      L_DBG("Explorer " << instance_nr_ << " starts to work.");
    bool sat = solver_i_->incIsSatModelOrCore(vector<int>(), si, model_or_core);
    statistics_.notifyAfterComputeCandidate();
    if(!sat)
    {
      if(precise_)
      {
        coordinator_.result_ = REALIZABLE;
        return;
      }
      if(coordinator_.result_ != UNKNOWN)
        return;
      L_DBG("Explorer " << instance_nr_ << " restarts (iteration " << it_cnt << ").");

      // minor performance optimization:
      // if a restart is ongoing, we do not trigger another one but wait here until it is
      // done. This is not a correctness issue: doing multiple restarts triggered by
      // several threads is also safe (but a waste of resources).
      bool restart_available = waitUntilOngoingRestartDone();
      if(coordinator_.result_ != UNKNOWN)
        return;
      if(restart_available == false)
        coordinator_.triggerExplorerRestart();
      continue;
    }

    if(instance_nr_ != 0)
      Utils::randomize(model_or_core);

    vector<int> state_input = model_or_core;
    vector<int> state = Utils::extract(state_input, VarInfo::PRES_STATE);
    vector<int> input = Utils::extract(state_input, VarInfo::INPUT);

    if(coordinator_.result_ != UNKNOWN)
      return;

    statistics_.notifyBeforeCheckCandidate();
    if(psi_.use_ind_ && mode_ == 1) // race condition does not harm here. Just a performance thing.
      sat = solver_ctrl_ind_->incIsSatModelOrCore(state, input, c, model_or_core);
    else
      sat = solver_ctrl_->incIsSatModelOrCore(state, input, c, model_or_core);

    if(!sat)
    {
      // we now try to minimize the core further:
      bool changed = true;
      while(changed)
      {
        changed = false;
        vector<int> blocking_clause(model_or_core);
        Utils::negateLiterals(blocking_clause);
        if(psi_.use_ind_)
        {
          solver_ctrl_ind_->incAddClause(blocking_clause);
          exp_.addExpNxtClauseToC(blocking_clause, solver_ctrl_ind_);
        }
        else
        {
          vector<int> next_blocking_clause(blocking_clause);
          Utils::swapPresentToNext(next_blocking_clause);
          solver_ctrl_->incAddClause(blocking_clause);
          solver_ctrl_->incAddClause(next_blocking_clause);
        }

        vector<int> orig_core = model_or_core;
        if(instance_nr_ != 0)
          Utils::randomize(orig_core);
        for(size_t lit_cnt = 0; lit_cnt < orig_core.size(); ++lit_cnt)
        {
          vector<int> tmp(model_or_core);
          Utils::remove(tmp, orig_core[lit_cnt]);

          vector<int> assumptions;
          assumptions.reserve(input.size() + tmp.size() + s.size());
          assumptions.insert(assumptions.end(), input.begin(), input.end());
          assumptions.insert(assumptions.end(), tmp.begin(), tmp.end());
          if(psi_.use_ind_)
          {
            // build the previous state-copy of tmp using the activation variables:
            for(size_t s_cnt = 0; s_cnt < s.size(); ++s_cnt)
            {
              if(Utils::contains(tmp, s[s_cnt]))
                assumptions.push_back(psi_.px_neg_[s_cnt]);
              else if(Utils::contains(tmp, -s[s_cnt]))
                assumptions.push_back(-psi_.px_neg_[s_cnt]);
              else
                assumptions.push_back(psi_.px_unused_[s_cnt]);
            }
            sat = solver_ctrl_ind_->incIsSat(assumptions);
          }
          else
            sat = solver_ctrl_->incIsSat(assumptions);
          if(!sat)
          {
            model_or_core = tmp;
            changed = true;
          }
        }
      }

      if(Utils::containsInit(model_or_core))
      {
        coordinator_.result_ = UNREALIZABLE;
        return;
      }

      // We compute the corresponding blocking clause, and update the winning region and the
      // solvers:
      vector<int> blocking_clause(model_or_core);
      Utils::negateLiterals(blocking_clause);
      statistics_.notifyAfterCheckCandidateFound(s.size(), blocking_clause.size());
      coordinator_.notifyNewWinRegClause(blocking_clause, EXPL);
      coordinator_.notifyNewCounterexample(state_input, model_or_core);
      precise_ = false;

    }
    else // sat == true, i.e., this is not a real counterexample
    {
      statistics_.notifyAfterCheckCandidateFailed();
      vector<int> ctrl_cube = model_or_core;
      if(coordinator_.result_ != UNKNOWN) // should be atomic
        return;
      statistics_.notifyBeforeRefine();
      sat = solver_i_->incIsSatModelOrCore(state_input, ctrl_cube, vector<int>(), model_or_core);
      MASSERT(sat == false, "Impossible " << instance_nr_);
      Utils::negateLiterals(model_or_core);
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
  win_.addClauseAndSimplify(clause);
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
void ClauseExplorerSAT::notifyRestart(SatSolver *solver_i)
{
  // if there have been several restarts in the meantime, we only care about the last one:
  delete next_solver_i_;
  next_solver_i_ = solver_i;
  new_win_reg_clauses_for_solver_i_.clear();
  new_foreign_win_reg_clauses_for_solver_i_.clear();
  ++new_restart_level_;
}

// -------------------------------------------------------------------------------------------
SatSolver *ClauseExplorerSAT::getFreshISolver() const
{
  if(solver_i_name_ == "lin_api")
    return new LingelingApi(false, false);
  if(solver_i_name_ == "min_api")
    return new MiniSatApi(false, false);
  if(solver_i_name_ == "pic_api")
    return new PicoSatApi(false, false);
  MASSERT(false, "Unknown SAT solver name.");
  return NULL;
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
  if(next_solver_i_ != NULL)
  {
    statistics_.notifyRestart();
    delete solver_i_;
    solver_i_ = next_solver_i_;
    next_solver_i_ = NULL;
    restart_level_ = new_restart_level_;
    precise_ = true;
  }
  if(new_win_reg_clauses_for_solver_ctrl_.getNrOfClauses() > 0)
  {
    if(clauses_added_ > win_.getNrOfClauses() + 100)
    {
      // reset solver_ctrl_ and solver_ctrl_ind_:
      if(reset_c_cnt_ % 1000 == 999)
        Utils::compressStateCNF(win_, true);
      else if(reset_c_cnt_ % 100 == 99)
        Utils::compressStateCNF(win_, false);
      CNF next_win(win_);
      next_win.swapPresentToNext();
      solver_ctrl_->startIncrementalSession(vars_to_keep_, false);
      solver_ctrl_->incAddCNF(win_);
      solver_ctrl_->incAddCNF(AIG2CNF::instance().getTrans());
      solver_ctrl_->incAddCNF(next_win);
      if(psi_.use_ind_)
      {
        CNF prev_win(win_);
        psi_.presentToPrevious(prev_win);
        exp_.resetSolverCExp(solver_ctrl_ind_);
        solver_ctrl_ind_->incAddCNF(win_);
        const list<vector<int> > &cl = win_.getClauses();
        for(CNF::ClauseConstIter it = cl.begin(); it != cl.end(); ++it)
          exp_.addExpNxtClauseToC(*it, solver_ctrl_ind_);
        solver_ctrl_ind_->incAddCNF(psi_.prev_trans_or_initial_);
        solver_ctrl_ind_->incAddCNF(prev_win);
        clauses_added_ = win_.getNrOfClauses();
      }
      new_win_reg_clauses_for_solver_ctrl_.clear();
      reset_c_cnt_++;
    }
    else
    {
      CNF next_win_reg_clauses(new_win_reg_clauses_for_solver_ctrl_);
      next_win_reg_clauses.swapPresentToNext();
      solver_ctrl_->incAddCNF(new_win_reg_clauses_for_solver_ctrl_);
      solver_ctrl_->incAddCNF(next_win_reg_clauses);
      if(psi_.use_ind_)
      {
        CNF prev_win_reg_clauses(new_win_reg_clauses_for_solver_ctrl_);
        psi_.presentToPrevious(prev_win_reg_clauses);
        solver_ctrl_ind_->incAddCNF(new_win_reg_clauses_for_solver_ctrl_);
        const list<vector<int> > &cl = new_win_reg_clauses_for_solver_ctrl_.getClauses();
        for(CNF::ClauseConstIter it = cl.begin(); it != cl.end(); ++it)
          exp_.addExpNxtClauseToC(*it, solver_ctrl_ind_);
        solver_ctrl_ind_->incAddCNF(prev_win_reg_clauses);
      }
      clauses_added_ += new_win_reg_clauses_for_solver_ctrl_.getNrOfClauses();
      new_win_reg_clauses_for_solver_ctrl_.clear();
    }
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

  if(mode_ == 0 && restart_level_ == new_useless_input_clauses_level_)
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
  bool restart_available = next_solver_i_ != 0;
  new_info_lock_.unlock();
  return restart_available;
}

// -------------------------------------------------------------------------------------------
CounterGenSAT::CounterGenSAT(ParallelLearner &coordinator, PrevStateInfo &psi):
         coordinator_(coordinator),
         vars_to_keep_(VarManager::instance().getAllNonTempVars()),
         solver_ctrl_(Options::instance().getSATSolver(false, false)),
         solver_win_(Options::instance().getSATSolver(false, true)),
         solver_ctrl_ind_(Options::instance().getSATSolver(false, true)),
         next_bored_index_(0),
         last_bored_compress_size_(1),
         psi_(psi),
         s_(VarManager::instance().getVarsOfType(VarInfo::PRES_STATE))
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

  if(psi_.use_ind_)
  {
    solver_ctrl_ind_->startIncrementalSession(vars_to_keep_, true);
    solver_ctrl_ind_->incAddCNF(A2C.getNextSafeStates());
    solver_ctrl_ind_->incAddCNF(A2C.getTrans());
    solver_ctrl_ind_->incAddCNF(A2C.getSafeStates());
    solver_ctrl_ind_->incAddCNF(psi_.prev_trans_or_initial_);
    solver_ctrl_ind_->incAddUnitClause(psi_.prev_safe_);
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
  if(psi_.use_ind_)
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
  if(psi_.use_ind_)
  {

    vector<int> blocking_clause(core);
    Utils::negateLiterals(blocking_clause);
    if(psi_.use_ind_)
    {
      solver_ctrl_ind_->incAddClause(blocking_clause);
      vector<int> next_blocking_clause(blocking_clause);
      Utils::swapPresentToNext(next_blocking_clause);
      solver_ctrl_ind_->incAddClause(next_blocking_clause);
    }
    else
    {
      vector<int> next_blocking_clause(blocking_clause);
      Utils::swapPresentToNext(next_blocking_clause);
      solver_ctrl_->incAddClause(blocking_clause);
      solver_ctrl_->incAddClause(next_blocking_clause);
    }
    for(size_t lit_cnt = 0; lit_cnt < orig_core.size(); ++lit_cnt)
    {
      vector<int> tmp(core);
      Utils::remove(tmp, orig_core[lit_cnt]);

      vector<int> assumptions;
      assumptions.reserve(in_cube.size() + tmp.size() + s_.size());
      assumptions.insert(assumptions.end(), in_cube.begin(), in_cube.end());
      assumptions.insert(assumptions.end(), tmp.begin(), tmp.end());

      // build the previous state-copy of tmp using the activation variables:
      for(size_t s_cnt = 0; s_cnt < s_.size(); ++s_cnt)
      {
        if(Utils::contains(tmp, s_[s_cnt]))
          assumptions.push_back(psi_.px_neg_[s_cnt]);
        else if(Utils::contains(tmp, -s_[s_cnt]))
          assumptions.push_back(-psi_.px_neg_[s_cnt]);
        else
          assumptions.push_back(psi_.px_unused_[s_cnt]);
      }
      if(!solver_ctrl_ind_->incIsSat(assumptions))
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
    if(psi_.use_ind_)
    {
      CNF prev_win_reg_clauses(new_win_reg_clauses_);
      psi_.presentToPrevious(prev_win_reg_clauses);
      solver_ctrl_ind_->incAddCNF(new_win_reg_clauses_);
      solver_ctrl_ind_->incAddCNF(next_win_reg_clauses);
      solver_ctrl_ind_->incAddCNF(prev_win_reg_clauses);
    }
    new_win_reg_clauses_.clear();
  }
  new_win_reg_clauses_lock_.unlock();
}

// -------------------------------------------------------------------------------------------
ClauseMinimizerQBF::ClauseMinimizerQBF(size_t instance_nr, ParallelLearner &coordinator,
                                       PrevStateInfo &psi):
                   coordinator_(coordinator),
                   qbf_solver_(Options::instance().getQBFSolver()),
                   inc_qbf_solver_(NULL),
                   sat_solver_(Options::instance().getSATSolver(false, false)),
                   psi_(psi)

{
  const vector<int> &prev = VarManager::instance().getVarsOfType(VarInfo::PREV);
  const vector<int> &ps = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &i = VarManager::instance().getVarsOfType(VarInfo::INPUT);
  const vector<int> &c = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  const vector<int> &ns = VarManager::instance().getVarsOfType(VarInfo::NEXT_STATE);
  const vector<int> &t = VarManager::instance().getVarsOfType(VarInfo::TMP);

  sat_solver_->startIncrementalSession(ps, false);

  if(psi_.use_ind_)
    gen_quant_.push_back(make_pair(prev, QBFSolver::E));
  gen_quant_.push_back(make_pair(ps, QBFSolver::E));
  gen_quant_.push_back(make_pair(i, QBFSolver::A));
  gen_quant_.push_back(make_pair(c, QBFSolver::E));
  gen_quant_.push_back(make_pair(ns, QBFSolver::E));
  gen_quant_.push_back(make_pair(t, QBFSolver::E));

  if(instance_nr != 0)
    inc_qbf_solver_ = new DepQBFApi(false);
}


// -------------------------------------------------------------------------------------------
ClauseMinimizerQBF::~ClauseMinimizerQBF()
{
  delete qbf_solver_;
  qbf_solver_ = NULL;

  delete inc_qbf_solver_;
  inc_qbf_solver_ = NULL;

  delete sat_solver_;
  sat_solver_ = NULL;
}

// -------------------------------------------------------------------------------------------
void ClauseMinimizerQBF::minimizeClauses()
{
  if(inc_qbf_solver_ == NULL)
    minimizeClausesNoInc();
  else
    minimizeClausesInc();
}

// -------------------------------------------------------------------------------------------
void ClauseMinimizerQBF::notifyNewWinRegClause(const vector<int> &clause, int src)
{
  new_win_reg_clauses_lock_.lock();
  new_win_reg_clauses_.addClause(clause);
  new_win_reg_clauses_lock_.unlock();
}

// -------------------------------------------------------------------------------------------
void ClauseMinimizerQBF::minimizeClausesInc()
{
  // initialize the solver:
  inc_qbf_solver_->startIncrementalSession(gen_quant_);
  inc_qbf_solver_->doMinCores(true);
  inc_qbf_solver_->incAddCNF(AIG2CNF::instance().getSafeStates());
  inc_qbf_solver_->incAddCNF(AIG2CNF::instance().getTrans());
  inc_qbf_solver_->incAddCNF(AIG2CNF::instance().getNextSafeStates());
  if(psi_.use_ind_)
  {
    inc_qbf_solver_->incAddCNF(psi_.prev_trans_or_initial_);
    inc_qbf_solver_->incAddUnitClause(psi_.prev_safe_);
  }
  sat_solver_->incAddCNF(AIG2CNF::instance().getSafeStates());

  vector<int> orig;

  while(true)
  {
    if(coordinator_.result_ != UNKNOWN) // should be atomic
      return;

    // fetch a new clause to minimize further:
    coordinator_.unminimized_clauses_lock_.lock();
    bool empty = coordinator_.unminimized_clauses_.getNrOfClauses() == 0;
    if(!empty)
      orig = coordinator_.unminimized_clauses_.removeSomeClause();
    coordinator_.unminimized_clauses_lock_.unlock();
    if(empty)
    {
      usleep(100000); // microseconds
      continue;
    }

    // consider new winning region clauses:
    new_win_reg_clauses_lock_.lock();
    sat_solver_->incAddCNF(new_win_reg_clauses_);
    inc_qbf_solver_->incAddCNF(new_win_reg_clauses_);
    if(psi_.use_ind_)
    {
      CNF prev_win_reg_clauses(new_win_reg_clauses_);
      psi_.presentToPrevious(prev_win_reg_clauses);
      inc_qbf_solver_->incAddCNF(prev_win_reg_clauses);
    }
    new_win_reg_clauses_.swapPresentToNext();
    inc_qbf_solver_->incAddCNF(new_win_reg_clauses_);
    new_win_reg_clauses_.clear();
    new_win_reg_clauses_lock_.unlock();

    // do the minimization:
    Utils::randomize(orig);
    vector<int> ce_cube(orig);
    Utils::negateLiterals(ce_cube);
    vector<int> min_ce_cube(ce_cube);
    if(psi_.use_ind_)
    {
      for(size_t lit_cnt = 0; lit_cnt < ce_cube.size(); ++lit_cnt)
      {
        vector<int> tmp(min_ce_cube);
        Utils::remove(tmp, ce_cube[lit_cnt]);

        // use what we already know immediately?
        //inc_qbf_solver_->incPush();
        //vector<int> prev_cube(min_ce_cube);
        //presentToPrevious(prev_cube);
        //vector<int> next_cube(min_ce_cube);
        //Utils::swapPresentToNext(next_cube);
        //inc_qbf_solver_->incAddNegCubeAsClause(min_ce_cube);
        //inc_qbf_solver_->incAddNegCubeAsClause(prev_cube);
        //inc_qbf_solver_->incAddNegCubeAsClause(next_cube);

        // now the interesting part:
        //vector<int> prev_tmp(tmp);
        //presentToPrevious(prev_tmp);
        //prev_tmp.push_back(-current_state_is_initial_);
        //inc_qbf_solver_->incAddNegCubeAsClause(prev_tmp);
        bool sat = inc_qbf_solver_->incIsSat(tmp);
        //inc_qbf_solver_->incPop();
        if(!sat)
          min_ce_cube = tmp;
      }
    }
    else
    {
      // TODO: make a loop here also. This way we waste one check because it
      // with all assumptions, it must be unsat.
      bool sat = inc_qbf_solver_->incIsSatCore(ce_cube, min_ce_cube);
      MASSERT(!sat, "Impossible.");
    }

    // communicate back the result:
    if(orig.size() > min_ce_cube.size())
    {
      if(Utils::containsInit(min_ce_cube))
      {
        coordinator_.result_ = UNREALIZABLE;  // should be atomic
        return;
      }
      // check if the smaller clause is already implied by what we have:
      if(sat_solver_->incIsSat(min_ce_cube))
      {
        vector<int> min_clause(min_ce_cube);
        Utils::negateLiterals(min_clause);
        coordinator_.notifyNewWinRegClause(min_clause, MIN);
      }
    }
  }
}

// -------------------------------------------------------------------------------------------
void ClauseMinimizerQBF::minimizeClausesNoInc()
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
    if(psi_.use_ind_)
    {
      generalize_clause_cnf.addCNF(psi_.prev_trans_or_initial_);
      CNF prev_win(win_reg);
      psi_.presentToPrevious(prev_win);
      generalize_clause_cnf.addCNF(prev_win);
    }

    vector<int> smallest_so_far(orig);
    for(size_t cnt = 0; cnt < orig.size(); ++cnt)
    {
      vector<int> tmp(smallest_so_far);
      Utils::remove(tmp, orig[cnt]);
      CNF check_cnf(generalize_clause_cnf);
      check_cnf.addNegClauseAsCube(tmp);
      if(psi_.use_ind_)
      {
        vector<int> prev_tmp(tmp);
        psi_.presentToPrevious(prev_tmp);
        prev_tmp.push_back(psi_.current_state_is_initial_);
        check_cnf.addClause(prev_tmp);
      }
      // begin use what we already have:
      check_cnf.addClause(smallest_so_far);
      vector<int> next_smallest_so_far(smallest_so_far);
      Utils::swapPresentToNext(next_smallest_so_far);
      check_cnf.addClause(next_smallest_so_far);
      // end use what we already have:
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
        coordinator_.notifyNewWinRegClause(smallest_so_far, MIN);
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

      if(coordinator_.result_ == UNKNOWN)
      {
        L_LOG("IFM: Found two equal clause sets: R" << (equal-1) << " and R" << equal);
        coordinator_.result_ = REALIZABLE;
        CNF winreg = getR(equal);
        Utils::negateStateCNF(winreg);
        coordinator_.winning_region_lock_.lock();
        coordinator_.winning_region_ = winreg;
        coordinator_.winning_region_lock_.unlock();
      }

      //L_LOG("Nr of iterations: " << k);
      return;
    }
    ++k;
    if(k > 1024)
      return; // with too many solver instances we risk running out of memory
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
  // coordinator_.notifyNewWinRegClause(blocking_clause, IFM);
  // this thread will get and consider the new clause via the notification mechanism also.
  win_.addClauseAndSimplify(blocking_clause);
  Utils::swapPresentToNext(blocking_clause);
  goto_win_solver_->incAddClause(blocking_clause);
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

// -------------------------------------------------------------------------------------------
TemplExplorer::TemplExplorer(ParallelLearner &coordinator) :
    coordinator_(coordinator),
    s_(VarManager::instance().getVarsOfType(VarInfo::PRES_STATE)),
    i_(VarManager::instance().getVarsOfType(VarInfo::INPUT)),
    c_(VarManager::instance().getVarsOfType(VarInfo::CTRL)),
    n_(VarManager::instance().getVarsOfType(VarInfo::NEXT_STATE)),
    orig_tmp_(VarManager::instance().getVarsOfType(VarInfo::TMP)),
    orig_next_free_var_(VarManager::instance().getMaxCNFVar() + 1),
    next_free_var_(VarManager::instance().getMaxCNFVar() + 1),
    p_err_(VarManager::instance().getPresErrorStateVar()),
    n_err_(VarManager::instance().getNextErrorStateVar())
{
  int max_s = 0;
  for(size_t cnt = 0; cnt < s_.size(); ++cnt)
    if(s_[cnt] > max_s)
      max_s = s_[cnt];
  pres_to_nxt_.reserve(max_s + 1);
  for(int cnt = 0; cnt < max_s + 1; ++cnt)
    pres_to_nxt_.push_back(cnt);
  for(size_t cnt = 0; cnt < s_.size(); ++cnt)
    pres_to_nxt_[s_[cnt]] = n_[cnt];
}

// -------------------------------------------------------------------------------------------
TemplExplorer::~TemplExplorer()
{
  // nothing to be done
}

// -------------------------------------------------------------------------------------------
void TemplExplorer::computeWinningRegion()
{
  size_t nr_of_clauses = 1;
  size_t to = 20;
  int found = 0;
  while(found != 1)
  {
    if(coordinator_.result_ != UNKNOWN)
      return;
    found = findWinRegCNFTempl(nr_of_clauses, to, true);
    if(found == 0)
      nr_of_clauses++;
    else if (found == 2) // time-out
    {
      nr_of_clauses = 1;
      if(coordinator_.result_ != UNKNOWN)
        return;
      found = findWinRegCNFTempl(nr_of_clauses, to, false);
      if(found == 0)
        nr_of_clauses++;
      else if(found == 2) // time-out
        nr_of_clauses = 1;
    }
  }
  if(coordinator_.result_ == UNKNOWN)
  {
    L_LOG("Template-engine found the solution.");
    coordinator_.result_ = REALIZABLE;
    coordinator_.winning_region_lock_.lock();
    coordinator_.winning_region_ = final_winning_region_;
    coordinator_.winning_region_lock_.unlock();
  }
}

// -------------------------------------------------------------------------------------------
void TemplExplorer::notifyNewWinRegClause(const vector<int> &clause, int src)
{
  known_clauses_lock_.lock();
  known_clauses_.addClause(clause);
  known_clauses_lock_.unlock();
}

// -------------------------------------------------------------------------------------------
int TemplExplorer::findWinRegCNFTempl(size_t nr_of_clauses, size_t timeout, bool use_sat)
{
  // Note: a lot of loops and code could be merged, but readability
  // beats performance in this case.

  known_clauses_lock_.lock();
  CNF known_clauses(known_clauses_);
  known_clauses_lock_.unlock();
  // eliminate all temporary variables that have been introduced previously:
  current_tmp_ = orig_tmp_;
  next_free_var_ = orig_next_free_var_;
  templ_.clear();

  vector<int> ps_vars = s_;
  vector<int> ns_vars = n_;

  // ps_vars[0] and ns_vars[0] are the error bits, which must be FALSE always.
  // Hence, we do not include these signals in our template
  ps_vars[0] = ps_vars[ps_vars.size() - 1];
  ps_vars.pop_back();
  ns_vars[0] = ns_vars[ns_vars.size() - 1];
  ns_vars.pop_back();
  size_t nr_of_vars = ps_vars.size();

  // Step 1: build the parameterized CNF:
  CNF win_constr;
  int w1 = newTmp();
  int w2 = newTmp();

  // Step 1a: create template parameters:
  vector<int> clause_active_vars;
  vector<vector<int> > contains_vars;
  vector<vector<int> > negated_vars;
  clause_active_vars.reserve(nr_of_clauses);
  contains_vars.reserve(nr_of_clauses);
  negated_vars.reserve(nr_of_clauses);
  for(size_t c_cnt = 0; c_cnt < nr_of_clauses; ++c_cnt)
  {
    clause_active_vars.push_back(newParam());
    contains_vars.push_back(vector<int>());
    negated_vars.push_back(vector<int>());
    for(size_t v_cnt = 0; v_cnt < nr_of_vars; ++v_cnt)
    {
      contains_vars[c_cnt].push_back(newParam());
      negated_vars[c_cnt].push_back(newParam());
    }
  }



  // Step 1b: constructing the constraints:
  // We do this for next_win_eq_w2 and win_eq_w1 in parallel
  vector<int> all_clause_lits1;
  vector<int> all_clause_lits2;
  all_clause_lits1.reserve(known_clauses.getNrOfClauses() + nr_of_clauses + 1);
  all_clause_lits2.reserve(known_clauses.getNrOfClauses() + nr_of_clauses + 1);
  // We add one fixed clause, namely that we are inside the safe states:
  all_clause_lits1.push_back(-p_err_);
  all_clause_lits2.push_back(-n_err_);


  // begin: constraints for existing clauses
  Utils::compressStateCNF(known_clauses, true);
  const list<vector<int> > &known_list = known_clauses.getClauses();
  for(CNF::ClauseConstIter it = known_list.begin(); it != known_list.end(); ++it)
  {
    vector<int> cl1 = *it;
    vector<int> cl2 = *it;
    swapPresentToNext(cl2);
    int cl1_true = newTmp();
    int cl2_true = newTmp();
    for(size_t lcnt = 0; lcnt < cl1.size(); ++lcnt)
    {
      // literal is true --> clause is true
      win_constr.add2LitClause(-cl1[lcnt], cl1_true);
      win_constr.add2LitClause(-cl2[lcnt], cl2_true);
    }
    // all literals false --> clause is false
    cl1.push_back(-cl1_true);
    win_constr.addClause(cl1);
    cl2.push_back(-cl2_true);
    win_constr.addClause(cl2);
    all_clause_lits1.push_back(cl1_true);
    all_clause_lits2.push_back(cl2_true);
  }
  // end: constraints for existing clauses


  for(size_t c = 0; c < nr_of_clauses; ++c)
  {
    int clause_lit1 = newTmp();  // TRUE iff clause c is TRUE
    int clause_lit2 = newTmp();  // TRUE iff clause c is TRUE
    all_clause_lits1.push_back(clause_lit1);
    all_clause_lits2.push_back(clause_lit2);
    vector<int> literals_in_clause1;
    vector<int> literals_in_clause2;
    literals_in_clause1.reserve(nr_of_vars + 2);
    literals_in_clause2.reserve(nr_of_vars + 2);
    // if clause_active_var[c]=FALSE, then the clause should be TRUE
    // so we add -clause_active_vars[c] as if it was a literal of this clause
    literals_in_clause1.push_back(-clause_active_vars[c]);
    literals_in_clause2.push_back(-clause_active_vars[c]);
    for(size_t v = 0; v < nr_of_vars; ++v)
    {
      int activated_var1 = newTmp();
      int activated_var2 = newTmp();
      literals_in_clause1.push_back(activated_var1);
      literals_in_clause2.push_back(activated_var2);
      // Part A: activated_var should be equal to
      // - FALSE        if (contains_var[c][v]==FALSE)
      // - not(var[v])  if (contains_var[c][v]==TRUE and negated_var[c][v]==TRUE)
      // - var[v]       if (contains_var[c][v]==TRUE and negated_var[c][v]==FALSE)
      // Part A1: not(contains_vars[c][v]) implies not(activated_var)
      win_constr.add2LitClause(contains_vars[c][v], -activated_var1);
      win_constr.add2LitClause(contains_vars[c][v], -activated_var2);
      // Part A2: (contains_var[c][v] and negated_var[c][v]) implies (activated_var <=> not(var[v]))
      win_constr.add4LitClause(-contains_vars[c][v], -negated_vars[c][v], ps_vars[v], activated_var1);
      win_constr.add4LitClause(-contains_vars[c][v], -negated_vars[c][v], -ps_vars[v], -activated_var1);
      win_constr.add4LitClause(-contains_vars[c][v], -negated_vars[c][v], ns_vars[v], activated_var2);
      win_constr.add4LitClause(-contains_vars[c][v], -negated_vars[c][v], -ns_vars[v], -activated_var2);
      // Part A3: (contains_var[c][v] and not(negated_var[c][v])) implies (activated_var <=> var[v])
      win_constr.add4LitClause(-contains_vars[c][v], negated_vars[c][v], -ps_vars[v], activated_var1);
      win_constr.add4LitClause(-contains_vars[c][v], negated_vars[c][v], ps_vars[v], -activated_var1);
      win_constr.add4LitClause(-contains_vars[c][v], negated_vars[c][v], -ns_vars[v], activated_var2);
      win_constr.add4LitClause(-contains_vars[c][v], negated_vars[c][v], ns_vars[v], -activated_var2);
    }
    // Part B: clause_lit should be equal to OR(literals_in_clause):
    // Part B1: if one literal in literals_in_clause is TRUE then clause_lit should be TRUE
    for(size_t cnt = 0; cnt < literals_in_clause1.size(); ++cnt)
    {
      win_constr.add2LitClause(-literals_in_clause1[cnt], clause_lit1);
      win_constr.add2LitClause(-literals_in_clause2[cnt], clause_lit2);
    }
    // Part B2: if all literals in literals_in_clause are FALSE, then the clause_lit should be FALSE
    // That is, we add the clause [l1, l2, l3, l4, -clause_lit]
    literals_in_clause1.push_back(-clause_lit1);
    win_constr.addClause(literals_in_clause1);
    literals_in_clause2.push_back(-clause_lit2);
    win_constr.addClause(literals_in_clause2);
  }
  // Part C: w should be equal to AND(all_clause_lits)
  // Part C1: if one literal in all_clause_lits is FALSE then w should be FALSE:
  for(size_t cnt = 0; cnt < all_clause_lits1.size(); ++cnt)
  {
    win_constr.add2LitClause(all_clause_lits1[cnt], -w1);
    win_constr.add2LitClause(all_clause_lits2[cnt], -w2);
  }
  // Part C2: if all literals in all_clause_lits are TRUE, the w should be TRUE:
  // That is, we create a clause [-l1, -l2, -l3, -l4, w]
  for(size_t cnt = 0; cnt < all_clause_lits1.size(); ++cnt)
  {
    all_clause_lits1[cnt] = -all_clause_lits1[cnt];
    all_clause_lits2[cnt] = -all_clause_lits2[cnt];
  }
  all_clause_lits1.push_back(w1);
  all_clause_lits2.push_back(w2);
  win_constr.addClause(all_clause_lits1);
  win_constr.addClause(all_clause_lits2);


  // Step 2: solve
  vector<int> model;
  int sat = 0;
  if(use_sat)
    sat = syntSAT(win_constr, w1, w2, model, timeout);
  else
    sat = syntQBF(win_constr, w1, w2, model, timeout);
  if(sat == 0 || sat == 2)
    return sat;

  // Step 3: Build a CNF for the winning region:
  final_winning_region_.addCNF(known_clauses);
  final_winning_region_.add1LitClause(-p_err_);
  vector<bool> can_be_0(next_free_var_, true);
  vector<bool> can_be_1(next_free_var_, true);
  for(size_t v_cnt = 0; v_cnt < model.size(); ++v_cnt)
  {
    if(model[v_cnt] < 0)
      can_be_1[-model[v_cnt]] = false;
    else
      can_be_0[model[v_cnt]] = false;
  }
  for(size_t c_cnt = 0; c_cnt < nr_of_clauses; ++c_cnt)
  {
    if(can_be_0[clause_active_vars[c_cnt]])
      continue;
    vector<int> clause;
    clause.reserve(nr_of_vars);
    for(size_t v_cnt = 0; v_cnt < nr_of_vars; ++v_cnt)
    {
      if(can_be_0[contains_vars[c_cnt][v_cnt]])
        continue;
      if(can_be_0[negated_vars[c_cnt][v_cnt]])
        clause.push_back(ps_vars[v_cnt]);
      else
        clause.push_back(-ps_vars[v_cnt]);
    }
    final_winning_region_.addClause(clause);
  }
  return 1;
}

// -------------------------------------------------------------------------------------------
int TemplExplorer::syntQBF(const CNF& win_constr, int w1, int w2, vector<int> &solution,
                            size_t timeout)
{
  AIG2CNF& A2C = AIG2CNF::instance();
  CNF query(A2C.getTransEqT());
  query.addCNF(win_constr);
  vector<int> init_implies_win;
  init_implies_win.reserve(s_.size() + 1);
  for(size_t cnt = 0; cnt < s_.size(); ++cnt)
    init_implies_win.push_back(s_[cnt]);
  init_implies_win.push_back(w1);
  query.addClause(init_implies_win);
  // from win, the system can enforce to stay in win
  query.add2LitClause(-w1, A2C.getT());
  query.add2LitClause(-w1, w2);

  // build the quantifier prefix:
  vector<pair<vector<int>, QBFSolver::Quant> > quant;
  quant.push_back(make_pair(templ_, QBFSolver::E));
  quant.push_back(make_pair(s_, QBFSolver::A));
  quant.push_back(make_pair(i_, QBFSolver::A));
  quant.push_back(make_pair(c_, QBFSolver::E));
  quant.push_back(make_pair(n_, QBFSolver::E));
  quant.push_back(make_pair(current_tmp_, QBFSolver::E));

  DepQBFExt solver(true, timeout);
  //DepQBFApi solver(true);
  bool sat = false;
  try
  {
    sat = solver.isSatModel(quant, query, solution);
  }
  catch(DemiurgeException e) // time-out
  {
    return 2;
  }
  if(sat)
    return 1;
  return 0;
}

// -------------------------------------------------------------------------------------------
int TemplExplorer::syntSAT(const CNF& win_constr, int w1, int w2, vector<int> &solution,
                           size_t timeout)
{
  AIG2CNF& A2C = AIG2CNF::instance();
  CNF gen(A2C.getTrans());
  gen.addCNF(win_constr);
  vector<int> init_implies_win;
  init_implies_win.reserve(s_.size() + 1);
  for(size_t cnt = 0; cnt < s_.size(); ++cnt)
    init_implies_win.push_back(s_[cnt]);
  init_implies_win.push_back(w1);
  gen.addClause(init_implies_win);
  // from win, the system can enforce to stay in win
  gen.add2LitClause(-w1, w2);


  vector<int> none;
  SatSolver *compute_solver = Options::instance().getSATSolver(false, false);
  compute_solver->startIncrementalSession(templ_, false);

  // let's fix correctness for the initial state right away:
  vector<int> initial_state;
  initial_state.reserve(s_.size() + i_.size());
  initial_state.insert(initial_state.end(), s_.begin(), s_.end());
  initial_state.insert(initial_state.end(), i_.begin(), i_.end());
  Utils::negateLiterals(initial_state);
  exclude(initial_state, gen, compute_solver);

  start_ = Stopwatch::start();

  size_t cnt = 0;
  while(true)
  {

    if(Stopwatch::getRealTimeSec(start_) > timeout || coordinator_.result_ != UNKNOWN)
    {
      delete compute_solver;
      return 2;
    }

    vector<int> candidate;
    ++cnt;
    bool sat = compute_solver->incIsSatModelOrCore(none, templ_, candidate);
    if(!sat)
    {
      delete compute_solver;
      return 0;
    }
    if(Stopwatch::getRealTimeSec(start_) > timeout || coordinator_.result_ != UNKNOWN)
    {
      delete compute_solver;
      return 2;
    }

    vector<int> counterexample;
    int correct = check(candidate, win_constr, w1, w2, timeout, counterexample);
    if(correct == 1)
    {
      delete compute_solver;
      solution = candidate;
      return 1;
    }
    exclude(counterexample, gen, compute_solver);
  }
}

// -------------------------------------------------------------------------------------------
int TemplExplorer::check(const vector<int> &cand, const CNF& win_constr, int w1, int w2,
                         size_t timeout, vector<int> &ce)
{
  AIG2CNF& A2C = AIG2CNF::instance();
  vector<int> sic;
  sic.reserve(s_.size() + i_.size() + c_.size());
  sic.insert(sic.end(), s_.begin(), s_.end());
  sic.insert(sic.end(), i_.begin(), i_.end());
  sic.insert(sic.end(), c_.begin(), c_.end());
  vector<int> si;
  si.reserve(s_.size() + i_.size());
  si.insert(si.end(), s_.begin(), s_.end());
  si.insert(si.end(), i_.begin(), i_.end());

  CNF check_cnf(win_constr);
  for(size_t cnt = 0; cnt < cand.size(); ++cnt)
    check_cnf.setVarValue(cand[cnt], true);
  check_cnf.addCNF(A2C.getTrans());
  check_cnf.add1LitClause(w1);
  CNF gen_cnf(check_cnf);
  gen_cnf.add1LitClause(w2);
  check_cnf.add1LitClause(-w2);


  SatSolver *check_solver = Options::instance().getSATSolver(false, true);
  check_solver->startIncrementalSession(sic, false);
  check_solver->incAddCNF(check_cnf);
  SatSolver *gen_solver = Options::instance().getSATSolver(false, true);
  gen_solver->startIncrementalSession(sic, false);
  gen_solver->incAddCNF(gen_cnf);

  vector<int> model_or_core;
  while(true)
  {
    if(Stopwatch::getRealTimeSec(start_) > timeout || coordinator_.result_ != UNKNOWN)
    {
      delete check_solver;
      delete gen_solver;
      return 2;
    }
    bool sat = check_solver->incIsSatModelOrCore(vector<int>(), si, model_or_core);
    if(!sat)
    {
      delete check_solver;
      delete gen_solver;
      return 1;
    }
    if(Stopwatch::getRealTimeSec(start_) > timeout || coordinator_.result_ != UNKNOWN)
    {
      delete check_solver;
      delete gen_solver;
      return 2;
    }
    vector<int> state_input = model_or_core;
    sat = gen_solver->incIsSatModelOrCore(vector<int>(), state_input, c_, model_or_core);
    if(!sat)
    {
      // randomization does not help:
      //for(size_t cnt = 0; cnt < state_input.size(); ++cnt)
      //{
      //  if(rand() & 1)
      //  {
      //    vector<int> ran(state_input);
      //    ran[cnt] = -ran[cnt];
      //    if(!gen_solver->incIsSatModelOrCore(vector<int>(), ran, vector<int>(), model_or_core))
      //      state_input = ran;
      //  }
      //}

      delete check_solver;
      delete gen_solver;
      ce = state_input;
      return 0;
    }

    if(Stopwatch::getRealTimeSec(start_) > timeout || coordinator_.result_ != UNKNOWN)
    {
      delete check_solver;
      delete gen_solver;
      return 2;
    }
    vector<int> resp(model_or_core);
    sat = check_solver->incIsSatModelOrCore(state_input, resp, vector<int>(), model_or_core);
    MASSERT(sat == false, "Impossible.");
    check_solver->incAddNegCubeAsClause(model_or_core);
  }
}

// -------------------------------------------------------------------------------------------
void TemplExplorer::exclude(const vector<int> &ce, const CNF &gen, SatSolver* solver)
{
  CNF to_add(gen);
  for(size_t cnt = 0; cnt < ce.size(); ++cnt)
    to_add.setVarValue(ce[cnt], true);

  // now we need to rename everything except for the template parameters:
  set<int> var_set;
  to_add.appendVarsTo(var_set);
  int max_idx = 0;
  if(!var_set.empty())
    max_idx = *var_set.rbegin();
  vector<int> rename_map;
  rename_map.reserve(max_idx + 1);
  for(int cnt = 0; cnt < max_idx+1; ++cnt)
    rename_map.push_back(cnt);
  for(set<int>::const_iterator it = var_set.begin(); it != var_set.end(); ++it)
  {
    if(!Utils::contains(templ_, *it))
      rename_map[*it] = newTmp();
  }
  to_add.renameVars(rename_map);
  solver->incAddCNF(to_add);
}

// -------------------------------------------------------------------------------------------
int TemplExplorer::newTmp()
{
  int new_var = next_free_var_;
  next_free_var_++;
  current_tmp_.push_back(new_var);
  return new_var;
}

// -------------------------------------------------------------------------------------------
int TemplExplorer::newParam()
{
  int new_var = next_free_var_;
  next_free_var_++;
  templ_.push_back(new_var);
  return new_var;
}

// -------------------------------------------------------------------------------------------
void TemplExplorer::swapPresentToNext(vector<int> &vec) const
{
  for(size_t lit_cnt = 0; lit_cnt < vec.size(); ++lit_cnt)
  {
    int old_lit = vec[lit_cnt];
    vec[lit_cnt] = old_lit < 0 ? -pres_to_nxt_[-old_lit] : pres_to_nxt_[old_lit];
  }
}
