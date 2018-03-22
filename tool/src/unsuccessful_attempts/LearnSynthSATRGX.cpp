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

  size_t max_back = 1;

  AIG2CNF& A2C = AIG2CNF::instance();
  VarManager &VM = VarManager::instance();
  vector<int> t = VM.getVarsOfType(VarInfo::TMP);

  current_to_previous_map_.resize(max_back);
  prev_trans_or_initial_.resize(max_back);
  current_state_is_initial_.resize(max_back, 0);
  int orig_nr_of_vars = VM.getMaxCNFVar() + 1;
  for(size_t pcnt = 0; pcnt < max_back; ++pcnt)
  {
    current_to_previous_map_[pcnt].resize(orig_nr_of_vars, 0);
    for(size_t v_cnt = 0; v_cnt < s_.size(); ++v_cnt)
      current_to_previous_map_[pcnt][s_[v_cnt]] = VM.createFreshPrevVar();
    for(size_t v_cnt = 0; v_cnt < c_.size(); ++v_cnt)
      current_to_previous_map_[pcnt][c_[v_cnt]] = VM.createFreshTmpVar();
    for(size_t v_cnt = 0; v_cnt < i_.size(); ++v_cnt)
      current_to_previous_map_[pcnt][i_[v_cnt]] = VM.createFreshTmpVar();
    for(size_t v_cnt = 0; v_cnt < t.size(); ++v_cnt)
      current_to_previous_map_[pcnt][t[v_cnt]] = VM.createFreshTmpVar();
    if(pcnt == 0)
    {
      for(size_t v_cnt = 0; v_cnt < n_.size(); ++v_cnt)
        current_to_previous_map_[pcnt][n_[v_cnt]] = s_[v_cnt];
    }
    else
    {
      for(size_t v_cnt = 0; v_cnt < n_.size(); ++v_cnt)
        current_to_previous_map_[pcnt][n_[v_cnt]] = current_to_previous_map_[pcnt-1][s_[v_cnt]];
    }

    current_state_is_initial_[pcnt] = VM.createFreshTmpVar();
    list<vector<int> > prev_trans_clauses = A2C.getTrans().getClauses();
    for(CNF::ClauseIter it = prev_trans_clauses.begin(); it != prev_trans_clauses.end(); ++it)
    {
      presentToPrevious(*it, pcnt);
      it->push_back(current_state_is_initial_[pcnt]);
    }
    prev_trans_or_initial_[pcnt].swapWith(prev_trans_clauses);
    // if one of the state variables is true, then current_state_is_initial must be false:
    if(pcnt == 0)
    {
      for(size_t cnt = 0; cnt < s_.size(); ++cnt)
        prev_trans_or_initial_[pcnt].add2LitClause(-s_[cnt], -current_state_is_initial_[pcnt]);
    }
    else
    {
      for(size_t cnt = 0; cnt < s_.size(); ++cnt)
      {
        int current_state = presentToPrevious(s_[cnt], pcnt - 1);
        prev_trans_or_initial_[pcnt].add2LitClause(-current_state, -current_state_is_initial_[pcnt]);
      }
    }
  }

  if(max_back > 0)
  {
    // different_from_prev_or_initial_ should be true if the current state is different from the
    // initial state.
    vector<int> one_is_diff;
    one_is_diff.reserve(s_.size() + 1);
    one_is_diff.push_back(current_state_is_initial_[0]);
    for(size_t cnt = 0; cnt < s_.size(); ++cnt)
    {
      int diff = VM.createFreshTmpVar();
      one_is_diff.push_back(diff);
      int curr = s_[cnt];
      int prev = presentToPrevious(curr, 0);
      // actually, we only need one direction of the implication:
      different_from_prev_or_initial_.add3LitClause(curr, prev, -diff);
      different_from_prev_or_initial_.add3LitClause(-curr, -prev, -diff);
      //different_from_prev_or_initial_.add3LitClause(-curr, prev, diff);
      //different_from_prev_or_initial_.add3LitClause(curr, -prev, diff);
    }
    different_from_prev_or_initial_.addClause(one_is_diff);
  }

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
    //if(Options::instance().getBackEndMode() == 5)
      real =  computeWinningRegionRGExp2();
    //else
    //  real =  computeWinningRegionRGExp();
    if(real)
      Utils::debugCheckWinReg(winning_region_);
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
  solver_ctrl_ind_->incAddCNF(prev_trans_or_initial_[0]);
  solver_ctrl_ind_->incAddUnitClause(-current_to_previous_map_[0][VM.getPresErrorStateVar()]);
  solver_ctrl_ind_->incAddCNF(A2C.getSafeStates());
  vector<int> safe;
  safe.push_back(-VarManager::instance().getPresErrorStateVar());
  expander.addExpNxtClauseToC(safe, solver_ctrl_ind_);
  // prepare the activated previous state clause:
  px_act.push_back(current_state_is_initial_[0]);
  solver_ctrl_ind_->incAddClause(px_act);
  for(size_t cnt = 0; cnt < s_.size(); ++cnt)
  {
    int prev = current_to_previous_map_[0][s_[cnt]];
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
      presentToPrevious(prev_blocking_clause, 0);
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
        presentToPrevious(prev_win, 0);
        expander.resetSolverCExp(solver_ctrl_ind_);
        solver_ctrl_ind_->incAddCNF(prev_trans_or_initial_[0]);
        solver_ctrl_ind_->incAddCNF(prev_win);
        solver_ctrl_ind_->incAddCNF(winning_region_);
        const list<vector<int> > &cl = winning_region_.getClauses();
        for(CNF::ClauseConstIter it = cl.begin(); it != cl.end(); ++it)
          expander.addExpNxtClauseToC(*it, solver_ctrl_ind_);
        solver_ctrl_ind_->incAddClause(px_act);
        for(size_t cnt = 0; cnt < s_.size(); ++cnt)
        {
          int prev = current_to_previous_map_[0][s_[cnt]];
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
bool LearnSynthSAT::computeWinningRegionRGExp2()
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
    solver_i_->startIncrementalSession(sic_, false);
    solver_i_->incAddCNF(A2C.getNextUnsafeStates());
    solver_i_->incAddCNF(A2C.getTrans());
    solver_i_->incAddCNF(A2C.getSafeStates());
  }

  // solver_ctrl_ind_ contains (I | T* & F*) & F & T & F':
  size_t max_back = prev_trans_or_initial_.size();
  vector<vector<int> > px_unused(max_back, vector<int>(s_.size(), 0));
  vector<vector<int> > px_neg(max_back, vector<int>(s_.size(), 0));
  vector<vector<int> > px_act(max_back, vector<int>(s_.size(), 0));
  for(size_t pcnt = 0; pcnt < max_back; ++pcnt)
  {
    for(size_t cnt = 0; cnt < s_.size(); ++cnt)
    {
      px_unused[pcnt][cnt] = VM.createFreshPrevVar();
      px_neg[pcnt][cnt] = VM.createFreshPrevVar();
      px_act[pcnt][cnt] = VM.createFreshPrevVar();
    }
  }
  solver_ctrl_ind_->doMinCores(false);
  solver_ctrl_ind_->doRandModels(false);
  expander.initSolverCExp(solver_ctrl_ind_, VM.getVarsOfType(VarInfo::PREV));
  solver_ctrl_ind_->incAddCNF(A2C.getSafeStates());
  vector<int> safe;
  safe.push_back(-VarManager::instance().getPresErrorStateVar());
  expander.addExpNxtClauseToC(safe, solver_ctrl_ind_);
  for(size_t pcnt = 0; pcnt < max_back; ++pcnt)
  {
    solver_ctrl_ind_->incAddCNF(prev_trans_or_initial_[pcnt]);
    solver_ctrl_ind_->incAddUnitClause(-current_to_previous_map_[pcnt][VM.getPresErrorStateVar()]);

    // prepare the activated previous state clause:
    px_act[pcnt].push_back(current_state_is_initial_[pcnt]);
    solver_ctrl_ind_->incAddClause(px_act[pcnt]);
    for(size_t cnt = 0; cnt < s_.size(); ++cnt)
    {
      int prev = presentToPrevious(s_[cnt], pcnt);
      solver_ctrl_ind_->incAdd2LitClause(-px_unused[pcnt][cnt], -px_act[pcnt][cnt]);
      solver_ctrl_ind_->incAdd4LitClause(px_unused[pcnt][cnt], px_neg[pcnt][cnt], prev, -px_act[pcnt][cnt]);
      solver_ctrl_ind_->incAdd4LitClause(px_unused[pcnt][cnt], px_neg[pcnt][cnt], -prev, px_act[pcnt][cnt]);
      solver_ctrl_ind_->incAdd4LitClause(px_unused[pcnt][cnt], -px_neg[pcnt][cnt], prev, px_act[pcnt][cnt]);
      solver_ctrl_ind_->incAdd4LitClause(px_unused[pcnt][cnt], -px_neg[pcnt][cnt], -prev, -px_act[pcnt][cnt]);
    }
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
          assumptions.reserve(input.size() + tmp.size() + (s_.size() * max_back));
          assumptions.insert(assumptions.end(), input.begin(), input.end());
          assumptions.insert(assumptions.end(), tmp.begin(), tmp.end());
          // build the previous state-copies of tmp using the activation variables:
          for(size_t pcnt = 0; pcnt < max_back; ++pcnt)
          {
            for(size_t s_cnt = 0; s_cnt < s_.size(); ++s_cnt)
            {
              if(Utils::contains(tmp, s_[s_cnt]))
                assumptions.push_back(px_neg[pcnt][s_cnt]);
              else if(Utils::contains(tmp, -s_[s_cnt]))
                assumptions.push_back(-px_neg[pcnt][s_cnt]);
              else
                assumptions.push_back(px_unused[pcnt][s_cnt]);
            }
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
      winning_region_.addClauseAndSimplify(blocking_clause);
      solver_i_->incAddClause(blocking_clause);
      for(size_t pcnt = 0; pcnt < max_back; ++pcnt)
      {
        vector<int> prev_blocking_clause(blocking_clause);
        presentToPrevious(prev_blocking_clause, pcnt);
        solver_ctrl_ind_->incAddClause(prev_blocking_clause);
      }
      clauses_added++;

      // reseting the solver_ctrl_ind_ seems to help.
      // TODO: find out good numbers for the heuristics when to restart
      if(clauses_added > winning_region_.getNrOfClauses() + 100)
      {
        if(reset_c_cnt % 1000 == 999)
          Utils::compressStateCNF(winning_region_, true);
        else if(reset_c_cnt % 100 == 99)
          Utils::compressStateCNF(winning_region_, false);
        expander.resetSolverCExp(solver_ctrl_ind_);
        solver_ctrl_ind_->incAddCNF(winning_region_);
        const list<vector<int> > &cl = winning_region_.getClauses();
        for(CNF::ClauseConstIter it = cl.begin(); it != cl.end(); ++it)
          expander.addExpNxtClauseToC(*it, solver_ctrl_ind_);
        for(size_t pcnt = 0; pcnt < max_back; ++pcnt)
        {
          solver_ctrl_ind_->incAddCNF(prev_trans_or_initial_[pcnt]);
          CNF prev_win(winning_region_);
          presentToPrevious(prev_win, pcnt);
          solver_ctrl_ind_->incAddCNF(prev_win);

          // prepare the activated previous state clause:
          solver_ctrl_ind_->incAddClause(px_act[pcnt]);
          for(size_t cnt = 0; cnt < s_.size(); ++cnt)
          {
            int prev = presentToPrevious(s_[cnt], pcnt);
            solver_ctrl_ind_->incAdd2LitClause(-px_unused[pcnt][cnt], -px_act[pcnt][cnt]);
            solver_ctrl_ind_->incAdd4LitClause(px_unused[pcnt][cnt], px_neg[pcnt][cnt], prev, -px_act[pcnt][cnt]);
            solver_ctrl_ind_->incAdd4LitClause(px_unused[pcnt][cnt], px_neg[pcnt][cnt], -prev, px_act[pcnt][cnt]);
            solver_ctrl_ind_->incAdd4LitClause(px_unused[pcnt][cnt], -px_neg[pcnt][cnt], prev, px_act[pcnt][cnt]);
            solver_ctrl_ind_->incAdd4LitClause(px_unused[pcnt][cnt], -px_neg[pcnt][cnt], -prev, -px_act[pcnt][cnt]);
          }
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
int LearnSynthSAT::presentToPrevious(int literal, int back) const
{
  int var = (literal < 0) ? -literal : literal;
  if(literal < 0)
    return -current_to_previous_map_[back][var];
  else
    return current_to_previous_map_[back][var];
}

// -------------------------------------------------------------------------------------------
void LearnSynthSAT::presentToPrevious(vector<int> &cube_or_clause, int back) const
{
  for(size_t cnt = 0; cnt < cube_or_clause.size(); ++cnt)
    cube_or_clause[cnt] = presentToPrevious(cube_or_clause[cnt], back);
}

// -------------------------------------------------------------------------------------------
void LearnSynthSAT::presentToPrevious(CNF &cnf, int back) const
{
  list<vector<int> > orig_clauses = cnf.getClauses();
  cnf.clear();
  for(CNF::ClauseConstIter it = orig_clauses.begin(); it != orig_clauses.end(); ++it)
  {
    vector<int> clause(*it);
    presentToPrevious(clause, back);
    cnf.addClause(clause);
  }
}


