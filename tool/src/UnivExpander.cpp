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
/// @file UnivExpander.cpp
/// @brief Contains the definition of the class UnivExpander.
// -------------------------------------------------------------------------------------------


#include "UnivExpander.h"
#include "VarManager.h"
#include "AIG2CNF.h"
#include "SatSolver.h"
#include "Options.h"
#include "stdint.h"
#include "Stopwatch.h"
#include "Logger.h"

#include <unordered_map>

extern "C" {
  #include "aiger.h"
}

namespace std
{
  /// @brief Provides a default implementation for computing the hash of a list of integers.
  ///
  /// This is used by the unordered_map to search for duplicate clauses.
  template <>
  struct hash<vector<int> >
  {
    /// @brief Computes the hash of a vector of integers.
    ///
    /// @param k The vector to compute the hash for.
    /// @return The hash of the vector
    std::size_t operator()(const vector<int>& k) const
    {
      size_t res = k.size();
      for(size_t cnt = 0; cnt < k.size(); ++cnt)
        res += k[cnt] * -1640531527;
      return res;
    }
  };

  /// @brief Provides a default implementation for comparing two vectors.
  ///
  /// This is used by the unordered_map to search for duplicate clauses.
  template <>
  struct equal_to<vector<int> > {

   /// @brief Compares two vectors for equality, assuming that they are sorted.
   ///
   /// @param lhs The first element for the comparison.
   /// @param rhs The second element for the comparison.
   /// @return True if equal (assuming the vectors to be sorted), False otherwise.
   bool operator()(const vector<int>& lhs, const vector<int>& rhs) const
   {
      if(lhs.size() != rhs.size())
        return false;
      // we assume that vectors are sorted
      // if vectors are not sorted, the hashmap algorithm is still correct but potentially
      // more inefficient.
      for(size_t cnt = 0; cnt < lhs.size(); ++cnt)
        if(lhs[cnt] != rhs[cnt])
          return false;
      return true;
   }
  };
}


// -------------------------------------------------------------------------------------------
UnivExpander::UnivExpander() :
              s_(VarManager::instance().getVarsOfType(VarInfo::PRES_STATE)),
              i_(VarManager::instance().getVarsOfType(VarInfo::INPUT)),
              c_(VarManager::instance().getVarsOfType(VarInfo::CTRL)),
              n_(VarManager::instance().getVarsOfType(VarInfo::NEXT_STATE)),
              i_max_trans_var_(0),
              abort_if_(NULL)
{
  // Nothing to do. Initialization of the data structures is done lazily on demand.
}

// -------------------------------------------------------------------------------------------
UnivExpander::~UnivExpander()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
void UnivExpander::univExpand(const CNF &orig, CNF &res, const vector<int> &vars_to_exp,
                              const vector<int> &keep, float rate,
                              const map<int, set<int> > *more_deps)
{
  const map<int, set<VarInfo> > &deps = AIG2CNF::instance().getTmpDepsTrans();
  typedef map<int, set<int> >::const_iterator DepsConstIter;
  map<int, set<int> > sdeps; // on which other variables does a certain variable depend on?
  // copied variables have the same dependencies as the originals so we use a new-to-old-map:
  map<int, int > sdeps_rename;
  for(map<int, set<VarInfo> >::const_iterator i1 = deps.begin(); i1 != deps.end(); ++i1)
  {
    sdeps_rename[i1->first] = i1->first;
    sdeps[i1->first] = set<int>();
    for(set<VarInfo>::const_iterator i2 = i1->second.begin(); i2 != i1->second.end(); ++i2)
      sdeps[i1->first].insert(i2->getLitInCNF());
  }
  if(more_deps != NULL)
  {
    for(DepsConstIter it = more_deps->begin(); it != more_deps->end(); ++it)
    {
      sdeps_rename[it->first] = it->first;
      sdeps[it->first] = it->second;
    }
  }
  int max_sdeps_idx = sdeps.rbegin()->first;

  set<int> all;
  orig.appendVarsTo(all);
  set<int> not_rename;
  for(size_t vc = 0; vc < keep.size(); vc++)
    not_rename.insert(keep[vc]);
  // check for variables that depend only on other variables that are not renamed:
  for(DepsConstIter it = sdeps.begin(); it != sdeps.end(); ++it)
    if(includes(keep.begin(), keep.end(), it->second.begin(), it->second.end()))
      not_rename.insert(it->first);
  for(size_t vc = 0; vc < vars_to_exp.size(); vc++)
    not_rename.insert(vars_to_exp[vc]);
  list<int> may_rename;
  list<int> must_rename;
  for(set<int>::const_iterator it = all.begin(); it != all.end(); ++it)
  {
    if(not_rename.count(*it) != 0)
      continue;
    DepsConstIter d = sdeps.find(*it);
    // if we do not have dependencies stored for a variable, then we must rename:
    if(d == sdeps.end())
      must_rename.push_back(*it);
    else if(!includes(not_rename.begin(), not_rename.end(), d->second.begin(), d->second.end()))
      must_rename.push_back(*it); // depends on at least one var outside {keep, vars_to_exp}
    else // depends only on keep and vars_to_exp
      may_rename.push_back(*it);
  }

  // count the number of variables to rename:
  vector<size_t> nr_of_vars_to_rename;
  nr_of_vars_to_rename.reserve(vars_to_exp.size());
  for(size_t cnt = 0; cnt < vars_to_exp.size(); ++cnt)
  {
    size_t vars_to_rename = 0;
    for(list<int>::const_iterator it = may_rename.begin(); it != may_rename.end(); ++it)
    {
      DepsConstIter ctrl_sdeps = sdeps.find(*it);
      if(ctrl_sdeps != sdeps.end())
        if(ctrl_sdeps->second.count(vars_to_exp[cnt]) == 0)
          continue;
      vars_to_rename++;
    }
    nr_of_vars_to_rename.push_back(vars_to_rename);
  }

  vector<int> sorted_vars_to_exp;
  while(sorted_vars_to_exp.size() < vars_to_exp.size())
  {
    // find the smallest number:
    size_t smallest_idx = 0;
    for(size_t cnt = 1; cnt < nr_of_vars_to_rename.size(); ++cnt)
      if(nr_of_vars_to_rename[cnt] < nr_of_vars_to_rename[smallest_idx])
        smallest_idx = cnt;
    sorted_vars_to_exp.push_back(vars_to_exp[smallest_idx]);
    nr_of_vars_to_rename[smallest_idx] = numeric_limits<size_t>::max();
  }


  int max_var = VarManager::instance().getMaxCNFVar();
  list<vector<int> > exp = orig.getClauses();

  int max_cnt = rate * sorted_vars_to_exp.size();
  if(max_cnt < 0)
    max_cnt = 0;
  else if(max_cnt > static_cast<int>(sorted_vars_to_exp.size()))
    max_cnt = sorted_vars_to_exp.size();
  for(int cnt = 0; cnt < max_cnt; ++cnt)
  {
    int ex = sorted_vars_to_exp[cnt];

    // find out which variables we really have to rename now:
    // first we create a cache of sdeps for 'ex' to avoid searching sdeps for ex a mio. times
    vector<bool> dep_on_ex(max_sdeps_idx + 1, false);
    for(DepsConstIter it = sdeps.begin(); it != sdeps.end(); ++it)
      if(it->second.count(ex) != 0)
        dep_on_ex[it->first] = true;

    vector<int> rename_map;
    rename_map.reserve(max_var + 1);
    for(int vc = 0; vc < max_var + 1; vc++)
      rename_map.push_back(vc);
    for(list<int>::const_iterator it = must_rename.begin(); it != must_rename.end(); ++it)
    {
      max_var++;
      rename_map[*it] = max_var;
      must_rename.push_front(max_var);
    }

    for(list<int>::const_iterator it = may_rename.begin(); it != may_rename.end(); ++it)
    {
      map<int, int >::const_iterator idx_in_sdeps = sdeps_rename.find(*it);
      DASSERT(idx_in_sdeps != sdeps_rename.end(), "Impossible. Must be in must_rename.");
      if(!dep_on_ex[idx_in_sdeps->second])
          continue;
      max_var++;
      rename_map[*it] = max_var;
      may_rename.push_front(max_var);
      sdeps_rename[max_var] = idx_in_sdeps->second;
    }

    // now we do the main work:
    // - see which clauses we really have to copy,
    // - merge them into exp,
    // - rename the copy while doing so
    // - set 'ex' to true in the copy
    // - set 'ex' to false in the original
    // all in one pass over the clauses for efficiency.
    size_t orig_nr_of_clauses_in_exp = exp.size();
    CNF::ClauseIter it = exp.begin();
    for(size_t cl_cnt = 0; cl_cnt < orig_nr_of_clauses_in_exp; ++cl_cnt)
    {
      vector<int> new_clause(*it);
      bool remove_copy = false;
      bool remove_orig = false;
      bool copy_identical = true;
      for(size_t lit_c = 0; lit_c < new_clause.size(); )
      {
        // rename all literals (ex will not be renamed):
        int old_lit = new_clause[lit_c];
        int new_lit = old_lit < 0 ? -rename_map[-old_lit] : rename_map[old_lit];
        new_clause[lit_c] = new_lit;
        if(old_lit != new_lit)
          copy_identical = false;
        // set ex to true in copy:
        if(new_lit == ex) // the clause is true in copy, literal is false in original
        {
          remove_copy = true;
          (*it)[lit_c] = it->back();
          it->pop_back();
          break;
        }
        else if(new_lit == -ex) // literal is false in copy, clause is true in original
        {
          new_clause[lit_c] = new_clause.back();
          new_clause.pop_back();
          copy_identical = false;
          remove_orig = true;
        }
        else
          lit_c++;
      }
      if(copy_identical)
        remove_copy = true;
      if(!remove_copy)
        exp.push_back(new_clause);

      if(remove_orig)
        it = exp.erase(it);
      else
        ++it;
    }
    // reduces the size but is (too?) expensive
    //CNF tmp;
    //tmp.swapWith(exp);
    //tmp.doPureAndUnit(keep);
    //tmp.swapWith(exp);
  }
  res.swapWith(exp);
  //cout << orig.getNrOfLits() <<  " --> " << res.getNrOfLits() << endl;
  //cout << " nr of clauses: " << res.getNrOfClauses() << endl;
  //cout << " nr of lits: " << res.getNrOfLits() << endl;
}

// -------------------------------------------------------------------------------------------
bool UnivExpander::resetSolverIExp(CNF &win_reg, SatSolver *solver_i, bool limit_size)
{
  vector<SatSolver*> solvers(1, solver_i);
  return resetSolverIExp(win_reg, solvers, limit_size);
}

// -------------------------------------------------------------------------------------------
bool UnivExpander::resetSolverIExp(CNF &win_reg, vector<SatSolver*> solvers, bool limit_size)
{
  // CNF exp_neg_win;
  if(i_trans_ands_.empty())
  {
    bool size_exceeded = initSolverIData(limit_size);
    if(size_exceeded)
      return size_exceeded;
  }

  CNF next_win_reg;
  Utils::compressNextStateCNF(win_reg, next_win_reg, false);

  // and initialize the solver:
  vector<int> vars_to_keep;
  vars_to_keep.reserve(s_.size() + i_.size());
  vars_to_keep.insert(vars_to_keep.end(), s_.begin(), s_.end());
  vars_to_keep.insert(vars_to_keep.end(), i_.begin(), i_.end());
  for(size_t s_cnt = 0; s_cnt < solvers.size(); ++s_cnt)
  {
    solvers[s_cnt]->startIncrementalSession(vars_to_keep, false);
    if(AIG2CNF::instance().isTrueInTrans())
      solvers[s_cnt]->incAddUnitClause(1);
    solvers[s_cnt]->incAddCNF(win_reg);
    for(size_t c = 0; c < i_trans_ands_.size(); ++c)
    {
      solvers[s_cnt]->incAdd2LitClause(-i_trans_ands_[c].l, i_trans_ands_[c].r1);
      solvers[s_cnt]->incAdd2LitClause(-i_trans_ands_[c].l, i_trans_ands_[c].r0);
      solvers[s_cnt]->incAdd3LitClause(i_trans_ands_[c].l, -i_trans_ands_[c].r1, -i_trans_ands_[c].r0);
    }
  }
  size_t literals_so_far = 6 * i_trans_ands_.size() * solvers.size();
  // now the only thing left in the solvers is the negation of the expanded winning region

  int max_n = 0;
  for(size_t cnt = 0; cnt < n_.size(); ++cnt)
    if(n_[cnt] > max_n)
      max_n = n_[cnt];
  vector<int> ren;
  ren.reserve(max_n + 1);
  for(int cnt = 0; cnt < max_n + 1; ++cnt)
    ren.push_back(cnt);
  int max_var = i_max_trans_var_;
  const list<vector<int> > &clauses = next_win_reg.getClauses();

  // we re-use the following vector across all iterations to save object creations and
  // deletions:
  vector<vector<int> > current_cnf;
  current_cnf.reserve(clauses.size());
  for(size_t cnt = 0; cnt < clauses.size(); ++cnt)
  {
    current_cnf.push_back(vector<int>());
    current_cnf.back().reserve(n_.size());
  }

  std::unordered_map<vector<int>, int > clause_to_neg_lit;
  vector<int> one_clause_false;
  one_clause_false.reserve(clauses.size());
  vector<int> is_unit(i_max_trans_var_ + 1, 0);
  vector<int> unit_vars; // used only to reset is_unit faster
  unit_vars.reserve(clauses.size());

  MASSERT(i_copy_maps_.size() < 64, "To many renamings. Must have been catched already.");
  size_t nr_of_copies = 1;
  for(size_t cnt = 0; cnt < i_copy_maps_.size(); ++cnt)
    nr_of_copies <<= 1;

  // Let's find out when we have to abort in order not to go out of memory:
  size_t kb_left = Utils::getCurrentMemUsage() - Options::instance().getSizeLimitForExpansion();
  size_t max_nr_of_literals = kb_left * 128; // 4 byte per literal + a safety margin for overhead

  for(size_t double_cnt = 0; double_cnt < nr_of_copies; ++double_cnt)
  {
    // build the second rename map for to apply after i_rename_maps_:
    vector<int> double_ren;
    if(i_copy_maps_.size() > 0)
    {
      if(double_cnt & 1)
        double_ren = i_copy_maps_[0];
      else
        double_ren = i_orig_prop_maps_[0];
      size_t mask = 2;
      for(size_t c0 = 1; c0 < i_copy_maps_.size(); ++c0)
      {
        if(double_cnt & mask)
        {
          for(set<int>::const_iterator it = i_occ_in_i_rename_maps_.begin(); it != i_occ_in_i_rename_maps_.end(); ++it)
          {
            int old = double_ren[*it];
            double_ren[*it] = old < 0 ? -i_copy_maps_[c0][-old] : i_copy_maps_[c0][old];
          }
        }
        else
        {
          for(set<int>::const_iterator it = i_occ_in_i_rename_maps_.begin(); it != i_occ_in_i_rename_maps_.end(); ++it)
          {
            int old = double_ren[*it];
            double_ren[*it] = old < 0 ? -i_orig_prop_maps_[c0][-old] : i_orig_prop_maps_[c0][old];
          }
        }
        mask <<= 1;
      }
    }

    // now we iteralte over all rename maps in i_rename_maps_ and apply the current double_ren
    // (if there is any):
    for(size_t copy_cnt = 0; copy_cnt < i_rename_maps_.size(); ++copy_cnt)
    {
      if(limit_size) // check if we already exceeded the limit:
      {
        if(literals_so_far > max_nr_of_literals)
        {
          cleanupIData();
          for(size_t s_cnt = 0; s_cnt < solvers.size(); ++s_cnt)
            solvers[s_cnt]->clearIncrementalSession();
          return true;
        }
      }

      for(size_t cnt = 0; cnt < n_.size(); ++cnt)
        ren[n_[cnt]] = i_rename_maps_[copy_cnt][cnt];
      if(i_copy_maps_.size() > 0)
      {
        for(size_t cnt = 0; cnt < n_.size(); ++cnt)
        {
          int old = ren[n_[cnt]];
          ren[n_[cnt]] = old < 0 ? -double_ren[-old] : double_ren[old];
        }
      }

      // this is faster than doing is_unit.assign(is_unit.size(), 0);
      for(size_t cnt = 0; cnt < unit_vars.size(); ++cnt)
        is_unit[unit_vars[cnt]] = 0;

      one_clause_false.clear();
      unit_vars.clear();

      // renaming and simplification:
      bool next_win_false = false;
      size_t current_nr_of_elems = 0;
      for(CNF::ClauseConstIter it = clauses.begin(); it != clauses.end(); ++it)
      {
        vector<int> &current_clause = current_cnf[current_nr_of_elems];
        current_clause.clear();
        bool clause_is_true = false;
        for(size_t lit_cnt = 0; lit_cnt < it->size(); ++lit_cnt)
        {
          int lit = (*it)[lit_cnt];
          int ren_lit = lit < 0 ? -ren[-lit] : ren[lit];
          if(ren_lit == 1)
          {
            clause_is_true = true;
            break;
          }
          else if(ren_lit != -1) // if a literal is false we can ignore it
          {
            // checking for tautological clauses eliminates almost nothing:
            // for(size_t neg_c = 0; neg_c < current_clause.size(); ++neg_c)
            // {
            //   if(current_clause[neg_c] == -ren_lit)
            //   {
            //     clause_is_true = true;
            //     break;
            //   }
            // }
            // if(clause_is_true)
            //   break;
            current_clause.push_back(ren_lit);
          }
        }
        if(!clause_is_true)
        {
          if(current_clause.empty()) // the clause is empty (i.e., false)
          {
            next_win_false = true;
            break;
          }
          else if(current_clause.size() == 1)
          {
            int li = current_clause[0];
            if(li < 0)
            {
              if(is_unit[-li] == 1) // unit clauses are contradicting
              {
                next_win_false = true;
                break;
              }
              is_unit[-li] = -1;
              unit_vars.push_back(-li);
            }
            else
            {
              if(is_unit[li] == -1) // unit clauses are contradicting
              {
                next_win_false = true;
                break;
              }
              is_unit[li] = 1;
              unit_vars.push_back(li);
            }
            one_clause_false.push_back(-li);
          }
          else
            current_nr_of_elems++;
        }
      }
      if(next_win_false)
        continue;

      // BEGIN simplification due to unit clauses:
      // This block can safely be commented out completely
      bool new_unit_discoverd = unit_vars.size() > 0;
      while(new_unit_discoverd && !next_win_false)
      {
        new_unit_discoverd = false;
        for(size_t cl_cnt = 0; cl_cnt < current_nr_of_elems; )
        {
          vector<int> &current_clause = current_cnf[cl_cnt];
          bool remove_clause = false;
          for(size_t lit_cnt = 0; lit_cnt < current_clause.size();)
          {
            int current_lit = current_clause[lit_cnt];
            if(current_lit < 0)
            {
              if(is_unit[-current_lit] == -1) // remove the clause
              {
                remove_clause = true;
                break;
              }
              else if(is_unit[-current_lit] == 1) // remove the literal
              {
                current_clause[lit_cnt] = current_clause.back();
                current_clause.pop_back();
              }
              else
                ++lit_cnt;
            }
            else
            {
              if(is_unit[current_lit] == 1) // remove the clause
              {
                remove_clause = true;
                break;
              }
              else if(is_unit[current_lit] == -1) // remove the literal
              {
                current_clause[lit_cnt] = current_clause.back();
                current_clause.pop_back();
              }
              else
                ++lit_cnt;
            }
          }
          if(remove_clause)
          {
            current_clause.swap(current_cnf[current_nr_of_elems - 1]);
            --current_nr_of_elems;
          }
          else if(current_clause.empty())
          {
            next_win_false = true;
            break;
          }
          else if(current_clause.size() == 1)
          {
            int li = current_clause[0];
            if(li < 0)
            {
              if(is_unit[-li] == 1) // unit clauses are contradicting
              {
                next_win_false = true;
                break;
              }
              is_unit[-li] = -1;
              unit_vars.push_back(-li);
            }
            else
            {
              if(is_unit[li] == -1) // unit clauses are contradicting
              {
                next_win_false = true;
                break;
              }
              is_unit[li] = 1;
              unit_vars.push_back(li);
            }
            new_unit_discoverd = true;
            one_clause_false.push_back(-li);
            current_clause.swap(current_cnf[current_nr_of_elems - 1]);
            --current_nr_of_elems;
          }
          else
            ++cl_cnt;
        }
      }
      if(next_win_false)
        continue;
      // END simplification due to unit clauses

      // actual negation:
      // the negation of the unit clauses have already been added to one_clause_false
      for(size_t cl_cnt = 0; cl_cnt < current_nr_of_elems; ++cl_cnt)
      {
        vector<int> &current_clause = current_cnf[cl_cnt];
        // Let's sort current_clause. This is selectionSort, taken from Minisat:
        size_t sz = current_clause.size(), best_i;
        int tmp;
        for(size_t i = 0; i < sz - 1; i++)
        {
          best_i = i;
          for(size_t j = i+1; j < sz; j++)
          {
            if(current_clause[j] < current_clause[best_i])
              best_i = j;
          }
          tmp = current_clause[i];
          current_clause[i] = current_clause[best_i];
          current_clause[best_i] = tmp;
        }

        pair<unordered_map<vector<int>, int>::const_iterator, bool> mapit;
        mapit = clause_to_neg_lit.emplace(current_clause, max_var + 1);
        if(!mapit.second) // element was already there
          one_clause_false.push_back(mapit.first->second);
        else
        {
          max_var++;
          int clause_false_lit = max_var;
          literals_so_far += 2 * current_clause.size() * solvers.size();
          for(size_t lit_cnt = 0; lit_cnt < current_clause.size(); ++lit_cnt)
            for(size_t s_cnt = 0; s_cnt < solvers.size(); ++s_cnt)
              solvers[s_cnt]->incAdd2LitClause(-clause_false_lit, -(current_clause[lit_cnt]));
          one_clause_false.push_back(clause_false_lit);
        }
      }
      for(size_t s_cnt = 0; s_cnt < solvers.size(); ++s_cnt)
        solvers[s_cnt]->incAddClause(one_clause_false);
      literals_so_far += one_clause_false.size() * solvers.size();
    }
  }

  return false;
}


// -------------------------------------------------------------------------------------------
void UnivExpander::initSolverCExp(SatSolver *solver_c,
                                  const vector<int> &keep,
                                  size_t max_nr_of_signals_to_expand)
{
  c_rename_maps_.clear();
  if(max_nr_of_signals_to_expand > i_.size())
    max_nr_of_signals_to_expand = i_.size();

  // we reverse-engineer the AND gates from the transition relation CNF
  // (constant propagation is easier on the AND gates than in the CNF)
  int max_n = 0;
  for(size_t cnt = 0; cnt < n_.size(); ++cnt)
    if(n_[cnt] > max_n)
      max_n = n_[cnt];
  vector<int> ren_n_to_andout; // from next-state to and-out
  ren_n_to_andout.reserve(max_n + 1);
  for(int cnt = 0; cnt < max_n + 1; ++cnt)
    ren_n_to_andout.push_back(cnt);

  list<vector<int> > trans_ands;
  const list<vector<int> > &tcl = AIG2CNF::instance().getTrans().getClauses();
  for(CNF::ClauseConstIter it = tcl.begin(); it != tcl.end(); ++it)
  {
    if(it->size() == 2)
    {
      CNF::ClauseConstIter nr2 = it;
      ++nr2;
      if(nr2 != tcl.end() && nr2->size() == 2)
      {
        CNF::ClauseConstIter nr3 = nr2;
        ++nr3;
        if(nr3 != tcl.end() && nr3->size() == 3)
        {
          int l = (*nr3)[0];
          int r1 = -((*nr3)[1]);
          int r0 = -((*nr3)[2]);
          if((*it)[0] == -l && (*it)[1] == r1 && (*nr2)[0] == -l && (*nr2)[1] == r0)
          {
            vector<int> new_and(3,0);
            new_and[0] = r0;
            new_and[1] = r1;
            new_and[2] = l;
            trans_ands.push_back(new_and);
            it++; it++;
            continue; // we found an AND gate
          }
        }
      }
    }
    if(it->size() == 2)
    {
      CNF::ClauseConstIter nr2 = it;
      ++nr2;
      if(nr2 != tcl.end() && nr2->size() == 2)
      {
        int in = -(*it)[0]; // out is a next-state variable
        int out = ((*it)[1]);
        if(out > 0 && (*nr2)[0] == in && (*nr2)[1] == -out)
        {
          ren_n_to_andout[out] = in;
          ++it;
          continue; // we found an equivalence
        }
      }
    }
    if(it->size() == 1 && (*it)[0] == 1)
      continue; // we found the constant TRUE
    MASSERT(false, "Unknown construction in transition relation.");
  }

  // find a good order for signal expansion
  // when calling AIG2CNF::instance().getTmpDepsTrans(), we can easily go out of memory, so
  // compute the dependencies in a more memory-efficient way:
  vector<int> nr_of_tmps_to_rename(i_.size(), 0);
  const map<int, set<VarInfo> > &deps = AIG2CNF::instance().getTmpDeps();
  for(size_t i_idx = 0; i_idx < i_.size(); ++i_idx)
  {
    set<int> d;
    for(size_t c_idx = 0; c_idx < c_.size(); ++c_idx)
      d.insert(c_[c_idx]);
    d.insert(i_[i_idx]);
    size_t old_d_size = 0;
    while(d.size() != old_d_size)
    {
      old_d_size = d.size();
      for(map<int, set<VarInfo> >::const_iterator it = deps.begin(); it != deps.end(); ++it)
      {
        for(set<VarInfo>::const_iterator i2 = it->second.begin(); i2 != it->second.end(); ++i2)
          if(d.count(i2->getLitInCNF()) != 0)
            d.insert(it->first);
      }
    }
    nr_of_tmps_to_rename[i_idx] = d.size();
  }

  vector<int> sorted_i_indices;
  while(sorted_i_indices.size() < i_.size())
  {
    size_t smallest_idx = 0;
    for(size_t cnt = 1; cnt < i_.size(); ++cnt)
      if(nr_of_tmps_to_rename[cnt] < nr_of_tmps_to_rename[smallest_idx])
        smallest_idx = cnt;
    sorted_i_indices.push_back(smallest_idx);
    nr_of_tmps_to_rename[smallest_idx] = numeric_limits<int>::max();
  }

  int c_max_var = VarManager::instance().getMaxCNFVar();

  int expansion_factor = (1 << max_nr_of_signals_to_expand);
  vector<int> rename_to_fresh;
  rename_to_fresh.reserve(c_.size() * expansion_factor);
  for(size_t cnt = 0; cnt < c_.size(); ++cnt) // ctrl-signals always have to be renamed
    rename_to_fresh.push_back(c_[cnt]);


  int max_s = 0;
  for(size_t cnt = 0; cnt < s_.size(); ++cnt)
    if(s_[cnt] > max_s)
      max_s = s_[cnt];
  c_rename_maps_.reserve(expansion_factor);
  vector<int> s_to_ren_n;
  s_to_ren_n.reserve(max_s + 1);
  for(int cnt = 0; cnt < max_s + 1; ++cnt)
    s_to_ren_n.push_back(cnt);
  for(size_t cnt = 0; cnt < s_.size(); ++cnt)
    s_to_ren_n[s_[cnt]] = ren_n_to_andout[n_[cnt]];
  c_rename_maps_.push_back(s_to_ren_n);

  for(size_t cnt = 0; cnt < max_nr_of_signals_to_expand; ++cnt)
  {
    int idx = sorted_i_indices[cnt];
    int ex = i_[idx];

    // build the rename map:
    vector<int> rename_map;
    vector<int> orig_prop_map;
    rename_map.reserve(c_max_var + 1);
    orig_prop_map.reserve(c_max_var + 1);
    for(int vc = 0; vc < c_max_var + 1; vc++)
    {
      rename_map.push_back(vc);
      orig_prop_map.push_back(vc);
    }
    // the control signals always need to be renamed to fresh variables in the copy:
    size_t orig_rename_size = rename_to_fresh.size();
    for(size_t vc = 0; vc < orig_rename_size; vc++)
    {
      int fresh_var = ++c_max_var;
      rename_map[rename_to_fresh[vc]] = fresh_var;
      rename_to_fresh.push_back(fresh_var);
    }

    // process trans_ands:
    orig_prop_map[ex] = -1; //-1 is FALSE, 1 is TRUE
    rename_map[ex] = 1; //-1 is FALSE, 1 is TRUE

    size_t orig_nr_of_ands = trans_ands.size();
    list<vector<int> >::iterator ita = trans_ands.begin();
    for(size_t a_cnt = 0; a_cnt < orig_nr_of_ands; ++a_cnt)
    {
      int orig_l = (*ita)[2];
      int orig_r0 = (*ita)[0];
      int orig_r1 = (*ita)[1];
      bool remove_orig = false;

      // do constant propagation on the original:
      int orig_prop_r0 = orig_r0 < 0 ? -orig_prop_map[-orig_r0] : orig_prop_map[orig_r0];
      int orig_prop_r1 = orig_r1 < 0 ? -orig_prop_map[-orig_r1] : orig_prop_map[orig_r1];
      if(orig_prop_r0 == -1 || orig_prop_r1 == -1) // one input is false --> output is false
      {
        orig_prop_map[orig_l] = -1;
        remove_orig = true;
      }
      else if(orig_prop_r0 == 1) // r0 is true, so the output is r1
      {
        orig_prop_map[orig_l] = orig_prop_r1;
        remove_orig = true;
      }
      else if(orig_prop_r1 == 1) // r1 is true, so the output is r0
      {
        orig_prop_map[orig_l] = orig_prop_r0;
        remove_orig = true;
      }

      // do constant propagation on the copy:
      bool remove_copy = false;
      int copy_prop_r0 = orig_r0 < 0 ? -rename_map[-orig_r0] : rename_map[orig_r0];
      int copy_prop_r1 = orig_r1 < 0 ? -rename_map[-orig_r1] : rename_map[orig_r1];
      if(copy_prop_r0 == -1 || copy_prop_r1 == -1) // one input is false --> output is false
      {
        rename_map[orig_l] = -1;
        remove_copy = true;
      }
      else if(copy_prop_r0 == 1) // r0 is true, so the output is r1
      {
        rename_map[orig_l] = copy_prop_r1;
        remove_copy = true;
      }
      else if(copy_prop_r1 == 1) // r1 is true, so the output is r0
      {
        rename_map[orig_l] = copy_prop_r0;
        remove_copy = true;
      }

      // now we do the actual copy or removal due to constant propagation:
      if(remove_orig)
        ita = trans_ands.erase(ita);
      else
      {
        (*ita)[0] = orig_prop_r0;
        (*ita)[1] = orig_prop_r1;
        ++ita;
      }

      if(!remove_copy)
      {
        if(remove_orig || // we take the copy even if it is equal because orig is gone
           (((orig_prop_r0 != copy_prop_r0) || (orig_prop_r1 != copy_prop_r1)) && //unequal
            ((orig_prop_r1 != copy_prop_r0) || (orig_prop_r0 != copy_prop_r1))))
        {
          int new_l = ++c_max_var;
          rename_map[orig_l] = new_l;
          vector<int> new_and(3, 0);
          new_and[0] = copy_prop_r0;
          new_and[1] = copy_prop_r1;
          new_and[2] = new_l;
          trans_ands.push_back(new_and);
        }
      }
    }

    // Done with the expansion of variable 'ex'. Now we produce the new rename maps:
    size_t c_rename_maps_orig_size = c_rename_maps_.size();
    for(size_t map_cnt = 0; map_cnt < c_rename_maps_orig_size; ++map_cnt)
    {
      vector<int> &old_map = c_rename_maps_[map_cnt];
      c_rename_maps_.push_back(old_map);
      vector<int> &new_map = c_rename_maps_.back();
      // apply the orig_prop_map to the old_map:
      for(vector<int>::iterator oit = old_map.begin(); oit != old_map.end(); ++oit)
        *oit = *oit < 0 ? -orig_prop_map[-(*oit)] : orig_prop_map[*oit];
      // apply the rename_map to the new_map:
      for(vector<int>::iterator nit = new_map.begin(); nit != new_map.end(); ++nit)
        *nit = *nit < 0 ? -rename_map[-(*nit)] : rename_map[*nit];
    }
  }

  vector<int> ext_keep;
  const vector<int> &pr = VarManager::instance().getVarsOfType(VarInfo::PREV);
  ext_keep.reserve(ext_keep.size() + s_.size() + i_.size() + c_rename_maps_.size() * s_.size());
  ext_keep.insert(ext_keep.end(), pr.begin(), pr.end());
  ext_keep.insert(ext_keep.end(), s_.begin(), s_.end());
  ext_keep.insert(ext_keep.end(), i_.begin(), i_.end());
  for(size_t map_cnt = 0; map_cnt < c_rename_maps_.size(); ++map_cnt)
    for(size_t s_cnt = 0; s_cnt < s_.size(); ++s_cnt)
      ext_keep.push_back(c_rename_maps_[map_cnt][s_[s_cnt]]);

  c_keep_ = ext_keep;
  solver_c->startIncrementalSession(ext_keep, false);
  c_cnf_.clear();
  if(AIG2CNF::instance().isTrueInTrans())
  {
    solver_c->incAddUnitClause(1);
    c_cnf_.add1LitClause(1);
  }
  for(list<vector<int> >::const_iterator it = trans_ands.begin(); it != trans_ands.end(); ++it)
  {
    solver_c->incAdd2LitClause(-((*it)[2]), (*it)[1]);
    solver_c->incAdd2LitClause(-((*it)[2]), (*it)[0]);
    solver_c->incAdd3LitClause((*it)[2], -((*it)[1]), -((*it)[0]));
    c_cnf_.add2LitClause(-((*it)[2]), (*it)[1]);
    c_cnf_.add2LitClause(-((*it)[2]), (*it)[0]);
    c_cnf_.add3LitClause((*it)[2], -((*it)[1]), -((*it)[0]));
  }
}

// -------------------------------------------------------------------------------------------
void UnivExpander::addExpNxtClauseToC(const vector<int> &clause, SatSolver *solver_c)
{
  typedef vector<vector<int> >::const_iterator MapConstIter;
  for(MapConstIter i1 = c_rename_maps_.begin(); i1 != c_rename_maps_.end(); ++i1)
  {
    const vector<int> &ren_map = *i1;
    vector<int> renamed;
    renamed.reserve(clause.size());
    for(vector<int>::const_iterator i2 = clause.begin(); i2 != clause.end(); ++i2)
      renamed.push_back(*i2 < 0 ? -ren_map[-(*i2)] : ren_map[*i2]);
    solver_c->incAddClause(renamed);
  }
}

// -------------------------------------------------------------------------------------------
void UnivExpander::resetSolverCExp(SatSolver *solver_c)
{
  solver_c->startIncrementalSession(c_keep_, false);
  solver_c->incAddCNF(c_cnf_);
}

// -------------------------------------------------------------------------------------------
void UnivExpander::extrExp(const CNF &nxt_win_reg,
                           SatSolver *solver,
                           const vector<int> to_exp) const
{
  // we reverse-engineer the AND gates from the transition relation CNF
  // (constant propagation is easier on the AND gates than in the CNF)
  int max_n = 0;
  for(size_t cnt = 0; cnt < n_.size(); ++cnt)
    if(n_[cnt] > max_n)
      max_n = n_[cnt];
  vector<int> ren_n_to_andout; // from next-state to and-out
  ren_n_to_andout.reserve(max_n + 1);
  for(int cnt = 0; cnt < max_n + 1; ++cnt)
    ren_n_to_andout.push_back(cnt);

  const list<vector<int> > &tcl = AIG2CNF::instance().getTrans().getClauses();
  vector<CNFAnd> trans_ands;
  trans_ands.reserve(tcl.size());
  for(CNF::ClauseConstIter it = tcl.begin(); it != tcl.end(); ++it)
  {
    if(it->size() == 2)
    {
      CNF::ClauseConstIter nr2 = it;
      ++nr2;
      if(nr2 != tcl.end() && nr2->size() == 2)
      {
        CNF::ClauseConstIter nr3 = nr2;
        ++nr3;
        if(nr3 != tcl.end() && nr3->size() == 3)
        {
          int l = (*nr3)[0];
          int r1 = -((*nr3)[1]);
          int r0 = -((*nr3)[2]);
          if((*it)[0] == -l && (*it)[1] == r1 && (*nr2)[0] == -l && (*nr2)[1] == r0)
          {
            CNFAnd new_and;
            new_and.r0 = r0;
            new_and.r1 = r1;
            new_and.l = l;
            trans_ands.push_back(new_and);
            it++; it++;
            continue; // we found an AND gate
          }
        }
      }
    }
    if(it->size() == 2)
    {
      CNF::ClauseConstIter nr2 = it;
      ++nr2;
      if(nr2 != tcl.end() && nr2->size() == 2)
      {
        int in = -(*it)[0]; // out is a next-state variable
        int out = ((*it)[1]);
        if(out > 0 && (*nr2)[0] == in && (*nr2)[1] == -out)
        {
          ren_n_to_andout[out] = in;
          ++it;
          continue; // we found an equivalence
        }
      }
    }
    if(it->size() == 1 && (*it)[0] == 1)
      continue; // we found the constant TRUE
    MASSERT(false, "Unknown construction in transition relation.");
  }

  // build the map to rename present-state variables to the renamed version of the
  // corresponding next-state copy (this is used for doing the present-to-next transformation
  // and the renaming to the output of the AND gates in one step):
  int max_s = 0;
  for(size_t cnt = 0; cnt < s_.size(); ++cnt)
    if(s_[cnt] > max_s)
      max_s = s_[cnt];
  vector<int> s_to_ren_n;
  s_to_ren_n.reserve(max_s + 1);
  for(int cnt = 0; cnt < max_s + 1; ++cnt)
    s_to_ren_n.push_back(cnt);
  for(size_t cnt = 0; cnt < s_.size(); ++cnt)
    s_to_ren_n[s_[cnt]] = ren_n_to_andout[n_[cnt]];
  int max_var_too_keep = VarManager::instance().getMaxCNFVar();
  int max_trans_var = max_var_too_keep;

  size_t nr_of_copies = 1;
  for(size_t cnt = 0; cnt < to_exp.size(); ++cnt)
    nr_of_copies <<= 1;
  vector<vector<int> > rename_maps;
  rename_maps.reserve(nr_of_copies);
  rename_maps.push_back(vector<int>());
  rename_maps.back().reserve(s_.size());
  for(size_t cnt = 0; cnt < s_.size(); ++cnt)
    rename_maps.back().push_back(s_to_ren_n[s_[cnt]]);
  for(size_t idx = 0; idx < to_exp.size(); ++idx)
  {
    // build the rename map:
    vector<int> rename_map;
    vector<int> orig_prop_map;
    rename_map.reserve(max_trans_var + 1);
    orig_prop_map.reserve(max_trans_var + 1);
    for(int vc = 0; vc < max_trans_var + 1; vc++)
    {
      rename_map.push_back(vc);
      orig_prop_map.push_back(vc);
    }

    // process trans_ands:
    orig_prop_map[to_exp[idx]] = -1; //-1 is FALSE, 1 is TRUE
    rename_map[to_exp[idx]] = 1; //-1 is FALSE, 1 is TRUE

    vector<CNFAnd> new_ands;
    map<uint64_t, int> cache;
    max_trans_var = max_var_too_keep;
    new_ands.reserve(2*trans_ands.size());
    for(size_t a_cnt = 0; a_cnt < trans_ands.size(); ++a_cnt)
    {
      int orig_r0 = trans_ands[a_cnt].r0;
      int orig_r1 = trans_ands[a_cnt].r1;
      int orig_l = trans_ands[a_cnt].l;

      // do constant propagation on the original:
      int orig_prop_r0 = orig_r0 < 0 ? -orig_prop_map[-orig_r0] : orig_prop_map[orig_r0];
      int orig_prop_r1 = orig_r1 < 0 ? -orig_prop_map[-orig_r1] : orig_prop_map[orig_r1];

      if(orig_prop_r0 == -1 || orig_prop_r1 == -1) // one input is false --> output is false
        orig_prop_map[orig_l] = -1;
      else if(orig_prop_r0 == 1) // r0 is true, so the output is r1
        orig_prop_map[orig_l] = orig_prop_r1;
      else if(orig_prop_r1 == 1) // r1 is true, so the output is r0
        orig_prop_map[orig_l] = orig_prop_r0;
      else if(orig_prop_r0 == orig_prop_r1) // inputs are equal
        orig_prop_map[orig_l] = orig_prop_r0;
      else if(orig_prop_r0 == -orig_prop_r1) // inputs are contradicting
        orig_prop_map[orig_l] = -1;
      else
      {
        // we need to keep the AND gate
        if(orig_prop_r0 > orig_prop_r1) // swap
        {
          int tmp = orig_prop_r0;
          orig_prop_r0 = orig_prop_r1;
          orig_prop_r1 = tmp;
        }
        uint64_t comb = static_cast<uint64_t>(orig_prop_r0) << 32;
        comb |=  static_cast<uint64_t>(orig_prop_r1) & 0x00000000FFFFFFFFULL;
        pair<uint64_t, int> new_cache_item(comb, max_trans_var + 1);
        pair<map<uint64_t, int>::iterator, bool> found = cache.insert(new_cache_item);
        if(found.second) // we do not have such an AND gate yet
        {
          ++max_trans_var;
          CNFAnd new_and;
          new_and.r0 = orig_prop_r0;
          new_and.r1 = orig_prop_r1;
          new_and.l = max_trans_var;
          new_ands.push_back(new_and);
          orig_prop_map[orig_l] = max_trans_var;
        }
        else // we already have this AND gate
          orig_prop_map[orig_l] = found.first->second;
      }

      // do constant propagation on the copy:
      int copy_prop_r0 = orig_r0 < 0 ? -rename_map[-orig_r0] : rename_map[orig_r0];
      int copy_prop_r1 = orig_r1 < 0 ? -rename_map[-orig_r1] : rename_map[orig_r1];
      if(copy_prop_r0 == -1 || copy_prop_r1 == -1) // one input is false --> output is false
        rename_map[orig_l] = -1;
      else if(copy_prop_r0 == 1) // r0 is true, so the output is r1
        rename_map[orig_l] = copy_prop_r1;
      else if(copy_prop_r1 == 1) // r1 is true, so the output is r0
        rename_map[orig_l] = copy_prop_r0;
      else if(copy_prop_r0 == copy_prop_r1) // inputs are equal
        rename_map[orig_l] = copy_prop_r0;
      else if(copy_prop_r0 == -copy_prop_r1) // inputs are contradicting
        rename_map[orig_l] = -1;
      else
      {
        // we need to keep the AND gate
        if(copy_prop_r0 > copy_prop_r1) // swap
        {
          int tmp = copy_prop_r0;
          copy_prop_r0 = copy_prop_r1;
          copy_prop_r1 = tmp;
        }
        uint64_t comb = static_cast<uint64_t>(copy_prop_r0) << 32;
        comb |=  static_cast<uint64_t>(copy_prop_r1) & 0x00000000FFFFFFFFULL;
        pair<uint64_t, int> new_cache_item(comb, max_trans_var + 1);
        pair<map<uint64_t, int>::iterator, bool> found = cache.insert(new_cache_item);
        if(found.second) // we do not have such an AND gate yet
        {
          ++max_trans_var;
          CNFAnd new_and;
          new_and.r0 = copy_prop_r0;
          new_and.r1 = copy_prop_r1;
          new_and.l = max_trans_var;
          new_ands.push_back(new_and);
          rename_map[orig_l] = max_trans_var;
        }
        else // we already have this AND gate
          rename_map[orig_l] = found.first->second;
      }
    }
    new_ands.swap(trans_ands);
    size_t orig_ns_size = rename_maps.size();
    for(size_t c1  = 0; c1 < orig_ns_size; ++c1)
    {
      rename_maps.push_back(vector<int>());
      rename_maps.back().reserve(n_.size());
      for(size_t c2 = 0; c2 < n_.size(); ++c2)
      {
        int lit = rename_maps[c1][c2];
        rename_maps.back().push_back(lit < 0 ? -rename_map[-lit] : rename_map[lit]);
        rename_maps[c1][c2] = lit < 0 ? -orig_prop_map[-lit] : orig_prop_map[lit];
      }
    }
  }

  vector<int> vars_to_keep;
  vars_to_keep.reserve(s_.size() + i_.size() + c_.size());
  vars_to_keep.insert(vars_to_keep.end(), s_.begin(), s_.end());
  vars_to_keep.insert(vars_to_keep.end(), i_.begin(), i_.end());
  vars_to_keep.insert(vars_to_keep.end(), c_.begin(), c_.end());
  solver->startIncrementalSession(vars_to_keep, false);
  if(AIG2CNF::instance().isTrueInTrans())
    solver->incAddUnitClause(1);
  for(size_t c = 0; c < trans_ands.size(); ++c)
  {
    solver->incAdd2LitClause(-trans_ands[c].l, trans_ands[c].r1);
    solver->incAdd2LitClause(-trans_ands[c].l, trans_ands[c].r0);
    solver->incAdd3LitClause(trans_ands[c].l, -trans_ands[c].r1, -trans_ands[c].r0);
  }
  // now the only thing left is the negation of the expanded winning region

  vector<int> ren;
  ren.reserve(max_n + 1);
  for(int cnt = 0; cnt < max_n + 1; ++cnt)
    ren.push_back(cnt);
  int max_var = max_trans_var;
  const list<vector<int> > &clauses = nxt_win_reg.getClauses();

  // we re-use the following vector across all iterations to save object creations and
  // deletions:
  vector<vector<int> > current_cnf;
  current_cnf.reserve(clauses.size());
  for(size_t cnt = 0; cnt < clauses.size(); ++cnt)
  {
    current_cnf.push_back(vector<int>());
    current_cnf.back().reserve(n_.size());
  }

  std::unordered_map<vector<int>, int > clause_to_neg_lit;
  vector<int> one_clause_false;
  one_clause_false.reserve(clauses.size());
  vector<int> is_unit(max_trans_var + 1, 0);
  vector<int> unit_vars; // used only to reset is_unit faster
  unit_vars.reserve(clauses.size());

  for(size_t copy_cnt = 0; copy_cnt < rename_maps.size(); ++copy_cnt)
  {
    for(size_t cnt = 0; cnt < n_.size(); ++cnt)
      ren[n_[cnt]] = rename_maps[copy_cnt][cnt];

    // this is faster than doing is_unit.assign(is_unit.size(), 0);
    for(size_t cnt = 0; cnt < unit_vars.size(); ++cnt)
      is_unit[unit_vars[cnt]] = 0;

    one_clause_false.clear();
    unit_vars.clear();

    // renaming and simplification:
    bool next_win_false = false;
    size_t current_nr_of_elems = 0;
    for(CNF::ClauseConstIter it = clauses.begin(); it != clauses.end(); ++it)
    {
      vector<int> &current_clause = current_cnf[current_nr_of_elems];
      current_clause.clear();
      bool clause_is_true = false;
      for(size_t lit_cnt = 0; lit_cnt < it->size(); ++lit_cnt)
      {
        int lit = (*it)[lit_cnt];
        int ren_lit = lit < 0 ? -ren[-lit] : ren[lit];
        if(ren_lit == 1)
        {
          clause_is_true = true;
          break;
        }
        else if(ren_lit != -1) // if a literal is false we can ignore it
        {
          // checking for tautological clauses eliminates almost nothing:
          // for(size_t neg_c = 0; neg_c < current_clause.size(); ++neg_c)
          // {
          //   if(current_clause[neg_c] == -ren_lit)
          //   {
          //     clause_is_true = true;
          //     break;
          //   }
          // }
          // if(clause_is_true)
          //   break;
          current_clause.push_back(ren_lit);
        }
      }
      if(!clause_is_true)
      {
        if(current_clause.empty()) // the clause is empty (i.e., false)
        {
          next_win_false = true;
          break;
        }
        else if(current_clause.size() == 1)
        {
          int li = current_clause[0];
          if(li < 0)
          {
            if(is_unit[-li] == 1) // unit clauses are contradicting
            {
              next_win_false = true;
              break;
            }
            is_unit[-li] = -1;
            unit_vars.push_back(-li);
          }
          else
          {
            if(is_unit[li] == -1) // unit clauses are contradicting
            {
              next_win_false = true;
              break;
            }
            is_unit[li] = 1;
            unit_vars.push_back(li);
          }
          one_clause_false.push_back(-li);
        }
        else
          current_nr_of_elems++;
      }
    }
    if(next_win_false)
      continue;

    // BEGIN simplification due to unit clauses:
    // This block can safely be commented out completely
    bool new_unit_discoverd = unit_vars.size() > 0;
    while(new_unit_discoverd && !next_win_false)
    {
      new_unit_discoverd = false;
      for(size_t cl_cnt = 0; cl_cnt < current_nr_of_elems; )
      {
        vector<int> &current_clause = current_cnf[cl_cnt];
        bool remove_clause = false;
        for(size_t lit_cnt = 0; lit_cnt < current_clause.size();)
        {
          int current_lit = current_clause[lit_cnt];
          if(current_lit < 0)
          {
            if(is_unit[-current_lit] == -1) // remove the clause
            {
              remove_clause = true;
              break;
            }
            else if(is_unit[-current_lit] == 1) // remove the literal
            {
              current_clause[lit_cnt] = current_clause.back();
              current_clause.pop_back();
            }
            else
              ++lit_cnt;
          }
          else
          {
            if(is_unit[current_lit] == 1) // remove the clause
            {
              remove_clause = true;
              break;
            }
            else if(is_unit[current_lit] == -1) // remove the literal
            {
              current_clause[lit_cnt] = current_clause.back();
              current_clause.pop_back();
            }
            else
              ++lit_cnt;
          }
        }
        if(remove_clause)
        {
          current_clause.swap(current_cnf[current_nr_of_elems - 1]);
          --current_nr_of_elems;
        }
        else if(current_clause.empty())
        {
          next_win_false = true;
          break;
        }
        else if(current_clause.size() == 1)
        {
          int li = current_clause[0];
          if(li < 0)
          {
            if(is_unit[-li] == 1) // unit clauses are contradicting
            {
              next_win_false = true;
              break;
            }
            is_unit[-li] = -1;
            unit_vars.push_back(-li);
          }
          else
          {
            if(is_unit[li] == -1) // unit clauses are contradicting
            {
              next_win_false = true;
              break;
            }
            is_unit[li] = 1;
            unit_vars.push_back(li);
          }
          new_unit_discoverd = true;
          one_clause_false.push_back(-li);
          current_clause.swap(current_cnf[current_nr_of_elems - 1]);
          --current_nr_of_elems;
        }
        else
          ++cl_cnt;
      }
    }
    if(next_win_false)
      continue;
    // END simplification due to unit clauses

    // actual negation:
    // the negation of the unit clauses have already been added to one_clause_false
    for(size_t cl_cnt = 0; cl_cnt < current_nr_of_elems; ++cl_cnt)
    {
      vector<int> &current_clause = current_cnf[cl_cnt];
      // Let's sort current_clause. This is selectionSort, taken from Minisat:
      size_t sz = current_clause.size(), best_i;
      int tmp;
      for(size_t i = 0; i < sz - 1; i++)
      {
        best_i = i;
        for(size_t j = i+1; j < sz; j++)
        {
          if(current_clause[j] < current_clause[best_i])
            best_i = j;
        }
        tmp = current_clause[i];
        current_clause[i] = current_clause[best_i];
        current_clause[best_i] = tmp;
      }

      pair<unordered_map<vector<int>, int>::const_iterator, bool> mapit;
      mapit = clause_to_neg_lit.emplace(current_clause, max_var + 1);
      if(!mapit.second) // element was already there
        one_clause_false.push_back(mapit.first->second);
      else
      {
        max_var++;
        int clause_false_lit = max_var;
        for(size_t lit_cnt = 0; lit_cnt < current_clause.size(); ++lit_cnt)
          solver->incAdd2LitClause(-clause_false_lit, -(current_clause[lit_cnt]));
        one_clause_false.push_back(clause_false_lit);
      }
    }
    solver->incAddClause(one_clause_false);
  }
}

// -------------------------------------------------------------------------------------------
void UnivExpander::setAbortCondition(volatile int *abort_if)
{
  abort_if_ = abort_if;
}

// -------------------------------------------------------------------------------------------
void UnivExpander::cleanup()
{
  cleanupIData();
  c_rename_maps_.clear();
  c_rename_maps_.shrink_to_fit();
  c_cnf_.clear();
  c_keep_.clear();
  c_keep_.shrink_to_fit();
}

// -------------------------------------------------------------------------------------------
bool UnivExpander::initSolverIData(bool limit_size)
{
  // we reverse-engineer the AND gates from the transition relation CNF
  // (constant propagation is easier on the AND gates than in the CNF)
  int max_n = 0;
  for(size_t cnt = 0; cnt < n_.size(); ++cnt)
    if(n_[cnt] > max_n)
      max_n = n_[cnt];
  vector<int> ren_n_to_andout; // from next-state to and-out
  ren_n_to_andout.reserve(max_n + 1);
  for(int cnt = 0; cnt < max_n + 1; ++cnt)
    ren_n_to_andout.push_back(cnt);

  const list<vector<int> > &tcl = AIG2CNF::instance().getTrans().getClauses();
  i_trans_ands_.clear();
  i_trans_ands_.reserve(tcl.size());
  for(CNF::ClauseConstIter it = tcl.begin(); it != tcl.end(); ++it)
  {
    if(it->size() == 2)
    {
      CNF::ClauseConstIter nr2 = it;
      ++nr2;
      if(nr2 != tcl.end() && nr2->size() == 2)
      {
        CNF::ClauseConstIter nr3 = nr2;
        ++nr3;
        if(nr3 != tcl.end() && nr3->size() == 3)
        {
          int l = (*nr3)[0];
          int r1 = -((*nr3)[1]);
          int r0 = -((*nr3)[2]);
          if((*it)[0] == -l && (*it)[1] == r1 && (*nr2)[0] == -l && (*nr2)[1] == r0)
          {
            CNFAnd new_and;
            new_and.r0 = r0;
            new_and.r1 = r1;
            new_and.l = l;
            i_trans_ands_.push_back(new_and);
            it++; it++;
            continue; // we found an AND gate
          }
        }
      }
    }
    if(it->size() == 2)
    {
      CNF::ClauseConstIter nr2 = it;
      ++nr2;
      if(nr2 != tcl.end() && nr2->size() == 2)
      {
        int in = -(*it)[0]; // out is a next-state variable
        int out = ((*it)[1]);
        if(out > 0 && (*nr2)[0] == in && (*nr2)[1] == -out)
        {
          ren_n_to_andout[out] = in;
          ++it;
          continue; // we found an equivalence
        }
      }
    }
    if(it->size() == 1 && (*it)[0] == 1)
      continue; // we found the constant TRUE
    MASSERT(false, "Unknown construction in transition relation.");
  }

  // build the map to rename present-state variables to the renamed version of the
  // corresponding next-state copy (this is used for doing the present-to-next transformation
  // and the renaming to the output of the AND gates in one step):
  int max_s = 0;
  for(size_t cnt = 0; cnt < s_.size(); ++cnt)
    if(s_[cnt] > max_s)
      max_s = s_[cnt];
  i_s_to_ren_n_.reserve(max_s + 1);
  for(int cnt = 0; cnt < max_s + 1; ++cnt)
    i_s_to_ren_n_.push_back(cnt);
  for(size_t cnt = 0; cnt < s_.size(); ++cnt)
    i_s_to_ren_n_[s_[cnt]] = ren_n_to_andout[n_[cnt]];
  int max_var_too_keep = max_s;
  for(size_t cnt = 0; cnt < i_.size(); ++cnt)
    if(i_[cnt] > max_var_too_keep)
      max_var_too_keep = i_[cnt];
  for(size_t cnt = 0; cnt < c_.size(); ++cnt)
    if(c_[cnt] > max_var_too_keep)
      max_var_too_keep = c_[cnt];
  i_max_trans_var_ = AIG2CNF::instance().getTmpDeps().rbegin()->first;

  bool use_i_rename_maps = true;
  if(c_.size() < 64)
  {
    size_t nr_of_copies = 1;
    for(size_t cnt = 0; cnt < c_.size(); ++cnt)
      nr_of_copies <<= 1;
    size_t mem_estimate_in_byte = nr_of_copies * s_.size() * 4;
    if(mem_estimate_in_byte < 100000000) // everything less than 100 MB is fine
      i_rename_maps_.reserve(nr_of_copies);
    else
      i_rename_maps_.reserve(25000000ULL / s_.size());
  }
  else
    i_rename_maps_.reserve(25000000ULL / s_.size());

  i_rename_maps_.push_back(vector<int>());
  i_rename_maps_.back().reserve(s_.size());
  for(size_t cnt = 0; cnt < s_.size(); ++cnt)
  {
    int l = i_s_to_ren_n_[s_[cnt]];
    i_rename_maps_.back().push_back(l);
    i_occ_.insert(l < 0 ? -l : l);
  }

  // find a good order for signal expansion
  // when calling AIG2CNF::instance().getTmpDepsTrans(), we can easily go out of memory, so
  // compute the dependencies in a more memory-efficient way:
  vector<int> nr_of_tmps_to_rename(c_.size(), 0);
  const map<int, set<VarInfo> > &deps = AIG2CNF::instance().getTmpDeps();
  for(size_t c_idx = 0; c_idx < c_.size(); ++c_idx)
  {
    set<int> d;
    d.insert(c_[c_idx]);
    size_t old_d_size = 0;
    while(d.size() != old_d_size)
    {
      old_d_size = d.size();
      for(map<int, set<VarInfo> >::const_iterator it = deps.begin(); it != deps.end(); ++it)
      {
        for(set<VarInfo>::const_iterator i2 = it->second.begin(); i2 != it->second.end(); ++i2)
          if(d.count(i2->getLitInCNF()) != 0)
            d.insert(it->first);
      }
    }
    nr_of_tmps_to_rename[c_idx] = d.size();
  }
  vector<int> sorted_c_indices;
  while(sorted_c_indices.size() < c_.size())
  {
    size_t smallest_idx = 0;
    for(size_t cnt = 1; cnt < c_.size(); ++cnt)
      if(nr_of_tmps_to_rename[cnt] < nr_of_tmps_to_rename[smallest_idx])
        smallest_idx = cnt;
    sorted_c_indices.push_back(smallest_idx);
    nr_of_tmps_to_rename[smallest_idx] = numeric_limits<int>::max();
  }
  // Let's do the most difficult first. This seems to have the best potential for
  // keeping i_rename_maps_ small and thus saves memory. But since we expand over all control
  // signals anyway, the order does not really matter. The memory consumption does not really
  // matter either. Who cares about a 100 MB.
  reverse(sorted_c_indices.begin(), sorted_c_indices.end());

  i_copy_maps_.reserve(c_.size());
  i_orig_prop_maps_.reserve(c_.size());
  for(size_t cnt = 0; cnt < c_.size(); ++cnt)
  {
    size_t idx = sorted_c_indices[cnt];

    // check if we already exceeded the limits:
    if(limit_size)
    {
      if(Utils::getCurrentMemUsage() > Options::instance().getSizeLimitForExpansion() ||
         shallAbort())
      {
        cleanupIData();
        return true;
      }
      size_t nr_of_next_state_copies = i_rename_maps_.size();
      // a safety net: more than 10 Mio copies of the winning region sounds intractable:
      if(nr_of_next_state_copies > 10000000)
      {
        cleanupIData();
        return true;
      }
      for(size_t c = 0; c < i_copy_maps_.size(); ++c)
      {
        nr_of_next_state_copies <<= 1;
        if(nr_of_next_state_copies > 10000000)
        {
          cleanupIData();
          return true;
        }
      }
    }

    // build the rename map:
    vector<int> rename_map;
    vector<int> orig_prop_map;
    rename_map.reserve(i_max_trans_var_ + 1);
    orig_prop_map.reserve(i_max_trans_var_ + 1);
    for(int vc = 0; vc < i_max_trans_var_ + 1; vc++)
    {
      rename_map.push_back(vc);
      orig_prop_map.push_back(vc);
    }

    // process trans_ands:
    orig_prop_map[c_[idx]] = -1; //-1 is FALSE, 1 is TRUE
    rename_map[c_[idx]] = 1; //-1 is FALSE, 1 is TRUE

    vector<CNFAnd> new_ands;
    map<uint64_t, int> cache;
    i_max_trans_var_ = max_var_too_keep;
    new_ands.reserve(2*i_trans_ands_.size());
    for(size_t a_cnt = 0; a_cnt < i_trans_ands_.size(); ++a_cnt)
    {
      if(shallAbort())
      {
        cleanupIData();
        return true;
      }
      int orig_r0 = i_trans_ands_[a_cnt].r0;
      int orig_r1 = i_trans_ands_[a_cnt].r1;
      int orig_l = i_trans_ands_[a_cnt].l;

      // do constant propagation on the original:
      int orig_prop_r0 = orig_r0 < 0 ? -orig_prop_map[-orig_r0] : orig_prop_map[orig_r0];
      int orig_prop_r1 = orig_r1 < 0 ? -orig_prop_map[-orig_r1] : orig_prop_map[orig_r1];

      if(orig_prop_r0 == -1 || orig_prop_r1 == -1) // one input is false --> output is false
        orig_prop_map[orig_l] = -1;
      else if(orig_prop_r0 == 1) // r0 is true, so the output is r1
        orig_prop_map[orig_l] = orig_prop_r1;
      else if(orig_prop_r1 == 1) // r1 is true, so the output is r0
        orig_prop_map[orig_l] = orig_prop_r0;
      else if(orig_prop_r0 == orig_prop_r1) // inputs are equal
        orig_prop_map[orig_l] = orig_prop_r0;
      else if(orig_prop_r0 == -orig_prop_r1) // inputs are contradicting
        orig_prop_map[orig_l] = -1;
      else
      {
        // we need to keep the AND gate
        if(orig_prop_r0 > orig_prop_r1) // swap
        {
          int tmp = orig_prop_r0;
          orig_prop_r0 = orig_prop_r1;
          orig_prop_r1 = tmp;
        }
        uint64_t comb = static_cast<uint64_t>(orig_prop_r0) << 32;
        comb |=  static_cast<uint64_t>(orig_prop_r1) & 0x00000000FFFFFFFFULL;
        pair<uint64_t, int> new_cache_item(comb, i_max_trans_var_ + 1);
        pair<map<uint64_t, int>::iterator, bool> found = cache.insert(new_cache_item);
        if(found.second) // we do not have such an AND gate yet
        {
          ++i_max_trans_var_;
          CNFAnd new_and;
          new_and.r0 = orig_prop_r0;
          new_and.r1 = orig_prop_r1;
          new_and.l = i_max_trans_var_;
          new_ands.push_back(new_and);
          orig_prop_map[orig_l] = i_max_trans_var_;
        }
        else // we already have this AND gate
          orig_prop_map[orig_l] = found.first->second;
      }

      // do constant propagation on the copy:
      int copy_prop_r0 = orig_r0 < 0 ? -rename_map[-orig_r0] : rename_map[orig_r0];
      int copy_prop_r1 = orig_r1 < 0 ? -rename_map[-orig_r1] : rename_map[orig_r1];
      if(copy_prop_r0 == -1 || copy_prop_r1 == -1) // one input is false --> output is false
        rename_map[orig_l] = -1;
      else if(copy_prop_r0 == 1) // r0 is true, so the output is r1
        rename_map[orig_l] = copy_prop_r1;
      else if(copy_prop_r1 == 1) // r1 is true, so the output is r0
        rename_map[orig_l] = copy_prop_r0;
      else if(copy_prop_r0 == copy_prop_r1) // inputs are equal
        rename_map[orig_l] = copy_prop_r0;
      else if(copy_prop_r0 == -copy_prop_r1) // inputs are contradicting
        rename_map[orig_l] = -1;
      else
      {
        // we need to keep the AND gate
        if(copy_prop_r0 > copy_prop_r1) // swap
        {
          int tmp = copy_prop_r0;
          copy_prop_r0 = copy_prop_r1;
          copy_prop_r1 = tmp;
        }
        uint64_t comb = static_cast<uint64_t>(copy_prop_r0) << 32;
        comb |=  static_cast<uint64_t>(copy_prop_r1) & 0x00000000FFFFFFFFULL;
        pair<uint64_t, int> new_cache_item(comb, i_max_trans_var_ + 1);
        pair<map<uint64_t, int>::iterator, bool> found = cache.insert(new_cache_item);
        if(found.second) // we do not have such an AND gate yet
        {
          ++i_max_trans_var_;
          CNFAnd new_and;
          new_and.r0 = copy_prop_r0;
          new_and.r1 = copy_prop_r1;
          new_and.l = i_max_trans_var_;
          new_ands.push_back(new_and);
          rename_map[orig_l] = i_max_trans_var_;
        }
        else // we already have this AND gate
          rename_map[orig_l] = found.first->second;
      }
    }
    new_ands.swap(i_trans_ands_);

    // update the rename maps:
    set<int> new_occ;
    bool is_eq = true;
    for(set<int>::const_iterator it = i_occ_.begin(); it != i_occ_.end(); ++it)
    {
      int l1 = rename_map[*it];
      int l2 = orig_prop_map[*it];
      new_occ.insert(l1 < 0 ? -l1 : l1);
      if(l1 != l2)
      {
        is_eq = false;
        new_occ.insert(l2 < 0 ? -l2 : l2);
      }
    }
    if(is_eq) // we just apply the new renaming in-place:
    {
      if(use_i_rename_maps)
      {
        for(size_t c1  = 0; c1 < i_rename_maps_.size(); ++c1)
        {
          for(size_t c2 = 0; c2 < s_.size(); ++c2)
          {
            int lit = i_rename_maps_[c1][c2];
            i_rename_maps_[c1][c2] = lit < 0 ? -orig_prop_map[-lit] : orig_prop_map[lit];
          }
        }
      }
      else
      {
        for(set<int>::const_iterator it = i_prev_occ_.begin(); it != i_prev_occ_.end(); ++it)
        {
          int from1 = i_copy_maps_.back()[*it];
          int to1 = from1 < 0 ? -orig_prop_map[-from1] : orig_prop_map[from1];
          i_copy_maps_.back()[*it] = to1;
          int from2 = i_orig_prop_maps_.back()[*it];
          int to2 = from2 < 0 ? -orig_prop_map[-from2] : orig_prop_map[from2];
          i_orig_prop_maps_.back()[*it] = to2;
        }
      }
      i_occ_.swap(new_occ);
    }
    else // we need to double the renaming maps:
    {
      if(use_i_rename_maps && i_rename_maps_.size() * 2 <= i_rename_maps_.capacity())
      {
        // here, we apply the new renaming to the i_rename_maps_:
        size_t orig_ns_size = i_rename_maps_.size();
        for(size_t c1  = 0; c1 < orig_ns_size; ++c1)
        {
          i_rename_maps_.push_back(vector<int>());
          i_rename_maps_.back().reserve(n_.size());
          for(size_t c2 = 0; c2 < n_.size(); ++c2)
          {
            int lit = i_rename_maps_[c1][c2];
            i_rename_maps_.back().push_back(lit < 0 ? -rename_map[-lit] : rename_map[lit]);
            i_rename_maps_[c1][c2] = lit < 0 ? -orig_prop_map[-lit] : orig_prop_map[lit];
          }
        }
        // BEGIN remove duplicate renamings:
        // (hardly ever helps, but does not cost too much)
        set<vector<int> > have;
        for(size_t c1 = 0; c1 < i_rename_maps_.size();)
        {
          pair<set<vector<int> >::const_iterator, bool> ins_res = have.insert(i_rename_maps_[c1]);
          if(ins_res.second == false) // already exists
          {
            i_rename_maps_[c1].swap(i_rename_maps_.back());
            i_rename_maps_.pop_back();
          }
          else
            ++c1;
        }
        // END remove duplicate renamings

      }
      else
      {
        use_i_rename_maps = false;
        i_copy_maps_.push_back(vector<int>());
        i_copy_maps_.back().swap(rename_map);
        i_orig_prop_maps_.push_back(vector<int>());
        i_orig_prop_maps_.back().swap(orig_prop_map);
      }
      i_prev_occ_.clear();
      i_prev_occ_.swap(i_occ_);
      i_occ_.swap(new_occ);
    }
  }

  if(shallAbort())
  {
    cleanupIData();
    return true;
  }

  // now some more optimizations which can also be disabled
  if(s_.size() > 3) // few state variables --> few iterations --> does not pay off
    simplifyTransAndsWithAbc();
  
  if(shallAbort())
  {
    cleanupIData();
    return true;
  }

  for(size_t c1  = 0; c1 < i_rename_maps_.size(); ++c1)
  {
    for(size_t c2 = 0; c2 < n_.size(); ++c2)
    {
      int lit = i_rename_maps_[c1][c2];
      i_occ_in_i_rename_maps_.insert(lit < 0 ? -lit : lit);
    }
  }

  return false;
}

// -------------------------------------------------------------------------------------------
void UnivExpander::simplifyTransAndsWithAbc()
{
  vector<int> si;
  si.reserve(s_.size() + i_.size());
  si.insert(si.end(), s_.begin(), s_.end());
  si.insert(si.end(), i_.begin(), i_.end());

  aiger *raw = aiger_init();
  int next_free_aig_lit = 2;
  vector<int> cnf2aig(i_max_trans_var_ + 1, 0);
  cnf2aig[1] = 1;
  for(size_t cnt = 0; cnt < si.size(); ++cnt)
  {
    cnf2aig[si[cnt]] = next_free_aig_lit;
    aiger_add_input(raw, next_free_aig_lit, NULL);
    next_free_aig_lit += 2;
  }
  for(size_t cnt = 0; cnt < i_trans_ands_.size(); ++cnt)
  {
    int l_cnf = i_trans_ands_[cnt].l;
    int r0_cnf = i_trans_ands_[cnt].r0;
    int r1_cnf = i_trans_ands_[cnt].r1;
    int r0_aig = r0_cnf < 0 ? aiger_not(cnf2aig[-r0_cnf]) : cnf2aig[r0_cnf];
    int r1_aig = r1_cnf < 0 ? aiger_not(cnf2aig[-r1_cnf]) : cnf2aig[r1_cnf];
    aiger_add_and(raw, next_free_aig_lit, r0_aig, r1_aig);
    cnf2aig[l_cnf] = next_free_aig_lit;
    next_free_aig_lit += 2;
  }

  vector<int> cnf2outidx(i_max_trans_var_ + 1, -1);
  int out_nr = 0;
  for(set<int>::const_iterator it = i_occ_.begin(); it != i_occ_.end(); ++it)
  {
    aiger_add_output(raw, cnf2aig[*it], NULL);
    cnf2outidx[*it] = out_nr++;
  }

  // done constructing AIGER version of cnf, now we optimize with ABC:
  string tmp_in_file = Options::instance().getUniqueTmpFileName("optimize_exp") + ".aig";
  int succ = aiger_open_and_write_to_file(raw, tmp_in_file.c_str());
  aiger_reset(raw);
  MASSERT(succ != 0, "Could not write out AIGER file for optimization with ABC.");

  // optimize circuit:
  string path_to_abc = Options::instance().getTPDirName() + "/abc/abc/abc";
  string tmp_out_file = Options::instance().getUniqueTmpFileName("optimized_exp") + ".aig";
  string abc_command = path_to_abc + " -c \"";
  abc_command += "read_aiger " + tmp_in_file + ";";
  abc_command += " refactor -zl; rewrite -zl; balance -l; resub -zl; rewrite -zl; ifraig;";
  abc_command += " write_aiger -s " + tmp_out_file + "\"";
  abc_command += " > /dev/null 2>&1";
  int ret = system(abc_command.c_str());
  ret = WEXITSTATUS(ret);
  if(ret != 0)
  {
    // It is not unusual that ABC crashes with a segmentation fault. The reason is often
    // fraiging. So let's try again but without fraiging:
    L_DBG("ABC crashed. I'm trying again without fraiging");
    abc_command = path_to_abc + " -c \"";
    abc_command += "read_aiger " + tmp_in_file + ";";
    abc_command += " refactor -zl; rewrite -zl; balance -l; resub -zl; rewrite -zl; ";
    abc_command += " write_aiger -s " + tmp_out_file + "\"";
    abc_command += " > /dev/null 2>&1";
    ret = system(abc_command.c_str());
    if(ret != 0)
      return; // now we give up
  }

  // read back the result:
  aiger *res = aiger_init();
  const char *err = aiger_open_and_read_from_file (res, tmp_out_file.c_str());
  MASSERT(err == NULL, "Could not open optimized AIGER file "
          << tmp_out_file << " (" << err << ").");
  std::remove(tmp_in_file.c_str());
  std::remove(tmp_out_file.c_str());

  vector<int> aig2cnf(2*res->maxvar + 2, 0);
  aig2cnf[0] = -1;
  aig2cnf[1] = 1;
  int next_cnf_var = 0;
  int next_aig_var = 2;
  MASSERT(si.size() == res->num_inputs, "Strange response from ABC.");
  for(size_t cnt = 0; cnt < si.size(); ++cnt)
  {
    aig2cnf[next_aig_var] = si[cnt];
    aig2cnf[next_aig_var+1] = -si[cnt];
    next_aig_var += 2;
    if(si[cnt] >= next_cnf_var)
      next_cnf_var = si[cnt] + 1;
  }
  i_trans_ands_.clear();
  i_trans_ands_.reserve(res->num_ands);
  for(unsigned cnt = 0; cnt < res->num_ands; ++cnt)
  {

    CNFAnd new_and;
    new_and.r0 = aig2cnf[res->ands[cnt].rhs0];
    new_and.r1 = aig2cnf[res->ands[cnt].rhs1];
    new_and.l = next_cnf_var;
    i_trans_ands_.push_back(new_and);
    aig2cnf[res->ands[cnt].lhs] = next_cnf_var;
    aig2cnf[res->ands[cnt].lhs + 1] = -next_cnf_var;
    ++next_cnf_var;
  }

  if(i_copy_maps_.empty())
  {
    for(size_t c1 = 0; c1 < i_rename_maps_.size(); ++c1)
    {
      for(size_t c2 = 0; c2 < i_rename_maps_[c1].size(); ++c2)
      {
        int old_lit = i_rename_maps_[c1][c2];
        int old_var = old_lit < 0 ? -old_lit : old_lit;
        unsigned aig_lit = res->outputs[cnf2outidx[old_var]].lit;
        i_rename_maps_[c1][c2] = old_lit < 0 ? -aig2cnf[aig_lit] : aig2cnf[aig_lit];
      }
    }
    // BEGIN remove duplicate renamings:
    // (hardly ever helps, but does not cost too much)
    set<vector<int> > have;
    for(size_t c1 = 0; c1 < i_rename_maps_.size();)
    {
      pair<set<vector<int> >::const_iterator, bool> ins_res = have.insert(i_rename_maps_[c1]);
      if(ins_res.second == false) // already exists
      {
        i_rename_maps_[c1].swap(i_rename_maps_.back());
        i_rename_maps_.pop_back();
      }
      else
        ++c1;
    }
    // END remove duplicate renamings
  }
  else
  {
    for(set<int>::const_iterator it = i_prev_occ_.begin(); it != i_prev_occ_.end(); ++it)
    {
      int l1 = i_copy_maps_.back()[*it];
      int v1 = l1 < 0 ? -l1 : l1;
      unsigned aig_lit1 = res->outputs[cnf2outidx[v1]].lit;
      i_copy_maps_.back()[*it] = l1 < 0 ? -aig2cnf[aig_lit1] : aig2cnf[aig_lit1];
      int l2 = i_orig_prop_maps_.back()[*it];
      int v2 = l2 < 0 ? -l2 : l2;
      unsigned aig_lit2 = res->outputs[cnf2outidx[v2]].lit;
      i_orig_prop_maps_.back()[*it] = l2 < 0 ? -aig2cnf[aig_lit2] : aig2cnf[aig_lit2];
    }
  }

  aiger_reset(res);
  i_max_trans_var_ = next_cnf_var;
}

// -------------------------------------------------------------------------------------------
bool UnivExpander::shallAbort() const
{
  if(abort_if_ == NULL)
    return false;
  return (*abort_if_ != 0);
}

// -------------------------------------------------------------------------------------------
void UnivExpander::cleanupIData()
{
  i_trans_ands_.clear();
  i_trans_ands_.shrink_to_fit();
  i_rename_maps_.clear();
  i_rename_maps_.shrink_to_fit();
  i_copy_maps_.clear();
  i_copy_maps_.shrink_to_fit();
  i_orig_prop_maps_.clear();
  i_orig_prop_maps_.shrink_to_fit();
  i_occ_.clear();
  i_prev_occ_.clear();
  i_occ_in_i_rename_maps_.clear();
}
