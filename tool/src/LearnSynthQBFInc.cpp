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
/// @file LearnSynthQBFInc.cpp
/// @brief Contains the definition of the class LearnSynthQBFInc.
// -------------------------------------------------------------------------------------------

#include "LearnSynthQBFInc.h"
#include "VarManager.h"
#include "Options.h"
#include "AIG2CNF.h"
#include "Logger.h"
#include "QBFCertImplExtractor.h"
#include "Utils.h"


// -------------------------------------------------------------------------------------------
LearnSynthQBFInc::LearnSynthQBFInc() :
               BackEnd()
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


}

// -------------------------------------------------------------------------------------------
LearnSynthQBFInc::~LearnSynthQBFInc()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInc::run()
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
bool LearnSynthQBFInc::computeWinningRegion()
{
  if(Options::instance().getBackEndMode() == 7)
    return computeWinningRegionOne();
  return computeWinningRegionAll(); // mode = 8
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInc::computeWinningRegionOne()
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

  bool precise = true;
  // build check_cnf = P(x) AND T(x,i,c,x') AND NOT P(x')
  recomputeCheckCNF();
  solver_check_.startIncrementalSession(check_quant_);
  solver_check_.incAddCNF(check_cnf_);
  // build generalize_clause_cnf = P(x) AND T(x,i,c,x') AND P(x')
  recomputeGenCNF();
  solver_gen_.startIncrementalSession(gen_quant_);
  solver_gen_.incAddCNF(generalize_clause_cnf_);

  vector<int> counterexample;
  vector<int> blocking_clause;
  blocking_clause.reserve(VM.getVarsOfType(VarInfo::PRES_STATE).size());

  size_t count = 0;
  while(true)
  {
    count++;
    bool ce_found = computeCounterexampleQBF(counterexample);
    if(!ce_found)
    {
      if(precise)
        return true;
      Utils::compressStateCNF(winning_region_);
      recomputeCheckCNF();
      solver_check_.startIncrementalSession(check_quant_);
      solver_check_.incAddCNF(check_cnf_);
      recomputeGenCNF();
      solver_gen_.startIncrementalSession(gen_quant_);
      solver_gen_.incAddCNF(generalize_clause_cnf_);
      precise = true;
      continue;
    }

    L_DBG("Found bad state (nr " << count << "), excluding it from the winning region.");
    bool realizable = computeBlockingClause(counterexample, blocking_clause);
    if(!realizable)
      return false;

    // update the CNFs:
    Utils::debugPrint(blocking_clause, "Blocking clause: ");
    winning_region_.addClauseAndSimplify(blocking_clause);
    winning_region_large_.addClauseAndSimplify(blocking_clause);
    solver_check_.incAddClause(blocking_clause);
    //solver_gen_.incAddClause(blocking_clause);
    Utils::compressStateCNF(winning_region_);
    recomputeGenCNF();
    solver_gen_.startIncrementalSession(gen_quant_);
    solver_gen_.incAddCNF(generalize_clause_cnf_);
    precise = false;

    if(count % 100 == 0)
    {
      L_DBG("Statistics at count=" << count);
      L_DBG("Winning region clauses: " << winning_region_.getNrOfClauses());
      L_DBG("Winning region lits: " << winning_region_.getNrOfLits());
      L_DBG("Winning region full clauses: " << winning_region_large_.getNrOfClauses());
      L_DBG("Winning region full lits: " << winning_region_large_.getNrOfLits());
      if(Logger::instance().isEnabled(Logger::DBG))
        statistics_.logStatistics();
    }
  }
  //reduceExistingClauses();
  return true;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInc::computeWinningRegionAll()
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

  bool precise = true;
  // build check_cnf = P(x) AND T(x,i,c,x') AND NOT P(x')
  recomputeCheckCNF();
  solver_check_.startIncrementalSession(check_quant_);
  solver_check_.incAddCNF(check_cnf_);
  // build generalize_clause_cnf = P(x) AND T(x,i,c,x') AND P(x')
  recomputeGenCNF();
  solver_gen_.startIncrementalSession(gen_quant_);
  solver_gen_.incAddCNF(generalize_clause_cnf_);

  vector<int> counterexample;
  vector<vector<int> > all_blocking_clauses;
  all_blocking_clauses.reserve(1000);


  size_t count = 0;
  while(true)
  {
    count++;
    bool ce_found = computeCounterexampleQBF(counterexample);
    if(!ce_found)
    {
      if(precise)
        return true;
      reduceExistingClauses();
      Utils::compressStateCNF(winning_region_);
      recomputeCheckCNF();
      solver_check_.startIncrementalSession(check_quant_);
      solver_check_.incAddCNF(check_cnf_);
      recomputeGenCNF();
      solver_gen_.startIncrementalSession(gen_quant_);
      solver_gen_.incAddCNF(generalize_clause_cnf_);
      precise = true;
      continue;
    }

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
      solver_check_.incAddClause(blocking_clause);
      //solver_gen_.incAddClause(blocking_clause);
      precise = false;
    }

    Utils::compressStateCNF(winning_region_);
    recomputeGenCNF();
    solver_gen_.startIncrementalSession(gen_quant_);
    solver_gen_.incAddCNF(generalize_clause_cnf_);

    if(count % 100 == 0)
    {
      L_DBG("Statistics at count=" << count);
      L_DBG("Winning region clauses: " << winning_region_.getNrOfClauses());
      L_DBG("Winning region lits: " << winning_region_.getNrOfLits());
      L_DBG("Winning region full clauses: " << winning_region_large_.getNrOfClauses());
      L_DBG("Winning region full lits: " << winning_region_large_.getNrOfLits());
      if(Logger::instance().isEnabled(Logger::DBG))
        statistics_.logStatistics();
    }
  }
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInc::computeCounterexampleQBF(vector<int> &ce)
{
  ce.clear();
  vector<int> none;
  statistics_.notifyBeforeComputeCube();
  bool found = solver_check_.incIsSatModelOrCore(none, ce);
  restrictToStates(ce);
  statistics_.notifyAfterComputeCube();
  return found;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInc::computeBlockingClause(vector<int> &ce, vector<int> &blocking_clause)
{
  statistics_.notifyBeforeCubeMin();
  Utils::randomize(ce);
  if(Utils::containsInit(ce))
    return false;
  size_t orig_size = ce.size();
  generalizeCounterexample(ce);
  if(Utils::containsInit(ce))
    return false;
  // BEGIN minimize further
  // It pays of to add the blocking clause to generalize_clause_cnf_ and do a second
  // iteration:
  blocking_clause = ce;
  Utils::negateLiterals(blocking_clause);
  generalize_clause_cnf_.addClause(blocking_clause);
  Utils::swapPresentToNext(blocking_clause);
  generalize_clause_cnf_.addClause(blocking_clause);
  solver_gen_.startIncrementalSession(gen_quant_);
  solver_gen_.incAddCNF(generalize_clause_cnf_);
  generalizeCounterexample(ce);
  if(Utils::containsInit(ce))
    return false;
  // END minimize further
  blocking_clause = ce;
  Utils::negateLiterals(blocking_clause);
  L_DBG("computeBlockingClause(): " << orig_size << " --> " << blocking_clause.size());
  statistics_.notifyAfterCubeMin(orig_size, blocking_clause.size());
  return true;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInc::computeAllBlockingClauses(vector<int> &ce,
                                                 vector<vector<int> > &blocking_clauses)
{
  statistics_.notifyBeforeCubeMin();
  list<set<int> > must_not_contain_queue;
  list<vector<int> > gen_ces;
  vector<int> orig_ce(ce);
  vector<int> first_ce(orig_ce);
  generalizeCounterexample(first_ce);
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
    for(; it != must_not_contain.end(); ++it)
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
    }
  }

  // BEGIN minimize further
  // This is all we could do with the current incremental session of solver_gen_.
  // However, in order to reduce the size of the clauses further, we can add all we have
  // learned meanwhile to solver_gen_ and see if we can reduce the clauses
  // further:
  for(size_t cl_cnt = 0; cl_cnt < blocking_clauses.size(); ++cl_cnt)
  {
    vector<int> new_blocking_clause(blocking_clauses[cl_cnt]);
    generalize_clause_cnf_.addClause(new_blocking_clause);
    Utils::swapPresentToNext(new_blocking_clause);
    generalize_clause_cnf_.addClause(new_blocking_clause);
  }
  solver_gen_.startIncrementalSession(gen_quant_);
  solver_gen_.incAddCNF(generalize_clause_cnf_);
  for(size_t cl_cnt = 0; cl_cnt < blocking_clauses.size(); ++cl_cnt)
  {
    vector<int> min_ce(blocking_clauses[cl_cnt]);
    Utils::negateLiterals(min_ce);
    size_t size_before_min = min_ce.size();
    generalizeCounterexample(min_ce);
    if(min_ce.size() < size_before_min)
    {
      Utils::negateLiterals(min_ce);
      blocking_clauses[cl_cnt] = min_ce;
    }
  }
  // END minimize further


  statistics_.notifyAfterCubeMin();

  return true;
}

// -------------------------------------------------------------------------------------------
void LearnSynthQBFInc::reduceExistingClauses()
{
  statistics_.notifyBeforeCubeMin();

  size_t or_cl = winning_region_.getNrOfClauses();
  size_t or_lits = winning_region_.getNrOfLits();
  const list<vector<int> > orig_clauses =  winning_region_.getClauses();
  for(CNF::ClauseConstIter it = orig_clauses.begin(); it != orig_clauses.end(); ++it)
  {
    vector<int> counterexample(*it);
    Utils::negateLiterals(counterexample);
    size_t size_before = counterexample.size();
    generalizeCounterexample(counterexample);
    if(counterexample.size() < size_before)
    {
      vector<int> clause(counterexample);
      Utils::negateLiterals(clause);
      winning_region_.addClauseAndSimplify(clause);
      winning_region_large_.addClauseAndSimplify(clause);
      generalize_clause_cnf_.addClause(clause);
      Utils::swapPresentToNext(clause);
      generalize_clause_cnf_.addClause(clause);
      solver_gen_.startIncrementalSession(gen_quant_);
      solver_gen_.incAddCNF(generalize_clause_cnf_);
    }
  }
  size_t new_cl = winning_region_.getNrOfClauses();
  size_t new_lits = winning_region_.getNrOfLits();;
  L_DBG("reduceExistingClauses(): " << or_cl << " --> " << new_cl << " clauses.");
  L_DBG("reduceExistingClauses(): " << or_lits << " --> " << new_lits << " literals.");
  statistics_.notifyAfterCubeMin(0, 0);
}

// -------------------------------------------------------------------------------------------
void LearnSynthQBFInc::recomputeCheckCNF(bool take_small_win)
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
void LearnSynthQBFInc::recomputeGenCNF(bool take_small_win)
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
void LearnSynthQBFInc::restrictToStates(vector<int> &vec) const
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
bool LearnSynthQBFInc::generalizeCounterexample(vector<int> &ce)
{
  vector<int> gen_ce;
  bool is_sat = solver_gen_.incIsSatModelOrCore(ce, gen_ce);
  if(is_sat)
    return false;
  ce = gen_ce;
  Utils::sort(ce);
  return true;
}
