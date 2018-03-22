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
#include "CNFImplExtractor.h"
#include "Utils.h"


// -------------------------------------------------------------------------------------------
LearnSynthQBFInc::LearnSynthQBFInc(CNFImplExtractor *impl_extractor) :
               BackEnd(),
               impl_extractor_(impl_extractor)
{
  // build the quantifier prefix for checking for counterexamples:
  //   check_quant_ = exists x,i: forall c: exists x',tmp:
  // (we misuse the type TEMPL_PARAMS for activation variables we may use)
  check_quant_.push_back(make_pair(VarInfo::TEMPL_PARAMS, QBFSolver::E));
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
  delete impl_extractor_;
  impl_extractor_ = 0;
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
  impl_extractor_->extractCircuit(winning_region_);
  L_INF("Synthesis done.");
  statistics_.logStatistics();
  impl_extractor_->logStatistics();
  return true;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInc::computeWinningRegion()
{
  if(Options::instance().getBackEndMode() == 7)
    return computeWinningRegionOne();
  if(Options::instance().getBackEndMode() == 8)
    return computeWinningRegionOnePush();
  if(Options::instance().getBackEndMode() == 9)
    return computeWinningRegionOnePool();
  if(Options::instance().getBackEndMode() == 10)
    return computeWinningRegionAll();
  if(Options::instance().getBackEndMode() == 11)
    return computeWinningRegionAllPush();
  return computeWinningRegionAllPool(); // mode = 12
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
  solver_check_.doMinCores(false);
  // build generalize_clause_cnf = P(x) AND T(x,i,c,x') AND P(x')
  recomputeGenCNF();
  solver_gen_.startIncrementalSession(gen_quant_);
  solver_gen_.incAddCNF(generalize_clause_cnf_);
  solver_gen_.doMinCores(false);

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
    solver_gen_.incAddClause(blocking_clause);
    Utils::swapPresentToNext(blocking_clause);
    solver_gen_.incAddClause(blocking_clause);

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
  return true;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInc::computeWinningRegionOnePush()
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
  solver_check_.startIncrementalSession(check_quant_);
  solver_check_.incAddCNF(AIG2CNF::instance().getTrans());
  solver_check_.incAddUnitClause(-VM.getPresErrorStateVar());
  vector<int> next_unsafe;
  next_unsafe.reserve(10000);
  next_unsafe.push_back(VM.getNextErrorStateVar());
  // build generalize_clause_cnf = P(x) AND T(x,i,c,x') AND P(x')
  solver_gen_.startIncrementalSession(gen_quant_);
  solver_gen_.incAddCNF(AIG2CNF::instance().getTrans());
  solver_gen_.incAddUnitClause(-VM.getPresErrorStateVar());
  solver_gen_.incAddUnitClause(-VM.getNextErrorStateVar());
  solver_gen_.doMinCores(false);

  vector<int> counterexample;
  vector<int> blocking_clause;
  blocking_clause.reserve(VM.getVarsOfType(VarInfo::PRES_STATE).size());

  size_t count = 0;
  while(true)
  {
    count++;
    solver_check_.incPush();
    solver_check_.incAddClause(next_unsafe);
    bool ce_found = computeCounterexampleQBF(counterexample);
    if(!ce_found)
      return true;
    solver_check_.incPop();

    L_DBG("Found bad state (nr " << count << "), excluding it from the winning region.");
    bool realizable = computeBlockingClause(counterexample, blocking_clause);
    if(!realizable)
      return false;

    // update the CNFs:
    Utils::debugPrint(blocking_clause, "Blocking clause: ");
    winning_region_.addClause(blocking_clause);
    solver_check_.incAddClause(blocking_clause);
    solver_gen_.incAddClause(blocking_clause);
    Utils::swapPresentToNext(blocking_clause);
    solver_gen_.incAddClause(blocking_clause);
    int clause_is_false = VarManager::instance().createFreshTmpVar();
    solver_check_.incAddVarAtInnermostQuant(clause_is_false);
    next_unsafe.push_back(clause_is_false);
    for(size_t cnt = 0; cnt < blocking_clause.size(); ++cnt)
      solver_check_.incAdd2LitClause(-clause_is_false, -blocking_clause[cnt]);

    if(count % 100 == 0)
    {
      L_DBG("Statistics at count=" << count);
      L_DBG("Winning region clauses: " << winning_region_.getNrOfClauses());
      L_DBG("Winning region lits: " << winning_region_.getNrOfLits());
      if(Logger::instance().isEnabled(Logger::DBG))
        statistics_.logStatistics();
    }
  }
  return true;
}


// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInc::computeWinningRegionOnePool()
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
  int pool_size = 20;
  negcl_var_pool_.clear();
  negcl_var_pool_.reserve(pool_size);
  negcl_var_pool_act_.clear();
  negcl_var_pool_act_.reserve(pool_size);
  for(int cnt = 0; cnt < pool_size; ++cnt)
  {
    negcl_var_pool_.push_back(VM.createFreshTmpVar());
    negcl_var_pool_act_.push_back(VM.createFreshTemplParam());
  }
  solver_check_.startIncrementalSession(check_quant_);
  solver_check_.incAddCNF(AIG2CNF::instance().getTrans());
  solver_check_.incAddUnitClause(-VM.getPresErrorStateVar());
  for(size_t cnt = 0; cnt < negcl_var_pool_act_.size(); ++cnt)
    solver_check_.incAdd2LitClause(-negcl_var_pool_act_[cnt], -negcl_var_pool_[cnt]);
  vector<int> next_unsafe;
  next_unsafe.reserve(negcl_var_pool_.size() + 1);
  next_unsafe.push_back(VM.getNextErrorStateVar());
  next_unsafe.insert(next_unsafe.end(), negcl_var_pool_.begin(), negcl_var_pool_.end());
  solver_check_.incAddClause(next_unsafe);


  // build generalize_clause_cnf = P(x) AND T(x,i,c,x') AND P(x')
  solver_gen_.startIncrementalSession(gen_quant_);
  solver_gen_.incAddCNF(AIG2CNF::instance().getTrans());
  solver_gen_.incAddUnitClause(-VM.getPresErrorStateVar());
  solver_gen_.incAddUnitClause(-VM.getNextErrorStateVar());
  solver_gen_.doMinCores(false);

  vector<int> counterexample;
  vector<int> blocking_clause;
  blocking_clause.reserve(VM.getVarsOfType(VarInfo::PRES_STATE).size());

  size_t count = 0;
  while(true)
  {
    count++;

    bool ce_found = computeCounterexampleQBFPool(counterexample);

    if(!ce_found)
      return true;

    L_DBG("Found bad state (nr " << count << "), excluding it from the winning region.");
    bool realizable = computeBlockingClause(counterexample, blocking_clause);
    if(!realizable)
      return false;

    // update the CNFs:
    Utils::debugPrint(blocking_clause, "Blocking clause: ");
    winning_region_.addClauseAndSimplify(blocking_clause);
    winning_region_large_.addClauseAndSimplify(blocking_clause);
    vector<int> next_blocking_clause(blocking_clause);
    Utils::swapPresentToNext(next_blocking_clause);
    solver_gen_.incAddClause(blocking_clause);
    solver_gen_.incAddClause(next_blocking_clause);
    if(negcl_var_pool_.empty())
    {
      VM.resetToLastPush();
      Utils::compressStateCNF(winning_region_);
      negcl_var_pool_.clear();
      negcl_var_pool_.reserve(pool_size);
      negcl_var_pool_act_.clear();
      negcl_var_pool_act_.reserve(pool_size);
      for(int cnt = 0; cnt < pool_size; ++cnt)
      {
        negcl_var_pool_.push_back(VM.createFreshTmpVar());
        negcl_var_pool_act_.push_back(VM.createFreshTemplParam());
      }
      vector<int> next_unsafe;
      next_unsafe.reserve(negcl_var_pool_.size() + winning_region_.getNrOfClauses());
      next_unsafe.insert(next_unsafe.end(), negcl_var_pool_.begin(), negcl_var_pool_.end());

      CNF check_cnf(AIG2CNF::instance().getTrans());
      check_cnf.addCNF(winning_region_);
      CNF next_win_reg(winning_region_);
      next_win_reg.swapPresentToNext();
      const list<vector<int> >& cl = next_win_reg.getClauses();
      for(CNF::ClauseConstIter it = cl.begin(); it != cl.end(); ++it)
      {
        if(it->size() == 1)
          next_unsafe.push_back(-(*it)[0]);
        else
        {
          int clause_false_lit = VarManager::instance().createFreshTmpVar();
          next_unsafe.push_back(clause_false_lit);
          for(size_t lit_cnt = 0; lit_cnt < it->size(); ++lit_cnt)
            check_cnf.add2LitClause(-clause_false_lit, -((*it)[lit_cnt]));
        }
      }
      for(size_t cnt = 0; cnt < negcl_var_pool_act_.size(); ++cnt)
        check_cnf.add2LitClause(-negcl_var_pool_act_[cnt], -negcl_var_pool_[cnt]);
      check_cnf.addClause(next_unsafe);
      solver_check_.startIncrementalSession(check_quant_);
      solver_check_.incAddCNF(check_cnf);
    }
    else
    {
      solver_check_.incAddClause(blocking_clause);
      int not_cl = negcl_var_pool_.back();
      negcl_var_pool_.pop_back();
      negcl_var_pool_act_.pop_back();
      for(size_t lit_cnt = 0; lit_cnt < next_blocking_clause.size(); ++lit_cnt)
        solver_check_.incAdd2LitClause(-not_cl, -next_blocking_clause[lit_cnt]);
    }

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
  solver_gen_.doMinCores(true);

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
      solver_gen_.incAddClause(blocking_clause);
      Utils::swapPresentToNext(blocking_clause);
      solver_gen_.incAddClause(blocking_clause);
      precise = false;
    }

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
bool LearnSynthQBFInc::computeWinningRegionAllPush()
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
  solver_check_.startIncrementalSession(check_quant_);
  solver_check_.incAddCNF(AIG2CNF::instance().getTrans());
  solver_check_.incAddUnitClause(-VM.getPresErrorStateVar());
  vector<int> next_unsafe;
  next_unsafe.reserve(10000);
  next_unsafe.push_back(VM.getNextErrorStateVar());
  // build generalize_clause_cnf = P(x) AND T(x,i,c,x') AND P(x')
  solver_gen_.startIncrementalSession(gen_quant_);
  solver_gen_.incAddCNF(AIG2CNF::instance().getTrans());
  solver_gen_.incAddUnitClause(-VM.getPresErrorStateVar());
  solver_gen_.incAddUnitClause(-VM.getNextErrorStateVar());
  solver_gen_.doMinCores(true);

  vector<int> counterexample;
  vector<vector<int> > all_blocking_clauses;
  all_blocking_clauses.reserve(1000);


  size_t count = 0;
  while(true)
  {
    count++;
    solver_check_.incPush();
    solver_check_.incAddClause(next_unsafe);
    bool ce_found = computeCounterexampleQBF(counterexample);
    if(!ce_found)
      return true;
    solver_check_.incPop();

    L_DBG("Found bad state (nr " << count << "), excluding it from the winning region.");
    bool realizable = computeAllBlockingClauses(counterexample, all_blocking_clauses);
    if(!realizable)
      return false;

    for(size_t clause_count = 0; clause_count < all_blocking_clauses.size(); ++clause_count)
    {
      vector<int> &blocking_clause = all_blocking_clauses[clause_count];
      // update the CNFs:
      Utils::debugPrint(blocking_clause, "Blocking clause: ");
      winning_region_.addClause(blocking_clause);
      solver_check_.incAddClause(blocking_clause);
      solver_gen_.incAddClause(blocking_clause);
      Utils::swapPresentToNext(blocking_clause);
      solver_gen_.incAddClause(blocking_clause);
      int clause_is_false = VarManager::instance().createFreshTmpVar();
      solver_check_.incAddVarAtInnermostQuant(clause_is_false);
      next_unsafe.push_back(clause_is_false);
      for(size_t cnt = 0; cnt < blocking_clause.size(); ++cnt)
        solver_check_.incAdd2LitClause(-clause_is_false, -blocking_clause[cnt]);
    }

    if(count % 100 == 0)
    {
      L_DBG("Statistics at count=" << count);
      L_DBG("Winning region clauses: " << winning_region_.getNrOfClauses());
      L_DBG("Winning region lits: " << winning_region_.getNrOfLits());
      if(Logger::instance().isEnabled(Logger::DBG))
        statistics_.logStatistics();
    }
  }
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInc::computeWinningRegionAllPool()
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
  int pool_size = 50;
  negcl_var_pool_.clear();
  negcl_var_pool_.reserve(pool_size);
  negcl_var_pool_act_.clear();
  negcl_var_pool_act_.reserve(pool_size);
  for(int cnt = 0; cnt < pool_size; ++cnt)
  {
    negcl_var_pool_.push_back(VM.createFreshTmpVar());
    negcl_var_pool_act_.push_back(VM.createFreshTemplParam());
  }
  solver_check_.startIncrementalSession(check_quant_);
  solver_check_.incAddCNF(AIG2CNF::instance().getTrans());
  solver_check_.incAddUnitClause(-VM.getPresErrorStateVar());
  for(size_t cnt = 0; cnt < negcl_var_pool_act_.size(); ++cnt)
    solver_check_.incAdd2LitClause(-negcl_var_pool_act_[cnt], -negcl_var_pool_[cnt]);
  vector<int> next_unsafe;
  next_unsafe.reserve(negcl_var_pool_.size() + 1);
  next_unsafe.push_back(VM.getNextErrorStateVar());
  next_unsafe.insert(next_unsafe.end(), negcl_var_pool_.begin(), negcl_var_pool_.end());
  solver_check_.incAddClause(next_unsafe);

  // build generalize_clause_cnf = P(x) AND T(x,i,c,x') AND P(x')
  solver_gen_.startIncrementalSession(gen_quant_);
  solver_gen_.incAddCNF(AIG2CNF::instance().getTrans());
  solver_gen_.incAddUnitClause(-VM.getPresErrorStateVar());
  solver_gen_.incAddUnitClause(-VM.getNextErrorStateVar());
  solver_gen_.doMinCores(true);

  vector<int> counterexample;
  vector<vector<int> > all_blocking_clauses;
  all_blocking_clauses.reserve(1000);

  size_t count = 0;
  while(true)
  {
    count++;
    bool ce_found = computeCounterexampleQBFPool(counterexample);
    if(!ce_found)
      return true;

    L_DBG("Found bad state (nr " << count << "), excluding it from the winning region.");
    bool realizable = computeAllBlockingClauses(counterexample, all_blocking_clauses);
    if(!realizable)
      return false;

    // update the CNFs and the solver_gen_:
    for(size_t clause_count = 0; clause_count < all_blocking_clauses.size(); ++clause_count)
    {
      vector<int> &blocking_clause = all_blocking_clauses[clause_count];
      Utils::debugPrint(blocking_clause, "Blocking clause: ");
      winning_region_.addClauseAndSimplify(blocking_clause);
      winning_region_large_.addClauseAndSimplify(blocking_clause);
      vector<int> next_blocking_clause(blocking_clause);
      Utils::swapPresentToNext(next_blocking_clause);
      solver_gen_.incAddClause(blocking_clause);
      solver_gen_.incAddClause(next_blocking_clause);
    }

    // update the solver_check_:
    if(negcl_var_pool_.size() < all_blocking_clauses.size())
    {
      VM.resetToLastPush();
      reduceExistingClauses();
      Utils::compressStateCNF(winning_region_);
      negcl_var_pool_.clear();
      negcl_var_pool_.reserve(pool_size);
      negcl_var_pool_act_.clear();
      negcl_var_pool_act_.reserve(pool_size);
      for(int cnt = 0; cnt < pool_size; ++cnt)
      {
        negcl_var_pool_.push_back(VM.createFreshTmpVar());
        negcl_var_pool_act_.push_back(VM.createFreshTemplParam());
      }
      vector<int> next_unsafe;
      next_unsafe.reserve(negcl_var_pool_.size() + winning_region_.getNrOfClauses());
      next_unsafe.insert(next_unsafe.end(), negcl_var_pool_.begin(), negcl_var_pool_.end());

      CNF check_cnf(AIG2CNF::instance().getTrans());
      check_cnf.addCNF(winning_region_);
      CNF next_win_reg(winning_region_);
      next_win_reg.swapPresentToNext();
      const list<vector<int> >& cl = next_win_reg.getClauses();
      for(CNF::ClauseConstIter it = cl.begin(); it != cl.end(); ++it)
      {
        if(it->size() == 1)
          next_unsafe.push_back(-(*it)[0]);
        else
        {
          int clause_false_lit = VarManager::instance().createFreshTmpVar();
          next_unsafe.push_back(clause_false_lit);
          for(size_t lit_cnt = 0; lit_cnt < it->size(); ++lit_cnt)
            check_cnf.add2LitClause(-clause_false_lit, -((*it)[lit_cnt]));
        }
      }
      for(size_t cnt = 0; cnt < negcl_var_pool_act_.size(); ++cnt)
        check_cnf.add2LitClause(-negcl_var_pool_act_[cnt], -negcl_var_pool_[cnt]);
      check_cnf.addClause(next_unsafe);
      solver_check_.startIncrementalSession(check_quant_);
      solver_check_.incAddCNF(check_cnf);
    }
    else
    {
      for(size_t clause_count = 0; clause_count < all_blocking_clauses.size(); ++clause_count)
      {
        vector<int> &blocking_clause = all_blocking_clauses[clause_count];
        vector<int> next_blocking_clause(blocking_clause);
        Utils::swapPresentToNext(next_blocking_clause);
        solver_check_.incAddClause(blocking_clause);
        int not_cl = negcl_var_pool_.back();
        negcl_var_pool_.pop_back();
        negcl_var_pool_act_.pop_back();
        for(size_t lit_cnt = 0; lit_cnt < next_blocking_clause.size(); ++lit_cnt)
          solver_check_.incAdd2LitClause(-not_cl, -next_blocking_clause[lit_cnt]);
      }
    }

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
  return true;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInc::computeCounterexampleQBF(vector<int> &ce)
{
  ce.clear();
  vector<int> none;
  statistics_.notifyBeforeComputeCube();
  bool found = solver_check_.incIsSatModel(none, ce);
  restrictToStates(ce);
  statistics_.notifyAfterComputeCube();
  return found;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInc::computeCounterexampleQBFPool(vector<int> &ce)
{
  statistics_.notifyBeforeComputeCube();
  ce.clear();
  bool found = solver_check_.incIsSatModel(negcl_var_pool_act_, ce);
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
  bool worked = generalizeCounterexample(ce);
  MASSERT(worked, "Impossible.");
  if(Utils::containsInit(ce))
    return false;
  // BEGIN minimize further
  // It pays of to add the blocking clause to generalize_clause_cnf_ and do a second
  // iteration:
  blocking_clause = ce;
  Utils::negateLiterals(blocking_clause);
  solver_gen_.incAddClause(blocking_clause);
  Utils::swapPresentToNext(blocking_clause);
  solver_gen_.incAddClause(blocking_clause);
  // TODO: maybe no core but explicit loop instead of the next line:
  generalizeCounterexampleFurther(ce);
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
  bool worked = generalizeCounterexample(first_ce);
  MASSERT(worked, "Impossible");
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
  // In order to reduce the size of the clauses further, we can add all we have
  // learned meanwhile to solver_gen_ and see if we can reduce the clauses
  // further:
  for(size_t cl_cnt = 0; cl_cnt < blocking_clauses.size(); ++cl_cnt)
  {
    vector<int> new_blocking_clause(blocking_clauses[cl_cnt]);
    solver_gen_.incAddClause(new_blocking_clause);
    Utils::swapPresentToNext(new_blocking_clause);
    solver_gen_.incAddClause(new_blocking_clause);
  }
  for(size_t cl_cnt = 0; cl_cnt < blocking_clauses.size(); ++cl_cnt)
  {
    vector<int> min_ce(blocking_clauses[cl_cnt]);
    Utils::negateLiterals(min_ce);
    size_t size_before_min = min_ce.size();
    // TODO: maybe no core but explicit loop instead of the next line:
    generalizeCounterexampleFurther(min_ce);
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
    generalizeCounterexampleFurther(counterexample);
    if(counterexample.size() < size_before)
    {
      vector<int> clause(counterexample);
      Utils::negateLiterals(clause);
      winning_region_.addClauseAndSimplify(clause);
      winning_region_large_.addClauseAndSimplify(clause);
      solver_gen_.incAddClause(clause);
      Utils::swapPresentToNext(clause);
      solver_gen_.incAddClause(clause);
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
  bool is_sat = solver_gen_.incIsSatCore(ce, gen_ce);
  if(is_sat)
    return false;
  ce = gen_ce;
  Utils::sort(ce);
  return true;
}

// -------------------------------------------------------------------------------------------
void LearnSynthQBFInc::generalizeCounterexampleFurther(vector<int> &ce)
{
  Utils::sort(ce);
  vector<int> orig_ce(ce);
  for(size_t lit_cnt = 0; lit_cnt < orig_ce.size(); ++lit_cnt)
  {
    vector<int> tmp(ce);
    Utils::remove(tmp, orig_ce[lit_cnt]);
    if(!solver_gen_.incIsSat(tmp))
      ce = tmp;
  }
  Utils::sort(ce);
}
