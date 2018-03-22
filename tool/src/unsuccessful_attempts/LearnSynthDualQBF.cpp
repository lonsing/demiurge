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

#include "LearnSynthDualQBF.h"
#include "VarManager.h"
#include "CNF.h"
#include "QBFSolver.h"
#include "Options.h"
#include "AIG2CNF.h"
#include "Logger.h"
#include "QBFCertImplExtractor.h"
#include "DepQBFExt.h"
#include "LingelingApi.h"
#include "Utils.h"

#include <sys/stat.h>

extern "C" {
 #include "aiger.h"
}

// -------------------------------------------------------------------------------------------
LearnSynthDualQBF::LearnSynthDualQBF() :
                   BackEnd(),
                   qbf_solver_(Options::instance().getQBFSolver()),
                   t_renamed_copy_(0)
{
  // build the quantifier prefix for checking for counterexamples:
  //   check_quant = exists x,i: forall c: exists x',tmp:
  // for the check-queries:
  check_quant_.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::E));
  check_quant_.push_back(make_pair(VarInfo::INPUT, QBFSolver::E));
  check_quant_.push_back(make_pair(VarInfo::CTRL, QBFSolver::A));
  check_quant_.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::E));
  check_quant_.push_back(make_pair(VarInfo::TMP, QBFSolver::E));

  // build the quantifier prefix for generalizing cubes:
  //    gen_quant = exists x: forall i: exists c,x',tmp:
  // for the generalization queries:
  gen_quant_.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::E));
  gen_quant_.push_back(make_pair(VarInfo::INPUT, QBFSolver::A));
  gen_quant_.push_back(make_pair(VarInfo::CTRL, QBFSolver::E));
  gen_quant_.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::E));
  gen_quant_.push_back(make_pair(VarInfo::TMP, QBFSolver::E));

  last_check_clause_.reserve(10000);

  // we copy and rename all temporary variables in the transition relation:
  VarManager &VM = VarManager::instance();
  const vector<int> &s = VM.getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &s_next = VM.getVarsOfType(VarInfo::NEXT_STATE);
  const vector<int> &c = VM.getVarsOfType(VarInfo::CTRL);
  const vector<int> &i = VM.getVarsOfType(VarInfo::INPUT);
  vector<int> t = VM.getVarsOfType(VarInfo::TMP);
  vector<int> orig_to_copy_map;
  orig_to_copy_map.resize(VM.getMaxCNFVar()+1, 0);
  for(size_t v_cnt = 0; v_cnt < s.size(); ++v_cnt)
    orig_to_copy_map[s[v_cnt]] = s[v_cnt];
  for(size_t v_cnt = 0; v_cnt < s_next.size(); ++v_cnt)
    orig_to_copy_map[s_next[v_cnt]] = s_next[v_cnt];
  for(size_t v_cnt = 0; v_cnt < c.size(); ++v_cnt)
    orig_to_copy_map[c[v_cnt]] = c[v_cnt];
  for(size_t v_cnt = 0; v_cnt < i.size(); ++v_cnt)
    orig_to_copy_map[i[v_cnt]] = i[v_cnt];
  for(size_t v_cnt = 0; v_cnt < t.size(); ++v_cnt)
    orig_to_copy_map[t[v_cnt]] = VM.createFreshTmpVar();
  list<vector<int> > trans_cl = AIG2CNF::instance().getTransEqT().getClauses();
  for(CNF::ClauseIter it = trans_cl.begin(); it != trans_cl.end(); ++it)
  {
    for(size_t var_cnt = 0; var_cnt < it->size(); ++var_cnt)
    {
      int literal = (*it)[var_cnt];
      int var = (literal < 0) ? -literal : literal;
      if(literal < 0)
        (*it)[var_cnt] = -orig_to_copy_map[var];
      else
        (*it)[var_cnt] = orig_to_copy_map[var];
    }
  }
  trans_eq_t_renamed_copy_.swapWith(trans_cl);
  int t_lit = AIG2CNF::instance().getT();
  int t_var = (t_lit < 0) ? -t_lit : t_lit;
  if(t_lit < 0)
    t_renamed_copy_ = -orig_to_copy_map[t_var];
  else
    t_renamed_copy_ = orig_to_copy_map[t_var];

  string in_file = Options::instance().getAigInFileName();
  size_t pos1 = in_file.find_last_of("/");
  string filename;
  if(pos1 != std::string::npos)
    filename.assign(in_file.begin() + pos1 + 1, in_file.end());
  else
    filename = in_file;
  size_t pos2 = filename.find_last_of(".");
  if(pos2 != std::string::npos)
    filename = filename.substr(0, pos2);


  string check_dir_name = Options::instance().getTmpDirName() + "check/";
  struct stat st;
  if(stat(check_dir_name.c_str(), &st) != 0)
  {
    int fail = mkdir(check_dir_name.c_str(), 0777);
    MASSERT(fail == 0, "Could not create directory for temporary files.");
  }

  string gen_dir_name = Options::instance().getTmpDirName() + "gen/";
  if(stat(gen_dir_name.c_str(), &st) != 0)
  {
    int fail = mkdir(gen_dir_name.c_str(), 0777);
    MASSERT(fail == 0, "Could not create directory for temporary files.");
  }

  filename_prefix_check_ = check_dir_name + filename;
  filename_prefix_gen_  = gen_dir_name + filename;
}

// -------------------------------------------------------------------------------------------
LearnSynthDualQBF::~LearnSynthDualQBF()
{
  delete qbf_solver_;
  qbf_solver_ = NULL;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthDualQBF::run()
{
  VarManager::instance().push();
//  L_DBG(VarManager::instance().toString());
//  L_DBG("TRANS:");
//  L_DBG(AIG2CNF::instance().getTrans().toString());
//  L_DBG("TRANS eq t:");
//  L_DBG(AIG2CNF::instance().getTransEqT().toString());
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
  statistics_.notifyRelDetStart();
  QBFCertImplExtractor extractor;
  extractor.extractCircuit(winning_region_);
  statistics_.notifyRelDetEnd();
  L_INF("Synthesis done.");
  statistics_.logStatistics();
  return true;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthDualQBF::computeWinningRegion()
{
  bool real =  computeWinningRegionOne();
  //bool real =  computeWinningRegionAll();
  if(real)
  {
    Utils::compressStateCNF(winning_region_);
    neg_winning_region_ = winning_region_;
    neg_winning_region_.negate();
    //Utils::debugCheckWinReg(winning_region_, neg_winning_region_);
  }
  return real;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthDualQBF::computeWinningRegionOne()
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
  check_cnf_.clear();
  check_cnf_.addCNF(A2C.getTrans());
  check_cnf_.addCNF(A2C.getSafeStates());
  CNF p2_eq_next_safe;
  int p2 = VM.createFreshTmpVar("eq_next_save");
  p2_eq_next_safe.add2LitClause(VM.getNextErrorStateVar(), p2);
  p2_eq_next_safe.add2LitClause(-VM.getNextErrorStateVar(), -p2);
  check_cnf_.addCNF(p2_eq_next_safe);
  check_cnf_.add1LitClause(-p2);
  last_check_clause_.clear();
  last_check_clause_.push_back(-p2);

  // build generalize_clause_cnf = P(x) AND T(x,i,c,x') AND P(x')
  generalize_clause_cnf_.clear();
  generalize_clause_cnf_.addCNF(A2C.getTrans());
  generalize_clause_cnf_.addCNF(A2C.getNextSafeStates());
  generalize_clause_cnf_.addCNF(A2C.getSafeStates());

  vector<int> counterexample;
  vector<int> blocking_clause;
  blocking_clause.reserve(VM.getVarsOfType(VarInfo::PRES_STATE).size());
  it_count_ = 0;
  bool ce_found = computeCounterexampleQBF(counterexample);

  while(ce_found)
  {
    it_count_++;
    L_DBG("Found bad state (nr " << it_count_ << "), excluding it from the winning region.");
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
    //  reduceExistingClauses(false);

    compressCNFs(false);

    if(it_count_ % 100 == 0)
    {
      L_DBG("Statistics at count=" << it_count_);
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
  //reduceExistingClauses(false);
  compressCNFs(false);
  return true;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthDualQBF::computeCounterexampleQBF(vector<int> &ce)
{
  ce.clear();
  statistics_.notifyBeforeComputeCube();
  bool found = qbf_solver_->isSatModel(check_quant_, check_cnf_, ce);
  ostringstream filename;
  filename << filename_prefix_check_ << "_check_" << it_count_ << ".qdimacs";
  VarManager::push();
  dumpNegCheckQBF();
  ExtQBFSolver::dumpQBF(check_quant_, check_cnf_, filename.str());
  VarManager::pop();
  statistics_.notifyAfterComputeCube();
  return found;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthDualQBF::computeBlockingClause(vector<int> &ce, vector<int> &blocking_clause)
{
  statistics_.notifyBeforeCubeMin();
  restrictToStates(ce);
  Utils::randomize(ce);
  set<int> gen_ce;
  for(size_t cnt = 0; cnt < ce.size(); ++cnt)
    gen_ce.insert(ce[cnt]);
  if(Utils::containsInit(gen_ce))
    return false;
  for(size_t cnt = 0; cnt < ce.size(); ++cnt)
  {
    CNF additional_clauses;
    set<int> tmp = gen_ce;
    tmp.erase(ce[cnt]);
    CNF tmp_cnf(generalize_clause_cnf_);
    for(set<int>::const_iterator it = tmp.begin(); it != tmp.end(); ++it)
    {
      tmp_cnf.add1LitClause(*it);
      additional_clauses.add1LitClause(*it);
    }
    vector<int> not_gen_ce;
    not_gen_ce.reserve(gen_ce.size());
    for(set<int>::const_iterator it = gen_ce.begin(); it != gen_ce.end(); ++it)
      not_gen_ce.push_back(-(*it));
    tmp_cnf.addClause(not_gen_ce);
    additional_clauses.addClause(not_gen_ce);
    Utils::swapPresentToNext(not_gen_ce);
    tmp_cnf.addClause(not_gen_ce);
    additional_clauses.addClause(not_gen_ce);

    ostringstream filename;
    filename << filename_prefix_gen_ << "_gen_" << it_count_ << "_" << cnt  << ".qdimacs";
    //ExtQBFSolver::dumpQBF(gen_quant_, tmp_cnf, filename.str());
    //dumpNegGenQBF(additional_clauses, cnt);

    if(!qbf_solver_->isSat(gen_quant_, tmp_cnf))
    {
      gen_ce = tmp;
      if(Utils::containsInit(gen_ce))
        return false;
    }
  }
  blocking_clause.clear();
  for(set<int>::const_iterator it = gen_ce.begin(); it != gen_ce.end(); ++it)
    blocking_clause.push_back(-(*it));
  L_DBG("computeBlockingClause(): " << ce.size() << " --> " << blocking_clause.size());
  statistics_.notifyAfterCubeMin(ce.size(), blocking_clause.size());
  return true;
}

// -------------------------------------------------------------------------------------------
void LearnSynthDualQBF::reduceExistingClauses(bool take_small_win)
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
      // tmp is clause without (*it)[cnt]
      vector<int> tmp;
      tmp.reserve(clause.size());
      for(size_t i = 0; i < clause.size(); ++i)
      {
        if(clause[i] != (*it)[cnt])
          tmp.push_back(clause[i]);
      }

      CNF tmp_cnf(generalize_clause_cnf_);
      for(size_t i = 0; i < tmp.size(); ++i)
        tmp_cnf.add1LitClause(-tmp[i]);
      if(!qbf_solver_->isSat(gen_quant_, tmp_cnf))
        clause = tmp;
    }
    if(it->size() > clause.size())
    {
      winning_region_.addClauseAndSimplify(clause);
      winning_region_large_.addClauseAndSimplify(clause);
      generalize_clause_cnf_.addClauseAndSimplify(clause);
      Utils::swapPresentToNext(clause);
      generalize_clause_cnf_.addClauseAndSimplify(clause);
    }
  }
  size_t new_cl = winning_region_.getNrOfClauses();
  size_t new_lits = winning_region_.getNrOfLits();;
  L_DBG("reduceExistingClauses(): " << or_cl << " --> " << new_cl << " clauses.");
  L_DBG("reduceExistingClauses(): " << or_lits << " --> " << new_lits << " literals.");

  recomputeCheckCNF(take_small_win);
  statistics_.notifyAfterCubeMin(0, 0);
}

// -------------------------------------------------------------------------------------------
void LearnSynthDualQBF::compressCNFs(bool update_all)
{
  if(winning_region_.getNrOfClauses() == 0)
    return;

  size_t or_cl = winning_region_.getNrOfClauses();
  size_t or_lits = winning_region_.getNrOfLits();

  LingelingApi sat_solver;
  CNF still_to_process(winning_region_);
  winning_region_.clear();
  vector<int> first = still_to_process.removeSmallest();
  winning_region_.addClause(first);
  if(update_all)
  {
    generalize_clause_cnf_.clear();
    generalize_clause_cnf_.addCNF(AIG2CNF::instance().getTrans());
    generalize_clause_cnf_.addClause(first);
    Utils::swapPresentToNext(first);
    generalize_clause_cnf_.addClause(first);
  }
  while(still_to_process.getNrOfClauses() != 0)
  {
    vector<int> check = still_to_process.removeSmallest();
    CNF tmp(winning_region_);
    for(size_t cnt = 0; cnt < check.size(); ++cnt)
      tmp.add1LitClause(-check[cnt]);
    if(sat_solver.isSat(tmp))
    {
      winning_region_.addClause(check);
      if(update_all)
      {
        generalize_clause_cnf_.addClause(check);
        Utils::swapPresentToNext(check);
        generalize_clause_cnf_.addClause(check);
      }
    }
  }

  recomputeCheckCNF(update_all);

  size_t new_cl = winning_region_.getNrOfClauses();
  size_t new_lits = winning_region_.getNrOfLits();

  L_DBG("compressCNFs(): " << or_cl << " --> " << new_cl << " clauses.");
  L_DBG("compressCNFs(): " << or_lits << " --> " << new_lits << " literals.");

}

// -------------------------------------------------------------------------------------------
void LearnSynthDualQBF::recomputeCheckCNF(bool take_small_win)
{
  VarManager::resetToLastPush();
  check_cnf_.clear();
  last_check_clause_.clear();
  check_cnf_.addCNF(AIG2CNF::instance().getTrans());
  if(take_small_win)
    check_cnf_.addCNF(winning_region_);
  else
    check_cnf_.addCNF(winning_region_large_);
  const list<vector<int> > &new_clauses =  winning_region_.getClauses();
  for(CNF::ClauseConstIter it = new_clauses.begin(); it != new_clauses.end(); ++it)
  {
    vector<int> clause(*it);
    Utils::swapPresentToNext(clause);
    // construct l -> gen_ce(x'), where l is a fresh variable
    int l = VarManager::instance().createFreshTmpVar("next gen ce");
    for(size_t cnt = 0; cnt < clause.size(); ++cnt)
      check_cnf_.add2LitClause(-l, -clause[cnt]);
    last_check_clause_.push_back(l);
  }
  check_cnf_.addClause(last_check_clause_);
}

// -------------------------------------------------------------------------------------------
void LearnSynthDualQBF::restrictToStates(vector<int> &vec) const
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
bool LearnSynthDualQBF::generalizeCounterexample(set<int> &ce, bool check_sat) const
{
  if(check_sat)
  {
    CNF tmp_cnf1(generalize_clause_cnf_);
    for(set<int>::const_iterator it = ce.begin(); it != ce.end(); ++it)
      tmp_cnf1.add1LitClause(*it);
    if(qbf_solver_->isSat(gen_quant_, tmp_cnf1))
      return false;
  }
  set<int> gen_ce(ce);
  for(set<int>::const_iterator it = ce.begin(); it != ce.end(); ++it)
  {
    set<int> tmp = gen_ce;
    tmp.erase(*it);
    CNF tmp_cnf(generalize_clause_cnf_);
    for(set<int>::const_iterator it2 = tmp.begin(); it2 != tmp.end(); ++it2)
      tmp_cnf.add1LitClause(*it2);
    vector<int> not_gen_ce;
    not_gen_ce.reserve(gen_ce.size());
    for(set<int>::const_iterator it = gen_ce.begin(); it != gen_ce.end(); ++it)
      not_gen_ce.push_back(-(*it));
    tmp_cnf.addClause(not_gen_ce);
    Utils::swapPresentToNext(not_gen_ce);
    tmp_cnf.addClause(not_gen_ce);
    if(!qbf_solver_->isSat(gen_quant_, tmp_cnf))
      gen_ce = tmp;
  }
  ce = gen_ce;
  return true;
}

// -------------------------------------------------------------------------------------------
void LearnSynthDualQBF::dumpNegCheckQBF()
{
  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > neg_check_quant;
  neg_check_quant.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::A));
  neg_check_quant.push_back(make_pair(VarInfo::INPUT, QBFSolver::A));
  neg_check_quant.push_back(make_pair(VarInfo::CTRL, QBFSolver::E));
  neg_check_quant.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::A));
  neg_check_quant.push_back(make_pair(VarInfo::TMP, QBFSolver::E));


  // build neg_check_cnf = NOT win(x) OR NOT T(x,i,c,x') OR win(x')
  CNF neg_check_cnf;
  neg_check_cnf.addCNF(AIG2CNF::instance().getTransEqT());
  //neg_check_cnf.addCNF(trans_eq_t_renamed_copy_);
  const list<vector<int> > &win_clauses = winning_region_.getClauses();

  int win_now_lit = VarManager::instance().createFreshTmpVar();
  int win_next_lit = VarManager::instance().createFreshTmpVar();
  vector<int> all_true_implies_win_now_lit_is_true;
  all_true_implies_win_now_lit_is_true.reserve(win_clauses.size() + 1);
  all_true_implies_win_now_lit_is_true.push_back(win_now_lit);
  vector<int> all_true_implies_win_next_lit_is_true;
  all_true_implies_win_next_lit_is_true.reserve(win_clauses.size() + 1);
  all_true_implies_win_next_lit_is_true.push_back(win_next_lit);

  for(CNF::ClauseConstIter it = win_clauses.begin(); it != win_clauses.end(); ++it)
  {
    const vector<int> &now_clause = *it;
    int now_clause_is_true = VarManager::instance().createFreshTmpVar();
    vector<int> all_false_implies_now_clause_is_false;
    all_false_implies_now_clause_is_false.reserve(now_clause.size() + 1);
    all_false_implies_now_clause_is_false.push_back(-now_clause_is_true);
    for(size_t lit_cnt = 0; lit_cnt < now_clause.size(); ++lit_cnt)
    {
      // if one of the literals is true, then now_clause_is_true is true
      neg_check_cnf.add2LitClause(-now_clause[lit_cnt], now_clause_is_true);
      all_false_implies_now_clause_is_false.push_back(now_clause[lit_cnt]);
    }
    neg_check_cnf.addClause(all_false_implies_now_clause_is_false);

    // now_clause_is_true is false implies win_now_lit is false
    neg_check_cnf.add2LitClause(now_clause_is_true, -win_now_lit);
    all_true_implies_win_now_lit_is_true.push_back(-now_clause_is_true);

    vector<int> next_clause(*it);
    Utils::swapPresentToNext(next_clause);
    int next_clause_is_true = VarManager::instance().createFreshTmpVar();
    vector<int> all_false_implies_next_clause_is_false;
    all_false_implies_next_clause_is_false.reserve(next_clause.size() + 1);
    all_false_implies_next_clause_is_false.push_back(-next_clause_is_true);
    for(size_t lit_cnt = 0; lit_cnt < next_clause.size(); ++lit_cnt)
    {
      // if one of the literals is true, then now_clause_is_true is true
      neg_check_cnf.add2LitClause(-next_clause[lit_cnt], next_clause_is_true);
      all_false_implies_next_clause_is_false.push_back(next_clause[lit_cnt]);
    }
    neg_check_cnf.addClause(all_false_implies_next_clause_is_false);

    // next_clause_is_true is false implies win_next_lit is false
    neg_check_cnf.add2LitClause(next_clause_is_true, -win_next_lit);
    all_true_implies_win_next_lit_is_true.push_back(-next_clause_is_true);
  }
  neg_check_cnf.addClause(all_true_implies_win_now_lit_is_true);
  neg_check_cnf.addClause(all_true_implies_win_next_lit_is_true);

  neg_check_cnf.add3LitClause(-AIG2CNF::instance().getT(), -win_now_lit, win_next_lit);
  //neg_check_cnf.add3LitClause(-t_renamed_copy_, -win_now_lit, win_next_lit);
  ostringstream filename;
  filename << filename_prefix_check_ << "_check_" << it_count_ << "_neg.qdimacs";
  ExtQBFSolver::dumpQBF(neg_check_quant, neg_check_cnf, filename.str());
}

// -------------------------------------------------------------------------------------------
void LearnSynthDualQBF::dumpNegGenQBF(CNF additional_clauses, size_t id)
{
  VarManager::push();

  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > neg_gen_quant;
  neg_gen_quant.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::A));
  neg_gen_quant.push_back(make_pair(VarInfo::INPUT, QBFSolver::E));
  neg_gen_quant.push_back(make_pair(VarInfo::CTRL, QBFSolver::A));
  neg_gen_quant.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::A));
  neg_gen_quant.push_back(make_pair(VarInfo::TMP, QBFSolver::E));

  // build neg_check_cnf = NOT T(x,i,c,x') OR NOT (win(x) AND win(x') and additional_clauses)
  CNF other_clauses_cnf(winning_region_);
  other_clauses_cnf.swapPresentToNext();
  other_clauses_cnf.addCNF(winning_region_);
  other_clauses_cnf.addCNF(additional_clauses);

  CNF neg_gen_cnf;
  neg_gen_cnf.addCNF(AIG2CNF::instance().getTransEqT());
  const list<vector<int> > &other_clauses = other_clauses_cnf.getClauses();
  int other_clauses_lit = VarManager::instance().createFreshTmpVar();

  vector<int> all_true_implies_other_clauses_lit_is_true;
  all_true_implies_other_clauses_lit_is_true.reserve(other_clauses.size() + 1);
  all_true_implies_other_clauses_lit_is_true.push_back(other_clauses_lit);

  for(CNF::ClauseConstIter it = other_clauses.begin(); it != other_clauses.end(); ++it)
  {
    const vector<int> &clause = *it;
    int clause_is_true = VarManager::instance().createFreshTmpVar();
    vector<int> all_false_implies_clause_is_false;
    all_false_implies_clause_is_false.reserve(clause.size() + 1);
    all_false_implies_clause_is_false.push_back(-clause_is_true);
    for(size_t lit_cnt = 0; lit_cnt < clause.size(); ++lit_cnt)
    {
      // if one of the literals is true, then clause_is_true is true
      neg_gen_cnf.add2LitClause(-clause[lit_cnt], clause_is_true);
      all_false_implies_clause_is_false.push_back(clause[lit_cnt]);
    }
    neg_gen_cnf.addClause(all_false_implies_clause_is_false);

    // clause_is_true is false implies other_clauses_lit is false
    neg_gen_cnf.add2LitClause(clause_is_true, -other_clauses_lit);
    all_true_implies_other_clauses_lit_is_true.push_back(-clause_is_true);
  }
  neg_gen_cnf.addClause(all_true_implies_other_clauses_lit_is_true);

  neg_gen_cnf.add2LitClause(-AIG2CNF::instance().getT(), -other_clauses_lit);
  ostringstream filename;
  filename << filename_prefix_gen_ << "_gen_" << it_count_ << "_" << id  << "_neg.qdimacs";
  ExtQBFSolver::dumpQBF(neg_gen_quant, neg_gen_cnf, filename.str());
  VarManager::pop();
}
