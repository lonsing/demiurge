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

#include "IC3Synth.h"
#include "VarManager.h"
#include "Options.h"
#include "AIG2CNF.h"
#include "Logger.h"
#include "LingelingApi.h"

// -------------------------------------------------------------------------------------------
IC3Synth::IC3Synth() :
          BackEnd(),
          qbf_solver_(Options::instance().getQBFSolver()),
          sat_solver_(new LingelingApi())
{
  f_.reserve(10000);
  f_.push_back(AIG2CNF::instance().getInitial());
  check_quant_.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::E));
  check_quant_.push_back(make_pair(VarInfo::INPUT, QBFSolver::E));
  check_quant_.push_back(make_pair(VarInfo::CTRL, QBFSolver::A));
  check_quant_.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::E));
  check_quant_.push_back(make_pair(VarInfo::TMP, QBFSolver::E));

  gen_quant_.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::E));
  gen_quant_.push_back(make_pair(VarInfo::INPUT, QBFSolver::A));
  gen_quant_.push_back(make_pair(VarInfo::CTRL, QBFSolver::E));
  gen_quant_.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::E));
  gen_quant_.push_back(make_pair(VarInfo::TMP, QBFSolver::E));
}

// -------------------------------------------------------------------------------------------
IC3Synth::~IC3Synth()
{
  delete qbf_solver_;
  qbf_solver_ = NULL;
  delete sat_solver_;
  sat_solver_ = NULL;
}

// -------------------------------------------------------------------------------------------
bool IC3Synth::run()
{
  //L_DBG(VarManager::instance().toString());
  bool realizable = computeWinningRegion();
  L_DBG("realizable: " << realizable);
  L_DBG("winning region:\n" << winning_region_.toString());
  if(realizable)
  {
    // The initial state must be part of the winning region
    CNF check(winning_region_);
    check.addCNF(AIG2CNF::instance().getInitial());
    MASSERT(sat_solver_->isSat(check), "Initial state is not winning.");

    // All states of the winning region must be safe
    check.clear();
    check.addCNF(winning_region_);
    check.addCNF(AIG2CNF::instance().getUnsafeStates());
    MASSERT(!sat_solver_->isSat(check), "Winning region is not safe.");

    // from the winning region, we can enforce to stay in the winning region:
    // exists x,i: forall c: exists x': W(x) & T(x,i,c,x') & !W(x) must be unsat
    check.clear();
    check.addCNF(winning_region_);
    check.swapPresentToNext();
    check.negate();
    check.addCNF(winning_region_);
    check.addCNF(AIG2CNF::instance().getTrans());
    MASSERT(!qbf_solver_->isSat(check_quant_, check), "Winning region is not inductive.");
  }

  return realizable;
}

// -------------------------------------------------------------------------------------------
bool IC3Synth::computeWinningRegion()
{
  // the initial state is always safe in our setting, so we do not need to search for
  // counterexamples of length 0.

  // check for counterexamples of length 1:
  CNF c1(AIG2CNF::instance().getTrans());
  c1.addCNF(AIG2CNF::instance().getNextUnsafeStates());
  c1.addCNF(AIG2CNF::instance().getInitial());
  if(qbf_solver_->isSat(check_quant_, c1))
    return false;

  size_t k = 1;
  while(true)
  {
    L_DBG("Starting iteration k=" << k);
    if(!strengthen(k))
    {
      L_DBG("Strengthening failed -> unrealizable");
      return false;
    }
    L_DBG("Done strengthening");
    propagateClauses(k);
    for(size_t i = 0; i <= k; ++i)
    {
      CNF& f1 = getF(i);
      CNF& f2 = getF(i+1);
      if(f1 == f2)
      {
        L_DBG("Found two equal clause sets: F" << i << " and F" << (i+1));
        winning_region_ = f1;
        return true;
      }
    }
    for(size_t i = 0; i <= k; ++i)
    {
      CNF leave(getF(i));
      leave.swapPresentToNext();
      leave.negate();
      leave.addCNF(getF(i));
      leave.addCNF(AIG2CNF::instance().getTrans());
      if(!qbf_solver_->isSat(check_quant_, leave))
      {
        L_DBG("Found inductive set: F" << i );
        winning_region_ = getF(i);
        return true;
      }
    }
    ++k;
  }
}

// -------------------------------------------------------------------------------------------
void IC3Synth::propagateClauses(size_t max_level)
{
  for(size_t i = 0; i <= max_level; ++i)
  {
    CNF& f1cnf = getF(i);
    const list<vector<int> > &f1clauses = getF(i).getClauses();
    CNF& f2cnf = getF(i+1);
    list<vector<int> > f2clauses = f2cnf.getClauses();
    list<vector<int> >::const_iterator it1 = f1clauses.begin();
    list<vector<int> >::iterator it2 = f2clauses.begin();
    for(; it1 != f1clauses.end(); )
    {
      if(*it1 == *it2)
      {
        ++it1;
        ++it2;
        continue;
      }
      CNF check;
      for(size_t lit_cnt = 0; lit_cnt < it1->size(); ++lit_cnt)
        check.add1LitClause(-(*it1)[lit_cnt]);
      check.swapPresentToNext();
      check.addCNF(AIG2CNF::instance().getTrans());
      check.addCNF(f1cnf);

      if(!sat_solver_->isSat(check))
      {
        //L_DBG("propagate worked");
        it2 = f2clauses.insert(it2,*it1);
        ++it1;
        ++it2;
      }
      else
      {
        ++it1;
      }
    }
    f2cnf.swapWith(f2clauses);
  }
}

// -------------------------------------------------------------------------------------------
bool IC3Synth::strengthen(size_t level)
{
  vector<int> counterexample;
  CNF check(getF(level+1));
  check.swapPresentToNext();
  check.negate();
  check.addCNF(AIG2CNF::instance().getTrans());
  check.addCNF(getF(level));
  bool found = qbf_solver_->isSatModel(check_quant_, check, counterexample);
  if(!found)
    return true;
  if(level == 0)
    return false;
  do
  {
    restrictToStates(counterexample);
    if(!excludeInd(counterexample, level))
      excludeForce(counterexample, level);
    check.clear();
    counterexample.clear();
    check.addCNF(getF(level+1));
    check.swapPresentToNext();
    check.negate();
    check.addCNF(AIG2CNF::instance().getTrans());
    check.addCNF(getF(level));
    found = qbf_solver_->isSatModel(check_quant_, check, counterexample);
  } while(found);
  return strengthen(level - 1);
}

// -------------------------------------------------------------------------------------------
bool IC3Synth::excludeInd(const vector<int> &counterexample_cube, size_t level)
{
  if(level == 1)
  {
    CNF check;
    for(size_t i = 0; i < counterexample_cube.size(); ++i)
      check.add1LitClause(counterexample_cube[i]);
    check.swapPresentToNext();
    check.addCNF(AIG2CNF::instance().getTrans());
    check.addCNF(getF(level));
    vector<int> not_counterexample_clause(counterexample_cube);
    for(size_t i = 0; i < not_counterexample_clause.size(); ++i)
      not_counterexample_clause[i] = -not_counterexample_clause[i];
    check.addClause(not_counterexample_clause);
    if(!sat_solver_->isSat(check))
    {
      // !counterexample_cube is inductive relative to Fi
      vector<int> blocking_clause = blockReach(counterexample_cube, level);
      for(size_t i = 0; i <= level+1; ++i)
        getF(i).addClause(blocking_clause);
      //L_DBG("ExcludeInd succeeded.");
      return true;
    }
    //L_DBG("ExcludeInd failed");
    return false;
  }
  CNF check;
  for(size_t i = 0; i < counterexample_cube.size(); ++i)
    check.add1LitClause(counterexample_cube[i]);
  check.swapPresentToNext();
  check.addCNF(AIG2CNF::instance().getTrans());
  check.addCNF(getF(level - 1));
  if(!sat_solver_->isSat(check))
  {
    // !counterexample_cube is inductive relative to F[i-1]
    check.clear();
    for(size_t i = 0; i < counterexample_cube.size(); ++i)
      check.add1LitClause(counterexample_cube[i]);
    check.swapPresentToNext();
    check.addCNF(AIG2CNF::instance().getTrans());
    check.addCNF(getF(level));
    if(!sat_solver_->isSat(check))
    {
      // !counterexample_cube is also inductive relative to F[i]
      vector<int> blocking_clause = blockReach(counterexample_cube, level);
      for(size_t i = 0; i <= level+1; ++i)
        getF(i).addClause(blocking_clause);
      //L_DBG("ExcludeInd succeeded");
      return true;
    }
    else
    {
      // !counterexample_cube is also inductive relative to F[i-1] but not F[i]
      vector<int> blocking_clause = blockReach(counterexample_cube, level - 1);
      for(size_t i = 0; i <= level; ++i)
        getF(i).addClause(blocking_clause);
      //L_DBG("ExcludeInd succeeded");
      return true;
    }
  }
  //L_DBG("ExcludeInd failed");
  return false;

}

// -------------------------------------------------------------------------------------------
void IC3Synth::excludeForce(const vector<int> &counterexample_cube, size_t level)
{
  vector<int> blocking_clause = blockForce(counterexample_cube, level);
  for(size_t i = 0; i <= level; ++i)
    getF(i).addClause(blocking_clause);
}

// -------------------------------------------------------------------------------------------
vector<int> IC3Synth::blockReach(const vector<int> &counterexample_cube, size_t level)
{
  set<int> gen_ce;
  for(size_t cnt = 0; cnt < counterexample_cube.size(); ++cnt)
    gen_ce.insert(counterexample_cube[cnt]);
  for(size_t cnt = 0; cnt < counterexample_cube.size(); ++cnt)
  {
    set<int> tmp = gen_ce;
    tmp.erase(counterexample_cube[cnt]);
    if(containsInitialState(tmp))
        continue;
    CNF tmp_cnf;
    for(set<int>::const_iterator it = tmp.begin(); it != tmp.end(); ++it)
      tmp_cnf.add1LitClause(*it);
    tmp_cnf.swapPresentToNext();
    tmp_cnf.addCNF(getF(level));
    tmp_cnf.addCNF(AIG2CNF::instance().getTrans());
    vector<int> not_tmp;
    not_tmp.reserve(tmp.size());
    for(set<int>::const_iterator it = tmp.begin(); it != tmp.end(); ++it)
      not_tmp.push_back(-*it);
    tmp_cnf.addClause(not_tmp);
    if(!sat_solver_->isSat(tmp_cnf))
      gen_ce = tmp;
  }

  vector<int> blocking_clause;
  blocking_clause.reserve(gen_ce.size());
  for(set<int>::const_iterator it = gen_ce.begin(); it != gen_ce.end(); ++it)
    blocking_clause.push_back(-(*it));
  return blocking_clause;
}

// -------------------------------------------------------------------------------------------
vector<int> IC3Synth::blockForce(const vector<int> &counterexample_cube, size_t level)
{
  set<int> gen_ce;
  for(size_t cnt = 0; cnt < counterexample_cube.size(); ++cnt)
    gen_ce.insert(counterexample_cube[cnt]);
  for(size_t cnt = 0; cnt < counterexample_cube.size(); ++cnt)
  {
    set<int> tmp = gen_ce;
    tmp.erase(counterexample_cube[cnt]);
    CNF tmp_cnf(getF(level+1));
    tmp_cnf.swapPresentToNext();
    tmp_cnf.addCNF(getF(level));
    tmp_cnf.addCNF(AIG2CNF::instance().getTrans());
    for(set<int>::const_iterator it = tmp.begin(); it != tmp.end(); ++it)
      tmp_cnf.add1LitClause(*it);
    if(!qbf_solver_->isSat(gen_quant_, tmp_cnf))
      gen_ce = tmp;
  }

  vector<int> blocking_clause;
  blocking_clause.reserve(gen_ce.size());
  for(set<int>::const_iterator it = gen_ce.begin(); it != gen_ce.end(); ++it)
    blocking_clause.push_back(-(*it));
  return blocking_clause;
}

// -------------------------------------------------------------------------------------------
CNF& IC3Synth::getF(size_t index)
{
  for(size_t i = f_.size(); i <= index; ++i)
  {
    f_.push_back(CNF());
    f_.back().add1LitClause(-VarManager::instance().getPresErrorStateVar());
  }
  return f_[index];
}

// -------------------------------------------------------------------------------------------
void IC3Synth::restrictToStates(vector<int> &vec) const
{
  for(size_t cnt = 0; cnt < vec.size(); ++cnt)
  {
    int var = (vec[cnt] < 0) ? -vec[cnt] : vec[cnt];
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
bool IC3Synth::containsInitialState(const set<int> &cube) const
{
  for(set<int>::const_iterator it = cube.begin(); it != cube.end(); ++it)
  {
    if((*it) > 0)
      return false;
  }
  return true;
}
