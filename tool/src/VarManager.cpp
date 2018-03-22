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
/// @file VarManager.cpp
/// @brief Contains the definition of the class VarManager.
// -------------------------------------------------------------------------------------------

#include "VarManager.h"
#include "StringUtils.h"

extern "C" {
 #include "aiger.h"
}

// -------------------------------------------------------------------------------------------
vector<VarManager*> VarManager::stack_;

// -------------------------------------------------------------------------------------------
VarManager &VarManager::instance()
{
  if(stack_.empty())
  {
    if(stack_.capacity() < 5)
      stack_.reserve(5);
    VarManager *new_man = new VarManager();
    stack_.push_back(new_man);
    MASSERT(new_man != NULL, "could not create VarManager instance");
  }
  return *(stack_.back());
}

// -------------------------------------------------------------------------------------------
void VarManager::push()
{
  VarManager *current = stack_.back();
  VarManager *copy = new VarManager(*current);
  stack_.push_back(copy);
}

// -------------------------------------------------------------------------------------------
void VarManager::resetToLastPush()
{
  MASSERT(stack_.size() > 1, "Nothing on stack.");
  VarManager *current = stack_.back();
  VarManager *last_push = stack_[stack_.size() - 2];
  current->vars_ = last_push->vars_;
  current->max_cnf_var_ = last_push->max_cnf_var_;
  current->max_aig_var_ = last_push->max_aig_var_;
  current->aig_to_cnf_lit_map_ = last_push->aig_to_cnf_lit_map_;
  current->inputs_ = last_push->inputs_;
  current->ctrl_vars_ = last_push->ctrl_vars_;
  current->pres_state_vars_ = last_push->pres_state_vars_;
  current->next_state_vars_ = last_push->next_state_vars_;
  current->tmp_vars_ = last_push->tmp_vars_;
  current->templs_params_ = last_push->templs_params_;
  current->prev_vars_ = last_push->prev_vars_;
}

// -------------------------------------------------------------------------------------------
void VarManager::pop()
{
  if(stack_.size() > 1)
  {
    delete stack_.back();
    stack_.pop_back();
  }
}

// -------------------------------------------------------------------------------------------
void VarManager::initFromAig(aiger *aig, const vector<bool>& refs)
{
  clear();
  // build map from AIG literals to CNF literals:
  aig_to_cnf_lit_map_.resize(2*(aig->maxvar+1), 0);
  max_cnf_var_ = 0;
  if (refs[0] || refs[1])
  {
    aig_to_cnf_lit_map_[0] = -1;
    aig_to_cnf_lit_map_[1] = 1;
    max_cnf_var_++;
    VarInfo info(VarInfo::TMP, 1, 1, "TRUE");
    vars_.insert(pair<int,VarInfo>(1, info));
  }
  for (unsigned lit = 2; lit <= 2*aig->maxvar; lit += 2)
  {
    if (refs[lit] || refs[lit+1])
    {
      aig_to_cnf_lit_map_[lit] = ++max_cnf_var_;
      aig_to_cnf_lit_map_[lit+1] = -max_cnf_var_;
      VarInfo info(VarInfo::TMP, max_cnf_var_, lit);
      vars_.insert(pair<int,VarInfo>(max_cnf_var_, info));
    }
  }
  max_aig_var_ = aig->maxvar;

  // set type and name properly:
  inputs_.reserve(aig->num_inputs);
  for(unsigned cnt = 0; cnt < aig->num_inputs; ++cnt)
  {
    unsigned lit = aig->inputs[cnt].lit;
    if (refs[lit] || refs[lit+1])
    {
      int cnf_in_var = aig_to_cnf_lit_map_[lit];
      MASSERT(vars_.find(cnf_in_var) != vars_.end(), "Var not found");
      string aig_name;
      ostringstream name;
      if(aig->inputs[cnt].name != NULL)
        aig_name = aig->inputs[cnt].name;
      string aig_name_lower = StringUtils::toLowerCase(aig_name);
      if(aig_name_lower.find("controllable_") == 0)
      {
        name << "c_" << cnt << "(" << aig_name << ")";
        vars_.find(cnf_in_var)->second.setName(name.str());
        vars_.find(cnf_in_var)->second.setKind(VarInfo::CTRL);
        ctrl_vars_.push_back(cnf_in_var);
      }
      else
      {
        name << "i_" << cnt << "(" << aig_name << ")";
        vars_.find(cnf_in_var)->second.setName(name.str());
        vars_.find(cnf_in_var)->second.setKind(VarInfo::INPUT);
        inputs_.push_back(cnf_in_var);
      }
    }
  }
  unsigned output_var = aig->outputs[0].lit;
  output_var &= ~1;
  if((aig->outputs[0].lit & 1) == 0)
    vars_.find(aig_to_cnf_lit_map_[output_var])->second.setName("error_output");
  else
    vars_.find(aig_to_cnf_lit_map_[output_var])->second.setName("neg_error_output");
  pres_state_vars_.reserve(aig->num_latches+1);
  next_state_vars_.reserve(aig->num_latches+1);
  // we latch the error_output to make it part of our state space:
  max_cnf_var_++;
  VarInfo pe_info(VarInfo::PRES_STATE, max_cnf_var_, 0, "pres_error");
  vars_.insert(pair<int,VarInfo>(max_cnf_var_, pe_info));
  pres_state_vars_.push_back(max_cnf_var_);
  max_cnf_var_++;
  VarInfo ne_info(VarInfo::NEXT_STATE, max_cnf_var_, 0, "next_error");
  vars_.insert(pair<int,VarInfo>(max_cnf_var_, ne_info));
  next_state_vars_.push_back(max_cnf_var_);


  for(unsigned cnt = 0; cnt < aig->num_latches; ++cnt)
  {
    // present state:
    unsigned lit = aig->latches[cnt].lit;
    int cnf_state_var = aig_to_cnf_lit_map_[lit];
    vars_.find(cnf_state_var)->second.setKind(VarInfo::PRES_STATE);
    ostringstream name;
    name << "x_" << cnt;
    if(aig->latches[cnt].name != NULL)
      name << "(" << string(aig->latches[cnt].name) << ")";
    vars_.find(cnf_state_var)->second.setName(name.str());
    pres_state_vars_.push_back(cnf_state_var);

    // next state:
    // we create a new variable to have a new name for sure:
    lit = aig->latches[cnt].next;
    ostringstream next_name;
    next_name << "x'_" << cnt;
    if(aig->latches[cnt].name != NULL)
      next_name << "(" << string(aig->latches[cnt].name) << ")";
    max_cnf_var_++;
    VarInfo ns_info(VarInfo::NEXT_STATE, max_cnf_var_, lit, next_name.str());
    vars_.insert(pair<int,VarInfo>(max_cnf_var_, ns_info));
    next_state_vars_.push_back(max_cnf_var_);
  }

  // all remaining variables must be temporary variables.
  tmp_vars_.reserve(1000000);
  for(VarsConstIter it = vars_.begin(); it != vars_.end(); ++it)
  {
    if(it->second.getKind() == VarInfo::TMP)
      tmp_vars_.push_back(it->first);
  }
}

// -------------------------------------------------------------------------------------------
void VarManager::clear()
{
  vars_.clear();
  max_cnf_var_ = 0;
  max_aig_var_ = 0;
  aig_to_cnf_lit_map_.clear();
  inputs_.clear();
  ctrl_vars_.clear();
  pres_state_vars_.clear();
  next_state_vars_.clear();
  tmp_vars_.clear();
  templs_params_.clear();
  prev_vars_.clear();
}

// -------------------------------------------------------------------------------------------
int VarManager::createFreshTmpVar()
{
  return createFreshTmpVar("tmp");
}

// -------------------------------------------------------------------------------------------
int VarManager::createFreshTmpVar(const string& name)
{
  max_cnf_var_++;
  VarInfo info(VarInfo::TMP, max_cnf_var_, 0, name);
  vars_.insert(pair<int,VarInfo>(max_cnf_var_, info));
  tmp_vars_.push_back(max_cnf_var_);
  return max_cnf_var_;
}

// -------------------------------------------------------------------------------------------
int VarManager::createFreshTemplParam()
{
  return createFreshTemplParam("param");
}

// -------------------------------------------------------------------------------------------
int VarManager::createFreshTemplParam(const string& name)
{
  max_cnf_var_++;
  VarInfo info(VarInfo::TEMPL_PARAMS, max_cnf_var_, 0, name);
  vars_.insert(pair<int,VarInfo>(max_cnf_var_, info));
  templs_params_.push_back(max_cnf_var_);
  return max_cnf_var_;
}

// -------------------------------------------------------------------------------------------
int VarManager::createFreshPrevVar()
{
  return createFreshPrevVar("prev");
}

// -------------------------------------------------------------------------------------------
int VarManager::createFreshPrevVar(const string& name)
{
  max_cnf_var_++;
  VarInfo info(VarInfo::PREV, max_cnf_var_, 0, name);
  vars_.insert(pair<int,VarInfo>(max_cnf_var_, info));
  prev_vars_.push_back(max_cnf_var_);
  return max_cnf_var_;
}

// -------------------------------------------------------------------------------------------
const VarInfo& VarManager::getInfo(int var_in_cnf) const
{
  MASSERT(vars_.find(var_in_cnf) != vars_.end(), "Var not found");
  return vars_.find(var_in_cnf)->second;
}

// -------------------------------------------------------------------------------------------
int VarManager::aigLitToCnfLit(unsigned aig_lit) const
{
  MASSERT(aig_lit < aig_to_cnf_lit_map_.size(), "Variable out of bounds.");
  return aig_to_cnf_lit_map_[aig_lit];
}

// -------------------------------------------------------------------------------------------
const vector<int>& VarManager::getVarsOfType(VarInfo::VarKind var_kind) const
{
  if(var_kind == VarInfo::INPUT)
    return inputs_;
  else if(var_kind == VarInfo::CTRL)
    return ctrl_vars_;
  else if(var_kind == VarInfo::PRES_STATE)
    return pres_state_vars_;
  else if(var_kind == VarInfo::NEXT_STATE)
    return next_state_vars_;
  else if(var_kind == VarInfo::TEMPL_PARAMS)
    return templs_params_;
  else if(var_kind == VarInfo::PREV)
    return prev_vars_;
  else //(var_kind == VarInfo::TMP)
    return tmp_vars_;
}

// -------------------------------------------------------------------------------------------
vector<int> VarManager::getAllNonTempVars() const
{
  vector<int> res;
  size_t size = inputs_.size();
  size += ctrl_vars_.size();
  size += pres_state_vars_.size();
  size += next_state_vars_.size();
  size += templs_params_.size();
  size += prev_vars_.size();
  res.reserve(size);
  res.insert(res.end(), inputs_.begin(), inputs_.end());
  res.insert(res.end(), ctrl_vars_.begin(), ctrl_vars_.end());
  res.insert(res.end(), pres_state_vars_.begin(), pres_state_vars_.end());
  res.insert(res.end(), next_state_vars_.begin(), next_state_vars_.end());
  res.insert(res.end(), templs_params_.begin(), templs_params_.end());
  res.insert(res.end(), prev_vars_.begin(), prev_vars_.end());
  return res;
}

// -------------------------------------------------------------------------------------------
int VarManager::getPresErrorStateVar() const
{
  return pres_state_vars_[0];
}

// -------------------------------------------------------------------------------------------
int VarManager::getNextErrorStateVar() const
{
  return next_state_vars_[0];
}

// -------------------------------------------------------------------------------------------
int VarManager::getMaxCNFVar() const
{
  return max_cnf_var_;
}

// -------------------------------------------------------------------------------------------
int VarManager::getMaxAIGVar() const
{
  return max_aig_var_;
}

// -------------------------------------------------------------------------------------------
string VarManager::toString()
{
  ostringstream oss;
  map<VarInfo::VarKind, string> var_kind_names;
  var_kind_names[VarInfo::PRES_STATE] = "PS";
  var_kind_names[VarInfo::NEXT_STATE] = "NS";
  var_kind_names[VarInfo::INPUT] = "IN";
  var_kind_names[VarInfo::CTRL] = "CTRL";
  var_kind_names[VarInfo::TMP] = "TMP";
  var_kind_names[VarInfo::TEMPL_PARAMS] = "PARAM";
  var_kind_names[VarInfo::PREV] = "PREV";
  oss << "Variables:" << endl;
  oss << "==========" << endl;
  for(VarsConstIter it = vars_.begin(); it != vars_.end(); ++it)
  {
    const VarInfo &info = it->second;
    oss << "- CNF: "<< info.getLitInCNF();
    oss << ", Kind:" << var_kind_names[info.getKind()];
    oss << ", AIG:" << info.getLitInAIG();
    oss << ", name:" << info.getName() << endl;
  }
  return oss.str();
}

// -------------------------------------------------------------------------------------------
VarManager::VarManager(): max_cnf_var_(0), max_aig_var_(0)
{
  templs_params_.reserve(10000);
}

// -------------------------------------------------------------------------------------------
VarManager::~VarManager()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
VarManager::VarManager(const VarManager &other) :
      vars_(other.vars_),
      max_cnf_var_(other.max_cnf_var_),
      max_aig_var_(other.max_aig_var_),
      aig_to_cnf_lit_map_(other.aig_to_cnf_lit_map_),
      inputs_(other.inputs_),
      ctrl_vars_(other.ctrl_vars_),
      pres_state_vars_(other.pres_state_vars_),
      next_state_vars_(other.next_state_vars_),
      tmp_vars_(other.tmp_vars_),
      templs_params_(other.templs_params_),
      prev_vars_(other.prev_vars_)
{
  // nothing to do
}

