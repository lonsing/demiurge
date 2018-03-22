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
/// @file AIG2CNF.cpp
/// @brief Contains the definition of the class AIG2CNF.
// -------------------------------------------------------------------------------------------

#include "AIG2CNF.h"
#include "VarManager.h"

extern "C" {
 #include "aiger.h"
}

// -------------------------------------------------------------------------------------------
AIG2CNF *AIG2CNF::instance_ = NULL;

// -------------------------------------------------------------------------------------------
AIG2CNF& AIG2CNF::instance()
{
  if(instance_ == NULL)
    instance_ = new AIG2CNF;
  MASSERT(instance_ != NULL, "Could not create AIG2CNF instance.");
  return *instance_;
}

// -------------------------------------------------------------------------------------------
void AIG2CNF::initFromAig(aiger *aig)
{
  clear();
  VarManager &VM = VarManager::instance();

  // Step 1:
  // Let's find out which AIG literals could potentially be referenced:
  vector<bool> refs(2*(aig->maxvar+1), 0);
  refs[aig->outputs[0].lit] = true;
  refs[aiger_not(aig->outputs[0].lit)] = true;
  for(unsigned cnt = 0; cnt < aig->num_latches; ++cnt)
  {
    refs[aig->latches[cnt].next] = true;
    refs[aiger_not(aig->latches[cnt].next)] = true;
    refs[aig->latches[cnt].lit] = true;
    refs[aiger_not(aig->latches[cnt].lit)] = true;
  }

  bool changed = true;
  while(changed)
  {
    changed = false;
    for(unsigned cnt = aig->num_ands; 0 < cnt--;)
    {
      unsigned lit = aig->ands[cnt].lhs;
      if (refs[lit])
      {
        if(!refs[aig->ands[cnt].rhs0] || !refs[aig->ands[cnt].rhs1])
          changed = true;
        refs[aig->ands[cnt].rhs0] = true;
        refs[aig->ands[cnt].rhs1] = true;
      }
      if (refs[aiger_not (lit)])
      {
        if(!refs[aiger_not(aig->ands[cnt].rhs0)] || !refs[aiger_not(aig->ands[cnt].rhs1)])
          changed = true;
        refs[aiger_not(aig->ands[cnt].rhs0)] = true;
        refs[aiger_not(aig->ands[cnt].rhs1)] = true;
      }
    }
  }


  // Step 2:
  // initialize the VarManager:
  VM.initFromAig(aig, refs);

  // Step 3:
  // now we create the CNF for the transition relation:
  if (refs[0] || refs[1])
  {
    trans_.add1LitClause(VM.aigLitToCnfLit(1));
    trans_tmp_deps_[VM.aigLitToCnfLit(1)] = set<VarInfo>();
  }
  // Step 3a:
  // clauses defining the outputs of the AND gates:
  for (unsigned i = 0; i < aig->num_ands; i++)
  {
    unsigned out_aig_lit = aig->ands[i].lhs;
    int out_cnf_lit = VarManager::instance().aigLitToCnfLit(out_aig_lit);
    int rhs1_cnf_lit = VarManager::instance().aigLitToCnfLit(aig->ands[i].rhs1);
    int rhs0_cnf_lit = VarManager::instance().aigLitToCnfLit(aig->ands[i].rhs0);
    if (refs[out_aig_lit])
    {
      // (lhs --> rhs1) AND (lhs --> rhs0)
      trans_.add2LitClause(-out_cnf_lit, rhs1_cnf_lit);
      trans_.add2LitClause(-out_cnf_lit, rhs0_cnf_lit);
    }
    if (refs[out_aig_lit+1])
    {
      // (!lhs --> (!rhs1 OR !rhs0)
      trans_.add3LitClause(out_cnf_lit, -rhs1_cnf_lit, -rhs0_cnf_lit);
    }
    set<VarInfo> deps;
    deps.insert(VM.getInfo((rhs1_cnf_lit < 0) ? -rhs1_cnf_lit : rhs1_cnf_lit));
    deps.insert(VM.getInfo((rhs0_cnf_lit < 0) ? -rhs0_cnf_lit : rhs0_cnf_lit));
    trans_tmp_deps_[out_cnf_lit] = deps;

  }
  trans_eq_t_ = trans_;

  // Step 3b:
  // For trans_, we add clauses saying that the inputs to the latches form the
  // next states:
  const vector<int> &next_state_vars = VM.getVarsOfType(VarInfo::NEXT_STATE);
  // state variable 0 contains the error bit, so we set
  // next_state_vars[0] <-> error_lit:
  int error_lit = VM.aigLitToCnfLit(aig->outputs[0].lit);
  trans_.add2LitClause(next_state_vars[0], -error_lit);
  trans_.add2LitClause(-next_state_vars[0], error_lit);

  // we also create equality constraints for the other state bits:
  for(unsigned cnt = 0; cnt < aig->num_latches; ++cnt)
  {
    int next_state_lit = VM.aigLitToCnfLit(aig->latches[cnt].next);
    int x_prime_lit = next_state_vars[cnt+1];
    trans_.add2LitClause(next_state_lit, -x_prime_lit);
    trans_.add2LitClause(-next_state_lit, x_prime_lit);
  }

  // Step 3c
  // For trans_eq_t_, we add clauses saying that the inputs to the latches form the
  // next states if and only if t_ is true
  t_ = VM.createFreshTmpVar("t");
  vector<int> all_equal_clause(aig->num_latches + 2, 0);
  all_equal_clause[0] = t_;
  // state variable 0 contains the error bit, so we set
  // err_eq <-> (next_state_vars[0] <-> error_lit):
  int err_eq = VM.createFreshTmpVar("x_eq_x'");
  trans_eq_t_.add3LitClause(error_lit, next_state_vars[0], err_eq);
  trans_eq_t_.add3LitClause(error_lit, -next_state_vars[0], -err_eq);
  trans_eq_t_.add3LitClause(-error_lit, next_state_vars[0], -err_eq);
  trans_eq_t_.add3LitClause(-error_lit, -next_state_vars[0], err_eq);
  all_equal_clause[1] = -err_eq;
  // t is false if err_eq is false
  trans_eq_t_.add2LitClause(err_eq, -t_);

  for(unsigned cnt = 0; cnt < aig->num_latches; ++cnt)
  {
    int next_state_lit = VM.aigLitToCnfLit(aig->latches[cnt].next);
    int x_prime_lit = next_state_vars[cnt+1];
    int equal = VM.createFreshTmpVar("x_eq_x'");
    all_equal_clause[cnt+2] = -equal;
    // equal is true    iff   next_state_lit is equal to x_prime_lit
    trans_eq_t_.add3LitClause(next_state_lit, x_prime_lit, equal);
    trans_eq_t_.add3LitClause(next_state_lit, -x_prime_lit, -equal);
    trans_eq_t_.add3LitClause(-next_state_lit, x_prime_lit, -equal);
    trans_eq_t_.add3LitClause(-next_state_lit, -x_prime_lit, equal);
    // t is false if one of the 'equal'-literals is false
    trans_eq_t_.add2LitClause(equal, -t_);
  }
  // t is true all the 'equal'-literals are true
  trans_eq_t_.addClause(all_equal_clause);


  // Step 4:
  // Initialize the CNFs characterizing the good/bad/initial states:
  safe_.add1LitClause(-VM.getPresErrorStateVar());
  unsafe_.add1LitClause(VM.getPresErrorStateVar());
  next_safe_.add1LitClause(-VM.getNextErrorStateVar());
  next_unsafe_.add1LitClause(VM.getNextErrorStateVar());
  const vector<int> &state_vars = VM.getVarsOfType(VarInfo::PRES_STATE);
  for(size_t cnt = 0; cnt < state_vars.size(); ++cnt)
    initial_.add1LitClause(-state_vars[cnt]);
}

// -------------------------------------------------------------------------------------------
void AIG2CNF::clear()
{
  trans_.clear();
  trans_eq_t_.clear();
  t_ = 0;
  safe_.clear();
  unsafe_.clear();
  next_safe_.clear();
  next_unsafe_.clear();
  initial_.clear();
}

// -------------------------------------------------------------------------------------------
const CNF& AIG2CNF::getTrans() const
{
  return trans_;
}

// -------------------------------------------------------------------------------------------
const CNF& AIG2CNF::getTransEqT() const
{
  return trans_eq_t_;
}

// -------------------------------------------------------------------------------------------
int AIG2CNF::getT() const
{
  return t_;
}

// -------------------------------------------------------------------------------------------
const CNF& AIG2CNF::getSafeStates() const
{
  return safe_;
}

// -------------------------------------------------------------------------------------------
const CNF& AIG2CNF::getUnsafeStates() const
{
  return unsafe_;
}

// -------------------------------------------------------------------------------------------
const CNF& AIG2CNF::getNextSafeStates() const
{
  return next_safe_;
}

// -------------------------------------------------------------------------------------------
const CNF& AIG2CNF::getNextUnsafeStates() const
{
  return next_unsafe_;
}

// -------------------------------------------------------------------------------------------
const CNF& AIG2CNF::getInitial() const
{
  return initial_;
}

// -------------------------------------------------------------------------------------------
const map<int, set<VarInfo> >& AIG2CNF::getTmpDeps() const
{
  return trans_tmp_deps_;
}


// -------------------------------------------------------------------------------------------
const map<int, set<VarInfo> >& AIG2CNF::getTmpDepsTrans()
{
  // we compute the transitive dependencies only lazily:
  typedef set<VarInfo>::const_iterator SetConstIter;
  while(trans_tmp_deps_trans_.size() != trans_tmp_deps_.size())
  {
    for(DepConstIter it = trans_tmp_deps_.begin(); it != trans_tmp_deps_.end(); ++it)
    {
      if(trans_tmp_deps_trans_.count(it->first)) // already done
        continue;
      bool abort = false;
      set<VarInfo> trans_dep;
      for(SetConstIter it2 = it->second.begin(); it2 != it->second.end(); ++it2)
      {
        if(it2->getKind() == VarInfo::TMP)
        {
          DepConstIter d = trans_tmp_deps_trans_.find(it2->getLitInCNF());
          if(d == trans_tmp_deps_trans_.end())
          {
            abort = true;
            break;
          }
          else
          {
            trans_dep.insert(d->second.begin(), d->second.end());
          }
        }
        else
          trans_dep.insert(*it2);
      }
      if(!abort)
        trans_tmp_deps_trans_[it->first] = trans_dep;
    }
  }
  return trans_tmp_deps_trans_;
}

// -------------------------------------------------------------------------------------------
AIG2CNF::AIG2CNF()
{
  // nothing to be done
}

// -------------------------------------------------------------------------------------------
AIG2CNF::~AIG2CNF()
{
  // nothing to be done
}


