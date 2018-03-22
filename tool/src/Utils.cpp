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
/// @file Utils.cpp
/// @brief Contains the definition of the class Utils.
// -------------------------------------------------------------------------------------------

#include "Utils.h"
#include "VarManager.h"
#include "Logger.h"
#include "CNF.h"
#include "SatSolver.h"
#include "QBFSolver.h"
#include "AIG2CNF.h"
#include "Options.h"
#include <unistd.h>

// -------------------------------------------------------------------------------------------
bool Utils::containsInit(const vector<int> &cube)
{
  for(size_t cnt = 0; cnt < cube.size(); ++cnt)
  {
    if(cube[cnt] > 0)
      return false;
  }
  return true;
}

// -------------------------------------------------------------------------------------------
bool Utils::containsInit(const set<int> &cube)
{
  for(set<int>::const_iterator it = cube.begin(); it != cube.end(); ++it)
  {
    if((*it) > 0)
      return false;
  }
  return true;
}

// -------------------------------------------------------------------------------------------
vector<int> Utils::extract(const vector<int> &cube_or_clause, VarInfo::VarKind kind)
{
  const vector<int> &rel_vars = VarManager::instance().getVarsOfType(kind);
  vector<int> res;
  res.reserve(cube_or_clause.size());
  for(size_t c1 = 0; c1 < cube_or_clause.size(); ++c1)
  {
    int var = cube_or_clause[c1];
    if(var < 0)
      var = -var;
    for(size_t c2 = 0; c2 < rel_vars.size(); ++c2)
    {
      if(var == rel_vars[c2])
      {
        res.push_back(cube_or_clause[c1]);
        break;
      }
    }
  }
  return res;
}

// -------------------------------------------------------------------------------------------
vector<int> Utils::extractPresIn(const vector<int> &cube_or_clause)
{
  const vector<int> &ps = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &in = VarManager::instance().getVarsOfType(VarInfo::INPUT);

  vector<int> res;
  res.reserve(cube_or_clause.size());
  for(size_t c1 = 0; c1 < cube_or_clause.size(); ++c1)
  {
    int var = cube_or_clause[c1];
    if(var < 0)
      var = -var;
    bool found = false;
    for(size_t c2 = 0; c2 < ps.size(); ++c2)
    {
      if(var == ps[c2])
      {
        res.push_back(cube_or_clause[c1]);
        found = true;
        break;
      }
    }
    if(!found)
    {
      for(size_t c2 = 0; c2 < in.size(); ++c2)
      {
        if(var == in[c2])
        {
          res.push_back(cube_or_clause[c1]);
          break;
        }
      }
    }
  }
  return res;
}

// -------------------------------------------------------------------------------------------
vector<int> Utils::extractNextAsPresent(const vector<int> &cube_or_clause)
{
  const vector<int> &ps = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &ns = VarManager::instance().getVarsOfType(VarInfo::NEXT_STATE);

  vector<int> res;
  res.reserve(cube_or_clause.size());
  for(size_t c1 = 0; c1 < cube_or_clause.size(); ++c1)
  {
    int var = cube_or_clause[c1];
    if(var < 0)
      var = -var;
    for(size_t c2 = 0; c2 < ns.size(); ++c2)
    {
      if(var == ns[c2])
      {
        res.push_back(cube_or_clause[c1] > 0 ? ps[c2] : -ps[c2]);
        break;
      }
    }
  }
  return res;
}

// -------------------------------------------------------------------------------------------
vector<int> Utils::extract(const vector<int> &cube_or_clause, const vector<int> &vars)
{
  vector<int> res;
  res.reserve(cube_or_clause.size());
  for(size_t cnt = 0; cnt < cube_or_clause.size(); ++cnt)
  {
    int lit = cube_or_clause[cnt];
    int var = lit < 0 ? -lit : lit;
    if(contains(vars, var))
      res.push_back(lit);
  }
  return res;
}

// -------------------------------------------------------------------------------------------
void Utils::randomize(vector<int> &vec)
{
  random_shuffle(vec.begin(),vec.end());
}

// -------------------------------------------------------------------------------------------
void Utils::sort(vector<int> &vec)
{
  std::sort(vec.begin(), vec.end());
}

// -------------------------------------------------------------------------------------------
bool Utils::remove(vector<int> &vec, int elem)
{
  for(size_t cnt = 0; cnt < vec.size(); ++cnt)
  {
    if(vec[cnt] == elem)
    {
      vec[cnt] = vec.back();
      vec.pop_back();
      return true;
    }
  }
  return false;
}

// -------------------------------------------------------------------------------------------
bool Utils::contains(const vector<int> &vec, int elem)
{
  for(size_t cnt = 0; cnt < vec.size(); ++cnt)
  {
    if(vec[cnt] == elem)
      return true;
  }
  return false;
}

// -------------------------------------------------------------------------------------------
void Utils::negateLiterals(vector<int> &cube_or_clause)
{
  for(size_t cnt = 0; cnt < cube_or_clause.size(); ++cnt)
    cube_or_clause[cnt] = -cube_or_clause[cnt];
}

// -------------------------------------------------------------------------------------------
void Utils::swapPresentToNext(vector<int> &vec)
{
  VarManager &VM = VarManager::instance();
  vector<int> olt_to_new_temp_var_map(VM.getMaxCNFVar()+1, 0);
  const vector<int>& ps_vars = VM.getVarsOfType(VarInfo::PRES_STATE);
  const vector<int>& ns_vars = VM.getVarsOfType(VarInfo::NEXT_STATE);
  MASSERT(ps_vars.size() == ns_vars.size(), "Sizes do not match.");
  for(size_t cnt = 0; cnt < ps_vars.size(); ++cnt)
    olt_to_new_temp_var_map[ps_vars[cnt]] = ns_vars[cnt];

  for(size_t lit_cnt = 0; lit_cnt < vec.size(); ++lit_cnt)
  {
    int old_var = (vec[lit_cnt] < 0) ? -vec[lit_cnt] : vec[lit_cnt];
    if(vec[lit_cnt] < 0)
      vec[lit_cnt] = -olt_to_new_temp_var_map[old_var];
    else
      vec[lit_cnt] = olt_to_new_temp_var_map[old_var];
  }
}

// -------------------------------------------------------------------------------------------
bool Utils::intersectionEmpty(const vector<int> &x, const set<int> &y)
{
  vector<int>::const_iterator i = x.begin();
  set<int>::const_iterator j = y.begin();
  while (i != x.end() && j != y.end())
  {
    if (*i == *j)
      return false;
    else if (*i < *j)
      ++i;
    else
      ++j;
  }
  return true;
}

// -------------------------------------------------------------------------------------------
bool Utils::intersectionEmpty(const set<int> &x, const set<int> &y)
{
  set<int>::const_iterator i = x.begin();
  set<int>::const_iterator j = y.begin();
  while (i != x.end() && j != y.end())
  {
    if (*i == *j)
      return false;
    else if (*i < *j)
      ++i;
    else
      ++j;
  }
  return true;
}

// -------------------------------------------------------------------------------------------
bool Utils::isSubset(const vector<int> &subset, const vector<int> &superset)
{
  if(subset.size() > superset.size())
    return false;
  for(size_t cnt1 = 0; cnt1 < subset.size(); ++cnt1)
  {
    bool found = false;
    int to_find = subset[cnt1];
    for(size_t cnt2 = 0; cnt2 < superset.size(); ++cnt2)
    {
      if(superset[cnt2] == to_find)
      {
        found = true;
        break;
      }
    }
    if(!found)
      return false;
  }
  return true;
}

// -------------------------------------------------------------------------------------------
bool Utils::compressStateCNF(CNF &cnf, bool hardcore)
{
  if(cnf.getNrOfClauses() == 0)
    return false;

  // size_t or_cl = cnf.getNrOfClauses();
  // size_t or_lits = cnf.getNrOfLits();

  bool something_changed = false;

  SatSolver *solver = Options::instance().getSATSolver(false, true);

  if(hardcore)
  {
    // remove literals from clauses before we turn to removing clauses
    list<vector<int> > clauses = cnf.getClauses();
    vector<int> none;
    solver->startIncrementalSession(VarManager::instance().getVarsOfType(VarInfo::PRES_STATE),
                                    false);
    solver->incAddCNF(cnf);
    cnf.clear();
    for(CNF::ClauseIter it = clauses.begin(); it != clauses.end(); ++it)
    {
      vector<int> neg_clause(*it);
      negateLiterals(neg_clause);
      vector<int> smaller_neg_clause;
      bool sat = solver->incIsSatModelOrCore(neg_clause,none,smaller_neg_clause);
      MASSERT(!sat, "Impossible.");
      cnf.addNegCubeAsClause(smaller_neg_clause);
    }
  }

  solver->startIncrementalSession(VarManager::instance().getVarsOfType(VarInfo::PRES_STATE),
                                  false);
  CNF still_to_process(cnf);
  cnf.clear();
  vector<int> first = still_to_process.removeSmallest();
  cnf.addClause(first);
  solver->incAddClause(first);
  while(still_to_process.getNrOfClauses() != 0)
  {
    vector<int> check_clause = still_to_process.removeSmallest();
    vector<int> check_cube(check_clause);
    negateLiterals(check_cube);
    if(solver->incIsSat(check_cube))
    {
      solver->incAddClause(check_clause);
      cnf.addClause(check_clause);
    }
    else
      something_changed = true;
  }
  delete solver;
  solver = NULL;
  // if(something_changed)
  // {
  //   size_t new_cl = cnf.getNrOfClauses();
  //   size_t new_lits = cnf.getNrOfLits();
  //   L_DBG("compressCNFs(): " << or_cl << " --> " << new_cl << " clauses.");
  //   L_DBG("compressCNFs(): " << or_lits << " --> " << new_lits << " literals.");
  // }
  return something_changed;

}

// -------------------------------------------------------------------------------------------
void Utils::negateStateCNF(CNF &cnf)
{
  CNF orig_cnf(cnf);
  CNF cnf_to_learn(cnf);
  VarManager::instance().push();
  cnf_to_learn.negate();
  cnf.clear();
  const vector<int> &vars = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE);

  SatSolver *find_solver = Options::instance().getSATSolver(false, true);
  find_solver->startIncrementalSession(vars, false);
  find_solver->incAddCNF(orig_cnf);
  SatSolver *gen_solver = Options::instance().getSATSolver(false, true);
  gen_solver->startIncrementalSession(vars, false);
  gen_solver->incAddCNF(cnf_to_learn);

  vector<int> model;
  vector<int> none;
  while(true)
  {
    bool sat = find_solver->incIsSatModelOrCore(none, vars, model);
    if(!sat)
      break;
    //Generalize model as long as model implies orig_cnf
    //                 as long as model & cnf_to_learn is unsat
    vector<int> core;
    sat = gen_solver->incIsSatModelOrCore(model, vars, core);
    MASSERT(!sat, "Impossible.");
    cnf.addNegCubeAsClause(core);
    find_solver->incAddNegCubeAsClause(core);
  }
  VarManager::instance().pop();
  delete find_solver;
  find_solver = NULL;
  delete gen_solver;
  gen_solver = NULL;
}

// -------------------------------------------------------------------------------------------
void Utils::debugPrint(const vector<int> &vec, string prefix)
{
  ostringstream oss;
  oss << "[";
  for(size_t count = 0; count < vec.size(); ++count)
  {
    oss << vec[count];
    if(count != vec.size() - 1)
      oss << ", ";
  }
  oss << "]";
  L_DBG(prefix << oss.str());
}

// -------------------------------------------------------------------------------------------
void Utils::debugCheckWinReg(const CNF &winning_region)
{
#ifndef NDEBUG
  CNF neg_winnning_region(winning_region);
  neg_winnning_region.negate();
  debugCheckWinReg(winning_region, neg_winnning_region);
#endif
}

// -------------------------------------------------------------------------------------------
void Utils::debugCheckWinReg(const CNF &winning_region, const CNF &neg_winning_region)
{
#ifndef NDEBUG
  L_DBG("Checking winning region for correctness ...");

  // The initial state must be contained in the winning region:
  SatSolver *sat_solver = Options::instance().getSATSolver();
  CNF check(winning_region);
  check.addCNF(AIG2CNF::instance().getInitial());
  MASSERT(sat_solver->isSat(check), "Initial state is not winning.");

  // All states of the winning region must be safe
  check.clear();
  check.addCNF(winning_region);
  check.addCNF(AIG2CNF::instance().getUnsafeStates());
  MASSERT(!sat_solver->isSat(check), "Winning region is not safe.");

  // from the winning region, we can enforce to stay in the winning region:
  // exists x,i: forall c: exists x': W(x) & T(x,i,c,x') & !W(x) must be unsat
  bool check_with_qbf = false;
  if(check_with_qbf)
  {
    QBFSolver *qbf_solver = Options::instance().getQBFSolver();
    check.clear();
    check.addCNF(neg_winning_region);
    check.swapPresentToNext();
    check.addCNF(winning_region);
    check.addCNF(AIG2CNF::instance().getTrans());
    vector<pair<VarInfo::VarKind, QBFSolver::Quant> > check_quant;
    check_quant.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::E));
    check_quant.push_back(make_pair(VarInfo::INPUT, QBFSolver::E));
    check_quant.push_back(make_pair(VarInfo::CTRL, QBFSolver::A));
    check_quant.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::E));
    check_quant.push_back(make_pair(VarInfo::TMP, QBFSolver::E));
    MASSERT(!qbf_solver->isSat(check_quant, check), "Winning region is not inductive.");
    delete qbf_solver;
    qbf_solver = NULL;
  }
  else
  {
    // SAT-based check:
    const vector<int> &in = VarManager::instance().getVarsOfType(VarInfo::INPUT);
    const vector<int> &pres = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE);
    const vector<int> &ctrl = VarManager::instance().getVarsOfType(VarInfo::CTRL);
    vector<int> sic;
    sic.reserve(pres.size() + in.size() + ctrl.size());
    sic.insert(sic.end(), pres.begin(), pres.end());
    sic.insert(sic.end(), in.begin(), in.end());
    sic.insert(sic.end(), ctrl.begin(), ctrl.end());
    vector<int> si;
    si.reserve(pres.size() + in.size());
    si.insert(si.end(), pres.begin(), pres.end());
    si.insert(si.end(), in.begin(), in.end());
    CNF check_cnf(neg_winning_region);
    check_cnf.swapPresentToNext();
    check_cnf.addCNF(winning_region);
    check_cnf.addCNF(AIG2CNF::instance().getTrans());
    SatSolver *check_solver = Options::instance().getSATSolver(false, true);
    check_solver->startIncrementalSession(sic, false);
    check_solver->incAddCNF(check_cnf);
    CNF gen_cnf(winning_region);
    gen_cnf.swapPresentToNext();
    gen_cnf.addCNF(winning_region);
    gen_cnf.addCNF(AIG2CNF::instance().getTrans());
    SatSolver *gen_solver = Options::instance().getSATSolver(false, true);
    gen_solver->startIncrementalSession(sic, false);
    gen_solver->incAddCNF(gen_cnf);

    vector<int> model_or_core;
    while(true)
    {
      bool sat = check_solver->incIsSatModelOrCore(vector<int>(), si, model_or_core);
      if(!sat)
        break;
      vector<int> state_input = model_or_core;
      vector<int> state = Utils::extract(state_input, VarInfo::PRES_STATE);
      vector<int> input = Utils::extract(state_input, VarInfo::INPUT);

      sat = gen_solver->incIsSatModelOrCore(state, input, ctrl, model_or_core);
      MASSERT(sat, "Winning region is not inductive.");

      vector<int> resp(model_or_core);
      sat = check_solver->incIsSatModelOrCore(state_input, resp, vector<int>(), model_or_core);
      MASSERT(sat == false, "Impossible.");
      check_solver->incAddNegCubeAsClause(model_or_core);
    }
    delete check_solver;
    check_solver = NULL;
    delete gen_solver;
    gen_solver = NULL;
  }

  delete sat_solver;
  sat_solver = NULL;

  L_DBG("All checks passed.");
#endif
}

// -------------------------------------------------------------------------------------------
void Utils::debugPrintCurrentMemUsage()
{
  using std::ios_base;
  using std::ifstream;
  using std::string;

  double vm_usage     = 0.0;
  double resident_set = 0.0;

  // 'file' stat seems to give the most reliable results
  //
  ifstream stat_stream("/proc/self/stat",ios_base::in);

  // dummy vars for leading entries in stat that we don't care about
  //
  string pid, comm, state, ppid, pgrp, session, tty_nr;
  string tpgid, flags, minflt, cminflt, majflt, cmajflt;
  string utime, stime, cutime, cstime, priority, nice;
  string O, itrealvalue, starttime;

  // the two fields we want
  //
  unsigned long vsize;
  long rss;

  stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
              >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
              >> utime >> stime >> cutime >> cstime >> priority >> nice
              >> O >> itrealvalue >> starttime >> vsize >> rss;

  stat_stream.close();

  long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024;
  resident_set = rss * page_size_kb;
  L_DBG("Resident set: " << resident_set << " kB.");
  vm_usage     = vsize / 1024.0;
  L_DBG("Virtual Memory: " << vm_usage << " kB.");
}

