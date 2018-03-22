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
/// @file LearnSynthSAT.cpp
/// @brief Contains the definition of the class LearnSynthSAT.
// -------------------------------------------------------------------------------------------

#include "LearnSynthSAT.h"
#include "VarManager.h"
#include "Options.h"
#include "AIG2CNF.h"
#include "Logger.h"
#include "CNFImplExtractor.h"
#include "LearningImplExtractor.h"
#include "LingelingApi.h"
#include "Utils.h"
#include "DepQBFApi.h"
#include "UnivExpander.h"

// -------------------------------------------------------------------------------------------
LearnSynthSAT::LearnSynthSAT(CNFImplExtractor *impl_extractor) :
               BackEnd(),
               solver_i_(Options::instance().getSATSolver()),
               solver_ctrl_(Options::instance().getSATSolver()),
               solver_i_ind_(Options::instance().getSATSolver()),
               solver_ctrl_ind_(Options::instance().getSATSolver()),
               impl_extractor_(impl_extractor),
               s_(VarManager::instance().getVarsOfType(VarInfo::PRES_STATE)),
               i_(VarManager::instance().getVarsOfType(VarInfo::INPUT)),
               c_(VarManager::instance().getVarsOfType(VarInfo::CTRL)),
               n_(VarManager::instance().getVarsOfType(VarInfo::NEXT_STATE))

{

  // build some often-used variables sets:
  si_.reserve(s_.size() + i_.size());
  si_.insert(si_.end(), s_.begin(), s_.end());
  si_.insert(si_.end(), i_.begin(), i_.end());
  sic_.reserve(si_.size() + c_.size());
  sic_.insert(sic_.end(), si_.begin(), si_.end());
  sic_.insert(sic_.end(), c_.begin(), c_.end());
  sicn_.reserve(sic_.size() + n_.size());
  sicn_.insert(sicn_.end(), sic_.begin(), sic_.end());
  sicn_.insert(sicn_.end(), n_.begin(), n_.end());


  // build previous-state copy of the transition relation. We want to have it enabled
  // if the current state is initial. The result is stored in prev_trans_or_initial_.
  AIG2CNF& A2C = AIG2CNF::instance();
  VarManager &VM = VarManager::instance();
  current_to_previous_map_.resize(VM.getMaxCNFVar() + 1, 0);
  vector<int> t = VM.getVarsOfType(VarInfo::TMP);

  for(size_t v_cnt = 0; v_cnt < s_.size(); ++v_cnt)
    current_to_previous_map_[s_[v_cnt]] = VM.createFreshPrevVar();
  for(size_t v_cnt = 0; v_cnt < c_.size(); ++v_cnt)
    current_to_previous_map_[c_[v_cnt]] = VM.createFreshTmpVar();
  for(size_t v_cnt = 0; v_cnt < i_.size(); ++v_cnt)
    current_to_previous_map_[i_[v_cnt]] = VM.createFreshTmpVar();
  for(size_t v_cnt = 0; v_cnt < t.size(); ++v_cnt)
    current_to_previous_map_[t[v_cnt]] = VM.createFreshTmpVar();
  for(size_t v_cnt = 0; v_cnt < n_.size(); ++v_cnt)
    current_to_previous_map_[n_[v_cnt]] = s_[v_cnt];

  current_state_is_initial_ = VM.createFreshTmpVar();
  list<vector<int> > prev_trans_clauses = A2C.getTrans().getClauses();
  for(CNF::ClauseIter it = prev_trans_clauses.begin(); it != prev_trans_clauses.end(); ++it)
  {
    presentToPrevious(*it);
    it->push_back(current_state_is_initial_);
  }
  prev_trans_or_initial_.swapWith(prev_trans_clauses);
  // if one of the state variables is true, then current_state_is_initial must be false:
  for(size_t cnt = 0; cnt < s_.size(); ++cnt)
    prev_trans_or_initial_.add2LitClause(-s_[cnt], -current_state_is_initial_);

  // different_from_prev_or_initial_ should be true if the current state is different from the
  // initial state.
  vector<int> one_is_diff;
  one_is_diff.reserve(s_.size() + 1);
  one_is_diff.push_back(current_state_is_initial_);
  for(size_t cnt = 0; cnt < s_.size(); ++cnt)
  {
    int diff = VM.createFreshTmpVar();
    one_is_diff.push_back(diff);
    int curr = s_[cnt];
    int prev = presentToPrevious(curr);
    // actually, we only need one direction of the implication:
    different_from_prev_or_initial_.add3LitClause(curr, prev, -diff);
    different_from_prev_or_initial_.add3LitClause(-curr, -prev, -diff);
    //different_from_prev_or_initial_.add3LitClause(-curr, prev, diff);
    //different_from_prev_or_initial_.add3LitClause(curr, -prev, diff);
  }
  different_from_prev_or_initial_.addClause(one_is_diff);
}

// -------------------------------------------------------------------------------------------
LearnSynthSAT::~LearnSynthSAT()
{
  delete solver_i_;
  solver_i_ = NULL;
  delete solver_ctrl_;
  solver_ctrl_ = NULL;
  delete solver_i_ind_;
  solver_i_ind_ = NULL;
  delete solver_ctrl_ind_;
  solver_ctrl_ind_ = NULL;
  delete impl_extractor_;
  impl_extractor_ = NULL;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthSAT::run()
{
  VarManager::instance().push();
  //L_DBG(VarManager::instance().toString());
  //L_DBG("TRANS:");
  //L_DBG(AIG2CNF::instance().getTrans().toString());
  L_INF("Starting to compute a winning region ...");
  statistics_.notifyWinRegStart();
  bool realizable = computeWinningRegion();
  statistics_.notifyWinRegEnd();
  if(!realizable)
  {
    L_RES("The specification is unrealizable.");
    statistics_.logStatistics();
    return false;
  }
  L_RES("The specification is realizable.");
  if(Options::instance().doRealizabilityOnly())
  {
    statistics_.logStatistics();
    return true;
  }

  L_INF("Starting to extract a circuit ...");
  MASSERT(Options::instance().getBackEndMode() != 2, "Not yet implemented.");
  impl_extractor_->extractCircuit(winning_region_);
  L_INF("Synthesis done.");
  statistics_.logStatistics();
  impl_extractor_->logStatistics();
  return true;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthSAT::computeWinningRegion()
{
  bool real = false;
  int m = Options::instance().getBackEndMode();
  if(m == 0)
  {
    real =  computeWinningRegionPlain();
    if(real)
      Utils::debugCheckWinReg(winning_region_);
  }
  else if(m == 1)
  {
    real =  computeWinningRegionRG();
    if(real)
      Utils::debugCheckWinReg(winning_region_);
  }
  else if(m == 2)
  {
    real =  computeWinningRegionRGRC();
    if(real)
      debugCheckWinReg();
  }
  else if(m == 3)
  {
    real =  computeWinningRegionPlainExp();
    if(real)
      Utils::debugCheckWinReg(winning_region_);
  }
  else // m==4
  {
    real =  computeWinningRegionRGExp();
    if(real)
      Utils::debugCheckWinReg(winning_region_);
  }
  return real;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthSAT::computeWinningRegionPlain()
{
  AIG2CNF& A2C = AIG2CNF::instance();

  winning_region_.clear();
  winning_region_ = A2C.getSafeStates();
  winning_region_large_.clear();
  winning_region_large_ = A2C.getSafeStates();

  // solver_i_ contains F & T & !F':
  solver_i_->doMinCores(true);
  solver_i_->doRandModels(false);
  solver_i_->startIncrementalSession(sic_, false);
  solver_i_->incAddCNF(A2C.getNextUnsafeStates());
  solver_i_->incAddCNF(A2C.getTrans());
  solver_i_->incAddCNF(A2C.getSafeStates());

  // solver_ctrl_ contains F & T & F':
  solver_ctrl_->doMinCores(true);
  solver_ctrl_->doRandModels(false);
  solver_ctrl_->startIncrementalSession(sicn_, false);
  solver_ctrl_->incAddCNF(A2C.getNextSafeStates());
  solver_ctrl_->incAddCNF(A2C.getTrans());
  solver_ctrl_->incAddCNF(A2C.getSafeStates());
  bool precise = true;

  size_t it_cnt = 0;
  size_t clauses_added = 0;
  size_t reset_c_cnt = 0;
  vector<int> model_or_core;
  while(true)
  {
    ++it_cnt;

    // First, we check if there is some transition with some values for the inputs i and
    // and c from F to !G' (G is a copy of F that is updated only lazily).
    statistics_.notifyBeforeComputeCandidate();
    bool sat = solver_i_->incIsSatModelOrCore(vector<int>(), si_, model_or_core);
    statistics_.notifyAfterComputeCandidate();

    if(!sat)
    {
      // No such transition exists.
      if(precise)
      {
        // G = F. This means that there exists no possibility for the antagonist to leave F.
        // Hence, F is a valid winning region. We can stop.
        return true;
      }
      // G != F. Hence, the reason why no such transition exists could be that G is not up-to
      // data. We set G:=F and start again. For that, we have to restart the incremental
      // of session solver_i_.
      statistics_.notifyRestart();
      L_DBG("Need to restart with fresh U (iteration " << it_cnt << ")");
      // get rid of all temporary variables introduced during negations:
      VarManager::instance().resetToLastPush();
      solver_i_->startIncrementalSession(sic_, false);
      Utils::compressStateCNF(winning_region_);
      CNF leave_win(winning_region_);
      leave_win.swapPresentToNext();
      leave_win.negate();
      leave_win.addCNF(AIG2CNF::instance().getTrans());
      leave_win.addCNF(winning_region_);
      //leave_win.addCNF(winning_region_large_);
      solver_i_->incAddCNF(leave_win);
      precise = true;
      continue;
    }

    vector<int> state_input = model_or_core;
    vector<int> state = Utils::extract(state_input, VarInfo::PRES_STATE);
    vector<int> input = Utils::extract(state_input, VarInfo::INPUT);

    // There exists a transition from F to !G' with some input i. Let's now see if the
    // protagonist can find some c such that leaving F is avoided:
    statistics_.notifyBeforeCheckCandidate();
    sat = solver_ctrl_->incIsSatModelOrCore(state, input, c_, model_or_core);
    if(!sat)
    {
      // No such response exists. This means that we have found a counterexample-state.
      // The solver solver_ctrl_ already computed a generalization of this
      // counterexample-state in form of an unsatisfiable core. The unsatisfiable core
      // represents the set of all state for which input i enforces that F is left.
      // (This is weaker than what we compute with the QBF-solver in LearnSynthQBF, but
      // it appears to be good enough in our experiments, and the computation is fast.)

      // We can try to reduce it further (does not pay of):
      //vector<int> first_core(model_or_core);
      //solver_ctrl_->incAddNegCubeAsClause(model_or_core);
      //Utils::swapPresentToNext(model_or_core);
      //solver_ctrl_->incAddNegCubeAsClause(model_or_core);
      //sat = solver_ctrl_->incIsSatModelOrCore(first_core, input, c, model_or_core);

      if(Utils::containsInit(model_or_core))
        return false;
      // We compute the corresponding blocking clause, and update the winning region and the
      // solvers:
      vector<int> blocking_clause(model_or_core);
      Utils::negateLiterals(blocking_clause);
      winning_region_.addClauseAndSimplify(blocking_clause);
      //winning_region_large_.addClauseAndSimplify(blocking_clause);
      solver_i_->incAddClause(blocking_clause);

      // reseting the solver_ctrl_ seems to help.
      // TODO: find out good numbers for the heuristics when to restart
      if(clauses_added > winning_region_.getNrOfClauses() + 100)
      {
        if(reset_c_cnt % 1000 == 999)
          Utils::compressStateCNF(winning_region_, true);
        else if(reset_c_cnt % 100 == 99)
          Utils::compressStateCNF(winning_region_, false);
        solver_ctrl_->startIncrementalSession(sicn_, false);
        CNF next_win(winning_region_);
        next_win.swapPresentToNext();
        solver_ctrl_->incAddCNF(winning_region_);
        solver_ctrl_->incAddCNF(A2C.getTrans());
        solver_ctrl_->incAddCNF(next_win);
        clauses_added = winning_region_.getNrOfClauses();
        reset_c_cnt++;
      }
      else
      {
        solver_ctrl_->incAddClause(blocking_clause);
        Utils::swapPresentToNext(blocking_clause);
        solver_ctrl_->incAddClause(blocking_clause);
        clauses_added++;
      }
      precise = false;
      statistics_.notifyAfterCheckCandidateFound(s_.size(), blocking_clause.size());
    }
    else
    {
      // There exists a response of the protagonist to this input vector. Hence, this input
      // vector is useless for the antagonist in trying to go from F to !G'. We need to
      // exclude it from solver_i_ so that the same state-input pair is not tried again.
      // However, instead of excluding this one state-input pair only, we generalize it using
      // an unsatisfiable core. The core gives us all state-input combinations for which
      // the control vector found by solver_ctrl_ prevents that we go from F to !G'.
      statistics_.notifyAfterCheckCandidateFailed();
      vector<int> ctrl(model_or_core);
      statistics_.notifyBeforeRefine();
      sat = solver_i_->incIsSatModelOrCore(state_input, ctrl, vector<int>(), model_or_core);
      MASSERT(sat == false, "Impossible.");
      solver_i_->incAddNegCubeAsClause(model_or_core);
      statistics_.notifyAfterRefine(si_.size(), model_or_core.size());
    }
  }
}

// -------------------------------------------------------------------------------------------
bool LearnSynthSAT::computeWinningRegionPlainExp()
{
  AIG2CNF& A2C = AIG2CNF::instance();

  winning_region_.clear();
  winning_region_ = A2C.getSafeStates();
  UnivExpander expander;

  // solver_i_ contains F & T & !F':
  solver_i_->doMinCores(true);
  solver_i_->doRandModels(false);
  expander.resetSolverIExp(winning_region_, solver_i_);

  // solver_ctrl_ contains F & T & F':
  solver_ctrl_->doMinCores(true);
  solver_ctrl_->doRandModels(false);
  expander.initSolverCExp(solver_ctrl_, vector<int>());
  solver_ctrl_->incAddCNF(A2C.getSafeStates());
  vector<int> safe;
  safe.push_back(-VarManager::instance().getPresErrorStateVar());
  expander.addExpNxtClauseToC(safe, solver_ctrl_);

  bool precise = true;

  size_t it_cnt = 0;
  size_t clauses_added = 0;
  size_t reset_c_cnt = 0;
  vector<int> model_or_core;
  while(true)
  {
    ++it_cnt;

    // First, we check if there is some transition with some values for the inputs i and
    // and c from F to !G' (G is a copy of F that is updated only lazily).
    statistics_.notifyBeforeComputeCandidate();
    bool sat = solver_i_->incIsSatModelOrCore(vector<int>(), si_, model_or_core);
    statistics_.notifyAfterComputeCandidate();

    if(!sat)
    {
      // No such transition exists.
      if(precise)
      {
        // G = F. This means that there exists no possibility for the antagonist to leave F.
        // Hence, F is a valid winning region. We can stop.
        return true;
      }
      // G != F. Hence, the reason why no such transition exists could be that G is not up-to
      // data. We set G:=F and start again. For that, we have to restart the incremental
      // of session solver_i_.
      statistics_.notifyRestart();
      L_DBG("Need to restart with fresh U (iteration " << it_cnt << ")");
      Utils::compressStateCNF(winning_region_);
      VarManager::instance().resetToLastPush();
      expander.resetSolverIExp(winning_region_, solver_i_);
      precise = true;
      continue;
    }

    vector<int> state_input = model_or_core;
    vector<int> state = Utils::extract(state_input, VarInfo::PRES_STATE);
    vector<int> input = Utils::extract(state_input, VarInfo::INPUT);

    // There exists a transition from F to !G' with some input i. Let's now see if the
    // protagonist can find some c such that leaving F is avoided:
    statistics_.notifyBeforeCheckCandidate();
    sat = solver_ctrl_->incIsSatModelOrCore(state, input, c_, model_or_core);
    MASSERT(!sat, "Since we expanded over all ctrl-signals, this is impossible.");

    // The solver solver_ctrl_ already computed a generalization of this
    // counterexample-state in form of an unsatisfiable core. The unsatisfiable core
    // represents the set of all state for which input i (and a few other inputs due to
    // expansion) enforces that F is left.

    if(Utils::containsInit(model_or_core))
      return false;
    // We compute the corresponding blocking clause, and update the winning region and the
    // solvers:
    vector<int> blocking_clause(model_or_core);
    Utils::negateLiterals(blocking_clause);
    winning_region_.addClauseAndSimplify(blocking_clause);
    solver_i_->incAddClause(blocking_clause);


    // reseting the solver_ctrl_ seems to help.
    // TODO: find out good numbers for the heuristics when to restart
    if(clauses_added > winning_region_.getNrOfClauses() + 100)
    {
      if(reset_c_cnt % 1000 == 999)
        Utils::compressStateCNF(winning_region_, true);
      else if(reset_c_cnt % 100 == 99)
        Utils::compressStateCNF(winning_region_, false);

      expander.resetSolverCExp(solver_ctrl_);
      solver_ctrl_->incAddCNF(winning_region_);
      const list<vector<int> > &cl = winning_region_.getClauses();
      for(CNF::ClauseConstIter it = cl.begin(); it != cl.end(); ++it)
        expander.addExpNxtClauseToC(*it, solver_ctrl_);
      clauses_added = winning_region_.getNrOfClauses();
      reset_c_cnt++;
    }
    else
    {
      solver_ctrl_->incAddClause(blocking_clause);
      expander.addExpNxtClauseToC(blocking_clause, solver_ctrl_);
      clauses_added++;
    }
    precise = false;
    statistics_.notifyAfterCheckCandidateFound(s_.size(), blocking_clause.size());

  }
}


// -------------------------------------------------------------------------------------------
bool LearnSynthSAT::computeWinningRegionPlainDep()
{
  AIG2CNF& A2C = AIG2CNF::instance();
  VarManager &VM = VarManager::instance();
  L_DBG("Nr. of state variables: " << VM.getVarsOfType(VarInfo::PRES_STATE).size());
  L_DBG("Nr. of uncontrollable inputs: " << VM.getVarsOfType(VarInfo::INPUT).size());
  L_DBG("Nr. of controllable inputs: " << VM.getVarsOfType(VarInfo::CTRL).size());

  const vector<int> &s = VM.getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &c = VM.getVarsOfType(VarInfo::CTRL);
  const vector<int> &i = VM.getVarsOfType(VarInfo::INPUT);
  const vector<int> &n = VM.getVarsOfType(VarInfo::NEXT_STATE);
  //vector<int> si;
  //si.reserve(s.size() + i.size());
  //si.insert(si.end(), s.begin(), s.end());
  //si.insert(si.end(), i.begin(), i.end());

  const map<int, set<VarInfo> > &deps = AIG2CNF::instance().getTmpDepsTrans();
  vector<int> ext_s;
  ext_s.reserve(s.size() + deps.size());
  ext_s.insert(ext_s.end(), s.begin(), s.end());
  vector<int> ext_i;
  ext_i.reserve(i.size() + deps.size());
  ext_i.insert(ext_i.end(), i.begin(), i.end());
  vector<int> ext_si;
  ext_si.reserve(s.size() + i.size() + deps.size());
  ext_si.insert(ext_si.end(), s.begin(), s.end());
  ext_si.insert(ext_si.end(), i.begin(), i.end());
  for(AIG2CNF::DepConstIter it = deps.begin(); it != deps.end(); ++it)
  {
    bool only_s = true;
    bool only_i = true;
    bool only_si = true;
    for(set<VarInfo>::const_iterator i2 = it->second.begin(); i2 != it->second.end(); ++i2)
    {
      if(i2->getKind() != VarInfo::PRES_STATE)
        only_s = false;
      if(i2->getKind() != VarInfo::INPUT)
        only_i = false;
      if(i2->getKind() != VarInfo::PRES_STATE && i2->getKind() != VarInfo::INPUT)
        only_si = false;
    }
    if(only_s)
      ext_s.push_back(it->first);
    if(only_i)
      ext_i.push_back(it->first);
    if(only_si)
      ext_si.push_back(it->first);
  }

  vector<int> vars_to_keep;
  vars_to_keep.reserve(ext_si.size() + c.size() + n.size());
  vars_to_keep.insert(vars_to_keep.end(), ext_si.begin(), ext_si.end());
  vars_to_keep.insert(vars_to_keep.end(), c.begin(), c.end());
  vars_to_keep.insert(vars_to_keep.end(), n.begin(), n.end());

  winning_region_.clear();
  winning_region_ = A2C.getSafeStates();
  winning_region_large_.clear();
  winning_region_large_ = A2C.getSafeStates();

  // solver_i_ contains F & T & !F':
  solver_i_->doMinCores(true);
  solver_i_->doRandModels(false);
  solver_i_->startIncrementalSession(vars_to_keep, false);
  solver_i_->incAddCNF(A2C.getNextUnsafeStates());
  solver_i_->incAddCNF(A2C.getTrans());
  solver_i_->incAddCNF(A2C.getSafeStates());

  // solver_ctrl_ contains F & T & F':
  solver_ctrl_->doMinCores(true);
  solver_ctrl_->doRandModels(false);
  solver_ctrl_->startIncrementalSession(vars_to_keep, false);
  solver_ctrl_->incAddCNF(A2C.getNextSafeStates());
  solver_ctrl_->incAddCNF(A2C.getTrans());
  solver_ctrl_->incAddCNF(A2C.getSafeStates());
  bool precise = true;

  size_t it_cnt = 0;
  size_t clauses_added = 0;
  size_t reset_c_cnt = 0;
  vector<int> model_or_core;
  while(true)
  {
    ++it_cnt;

    // First, we check if there is some transition with some values for the inputs i and
    // and c from F to !G' (G is a copy of F that is updated only lazily).
    statistics_.notifyBeforeComputeCandidate();
    bool sat = solver_i_->incIsSatModelOrCore(vector<int>(), ext_si, model_or_core);
    statistics_.notifyAfterComputeCandidate();

    if(!sat)
    {
      // No such transition exists.
      if(precise)
      {
        // G = F. This means that there exists no possibility for the antagonist to leave F.
        // Hence, F is a valid winning region. We can stop.
        return true;
      }
      // G != F. Hence, the reason why no such transition exists could be that G is not up-to
      // data. We set G:=F and start again. For that, we have to restart the incremental
      // of session solver_i_.
      statistics_.notifyRestart();
      L_DBG("Need to restart with fresh U (iteration " << it_cnt << ")");
      // get rid of all temporary variables introduced during negations:
      VM.resetToLastPush();
      solver_i_->startIncrementalSession(vars_to_keep, false);
      Utils::compressStateCNF(winning_region_);
      CNF leave_win(winning_region_);
      leave_win.swapPresentToNext();
      leave_win.negate();
      leave_win.addCNF(AIG2CNF::instance().getTrans());
      leave_win.addCNF(winning_region_);
      //leave_win.addCNF(winning_region_large_);
      solver_i_->incAddCNF(leave_win);
      precise = true;
      continue;
    }

    vector<int> state_input = model_or_core;
    vector<int> state = Utils::extract(state_input, s);
    vector<int> input = Utils::extract(state_input, ext_i);

    // There exists a transition from F to !G' with some input i. Let's now see if the
    // protagonist can find some c such that leaving F is avoided:
    statistics_.notifyBeforeCheckCandidate();
    sat = solver_ctrl_->incIsSatModelOrCore(state, input, c, model_or_core);
    if(!sat)
    {
      // No such response exists. This means that we have found a counterexample-state.
      // The solver solver_ctrl_ already computed a generalization of this
      // counterexample-state in form of an unsatisfiable core. The unsatisfiable core
      // represents the set of all state for which input i enforces that F is left.
      // (This is weaker than what we compute with the QBF-solver in LearnSynthQBF, but
      // it appears to be good enough in our experiments, and the computation is fast.)

      // We can try to reduce it further (does not pay of):
      //vector<int> first_core(model_or_core);
      //solver_ctrl_->incAddNegCubeAsClause(model_or_core);
      //Utils::swapPresentToNext(model_or_core);
      //solver_ctrl_->incAddNegCubeAsClause(model_or_core);
      //sat = solver_ctrl_->incIsSatModelOrCore(first_core, input, c, model_or_core);

      if(Utils::containsInit(model_or_core))
        return false;
      // We compute the corresponding blocking clause, and update the winning region and the
      // solvers:
      vector<int> blocking_clause(model_or_core);
      for(size_t cnt = 0; cnt < blocking_clause.size(); ++cnt)
        blocking_clause[cnt] = -blocking_clause[cnt];
      winning_region_.addClauseAndSimplify(blocking_clause);
      winning_region_large_.addClauseAndSimplify(blocking_clause);
      solver_i_->incAddClause(blocking_clause);

      // reseting the solver_ctrl_ seems to help.
      // TODO: find out good numbers for the heuristics when to restart
      if(clauses_added > winning_region_.getNrOfClauses() + 100)
      {
        if(reset_c_cnt % 1000 == 999)
          Utils::compressStateCNF(winning_region_, true);
        else if(reset_c_cnt % 100 == 99)
          Utils::compressStateCNF(winning_region_, false);

        solver_ctrl_->startIncrementalSession(vars_to_keep, false);
        CNF next_win(winning_region_);
        next_win.swapPresentToNext();
        solver_ctrl_->incAddCNF(winning_region_);
        solver_ctrl_->incAddCNF(A2C.getTrans());
        solver_ctrl_->incAddCNF(next_win);
        clauses_added = winning_region_.getNrOfClauses();
        reset_c_cnt++;
      }
      else
      {
        solver_ctrl_->incAddClause(blocking_clause);
        Utils::swapPresentToNext(blocking_clause);
        solver_ctrl_->incAddClause(blocking_clause);
        clauses_added++;
      }
      precise = false;
      statistics_.notifyAfterCheckCandidateFound(ext_s.size(), blocking_clause.size());
    }
    else
    {
      // There exists a response of the protagonist to this input vector. Hence, this input
      // vector is useless for the antagonist in trying to go from F to !G'. We need to
      // exclude it from solver_i_ so that the same state-input pair is not tried again.
      // However, instead of excluding this one state-input pair only, we generalize it using
      // an unsatisfiable core. The core gives us all state-input combinations for which
      // the control vector found by solver_ctrl_ prevents that we go from F to !G'.
      statistics_.notifyAfterCheckCandidateFailed();
      vector<int> ctrl(model_or_core);
      statistics_.notifyBeforeRefine();
      //Utils::randomize(state_input);
      sat = solver_i_->incIsSatModelOrCore(state_input, ctrl, vector<int>(), model_or_core);
      MASSERT(sat == false, "Impossible.");
      solver_i_->incAddNegCubeAsClause(model_or_core);
      statistics_.notifyAfterRefine(ext_si.size(), model_or_core.size());
    }
  }
}

// -------------------------------------------------------------------------------------------
bool LearnSynthSAT::computeWinningRegionPlainDep2()
{
  AIG2CNF& A2C = AIG2CNF::instance();
  VarManager &VM = VarManager::instance();
  L_DBG("Nr. of state variables: " << VM.getVarsOfType(VarInfo::PRES_STATE).size());
  L_DBG("Nr. of uncontrollable inputs: " << VM.getVarsOfType(VarInfo::INPUT).size());
  L_DBG("Nr. of controllable inputs: " << VM.getVarsOfType(VarInfo::CTRL).size());

  const vector<int> &s = VM.getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &c = VM.getVarsOfType(VarInfo::CTRL);
  const vector<int> &i = VM.getVarsOfType(VarInfo::INPUT);
  const vector<int> &n = VM.getVarsOfType(VarInfo::NEXT_STATE);
  //vector<int> si;
  //si.reserve(s.size() + i.size());
  //si.insert(si.end(), s.begin(), s.end());
  //si.insert(si.end(), i.begin(), i.end());

  const map<int, set<VarInfo> > &deps = AIG2CNF::instance().getTmpDepsTrans();
  vector<int> ext_s;
  ext_s.reserve(s.size() + deps.size());
  ext_s.insert(ext_s.end(), s.begin(), s.end());
  vector<int> ext_i;
  ext_i.reserve(i.size() + deps.size());
  ext_i.insert(ext_i.end(), i.begin(), i.end());
  vector<int> ext_si;
  ext_si.reserve(s.size() + i.size() + deps.size());
  ext_si.insert(ext_si.end(), s.begin(), s.end());
  ext_si.insert(ext_si.end(), i.begin(), i.end());
  set<int> miss;
  list<int> analyze_further_queue;
  for(AIG2CNF::DepConstIter it = deps.begin(); it != deps.end(); ++it)
  {
    if(it->first == 1)// The CNF var '1' is always the constant true and thus not interesting
      continue;

    bool only_s = true;
    bool only_i = true;
    bool only_si = true;
    for(set<VarInfo>::const_iterator i2 = it->second.begin(); i2 != it->second.end(); ++i2)
    {
      if(i2->getKind() != VarInfo::PRES_STATE)
        only_s = false;
      if(i2->getKind() != VarInfo::INPUT)
        only_i = false;
      if(i2->getKind() != VarInfo::PRES_STATE && i2->getKind() != VarInfo::INPUT)
        only_si = false;
    }
    if(only_s)
    {
      ext_s.push_back(it->first);
      miss.insert(it->first);
      analyze_further_queue.push_back(it->first);
    }
    if(only_i)
      ext_i.push_back(it->first);
    if(only_si)
      ext_si.push_back(it->first);
  }

  // find the definition of all temporary variables that depend only on state vars:
  CNF s_dep_defs;
  const map<int, set<VarInfo> > &direct_deps = AIG2CNF::instance().getTmpDeps();
  while(!analyze_further_queue.empty())
  {
    int analyze = analyze_further_queue.front();
    analyze_further_queue.pop_front();
    AIG2CNF::DepConstIter d = direct_deps.find(analyze);
    MASSERT(d != direct_deps.end(), "Impossible.");
    for(set<VarInfo>::const_iterator it = d->second.begin(); it != d->second.end(); ++it)
    {
      if(it->getKind() == VarInfo::TMP && miss.count(it->getLitInCNF()) == 0)
      {
        miss.insert(it->getLitInCNF());
        analyze_further_queue.push_back(it->getLitInCNF());
      }
    }
  }
  set<int> to_find(miss);
  const list<vector<int> > &all_clauses = AIG2CNF::instance().getTrans().getClauses();
  for(CNF::ClauseConstIter it = all_clauses.begin(); it != all_clauses.end(); ++it)
  {
    const vector<int> &cl = *it;
    if(cl.size() == 2 && to_find.count(-cl[0]))
    {
      int v = -cl[0];
      s_dep_defs.addClause(cl);
      ++it;
      MASSERT(it != all_clauses.end(), "Impossible.");
      const vector<int> &cl2 = *it;
      DASSERT(cl2.size() == 2 && cl2[0] == -v, "Impossible.");
      s_dep_defs.addClause(cl2);
      ++it;
      MASSERT(it != all_clauses.end(), "Impossible.");
      const vector<int> &cl3 = *it;
      DASSERT(cl3.size() == 3 && cl3[0] == v, "Impossible.");
      s_dep_defs.addClause(cl3);
      to_find.erase(v);
    }
    if(to_find.empty())
      break;
  }
  int nr_of_vars = VarManager::instance().getMaxCNFVar() + 1;
  vector<int> pres_to_next_map(nr_of_vars, 0);
  for(int cnt = 1; cnt < nr_of_vars; ++cnt)
    pres_to_next_map[cnt] = cnt;
  for(size_t cnt = 0; cnt < s.size(); ++cnt)
    pres_to_next_map[s[cnt]] = n[cnt];
  for(set<int>::const_iterator it = miss.begin(); it != miss.end(); ++it)
    pres_to_next_map[*it] = VarManager::instance().createFreshTmpVar();
  CNF n_dep_defs(s_dep_defs);
  n_dep_defs.renameVars(pres_to_next_map);

  vector<int> vars_to_keep;
  vars_to_keep.reserve(ext_si.size() + c.size() + n.size() + miss.size());
  vars_to_keep.insert(vars_to_keep.end(), ext_si.begin(), ext_si.end());
  vars_to_keep.insert(vars_to_keep.end(), c.begin(), c.end());
  vars_to_keep.insert(vars_to_keep.end(), n.begin(), n.end());
  for(set<int>::const_iterator it = miss.begin(); it != miss.end(); ++it)
    vars_to_keep.push_back(pres_to_next_map[*it]);

  VM.push();

  winning_region_.clear();
  winning_region_ = A2C.getSafeStates();
  winning_region_large_.clear();
  winning_region_large_ = A2C.getSafeStates();

  // solver_i_ contains F & T & !F':
  solver_i_->doMinCores(true);
  solver_i_->doRandModels(false);
  solver_i_->startIncrementalSession(vars_to_keep, false);
  solver_i_->incAddCNF(A2C.getNextUnsafeStates());
  solver_i_->incAddCNF(A2C.getTrans());
  solver_i_->incAddCNF(A2C.getSafeStates());
  solver_i_->incAddCNF(n_dep_defs);

  // solver_ctrl_ contains F & T & F':
  solver_ctrl_->doMinCores(true);
  solver_ctrl_->doRandModels(false);
  solver_ctrl_->startIncrementalSession(vars_to_keep, false);
  solver_ctrl_->incAddCNF(A2C.getNextSafeStates());
  solver_ctrl_->incAddCNF(A2C.getTrans());
  solver_ctrl_->incAddCNF(A2C.getSafeStates());
  solver_ctrl_->incAddCNF(n_dep_defs);
  bool precise = true;

  size_t it_cnt = 0;
  size_t clauses_added = 0;
  size_t reset_c_cnt = 0;
  vector<int> model_or_core;
  while(true)
  {
    ++it_cnt;

    // First, we check if there is some transition with some values for the inputs i and
    // and c from F to !G' (G is a copy of F that is updated only lazily).
    statistics_.notifyBeforeComputeCandidate();
    bool sat = solver_i_->incIsSatModelOrCore(vector<int>(), ext_si, model_or_core);
    statistics_.notifyAfterComputeCandidate();

    if(!sat)
    {
      // No such transition exists.
      if(precise)
      {
        // G = F. This means that there exists no possibility for the antagonist to leave F.
        // Hence, F is a valid winning region. We can stop.
        return true;
      }
      // G != F. Hence, the reason why no such transition exists could be that G is not up-to
      // data. We set G:=F and start again. For that, we have to restart the incremental
      // of session solver_i_.
      statistics_.notifyRestart();
      L_DBG("Need to restart with fresh U (iteration " << it_cnt << ")");
      // get rid of all temporary variables introduced during negations:
      VM.resetToLastPush();
      solver_i_->startIncrementalSession(vars_to_keep, false);
      Utils::compressStateCNF(winning_region_);
      CNF leave_win(winning_region_);
      leave_win.renameVars(pres_to_next_map);
      leave_win.negate();
      leave_win.addCNF(AIG2CNF::instance().getTrans());
      leave_win.addCNF(winning_region_);
      leave_win.addCNF(n_dep_defs);
      //leave_win.addCNF(winning_region_large_);
      solver_i_->incAddCNF(leave_win);
      precise = true;
      continue;
    }

    vector<int> state_input = model_or_core;
    vector<int> state = Utils::extract(state_input, ext_s);
    vector<int> input = Utils::extract(state_input, ext_i);

    // There exists a transition from F to !G' with some input i. Let's now see if the
    // protagonist can find some c such that leaving F is avoided:
    statistics_.notifyBeforeCheckCandidate();
    sat = solver_ctrl_->incIsSatModelOrCore(state, input, c, model_or_core);
    if(!sat)
    {
      // No such response exists. This means that we have found a counterexample-state.
      // The solver solver_ctrl_ already computed a generalization of this
      // counterexample-state in form of an unsatisfiable core. The unsatisfiable core
      // represents the set of all state for which input i enforces that F is left.
      // (This is weaker than what we compute with the QBF-solver in LearnSynthQBF, but
      // it appears to be good enough in our experiments, and the computation is fast.)

      // We can try to reduce it further (does not pay of):
      //vector<int> first_core(model_or_core);
      //solver_ctrl_->incAddNegCubeAsClause(model_or_core);
      //Utils::swapPresentToNext(model_or_core);
      //solver_ctrl_->incAddNegCubeAsClause(model_or_core);
      //sat = solver_ctrl_->incIsSatModelOrCore(first_core, input, c, model_or_core);

      //if(Utils::containsInit(model_or_core))
      //  return false;
      // We compute the corresponding blocking clause, and update the winning region and the
      // solvers:
      vector<int> blocking_clause(model_or_core);
      for(size_t cnt = 0; cnt < blocking_clause.size(); ++cnt)
        blocking_clause[cnt] = -blocking_clause[cnt];
      winning_region_.addClauseAndSimplify(blocking_clause);
      winning_region_large_.addClauseAndSimplify(blocking_clause);
      solver_i_->incAddClause(blocking_clause);

      // reseting the solver_ctrl_ seems to help.
      // TODO: find out good numbers for the heuristics when to restart
      if(clauses_added > winning_region_.getNrOfClauses() + 100)
      {
        if(reset_c_cnt % 1000 == 999)
          Utils::compressStateCNF(winning_region_, true);
        else if(reset_c_cnt % 100 == 99)
          Utils::compressStateCNF(winning_region_, false);
        solver_ctrl_->startIncrementalSession(vars_to_keep, false);
        solver_ctrl_->incAddCNF(n_dep_defs);
        solver_ctrl_->incAddCNF(winning_region_);
        solver_ctrl_->incAddCNF(A2C.getTrans());
        CNF next_win(winning_region_);
        next_win.renameVars(pres_to_next_map);
        solver_ctrl_->incAddCNF(next_win);
        clauses_added = winning_region_.getNrOfClauses();
        reset_c_cnt++;
      }
      else
      {
        solver_ctrl_->incAddClause(blocking_clause);
        CNF next_bl;
        next_bl.addClause(blocking_clause);
        next_bl.renameVars(pres_to_next_map);
        solver_ctrl_->incAddCNF(next_bl);
        ++clauses_added;
      }

      precise = false;
      statistics_.notifyAfterCheckCandidateFound(ext_s.size(), blocking_clause.size());
    }
    else
    {
      // There exists a response of the protagonist to this input vector. Hence, this input
      // vector is useless for the antagonist in trying to go from F to !G'. We need to
      // exclude it from solver_i_ so that the same state-input pair is not tried again.
      // However, instead of excluding this one state-input pair only, we generalize it using
      // an unsatisfiable core. The core gives us all state-input combinations for which
      // the control vector found by solver_ctrl_ prevents that we go from F to !G'.
      statistics_.notifyAfterCheckCandidateFailed();
      vector<int> ctrl(model_or_core);
      statistics_.notifyBeforeRefine();
      //Utils::randomize(state_input);
      sat = solver_i_->incIsSatModelOrCore(state_input, ctrl, vector<int>(), model_or_core);
      MASSERT(sat == false, "Impossible.");
      solver_i_->incAddNegCubeAsClause(model_or_core);
      statistics_.notifyAfterRefine(ext_si.size(), model_or_core.size());
    }
  }
}

// -------------------------------------------------------------------------------------------
bool LearnSynthSAT::computeWinningRegionRG()
{
  AIG2CNF &A2C = AIG2CNF::instance();
  VarManager &VM = VarManager::instance();

  winning_region_.clear();
  winning_region_ = A2C.getSafeStates();
  winning_region_large_.clear();
  winning_region_large_ = A2C.getSafeStates();

  // solver_i_ contains F & T & !F':
  solver_i_->doMinCores(true);
  solver_i_->doRandModels(false);
  solver_i_->startIncrementalSession(sic_, false);
  solver_i_->incAddCNF(A2C.getNextUnsafeStates());
  solver_i_->incAddCNF(A2C.getTrans());
  solver_i_->incAddCNF(A2C.getSafeStates());

  // solver_ctrl_ contains F & T & F':
  // We set doMinCores to false because we minimize the core we get from solver_ctrl_ further
  // with solver_ctrl_ind_ anyway:
  solver_ctrl_->doMinCores(false);
  solver_ctrl_->doRandModels(false);
  solver_ctrl_->startIncrementalSession(sicn_, false);
  solver_ctrl_->incAddCNF(A2C.getNextSafeStates());
  solver_ctrl_->incAddCNF(A2C.getTrans());
  solver_ctrl_->incAddCNF(A2C.getSafeStates());

  // solver_ctrl_ind_ contains (I | T* & F*) & F & T & F':
  solver_ctrl_ind_->doMinCores(true);
  solver_ctrl_ind_->doRandModels(false);
  solver_ctrl_ind_->startIncrementalSession(VM.getAllNonTempVars(), true);
  solver_ctrl_ind_->incAddCNF(A2C.getNextSafeStates());
  solver_ctrl_ind_->incAddCNF(A2C.getTrans());
  solver_ctrl_ind_->incAddCNF(A2C.getSafeStates());
  solver_ctrl_ind_->incAddCNF(prev_trans_or_initial_);
  solver_ctrl_ind_->incAddUnitClause(-current_to_previous_map_[VM.getPresErrorStateVar()]);
  bool precise = true;

  size_t it_cnt = 0;
  size_t clauses_added = 0;
  size_t reset_c_cnt = 0;
  vector<int> model_or_core;
  while(true)
  {
    ++it_cnt;

    // First, we check if there is some transition with some values for the inputs i and
    // and c from F to !G' (G is a copy of F that is updated only lazily).
    statistics_.notifyBeforeComputeCandidate();
    bool sat = solver_i_->incIsSatModelOrCore(vector<int>(), si_, model_or_core);
    statistics_.notifyAfterComputeCandidate();

    if(!sat)
    {
      // No such transition exists.
      if(precise)
      {
        // G = F. This means that there exists no possibility for the antagonist to leave F.
        // Hence, F is a valid winning region. We can stop.
        return true;
      }
      // G != F. Hence, the reason why no such transition exists could be that G is not up-to
      // data. We set G:=F and start again. For that, we have to restart the incremental
      // of session solver_i_.
      statistics_.notifyRestart();
      L_DBG("Need to restart with fresh U (iteration " << it_cnt << ")");
      // get rid of all temporary variables introduced during negations:
      VM.resetToLastPush();
      solver_i_->startIncrementalSession(sic_, false);
      Utils::compressStateCNF(winning_region_);
      CNF leave_win(winning_region_);
      leave_win.swapPresentToNext();
      leave_win.negate();
      leave_win.addCNF(AIG2CNF::instance().getTrans());
      leave_win.addCNF(winning_region_);
      //leave_win.addCNF(winning_region_large_);
      solver_i_->incAddCNF(leave_win);
      precise = true;
      continue;
    }

    vector<int> state_input = model_or_core;
    vector<int> state = Utils::extract(state_input, VarInfo::PRES_STATE);
    vector<int> input = Utils::extract(state_input, VarInfo::INPUT);

    // There exists a transition from F to !G' with some input i. Let's now see if the
    // protagonist can find some c such that leaving F is avoided:
    statistics_.notifyBeforeCheckCandidate();
    // Note that we use solver_ctrl_ and not solver_ctrl_ind_ here. Using solver_ctrl_ind_
    // would also be correct but slower.
    sat = solver_ctrl_->incIsSatModelOrCore(state, input, c_, model_or_core);
    if(!sat)
    {
      // No such response exists. This means that we have found a counterexample-state.
      // The solver solver_ctrl_ already computed a generalization of this
      // counterexample-state in form of an unsatisfiable core. The unsatisfiable core
      // represents the set of all state for which input i enforces that F is left.
      // (This is weaker than what we compute with the QBF-solver in LearnSynthQBF, but
      // it appears to be good enough in our experiments, and the computation is fast.)

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

      if(Utils::containsInit(model_or_core))
        return false;
      // We compute the corresponding blocking clause, and update the winning region and the
      // solvers:
      vector<int> blocking_clause(model_or_core);
      Utils::negateLiterals(blocking_clause);
      vector<int> prev_blocking_clause(blocking_clause);
      presentToPrevious(prev_blocking_clause);
      vector<int> next_blocking_clause(blocking_clause);
      Utils::swapPresentToNext(next_blocking_clause);
      winning_region_.addClauseAndSimplify(blocking_clause);
      solver_i_->incAddClause(blocking_clause);
      // reseting the solver_ctrl_ind_ seems to help.
      // TODO: find out good numbers for the heuristics when to restart
      if(clauses_added > winning_region_.getNrOfClauses() + 100)
      {
        if(reset_c_cnt % 1000 == 999)
          Utils::compressStateCNF(winning_region_, true);
        else if(reset_c_cnt % 100 == 99)
          Utils::compressStateCNF(winning_region_, false);
        solver_ctrl_->startIncrementalSession(sicn_, false);
        CNF next_win(winning_region_);
        next_win.swapPresentToNext();
        CNF prev_win(winning_region_);
        presentToPrevious(prev_win);
        solver_ctrl_->incAddCNF(winning_region_);
        solver_ctrl_->incAddCNF(A2C.getTrans());
        solver_ctrl_->incAddCNF(next_win);

        // solver_ctrl_ind_ contains (I | T* & F*) & F & T & F':
        solver_ctrl_ind_->startIncrementalSession(VM.getAllNonTempVars(), true);
        solver_ctrl_ind_->incAddCNF(winning_region_);
        solver_ctrl_ind_->incAddCNF(A2C.getTrans());
        solver_ctrl_ind_->incAddCNF(next_win);
        solver_ctrl_ind_->incAddCNF(prev_trans_or_initial_);
        solver_ctrl_ind_->incAddCNF(prev_win);
        clauses_added = winning_region_.getNrOfClauses();
        reset_c_cnt++;
      }
      else
      {
        solver_ctrl_->incAddClause(blocking_clause);
        solver_ctrl_->incAddClause(next_blocking_clause);
        solver_ctrl_ind_->incAddClause(prev_blocking_clause);
        solver_ctrl_ind_->incAddClause(blocking_clause);
        solver_ctrl_ind_->incAddClause(next_blocking_clause);
        clauses_added++;
      }
      precise = false;
      statistics_.notifyAfterCheckCandidateFound(s_.size(), blocking_clause.size());
    }
    else
    {
      // There exists a response of the protagonist to this input vector. Hence, this input
      // vector is useless for the antagonist in trying to go from F to !G'. We need to
      // exclude it from solver_i_ so that the same state-input pair is not tried again.
      // However, instead of excluding this one state-input pair only, we generalize it using
      // an unsatisfiable core. The core gives us all state-input combinations for which
      // the control vector found by solver_ctrl_ prevents that we go from F to !G'.
      statistics_.notifyAfterCheckCandidateFailed();
      vector<int> ctrl(model_or_core);
      statistics_.notifyBeforeRefine();
      sat = solver_i_->incIsSatModelOrCore(state_input, ctrl, vector<int>(), model_or_core);
      MASSERT(sat == false, "Impossible.");
      solver_i_->incAddNegCubeAsClause(model_or_core);
      statistics_.notifyAfterRefine(si_.size(), model_or_core.size());
    }
  }
}

// -------------------------------------------------------------------------------------------
bool LearnSynthSAT::computeWinningRegionRGExp()
{
  AIG2CNF &A2C = AIG2CNF::instance();
  VarManager &VM = VarManager::instance();

  winning_region_.clear();
  winning_region_ = A2C.getSafeStates();
  UnivExpander expander;

  // solver_i_ contains F & T & !F':
  solver_i_->doMinCores(true);
  solver_i_->doRandModels(false);
  bool size_exceeded = expander.resetSolverIExp(winning_region_, solver_i_, true);
  if(size_exceeded)
  {
    L_LOG("Size limit exceeded during expansion.");
    solver_i_->startIncrementalSession(sic_, false);
    solver_i_->incAddCNF(A2C.getNextUnsafeStates());
    solver_i_->incAddCNF(A2C.getTrans());
    solver_i_->incAddCNF(A2C.getSafeStates());
  }

  // solver_ctrl_ind_ contains (I | T* & F*) & F & T & F':
  vector<int> px_unused(s_.size(), 0);
  vector<int> px_neg(s_.size(), 0);
  vector<int> px_act(s_.size(), 0);
  for(size_t cnt = 0; cnt < s_.size(); ++cnt)
  {
    px_unused[cnt] = VM.createFreshPrevVar();
    px_neg[cnt] = VM.createFreshPrevVar();
    px_act[cnt] = VM.createFreshPrevVar();
  }
  solver_ctrl_ind_->doMinCores(false);
  solver_ctrl_ind_->doRandModels(false);
  expander.initSolverCExp(solver_ctrl_ind_, VM.getVarsOfType(VarInfo::PREV));
  solver_ctrl_ind_->incAddCNF(prev_trans_or_initial_);
  solver_ctrl_ind_->incAddUnitClause(-current_to_previous_map_[VM.getPresErrorStateVar()]);
  solver_ctrl_ind_->incAddCNF(A2C.getSafeStates());
  vector<int> safe;
  safe.push_back(-VarManager::instance().getPresErrorStateVar());
  expander.addExpNxtClauseToC(safe, solver_ctrl_ind_);
  // prepare the activated previous state clause:
  px_act.push_back(current_state_is_initial_);
  solver_ctrl_ind_->incAddClause(px_act);
  for(size_t cnt = 0; cnt < s_.size(); ++cnt)
  {
    int prev = current_to_previous_map_[s_[cnt]];
    solver_ctrl_ind_->incAdd2LitClause(-px_unused[cnt], -px_act[cnt]);
    solver_ctrl_ind_->incAdd4LitClause(px_unused[cnt], px_neg[cnt], prev, -px_act[cnt]);
    solver_ctrl_ind_->incAdd4LitClause(px_unused[cnt], px_neg[cnt], -prev, px_act[cnt]);
    solver_ctrl_ind_->incAdd4LitClause(px_unused[cnt], -px_neg[cnt], prev, px_act[cnt]);
    solver_ctrl_ind_->incAdd4LitClause(px_unused[cnt], -px_neg[cnt], -prev, -px_act[cnt]);
  }

  bool precise = true;
  size_t it_cnt = 0;
  size_t clauses_added = 0;
  size_t reset_c_cnt = 0;
  vector<int> model_or_core;
  VM.push();
  while(true)
  {
    ++it_cnt;

    // First, we check if there is some transition with some values for the inputs i and
    // and c from F to !G' (G is a copy of F that is updated only lazily).
    statistics_.notifyBeforeComputeCandidate();
    bool sat = solver_i_->incIsSatModelOrCore(vector<int>(), si_, model_or_core);
    statistics_.notifyAfterComputeCandidate();

    if(!sat)
    {
      // No such transition exists.
      if(precise)
      {
        // G = F. This means that there exists no possibility for the antagonist to leave F.
        // Hence, F is a valid winning region. We can stop.
        return true;
      }
      // G != F. Hence, the reason why no such transition exists could be that G is not up-to
      // data. We set G:=F and start again. For that, we have to restart the incremental
      // of session solver_i_.
      statistics_.notifyRestart();
      L_DBG("Need to restart with fresh U (iteration " << it_cnt << ")");
      if(!size_exceeded)
        size_exceeded = expander.resetSolverIExp(winning_region_, solver_i_, true);
      if(size_exceeded)
      {
        VM.resetToLastPush();
        solver_i_->startIncrementalSession(sic_, false);
        Utils::compressStateCNF(winning_region_);
        CNF leave_win(winning_region_);
        leave_win.swapPresentToNext();
        leave_win.negate();
        leave_win.addCNF(AIG2CNF::instance().getTrans());
        leave_win.addCNF(winning_region_);
        solver_i_->incAddCNF(leave_win);
      }
      precise = true;
      continue;
    }

    vector<int> state_input = model_or_core;
    vector<int> state = Utils::extract(state_input, VarInfo::PRES_STATE);
    vector<int> input = Utils::extract(state_input, VarInfo::INPUT);

    // There exists a transition from F to !G' with some input i. Let's now see if the
    // protagonist can find some c such that leaving F is avoided:
    statistics_.notifyBeforeCheckCandidate();
    // Note that we use solver_ctrl_ and not solver_ctrl_ind_ here. Using solver_ctrl_ind_
    // would also be correct but slower.
    sat = solver_ctrl_ind_->incIsSatModelOrCore(state, input, c_, model_or_core);

    if(!sat)
    {
      // we now try to minimize the core further using reachability information:
      bool changed = true;
      while(changed)
      {
        changed = false;
        vector<int> blocking_clause(model_or_core);
        Utils::negateLiterals(blocking_clause);
        expander.addExpNxtClauseToC(blocking_clause, solver_ctrl_ind_);
        solver_ctrl_ind_->incAddClause(blocking_clause);

        vector<int> orig_core(model_or_core);
        //Utils::randomize(orig_core);
        for(size_t lit_cnt = 0; lit_cnt < orig_core.size(); ++lit_cnt)
        {
          vector<int> tmp(model_or_core);
          Utils::remove(tmp, orig_core[lit_cnt]);

          vector<int> assumptions;
          assumptions.reserve(input.size() + tmp.size() + s_.size());
          assumptions.insert(assumptions.end(), input.begin(), input.end());
          assumptions.insert(assumptions.end(), tmp.begin(), tmp.end());
          // build the previous state-copy of tmp using the activation variables:
          for(size_t s_cnt = 0; s_cnt < s_.size(); ++s_cnt)
          {
            if(Utils::contains(tmp, s_[s_cnt]))
              assumptions.push_back(px_neg[s_cnt]);
            else if(Utils::contains(tmp, -s_[s_cnt]))
              assumptions.push_back(-px_neg[s_cnt]);
            else
              assumptions.push_back(px_unused[s_cnt]);
          }
          sat = solver_ctrl_ind_->incIsSat(assumptions);
          if(!sat)
          {
            model_or_core = tmp;
            changed = true;
          }
        }
      }

      if(Utils::containsInit(model_or_core))
        return false;

      // We compute the corresponding blocking clause, and update the winning region and the
      // solvers:
      vector<int> blocking_clause(model_or_core);
      Utils::negateLiterals(blocking_clause);
      vector<int> prev_blocking_clause(blocking_clause);
      presentToPrevious(prev_blocking_clause);
      winning_region_.addClauseAndSimplify(blocking_clause);
      solver_i_->incAddClause(blocking_clause);
      solver_ctrl_ind_->incAddClause(prev_blocking_clause);
      clauses_added++;

      // reseting the solver_ctrl_ind_ seems to help.
      // TODO: find out good numbers for the heuristics when to restart
      if(clauses_added > winning_region_.getNrOfClauses() + 100)
      {
        if(reset_c_cnt % 1000 == 999)
          Utils::compressStateCNF(winning_region_, true);
        else if(reset_c_cnt % 100 == 99)
          Utils::compressStateCNF(winning_region_, false);
        CNF prev_win(winning_region_);
        presentToPrevious(prev_win);
        expander.resetSolverCExp(solver_ctrl_ind_);
        solver_ctrl_ind_->incAddCNF(prev_trans_or_initial_);
        solver_ctrl_ind_->incAddCNF(prev_win);
        solver_ctrl_ind_->incAddCNF(winning_region_);
        const list<vector<int> > &cl = winning_region_.getClauses();
        for(CNF::ClauseConstIter it = cl.begin(); it != cl.end(); ++it)
          expander.addExpNxtClauseToC(*it, solver_ctrl_ind_);
        solver_ctrl_ind_->incAddClause(px_act);
        for(size_t cnt = 0; cnt < s_.size(); ++cnt)
        {
          int prev = current_to_previous_map_[s_[cnt]];
          solver_ctrl_ind_->incAdd2LitClause(-px_unused[cnt], -px_act[cnt]);
          solver_ctrl_ind_->incAdd4LitClause(px_unused[cnt], px_neg[cnt], prev, -px_act[cnt]);
          solver_ctrl_ind_->incAdd4LitClause(px_unused[cnt], px_neg[cnt], -prev, px_act[cnt]);
          solver_ctrl_ind_->incAdd4LitClause(px_unused[cnt], -px_neg[cnt], prev, px_act[cnt]);
          solver_ctrl_ind_->incAdd4LitClause(px_unused[cnt], -px_neg[cnt], -prev, -px_act[cnt]);
        }

        clauses_added = winning_region_.getNrOfClauses();
        reset_c_cnt++;
      }
      precise = false;
      statistics_.notifyAfterCheckCandidateFound(s_.size(), blocking_clause.size());
    }
    else
    {
      MASSERT(size_exceeded, "Since we expanded over all ctrl-signals, this is impossible.");
      // There exists a response of the protagonist to this input vector. Hence, this input
      // vector is useless for the antagonist in trying to go from F to !G'. We need to
      // exclude it from solver_i_ so that the same state-input pair is not tried again.
      // However, instead of excluding this one state-input pair only, we generalize it using
      // an unsatisfiable core. The core gives us all state-input combinations for which
      // the control vector found by solver_ctrl_ prevents that we go from F to !G'.
      statistics_.notifyAfterCheckCandidateFailed();
      vector<int> ctrl(model_or_core);
      statistics_.notifyBeforeRefine();
      sat = solver_i_->incIsSatModelOrCore(state_input, ctrl, vector<int>(), model_or_core);
      MASSERT(sat == false, "Impossible.");
      solver_i_->incAddNegCubeAsClause(model_or_core);
      statistics_.notifyAfterRefine(si_.size(), model_or_core.size());
    }
  }
}



// -------------------------------------------------------------------------------------------
bool LearnSynthSAT::computeWinningRegionRGRecy()
{
  AIG2CNF& A2C = AIG2CNF::instance();
  VarManager &VM = VarManager::instance();
  vector<int> none;
  L_DBG("Nr. of state variables: " << VM.getVarsOfType(VarInfo::PRES_STATE).size());
  L_DBG("Nr. of uncontrollable inputs: " << VM.getVarsOfType(VarInfo::INPUT).size());
  L_DBG("Nr. of controllable inputs: " << VM.getVarsOfType(VarInfo::CTRL).size());

  const vector<int> &s = VM.getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &c = VM.getVarsOfType(VarInfo::CTRL);
  const vector<int> &i = VM.getVarsOfType(VarInfo::INPUT);
  vector<int> si;
  si.reserve(s.size() + i.size());
  si.insert(si.end(), s.begin(), s.end());
  si.insert(si.end(), i.begin(), i.end());
  vector<int> vars_to_keep = VM.getAllNonTempVars();

  winning_region_.clear();
  winning_region_ = A2C.getSafeStates();
  winning_region_large_.clear();
  winning_region_large_ = A2C.getSafeStates();

  // solver_i_ contains F & T & !F':
  solver_i_->doMinCores(true);
  solver_i_->doRandModels(false);
  solver_i_->startIncrementalSession(vars_to_keep, false);
  solver_i_->incAddCNF(A2C.getNextUnsafeStates());
  solver_i_->incAddCNF(A2C.getTrans());
  solver_i_->incAddCNF(A2C.getSafeStates());

  // solver_ctrl_ contains F & T & F':
  // We set doMinCores to false because we minimize the core we get from solver_ctrl_ further
  // with solver_ctrl_ind_ anyway:
  solver_ctrl_->doMinCores(false);
  solver_ctrl_->doRandModels(false);
  solver_ctrl_->startIncrementalSession(vars_to_keep, false);
  solver_ctrl_->incAddCNF(A2C.getNextSafeStates());
  solver_ctrl_->incAddCNF(A2C.getTrans());
  solver_ctrl_->incAddCNF(A2C.getSafeStates());

  // solver_ctrl_ind_ contains (I | T* & F*) & F & T & F':
  solver_ctrl_ind_->doMinCores(true);
  solver_ctrl_ind_->doRandModels(false);
  solver_ctrl_ind_->startIncrementalSession(vars_to_keep, true);
  solver_ctrl_ind_->incAddCNF(A2C.getNextSafeStates());
  solver_ctrl_ind_->incAddCNF(A2C.getTrans());
  solver_ctrl_ind_->incAddCNF(A2C.getSafeStates());
  solver_ctrl_ind_->incAddCNF(prev_trans_or_initial_);
  solver_ctrl_ind_->incAddUnitClause(-current_to_previous_map_[VM.getPresErrorStateVar()]);
  bool precise = true;

  vector<vector<int> > hist_si;
  vector<vector<int> > hist_ctrl;

  size_t it_cnt = 0;
  vector<int> model_or_core;
  while(true)
  {
    ++it_cnt;

    // First, we check if there is some transition with some values for the inputs i and
    // and c from F to !G' (G is a copy of F that is updated only lazily).
    statistics_.notifyBeforeComputeCandidate();
    bool sat = solver_i_->incIsSatModelOrCore(vector<int>(), si, model_or_core);
    statistics_.notifyAfterComputeCandidate();

    if(!sat)
    {
      // No such transition exists.
      if(precise)
      {
        // G = F. This means that there exists no possibility for the antagonist to leave F.
        // Hence, F is a valid winning region. We can stop.
        return true;
      }
      // G != F. Hence, the reason why no such transition exists could be that G is not up-to
      // data. We set G:=F and start again. For that, we have to restart the incremental
      // of session solver_i_.
      statistics_.notifyRestart();
      L_DBG("Need to restart with fresh U (iteration " << it_cnt << ")");
      // get rid of all temporary variables introduced during negations:
      VM.resetToLastPush();
      solver_i_->startIncrementalSession(vars_to_keep, false);
      Utils::compressStateCNF(winning_region_);
      CNF leave_win(winning_region_);
      leave_win.swapPresentToNext();
      leave_win.negate();
      leave_win.addCNF(AIG2CNF::instance().getTrans());
      leave_win.addCNF(winning_region_);
      //leave_win.addCNF(winning_region_large_);
      solver_i_->incAddCNF(leave_win);

      // let's see how many U-clauses we can recycle:
      for(size_t cnt = 0; cnt < hist_si.size(); ++cnt)
      {
        if(winning_region_.isSatBy(hist_si[cnt]) &&
           !solver_i_->incIsSatModelOrCore(hist_si[cnt], hist_ctrl[cnt], none, model_or_core))
        {
            solver_i_->incAddNegCubeAsClause(model_or_core);
        }
        else
        {
          hist_si[cnt].swap(hist_si.back());
          hist_si.pop_back();
          hist_ctrl[cnt].swap(hist_ctrl.back());
          hist_ctrl.pop_back();
        }
      }

      precise = true;
      continue;
    }

    vector<int> state_input = model_or_core;
    vector<int> state = Utils::extract(state_input, VarInfo::PRES_STATE);
    vector<int> input = Utils::extract(state_input, VarInfo::INPUT);

    // There exists a transition from F to !G' with some input i. Let's now see if the
    // protagonist can find some c such that leaving F is avoided:
    statistics_.notifyBeforeCheckCandidate();
    // Note that we use solver_ctrl_ and not solver_ctrl_ind_ here. Using solver_ctrl_ind_
    // would also be correct but slower.
    sat = solver_ctrl_->incIsSatModelOrCore(state, input, c, model_or_core);
    if(!sat)
    {
      // No such response exists. This means that we have found a counterexample-state.
      // The solver solver_ctrl_ already computed a generalization of this
      // counterexample-state in form of an unsatisfiable core. The unsatisfiable core
      // represents the set of all state for which input i enforces that F is left.
      // (This is weaker than what we compute with the QBF-solver in LearnSynthQBF, but
      // it appears to be good enough in our experiments, and the computation is fast.)

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

      if(Utils::containsInit(model_or_core))
        return false;
      // We compute the corresponding blocking clause, and update the winning region and the
      // solvers:
      vector<int> blocking_clause(model_or_core);
      for(size_t cnt = 0; cnt < blocking_clause.size(); ++cnt)
        blocking_clause[cnt] = -blocking_clause[cnt];
      vector<int> prev_blocking_clause(blocking_clause);
      presentToPrevious(prev_blocking_clause);
      vector<int> next_blocking_clause(blocking_clause);
      Utils::swapPresentToNext(next_blocking_clause);
      winning_region_.addClauseAndSimplify(blocking_clause);
      winning_region_large_.addClauseAndSimplify(blocking_clause);
      solver_i_->incAddClause(blocking_clause);
      solver_ctrl_->incAddClause(blocking_clause);
      solver_ctrl_->incAddClause(next_blocking_clause);
      solver_ctrl_ind_->incAddClause(prev_blocking_clause);
      solver_ctrl_ind_->incAddClause(blocking_clause);
      solver_ctrl_ind_->incAddClause(next_blocking_clause);
      precise = false;
      statistics_.notifyAfterCheckCandidateFound(s.size(), blocking_clause.size());
    }
    else
    {
      // There exists a response of the protagonist to this input vector. Hence, this input
      // vector is useless for the antagonist in trying to go from F to !G'. We need to
      // exclude it from solver_i_ so that the same state-input pair is not tried again.
      // However, instead of excluding this one state-input pair only, we generalize it using
      // an unsatisfiable core. The core gives us all state-input combinations for which
      // the control vector found by solver_ctrl_ prevents that we go from F to !G'.
      statistics_.notifyAfterCheckCandidateFailed();
      vector<int> ctrl(model_or_core);
      statistics_.notifyBeforeRefine();
      hist_si.push_back(state_input);
      hist_ctrl.push_back(ctrl);
      sat = solver_i_->incIsSatModelOrCore(state_input, ctrl, vector<int>(), model_or_core);
      MASSERT(sat == false, "Impossible.");
      for(size_t cnt = 0; cnt < model_or_core.size(); ++cnt)
        model_or_core[cnt] = -model_or_core[cnt];
      solver_i_->incAddClause(model_or_core);
      statistics_.notifyAfterRefine(si.size(), model_or_core.size());
    }
  }
}


// -------------------------------------------------------------------------------------------
bool LearnSynthSAT::computeWinningRegionRGRC()
{

  AIG2CNF& A2C = AIG2CNF::instance();
  VarManager &VM = VarManager::instance();
  L_DBG("Nr. of state variables: " << VM.getVarsOfType(VarInfo::PRES_STATE).size());
  L_DBG("Nr. of uncontrollable inputs: " << VM.getVarsOfType(VarInfo::INPUT).size());
  L_DBG("Nr. of controllable inputs: " << VM.getVarsOfType(VarInfo::CTRL).size());

  const vector<int> &s = VM.getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &c = VM.getVarsOfType(VarInfo::CTRL);
  const vector<int> &i = VM.getVarsOfType(VarInfo::INPUT);
  vector<int> si;
  si.reserve(s.size() + i.size());
  si.insert(si.end(), s.begin(), s.end());
  si.insert(si.end(), i.begin(), i.end());
  vector<int> vars_to_keep = VM.getAllNonTempVars();

  winning_region_.clear();
  winning_region_ = A2C.getSafeStates();
  winning_region_large_.clear();
  winning_region_large_ = A2C.getSafeStates();

  // solver_i_ contains F & T & !F':
  solver_i_->doMinCores(false);
  solver_i_->doRandModels(false);
  solver_i_->startIncrementalSession(vars_to_keep, false);
  solver_i_->incAddCNF(A2C.getNextUnsafeStates());
  solver_i_->incAddCNF(A2C.getTrans());
  solver_i_->incAddCNF(A2C.getSafeStates());

  // solver_ctrl_ind_ contains (I | T* & F*) & F & T & !F':
  solver_i_ind_->doMinCores(true);
  solver_i_ind_->doRandModels(false);
  solver_i_ind_->startIncrementalSession(vars_to_keep, false);
  solver_i_ind_->incAddCNF(A2C.getNextUnsafeStates());
  solver_i_ind_->incAddCNF(A2C.getTrans());
  solver_i_ind_->incAddCNF(A2C.getSafeStates());
  solver_i_ind_->incAddCNF(prev_trans_or_initial_);
  solver_i_ind_->incAddUnitClause(-current_to_previous_map_[VM.getPresErrorStateVar()]);
  solver_i_ind_->incAddCNF(different_from_prev_or_initial_);

  // solver_ctrl_ contains F & T & F':
  // We set doMinCores to false because we minimize the core we get from solver_ctrl_ further
  // with solver_ctrl_ind_ anyway:
  solver_ctrl_->doMinCores(false);
  solver_ctrl_->doRandModels(false);
  solver_ctrl_->startIncrementalSession(vars_to_keep, false);
  solver_ctrl_->incAddCNF(A2C.getNextSafeStates());
  solver_ctrl_->incAddCNF(A2C.getTrans());
  solver_ctrl_->incAddCNF(A2C.getSafeStates());

  // solver_ctrl_ind_ contains (I | T* & F*) & F & T & F':
  solver_ctrl_ind_->doMinCores(false);
  solver_ctrl_ind_->doRandModels(false);
  solver_ctrl_ind_->startIncrementalSession(vars_to_keep, true);
  solver_ctrl_ind_->incAddCNF(A2C.getNextSafeStates());
  solver_ctrl_ind_->incAddCNF(A2C.getTrans());
  solver_ctrl_ind_->incAddCNF(A2C.getSafeStates());
  solver_ctrl_ind_->incAddCNF(prev_trans_or_initial_);
  solver_ctrl_ind_->incAddUnitClause(-current_to_previous_map_[VM.getPresErrorStateVar()]);
  bool precise = true;

  size_t it_cnt = 0;
  vector<int> model_or_core;
  while(true)
  {
    ++it_cnt;

    // First, we check if there is some transition with some values for the inputs i and
    // and c from F to !G' (G is a copy of F that is updated only lazily).
    statistics_.notifyBeforeComputeCandidate();
    bool sat = solver_i_ind_->incIsSatModelOrCore(vector<int>(), si, model_or_core);
    statistics_.notifyAfterComputeCandidate();

    if(!sat)
    {
      // No such transition exists.
      if(precise)
      {
        // G = F. This means that there exists no possibility for the antagonist to leave F.
        // Hence, F is a valid winning region. We can stop.
        return true;
      }
      // G != F. Hence, the reason why no such transition exists could be that G is not up-to
      // data. We set G:=F and start again. For that, we have to restart the incremental
      // of session solver_i_.
      statistics_.notifyRestart();
      L_DBG("Need to restart with fresh U (iteration " << it_cnt << ")");
      // get rid of all temporary variables introduced during negations:
      VM.resetToLastPush();
      solver_i_->startIncrementalSession(vars_to_keep, false);
      Utils::compressStateCNF(winning_region_);
      CNF leave_win(winning_region_);
      leave_win.swapPresentToNext();
      leave_win.negate();
      leave_win.addCNF(AIG2CNF::instance().getTrans());
      leave_win.addCNF(winning_region_);
      //leave_win.addCNF(winning_region_large_);
      solver_i_->incAddCNF(leave_win);

      solver_i_ind_->startIncrementalSession(vars_to_keep, false);
      solver_i_ind_->incAddCNF(leave_win);
      solver_i_ind_->incAddCNF(prev_trans_or_initial_);
      CNF prev_win(winning_region_);
      presentToPrevious(prev_win);
      solver_i_ind_->incAddCNF(prev_win);
      solver_i_ind_->incAddCNF(different_from_prev_or_initial_);
      precise = true;
      continue;
    }

    vector<int> state_input = model_or_core;
    vector<int> state = Utils::extract(state_input, VarInfo::PRES_STATE);
    vector<int> input = Utils::extract(state_input, VarInfo::INPUT);

    // There exists a transition from F to !G' with some input i. Let's now see if the
    // protagonist can find some c such that leaving F is avoided:
    statistics_.notifyBeforeCheckCandidate();
    // Note that we use solver_ctrl_ and not solver_ctrl_ind_ here. Using solver_ctrl_ind_
    // would also be correct but slower.
    sat = solver_ctrl_->incIsSatModelOrCore(state, input, c, model_or_core);
    if(!sat)
    {
      // No such response exists. This means that we have found a counterexample-state.
      // The solver solver_ctrl_ already computed a generalization of this
      // counterexample-state in form of an unsatisfiable core. The unsatisfiable core
      // represents the set of all state for which input i enforces that F is left.
      // (This is weaker than what we compute with the QBF-solver in LearnSynthQBF, but
      // it appears to be good enough in our experiments, and the computation is fast.)

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

      if(Utils::containsInit(model_or_core))
        return false;
      // We compute the corresponding blocking clause, and update the winning region and the
      // solvers:
      vector<int> blocking_clause(model_or_core);
      for(size_t cnt = 0; cnt < blocking_clause.size(); ++cnt)
        blocking_clause[cnt] = -blocking_clause[cnt];
      vector<int> prev_blocking_clause(blocking_clause);
      presentToPrevious(prev_blocking_clause);
      vector<int> next_blocking_clause(blocking_clause);
      Utils::swapPresentToNext(next_blocking_clause);
      winning_region_.addClauseAndSimplify(blocking_clause);
      winning_region_large_.addClauseAndSimplify(blocking_clause);
      solver_i_->incAddClause(blocking_clause);
      solver_i_ind_->incAddClause(blocking_clause);
      solver_i_ind_->incAddClause(prev_blocking_clause);
      solver_ctrl_->incAddClause(blocking_clause);
      solver_ctrl_->incAddClause(next_blocking_clause);
      solver_ctrl_ind_->incAddClause(prev_blocking_clause);
      solver_ctrl_ind_->incAddClause(blocking_clause);
      solver_ctrl_ind_->incAddClause(next_blocking_clause);
      precise = false;
      statistics_.notifyAfterCheckCandidateFound(s.size(), blocking_clause.size());
    }
    else
    {
      // There exists a response of the protagonist to this input vector. Hence, this input
      // vector is useless for the antagonist in trying to go from F to !G'. We need to
      // exclude it from solver_i_ so that the same state-input pair is not tried again.
      // However, instead of excluding this one state-input pair only, we generalize it using
      // an unsatisfiable core. The core gives us all state-input combinations for which
      // the control vector found by solver_ctrl_ prevents that we go from F to !G'.
      statistics_.notifyAfterCheckCandidateFailed();
      vector<int> ctrl(model_or_core);
      statistics_.notifyBeforeRefine();

      // A two-step minimization seems to be better than doing everything in one go:
      sat = solver_i_->incIsSatModelOrCore(state_input, ctrl, vector<int>(), model_or_core);
      MASSERT(sat == false, "Impossible.");
      vector<int> core = model_or_core;
      sat = solver_i_ind_->incIsSatModelOrCore(core, ctrl, vector<int>(), model_or_core);
      MASSERT(sat == false, "Impossible.");
      for(size_t cnt = 0; cnt < model_or_core.size(); ++cnt)
        model_or_core[cnt] = -model_or_core[cnt];
      solver_i_->incAddClause(model_or_core);
      solver_i_ind_->incAddClause(model_or_core);
      statistics_.notifyAfterRefine(si.size(), model_or_core.size());
    }
  }
}


// -------------------------------------------------------------------------------------------
bool LearnSynthSAT::computeWinningRegionRGRCExp()
{
  AIG2CNF &A2C = AIG2CNF::instance();
  VarManager &VM = VarManager::instance();

  winning_region_.clear();
  winning_region_ = A2C.getSafeStates();
  UnivExpander expander;

  solver_i_ind_->doMinCores(true);
  solver_i_ind_->doRandModels(false);
  expander.resetSolverIExp(winning_region_, solver_i_ind_);
  solver_i_ind_->incAddCNF(prev_trans_or_initial_);
  solver_i_ind_->incAddUnitClause(-current_to_previous_map_[VM.getPresErrorStateVar()]);
  solver_i_ind_->incAddCNF(different_from_prev_or_initial_);


  // solver_ctrl_ind_ contains (I | T* & F*) & F & T & F':
  solver_ctrl_ind_->doMinCores(true);
  solver_ctrl_ind_->doRandModels(false);
  expander.initSolverCExp(solver_ctrl_ind_, VM.getVarsOfType(VarInfo::PREV));
  solver_ctrl_ind_->incAddCNF(prev_trans_or_initial_);
  solver_ctrl_ind_->incAddUnitClause(-current_to_previous_map_[VM.getPresErrorStateVar()]);
  solver_ctrl_ind_->incAddCNF(A2C.getSafeStates());
  vector<int> safe;
  safe.push_back(-VarManager::instance().getPresErrorStateVar());
  expander.addExpNxtClauseToC(safe, solver_ctrl_ind_);
  bool precise = true;

  size_t it_cnt = 0;
  size_t clauses_added = 0;
  size_t reset_c_cnt = 0;
  vector<int> model_or_core;
  while(true)
  {
    ++it_cnt;

    // First, we check if there is some transition with some values for the inputs i and
    // and c from F to !G' (G is a copy of F that is updated only lazily).
    statistics_.notifyBeforeComputeCandidate();
    bool sat = solver_i_ind_->incIsSatModelOrCore(vector<int>(), si_, model_or_core);
    statistics_.notifyAfterComputeCandidate();

    if(!sat)
    {
      // No such transition exists.
      if(precise)
      {
        // G = F. This means that there exists no possibility for the antagonist to leave F.
        // Hence, F is a valid winning region. We can stop.
        return true;
      }
      // G != F. Hence, the reason why no such transition exists could be that G is not up-to
      // data. We set G:=F and start again. For that, we have to restart the incremental
      // of session solver_i_.
      statistics_.notifyRestart();
      L_DBG("Need to restart with fresh U (iteration " << it_cnt << ")");
      expander.resetSolverIExp(winning_region_, solver_i_ind_);
      solver_i_ind_->incAddCNF(prev_trans_or_initial_);
      CNF prev_win(winning_region_);
      presentToPrevious(prev_win);
      solver_i_ind_->incAddCNF(prev_win);
      solver_i_ind_->incAddCNF(different_from_prev_or_initial_);
      precise = true;
      continue;
    }

    vector<int> state_input = model_or_core;
    vector<int> state = Utils::extract(state_input, VarInfo::PRES_STATE);
    vector<int> input = Utils::extract(state_input, VarInfo::INPUT);

    // There exists a transition from F to !G' with some input i. Let's now see if the
    // protagonist can find some c such that leaving F is avoided:
    statistics_.notifyBeforeCheckCandidate();
    // Note that we use solver_ctrl_ and not solver_ctrl_ind_ here. Using solver_ctrl_ind_
    // would also be correct but slower.
    sat = solver_ctrl_ind_->incIsSatModelOrCore(state, input, c_, model_or_core);
    MASSERT(!sat, "Since we expanded over all ctrl-signals, this is impossible.");

    // No such response exists. This means that we have found a counterexample-state.
    // The solver solver_ctrl_ already computed a generalization of this
    // counterexample-state in form of an unsatisfiable core. The unsatisfiable core
    // represents the set of all state for which input i (and some other inputs due to
    // partial expansion) enforces that F is left.


    // BEGIN second minimization round:
    //if(Utils::containsInit(model_or_core))
    //  return false;
    //vector<int> first_blocking_clause(model_or_core);
    //Utils::negateLiterals(first_blocking_clause);
    //vector<int> first_prev_blocking_clause(first_blocking_clause);
    //presentToPrevious(first_prev_blocking_clause);
    //solver_ctrl_ind_->incAddClause(first_prev_blocking_clause);
    //solver_ctrl_ind_->incAddClause(first_blocking_clause);
    //expander.addExpNxtClauseToC(first_blocking_clause, solver_ctrl_ind_);
    //vector<int> smallest_so_far(model_or_core);
    //sat = solver_ctrl_ind_->incIsSatModelOrCore(smallest_so_far, input, c_, model_or_core);
    //MASSERT(!sat, "Since we expanded over all ctrl-signals, this is impossible.");
    // END second minimization round

    if(Utils::containsInit(model_or_core))
      return false;

    // We compute the corresponding blocking clause, and update the winning region and the
    // solvers:
    vector<int> blocking_clause(model_or_core);
    Utils::negateLiterals(blocking_clause);
    vector<int> prev_blocking_clause(blocking_clause);
    presentToPrevious(prev_blocking_clause);
    winning_region_.addClauseAndSimplify(blocking_clause);
    solver_i_ind_->incAddClause(prev_blocking_clause);
    solver_i_ind_->incAddClause(blocking_clause);

    // reseting the solver_ctrl_ind_ seems to help.
    // TODO: find out good numbers for the heuristics when to restart
    if(clauses_added > winning_region_.getNrOfClauses() + 100)
    {
      if(reset_c_cnt % 1000 == 999)
        Utils::compressStateCNF(winning_region_, true);
      else if(reset_c_cnt % 100 == 99)
        Utils::compressStateCNF(winning_region_, false);
      CNF prev_win(winning_region_);
      presentToPrevious(prev_win);
      expander.resetSolverCExp(solver_ctrl_ind_);
      solver_ctrl_ind_->incAddCNF(prev_trans_or_initial_);
      solver_ctrl_ind_->incAddCNF(prev_win);
      solver_ctrl_ind_->incAddCNF(winning_region_);
      const list<vector<int> > &cl = winning_region_.getClauses();
      for(CNF::ClauseConstIter it = cl.begin(); it != cl.end(); ++it)
        expander.addExpNxtClauseToC(*it, solver_ctrl_ind_);
      clauses_added = winning_region_.getNrOfClauses();
      reset_c_cnt++;
    }
    else
    {
      solver_ctrl_ind_->incAddClause(prev_blocking_clause);
      solver_ctrl_ind_->incAddClause(blocking_clause);
      expander.addExpNxtClauseToC(blocking_clause, solver_ctrl_ind_);
      clauses_added++;
    }
    precise = false;
    statistics_.notifyAfterCheckCandidateFound(s_.size(), blocking_clause.size());
  }
}



// -------------------------------------------------------------------------------------------
int LearnSynthSAT::presentToPrevious(int literal) const
{
  int var = (literal < 0) ? -literal : literal;
  if(literal < 0)
    return -current_to_previous_map_[var];
  else
    return current_to_previous_map_[var];
}

// -------------------------------------------------------------------------------------------
void LearnSynthSAT::presentToPrevious(vector<int> &cube_or_clause) const
{
  for(size_t cnt = 0; cnt < cube_or_clause.size(); ++cnt)
    cube_or_clause[cnt] = presentToPrevious(cube_or_clause[cnt]);
}

// -------------------------------------------------------------------------------------------
void LearnSynthSAT::presentToPrevious(CNF &cnf) const
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
void LearnSynthSAT::debugCheckWinReg()
{
#ifndef NDEBUG
  L_DBG("Checking winning region for correctness ...");
  LingelingApi sat_solver;
  CNF check(winning_region_);
  check.addCNF(AIG2CNF::instance().getInitial());
  MASSERT(sat_solver.isSat(check), "Initial state is not winning.");

  // All states of the winning region must be safe
  check.clear();
  check.addCNF(winning_region_);
  check.addCNF(AIG2CNF::instance().getUnsafeStates());
  MASSERT(!sat_solver.isSat(check), "Winning region is not safe.");

  // for all potentially reachable state in the winning region, we can enforce to stay in the
  // winning region:
  // exists x*,i*,c*: exists x,i: forall c: exists x':
  //   (I(x) | (W(x*) & T(x*,i*,c*,x))) & W(x) & T(x,i,c,x') & !W(x')
  // must be unsat
  check.clear();
  check.addCNF(winning_region_);
  check.swapPresentToNext();
  check.negate();
  check.addCNF(winning_region_);
  check.addCNF(AIG2CNF::instance().getTrans());
  check.addCNF(prev_trans_or_initial_);
  check.addCNF(different_from_prev_or_initial_);
  CNF prev_win(winning_region_);
  presentToPrevious(prev_win);
  check.addCNF(prev_win);

  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > check_quant;
  check_quant.push_back(make_pair(VarInfo::PREV, QBFSolver::E));
  check_quant.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::E));
  check_quant.push_back(make_pair(VarInfo::INPUT, QBFSolver::E));
  check_quant.push_back(make_pair(VarInfo::CTRL, QBFSolver::A));
  check_quant.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::E));
  check_quant.push_back(make_pair(VarInfo::TMP, QBFSolver::E));
  DepQBFApi qbf_solver(false);
  MASSERT(!qbf_solver.isSat(check_quant, check), "Winning region is not inductive.");
#endif
}

