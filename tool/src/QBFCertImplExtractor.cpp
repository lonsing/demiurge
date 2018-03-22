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
/// @file QBFCertImplExtractor.cpp
/// @brief Contains the definition of the class QBFCertImplExtractor.
// -------------------------------------------------------------------------------------------

#include "QBFCertImplExtractor.h"
#include "VarManager.h"
#include "AIG2CNF.h"
#include "CNF.h"
#include "DepQBFExt.h"
#include "StringUtils.h"
#include "Options.h"
#include "Logger.h"
#include "FileUtils.h"

extern "C" {
  #include "aiger.h"

// -------------------------------------------------------------------------------------------
///
/// @struct aiger_private
/// @brief Redefinition of the aiger_private struct to be able to modify it.
///
/// Modifying the read AIGER structure (removing inputs, adding AND gates)
/// requires modifying the private AIGER structure in the back-ground in
/// order not to corrupt the memory. In order not to modify the original AIGER
/// sources, we re-define the structure here.
  struct aiger_private
  {
    aiger publ;
    void *types;            /* [0..maxvar] */
    unsigned size_types;
    unsigned char * coi;
    unsigned size_coi;
    unsigned size_inputs;
    unsigned size_latches;
    unsigned size_outputs;
    unsigned size_ands;
    unsigned size_bad;
    unsigned size_constraints;
    unsigned size_justice;
    unsigned size_fairness;
    unsigned num_comments;
    unsigned size_comments;
    void *memory_mgr;
    void* malloc_callback;
    void* free_callback;
    char *error;
  };

// -------------------------------------------------------------------------------------------
///
/// @typedef struct aiger_private aiger_private
/// @brief An alias for aiger_private structs.
  typedef struct aiger_private aiger_private;
}

// -------------------------------------------------------------------------------------------
QBFCertImplExtractor::QBFCertImplExtractor()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
QBFCertImplExtractor::~QBFCertImplExtractor()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
void QBFCertImplExtractor::extractCircuit(const CNF &winning_region)
{
  CNF simplified_winning_region(winning_region);
  Utils::compressStateCNF(simplified_winning_region, true);
  CNF neg_winning_region(simplified_winning_region);
  neg_winning_region.negate();
  extractCircuit(simplified_winning_region, neg_winning_region);
}

// -------------------------------------------------------------------------------------------
void QBFCertImplExtractor::extractCircuit(const CNF &winning_region,
                                          const CNF &neg_winning_region)
{
  // We would like to find skolem functions for the c signals in:
  // forall x,i: exists c,x': (NOT W(x)) OR ( T(x,i,c,x') AND W(x')),
  // where W(x) denotes the winning region.

  // Instead, we compute Herbrand functions for the c signals in:
  // exists x,i: forall c: exists x': W(x) AND T(x,i,c,x') AND NOT W(x')),
  // where W(x) denotes the winning region.

  CNF strategy(neg_winning_region);
  strategy.swapPresentToNext();
  strategy.addCNF(AIG2CNF::instance().getTrans());
  strategy.addCNF(winning_region);

  // build the quantifier prefix:
  vector<pair<VarInfo::VarKind, DepQBFExt::Quant> > quant;
  quant.push_back(make_pair(VarInfo::PRES_STATE, DepQBFExt::E));
  quant.push_back(make_pair(VarInfo::INPUT, DepQBFExt::E));
  quant.push_back(make_pair(VarInfo::CTRL, DepQBFExt::A));
  quant.push_back(make_pair(VarInfo::NEXT_STATE, DepQBFExt::E));
  quant.push_back(make_pair(VarInfo::TMP, DepQBFExt::E));

  // now we compute functions for the control signals c:
  DepQBFExt solver;
#ifndef NDEBUG
  MASSERT(solver.isSat(quant,strategy) == false, "Error in winning region.");
#endif
  string answer = solver.qbfCert(quant, strategy);

  // now, we only need to parse the returned aiger file:
  vector<vector<int> > and_gates;
  size_t max_idx = parseQBFCertAnswer(answer, and_gates);

  L_INF("Writing the result file ...");
  dumpResAigerLib(max_idx, and_gates);

}

// -------------------------------------------------------------------------------------------
size_t QBFCertImplExtractor::parseQBFCertAnswer(const string &answer,
                                                vector<vector<int> > &and_gates) const
{
  vector<string> lines;
  StringUtils::splitLines(answer, lines, false);
  MASSERT(lines.size() > 0, "QBFCert parse error.");
  istringstream header(lines[0]);
  size_t max_idx = 0, nr_in = 0, nr_l = 0, nr_out = 0, nr_gates = 0;
  string aag;
  header >> aag >> max_idx  >> nr_in >> nr_l >> nr_out >> nr_gates;
  MASSERT(lines.size() >= nr_in + nr_out + nr_gates, "QBFCert parse error.");
  MASSERT(nr_l == 0, "QBFCert parse error.");

  // let's build up a map to rename the variable indices:
  vector<int> rename_map(2*(max_idx + 1), -1);
  rename_map[0] = 0;
  rename_map[1] = 1;

  VarManager& VM = VarManager::instance();
  size_t next_line = 1;
  for(size_t cnt = 0; cnt < nr_in + nr_out; ++cnt)
  {
    istringstream nr(lines[next_line++]);
    int old_nr = 0;
    nr >> old_nr;
    int new_nr = VM.getInfo(old_nr >> 1).getLitInAIG();
    rename_map[old_nr] = new_nr;
    rename_map[old_nr+1] = new_nr + 1;
  }

  // now we are applying the map:
  const vector<int> &ctrl_cnf_vars = VM.getVarsOfType(VarInfo::CTRL);
  and_gates.reserve(nr_gates + ctrl_cnf_vars.size());
  int next_free_aig_var = VM.getMaxAIGVar()*2 + 2;
  L_DBG("Resulting AND gates:");
  for(size_t cnt = 0; cnt < nr_gates; ++cnt)
  {
    istringstream triple(lines[next_line++]);
    vector<int> and_gate(3,0);
    triple >> and_gate[0] >> and_gate[1] >> and_gate[2];
    for(size_t val_cnt = 0; val_cnt < 3; ++val_cnt)
    {
      int var_to_rename = and_gate[val_cnt];
      if(rename_map[var_to_rename] == -1)
      {
        if((var_to_rename & 1) != 0)
          var_to_rename -= 1;
        rename_map[var_to_rename] = next_free_aig_var;
        rename_map[var_to_rename+1] = next_free_aig_var+1;
        next_free_aig_var += 2;
      }
      and_gate[val_cnt] = rename_map[and_gate[val_cnt]];
    }
    and_gates.push_back(and_gate);
    L_DBG(and_gate[0] << " " << and_gate[1] << " " << and_gate[2]);
  }

  // if certain control signals are not defined by AND gates, then this means
  // that we can set them arbitrarily:
  for(size_t cnt = 0; cnt < ctrl_cnf_vars.size(); ++cnt)
  {
    int aig_var = VM.getInfo(ctrl_cnf_vars[cnt]).getLitInAIG();
    bool found = false;
    for(size_t cnt2 = 0; cnt2 < and_gates.size(); ++cnt2)
    {
      if(and_gates[cnt2][0] == aig_var)
      {
        found = true;
        break;
      }
    }
    if(!found)
    {
      L_DBG("AIGER-literal " << aig_var << " could be chosen arbitrarily.");
      vector<int> and_gate(3,0);
      and_gate[0] = aig_var;
      and_gates.push_back(and_gate);
    }
  }

  return (next_free_aig_var - 2) >> 1;
}

// -------------------------------------------------------------------------------------------
void QBFCertImplExtractor::dumpResAigerLib(size_t max_var_idx,
                                           const vector<vector<int> > &and_gates)
{
  const string &in_file = Options::instance().getAigInFileName();
  const string &out_file = Options::instance().getAigOutFileName();
  const char *error = NULL;
  aiger *aig = aiger_init();
  error = aiger_open_and_read_from_file (aig, in_file.c_str());
  MASSERT(error == NULL, "Could not open AIGER file " << in_file << " (" << error << ").");

  // remove controllable inputs:
  aiger_private * priv = (aiger_private*) (aig);
  for(unsigned cnt = 0; cnt < aig->num_inputs; ++cnt)
  {
    string name;
    if(aig->inputs[cnt].name != NULL)
      name = string(aig->inputs[cnt].name);
    StringUtils::toLowerCaseIn(name);
    if(name.find("controllable_") == 0)
    {
      free(aig->inputs[cnt].name);
      for(unsigned cnt2 = cnt; cnt2+1 < aig->num_inputs; ++cnt2)
        aig->inputs[cnt2] = aig->inputs[cnt2+1];
      aig->num_inputs--;
      priv->size_inputs--;
      cnt--;
    }
  }

  // add AND gates:
  unsigned new_size = aig->num_ands + and_gates.size();
  aig->ands = static_cast<aiger_and*>(realloc(aig->ands, new_size * sizeof(aiger_and)));
  for(unsigned cnt = 0; cnt < and_gates.size(); ++cnt)
  {
    unsigned idx = aig->num_ands + cnt;
    aig->ands[idx].lhs = and_gates[cnt][0];
    aig->ands[idx].rhs0 = and_gates[cnt][1];
    aig->ands[idx].rhs1 = and_gates[cnt][2];
  }
  aig->num_ands = new_size;
  priv->size_ands = new_size;
  unsigned new_max_v = aig->num_inputs + aig->num_latches + aig->num_ands;
  MASSERT(max_var_idx == new_max_v, "Error in max var index.");
  aig->maxvar = new_max_v;

  int success = aiger_open_and_write_to_file(aig, out_file.c_str());
  MASSERT(success, "Could not write result file.");
  aiger_reset(aig);
}
