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
/// @file CNFImplExtractor.cpp
/// @brief Contains the definition of the class CNFImplExtractor.
// -------------------------------------------------------------------------------------------


#include "CNFImplExtractor.h"
#include "CNF.h"
#include "Options.h"
#include "VarManager.h"
#include "StringUtils.h"
#include "Stopwatch.h"
#include "Logger.h"

extern "C" {
  #include "aiger.h"

// -------------------------------------------------------------------------------------------
///
/// @struct aiger_private
/// @brief We redefine this structure so that we can resize it without reallocating too often.
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
}

// -------------------------------------------------------------------------------------------
CNFImplExtractor::CNFImplExtractor() :
    extraction_cpu_time_(0.0),
    extraction_real_time_(0)
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
CNFImplExtractor::~CNFImplExtractor()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
void CNFImplExtractor::extractCircuit(const CNF &winning_region)
{
  PointInTime start_time = Stopwatch::start();
  CNF simplified_winning_region(winning_region);
  Utils::compressStateCNF(simplified_winning_region, true);
  CNF neg_winning_region(simplified_winning_region);
  neg_winning_region.negate();
  // We can also compute a negation without introducing temporary variables, but this is
  // expensive:
  //Utils::negateStateCNF(neg_winning_region);
  //Utils::compressStateCNF(neg_winning_region, false);
  run(simplified_winning_region, neg_winning_region);
  extraction_cpu_time_ = Stopwatch::getCPUTimeSec(start_time);
  extraction_real_time_ = Stopwatch::getRealTimeSec(start_time);
}

// -------------------------------------------------------------------------------------------
void CNFImplExtractor::extractCircuit(const CNF &winning_region,
                                      const CNF &neg_winning_region)
{
  PointInTime start_time = Stopwatch::start();
  run(winning_region, neg_winning_region);
  extraction_cpu_time_ = Stopwatch::getCPUTimeSec(start_time);
  extraction_real_time_ = Stopwatch::getRealTimeSec(start_time);
}

// -------------------------------------------------------------------------------------------
aiger* CNFImplExtractor::optimizeWithABC(aiger *circuit) const
{
  //string dbg_in_file = Options::instance().getUniqueTmpFileName("optimize_me") + ".aag";
  //int succ = aiger_open_and_write_to_file(circuit, dbg_in_file.c_str());
  string tmp_in_file = Options::instance().getUniqueTmpFileName("optimize_me") + ".aig";
  int succ = aiger_open_and_write_to_file(circuit, tmp_in_file.c_str());
  MASSERT(succ != 0, "Could not write out AIGER file for optimization with ABC.");

  // optimize circuit:
  string path_to_abc = Options::instance().getTPDirName() + "/abc/abc/abc";
  string tmp_out_file = Options::instance().getUniqueTmpFileName("optimized") + ".aig";
  string abc_command = path_to_abc + " -c \"";
  abc_command += "read_aiger " + tmp_in_file + ";";
  abc_command += " strash; refactor -zl; rewrite -zl;";
  if(circuit->num_ands < 1000000)
    abc_command += " strash; refactor -zl; rewrite -zl;";
  if(circuit->num_ands < 200000)
    abc_command += " strash; refactor -zl; rewrite -zl;";
  if(circuit->num_ands < 200000)
    abc_command += " dfraig; rewrite -zl; dfraig;";
  abc_command += " write_aiger -s " + tmp_out_file + "\"";
  abc_command += " > /dev/null 2>&1";
  int ret = system(abc_command.c_str());
  ret = WEXITSTATUS(ret);
  MASSERT(ret == 0, "ABC terminated with strange return code.");

  // read back the result:
  aiger *res = aiger_init();
  const char *err = aiger_open_and_read_from_file (res, tmp_out_file.c_str());
  MASSERT(err == NULL, "Could not open optimized AIGER file "
          << tmp_out_file << " (" << err << ").");
  remove(tmp_in_file.c_str());
  remove(tmp_out_file.c_str());
  return res;
}

// -------------------------------------------------------------------------------------------
size_t CNFImplExtractor::insertIntoSpec(aiger *standalone_circuit) const
{
  const vector<int> &ctrl = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  const vector<int> &in = VarManager::instance().getVarsOfType(VarInfo::INPUT);
  const vector<int> &pres = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE);
  MASSERT(standalone_circuit->num_inputs == pres.size() + in.size() - 1, "Wrong inputs.");
  MASSERT(standalone_circuit->num_outputs == ctrl.size(), "Wrong outputs.");

  // 1.: build the literal-renaming map
  int max_lit = 2*(standalone_circuit->maxvar + 1);
  vector<int> rename_map(max_lit + 1, -1);
  rename_map[0] = 0;
  rename_map[1] = 1;

  // 1a.: renaming of the inputs of the stand-alone circuit:
  vector<int> ip;
  ip.reserve(in.size() + pres.size());
  ip.insert(ip.end(), in.begin(), in.end());
  ip.insert(ip.end(), pres.begin(), pres.end());
  int in_cnt = 0;
  for(size_t cnt = 0; cnt < ip.size(); ++cnt)
  {
    if(ip[cnt] != VarManager::instance().getPresErrorStateVar())
    {
      unsigned from_aig_lit = standalone_circuit->inputs[in_cnt].lit;
      unsigned to_aig_lit = VarManager::instance().getInfo(ip[cnt]).getLitInAIG();
      rename_map[from_aig_lit] = to_aig_lit;
      rename_map[from_aig_lit+1] = to_aig_lit+1;
      in_cnt++;
    }
  }

  // 1b.: renaming of the outputs of the AND gates:
  int to_aig_lit = VarManager::instance().getMaxAIGVar() * 2 + 2;
  for(unsigned cnt = 0; cnt < standalone_circuit->num_ands; ++cnt)
  {
    unsigned from_aig_lit = standalone_circuit->ands[cnt].lhs;
    rename_map[from_aig_lit] = to_aig_lit;
    rename_map[from_aig_lit+1] = to_aig_lit+1;
    to_aig_lit += 2;
  }
  unsigned new_max_aig_var = (to_aig_lit - 2)/2;

  // 1c.: some of the outputs can be connected to the spec-signals by renaming:
  // If a control signal (an output of the stand-alone circuit) is an unnegated output of an
  // AND gate, then we can connect it to the spec signal simply by renaming:
  vector<unsigned> connect_out_indices;
  set<unsigned> already_renamed;
  connect_out_indices.reserve(standalone_circuit->num_outputs);
  for(unsigned cnt = 0; cnt < standalone_circuit->num_outputs; ++cnt)
  {
    unsigned output_lit = standalone_circuit->outputs[cnt].lit;
    if(!aiger_sign(output_lit) &&
        output_lit > (standalone_circuit->num_inputs*2+1) &&
        already_renamed.count(output_lit) == 0)
    {
      // this output of the stand-alone circuit is unnegated and it is defined by an AND
      // gate.
      int to_lit = VarManager::instance().getInfo(ctrl[cnt]).getLitInAIG();
      rename_map[output_lit] = to_lit;
      rename_map[output_lit+1] = to_lit+1;
      already_renamed.insert(output_lit);
    }
    else
      connect_out_indices.push_back(cnt);
  }

  // 2.: Insert the stand-alone circuit into the spec:
  const string &in_file = Options::instance().getAigInFileName();
  const string &out_file = Options::instance().getAigOutFileName();
  const char *error = NULL;
  aiger *aig = aiger_init();
  error = aiger_open_and_read_from_file (aig, in_file.c_str());
  MASSERT(error == NULL, "Could not open AIGER file " << in_file << " (" << error << ").");

  // 2.a.: remove controllable inputs:
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

  // 2.b.: add AND gates from standalone_circuit:
  unsigned new_size = aig->num_ands + standalone_circuit->num_ands + connect_out_indices.size();
  aig->ands = static_cast<aiger_and*>(realloc(aig->ands, new_size * sizeof(aiger_and)));
  unsigned next_and_idx = aig->num_ands;
  for(unsigned cnt = 0; cnt < standalone_circuit->num_ands; ++cnt)
  {
    aig->ands[next_and_idx].lhs = rename_map[standalone_circuit->ands[cnt].lhs];
    aig->ands[next_and_idx].rhs0 = rename_map[standalone_circuit->ands[cnt].rhs0];
    aig->ands[next_and_idx].rhs1 = rename_map[standalone_circuit->ands[cnt].rhs1];
    next_and_idx++;
  }

  // 2.c.: add additional AND gates to connect the output of the standalone circuit
  // to the controllable inputs:
  for(size_t cnt = 0; cnt < connect_out_indices.size(); ++cnt)
  {
    unsigned output_lit = standalone_circuit->outputs[connect_out_indices[cnt]].lit;
    // this output of the stand-alone circuit is negated of not defined by an AND
    // gate. Hence, we need to connect it with an additional AND gate.
    int to_lit = VarManager::instance().getInfo(ctrl[connect_out_indices[cnt]]).getLitInAIG();
    aig->ands[next_and_idx].lhs = to_lit;
    aig->ands[next_and_idx].rhs0 = rename_map[output_lit];
    aig->ands[next_and_idx].rhs1 = 1;
    next_and_idx++;
  }

  // 3.: Update the size of the regions:
  aig->num_ands = new_size;
  priv->size_ands = new_size;
  aig->maxvar = new_max_aig_var;

  // 4.: Write out the result:
  if(out_file == "stdout")
  {
    int success = aiger_write_to_file(aig, aiger_ascii_mode, stdout);
    MASSERT(success, "Could not write result file.");
  }
  else
  {
    int success = aiger_open_and_write_to_file(aig, out_file.c_str());
    MASSERT(success, "Could not write result file.");
  }
  aiger_reset(aig);
  return standalone_circuit->num_ands + connect_out_indices.size();
}

// -------------------------------------------------------------------------------------------
void CNFImplExtractor::logStatistics()
{
  L_LOG("Relation determinization time: " << extraction_cpu_time_ << " sec CPU time.");
  L_LOG("Relation determinization time: " << extraction_real_time_ << " sec real time.");
  logDetailedStatistics();
}

// -------------------------------------------------------------------------------------------
void CNFImplExtractor::logDetailedStatistics()
{
  // Override this method if you want to include more detailed statistics.
}

