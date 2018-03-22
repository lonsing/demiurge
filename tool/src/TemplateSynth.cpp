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
#include "SatSolver.h"
#include "CNFImplExtractor.h"
#include "Options.h"


// -------------------------------------------------------------------------------------------
TemplateSynth::TemplateSynth(CNFImplExtractor *impl_extractor) :
               BackEnd(),
               qbf_solver_(Options::instance().getQBFSolver()),
               sat_solver_(Options::instance().getSATSolver(false, false)),
               impl_extractor_(impl_extractor)
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
TemplateSynth::~TemplateSynth()
{
  delete qbf_solver_;
  qbf_solver_ = NULL;

  delete sat_solver_;
  sat_solver_ = NULL;

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
  VarManager::instance().push();
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
  impl_extractor_->extractCircuit(winning_region_);
  L_INF("Synthesis done.");
  impl_extractor_->logStatistics();
  return true;
}

// -------------------------------------------------------------------------------------------
bool TemplateSynth::computeWinningRegion()
{
  if(Options::instance().getBackEndMode() == 0 || Options::instance().getBackEndMode() == 1)
    return computeWinningRegionCNF();
  else
    return computeWinningRegionAndNet();
}

// -------------------------------------------------------------------------------------------
bool TemplateSynth::computeWinningRegionCNF()
{
  size_t nr_of_clauses = 1;
  size_t nr_of_bits = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE).size() - 1;
  // (-1 because we do not use the error-bit in our state-space)
  size_t max_nr_clauses = numeric_limits<size_t>::max();
  if(nr_of_bits < numeric_limits<size_t>::digits - 1)
    max_nr_clauses = (1UL << nr_of_bits);
  while(true)
  {
    VarManager::resetToLastPush();
    L_INF("Trying CNF template with " << nr_of_clauses << " clauses ...");
    bool found = findWinRegCNFTempl(nr_of_clauses);
    if(found)
    {
      L_LOG("Found winning region with " << nr_of_clauses << " clauses.");
      return true;
    }
    L_LOG("No winning region with " << nr_of_clauses << " clauses.");
    if(nr_of_clauses >= max_nr_clauses)
      return false;
    if(nr_of_clauses < 4)
      nr_of_clauses++;
    else
      nr_of_clauses <<= 1;
  }
  return false;
}

// -------------------------------------------------------------------------------------------
bool TemplateSynth::computeWinningRegionAndNet()
{
  size_t nr_of_gates = 1;
  size_t nr_of_bits = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE).size() - 1;
  // (-1 because we do not use the error-bit in our state-space)
  size_t max_nr_gates = numeric_limits<size_t>::max();
  if(nr_of_bits < numeric_limits<size_t>::digits - 1)
    max_nr_gates = (1UL << nr_of_bits);
  while(true)
  {
    VarManager::resetToLastPush();
    L_INF("Trying AND-template with " << nr_of_gates << " gates ...");
    bool found = findWinRegANDNetwork(nr_of_gates);
    if(found)
    {
      L_LOG("Found winning region with " << nr_of_gates << " gates.");
      return true;
    }
    L_LOG("No winning region with " << nr_of_gates << " gates.");
    if(nr_of_gates >= max_nr_gates)
      return false;
    if(nr_of_gates < 4)
      nr_of_gates++;
    else
      nr_of_gates <<= 1;
  }
  return false;
}

// -------------------------------------------------------------------------------------------
bool TemplateSynth::findWinRegCNFTempl(size_t nr_of_clauses)
{
  // Note: a lot of loops and code could be merged, but readability
  // beats performance in this case.

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
  CNF win_constr;
  int w1 = VM.createFreshTmpVar("w1");
  int w2 = VM.createFreshTmpVar("w2");

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
      win_constr.add2LitClause(contains_vars[c][v], -activated_var1);
      win_constr.add2LitClause(contains_vars[c][v], -activated_var2);
      // Part A2: (contains_var[c][v] and negated_var[c][v]) implies (activated_var <=> not(var[v]))
      win_constr.add4LitClause(-contains_vars[c][v], -negated_vars[c][v], ps_vars[v], activated_var1);
      win_constr.add4LitClause(-contains_vars[c][v], -negated_vars[c][v], -ps_vars[v], -activated_var1);
      win_constr.add4LitClause(-contains_vars[c][v], -negated_vars[c][v], ns_vars[v], activated_var2);
      win_constr.add4LitClause(-contains_vars[c][v], -negated_vars[c][v], -ns_vars[v], -activated_var2);
      // Part A3: (contains_var[c][v] and not(negated_var[c][v])) implies (activated_var <=> var[v])
      win_constr.add4LitClause(-contains_vars[c][v], negated_vars[c][v], -ps_vars[v], activated_var1);
      win_constr.add4LitClause(-contains_vars[c][v], negated_vars[c][v], ps_vars[v], -activated_var1);
      win_constr.add4LitClause(-contains_vars[c][v], negated_vars[c][v], -ns_vars[v], activated_var2);
      win_constr.add4LitClause(-contains_vars[c][v], negated_vars[c][v], ns_vars[v], -activated_var2);
    }
    // Part B: clause_lit should be equal to OR(literals_in_clause):
    // Part B1: if one literal in literals_in_clause is TRUE then clause_lit should be TRUE
    for(size_t cnt = 0; cnt < literals_in_clause1.size(); ++cnt)
    {
      win_constr.add2LitClause(-literals_in_clause1[cnt], clause_lit1);
      win_constr.add2LitClause(-literals_in_clause2[cnt], clause_lit2);
    }
    // Part B2: if all literals in literals_in_clause are FALSE, then the clause_lit should be FALSE
    // That is, we add the clause [l1, l2, l3, l4, -clause_lit]
    literals_in_clause1.push_back(-clause_lit1);
    win_constr.addClause(literals_in_clause1);
    literals_in_clause2.push_back(-clause_lit2);
    win_constr.addClause(literals_in_clause2);
  }
  // Part C: w should be equal to AND(all_clause_lits)
  // Part C1: if one literal in all_clause_lits is FALSE then w should be FALSE:
  for(size_t cnt = 0; cnt < all_clause_lits1.size(); ++cnt)
  {
    win_constr.add2LitClause(all_clause_lits1[cnt], -w1);
    win_constr.add2LitClause(all_clause_lits2[cnt], -w2);
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
  win_constr.addClause(all_clause_lits1);
  win_constr.addClause(all_clause_lits2);


  // Step 2: solve
  vector<int> model;
  bool sat = false;
  if(Options::instance().getBackEndMode() == 0)
    sat = syntQBF(win_constr, w1, w2, model);
  else
    sat = syntSAT(win_constr, w1, w2, model);
  if(!sat)
    return false;


  // Step 3: Build a CNF for the winning region:
  winning_region_.add1LitClause(-VM.getPresErrorStateVar());
  vector<bool> can_be_0(VM.getMaxCNFVar() + 1, true);
  vector<bool> can_be_1(VM.getMaxCNFVar() + 1, true);
  VM.resetToLastPush();
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
  neg_winning_region_ = winning_region_;
  neg_winning_region_.negate();
  return true;
}


// -------------------------------------------------------------------------------------------
bool TemplateSynth::findWinRegANDNetwork(size_t nr_of_gates)
{
  // Note: a lot of loops and code could be merged, but readability
  // beats performance in this case.

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
  CNF win_constr;
  int w2 = VM.createFreshTmpVar("w2");

  // Step 1a: create template parameters:
  vector<vector<int> > used;
  vector<vector<int> > neg;
  used.reserve(nr_of_gates);
  neg.reserve(nr_of_gates);
  for(size_t g_cnt = 0; g_cnt < nr_of_gates; ++g_cnt)
  {
    used.push_back(vector<int>());
    neg.push_back(vector<int>());
    for(size_t v_cnt = 0; v_cnt < nr_of_vars + g_cnt; ++v_cnt)
    {
      ostringstream var_idx;
      var_idx << "[" << g_cnt << "][" << v_cnt << "]";
      used[g_cnt].push_back(VM.createFreshTemplParam("cont" + var_idx.str()));
      neg[g_cnt].push_back(VM.createFreshTemplParam("neg" + var_idx.str()));
    }
  }

  // Step 1b: construct gate outputs and list of gate inputs:
  vector<int> gop; // gop[i] = output of gate i (present state)
  gop.reserve(nr_of_gates);
  vector<int> gon; // gon[i] = output of gate i (next state)
  gon.reserve(nr_of_gates);
  vector<vector<int> > gip; // gip[i] = inputs of gate i (present state)
  gip.reserve(nr_of_gates);
  vector<vector<int> > gin; // gin[i] = inputs of gate i (next state)
  gin.reserve(nr_of_gates);
  for(size_t g_cnt = 0; g_cnt < nr_of_gates; ++g_cnt)
  {
    gop.push_back(VM.createFreshTmpVar());
    gon.push_back(VM.createFreshTmpVar());
    gip.push_back(ps_vars);
    gin.push_back(ns_vars);
    // gate i can also have the output of gate j as input if j < i:
    for(size_t g_cnt2 = 0; g_cnt2 < g_cnt; ++g_cnt2)
    {
      gip[g_cnt].push_back(gop[g_cnt2]);
      gin[g_cnt].push_back(gon[g_cnt2]);
    }
  }

  // Step 1c: constructing the constraints:
  // We do this for w1 and w2 in parallel
  for(size_t g = 0; g < nr_of_gates; ++g)
  {
    vector<int> act_in_varsP;
    vector<int> act_in_varsN;
    act_in_varsP.reserve(gip[g].size() + 1);
    act_in_varsN.reserve(gip[g].size() + 1);

    // construct the constraints defining the activated inputs of the gate:
    for(size_t v = 0; v < gip[g].size(); ++v)
    {
      int activated_varP = VM.createFreshTmpVar();
      act_in_varsP.push_back(activated_varP);
      int activated_varN = VM.createFreshTmpVar();
      act_in_varsN.push_back(activated_varN);
      // Part A: activated_var should be equal to
      // - TRUE         if (used[g][v]==FALSE)
      // - not(var[v])  if (used[g][v]==TRUE and neg[g][v]==TRUE)
      // - var[v]       if (used[g][v]==TRUE and neg[g][v]==FALSE)
      // Part A1: not(used[g][v]) implies activated_var
      win_constr.add2LitClause(used[g][v], activated_varP);
      win_constr.add2LitClause(used[g][v], activated_varN);
      // Part A2: (used[g][v] and neg[g][v]) implies (activated_var <=> not(var[v]))
      win_constr.add4LitClause(-used[g][v], -neg[g][v], gip[g][v], activated_varP);
      win_constr.add4LitClause(-used[g][v], -neg[g][v], -gip[g][v], -activated_varP);
      win_constr.add4LitClause(-used[g][v], -neg[g][v], gin[g][v], activated_varN);
      win_constr.add4LitClause(-used[g][v], -neg[g][v], -gin[g][v], -activated_varN);
      // Part A3: (used[g][v] and not(neg[g][v])) implies (activated_var <=> var[v])
      win_constr.add4LitClause(-used[g][v], neg[g][v], -gip[g][v], activated_varP);
      win_constr.add4LitClause(-used[g][v], neg[g][v], gip[g][v], -activated_varP);
      win_constr.add4LitClause(-used[g][v], neg[g][v], -gin[g][v], activated_varN);
      win_constr.add4LitClause(-used[g][v], neg[g][v], gin[g][v], -activated_varN);
    }

    // Part B: gop[g] should be equal to AND(act_in_vars):
    // Part B1: if one literal in act_in_vars is FALSE then clause_lit should be FALSE
    for(size_t cnt = 0; cnt < act_in_varsP.size(); ++cnt)
    {
      win_constr.add2LitClause(act_in_varsP[cnt], -gop[g]);
      win_constr.add2LitClause(act_in_varsN[cnt], -gon[g]);
    }
    // Part B2: if all literals in act_in_vars are TRUE, then gop[g] should be TRUE
    // That is, we add the clause [-l1, -l2, -l3, -l4, ..., gop[g]]
    Utils::negateLiterals(act_in_varsP);
    act_in_varsP.push_back(gop[g]);
    win_constr.addClause(act_in_varsP);
    Utils::negateLiterals(act_in_varsN);
    act_in_varsN.push_back(gon[g]);
    win_constr.addClause(act_in_varsN);
  }

  // Part C: the final output is the output of the last AND gate, possibly negated, and then
  // conjuncted with the safe states:
  int out_neg = VM.createFreshTemplParam();
  int ps_safe = -VM.getPresErrorStateVar();
  int ns_safe = -VM.getNextErrorStateVar();
  win_constr.add2LitClause(ps_safe, -w1);
  win_constr.add4LitClause(-ps_safe, out_neg, gop[nr_of_gates-1], -w1);
  win_constr.add4LitClause(-ps_safe, out_neg, -gop[nr_of_gates-1], w1);
  win_constr.add4LitClause(-ps_safe, -out_neg, gop[nr_of_gates-1], w1);
  win_constr.add4LitClause(-ps_safe, -out_neg, -gop[nr_of_gates-1], -w1);
  win_constr.add2LitClause(ns_safe, -w2);
  win_constr.add4LitClause(-ns_safe, out_neg, gon[nr_of_gates-1], -w2);
  win_constr.add4LitClause(-ns_safe, out_neg, -gon[nr_of_gates-1], w2);
  win_constr.add4LitClause(-ns_safe, -out_neg, gon[nr_of_gates-1], w2);
  win_constr.add4LitClause(-ns_safe, -out_neg, -gon[nr_of_gates-1], -w2);

  // Step 2: solve
  vector<int> model;

  bool sat = false;
  if(Options::instance().getBackEndMode() == 2)
    sat = syntQBF(win_constr, w1, w2, model);
  else
    sat = syntSAT(win_constr, w1, w2, model);
  if(!sat)
    return false;

  // Step 3: parse the solution:
  vector<int> params = VM.getVarsOfType(VarInfo::TEMPL_PARAMS);
  int max_param_idx = 0;
  for(size_t cnt = 0; cnt < params.size(); ++cnt)
    if(params[cnt] > max_param_idx)
      max_param_idx = params[cnt];

  // 3a: reset the manager so that we get rid of all parameters and auxiliary vars that are
  // not needed any more.
  // We re-do the aux-vars that are the output of the and gates in a persistent way:
  VM.resetToLastPush();
  gop.clear(); // gop[i] = output of gate i (present state)
  gop.reserve(nr_of_gates);
  gip.clear(); // gip[i] = inputs of gate i (present state)
  gip.reserve(nr_of_gates);
  vector<int> gop2; // copy for the negation of the winning region
  gop2.reserve(nr_of_gates);
  vector<vector<int> > gip2; // copy for the negation of the winning region
  gip2.reserve(nr_of_gates);
  for(size_t g_cnt = 0; g_cnt < nr_of_gates; ++g_cnt)
  {
    gop.push_back(VM.createFreshTmpVar());
    gop2.push_back(VM.createFreshTmpVar());
    gip.push_back(ps_vars);
    gip2.push_back(ps_vars);
    // gate i can also have the output of gate j as input if j < i:
    for(size_t g_cnt2 = 0; g_cnt2 < g_cnt; ++g_cnt2)
    {
      gip[g_cnt].push_back(gop[g_cnt2]);
      gip2[g_cnt].push_back(gop2[g_cnt2]);
    }
  }


  // 3b: parse the model:
  vector<bool> can_be_0(max_param_idx + 1, true);
  vector<bool> can_be_1(max_param_idx + 1, true);
  for(size_t v_cnt = 0; v_cnt < model.size(); ++v_cnt)
  {
    if(model[v_cnt] < 0)
      can_be_1[-model[v_cnt]] = false;
    else
      can_be_0[model[v_cnt]] = false;
  }

  // 3c: construct the winning region formula:
  for(size_t g = 0; g < nr_of_gates; ++g)
  {
    vector<int> and_over;  // for winning_region_
    vector<int> and_over2; // for neg_winning_region_
    and_over.reserve(gip[g].size() + 1);
    and_over2.reserve(gip[g].size() + 1);
    for(size_t v = 0; v < gip[g].size(); ++v)
    {
      if(can_be_0[used[g][v]])
        continue;

      if(can_be_0[neg[g][v]])
      {
        // var is unnegated:
        winning_region_.add2LitClause(gip[g][v], -gop[g]);
        neg_winning_region_.add2LitClause(gip2[g][v], -gop2[g]);
        and_over.push_back(gip[g][v]);
        and_over2.push_back(gip2[g][v]);
      }
      else
      {
        // var is negated:
        winning_region_.add2LitClause(-gip[g][v], -gop[g]);
        neg_winning_region_.add2LitClause(-gip2[g][v], -gop2[g]);
        and_over.push_back(-gip[g][v]);
        and_over2.push_back(gip2[g][v]);
      }
    }
    // if all literals in and_over are TRUE, then gop[g] should be TRUE
    // That is, we add the clause [-l1, -l2, -l3, -l4, ..., gop[g]]
    Utils::negateLiterals(and_over);
    and_over.push_back(gop[g]);
    winning_region_.addClause(and_over);
    Utils::negateLiterals(and_over2);
    and_over2.push_back(gop2[g]);
    neg_winning_region_.addClause(and_over2);
  }
  winning_region_.add1LitClause(-VM.getPresErrorStateVar());
  if(can_be_0[out_neg])
  {
    winning_region_.add1LitClause(gop[nr_of_gates - 1]);
    neg_winning_region_.add2LitClause(VM.getPresErrorStateVar(), -gop2[nr_of_gates - 1]);
  }
  else
  {
    winning_region_.add1LitClause(-gop[nr_of_gates - 1]);
    neg_winning_region_.add2LitClause(VM.getPresErrorStateVar(), gop2[nr_of_gates - 1]);
  }
  return true;
}

// -------------------------------------------------------------------------------------------
bool TemplateSynth::syntQBF(const CNF& win_constr, int w1, int w2, vector<int> &solution)
{
  VarManager &VM = VarManager::instance();
  AIG2CNF& A2C = AIG2CNF::instance();
  CNF query(A2C.getTransEqT());
  query.addCNF(win_constr);
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

  return qbf_solver_->isSatModel(quant, query, solution);

}


// -------------------------------------------------------------------------------------------
bool TemplateSynth::syntSAT(const CNF& win_constr, int w1, int w2, vector<int> &solution)
{
  VarManager &VM = VarManager::instance();
  AIG2CNF& A2C = AIG2CNF::instance();
  CNF gen(A2C.getTrans());
  gen.addCNF(win_constr);
  vector<int> init_implies_win;
  const vector<int> &all_ps_vars = VM.getVarsOfType(VarInfo::PRES_STATE);
  init_implies_win.reserve(all_ps_vars.size() + 1);
  for(size_t cnt = 0; cnt < all_ps_vars.size(); ++cnt)
    init_implies_win.push_back(all_ps_vars[cnt]);
  init_implies_win.push_back(w1);
  gen.addClause(init_implies_win);
  // from win, the system can enforce to stay in win
  gen.add2LitClause(-w1, w2);


  vector<int> none;
  const vector<int> &params = VM.getVarsOfType(VarInfo::TEMPL_PARAMS);
  sat_solver_->startIncrementalSession(params, false);

  // let's fix correctness for the initial state right away:
  vector<int> initial_state;
  const vector<int> &in = VM.getVarsOfType(VarInfo::INPUT);
  const vector<int> &pres = VM.getVarsOfType(VarInfo::PRES_STATE);
  initial_state.reserve(pres.size() + in.size());
  initial_state.insert(initial_state.end(), pres.begin(), pres.end());
  initial_state.insert(initial_state.end(), in.begin(), in.end());
  Utils::negateLiterals(initial_state);
  exclude(initial_state, gen);


  size_t cnt = 0;
  while(true)
  {
    vector<int> candidate;
    ++cnt;
    bool sat = sat_solver_->incIsSatModelOrCore(none, params, candidate);
    if(!sat)
      return false;
    vector<int> counterexample;
    bool correct = check(candidate, win_constr, w1, w2, counterexample);
    if(correct)
    {
      solution = candidate;
      return true;
    }
    exclude(counterexample, gen);
  }
}

// -------------------------------------------------------------------------------------------
bool TemplateSynth::check(const vector<int> &cand, const CNF& win_constr, int w1, int w2,
                          vector<int> &ce)
{
  VarManager &VM = VarManager::instance();
  AIG2CNF& A2C = AIG2CNF::instance();
  const vector<int> &in = VM.getVarsOfType(VarInfo::INPUT);
  const vector<int> &pres = VM.getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &ctrl = VM.getVarsOfType(VarInfo::CTRL);
  vector<int> sic;
  sic.reserve(pres.size() + in.size() + ctrl.size());
  sic.insert(sic.end(), pres.begin(), pres.end());
  sic.insert(sic.end(), in.begin(), in.end());
  sic.insert(sic.end(), ctrl.begin(), ctrl.end());
  vector<int> si;
  si.reserve(pres.size() + in.size());
  si.insert(si.end(), pres.begin(), pres.end());
  si.insert(si.end(), in.begin(), in.end());

  CNF check_cnf(win_constr);
  for(size_t cnt = 0; cnt < cand.size(); ++cnt)
    check_cnf.setVarValue(cand[cnt], true);
  check_cnf.addCNF(A2C.getTrans());
  check_cnf.add1LitClause(w1);
  CNF gen_cnf(check_cnf);
  gen_cnf.add1LitClause(w2);
  check_cnf.add1LitClause(-w2);


  SatSolver *check_solver = Options::instance().getSATSolver(false, true);
  check_solver->startIncrementalSession(sic, false);
  check_solver->incAddCNF(check_cnf);
  SatSolver *gen_solver = Options::instance().getSATSolver(false, true);
  gen_solver->startIncrementalSession(sic, false);
  gen_solver->incAddCNF(gen_cnf);

  vector<int> model_or_core;
  while(true)
  {
    bool sat = check_solver->incIsSatModelOrCore(vector<int>(), si, model_or_core);
    if(!sat)
    {
      delete check_solver;
      delete gen_solver;
      return true;
    }
    vector<int> state_input = model_or_core;
    sat = gen_solver->incIsSatModelOrCore(vector<int>(), state_input, ctrl, model_or_core);
    if(!sat)
    {
      // randomization does not help:
      //for(size_t cnt = 0; cnt < state_input.size(); ++cnt)
      //{
      //  if(rand() & 1)
      //  {
      //    vector<int> ran(state_input);
      //    ran[cnt] = -ran[cnt];
      //    if(!gen_solver->incIsSatModelOrCore(vector<int>(), ran, vector<int>(), model_or_core))
      //      state_input = ran;
      //  }
      //}

      delete check_solver;
      delete gen_solver;
      ce = state_input;
      return false;
    }

    vector<int> resp(model_or_core);
    sat = check_solver->incIsSatModelOrCore(state_input, resp, vector<int>(), model_or_core);
    MASSERT(sat == false, "Impossible.");
    check_solver->incAddNegCubeAsClause(model_or_core);
  }
}

// -------------------------------------------------------------------------------------------
void TemplateSynth::exclude(const vector<int> &ce, const CNF &gen)
{
  CNF to_add(gen);
  for(size_t cnt = 0; cnt < ce.size(); ++cnt)
    to_add.setVarValue(ce[cnt], true);

  // now we need to rename everything except for the template parameters:
  set<int> var_set;
  to_add.appendVarsTo(var_set);
  int max_idx = 0;
  if(!var_set.empty())
    max_idx = *var_set.rbegin();
  vector<int> rename_map;
  rename_map.reserve(max_idx + 1);
  for(int cnt = 0; cnt < max_idx+1; ++cnt)
    rename_map.push_back(cnt);
  for(set<int>::const_iterator it = var_set.begin(); it != var_set.end(); ++it)
  {
    if(VarManager::instance().getInfo(*it).getKind() != VarInfo::TEMPL_PARAMS)
      rename_map[*it] = VarManager::instance().createFreshTmpVar();
  }
  to_add.renameVars(rename_map);
  sat_solver_->incAddCNF(to_add);
}
