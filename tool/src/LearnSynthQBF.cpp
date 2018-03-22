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
/// @file LearnSynthQBF.cpp
/// @brief Contains the definition of the class LearnSynthQBF.
// -------------------------------------------------------------------------------------------

#include "LearnSynthQBF.h"
#include "VarManager.h"
#include "Options.h"
#include "AIG2CNF.h"
#include "Logger.h"
#include "QBFCertImplExtractor.h"
#include "Utils.h"
#include "SatSolver.h"

// -------------------------------------------------------------------------------------------
LearnSynthQBF::LearnSynthQBF() :
               BackEnd(),
               qbf_solver_(Options::instance().getQBFSolver()),
               solver_i_(Options::instance().getSATSolver()),
               solver_ctrl_(Options::instance().getSATSolver()),
               incremental_vars_to_keep_(VarManager::instance().getAllNonTempVars()),
               solver_i_precise_(true)
{
  // build the quantifier prefix for checking for counterexamples:
  //   check_quant_ = exists x,i: forall c: exists x',tmp:
  check_quant_.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::E));
  check_quant_.push_back(make_pair(VarInfo::INPUT, QBFSolver::E));
  check_quant_.push_back(make_pair(VarInfo::CTRL, QBFSolver::A));
  check_quant_.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::E));
  check_quant_.push_back(make_pair(VarInfo::TMP, QBFSolver::E));

  // build the quantifier prefix for generalizing cubes:
  //    gen_quant_ = exists x: forall i: exists c,x',tmp:
  gen_quant_.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::E));
  gen_quant_.push_back(make_pair(VarInfo::INPUT, QBFSolver::A));
  gen_quant_.push_back(make_pair(VarInfo::CTRL, QBFSolver::E));
  gen_quant_.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::E));
  gen_quant_.push_back(make_pair(VarInfo::TMP, QBFSolver::E));

  // if we use computeWinningRegionOneSAT() then we need two SAT-solvers for computing
  // counterexamples:
  solver_i_->startIncrementalSession(incremental_vars_to_keep_, false);
  solver_i_->incAddCNF(AIG2CNF::instance().getNextUnsafeStates());
  solver_i_->incAddCNF(AIG2CNF::instance().getTrans());
  solver_i_->incAddCNF(AIG2CNF::instance().getSafeStates());
  solver_i_precise_ = true;
  solver_ctrl_->startIncrementalSession(incremental_vars_to_keep_, false);
  solver_ctrl_->incAddCNF(AIG2CNF::instance().getNextSafeStates());
  solver_ctrl_->incAddCNF(AIG2CNF::instance().getTrans());
  solver_ctrl_->incAddCNF(AIG2CNF::instance().getSafeStates());
}

// -------------------------------------------------------------------------------------------
LearnSynthQBF::~LearnSynthQBF()
{
  delete qbf_solver_;
  qbf_solver_ = NULL;
  delete solver_i_;
  solver_i_ = NULL;
  delete solver_ctrl_;
  solver_ctrl_ = NULL;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBF::run()
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
bool LearnSynthQBF::computeWinningRegion()
{
  if(Options::instance().getBackEndMode() == 0)
    return computeWinningRegionOne();
  if(Options::instance().getBackEndMode() == 1)
    return computeWinningRegionOneSAT();
  return computeWinningRegionAll(); // mode = 4
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBF::computeWinningRegionOne()
{
  AIG2CNF& A2C = AIG2CNF::instance();
  VarManager &VM = VarManager::instance();
  L_DBG("Nr. of state variables: " << VM.getVarsOfType(VarInfo::PRES_STATE).size());
  L_DBG("Nr. of uncontrollable inputs: " << VM.getVarsOfType(VarInfo::INPUT).size());
  L_DBG("Nr. of controllable inputs: " << VM.getVarsOfType(VarInfo::CTRL).size());

  winning_region_.clear();
  winning_region_ = A2C.getSafeStates();
  winning_region_large_.clear();
  winning_region_large_ = A2C.getSafeStates();

  // build check_cnf = P(x) AND T(x,i,c,x') AND NOT P(x')
  recomputeCheckCNF();

  // build generalize_clause_cnf = P(x) AND T(x,i,c,x') AND P(x')
  recomputeGenCNF();

  vector<int> counterexample;
  vector<int> blocking_clause;
  blocking_clause.reserve(VM.getVarsOfType(VarInfo::PRES_STATE).size());
  bool ce_found = computeCounterexampleQBF(counterexample);

  size_t count = 0;
  while(ce_found)
  {
    count++;
    L_DBG("Found bad state (nr " << count << "), excluding it from the winning region.");
    bool realizable = computeBlockingClause(counterexample, blocking_clause);
    if(!realizable)
      return false;

    // update the CNFs:
    Utils::debugPrint(blocking_clause, "Blocking clause: ");
    winning_region_.addClauseAndSimplify(blocking_clause);
    winning_region_large_.addClauseAndSimplify(blocking_clause);
    generalize_clause_cnf_.addClauseAndSimplify(blocking_clause);
    Utils::swapPresentToNext(blocking_clause);
    generalize_clause_cnf_.addClauseAndSimplify(blocking_clause);
    //if(count % 100 == 0)
    //{
    //  reduceExistingClauses();
    //  recomputeGenCNF(false);
    //}

    Utils::compressStateCNF(winning_region_);
    recomputeCheckCNF(false);

    if(count % 100 == 0)
    {
      L_DBG("Statistics at count=" << count);
      L_DBG("Winning region clauses: " << winning_region_.getNrOfClauses());
      L_DBG("Winning region lits: " << winning_region_.getNrOfLits());
      L_DBG("Winning region full clauses: " << winning_region_large_.getNrOfClauses());
      L_DBG("Winning region full lits: " << winning_region_large_.getNrOfLits());
      L_DBG("Check CNF clauses: " << check_cnf_.getNrOfClauses());
      L_DBG("Check CNF lits: " << check_cnf_.getNrOfLits());
      L_DBG("Generalize CNF clauses: " << generalize_clause_cnf_.getNrOfClauses());
      L_DBG("Generalize CNF lits: " << generalize_clause_cnf_.getNrOfLits());
      if(Logger::instance().isEnabled(Logger::DBG))
        statistics_.logStatistics();
    }
    ce_found = computeCounterexampleQBF(counterexample);
  }
  return true;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBF::computeWinningRegionOneSAT()
{
  AIG2CNF& A2C = AIG2CNF::instance();
  VarManager &VM = VarManager::instance();
  L_DBG("Nr. of state variables: " << VM.getVarsOfType(VarInfo::PRES_STATE).size());
  L_DBG("Nr. of uncontrollable inputs: " << VM.getVarsOfType(VarInfo::INPUT).size());
  L_DBG("Nr. of controllable inputs: " << VM.getVarsOfType(VarInfo::CTRL).size());

  winning_region_.clear();
  winning_region_ = A2C.getSafeStates();
  winning_region_large_.clear();
  winning_region_large_ = A2C.getSafeStates();

  // build generalize_clause_cnf = P(x) AND T(x,i,c,x') AND P(x')
  recomputeGenCNF();

  vector<int> counterexample;
  vector<int> blocking_clause;
  blocking_clause.reserve(VM.getVarsOfType(VarInfo::PRES_STATE).size());
  bool ce_found = computeCounterexampleSAT(counterexample);

  size_t count = 0;
  while(ce_found)
  {
    count++;
    L_DBG("Found bad state (nr " << count << "), excluding it from the winning region.");
    bool realizable = computeBlockingClause(counterexample, blocking_clause);
    if(!realizable)
      return false;

    // update the CNFs:
    Utils::debugPrint(blocking_clause, "Blocking clause: ");
    vector<int> next_blocking_clause(blocking_clause);
    Utils::swapPresentToNext(next_blocking_clause);
    winning_region_.addClauseAndSimplify(blocking_clause);
    winning_region_large_.addClauseAndSimplify(blocking_clause);
    generalize_clause_cnf_.addClauseAndSimplify(blocking_clause);
    generalize_clause_cnf_.addClauseAndSimplify(next_blocking_clause);
    solver_i_->incAddClause(blocking_clause);
    solver_ctrl_->incAddClause(blocking_clause);
    solver_ctrl_->incAddClause(next_blocking_clause);

    if(count % 100 == 0)
    {
      L_DBG("Statistics at count=" << count);
      L_DBG("Winning region clauses: " << winning_region_.getNrOfClauses());
      L_DBG("Winning region lits: " << winning_region_.getNrOfLits());
      L_DBG("Winning region full clauses: " << winning_region_large_.getNrOfClauses());
      L_DBG("Winning region full lits: " << winning_region_large_.getNrOfLits());
      L_DBG("Check CNF clauses: " << check_cnf_.getNrOfClauses());
      L_DBG("Check CNF lits: " << check_cnf_.getNrOfLits());
      L_DBG("Generalize CNF clauses: " << generalize_clause_cnf_.getNrOfClauses());
      L_DBG("Generalize CNF lits: " << generalize_clause_cnf_.getNrOfLits());
      if(Logger::instance().isEnabled(Logger::DBG))
        statistics_.logStatistics();
    }
    ce_found = computeCounterexampleSAT(counterexample);
  }
  return true;
}


// -------------------------------------------------------------------------------------------
bool LearnSynthQBF::computeWinningRegionAll()
{
  AIG2CNF& A2C = AIG2CNF::instance();
  VarManager &VM = VarManager::instance();
  L_DBG("Nr. of state variables: " << VM.getVarsOfType(VarInfo::PRES_STATE).size());
  L_DBG("Nr. of uncontrollable inputs: " << VM.getVarsOfType(VarInfo::INPUT).size());
  L_DBG("Nr. of controllable inputs: " << VM.getVarsOfType(VarInfo::CTRL).size());

  winning_region_.clear();
  winning_region_ = A2C.getSafeStates();
  winning_region_large_.clear();
  winning_region_large_ = A2C.getSafeStates();

  // build check_cnf = P(x) AND T(x,i,c,x') AND NOT P(x')
  recomputeCheckCNF();

  // build generalize_clause_cnf = P(x) AND T(x,i,c,x') AND P(x')
  recomputeGenCNF();

  vector<int> counterexample;
  vector<vector<int> > all_blocking_clauses;
  all_blocking_clauses.reserve(1000);
  bool ce_found = computeCounterexampleQBF(counterexample);
  size_t count = 0;
  while(ce_found)
  {
    count++;
    L_DBG("Found bad state (nr " << count << "), excluding it from the winning region.");

    bool realizable = computeAllBlockingClauses(counterexample, all_blocking_clauses);
    if(!realizable)
      return false;

    for(size_t clause_count = 0; clause_count < all_blocking_clauses.size(); ++clause_count)
    {
      vector<int> &blocking_clause = all_blocking_clauses[clause_count];
      // update the CNFs:
      Utils::debugPrint(blocking_clause, "Blocking clause: ");
      winning_region_.addClauseAndSimplify(blocking_clause);
      winning_region_large_.addClauseAndSimplify(blocking_clause);
    }
    if(count % 100 == 0)
      reduceExistingClauses();
    Utils::compressStateCNF(winning_region_);
    recomputeCheckCNF(true);
    recomputeGenCNF(true);
    if(count % 100 == 0)
    {
      L_DBG("Statistics at count=" << count);
      L_DBG("Winning region clauses: " << winning_region_.getNrOfClauses());
      L_DBG("Winning region lits: " << winning_region_.getNrOfLits());
      L_DBG("Winning region full clauses: " << winning_region_large_.getNrOfClauses());
      L_DBG("Winning region full lits: " << winning_region_large_.getNrOfLits());
      L_DBG("Check CNF clauses: " << check_cnf_.getNrOfClauses());
      L_DBG("Check CNF lits: " << check_cnf_.getNrOfLits());
      L_DBG("Generalize CNF clauses: " << generalize_clause_cnf_.getNrOfClauses());
      L_DBG("Generalize CNF lits: " << generalize_clause_cnf_.getNrOfLits());
      if(Logger::instance().isEnabled(Logger::DBG))
        statistics_.logStatistics();
    }
    ce_found = computeCounterexampleQBF(counterexample);
  }
  return true;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBF::computeCounterexampleQBF(vector<int> &ce)
{
  ce.clear();
  statistics_.notifyBeforeComputeCube();
  bool found = qbf_solver_->isSatModel(check_quant_, check_cnf_, ce);
  restrictToStates(ce);
  statistics_.notifyAfterComputeCube();
  return found;
}


// -------------------------------------------------------------------------------------------
bool LearnSynthQBF::computeCounterexampleSAT(vector<int> &ce)
{
  statistics_.notifyBeforeComputeCube();
  ce.clear();
  vector<int> none;
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
    // Here, we try to find some transition that leaves the winning region.
    bool sat = solver_i_->incIsSatModelOrCore(vector<int>(), si, model_or_core);
    if(!sat)
    {
      if(solver_i_precise_)
      {
        // None found and solver_i_ is precise: this means that we are done with the
        // computation of the winning region (no more counterexamples exist):
        statistics_.notifyAfterComputeCube();
        return false;
      }
      // We could not find a possibility to leave the winning region, but solver_i_
      // is not precise (it uses an out-dated next-state-copy of the winning region for
      // performance reasons). In this case, we simply update the out-dated next-state copy
      // of the winning region and try again:
      L_DBG("Need to restart the solver_i_ (iteration " << it_cnt << ")");
      Utils::compressStateCNF(winning_region_);
      solver_i_->startIncrementalSession(incremental_vars_to_keep_, false);
      CNF leave_win(winning_region_);
      leave_win.swapPresentToNext();
      leave_win.negate();
      leave_win.addCNF(AIG2CNF::instance().getTrans());
      leave_win.addCNF(winning_region_);
      solver_i_->incAddCNF(leave_win);
      solver_i_precise_ = true;
      continue;
    }

    vector<int> state_input = model_or_core;
    vector<int> state = Utils::extract(state_input, VarInfo::PRES_STATE);
    vector<int> input = Utils::extract(state_input, VarInfo::INPUT);

    // solver_i_ has found a way to leave the winning region in form of a state-input pair
    // for which some control signal values make us leave the winning region. Now we check if
    // there is a response (in form of control values) to this input such that we do not
    // leave the winning region:
    sat = solver_ctrl_->incIsSatModelOrCore(state, input, c, model_or_core);
    if(!sat)
    {
      // No such response exists --> we have found a counterexample state
      // L_DBG("Found bad state in iteration " << it_cnt);
      // We take the unsatisfiable core because it represents a generalization of the
      // counterexample state. We will do the generalization step with a QBF-solver later,
      // but we get the core (almost) for free, and the more we generalize now, the less work
      // we have to do later:
      ce = model_or_core;
      solver_i_precise_ = false;
      statistics_.notifyAfterComputeCube();
      return true;
    }
    else
    {
      // There exists a response. This means: the state input combination state_input is not
      // useful for the antagonist in trying to break out of winning region. What we do now
      // is: we generalize this state_input into a larger set of state-input combinations by
      // dropping literals from the cube. This is done by computing an unsatisfiable core.
      // Finally, we exclude the generalized state_input from solver_i_. This prevents that
      // the same state-input pair will be tried in a future iteration.
      const vector<int> &ctrl_cube = model_or_core;
      sat = solver_i_->incIsSatModelOrCore(state_input, ctrl_cube, none, model_or_core);
      MASSERT(sat == false, "Impossible.");
      for(size_t cnt = 0; cnt < model_or_core.size(); ++cnt)
        model_or_core[cnt] = -model_or_core[cnt];
      solver_i_->incAddClause(model_or_core);
      // L_DBG("U core: " << si.size() << " --> " << model_or_core.size());
    }
  }
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBF::computeBlockingClause(vector<int> &ce, vector<int> &blocking_clause)
{
  statistics_.notifyBeforeCubeMin();
  Utils::randomize(ce);
  if(Utils::containsInit(ce))
    return false;
  size_t orig_size = ce.size();
  generalizeCounterexample(ce, false);
  if(Utils::containsInit(ce))
    return false;
  blocking_clause = ce;
  Utils::negateLiterals(blocking_clause);
  L_DBG("computeBlockingClause(): " << orig_size << " --> " << blocking_clause.size());
  statistics_.notifyAfterCubeMin(orig_size, blocking_clause.size());
  return true;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBF::computeAllBlockingClauses(vector<int> &ce,
                                              vector<vector<int> > &blocking_clauses)
{
  statistics_.notifyBeforeCubeMin();
  list<set<int> > must_not_contain_queue;
  list<vector<int> > gen_ces;
  vector<int> orig_ce(ce);
  vector<int> first_ce(orig_ce);
  generalizeCounterexample(first_ce, false);
  statistics_.notifyCubeMin(orig_ce.size(), first_ce.size());
  if(Utils::containsInit(first_ce))
    return false;
  blocking_clauses.clear();
  vector<int> blocking_clause(first_ce);
  Utils::negateLiterals(blocking_clause);
  blocking_clauses.push_back(blocking_clause);

  gen_ces.push_back(first_ce);
  for(size_t cnt = 0; cnt < first_ce.size(); ++cnt)
  {
    set<int> singleton_set;
    singleton_set.insert(first_ce[cnt]);
    must_not_contain_queue.push_back(singleton_set);
  }

  // we add our blocking_clause to generalize_clause_cnf_ to benefit from it immediately:
  generalize_clause_cnf_.addClause(blocking_clause);
  Utils::swapPresentToNext(blocking_clause);
  generalize_clause_cnf_.addClause(blocking_clause);
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
    vector<int> new_ce(orig_ce);
    set<int>::const_iterator it = must_not_contain.begin();
    for( ; it != must_not_contain.end(); ++it)
      Utils::remove(new_ce, *it);
    size_t orig_size = new_ce.size();
    bool is_ok = generalizeCounterexample(new_ce);
    statistics_.notifyCubeMin(orig_size, new_ce.size());
    if(is_ok)
    {
      if(Utils::containsInit(new_ce))
        return false;
      gen_ces.push_back(new_ce);
      blocking_clause = new_ce;
      Utils::negateLiterals(blocking_clause);
      blocking_clauses.push_back(blocking_clause);
      for(size_t cnt = 0; cnt < new_ce.size(); ++cnt)
      {
        set<int> new_must_not_contain(must_not_contain);
        new_must_not_contain.insert(new_ce[cnt]);
        must_not_contain_queue.push_back(new_must_not_contain);
      }
      // we add our blocking_clause to generalize_clause_cnf_ to benefit from it immediately:
      generalize_clause_cnf_.addClause(blocking_clause);
      Utils::swapPresentToNext(blocking_clause);
      generalize_clause_cnf_.addClause(blocking_clause);
    }
  }

  statistics_.notifyAfterCubeMin();

  return true;
}

// -------------------------------------------------------------------------------------------
void LearnSynthQBF::reduceExistingClauses()
{
  statistics_.notifyBeforeCubeMin();

  size_t or_cl = winning_region_.getNrOfClauses();
  size_t or_lits = winning_region_.getNrOfLits();
  const list<vector<int> > orig_clauses =  winning_region_.getClauses();
  for(CNF::ClauseConstIter it = orig_clauses.begin(); it != orig_clauses.end(); ++it)
  {
    vector<int> clause(*it);
    for(size_t cnt = 0; cnt < it->size(); ++cnt)
    {
      vector<int> tmp(clause);
      Utils::remove(tmp, (*it)[cnt]);
      CNF tmp_cnf(generalize_clause_cnf_);
      tmp_cnf.addNegClauseAsCube(tmp);
      if(!qbf_solver_->isSat(gen_quant_, tmp_cnf))
        clause = tmp;
    }
    if(it->size() > clause.size())
    {
      winning_region_.addClauseAndSimplify(clause);
      winning_region_large_.addClauseAndSimplify(clause);
    }
  }
  size_t new_cl = winning_region_.getNrOfClauses();
  size_t new_lits = winning_region_.getNrOfLits();;
  L_DBG("reduceExistingClauses(): " << or_cl << " --> " << new_cl << " clauses.");
  L_DBG("reduceExistingClauses(): " << or_lits << " --> " << new_lits << " literals.");
  statistics_.notifyAfterCubeMin(0, 0);
}

// -------------------------------------------------------------------------------------------
void LearnSynthQBF::recomputeCheckCNF(bool take_small_win)
{
  VarManager::resetToLastPush();
  check_cnf_.clear();
  if(take_small_win)
  {
    check_cnf_.addCNF(winning_region_);
    check_cnf_.negate();
    check_cnf_.swapPresentToNext();
    check_cnf_.addCNF(AIG2CNF::instance().getTrans());
    check_cnf_.addCNF(winning_region_);
  }
  else
  {
    check_cnf_.addCNF(winning_region_);
    check_cnf_.negate();
    check_cnf_.swapPresentToNext();
    check_cnf_.addCNF(AIG2CNF::instance().getTrans());
    check_cnf_.addCNF(winning_region_large_);
  }
}

// -------------------------------------------------------------------------------------------
void LearnSynthQBF::recomputeGenCNF(bool take_small_win)
{
  generalize_clause_cnf_.clear();
  if(take_small_win)
  {
    generalize_clause_cnf_.addCNF(winning_region_);
    generalize_clause_cnf_.swapPresentToNext();
    generalize_clause_cnf_.addCNF(AIG2CNF::instance().getTrans());
    generalize_clause_cnf_.addCNF(winning_region_);
  }
  else
  {
    generalize_clause_cnf_.addCNF(winning_region_large_);
    generalize_clause_cnf_.swapPresentToNext();
    generalize_clause_cnf_.addCNF(AIG2CNF::instance().getTrans());
    generalize_clause_cnf_.addCNF(winning_region_large_);
  }
}

// -------------------------------------------------------------------------------------------
void LearnSynthQBF::restrictToStates(vector<int> &vec) const
{
  for(size_t cnt = 0; cnt < vec.size(); ++cnt)
  {
    int var = (vec[cnt] < 0) ? -vec[cnt] : vec[cnt];
    // We can always drop the error-bit of the state space (because we can be sure that all
    // states with error=1 have already been removed from the winning region). This saves
    // one iteration during the generalization.
    if(var == VarManager::instance().getPresErrorStateVar() ||
       VarManager::instance().getInfo(var).getKind() != VarInfo::PRES_STATE)
    {
      vec[cnt] = vec[vec.size() - 1];
      vec.pop_back();
      cnt--;
    }
  }
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBF::generalizeCounterexample(vector<int> &ce, bool check_sat) const
{
  if(check_sat)
  {
    CNF tmp_cnf1(generalize_clause_cnf_);
    tmp_cnf1.addCube(ce);
    if(qbf_solver_->isSat(gen_quant_, tmp_cnf1))
      return false;
  }
  vector<int> gen_ce(ce);
  for(size_t cnt = 0; cnt < ce.size(); ++cnt)
  {
    vector<int> tmp = gen_ce;
    Utils::remove(tmp, ce[cnt]);
    CNF tmp_cnf(generalize_clause_cnf_);
    tmp_cnf.addCube(tmp);
    tmp_cnf.addNegCubeAsClause(gen_ce);
    vector<int> next_gen_ce(gen_ce);
    Utils::swapPresentToNext(next_gen_ce);
    tmp_cnf.addNegCubeAsClause(next_gen_ce);
    if(!qbf_solver_->isSat(gen_quant_, tmp_cnf))
      gen_ce = tmp;
  }
  ce = gen_ce;
  Utils::sort(ce);
  return true;
}
