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
/// @file DepQBFApi.cpp
/// @brief Contains the definition of the class DepQBFApi.
// -------------------------------------------------------------------------------------------

#include "DepQBFApi.h"
#include "VarManager.h"
#include "CNF.h"
#include "Utils.h"
#include "DepQBFExt.h"
#include "Logger.h"
extern "C" {
  #include "qdpll.h"
  #include "bloqqer.h"
}



// -------------------------------------------------------------------------------------------
DepQBFApi::DepQBFApi(bool use_bloqqer) : use_bloqqer_(use_bloqqer)
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
DepQBFApi::~DepQBFApi()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
bool DepQBFApi::isSat(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                      const CNF& cnf)
{
  if(use_bloqqer_)
  {
    initBloqqer(quantifier_prefix, cnf);
    bloqqer_preprocess();
    int res = bloqqer_solve();
    //debugCheckBloqqerVerdict(res, quantifier_prefix, cnf);
    bloqqer_release();
    if(res == 10)
      return true;
    else if(res == 20)
      return false;
    MASSERT(false, "Strange return code from Bloqqer/DepQBF.");
  }
  else
  {
    QDPLL *solver = qdpll_create();
    initDepQBF(solver, quantifier_prefix, cnf);
    QDPLLResult res = qdpll_sat(solver);
    qdpll_delete(solver);
    if(res == QDPLL_RESULT_SAT)
      return true;
    if(res == QDPLL_RESULT_UNSAT)
      return false;
    MASSERT(false, "Strange return code from DepQBF.");
  }
  return false;
}

// -------------------------------------------------------------------------------------------
bool DepQBFApi::isSat(const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                      const CNF& cnf)
{
  if(use_bloqqer_)
  {
    initBloqqer(quantifier_prefix, cnf);
    bloqqer_preprocess();
    int res = bloqqer_solve();
    //debugCheckBloqqerVerdict(res, quantifier_prefix, cnf);
    bloqqer_release();
    if(res == 10)
      return true;
    else if(res == 20)
      return false;
    MASSERT(false, "Strange return code from Bloqqer/DepQBF.");
  }
  else
  {
    QDPLL *solver = qdpll_create();
    initDepQBF(solver, quantifier_prefix, cnf);
    QDPLLResult res = qdpll_sat(solver);
    qdpll_delete(solver);
    if(res == QDPLL_RESULT_SAT)
      return true;
    if(res == QDPLL_RESULT_UNSAT)
      return false;
    MASSERT(false, "Strange return code from DepQBF.");
  }
  return false;
}

// -------------------------------------------------------------------------------------------
bool DepQBFApi::isSatModel(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                           const CNF& cnf,
                           vector<int> &model)
{
  if(use_bloqqer_)
  {
    initBloqqer(quantifier_prefix, cnf);
    bloqqer_preprocess();
    int res = bloqqer_solve();
    if(res == 20)
    {
      debugCheckBloqqerVerdict(res, quantifier_prefix, cnf);
      bloqqer_release();
      return false;
    }
    MASSERT(res == 10, "Strange return code from Bloqqer/DepQBF.");
    VarManager &VM = VarManager::instance();
    size_t model_size = 0;
    for(size_t q_cnt = 0; q_cnt < quantifier_prefix.size(); ++q_cnt)
    {
      model_size += VM.getVarsOfType(quantifier_prefix[q_cnt].first).size();
      if(q_cnt + 1 < quantifier_prefix.size())
        if(quantifier_prefix[q_cnt].second != quantifier_prefix[q_cnt+1].second)
          break;
    }
    model.clear();
    model.reserve(model_size);
    for(size_t q_cnt = 0; q_cnt < quantifier_prefix.size(); ++q_cnt)
    {
      const vector<int> &vars = VM.getVarsOfType(quantifier_prefix[q_cnt].first);
      for(size_t v_cnt = 0; v_cnt < vars.size(); ++v_cnt)
      {
        VarValue val = bloqqer_getvalue(vars[v_cnt]);
        MASSERT(val != UNKN, "Bloqqer return an UNKN as value");
        if(val == NEG)
          model.push_back(-vars[v_cnt]);
        else if(val == POS)
          model.push_back(vars[v_cnt]);
      }
      if(q_cnt + 1 < quantifier_prefix.size())
        if(quantifier_prefix[q_cnt].second != quantifier_prefix[q_cnt+1].second)
          break;
    }
    //debugCheckBloqqerModel(model, quantifier_prefix, cnf);
    bloqqer_release();
    return true;
  }
  else
  {
    QDPLL *solver = qdpll_create();
    initDepQBF(solver, quantifier_prefix, cnf);
    QDPLLResult res = qdpll_sat(solver);
    if(res == QDPLL_RESULT_UNSAT)
    {
      qdpll_delete(solver);
      return false;
    }
    MASSERT(res != QDPLL_RESULT_UNKNOWN, "Strange return code from DepQBF.");
    VarManager &VM = VarManager::instance();
    size_t model_size = 0;
    for(size_t q_cnt = 0; q_cnt < quantifier_prefix.size(); ++q_cnt)
    {
      model_size += VM.getVarsOfType(quantifier_prefix[q_cnt].first).size();
      if(q_cnt + 1 < quantifier_prefix.size())
        if(quantifier_prefix[q_cnt].second != quantifier_prefix[q_cnt+1].second)
          break;
    }
    model.clear();
    model.reserve(model_size);
    for(size_t q_cnt = 0; q_cnt < quantifier_prefix.size(); ++q_cnt)
    {
      const vector<int> &vars = VM.getVarsOfType(quantifier_prefix[q_cnt].first);
      for(size_t v_cnt = 0; v_cnt < vars.size(); ++v_cnt)
      {
        QDPLLAssignment val = qdpll_get_value(solver, vars[v_cnt]);
        if(val == QDPLL_ASSIGNMENT_FALSE)
          model.push_back(-vars[v_cnt]);
        else if(val == QDPLL_ASSIGNMENT_TRUE)
          model.push_back(vars[v_cnt]);
      }
      if(q_cnt + 1 < quantifier_prefix.size())
        if(quantifier_prefix[q_cnt].second != quantifier_prefix[q_cnt+1].second)
          break;
    }
    qdpll_delete(solver);
    return true;
  }
}

// -------------------------------------------------------------------------------------------
bool DepQBFApi::isSatModel(const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                           const CNF& cnf,
                           vector<int> &model)
{
  if(use_bloqqer_)
  {
    initBloqqer(quantifier_prefix, cnf);
    bloqqer_preprocess();
    int res = bloqqer_solve();
    if(res == 20)
    {
      debugCheckBloqqerVerdict(res, quantifier_prefix, cnf);
      bloqqer_release();
      return false;
    }
    MASSERT(res == 10, "Strange return code from Bloqqer/DepQBF.");
    size_t model_size = 0;
    for(size_t q_cnt = 0; q_cnt < quantifier_prefix.size(); ++q_cnt)
    {
      model_size += quantifier_prefix[q_cnt].first.size();
      if(q_cnt + 1 < quantifier_prefix.size())
        if(quantifier_prefix[q_cnt].second != quantifier_prefix[q_cnt+1].second)
          break;
    }
    model.clear();
    model.reserve(model_size);
    for(size_t q_cnt = 0; q_cnt < quantifier_prefix.size(); ++q_cnt)
    {
      const vector<int> &vars = quantifier_prefix[q_cnt].first;
      for(size_t v_cnt = 0; v_cnt < vars.size(); ++v_cnt)
      {
        VarValue val = bloqqer_getvalue(vars[v_cnt]);
        MASSERT(val != UNKN, "Bloqqer return an UNKN as value");
        if(val == NEG)
          model.push_back(-vars[v_cnt]);
        else if(val == POS)
          model.push_back(vars[v_cnt]);
      }
      if(q_cnt + 1 < quantifier_prefix.size())
        if(quantifier_prefix[q_cnt].second != quantifier_prefix[q_cnt+1].second)
          break;
    }
    //debugCheckBloqqerModel(model, quantifier_prefix, cnf);
    bloqqer_release();
    return true;
  }
  else
  {
    QDPLL *solver = qdpll_create();
    initDepQBF(solver, quantifier_prefix, cnf);
    // begin randomization
    // leads to 'better' counterexamples but is much much slower ...
    //  char *error = qdpll_configure(solver, const_cast<char*>("--dec-heur=rand"));
    //  MASSERT(error == NULL, "DepQBF configure error: " << string(error));
    //  ostringstream seed_str;
    //  seed_str << "--seed=" << next_seed_++;
    //  error = qdpll_configure(solver, const_cast<char*>(seed_str.str().c_str()));
    //  MASSERT(error == NULL, "DepQBF configure error: " << string(error));
    // end randomization
    QDPLLResult res = qdpll_sat(solver);
    if(res == QDPLL_RESULT_UNSAT)
    {
      qdpll_delete(solver);
      return false;
    }
    MASSERT(res != QDPLL_RESULT_UNKNOWN, "Strange return code from DepQBF.");
    size_t model_size = 0;
    for(size_t q_cnt = 0; q_cnt < quantifier_prefix.size(); ++q_cnt)
    {
      model_size += quantifier_prefix[q_cnt].first.size();
      if(q_cnt + 1 < quantifier_prefix.size())
        if(quantifier_prefix[q_cnt].second != quantifier_prefix[q_cnt+1].second)
          break;
    }
    model.clear();
    model.reserve(model_size);
    for(size_t q_cnt = 0; q_cnt < quantifier_prefix.size(); ++q_cnt)
    {
      const vector<int> &vars = quantifier_prefix[q_cnt].first;
      for(size_t v_cnt = 0; v_cnt < vars.size(); ++v_cnt)
      {
        QDPLLAssignment val = qdpll_get_value(solver, vars[v_cnt]);
        if(val == QDPLL_ASSIGNMENT_FALSE)
          model.push_back(-vars[v_cnt]);
        else if(val == QDPLL_ASSIGNMENT_TRUE)
          model.push_back(vars[v_cnt]);
      }
      if(q_cnt + 1 < quantifier_prefix.size())
        if(quantifier_prefix[q_cnt].second != quantifier_prefix[q_cnt+1].second)
          break;
    }
    qdpll_delete(solver);
    return true;
  }
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::initBloqqer(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                            const CNF& cnf)
{
  VarManager &VM = VarManager::instance();

  bloqqer_init(VM.getMaxCNFVar(), cnf.getNrOfClauses());
  for(size_t level = 0; level < quantifier_prefix.size(); ++level)
  {
    const vector<int> &vars = VM.getVarsOfType(quantifier_prefix[level].first);
    for(size_t var_nr = 0; var_nr < vars.size(); ++var_nr)
    {
      if(quantifier_prefix[level].second == E)
        bloqqer_decl_var(vars[var_nr]);
      else
        bloqqer_decl_var(-vars[var_nr]);
    }
  }

  const list<vector<int> > &clauses = cnf.getClauses();
  for(CNF::ClauseConstIter it = clauses.begin(); it != clauses.end(); ++it)
  {
    for(size_t lit_cnt = 0; lit_cnt < it->size(); ++lit_cnt)
      bloqqer_add((*it)[lit_cnt]);
    bloqqer_add(0);
  }
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::initBloqqer(const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                            const CNF& cnf)
{
  int max_cnf_var = 0;
  for(size_t level = 0; level < quantifier_prefix.size(); ++level)
  {
    for(size_t var_cnt = 0; var_cnt < quantifier_prefix[level].first.size(); ++var_cnt)
    {
      if(quantifier_prefix[level].first[var_cnt] > max_cnf_var)
        max_cnf_var = quantifier_prefix[level].first[var_cnt];
    }
  }

  bloqqer_init(max_cnf_var, cnf.getNrOfClauses());
  for(size_t level = 0; level < quantifier_prefix.size(); ++level)
  {
    const vector<int> &vars = quantifier_prefix[level].first;
    for(size_t var_nr = 0; var_nr < vars.size(); ++var_nr)
    {
      if(quantifier_prefix[level].second == E)
        bloqqer_decl_var(vars[var_nr]);
      else
        bloqqer_decl_var(-vars[var_nr]);
    }
  }

  const list<vector<int> > &clauses = cnf.getClauses();
  for(CNF::ClauseConstIter it = clauses.begin(); it != clauses.end(); ++it)
  {
    for(size_t lit_cnt = 0; lit_cnt < it->size(); ++lit_cnt)
      bloqqer_add((*it)[lit_cnt]);
    bloqqer_add(0);
  }
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::initDepQBF(QDPLL *solver,
                           const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                           const CNF& cnf)
{
  VarManager &VM = VarManager::instance();
  qdpll_adjust_vars(solver, VM.getMaxCNFVar());
  for(size_t q_cnt = 0; q_cnt < quantifier_prefix.size(); ++q_cnt)
  {
    if(quantifier_prefix[q_cnt].second == E)
      qdpll_new_scope(solver, QDPLL_QTYPE_EXISTS);
    else
      qdpll_new_scope(solver, QDPLL_QTYPE_FORALL);
    const vector<int> &vars = VM.getVarsOfType(quantifier_prefix[q_cnt].first);
    for(size_t v_cnt = 0; v_cnt < vars.size(); ++v_cnt)
      qdpll_add(solver, vars[v_cnt]);
    qdpll_add(solver, 0);
  }
  const list<vector<int> > &clauses = cnf.getClauses();
  for(CNF::ClauseConstIter it = clauses.begin(); it != clauses.end(); ++it)
  {
    for(size_t lit_cnt = 0; lit_cnt < it->size(); ++lit_cnt)
      qdpll_add(solver, (*it)[lit_cnt]);
    qdpll_add(solver, 0);
  }
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::initDepQBF(QDPLL *solver,
                           const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                           const CNF& cnf)
{
  int max_cnf_var = 0;
  for(size_t level = 0; level < quantifier_prefix.size(); ++level)
  {
    for(size_t var_cnt = 0; var_cnt < quantifier_prefix[level].first.size(); ++var_cnt)
    {
      if(quantifier_prefix[level].first[var_cnt] > max_cnf_var)
        max_cnf_var = quantifier_prefix[level].first[var_cnt];
    }
  }

  qdpll_adjust_vars(solver, max_cnf_var);
  for(size_t q_cnt = 0; q_cnt < quantifier_prefix.size(); ++q_cnt)
  {
    if(quantifier_prefix[q_cnt].second == E)
      qdpll_new_scope(solver, QDPLL_QTYPE_EXISTS);
    else
      qdpll_new_scope(solver, QDPLL_QTYPE_FORALL);
    const vector<int> &vars = quantifier_prefix[q_cnt].first;
    for(size_t v_cnt = 0; v_cnt < vars.size(); ++v_cnt)
      qdpll_add(solver, vars[v_cnt]);
    qdpll_add(solver, 0);
  }
  const list<vector<int> > &clauses = cnf.getClauses();
  for(CNF::ClauseConstIter it = clauses.begin(); it != clauses.end(); ++it)
  {
    for(size_t lit_cnt = 0; lit_cnt < it->size(); ++lit_cnt)
      qdpll_add(solver, (*it)[lit_cnt]);
    qdpll_add(solver, 0);
  }
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::debugCheckBloqqerVerdict(int bloqqer_verdict,
                              const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                              const CNF& cnf)
{
#ifndef NDEBUG
  DepQBFApi checker(false);
  bool check_sat = checker.isSat(quantifier_prefix, cnf);
  if(((!check_sat) && (bloqqer_verdict == 10)) || (check_sat && (bloqqer_verdict == 20)))
  {
    L_ERR("Error in DepQBFApi when using Bloqqer.");
    if((!check_sat) && (bloqqer_verdict == 10))
    {
      L_ERR("Bloqqer said SAT but it should be UNSAT.");
    }
    else
    {
      L_ERR("Bloqqer said UNSAT but it should be SAT.");
    }
    DepQBFExt printer;
    printer.dumpQBF(quantifier_prefix, cnf, "tmp/incorrect.qdimacs");
    L_ERR("QBF problem has been written to tmp/incorrect.qdimacs");
    L_ERR("The formula after pre-processing:");
    bloqqer_print();
    MASSERT(false, "Error in DepQBFApi when using Bloqqer.");
  }
#endif
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::debugCheckBloqqerVerdict(int bloqqer_verdict,
                              const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                              const CNF& cnf)
{
#ifndef NDEBUG
  DepQBFApi checker(false);
  bool check_sat = checker.isSat(quantifier_prefix, cnf);
  if(((!check_sat) && (bloqqer_verdict == 10)) || (check_sat && (bloqqer_verdict == 20)))
  {
    L_ERR("Error in DepQBFApi when using Bloqqer.");
    if((!check_sat) && (bloqqer_verdict == 10))
    {
      L_ERR("Bloqqer said SAT but it should be UNSAT.");
    }
    else
    {
      L_ERR("Bloqqer said UNSAT but it should be SAT.");
    }
    DepQBFExt printer;
    printer.dumpQBF(quantifier_prefix, cnf, "tmp/incorrect.qdimacs");
    L_ERR("QBF problem has been written to tmp/incorrect.qdimacs");
    L_ERR("The formula after pre-processing:");
    bloqqer_print();
    MASSERT(false, "Error in DepQBFApi when using Bloqqer.");
  }
#endif
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::debugCheckBloqqerModel(const vector<int> &model,
                            const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                            const CNF& cnf)
{
#ifndef NDEBUG
  VarManager &VM = VarManager::instance();
  DepQBFApi checker(false);
  CNF check(cnf);
  check.addCube(model);
  bool check_sat = checker.isSat(quantifier_prefix, check);
  if(!check_sat)
  {
    L_ERR("Bloqqer computed an incorrect model.");
    DepQBFExt printer;
    printer.dumpQBF(quantifier_prefix, cnf, "tmp/incorrect.qdimacs");
    L_ERR("QBF problem has been written to tmp/incorrect.qdimacs");
    Utils::debugPrint(model, "The computed model was: ");
    L_ERR("The formula after pre-processing:");
    bloqqer_print();
    L_ERR("The variable values as obtained from Bloqqer:");
    for(size_t q_cnt = 0; q_cnt < quantifier_prefix.size(); ++q_cnt)
    {
      const vector<int> &vars = VM.getVarsOfType(quantifier_prefix[q_cnt].first);
      for(size_t v_cnt = 0; v_cnt < vars.size(); ++v_cnt)
      {
        VarValue val = bloqqer_getvalue(vars[v_cnt]);
        string str_val;
        if(val == NEG)
          str_val = "NEG";
        else if(val == POS)
          str_val = "POS";
        else if(val == UNKN)
          str_val = "UNKN";
        else if(val == DONTCARE)
          str_val = "DONTCARE1";
        L_ERR("variable " << vars[v_cnt] << " = " << str_val);
      }
      if(q_cnt + 1 < quantifier_prefix.size())
        if(quantifier_prefix[q_cnt].second != quantifier_prefix[q_cnt+1].second)
          break;
    }
    MASSERT(false, "Bloqqer computed an incorrect model.");
  }
#endif
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::debugCheckBloqqerModel(const vector<int> &model,
                            const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                            const CNF& cnf)
{
#ifndef NDEBUG
  DepQBFApi checker(false);
  CNF check(cnf);
  check.addCube(model);
  bool check_sat = checker.isSat(quantifier_prefix, check);
  if(!check_sat)
  {
    L_ERR("Bloqqer computed an incorrect model.");
    DepQBFExt printer;
    printer.dumpQBF(quantifier_prefix, cnf, "tmp/incorrect.qdimacs");
    L_ERR("QBF problem has been written to tmp/incorrect.qdimacs");
    Utils::debugPrint(model, "The computed model was: ");
    L_ERR("The formula after pre-processing:");
    bloqqer_print();
    L_ERR("The variable values as obtained from Bloqqer:");
    for(size_t q_cnt = 0; q_cnt < quantifier_prefix.size(); ++q_cnt)
    {
      const vector<int> &vars = quantifier_prefix[q_cnt].first;
      for(size_t v_cnt = 0; v_cnt < vars.size(); ++v_cnt)
      {
        VarValue val = bloqqer_getvalue(vars[v_cnt]);
        string str_val;
        if(val == NEG)
          str_val = "NEG";
        else if(val == POS)
          str_val = "POS";
        else if(val == UNKN)
          str_val = "UNKN";
        else if(val == DONTCARE)
          str_val = "DONTCARE2";
        L_ERR("variable " << vars[v_cnt] << " = " << str_val);
      }
      if(q_cnt + 1 < quantifier_prefix.size())
        if(quantifier_prefix[q_cnt].second != quantifier_prefix[q_cnt+1].second)
          break;
    }
    MASSERT(false, "Bloqqer computed an incorrect model.");
  }
#endif
}
