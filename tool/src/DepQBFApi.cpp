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
DepQBFApi::DepQBFApi(bool use_bloqqer) :
    use_bloqqer_(use_bloqqer),
    min_cores_(true),
    inc_solver_(NULL)
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
DepQBFApi::~DepQBFApi()
{
  clearIncrementalSession();
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
        if(qdpll_is_var_declared(solver, vars[v_cnt]))
        {
          QDPLLAssignment val = qdpll_get_value(solver, vars[v_cnt]);
          if(val == QDPLL_ASSIGNMENT_FALSE)
            model.push_back(-vars[v_cnt]);
          else if(val == QDPLL_ASSIGNMENT_TRUE)
            model.push_back(vars[v_cnt]);
        }
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
        if(qdpll_is_var_declared(solver, vars[v_cnt]))
        {
          QDPLLAssignment val = qdpll_get_value(solver, vars[v_cnt]);
          if(val == QDPLL_ASSIGNMENT_FALSE)
            model.push_back(-vars[v_cnt]);
          else if(val == QDPLL_ASSIGNMENT_TRUE)
            model.push_back(vars[v_cnt]);
        }
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
void DepQBFApi::startIncrementalSession(
    const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix)
{
  if(inc_solver_ != NULL)
    qdpll_delete(inc_solver_);
  inc_solver_ = qdpll_create();
  qdpll_configure (inc_solver_, const_cast<char*>("--incremental-use"));
  qdpll_configure(inc_solver_, const_cast<char*>("--dep-man=simple"));
  CNF empty;
  initDepQBF(inc_solver_, quantifier_prefix, empty);

  VarManager &VM = VarManager::instance();
  inc_model_vars_.clear();
  inc_model_vars_.reserve(VM.getMaxCNFVar());
  for(size_t q_cnt = 0; q_cnt < quantifier_prefix.size(); ++q_cnt)
  {
    if(quantifier_prefix[q_cnt].second == E)
    {
      const vector<int> &vars = VM.getVarsOfType(quantifier_prefix[q_cnt].first);
      for(size_t v_cnt = 0; v_cnt < vars.size(); ++v_cnt)
        inc_model_vars_.push_back(vars[v_cnt]);
    }
    else
      break;
  }

#ifndef NDEBUG
  inc_decl_vars_.clear();
  for(size_t q_cnt = 0; q_cnt < quantifier_prefix.size(); ++q_cnt)
  {
    const vector<int> &vars = VM.getVarsOfType(quantifier_prefix[q_cnt].first);
    for(size_t v_cnt = 0; v_cnt < vars.size(); ++v_cnt)
      inc_decl_vars_.insert(vars[v_cnt]);
  }
#endif

}

// -------------------------------------------------------------------------------------------
void DepQBFApi::startIncrementalSession(
    const vector<pair<vector<int>, Quant> > &quantifier_prefix)
{
  if(inc_solver_ != NULL)
    qdpll_delete(inc_solver_);
  inc_solver_ = qdpll_create();
  qdpll_configure(inc_solver_, const_cast<char*>("--dep-man=simple"));
  qdpll_configure (inc_solver_, const_cast<char*>("--incremental-use"));
  CNF empty;
  initDepQBF(inc_solver_, quantifier_prefix, empty);

  VarManager &VM = VarManager::instance();
  inc_model_vars_.clear();
  inc_model_vars_.reserve(VM.getMaxCNFVar());
  for(size_t q_cnt = 0; q_cnt < quantifier_prefix.size(); ++q_cnt)
  {
    if(quantifier_prefix[q_cnt].second == E)
    {
      const vector<int> &vars = quantifier_prefix[q_cnt].first;
      for(size_t v_cnt = 0; v_cnt < vars.size(); ++v_cnt)
        inc_model_vars_.push_back(vars[v_cnt]);
    }
    else
      break;
  }

#ifndef NDEBUG
  inc_decl_vars_.clear();
  for(size_t q_cnt = 0; q_cnt < quantifier_prefix.size(); ++q_cnt)
  {
    const vector<int> &vars = quantifier_prefix[q_cnt].first;
    for(size_t v_cnt = 0; v_cnt < vars.size(); ++v_cnt)
      inc_decl_vars_.insert(vars[v_cnt]);
  }
#endif

}

// -------------------------------------------------------------------------------------------
void DepQBFApi::doMinCores(bool min_cores)
{
  min_cores_ = min_cores;
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::clearIncrementalSession()
{
  if(inc_solver_ != NULL)
  {
    qdpll_delete(inc_solver_);
    inc_solver_ = NULL;
  }
  inc_model_vars_.clear();
  inc_decl_vars_.clear();
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::incAddVarAtInnermostQuant(int var)
{
  MASSERT(inc_solver_ != NULL, "No open session.");
  Nesting n = qdpll_get_max_scope_nesting(inc_solver_);
  qdpll_add_var_to_scope(inc_solver_, var, n);
#ifndef NDEBUG
      inc_decl_vars_.insert(var);
#endif
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::incAddCNF(const CNF &cnf)
{
  MASSERT(inc_solver_ != NULL, "No open session.");
  const list<vector<int> > &clauses = cnf.getClauses();
  for(CNF::ClauseConstIter it = clauses.begin(); it != clauses.end(); ++it)
  {
    for(size_t lit_cnt = 0; lit_cnt < it->size(); ++lit_cnt)
    {
      debugAssertIsDeclaredInc((*it)[lit_cnt]);
      qdpll_add(inc_solver_, (*it)[lit_cnt]);
    }
    qdpll_add(inc_solver_, 0);
  }
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::incAddClause(const vector<int> &clause)
{
  MASSERT(inc_solver_ != NULL, "No open session.");
  for(size_t lit_cnt = 0; lit_cnt < clause.size(); ++lit_cnt)
  {
    debugAssertIsDeclaredInc(clause[lit_cnt]);
    qdpll_add(inc_solver_, clause[lit_cnt]);
  }
  qdpll_add(inc_solver_, 0);
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::incAddUnitClause(int lit)
{
  MASSERT(inc_solver_ != NULL, "No open session.");
  debugAssertIsDeclaredInc(lit);
  qdpll_add(inc_solver_, lit);
  qdpll_add(inc_solver_, 0);
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::incAdd2LitClause(int lit1, int lit2)
{
  MASSERT(inc_solver_ != NULL, "No open session.");
  debugAssertIsDeclaredInc(lit1);
  debugAssertIsDeclaredInc(lit2);
  qdpll_add(inc_solver_, lit1);
  qdpll_add(inc_solver_, lit2);
  qdpll_add(inc_solver_, 0);
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::incAdd3LitClause(int lit1, int lit2, int lit3)
{
  MASSERT(inc_solver_ != NULL, "No open session.");
  debugAssertIsDeclaredInc(lit1);
  debugAssertIsDeclaredInc(lit2);
  debugAssertIsDeclaredInc(lit3);
  qdpll_add(inc_solver_, lit1);
  qdpll_add(inc_solver_, lit2);
  qdpll_add(inc_solver_, lit3);
  qdpll_add(inc_solver_, 0);
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::incAddCube(const vector<int> &cube)
{
  MASSERT(inc_solver_ != NULL, "No open session.");
  for(size_t lit_cnt = 0; lit_cnt < cube.size(); ++lit_cnt)
  {
    debugAssertIsDeclaredInc(cube[lit_cnt]);
    qdpll_add(inc_solver_, cube[lit_cnt]);
    qdpll_add(inc_solver_, 0);
  }
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::incAddNegCubeAsClause(const vector<int> &cube)
{
  MASSERT(inc_solver_ != NULL, "No open session.");
  for(size_t lit_cnt = 0; lit_cnt < cube.size(); ++lit_cnt)
  {
    debugAssertIsDeclaredInc(-cube[lit_cnt]);
    qdpll_add(inc_solver_, -cube[lit_cnt]);
  }
  qdpll_add(inc_solver_, 0);
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::incPush()
{
  MASSERT(inc_solver_ != NULL, "No open session.");
  qdpll_push(inc_solver_);
}

// -------------------------------------------------------------------------------------------
void DepQBFApi::incPop()
{
  MASSERT(inc_solver_ != NULL, "No open session.");
  qdpll_pop(inc_solver_);
}

// -------------------------------------------------------------------------------------------
bool DepQBFApi::incIsSat()
{
  MASSERT(inc_solver_ != NULL, "No open session.");
  QDPLLResult res = qdpll_sat(inc_solver_);
  qdpll_reset(inc_solver_);
  if(res == QDPLL_RESULT_SAT)
    return true;
  if(res == QDPLL_RESULT_UNSAT)
    return false;
  MASSERT(false, "Strange return code from DepQBF.");
}

// -------------------------------------------------------------------------------------------
bool DepQBFApi::incIsSat(const vector<int> &assumptions)
{
  MASSERT(inc_solver_ != NULL, "No open session.");
  // if you try to add assumptions before solving for the first time, the solver will crash
  // because the dependency manager has not yet been initialized. So we do it here, just in
  // case:
  qdpll_init_deps(inc_solver_);
  for(size_t ass_cnt = 0; ass_cnt  < assumptions.size(); ++ass_cnt)
  {
    debugAssertIsDeclaredInc(assumptions[ass_cnt]);
    qdpll_assume(inc_solver_, assumptions[ass_cnt]);
  }
  QDPLLResult res = qdpll_sat(inc_solver_);
  qdpll_reset(inc_solver_);
  if(res == QDPLL_RESULT_SAT)
    return true;
  if(res == QDPLL_RESULT_UNSAT)
    return false;
  MASSERT(false, "Strange return code from DepQBF.");
}

// -------------------------------------------------------------------------------------------
bool DepQBFApi::incIsSatModelOrCore(const vector<int> &assumptions,
                                    vector<int> &model_or_core)
{
  MASSERT(inc_solver_ != NULL, "No open session.");
  // if you try to add assumptions before solving for the first time, the solver will crash
  // because the dependency manager has not yet been initialized. So we do it here, just in
  // case:
  qdpll_init_deps(inc_solver_);
  for(size_t ass_cnt = 0; ass_cnt  < assumptions.size(); ++ass_cnt)
  {
    debugAssertIsDeclaredInc(assumptions[ass_cnt]);
    qdpll_assume(inc_solver_, assumptions[ass_cnt]);
  }
  QDPLLResult res = qdpll_sat(inc_solver_);
  if(res == QDPLL_RESULT_SAT)
  {
    model_or_core.clear();
    model_or_core.reserve(inc_model_vars_.size());
    for(size_t var_cnt = 0; var_cnt < inc_model_vars_.size(); ++var_cnt)
    {
      if(qdpll_is_var_declared(inc_solver_, inc_model_vars_[var_cnt]))
      {
        QDPLLAssignment val = qdpll_get_value(inc_solver_, inc_model_vars_[var_cnt]);
        if(val == QDPLL_ASSIGNMENT_FALSE)
          model_or_core.push_back(-inc_model_vars_[var_cnt]);
        else if(val == QDPLL_ASSIGNMENT_TRUE)
          model_or_core.push_back(inc_model_vars_[var_cnt]);
      }
    }
    qdpll_reset(inc_solver_);
    return true;
  }
  else if(res == QDPLL_RESULT_UNSAT)
  {
    if(assumptions.size() > 0)
    {
      model_or_core.clear();
      model_or_core.reserve(assumptions.size());
      extractCore(inc_solver_, model_or_core);
    }
    else
      qdpll_reset(inc_solver_);
    return false;
  }
  else
  {
    MASSERT(false, "Strange return code from DepQBF.");
  }
}

// -------------------------------------------------------------------------------------------
bool DepQBFApi::incIsSatModel(const vector<int> &assumptions,
                              vector<int> &model)
{
  MASSERT(inc_solver_ != NULL, "No open session.");
  // if you try to add assumptions before solving for the first time, the solver will crash
  // because the dependency manager has not yet been initialized. So we do it here, just in
  // case:
  qdpll_init_deps(inc_solver_);
  for(size_t ass_cnt = 0; ass_cnt  < assumptions.size(); ++ass_cnt)
  {
    debugAssertIsDeclaredInc(assumptions[ass_cnt]);
    qdpll_assume(inc_solver_, assumptions[ass_cnt]);
  }
  QDPLLResult res = qdpll_sat(inc_solver_);
  if(res == QDPLL_RESULT_SAT)
  {
    model.clear();
    model.reserve(inc_model_vars_.size());
    for(size_t var_cnt = 0; var_cnt < inc_model_vars_.size(); ++var_cnt)
    {
      if(qdpll_is_var_declared(inc_solver_, inc_model_vars_[var_cnt]))
      {
        QDPLLAssignment val = qdpll_get_value(inc_solver_, inc_model_vars_[var_cnt]);
        if(val == QDPLL_ASSIGNMENT_FALSE)
          model.push_back(-inc_model_vars_[var_cnt]);
        else if(val == QDPLL_ASSIGNMENT_TRUE)
          model.push_back(inc_model_vars_[var_cnt]);
      }
    }
    qdpll_reset(inc_solver_);
    return true;
  }
  else if(res == QDPLL_RESULT_UNSAT)
  {
    qdpll_reset(inc_solver_);
    return false;
  }
  else
  {
    MASSERT(false, "Strange return code from DepQBF.");
  }
}

// -------------------------------------------------------------------------------------------
bool DepQBFApi::incIsSatCore(const vector<int> &assumptions,
                             vector<int> &core)
{
  MASSERT(inc_solver_ != NULL, "No open session.");
  // if you try to add assumptions before solving for the first time, the solver will crash
  // because the dependency manager has not yet been initialized. So we do it here, just in
  // case:
  qdpll_init_deps(inc_solver_);
  for(size_t ass_cnt = 0; ass_cnt  < assumptions.size(); ++ass_cnt)
  {
    debugAssertIsDeclaredInc(assumptions[ass_cnt]);
    qdpll_assume(inc_solver_, assumptions[ass_cnt]);
  }
  QDPLLResult res = qdpll_sat(inc_solver_);
  if(res == QDPLL_RESULT_SAT)
  {
    qdpll_reset(inc_solver_);
    return true;
  }
  else if(res == QDPLL_RESULT_UNSAT)
  {
    if(assumptions.size() > 0)
    {
      core.clear();
      core.reserve(assumptions.size());
      extractCore(inc_solver_, core);
    }
    else
      qdpll_reset(inc_solver_);
    return false;
  }
  else
  {
    MASSERT(false, "Strange return code from DepQBF.");
  }
}


// -------------------------------------------------------------------------------------------
void DepQBFApi::debugDumpIncSolverState()
{
  MASSERT(inc_solver_ != NULL, "No open session.");
  qdpll_print(inc_solver_, stdout);
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
void DepQBFApi::extractCore(QDPLL *solver, vector<int> &core)
{
  LitID *raw_core = qdpll_get_relevant_assumptions(solver);
  LitID *next_core_element = raw_core;
  while(*next_core_element != 0)
  {
    core.push_back(*next_core_element);
    ++next_core_element;
  }
  free(raw_core);
  raw_core = NULL;
  qdpll_reset(solver);
  if(min_cores_ && core.size() > 0)
  {
    vector<int> orig_core(core);
    Utils::randomize(orig_core);
    for(size_t cnt = 0; cnt < orig_core.size(); ++cnt)
    {
      vector<int> tmp(core);
      bool found = Utils::remove(tmp, orig_core[cnt]);
      if(found)
      {
        for(size_t ass_cnt = 0; ass_cnt  < tmp.size(); ++ass_cnt)
          qdpll_assume(solver, tmp[ass_cnt]);
        QDPLLResult res = qdpll_sat(solver);
        if(res == QDPLL_RESULT_UNSAT)
        {
          core.clear();
          if(tmp.size() > 0)
          {
            core.reserve(tmp.size());
            raw_core = qdpll_get_relevant_assumptions(solver);
            next_core_element = raw_core;
            while(*next_core_element != 0)
            {
              core.push_back(*next_core_element);
              ++next_core_element;
            }
          }
        }
        qdpll_reset(solver);
      }
    }
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

// -------------------------------------------------------------------------------------------
void DepQBFApi::debugAssertIsDeclaredInc(int lit) const
{
#ifndef NDEBUG
  int v = lit > 0 ? lit : -lit;
  DASSERT(inc_decl_vars_.count(v) > 0, "Var " << v  << " is not quantified.");
#endif
}
