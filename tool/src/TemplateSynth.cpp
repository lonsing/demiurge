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
/// @file TemplateSynth.cpp
/// @brief Contains the definition of the class TemplateSynth.
// -------------------------------------------------------------------------------------------

#include "TemplateSynth.h"
#include "defines.h"
#include "AIG2CNF.h"
#include "VarManager.h"
#include "CNF.h"
#include "Logger.h"
#include "Utils.h"
#include "QBFSolver.h"
#include "CNFImplExtractor.h"
#include "Options.h"

// -------------------------------------------------------------------------------------------
TemplateSynth::TemplateSynth(CNFImplExtractor *impl_extractor) :
               BackEnd(),
               qbf_solver_(Options::instance().getQBFSolver()),
               impl_extractor_(impl_extractor)
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
TemplateSynth::~TemplateSynth()
{
  delete qbf_solver_;
  qbf_solver_ = NULL;

  delete impl_extractor_;
  impl_extractor_ = NULL;
}

// -------------------------------------------------------------------------------------------
bool TemplateSynth::run()
{
  //L_DBG(VarManager::instance().toString());
  //L_DBG("TRANS:");
  //L_DBG(AIG2CNF::instance().getTrans().toString());
  L_INF("Starting to compute a winning region ...");
  bool realizable = computeWinningRegion();
  if(!realizable)
  {
    L_RES("The specification is unrealizable.");
    return false;
  }
  L_RES("The specification is realizable.");
  Utils::debugCheckWinReg(winning_region_);
  if(Options::instance().doRealizabilityOnly())
     return true;

  L_INF("Starting to extract a circuit ...");
  impl_extractor_->extractCircuit(winning_region_);
  L_INF("Synthesis done.");
  impl_extractor_->logStatistics();
  return true;
}

// -------------------------------------------------------------------------------------------
bool TemplateSynth::computeWinningRegion()
{
  size_t nr_of_clauses = 1;
  size_t nr_of_bits = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE).size() - 1;
  // (-1 because we do not use the error-bit in our state-space)
  size_t max_nr_clauses = numeric_limits<size_t>::max();
  if(nr_of_bits < numeric_limits<size_t>::digits - 1)
    max_nr_clauses = (1UL << nr_of_bits);
  while(true)
  {
    VarManager::push();
    L_INF("Trying CNF template with " << nr_of_clauses << " clauses ...");
    bool found = findWinRegCNFTempl(nr_of_clauses);
    VarManager::pop(); // to eliminate all the temporary variables again.
    if(found)
    {
      L_LOG("Found winning region with " << nr_of_clauses << " clauses.");
      return true;
    }
    L_LOG("No winning region with " << nr_of_clauses << " clauses.");
    if(nr_of_clauses >= max_nr_clauses)
      return false;
    if(nr_of_clauses == 1)
      nr_of_clauses = 4;
    else if(nr_of_clauses == 4)
      nr_of_clauses = 16;
    else
      nr_of_clauses <<= 1;
  }
  return false;
}

// -------------------------------------------------------------------------------------------
bool TemplateSynth::findWinRegCNFTempl(size_t nr_of_clauses)
{
  // Note: a lot of loops and code could be merged, but readability
  // beats performance in this case.

  AIG2CNF& A2C = AIG2CNF::instance();
  VarManager &VM = VarManager::instance();
  vector<int> ps_vars = VM.getVarsOfType(VarInfo::PRES_STATE);
  vector<int> ns_vars = VM.getVarsOfType(VarInfo::NEXT_STATE);

  // ps_vars[0] and ns_vars[0] are the error bits, which must be FALSE always.
  // Hence, we do not include these signals in our template
  ps_vars[0] = ps_vars[ps_vars.size() - 1];
  ps_vars.pop_back();
  ns_vars[0] = ns_vars[ns_vars.size() - 1];
  ns_vars.pop_back();
  size_t nr_of_vars = ps_vars.size();

  // Step 1: build the parameterized CNF:
  int w1 = VM.createFreshTmpVar("w1");
  CNF win_eq_w1;
  int w2 = VM.createFreshTmpVar("w2");
  CNF next_win_eq_w2;

  // Step 1a: create template parameters:
  vector<int> clause_active_vars;
  vector<vector<int> > contains_vars;
  vector<vector<int> > negated_vars;
  clause_active_vars.reserve(nr_of_clauses);
  contains_vars.reserve(nr_of_clauses);
  negated_vars.reserve(nr_of_clauses);
  for(size_t c_cnt = 0; c_cnt < nr_of_clauses; ++c_cnt)
  {
    ostringstream act_name;
    act_name << "act[" << c_cnt << "]";
    clause_active_vars.push_back(VM.createFreshTemplParam(act_name.str()));
    contains_vars.push_back(vector<int>());
    negated_vars.push_back(vector<int>());
    for(size_t v_cnt = 0; v_cnt < nr_of_vars; ++v_cnt)
    {
      ostringstream var_idx;
      var_idx << "[" << c_cnt << "][" << v_cnt << "]";
      contains_vars[c_cnt].push_back(VM.createFreshTemplParam("cont" + var_idx.str()));
      negated_vars[c_cnt].push_back(VM.createFreshTemplParam("cont" + var_idx.str()));
    }
  }

  // Step 1b: constructing the constraints:
  // We do this for next_win_eq_w2 and win_eq_w1 in parallel
  vector<int> all_clause_lits1;
  vector<int> all_clause_lits2;
  all_clause_lits1.reserve(nr_of_clauses + 1);
  all_clause_lits2.reserve(nr_of_clauses + 1);
  // We add one fixed clause, namely that we are inside the safe states:
  all_clause_lits1.push_back(-VM.getPresErrorStateVar());
  all_clause_lits2.push_back(-VM.getNextErrorStateVar());
  for(size_t c = 0; c < nr_of_clauses; ++c)
  {
    int clause_lit1 = VM.createFreshTmpVar();  // TRUE iff clause c is TRUE
    int clause_lit2 = VM.createFreshTmpVar();  // TRUE iff clause c is TRUE
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
      int activated_var1 = VM.createFreshTmpVar();
      int activated_var2 = VM.createFreshTmpVar();
      literals_in_clause1.push_back(activated_var1);
      literals_in_clause2.push_back(activated_var2);
      // Part A: activated_var should be equal to
      // - FALSE        if (contains_var[c][v]==FALSE)
      // - not(var[v])  if (contains_var[c][v]==TRUE and negated_var[c][v]==TRUE)
      // - var[v]       if (contains_var[c][v]==TRUE and negated_var[c][v]==FALSE)
      // Part A1: not(contains_vars[c][v]) implies not(activated_var)
      win_eq_w1.add2LitClause(contains_vars[c][v], -activated_var1);
      next_win_eq_w2.add2LitClause(contains_vars[c][v], -activated_var2);
      // Part A2: (contains_var[c][v] and negated_var[c][v]) implies (activated_var <=> not(var[v]))
      win_eq_w1.add4LitClause(-contains_vars[c][v], -negated_vars[c][v], ps_vars[v], activated_var1);
      win_eq_w1.add4LitClause(-contains_vars[c][v], -negated_vars[c][v], -ps_vars[v], -activated_var1);
      next_win_eq_w2.add4LitClause(-contains_vars[c][v], -negated_vars[c][v], ns_vars[v], activated_var2);
      next_win_eq_w2.add4LitClause(-contains_vars[c][v], -negated_vars[c][v], -ns_vars[v], -activated_var2);
      // Part A3: (contains_var[c][v] and not(negated_var[c][v])) implies (activated_var <=> var[v])
      win_eq_w1.add4LitClause(-contains_vars[c][v], negated_vars[c][v], -ps_vars[v], activated_var1);
      win_eq_w1.add4LitClause(-contains_vars[c][v], negated_vars[c][v], ps_vars[v], -activated_var1);
      next_win_eq_w2.add4LitClause(-contains_vars[c][v], negated_vars[c][v], -ns_vars[v], activated_var2);
      next_win_eq_w2.add4LitClause(-contains_vars[c][v], negated_vars[c][v], ns_vars[v], -activated_var2);
    }
    // Part B: clause_lit should be equal to OR(literals_in_clause):
    // Part B1: if one literal in literals_in_clause is TRUE then clause_lit should be TRUE
    for(size_t cnt = 0; cnt < literals_in_clause1.size(); ++cnt)
    {
      win_eq_w1.add2LitClause(-literals_in_clause1[cnt], clause_lit1);
      next_win_eq_w2.add2LitClause(-literals_in_clause2[cnt], clause_lit2);
    }
    // Part B2: if all literals in literals_in_clause are FALSE, then the clause_lit should be FALSE
    // That is, we add the clause [l1, l2, l3, l4, -clause_lit]
    literals_in_clause1.push_back(-clause_lit1);
    win_eq_w1.addClause(literals_in_clause1);
    literals_in_clause2.push_back(-clause_lit2);
    next_win_eq_w2.addClause(literals_in_clause2);
  }
  // Part C: w should be equal to AND(all_clause_lits)
  // Part C1: if one literal in all_clause_lits is FALSE then w should be FALSE:
  for(size_t cnt = 0; cnt < all_clause_lits1.size(); ++cnt)
  {
    win_eq_w1.add2LitClause(all_clause_lits1[cnt], -w1);
    next_win_eq_w2.add2LitClause(all_clause_lits2[cnt], -w2);
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
  win_eq_w1.addClause(all_clause_lits1);
  next_win_eq_w2.addClause(all_clause_lits2);


  //Step 2: Solve:
  CNF query(A2C.getTransEqT());
  query.addCNF(win_eq_w1);
  query.addCNF(next_win_eq_w2);
  vector<int> init_implies_win;
  const vector<int> &all_ps_vars = VM.getVarsOfType(VarInfo::PRES_STATE);
  init_implies_win.reserve(all_ps_vars.size() + 1);
  for(size_t cnt = 0; cnt < all_ps_vars.size(); ++cnt)
    init_implies_win.push_back(all_ps_vars[cnt]);
  init_implies_win.push_back(w1);
  query.addClause(init_implies_win);
  // from win, the system can enforce to stay in win
  query.add2LitClause(-w1, A2C.getT());
  query.add2LitClause(-w1, w2);

  // build the quantifier prefix:
  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > quant;
  quant.push_back(make_pair(VarInfo::TEMPL_PARAMS, QBFSolver::E));
  quant.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::A));
  quant.push_back(make_pair(VarInfo::INPUT, QBFSolver::A));
  quant.push_back(make_pair(VarInfo::CTRL, QBFSolver::E));
  quant.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::E));
  quant.push_back(make_pair(VarInfo::TMP, QBFSolver::E));

  vector<int> model;
  bool sat = qbf_solver_->isSatModel(quant, query, model);
  if(!sat)
    return false;

  // Step 3: Build a CNF for the winning region:
  winning_region_.add1LitClause(-VM.getPresErrorStateVar());
  vector<bool> can_be_0(VM.getMaxCNFVar() + 1, true);
  vector<bool> can_be_1(VM.getMaxCNFVar() + 1, true);
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
    winning_region_.addClause(clause);
  }
  return true;
}


