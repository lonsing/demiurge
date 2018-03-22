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
/// @file LearningImplExtractor.cpp
/// @brief Contains the definition of the class LearningImplExtractor.
// -------------------------------------------------------------------------------------------


#include "LearningImplExtractor.h"
#include "CNF.h"
#include "QBFSolver.h"
#include "DepQBFApi.h"
#include "Options.h"
#include "VarManager.h"
#include "AIG2CNF.h"
#include "StringUtils.h"
#include "Logger.h"
#include "SatSolver.h"

extern "C" {
  #include "aiger.h"
}

// -------------------------------------------------------------------------------------------
LearningImplExtractor::LearningImplExtractor() :
  qbf_solver_(Options::instance().getQBFSolver()),
  standalone_circuit_(aiger_init())
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
    next_free_aig_lit_ += 2;
  }

  for(AIG2CNF::DepConstIter it = trans_deps.begin(); it != trans_deps.end(); ++it)
  {
    if(it->first == 1) // the CNF variable 1 is always the constant TRUE, so we map it to 1
      cnf_var_to_standalone_aig_var_[1] = 1;
    else
    {
      cnf_var_to_standalone_aig_var_[it->first] = next_free_aig_lit_;
      next_free_aig_lit_ += 2;
    }
  }
}

// -------------------------------------------------------------------------------------------
LearningImplExtractor::~LearningImplExtractor()
{
  delete qbf_solver_;
  qbf_solver_ = NULL;
  aiger_reset(standalone_circuit_);
  standalone_circuit_ = NULL;
}

// -------------------------------------------------------------------------------------------
void LearningImplExtractor::run(const CNF &winning_region,
                                const CNF &neg_winning_region)
{
  if(Options::instance().getCircuitExtractionMode() == 0)
    runLearningQBF(winning_region, neg_winning_region);
  else if(Options::instance().getCircuitExtractionMode() == 1)
    runLearningQBFInc(winning_region, neg_winning_region);
  else if(Options::instance().getCircuitExtractionMode() == 2)
    runLearningJiangSAT(winning_region, neg_winning_region, false, false);
  else if(Options::instance().getCircuitExtractionMode() == 3)
    runLearningJiangSAT(winning_region, neg_winning_region, false, true);
  else if(Options::instance().getCircuitExtractionMode() == 4)
    runLearningJiangSAT(winning_region, neg_winning_region, true, false);
  else if(Options::instance().getCircuitExtractionMode() == 5)
    runLearningJiangSAT(winning_region, neg_winning_region, true, true);
  else if(Options::instance().getCircuitExtractionMode() == 6)
    runLearningJiangSATInc1(winning_region, neg_winning_region, false, false);
  else if(Options::instance().getCircuitExtractionMode() == 7)
    runLearningJiangSATInc1(winning_region, neg_winning_region, false, true);
  else if(Options::instance().getCircuitExtractionMode() == 8)
    runLearningJiangSATInc1(winning_region, neg_winning_region, true, false);
  else if(Options::instance().getCircuitExtractionMode() == 9)
    runLearningJiangSATInc1(winning_region, neg_winning_region, true, true);
  else if(Options::instance().getCircuitExtractionMode() == 10)
    runLearningJiangSATInc2(winning_region, neg_winning_region, false, false);
  else if(Options::instance().getCircuitExtractionMode() == 11)
    runLearningJiangSATInc2(winning_region, neg_winning_region, false, true);
  else if(Options::instance().getCircuitExtractionMode() == 12)
    runLearningJiangSATInc2(winning_region, neg_winning_region, true, false);
  else if(Options::instance().getCircuitExtractionMode() == 13)
    runLearningJiangSATInc2(winning_region, neg_winning_region, true, true);
  else if(Options::instance().getCircuitExtractionMode() == 14)
    runLearningJiangSATTmp(winning_region, neg_winning_region, false, false);
  else if(Options::instance().getCircuitExtractionMode() == 15)
    runLearningJiangSATTmp(winning_region, neg_winning_region, false, true);
  else if(Options::instance().getCircuitExtractionMode() == 16)
    runLearningJiangSATTmp(winning_region, neg_winning_region, true, false);
  else if(Options::instance().getCircuitExtractionMode() == 17)
    runLearningJiangSATTmp(winning_region, neg_winning_region, true, true);
  else if(Options::instance().getCircuitExtractionMode() == 18)
    runLearningJiangSATTmpCtrl(winning_region, neg_winning_region, false, false);
  else if(Options::instance().getCircuitExtractionMode() == 19)
    runLearningJiangSATTmpCtrl(winning_region, neg_winning_region, false, true);
  else if(Options::instance().getCircuitExtractionMode() == 20)
    runLearningJiangSATTmpCtrl(winning_region, neg_winning_region, true, false);
  else if(Options::instance().getCircuitExtractionMode() == 21)
    runLearningJiangSATTmpCtrl(winning_region, neg_winning_region, true, true);
  else if(Options::instance().getCircuitExtractionMode() == 22)
    runLearningJiangSATTmpCtrlInc2(winning_region, neg_winning_region, false, false);
  else if(Options::instance().getCircuitExtractionMode() == 23)
    runLearningJiangSATTmpCtrlInc2(winning_region, neg_winning_region, false, true);
  else if(Options::instance().getCircuitExtractionMode() == 24)
    runLearningJiangSATTmpCtrlInc2(winning_region, neg_winning_region, true, false);
  else if(Options::instance().getCircuitExtractionMode() == 25)
    runLearningJiangSATTmpCtrlInc2(winning_region, neg_winning_region, true, true);

  statistics.notifyBeforeABC(standalone_circuit_->num_ands);
  aiger *opt = optimizeWithABC(standalone_circuit_);
  aiger_reset(standalone_circuit_);
  standalone_circuit_ = opt;
  statistics.notifyAfterABC(standalone_circuit_->num_ands);
  size_t nr_of_new_ands = insertIntoSpec(standalone_circuit_);
  statistics.notifyFinalSize(nr_of_new_ands);
}

// -------------------------------------------------------------------------------------------
void LearningImplExtractor::runLearningQBF(const CNF &win_region, const CNF &neg_win_region)
{
  const vector<int> &ctrl = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  const vector<int> &next = VarManager::instance().getVarsOfType(VarInfo::NEXT_STATE);
  const vector<int> &tmp = VarManager::instance().getVarsOfType(VarInfo::TMP);

  CNF neg_rel(neg_win_region);
  neg_rel.swapPresentToNext();
  neg_rel.addCNF(win_region);
  neg_rel.addCNF(AIG2CNF::instance().getTrans());
  // neg_rel will have a clause that sets the error state variable to false (it must be
  // part of win_region. However, we also replace it by false so that this variable is gone
  // from the CNF. This makes one state variable less to consider:
  neg_rel.setVarValue(VarManager::instance().getPresErrorStateVar(), false);

  vector<int> ctrl_univ(ctrl);
  vector<int> ctrl_exist;
  ctrl_exist.reserve(ctrl.size());
  for(size_t ctrl_cnt = 0; ctrl_cnt < ctrl.size(); ++ctrl_cnt)
  {
    statistics.notifyBeforeCtrlSignal();
    int current_ctrl = ctrl[ctrl_cnt];
    Utils::remove(ctrl_univ, current_ctrl);
    ctrl_exist.push_back(current_ctrl);

    // Build the quantifier prefix:
    vector<pair<vector<int>, QBFSolver::Quant> > quant;
    quant.push_back(make_pair(ip_, QBFSolver::E));
    quant.push_back(make_pair(ctrl_univ, QBFSolver::A));
    quant.push_back(make_pair(ctrl_exist, QBFSolver::E));
    quant.push_back(make_pair(next, QBFSolver::E));
    quant.push_back(make_pair(tmp, QBFSolver::E));

    // Build the CNF for computing false-positives (ctrl-signal is 1 but must be 0):
    CNF check(neg_rel);
    check.setVarValue(current_ctrl, true);

    // Build the CNF for generalizing false-positives (ctrl-signal can be 0)
    CNF gen(neg_rel);
    gen.setVarValue(current_ctrl, false);

    // do the learning loop:
    CNF solution;
    while(true)
    {
      // compute a false-positives (ctrl-signal is 1 but must not be):
      vector<int> false_pos;
      statistics.notifyBeforeClauseComp();
      bool false_pos_exists = qbf_solver_->isSatModel(quant, check, false_pos);
      statistics.notifyAfterClauseComp();
      if(!false_pos_exists)
        break;

      // generalize the false-positives:
      // (compute a larger set of situations (including the false-positive) for which
      // setting the control signal to 0 is allowed:
      statistics.notifyBeforeClauseMin();
      vector<int> gen_false_pos(false_pos);
      for(size_t lit_cnt = 0; lit_cnt < false_pos.size(); ++lit_cnt)
      {
        vector<int> tmp(gen_false_pos);
        Utils::remove(tmp, false_pos[lit_cnt]);
        CNF current_gen(gen);
        current_gen.addCube(tmp);
        if(!qbf_solver_->isSat(quant,current_gen))
          gen_false_pos = tmp;
      }
      statistics.notifyAfterClauseMin(false_pos.size(), gen_false_pos.size());
      check.addNegCubeAsClause(gen_false_pos);
      solution.addNegCubeAsClause(gen_false_pos);
    }

    // dump solution in AIGER format:
    addToStandAloneAiger(current_ctrl, solution);

    // re-substitution:
    neg_rel.addCNF(makeEq(current_ctrl, solution));
    statistics.notifyAfterCtrlSignal();
  }
}

// -------------------------------------------------------------------------------------------
void LearningImplExtractor::runLearningQBFInc(const CNF &win_region,
                                              const CNF &neg_win_region)
{
  const vector<int> &ctrl = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  const vector<int> &next = VarManager::instance().getVarsOfType(VarInfo::NEXT_STATE);
  const vector<int> &tmp = VarManager::instance().getVarsOfType(VarInfo::TMP);
  vector<int> none;

  CNF neg_rel(neg_win_region);
  neg_rel.swapPresentToNext();
  neg_rel.addCNF(win_region);
  neg_rel.addCNF(AIG2CNF::instance().getTrans());
  // neg_rel will have a clause that sets the error state variable to false (it must be
  // part of win_region. However, we also replace it by false so that this variable is gone
  // from the CNF. This makes one state variable less to consider:
  neg_rel.setVarValue(VarManager::instance().getPresErrorStateVar(), false);

  vector<int> ctrl_univ(ctrl);
  vector<int> ctrl_exist;
  ctrl_exist.reserve(ctrl.size());
  for(size_t ctrl_cnt = 0; ctrl_cnt < ctrl.size(); ++ctrl_cnt)
  {
    statistics.notifyBeforeCtrlSignal();
    int current_ctrl = ctrl[ctrl_cnt];
    Utils::remove(ctrl_univ, current_ctrl);
    ctrl_exist.push_back(current_ctrl);

    // Build the quantifier prefix:
    vector<pair<vector<int>, QBFSolver::Quant> > quant;
    quant.push_back(make_pair(ip_, QBFSolver::E));
    quant.push_back(make_pair(ctrl_univ, QBFSolver::A));
    quant.push_back(make_pair(ctrl_exist, QBFSolver::E));
    quant.push_back(make_pair(next, QBFSolver::E));
    quant.push_back(make_pair(tmp, QBFSolver::E));

    // Build the CNF for computing false-positives (ctrl-signal is 1 but must be 0):
    CNF check(neg_rel);
    check.setVarValue(current_ctrl, true);
    DepQBFApi check_solver;
    check_solver.startIncrementalSession(quant);
    check_solver.incAddCNF(check);

    // Build the CNF for generalizing false-positives (ctrl-signal can be 0)
    CNF gen(neg_rel);
    gen.setVarValue(current_ctrl, false);
    DepQBFApi gen_solver;
    gen_solver.startIncrementalSession(quant);
    gen_solver.incAddCNF(gen);

    // do the learning loop:
    CNF solution;
    while(true)
    {
      // compute a false-positives (ctrl-signal is 1 but must not be):
      vector<int> false_pos;
      statistics.notifyBeforeClauseComp();
      bool false_pos_exists = check_solver.incIsSatModel(none, false_pos);
      statistics.notifyAfterClauseComp();
      if(!false_pos_exists)
        break;

      // generalize the false-positives:
      // (compute a larger set of situations (including the false-positive) for which
      // setting the control signal to 0 is allowed:
      statistics.notifyBeforeClauseMin();
      vector<int> gen_false_pos;
      bool sat = gen_solver.incIsSatCore(false_pos, gen_false_pos);
      MASSERT(!sat, "Impossible");
      statistics.notifyAfterClauseMin(false_pos.size(), gen_false_pos.size());
      check_solver.incAddNegCubeAsClause(gen_false_pos);
      solution.addNegCubeAsClause(gen_false_pos);
    }

    // dump solution in AIGER format:
    addToStandAloneAiger(current_ctrl, solution);

    // re-substitution:
    neg_rel.addCNF(makeEq(current_ctrl, solution));
    statistics.notifyAfterCtrlSignal();
  }
}

// -------------------------------------------------------------------------------------------
void LearningImplExtractor::runLearningJiangSAT(const CNF &win_region,
                                                const CNF &neg_win_region,
                                                bool second_run,
                                                bool min_cores)
{
  const vector<int> &ctrl = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  // setting the minimal core flag to false is faster but leads to larger circuits:
  SatSolver *check = Options::instance().getSATSolverExtr(false, false);
  SatSolver *gen = Options::instance().getSATSolverExtr(false, min_cores);
  CNF trans = AIG2CNF::instance().getTrans();
  vector<int> none;

  CNF next_win(win_region);
  next_win.swapPresentToNext();
  CNF leave_win(neg_win_region);
  leave_win.swapPresentToNext();
  leave_win.addCNF(win_region);

  vector<CNF> c_eq_impl;
  c_eq_impl.reserve(ctrl.size());
  vector<CNF> impl;
  impl.reserve(ctrl.size());

  vector<int> dep_vars(ipc_);
  for(size_t ctrl_cnt = 0; ctrl_cnt < ctrl.size(); ++ctrl_cnt)
  {
    statistics.notifyBeforeCtrlSignal();
    int current_ctrl = ctrl[ctrl_cnt];
    Utils::remove(dep_vars, current_ctrl);

    // we need to rename all variables except for the dep_vars:
    int nr_of_vars = VarManager::instance().getMaxCNFVar() + 1;
    int next_free_var = nr_of_vars;
    vector<int> rename_map(nr_of_vars, 0);
    for(int cnt = 1; cnt < nr_of_vars; ++cnt)
      rename_map[cnt] = next_free_var++;
    for(size_t cnt = 0; cnt < dep_vars.size(); ++cnt)
      rename_map[dep_vars[cnt]] = dep_vars[cnt];

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
    mustBe1.setVarValue(VarManager::instance().getPresErrorStateVar(), false);

    // now we can do the actual learning loop:
    check->startIncrementalSession(dep_vars, false);
    check->incAddCNF(mustBe0);
    gen->startIncrementalSession(dep_vars, false);
    gen->incAddCNF(mustBe1);
    CNF solution;
    while(true)
    {
      vector<int> false_pos;
      // compute a false-positives (ctrl-signal is 1 but must not be):
      statistics.notifyBeforeClauseComp();
      bool false_pos_exists = check->incIsSatModelOrCore(none, dep_vars, false_pos);
      statistics.notifyAfterClauseComp();
      if(!false_pos_exists)
        break;
      // generalize the false-positives:
      // (compute a larger set of situations (including the false-positive) for which
      // setting the control signal to 0 is allowed:
      vector<int> gen_false_pos;
      statistics.notifyBeforeClauseMin();
      //Utils::randomize(false_pos);
      bool sat = gen->incIsSatModelOrCore(false_pos, none, gen_false_pos);
      MASSERT(!sat, "Impossible.");
      statistics.notifyAfterClauseMin(false_pos.size(), gen_false_pos.size());
      solution.addNegCubeAsClause(gen_false_pos);
      check->incAddNegCubeAsClause(gen_false_pos);
    }

    if(!second_run)
    {
      // dump solution in AIGER format:
      addToStandAloneAiger(current_ctrl, solution);
      // re-substitution:
      trans.addCNF(makeEq(current_ctrl, solution));
    }
    else
    {
      // store solution for future optimization:
      impl.push_back(solution);
      // re-substitution:
      CNF resub = makeEq(current_ctrl, solution);
      trans.addCNF(resub);
      c_eq_impl.push_back(resub);
    }
    statistics.notifyAfterCtrlSignal();
  }

  if(second_run)
  {
    // Now the second step: try to reduce the implementations further
    gen->doMinCores(true);
    CNF neg_rel(leave_win);
    neg_rel.addCNF(AIG2CNF::instance().getTrans());
    neg_rel.setVarValue(VarManager::instance().getPresErrorStateVar(), false);

    dep_vars = ipc_;
    for(size_t ctrl_cnt = 0; ctrl_cnt < ctrl.size(); ++ctrl_cnt)
    {
      statistics.notifyBeforeCtrlSignal();
      int current_ctrl = ctrl[ctrl_cnt];
      Utils::remove(dep_vars, current_ctrl);
      CNF neg_rel_fixed(neg_rel);
      for(size_t ctrl_cnt2 = 0; ctrl_cnt2 < ctrl.size(); ++ctrl_cnt2)
        if(ctrl_cnt2 != ctrl_cnt)
          neg_rel_fixed.addCNF(c_eq_impl[ctrl_cnt2]);
      check->startIncrementalSession(dep_vars, false);
      check->incAddCNF(neg_rel_fixed);
      check->incAddUnitClause(current_ctrl);
      gen->startIncrementalSession(dep_vars, false);
      gen->incAddCNF(neg_rel_fixed);
      gen->incAddUnitClause(-current_ctrl);

      CNF solution;
      while(impl[ctrl_cnt].getNrOfClauses() > 0)
      {
        vector<int> neg_clause = impl[ctrl_cnt].removeSmallest();
        Utils::negateLiterals(neg_clause);
        vector<int> smaller_cube;
        statistics.notifyBeforeClauseMin();
        bool sat = gen->incIsSatModelOrCore(neg_clause, none, smaller_cube);
        statistics.notifyAfterClauseMin(neg_clause.size(), smaller_cube.size());
        MASSERT(!sat, "Impossible");
        solution.addNegCubeAsClause(smaller_cube);
        check->incAddNegCubeAsClause(smaller_cube);
        statistics.notifyBeforeClauseComp();
        sat = check->incIsSat();
        statistics.notifyAfterClauseComp();
        if(!sat)
          break;
      }
      impl[ctrl_cnt] = solution;
      addToStandAloneAiger(current_ctrl, solution);
      c_eq_impl[ctrl_cnt] = makeEq(current_ctrl, solution);
      statistics.notifyAfterCtrlSignal();
    }
  }

  delete check;
  check = NULL;
  delete gen;
  gen = NULL;
}


// -------------------------------------------------------------------------------------------
void LearningImplExtractor::runLearningJiangSATTmp(const CNF &win_region,
                                                 const CNF &neg_win_region,
                                                 bool second_run,
                                                 bool min_cores)
{
  const vector<int> &ctrl = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  // setting the minimal core flag to false is faster but leads to larger circuits:
  SatSolver *check = Options::instance().getSATSolverExtr(false, false);
  SatSolver *gen = Options::instance().getSATSolverExtr(false, min_cores);
  CNF trans = AIG2CNF::instance().getTrans();
  vector<int> none;

  CNF next_win(win_region);
  next_win.swapPresentToNext();
  CNF leave_win(neg_win_region);
  leave_win.swapPresentToNext();
  leave_win.addCNF(win_region);

  vector<CNF> c_eq_impl;
  c_eq_impl.reserve(ctrl.size());
  vector<CNF> impl;
  impl.reserve(ctrl.size());

  vector<int> dep_vars(ipc_);
  for(size_t ctrl_cnt = 0; ctrl_cnt < ctrl.size(); ++ctrl_cnt)
  {
    statistics.notifyBeforeCtrlSignal();
    int current_ctrl = ctrl[ctrl_cnt];
    Utils::remove(dep_vars, current_ctrl);
    const map<int, set<VarInfo> > &trans_deps = AIG2CNF::instance().getTmpDepsTrans();
    vector<int> real_dep_vars;
    real_dep_vars.reserve(dep_vars.size() + trans_deps.size());
    real_dep_vars.insert(real_dep_vars.end(), dep_vars.begin(), dep_vars.end());
    for(AIG2CNF::DepConstIter it = trans_deps.begin(); it != trans_deps.end(); ++it)
    {
      bool can_dep_on_this_tmp = true;
      for(set<VarInfo>::const_iterator i2 = it->second.begin(); i2 != it->second.end(); ++i2)
      {
        if(i2->getKind() == VarInfo::CTRL && !Utils::contains(dep_vars, i2->getLitInCNF()))
        {
          can_dep_on_this_tmp = false;
          break;
        }
      }
      if(can_dep_on_this_tmp)
        real_dep_vars.push_back(it->first);
    }

    // we need to rename all variables except for the dep_vars:
    int nr_of_vars = VarManager::instance().getMaxCNFVar() + 1;
    int next_free_var = nr_of_vars;
    vector<int> rename_map(nr_of_vars, 0);
    for(int cnt = 1; cnt < nr_of_vars; ++cnt)
      rename_map[cnt] = next_free_var++;
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
    check->startIncrementalSession(real_dep_vars, false);
    check->incAddCNF(mustBe0);
    gen->startIncrementalSession(real_dep_vars, false);
    gen->incAddCNF(mustBe1);
    CNF solution;
    while(true)
    {
      vector<int> false_pos;
      // compute a false-positives (ctrl-signal is 1 but must not be):
      statistics.notifyBeforeClauseComp();
      bool false_pos_exists = check->incIsSatModelOrCore(none, real_dep_vars, false_pos);

      statistics.notifyAfterClauseComp();
      if(!false_pos_exists)
        break;
      // generalize the false-positives:
      // (compute a larger set of situations (including the false-positive) for which
      // setting the control signal to 0 is allowed:
      vector<int> gen_false_pos;
      statistics.notifyBeforeClauseMin();
      Utils::randomize(false_pos);
      bool sat = gen->incIsSatModelOrCore(false_pos, none, gen_false_pos);
      MASSERT(!sat, "Impossible.");
      statistics.notifyAfterClauseMin(false_pos.size(), gen_false_pos.size());
      solution.addNegCubeAsClause(gen_false_pos);
      check->incAddNegCubeAsClause(gen_false_pos);
    }

    if(!second_run)
    {
      // dump solution in AIGER format:
      addToStandAloneAiger(current_ctrl, solution);
      // re-substitution:
      trans.addCNF(makeEq(current_ctrl, solution));
    }
    else
    {
      // store solution for future optimization:
      impl.push_back(solution);
      // re-substitution:
      CNF resub = makeEq(current_ctrl, solution);
      trans.addCNF(resub);
      c_eq_impl.push_back(resub);
    }
    statistics.notifyAfterCtrlSignal();
  }

  if(second_run)
  {
    // Now the second step: try to reduce the implementations further
    gen->doMinCores(true);
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
      check->startIncrementalSession(dep_vars, false);
      check->incAddCNF(neg_rel_fixed);
      check->incAddUnitClause(current_ctrl);
      gen->startIncrementalSession(dep_vars, false);
      gen->incAddCNF(neg_rel_fixed);
      gen->incAddUnitClause(-current_ctrl);

      CNF solution;
      while(impl[ctrl_cnt].getNrOfClauses() > 0)
      {
        vector<int> neg_clause = impl[ctrl_cnt].removeSmallest();
        Utils::negateLiterals(neg_clause);
        vector<int> smaller_cube;
        statistics.notifyBeforeClauseMin();
        bool sat = gen->incIsSatModelOrCore(neg_clause, none, smaller_cube);
        statistics.notifyAfterClauseMin(neg_clause.size(), smaller_cube.size());
        MASSERT(!sat, "Impossible");
        solution.addNegCubeAsClause(smaller_cube);
        check->incAddNegCubeAsClause(smaller_cube);
        statistics.notifyBeforeClauseComp();
        sat = check->incIsSat();
        statistics.notifyAfterClauseComp();
        if(!sat)
          break;
      }
      impl[ctrl_cnt] = solution;
      addToStandAloneAiger(current_ctrl, solution);
      c_eq_impl[ctrl_cnt] = makeEq(current_ctrl, solution);
      statistics.notifyAfterCtrlSignal();
    }
  }

  // finally, we have to copy AND gates from the transition relation:
  insertMissingAndFromTrans();

  delete check;
  check = NULL;
  delete gen;
  gen = NULL;
}

// -------------------------------------------------------------------------------------------
void LearningImplExtractor::runLearningJiangSATTmpCtrl(const CNF &win_region,
                                                 const CNF &neg_win_region,
                                                 bool second_run,
                                                 bool min_cores)
{
  const vector<int> &ctrl = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  typedef map<int, set<int> >::const_iterator MapConstIter;
  // setting the minimal core flag to false is faster but leads to larger circuits:
  SatSolver *check = Options::instance().getSATSolverExtr(false, false);
  SatSolver *gen = Options::instance().getSATSolverExtr(false, min_cores);
  CNF trans = AIG2CNF::instance().getTrans();
  vector<int> none;

  CNF next_win(win_region);
  next_win.swapPresentToNext();
  CNF leave_win(neg_win_region);
  leave_win.swapPresentToNext();
  leave_win.addCNF(win_region);

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
    int nr_of_vars = VarManager::instance().getMaxCNFVar() + 1;
    int next_free_var = nr_of_vars;
    vector<int> rename_map(nr_of_vars, 0);
    for(int cnt = 1; cnt < nr_of_vars; ++cnt)
      rename_map[cnt] = next_free_var++;
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
    check->startIncrementalSession(real_dep_vars, false);
    check->incAddCNF(mustBe0);
    gen->startIncrementalSession(real_dep_vars, false);
    gen->incAddCNF(mustBe1);
    CNF solution;
    while(true)
    {
      vector<int> false_pos;
      // compute a false-positives (ctrl-signal is 1 but must not be):
      statistics.notifyBeforeClauseComp();
      bool false_pos_exists = check->incIsSatModelOrCore(none, real_dep_vars, false_pos);

      statistics.notifyAfterClauseComp();
      if(!false_pos_exists)
        break;
      // generalize the false-positives:
      // (compute a larger set of situations (including the false-positive) for which
      // setting the control signal to 0 is allowed:
      vector<int> gen_false_pos;
      statistics.notifyBeforeClauseMin();
      Utils::randomize(false_pos);
      bool sat = gen->incIsSatModelOrCore(false_pos, none, gen_false_pos);
      MASSERT(!sat, "Impossible.");
      statistics.notifyAfterClauseMin(false_pos.size(), gen_false_pos.size());
      solution.addNegCubeAsClause(gen_false_pos);
      check->incAddNegCubeAsClause(gen_false_pos);
    }

    if(!second_run)
    {
      // dump solution in AIGER format:
      addToStandAloneAiger(current_ctrl, solution);
      // re-substitution:
      trans.addCNF(makeEq(current_ctrl, solution));
    }
    else
    {
      // store solution for future optimization:
      impl.push_back(solution);
      // re-substitution:
      CNF resub = makeEq(current_ctrl, solution);
      trans.addCNF(resub);
      c_eq_impl.push_back(resub);
    }
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

  if(second_run)
  {
    // Now the second step: try to reduce the implementations further
    gen->doMinCores(true);
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
      check->startIncrementalSession(dep_vars, false);
      check->incAddCNF(neg_rel_fixed);
      check->incAddUnitClause(current_ctrl);
      gen->startIncrementalSession(dep_vars, false);
      gen->incAddCNF(neg_rel_fixed);
      gen->incAddUnitClause(-current_ctrl);

      CNF solution;
      while(impl[ctrl_cnt].getNrOfClauses() > 0)
      {
        vector<int> neg_clause = impl[ctrl_cnt].removeSmallest();
        Utils::negateLiterals(neg_clause);
        vector<int> smaller_cube;
        statistics.notifyBeforeClauseMin();
        bool sat = gen->incIsSatModelOrCore(neg_clause, none, smaller_cube);
        statistics.notifyAfterClauseMin(neg_clause.size(), smaller_cube.size());
        MASSERT(!sat, "Impossible");
        solution.addNegCubeAsClause(smaller_cube);
        check->incAddNegCubeAsClause(smaller_cube);
        statistics.notifyBeforeClauseComp();
        sat = check->incIsSat();
        statistics.notifyAfterClauseComp();
        if(!sat)
          break;
      }
      impl[ctrl_cnt] = solution;
      addToStandAloneAiger(current_ctrl, solution);
      c_eq_impl[ctrl_cnt] = makeEq(current_ctrl, solution);
      statistics.notifyAfterCtrlSignal();
    }
  }

  // finally, we have to copy AND gates from the transition relation:
  insertMissingAndFromTrans();

  delete check;
  check = NULL;
  delete gen;
  gen = NULL;
}

// -------------------------------------------------------------------------------------------
void LearningImplExtractor::runLearningJiangSATTmpCtrlInc2(const CNF &win_region,
                                                           const CNF &neg_win_region,
                                                           bool second_run,
                                                           bool min_cores)
{
  const vector<int> &ctrl = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  typedef map<int, set<int> >::const_iterator MapConstIter;
  // setting the minimal core flag to false is faster but leads to larger circuits:
  SatSolver *solver = Options::instance().getSATSolverExtr(false, min_cores);
  CNF trans = AIG2CNF::instance().getTrans();
  vector<int> none;

  CNF next_win(win_region);
  next_win.swapPresentToNext();
  CNF leave_win(neg_win_region);
  leave_win.swapPresentToNext();
  leave_win.addCNF(win_region);

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
    int nr_of_vars = VarManager::instance().getMaxCNFVar() + 1;
    int next_free_var = nr_of_vars;
    vector<int> rename_map(nr_of_vars, 0);
    for(int cnt = 1; cnt < nr_of_vars; ++cnt)
      rename_map[cnt] = next_free_var++;
    for(size_t cnt = 0; cnt < real_dep_vars.size(); ++cnt)
      rename_map[real_dep_vars[cnt]] = real_dep_vars[cnt];

    int act_gen = next_free_var++;
    CNF canBe(trans);
    canBe.addCNF(next_win);
    canBe.renameVars(rename_map);
    CNF mustBe(trans);
    mustBe.addCNF(leave_win);
    mustBe.addCNF(canBe);
    // just to be sure that the solution does not depend on our artificial state variable
    // (this could otherwise happen if we do not use minimal cores):
    mustBe.setVarValue(VarManager::instance().getPresErrorStateVar(), false);
    mustBe.add2LitClause(-act_gen, rename_map[current_ctrl]);
    mustBe.add2LitClause(-act_gen, -current_ctrl);
    mustBe.add2LitClause(act_gen, -rename_map[current_ctrl]);
    mustBe.add2LitClause(act_gen, current_ctrl);

    // now we can do the actual learning loop:
    vector<int> non_frozen(real_dep_vars);
    non_frozen.push_back(act_gen);
    solver->startIncrementalSession(non_frozen, false);
    solver->incAddCNF(mustBe);
    CNF solution;
    vector<int> gen_ass(1, act_gen);
    vector<int> check_ass(1, -act_gen);
    while(true)
    {
      vector<int> false_pos;
      // compute a false-positives (ctrl-signal is 1 but must not be):
      statistics.notifyBeforeClauseComp();
      bool false_pos_exists = solver->incIsSatModelOrCore(none, check_ass, real_dep_vars, false_pos);

      statistics.notifyAfterClauseComp();
      if(!false_pos_exists)
        break;
      // generalize the false-positives:
      // (compute a larger set of situations (including the false-positive) for which
      // setting the control signal to 0 is allowed:
      vector<int> gen_false_pos;
      statistics.notifyBeforeClauseMin();
      Utils::randomize(false_pos);
      bool sat = solver->incIsSatModelOrCore(false_pos, gen_ass, none, gen_false_pos);
      MASSERT(!sat, "Impossible.");
      statistics.notifyAfterClauseMin(false_pos.size(), gen_false_pos.size());
      solution.addNegCubeAsClause(gen_false_pos);
      gen_false_pos.push_back(-act_gen);
      solver->incAddNegCubeAsClause(gen_false_pos);
    }

    if(!second_run)
    {
      // dump solution in AIGER format:
      addToStandAloneAiger(current_ctrl, solution);
      // re-substitution:
      trans.addCNF(makeEq(current_ctrl, solution));
    }
    else
    {
      // store solution for future optimization:
      impl.push_back(solution);
      // re-substitution:
      CNF resub = makeEq(current_ctrl, solution);
      trans.addCNF(resub);
      c_eq_impl.push_back(resub);
    }
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

  if(second_run)
  {
    // Now the second step: try to reduce the implementations further
    solver->doMinCores(true);
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
      vector<int> non_frozen(dep_vars);
      non_frozen.push_back(current_ctrl);
      solver->startIncrementalSession(non_frozen, false);
      solver->incAddCNF(neg_rel_fixed);

      CNF solution;
      vector<int> gen_ass(1, -current_ctrl);
      vector<int> check_ass(1, current_ctrl);
      while(impl[ctrl_cnt].getNrOfClauses() > 0)
      {
        vector<int> neg_clause = impl[ctrl_cnt].removeSmallest();
        Utils::negateLiterals(neg_clause);
        vector<int> smaller_cube;
        statistics.notifyBeforeClauseMin();
        bool sat = solver->incIsSatModelOrCore(neg_clause, gen_ass,  none, smaller_cube);
        statistics.notifyAfterClauseMin(neg_clause.size(), smaller_cube.size());
        MASSERT(!sat, "Impossible");
        solution.addNegCubeAsClause(smaller_cube);
        smaller_cube.push_back(current_ctrl);
        solver->incAddNegCubeAsClause(smaller_cube);
        statistics.notifyBeforeClauseComp();
        sat = solver->incIsSat(check_ass);
        statistics.notifyAfterClauseComp();
        if(!sat)
          break;
      }
      impl[ctrl_cnt] = solution;
      addToStandAloneAiger(current_ctrl, solution);
      c_eq_impl[ctrl_cnt] = makeEq(current_ctrl, solution);
      statistics.notifyAfterCtrlSignal();
    }
  }

  // finally, we have to copy AND gates from the transition relation:
  insertMissingAndFromTrans();

  delete solver;
  solver = NULL;
}


// -------------------------------------------------------------------------------------------
void LearningImplExtractor::runLearningJiangSATInc1(const CNF &win_region,
                                                   const CNF &neg_win_region,
                                                   bool second_run,
                                                   bool min_cores)
{
  const vector<int> &ctrl = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  SatSolver *check = Options::instance().getSATSolverExtr(false, false);
  SatSolver *gen = Options::instance().getSATSolverExtr(false, min_cores);
  vector<int> none;

  vector<CNF> c_eq_impl;
  c_eq_impl.reserve(ctrl.size());
  vector<CNF> impl;
  impl.reserve(ctrl.size());

  // we need to rename all variables except for the states and inputs:
  int nr_of_vars = VarManager::instance().getMaxCNFVar() + 1;
  vector<int> rename_map(nr_of_vars, 0);
  for(int cnt = 1; cnt < nr_of_vars; ++cnt)
    rename_map[cnt] = VarManager::instance().createFreshTmpVar();
  for(size_t cnt = 0; cnt < ip_.size(); ++cnt)
    rename_map[ip_[cnt]] = ip_[cnt];

  CNF mustBe(neg_win_region);
  mustBe.swapPresentToNext();
  mustBe.addCNF(win_region);
  mustBe.addCNF(AIG2CNF::instance().getTrans());
  CNF goto_win(win_region);
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
    act_vars[cnt] = VarManager::instance().createFreshTmpVar();
    eq_vars[cnt] = VarManager::instance().createFreshTmpVar();
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
  check->startIncrementalSession(vars_to_keep, false);
  check->incAddCNF(mustBe0);
  gen->startIncrementalSession(vars_to_keep, false);
  gen->incAddCNF(mustBe1);

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
      vector<int> false_pos;
      // compute a false-positives (ctrl-signal is 1 but must not be):
      statistics.notifyBeforeClauseComp();
      bool false_pos_exists = check->incIsSatModelOrCore(none, fixed, dep_vars, false_pos);
      statistics.notifyAfterClauseComp();
      if(!false_pos_exists)
        break;
      // generalize the false-positives:
      // (compute a larger set of situations (including the false-positive) for which
      // setting the control signal to 0 is allowed:
      vector<int> gen_false_pos;
      statistics.notifyBeforeClauseMin();
      bool sat = gen->incIsSatModelOrCore(false_pos, fixed, none, gen_false_pos);
      MASSERT(!sat, "Impossible.");

      statistics.notifyAfterClauseMin(false_pos.size(), gen_false_pos.size());
      solution.addNegCubeAsClause(gen_false_pos);
      gen_false_pos.push_back(act_vars[ctrl_cnt]);
      check->incAddNegCubeAsClause(gen_false_pos);
    }

    // dump solution in AIGER format or store it for future optimization:
    if(!second_run)
      addToStandAloneAiger(current_ctrl, solution);
    else
      impl.push_back(solution);

    // re-substitution:
    CNF orig_resub = makeEq(current_ctrl, solution);
    c_eq_impl.push_back(orig_resub);
    check->incAddCNF(orig_resub);
    gen->incAddCNF(orig_resub);
    solution.renameVars(rename_map);
    CNF renamed_resub = makeEq(rename_map[current_ctrl], solution);
    check->incAddCNF(renamed_resub);
    gen->incAddCNF(renamed_resub);
    statistics.notifyAfterCtrlSignal();
  }

  if(second_run)
  {
    // Now the second step: try to reduce the implementations further
    gen->doMinCores(true);
    CNF mustBe(neg_win_region);
    mustBe.swapPresentToNext();
    mustBe.addCNF(win_region);
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
    check->startIncrementalSession(vars_to_keep, false);
    check->incAddCNF(mustBe0);
    gen->startIncrementalSession(vars_to_keep, false);
    gen->incAddCNF(mustBe1);

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
        bool sat = gen->incIsSatModelOrCore(neg_clause, fixed, none, smaller_cube);
        statistics.notifyAfterClauseMin(neg_clause.size(), smaller_cube.size());
        MASSERT(!sat, "Impossible");
        solution.addNegCubeAsClause(smaller_cube);
        smaller_cube.push_back(act_vars[ctrl_cnt]);
        check->incAddNegCubeAsClause(smaller_cube);
        statistics.notifyBeforeClauseComp();
        sat = check->incIsSat(fixed);
        statistics.notifyAfterClauseComp();
        if(!sat)
          break;
      }
      addToStandAloneAiger(current_ctrl, solution);
      c_eq_impl[ctrl_cnt] = makeEq(current_ctrl, solution);
      check->incAddCNF(c_eq_impl[ctrl_cnt]);
      gen->incAddCNF(c_eq_impl[ctrl_cnt]);
    }
  }

  delete check;
  check = NULL;
  delete gen;
  gen = NULL;
}

// -------------------------------------------------------------------------------------------
void LearningImplExtractor::runLearningJiangSATInc2(const CNF &win_region,
                                                       const CNF &neg_win_region,
                                                       bool second_run,
                                                       bool min_cores)
{
  const vector<int> &ctrl = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  SatSolver *solver = Options::instance().getSATSolverExtr(false, min_cores);
  vector<int> none;

  vector<CNF> c_eq_impl;
  c_eq_impl.reserve(ctrl.size());
  vector<CNF> impl;
  impl.reserve(ctrl.size());

  // we need to rename all variables except for the states and inputs:
  int nr_of_vars = VarManager::instance().getMaxCNFVar() + 1;
  vector<int> rename_map(nr_of_vars, 0);
  for(int cnt = 1; cnt < nr_of_vars; ++cnt)
    rename_map[cnt] = VarManager::instance().createFreshTmpVar();
  for(size_t cnt = 0; cnt < ip_.size(); ++cnt)
    rename_map[ip_[cnt]] = ip_[cnt];

  CNF mustBe(neg_win_region);
  mustBe.swapPresentToNext();
  mustBe.addCNF(win_region);
  mustBe.addCNF(AIG2CNF::instance().getTrans());
  CNF goto_win(win_region);
  goto_win.swapPresentToNext();
  goto_win.addCNF(AIG2CNF::instance().getTrans());
  goto_win.renameVars(rename_map);
  mustBe.addCNF(goto_win);

  // We need two activation variables per control signal. These activation variables
  // activates all control-signal-dependent constraints for mustBe0 and mustBe1:
  vector<int> act_vars0(ctrl.size(), 0);
  vector<int> act_vars1(ctrl.size(), 0);
  // If eq_vars[cnt] is set then ctrl[i] is equal to its copy for all cnt<i<ctrl.size()
  vector<int> eq_vars(ctrl.size(), 0);
  for(size_t cnt = 0; cnt < ctrl.size(); ++cnt)
  {
    act_vars1[cnt] = VarManager::instance().createFreshTmpVar();
    act_vars0[cnt] = VarManager::instance().createFreshTmpVar();
    eq_vars[cnt] = VarManager::instance().createFreshTmpVar();
    mustBe.add2LitClause(-act_vars1[cnt], -ctrl[cnt]);
    mustBe.add2LitClause(-act_vars1[cnt], rename_map[ctrl[cnt]]);
    mustBe.add2LitClause(-act_vars0[cnt], ctrl[cnt]);
    mustBe.add2LitClause(-act_vars0[cnt], -rename_map[ctrl[cnt]]);
  }
  for(size_t cnt = 0; cnt < ctrl.size() - 1; ++cnt)
  {
    // ctrl[cnt + 1] must be equal to its copy:
    mustBe.add3LitClause(-eq_vars[cnt], ctrl[cnt+1], -rename_map[ctrl[cnt+1]]);
    mustBe.add3LitClause(-eq_vars[cnt], -ctrl[cnt+1], rename_map[ctrl[cnt+1]]);
    // eq_vars[cnt + 1] must be set:
    mustBe.add2LitClause(-eq_vars[cnt], eq_vars[cnt+1]);
    mustBe.add2LitClause(-act_vars1[cnt], eq_vars[cnt]);
    mustBe.add2LitClause(-act_vars0[cnt], eq_vars[cnt]);
  }

  // now we initialize the solver:
  vector<int> vars_to_keep;
  vars_to_keep.reserve(ipc_.size() + ctrl.size() + ctrl.size() + ctrl.size());
  vars_to_keep.insert(vars_to_keep.end(), ipc_.begin(), ipc_.end());
  for(size_t cnt = 0; cnt < ctrl.size(); ++cnt)
  {
    vars_to_keep.push_back(act_vars1[cnt]);
    vars_to_keep.push_back(act_vars0[cnt]);
    vars_to_keep.push_back(rename_map[ctrl[cnt]]);
  }
  solver->startIncrementalSession(vars_to_keep, false);
  solver->incAddCNF(mustBe);

  vector<int> dep_vars(ipc_);
  for(size_t ctrl_cnt = 0; ctrl_cnt < ctrl.size(); ++ctrl_cnt)
  {
    statistics.notifyBeforeCtrlSignal();
    int current_ctrl = ctrl[ctrl_cnt];
    Utils::remove(dep_vars, current_ctrl);
    vector<int> check(1, act_vars0[ctrl_cnt]);
    vector<int> gen(1, act_vars1[ctrl_cnt]);

    // now we can do the actual learning loop:
    CNF solution;
    while(true)
    {
      vector<int> false_pos;
      // compute a false-positives (ctrl-signal is 1 but must not be):
      statistics.notifyBeforeClauseComp();
      bool false_pos_exists = solver->incIsSatModelOrCore(none, check, dep_vars, false_pos);
      statistics.notifyAfterClauseComp();
      if(!false_pos_exists)
        break;
      // generalize the false-positives:
      // (compute a larger set of situations (including the false-positive) for which
      // setting the control signal to 0 is allowed:
      vector<int> gen_false_pos;
      statistics.notifyBeforeClauseMin();
      bool sat = solver->incIsSatModelOrCore(false_pos, gen, none, gen_false_pos);
      MASSERT(!sat, "Impossible.");

      statistics.notifyAfterClauseMin(false_pos.size(), gen_false_pos.size());
      solution.addNegCubeAsClause(gen_false_pos);
      gen_false_pos.push_back(act_vars0[ctrl_cnt]);
      solver->incAddNegCubeAsClause(gen_false_pos);
    }

    // dump solution in AIGER format or store it for future optimization:
    if(!second_run)
      addToStandAloneAiger(current_ctrl, solution);
    else
      impl.push_back(solution);

    // re-substitution:
    CNF orig_resub = makeEq(current_ctrl, solution);
    c_eq_impl.push_back(orig_resub);
    solver->incAddCNF(orig_resub);
    solution.renameVars(rename_map);
    CNF renamed_resub = makeEq(rename_map[current_ctrl], solution);
    solver->incAddCNF(renamed_resub);
    statistics.notifyAfterCtrlSignal();
  }

  if(second_run)
  {
    // Now the second step: try to reduce the implementations further
    solver->doMinCores(true);
    CNF mustBe(neg_win_region);
    mustBe.swapPresentToNext();
    mustBe.addCNF(win_region);
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
      mustBe.add2LitClause(-act_vars0[cnt], eq_vars[cnt]);
      mustBe.add2LitClause(-act_vars1[cnt], eq_vars[cnt]);
    }
    for(size_t cnt = 0; cnt < ctrl.size(); ++cnt)
    {
      mustBe.add2LitClause(-act_vars0[cnt], ctrl[cnt]);
      mustBe.add2LitClause(-act_vars1[cnt], -ctrl[cnt]);
    }

    // now we initialize the solvers:
    vector<int> vars_to_keep;
    vars_to_keep.reserve(ipc_.size() + act_vars0.size() + act_vars1.size());
    vars_to_keep.insert(vars_to_keep.end(), ipc_.begin(), ipc_.end());
    vars_to_keep.insert(vars_to_keep.end(), act_vars0.begin(), act_vars0.end());
    vars_to_keep.insert(vars_to_keep.end(), act_vars1.begin(), act_vars1.end());
    solver->startIncrementalSession(vars_to_keep, false);
    solver->incAddCNF(mustBe);

    for(size_t ctrl_cnt = 0; ctrl_cnt < ctrl.size(); ++ctrl_cnt)
    {
      statistics.notifyBeforeCtrlSignal();
      int current_ctrl = ctrl[ctrl_cnt];
      vector<int> check(1, act_vars0[ctrl_cnt]);
      vector<int> gen(1, act_vars1[ctrl_cnt]);

      CNF solution;
      while(impl[ctrl_cnt].getNrOfClauses() > 0)
      {
        vector<int> neg_clause = impl[ctrl_cnt].removeSmallest();
        Utils::negateLiterals(neg_clause);
        vector<int> smaller_cube;
        statistics.notifyBeforeClauseMin();
        bool sat = solver->incIsSatModelOrCore(neg_clause, gen, none, smaller_cube);
        statistics.notifyAfterClauseMin(neg_clause.size(), smaller_cube.size());
        MASSERT(!sat, "Impossible");
        solution.addNegCubeAsClause(smaller_cube);
        smaller_cube.push_back(act_vars0[ctrl_cnt]);
        solver->incAddNegCubeAsClause(smaller_cube);
        statistics.notifyBeforeClauseComp();
        sat = solver->incIsSat(check);
        statistics.notifyAfterClauseComp();
        if(!sat)
          break;
      }
      addToStandAloneAiger(current_ctrl, solution);
      solver->incAddCNF(makeEq(current_ctrl, solution));
      statistics.notifyAfterCtrlSignal();
    }
  }

  delete solver;
  solver = NULL;
}

// -------------------------------------------------------------------------------------------
CNF LearningImplExtractor::makeEq(int var, CNF impl)
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
  CNF neg_impl(impl);
  neg_impl.negate();
  // the last clause of our negation is the disjunction which says that one clause must be
  // false. Adding var only to this clause and not to all clauses seems to be better for the
  // solver:
  list<vector<int> > m;
  neg_impl.swapWith(m);
  m.back().push_back(var);
  neg_impl.swapWith(m);
  res.addCNF(neg_impl);

  // adding var to every clause of the negation is slower, at least with MiniSat:
  //const list<vector<int> > &neg_impl_clauses = neg_impl.getClauses();
  //for(CNF::ClauseConstIter it = neg_impl_clauses.begin(); it != neg_impl_clauses.end(); ++it)
  //{
  //  vector<int> clause(*it);
  //  clause.push_back(var);
  //  res.addClause(clause);
  //}

  return res;
}

// -------------------------------------------------------------------------------------------
CNF LearningImplExtractor::makeEq2(int var, CNF impl)
{
  CNF res;
  const list<vector<int> > &impl_clauses = impl.getClauses();

  // if we do not have any clauses, the result is true:
  if(impl_clauses.size() == 0)
    return res;

  // if one clause is empty, the result is false:
  for(CNF::ClauseConstIter it = impl_clauses.begin(); it != impl_clauses.end(); ++it)
  {
    if(it->size() == 0)
    {
      res.addClause(vector<int>());
      return res;
    }
  }

  int and_res = 0;
  for(CNF::ClauseConstIter it = impl_clauses.begin(); it != impl_clauses.end(); ++it)
  {
    int or_res = (*it)[0];
    for(size_t lit_cnt = 1; lit_cnt < it->size(); ++lit_cnt)
    {
      // or_res = OR(or_res, (*it)[lit_cnt]):
      int old_or_res = or_res;
      or_res = VarManager::instance().createFreshTmpVar();
      res.add2LitClause(-old_or_res, or_res);
      res.add2LitClause(-((*it)[lit_cnt]), or_res);
      res.add3LitClause(old_or_res, ((*it)[lit_cnt]), -or_res);
    }
    if(it == impl_clauses.begin())
      and_res = or_res;
    else
    {
      // and_res = AND(and_res, or_res):
      int old_and_res = and_res;
      and_res = VarManager::instance().createFreshTmpVar();
      res.add2LitClause(old_and_res, -and_res);
      res.add2LitClause(or_res, -or_res);
      res.add3LitClause(-old_and_res, -or_res, or_res);
    }
  }
  // var = and_res:
  res.add2LitClause(-and_res, var);
  res.add2LitClause(and_res, -var);
  return res;
}

// -------------------------------------------------------------------------------------------
void LearningImplExtractor::addToStandAloneAiger(int ctrl_var, const CNF &solution)
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
int LearningImplExtractor::cnfToAig(int cnf_lit)
{
  int cnf_var = (cnf_lit < 0) ? -cnf_lit : cnf_lit;
  if(cnf_lit < 0)
    return aiger_not(cnf_var_to_standalone_aig_var_[cnf_var]);
  else
    return cnf_var_to_standalone_aig_var_[cnf_var];
}

// -------------------------------------------------------------------------------------------
int LearningImplExtractor::makeAnd(int in1, int in2)
{
  int res = next_free_aig_lit_;
  aiger_add_and(standalone_circuit_, next_free_aig_lit_, in1, in2);
  next_free_aig_lit_ += 2;
  return res;
}

// -------------------------------------------------------------------------------------------
int LearningImplExtractor::makeOr(int in1, int in2)
{
  int res = aiger_not(next_free_aig_lit_);
  aiger_add_and(standalone_circuit_, next_free_aig_lit_, aiger_not(in1), aiger_not(in2));
  next_free_aig_lit_ += 2;
  return res;
}

// -------------------------------------------------------------------------------------------
void LearningImplExtractor::insertMissingAndFromTrans()
{
  VarManager &VM = VarManager::instance();

  // Step 1: find out which variables are not yet defined in the standalone_circuit_:
  set<int> ref_aiger_vars;
  for(unsigned and_cnt = 0; and_cnt < standalone_circuit_->num_ands; ++and_cnt)
  {
    ref_aiger_vars.insert(aiger_strip(standalone_circuit_->ands[and_cnt].rhs0));
    ref_aiger_vars.insert(aiger_strip(standalone_circuit_->ands[and_cnt].rhs1));
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
      aiger_add_and(standalone_circuit_, cnfToAig(lhs_cnf), r0, r1);
    }
  }
  aiger_reset(aig);
}

// -------------------------------------------------------------------------------------------
void LearningImplExtractor::logDetailedStatistics()
{
  statistics.logStatistics();
}
