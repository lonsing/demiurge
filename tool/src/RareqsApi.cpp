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
//   <http://www.iaik.tugraz.at/content/research/design_verification/others/>
// or email the authors directly.
//
// ----------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------
/// @file RareqsApi.cpp
/// @brief Contains the definition of the class RareqsApi.
// -------------------------------------------------------------------------------------------


#include "RareqsApi.h"
#include "VarManager.h"
#include "CNF.h"
#include "rareqs_api.hh"

// -------------------------------------------------------------------------------------------
///
/// @brief A global variable that is needed by the Rareqs implementation.
int verbose=0;

// -------------------------------------------------------------------------------------------
///
/// @brief A global variable that is needed by the Rareqs implementation.
int use_blocking=0;

// -------------------------------------------------------------------------------------------
///
/// @brief A global variable that is needed by the Rareqs implementation.
int use_pure=0;

// -------------------------------------------------------------------------------------------
///
/// @brief A global variable that is needed by the Rareqs implementation.
int use_universal_reduction=0;

// -------------------------------------------------------------------------------------------
RareqsApi::RareqsApi() : QBFSolver()
{
  // nothing to be done
}

// -------------------------------------------------------------------------------------------
RareqsApi::~RareqsApi()
{
  // nothing to be done
}

// -------------------------------------------------------------------------------------------
bool RareqsApi::isSat(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                      const CNF& cnf)
{
  vector<int> model;
  VarManager &VM = VarManager::instance();
  vector<pair<vector<int>, RAReQSQuantifierType> > pr;
  for(size_t cnt = 0; cnt < quantifier_prefix.size(); ++cnt)
  {
    vector<int> merged_vars;
    while(true)
    {
      const vector<int> &vars = VM.getVarsOfType(quantifier_prefix[cnt].first);
      merged_vars.insert(merged_vars.end(), vars.begin(), vars.end());
      if(cnt == quantifier_prefix.size() - 1)
        break;
      if(quantifier_prefix[cnt].second != quantifier_prefix[cnt+1].second)
        break;
      ++cnt;
    }
    if(quantifier_prefix[cnt].second == E)
      pr.push_back(make_pair(merged_vars, RAReQS_EXISTS));
    else
      pr.push_back(make_pair(merged_vars, RAReQS_FORALL));
  }
  return RAReQSisSatModel(pr, cnf.getClauses(), model);
}


// -------------------------------------------------------------------------------------------
bool RareqsApi::isSat(const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                      const CNF& cnf)
{
  vector<int> model;
  vector<pair<vector<int>, RAReQSQuantifierType> > pr;
  for(size_t cnt = 0; cnt < quantifier_prefix.size(); ++cnt)
  {
    vector<int> merged_vars;
    while(true)
    {
      const vector<int> &vars = quantifier_prefix[cnt].first;
      merged_vars.insert(merged_vars.end(), vars.begin(), vars.end());
      if(cnt == quantifier_prefix.size() - 1)
        break;
      if(quantifier_prefix[cnt].second != quantifier_prefix[cnt+1].second)
        break;
      ++cnt;
    }
    if(quantifier_prefix[cnt].second == E)
      pr.push_back(make_pair(merged_vars, RAReQS_EXISTS));
    else
      pr.push_back(make_pair(merged_vars, RAReQS_FORALL));
  }
  return RAReQSisSatModel(pr, cnf.getClauses(), model);
}

// -------------------------------------------------------------------------------------------
bool RareqsApi::isSatModel(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                           const CNF& cnf,
                           vector<int> &model)
{
  VarManager &VM = VarManager::instance();
  vector<pair<vector<int>, RAReQSQuantifierType> > pr;
  for(size_t cnt = 0; cnt < quantifier_prefix.size(); ++cnt)
  {
    vector<int> merged_vars;
    while(true)
    {
      const vector<int> &vars = VM.getVarsOfType(quantifier_prefix[cnt].first);
      merged_vars.insert(merged_vars.end(), vars.begin(), vars.end());
      if(cnt == quantifier_prefix.size() - 1)
        break;
      if(quantifier_prefix[cnt].second != quantifier_prefix[cnt+1].second)
        break;
      ++cnt;
    }
    if(quantifier_prefix[cnt].second == E)
      pr.push_back(make_pair(merged_vars, RAReQS_EXISTS));
    else
      pr.push_back(make_pair(merged_vars, RAReQS_FORALL));
  }
  return RAReQSisSatModel(pr, cnf.getClauses(), model);
}

// -------------------------------------------------------------------------------------------
bool RareqsApi::isSatModel(const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                           const CNF& cnf,
                           vector<int> &model)
{
  vector<pair<vector<int>, RAReQSQuantifierType> > pr;
  for(size_t cnt = 0; cnt < quantifier_prefix.size(); ++cnt)
  {
    vector<int> merged_vars;
    while(true)
    {
      const vector<int> &vars = quantifier_prefix[cnt].first;
      merged_vars.insert(merged_vars.end(), vars.begin(), vars.end());
      if(cnt == quantifier_prefix.size() - 1)
        break;
      if(quantifier_prefix[cnt].second != quantifier_prefix[cnt+1].second)
        break;
      ++cnt;
    }
    if(quantifier_prefix[cnt].second == E)
      pr.push_back(make_pair(merged_vars, RAReQS_EXISTS));
    else
      pr.push_back(make_pair(merged_vars, RAReQS_FORALL));
  }
  return RAReQSisSatModel(pr, cnf.getClauses(), model);
}
