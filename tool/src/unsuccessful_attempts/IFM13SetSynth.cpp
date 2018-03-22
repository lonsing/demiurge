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

#include "IFM13SetSynth.h"
#include "VarManager.h"
#include "Options.h"
#include "AIG2CNF.h"
#include "Logger.h"
#include "SatSolver.h"
#include "DepQBFApi.h"
#include "Utils.h"
#include "QBFCertImplExtractor.h"

#define IS_LOSE false
#define IS_GREATER true

// -------------------------------------------------------------------------------------------
IFM13SetSynth::IFM13SetSynth() :
            BackEnd()
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
  r_.push_back(AIG2CNF::instance().getUnsafeStates());
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

}

// -------------------------------------------------------------------------------------------
IFM13SetSynth::~IFM13SetSynth()
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
bool IFM13SetSynth::run()
{
  //L_DBG(VarManager::instance().toString());
  L_INF("Starting to compute a winning region ...");
  bool realizable = computeWinningRegion();

  if(!realizable)
  {
    L_RES("The specification is unrealizable.");
    return false;
  }
  L_RES("The specification is realizable.");
  Utils::debugCheckWinReg(winning_region_, neg_winning_region_);
  if(Options::instance().doRealizabilityOnly())
    return true;

  L_INF("Starting to extract a circuit ...");
  QBFCertImplExtractor extractor;
  extractor.extractCircuit(winning_region_, neg_winning_region_);
  L_INF("Synthesis done.");
  return true;
}

// -------------------------------------------------------------------------------------------
bool IFM13SetSynth::computeWinningRegion()
{
  const vector<int> &state_vars = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE);
  vector<int> initial_state_cube(state_vars.size(), 0);
  for(size_t cnt = 0; cnt < state_vars.size(); ++cnt)
    initial_state_cube[cnt] = -state_vars[cnt];

  size_t k = 1;
  while(true)
  {
    L_DBG("------ Iteration k=" << k);
    if(recBlockCube(initial_state_cube, k) == IS_LOSE)
      return false;

    debugCheckInvariants(k);
    size_t equal = propagateBlockedStates(k);
    //debugCheckInvariants(k);
    if(equal != 0)
    {
      L_LOG("Found two equal clause sets: R" << (equal-1) << " and R" << equal);
      L_LOG("Nr of iterations: " << k);
      //L_DBG("Clauses: " << getR(equal).toString());
      winning_region_ = getR(equal).toCNF();
      winning_region_.negate();
      neg_winning_region_ = getR(equal).toCNF();
      return true;
    }
    ++k;
  }
}

// -------------------------------------------------------------------------------------------
size_t IFM13SetSynth::propagateBlockedStates(size_t max_level)
{
  for(size_t i = 1; i <= max_level; ++i)
  {
    bool equal = true;
    const set<set<int> > &r1clauses = getR(i).getClauses();
    CNFSet& r2cnf = getR(i+1);
    set<set<int> > r2clauses = r2cnf.getClauses();
    set<set<int> >::const_iterator it1 = r1clauses.begin();
    set<set<int> >::iterator it2 = r2clauses.begin();

    for(; it1 != r1clauses.end(); )
    {
      if(it2 != r2clauses.end() && *it1 == *it2)
      {
        ++it1;
        ++it2;
        continue;
      }
      vector<int> neg_clause(it1->begin(), it1->end());
      for(size_t lit_cnt = 0; lit_cnt < neg_clause.size(); ++lit_cnt)
        neg_clause[lit_cnt] = -neg_clause[lit_cnt];
      if(!getGotoNextLowerSolver(i+1)->incIsSat(neg_clause))
      {
        // There is no edge from s (the negated clause) to Ri
        // --> no state in s can be part of Ri+1
        pair<set<set<int> >::iterator, bool> inserted = r2clauses.insert(*it1);
        MASSERT(inserted.second, "Element was already there.");
        it2 = inserted.first;
        vector<int> propagated(it1->begin(), it1->end());
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
bool IFM13SetSynth::recBlockCube(const vector<int> &state_cube, size_t level)
{
  list<IFMProofObligation> queue;
  queue.push_back(IFMProofObligation(state_cube, level));

  vector<int> model_or_core;
  while(!queue.empty())
  {
    IFMProofObligation proof_obligation = popMin(queue);
    const vector<int> &s = proof_obligation.getState();
    size_t s_level = proof_obligation.getLevel();

    if(isLose(s))
      continue;
    if(isBlocked(s, s_level))
    {
      // BEGIN optimization A
      // Performance optimization not mentioned in the IFM13 paper, but exploited in
      // Morgenstern's implementation:
      // We block the transition of the predecessor to s
      if(proof_obligation.hasPre())
      {
        const vector<int> &si = proof_obligation.getPreStateInCube();
        const vector<int> &c = proof_obligation.getPreCtrlCube();
        genAndBlockTrans(si, c, s_level + 1);
      }
      // END optimization A
      continue;
    }
    SatSolver *goto_next_lower = getGotoNextLowerSolver(s_level);
    bool isSat = goto_next_lower->incIsSatModelOrCore(s, sin_, model_or_core);
    if(isSat)
    {
      //L_DBG("Found transition from R" << s_level << " to R" << (s_level - 1));
      vector<int> succ = Utils::extractNextAsPresent(model_or_core);
      if(s_level == 1 || isLose(succ))
      {
        //L_DBG(" Successor is losing");
        vector<int> in_cube = Utils::extract(model_or_core, VarInfo::INPUT);
        vector<int> state_cube = Utils::extract(model_or_core, VarInfo::PRES_STATE);
        isSat = goto_win_solver_->incIsSatModelOrCore(state_cube, in_cube, sicn_, model_or_core);
        if(isSat)
        {
          //L_DBG(" We can also go to a winning state");
          succ = Utils::extractNextAsPresent(model_or_core);
          vector<int> si = Utils::extractPresIn(model_or_core);
          vector<int> c = Utils::extract(model_or_core, VarInfo::CTRL);
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
      // Morgenstern's implementation:
      // We block the transition of the predecessor to s
      if(proof_obligation.hasPre())
      {
        const vector<int> &si = proof_obligation.getPreStateInCube();
        const vector<int> &c = proof_obligation.getPreCtrlCube();
        genAndBlockTrans(si, c, s_level + 1);
      }
      // END optimization A

      // BEGIN optimization B
      // Performance optimization not mentioned in the IFM13 paper, but exploited in
      // Morgenstern's implementation:
      // We aggressively decide s also on later levels:
      //if(s_level < r_.size() - 1)
      //{
      //  queue.push_back(ProofObligation(s, s_level+1, proof_obligation.getPreStateInCube(),
      //                                  proof_obligation.getPreCtrlCube()));
      //}
      // END optimization B
    }
  }
  return IS_GREATER;
}

// -------------------------------------------------------------------------------------------
void IFM13SetSynth::addBlockedTransition(const vector<int> &state_in_cube, size_t level)
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
void IFM13SetSynth::addBlockedState(const vector<int> &cube, size_t level)
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
  blocking_clause.reserve(cube.size() + 1);
  for(size_t cnt = 0; cnt < cube.size(); ++cnt)
    blocking_clause.push_back(-cube[cnt]);
  blocking_clause.push_back(VarManager::instance().getPresErrorStateVar());
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
  // Morgenstern's implementation:
  // We try to push the blocking_clause forward as far as possible:
  vector<int> neg_clause(cube);
  neg_clause.push_back(-VarManager::instance().getPresErrorStateVar());
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
bool IFM13SetSynth::isBlocked(const vector<int> &state_cube, size_t level)
{
  return !getR(level).isSatBy(state_cube);
}

// -------------------------------------------------------------------------------------------
void IFM13SetSynth::addLose(const vector<int> &cube)
{
  vector<int> blocking_clause(cube);
  for(size_t cnt = 0; cnt < blocking_clause.size(); ++cnt)
    blocking_clause[cnt] = -blocking_clause[cnt];
  win_.addClauseAndSimplify(blocking_clause);
  Utils::swapPresentToNext(blocking_clause);
  goto_win_solver_->incAddClause(blocking_clause);
}

// -------------------------------------------------------------------------------------------
bool IFM13SetSynth::isLose(const vector<int> &state_cube)
{
  return !win_.isSatBy(state_cube);
}

// -------------------------------------------------------------------------------------------
void IFM13SetSynth::genAndBlockTrans(const vector<int> &state_in_cube,
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
CNFSet& IFM13SetSynth::getR(size_t index)
{
  for(size_t i = r_.size(); i <= index; ++i)
    r_.push_back(CNFSet());
  return r_[index];
}

// -------------------------------------------------------------------------------------------
CNF& IFM13SetSynth::getU(size_t index)
{
  for(size_t i = u_.size(); i <= index; ++i)
    u_.push_back(CNF());
  return u_[index];
}

// -------------------------------------------------------------------------------------------
SatSolver* IFM13SetSynth::getGotoNextLowerSolver(size_t index)
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
SatSolver* IFM13SetSynth::getGenBlockTransSolver(size_t index)
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
IFMProofObligation IFM13SetSynth::popMin(list<IFMProofObligation> &queue)
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
void IFM13SetSynth::debugPrintRs() const
{
  for(size_t cnt = 0; cnt < r_.size(); ++cnt)
  {
    L_DBG("R" << cnt << ":");
    L_DBG(r_[cnt].toString());
  }
}

// -------------------------------------------------------------------------------------------
void IFM13SetSynth::debugCheckInvariants(size_t k)
{
#ifndef NDEBUG
  SatSolver *sat_solver = Options::instance().getSATSolver();

  // check if W implies P:
  CNF check(AIG2CNF::instance().getUnsafeStates());
  check.addCNF(win_);
  bool v = sat_solver->isSat(check);
  MASSERT(!v, "W does not imply P");

  // check if R0 is still !P:
  check.clear();
  check.addCNF(getR(0).toCNF());
  check.negate();
  check.addCNF(AIG2CNF::instance().getUnsafeStates());
  v = sat_solver->isSat(check);
  MASSERT(!v, "R0 is unequal to !P");
  check.clear();
  check.addCNF(AIG2CNF::instance().getUnsafeStates());
  check.negate();
  check.addCNF(getR(0).toCNF());
  v = sat_solver->isSat(check);
  MASSERT(!v, "R0 is unequal to !P");

  // check if R_i implies Ri+1:
  for(size_t i = 0; i < k; ++i)
  {
    CNF check(getR(i+1).toCNF());
    check.negate();
    check.addCNF(getR(i).toCNF());
    bool v = sat_solver->isSat(check);
    MASSERT(!v, "R" << i << " does not imply R" << i+1);
  }

  // check if all R_i imply not s_0:
  for(size_t i = 0; i <= k; ++i)
  {
    CNF check(getR(i).toCNF());
    check.addCNF(AIG2CNF::instance().getInitial());
    bool v = sat_solver->isSat(check);
    MASSERT(!v, "R" << i << " does not imply !s0" << i+1);
  }

  // check if U_i implies Ui+1:
  for(size_t i = 1; i < k; ++i)
  {
    CNF check(getU(i+1));
    check.negate();
    check.addCNF(getU(i));
    bool v = sat_solver->isSat(check);
    MASSERT(!v, "U" << i << " does not imply U" << i+1);
  }


  // check if Force^e_1(R_i) => Ri+1
  // i.e., if all states from which then environment can enforce R_i are contained in Ri+1:
  // \exists x,i: \forall c: \exists x',tmp: !Ri+1 & T & ri'
  DepQBFApi qbf_solver;
  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > env_quant;
  env_quant.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::E));
  env_quant.push_back(make_pair(VarInfo::INPUT, QBFSolver::E));
  env_quant.push_back(make_pair(VarInfo::CTRL, QBFSolver::A));
  env_quant.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::E));
  env_quant.push_back(make_pair(VarInfo::TMP, QBFSolver::E));

  for(size_t i = 0; i < k; ++i)
  {
    CNF check(getR(i).toCNF());
    check.swapPresentToNext();
    check.addCNF(AIG2CNF::instance().getTrans());
    CNF neg_r_next(getR(i+1).toCNF());
    neg_r_next.negate();
    check.addCNF(neg_r_next);
    bool v = qbf_solver.isSat(env_quant, check);
    MASSERT(!v, "R" << i+1  << " misses a state from which the ENV can enforce R" << i);
  }

  // U_i+1 must not exclude a state-input pair for which the environment can enforce
  // to go to R_i
  for(size_t i = 0; i < k; ++i)
  {
    CNF check(getR(i).toCNF());
    check.swapPresentToNext();
    check.addCNF(AIG2CNF::instance().getTrans());
    //check.addCNF(getR(i+1).toCNF());
    CNF neg_u_next(getU(i+1));
    neg_u_next.negate();
    check.addCNF(neg_u_next);
    bool v = qbf_solver.isSat(env_quant, check);
    MASSERT(!v, "U" << i+1  << " excludes an input for which the ENV can enforce R" << i);
  }

  delete sat_solver;
#endif
}

// -------------------------------------------------------------------------------------------
void IFM13SetSynth::debugCheckWinningRegion() const
{
#ifndef NDEBUG
  SatSolver *sat_solver = Options::instance().getSATSolver();

  // The initial state must be part of the winning region
  CNF check(neg_winning_region_);
  check.addCNF(AIG2CNF::instance().getInitial());
  MASSERT(!sat_solver->isSat(check), "Initial state is not winning.");

  // All states of the winning region must be safe
  check.clear();
  check.addCNF(winning_region_);
  check.addCNF(AIG2CNF::instance().getUnsafeStates());
  MASSERT(!sat_solver->isSat(check), "Winning region is not safe.");

  // from the winning region, we can enforce to stay in the winning region:
  // exists x,i: forall c: exists x': W(x) & T(x,i,c,x') & !W(x) must be unsat
  DepQBFApi qbf_solver;
  check.clear();
  check.addCNF(neg_winning_region_);
  check.swapPresentToNext();
  check.addCNF(winning_region_);
  check.addCNF(AIG2CNF::instance().getTrans());
  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > check_quant;
  check_quant.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::E));
  check_quant.push_back(make_pair(VarInfo::INPUT, QBFSolver::E));
  check_quant.push_back(make_pair(VarInfo::CTRL, QBFSolver::A));
  check_quant.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::E));
  check_quant.push_back(make_pair(VarInfo::TMP, QBFSolver::E));
  MASSERT(!qbf_solver.isSat(check_quant, check), "Winning region is not inductive.");
  delete sat_solver;
#endif
}

