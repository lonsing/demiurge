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
/// @file OrigUnivExpander.cpp
/// @brief Contains the definition of the class OrigUnivExpander.
// -------------------------------------------------------------------------------------------


#include "OrigUnivExpander.h"
#include "VarManager.h"
#include "AIG2CNF.h"
#include "SatSolver.h"
#include "Options.h"

// -------------------------------------------------------------------------------------------
OrigUnivExpander::OrigUnivExpander() :
              s_(VarManager::instance().getVarsOfType(VarInfo::PRES_STATE)),
              i_(VarManager::instance().getVarsOfType(VarInfo::INPUT)),
              c_(VarManager::instance().getVarsOfType(VarInfo::CTRL)),
              n_(VarManager::instance().getVarsOfType(VarInfo::NEXT_STATE)),
              i_max_trans_var_(0)
{
  // Nothing to do. Initialization of the data structures is done lazily on demand.
}

// -------------------------------------------------------------------------------------------
OrigUnivExpander::~OrigUnivExpander()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
void OrigUnivExpander::univExpand(const CNF &orig, CNF &res, const vector<int> &vars_to_exp,
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
bool OrigUnivExpander::resetSolverIExp(const CNF &win_reg, SatSolver *solver_i, bool limit_size)
{
  if(i_rename_maps_.empty())
  {
    bool size_exceeded = initSolverIData(limit_size);
    if(size_exceeded)
      return size_exceeded;
  }

  // and initialize the solver:
  vector<int> vars_to_keep;
  vars_to_keep.reserve(s_.size() + c_.size() + i_.size());
  vars_to_keep.insert(vars_to_keep.end(), s_.begin(), s_.end());
  vars_to_keep.insert(vars_to_keep.end(), c_.begin(), c_.end());
  vars_to_keep.insert(vars_to_keep.end(), i_.begin(), i_.end());
  solver_i->startIncrementalSession(vars_to_keep, false);
  solver_i->incAddUnitClause(1);
  solver_i->incAddCNF(win_reg);
  solver_i->incAddCNF(i_exp_trans_);
  // now the only thing left in solver_i is the negation of the expanded winning region

  // initialize the clauses to copy:
  // General strategy: We do copying, renaming, and constant propagation first, and only then
  // we negate. Reason: Working on the original clauses is easier (and less effort) than on
  // the Tseitin encoding  of the negation.
  int max_var = i_max_trans_var_;
  list<list<vector<int> > > clauses; // each item is a CNF we have to negate later

  // fill the first CNF in clauses with the next-state copy of win_reg:
  const list<vector<int> >&win_clauses = win_reg.getClauses();
  clauses.push_back(list<vector<int> >());
  list<vector<int> > &target = clauses.back();
  for(CNF::ClauseConstIter i1 = win_clauses.begin(); i1 != win_clauses.end(); ++i1)
  {
    target.push_back(vector<int>());
    vector<int> &target_clause = target.back();
    target_clause.reserve(i1->size() + 1);
    target_clause.push_back(++max_var); // clause[0] is the clause-is-false Tseitin literal
    for(vector<int>::const_iterator i2 = i1->begin(); i2 != i1->end(); ++i2)
      target_clause.push_back(*i2 < 0 ? -i_s_to_ren_n_[-(*i2)] : i_s_to_ren_n_[*i2]);
  }

  // do the copying, renaming and constant propagation:
  for(size_t ccnt = 0; ccnt < c_.size(); ++ccnt)
  {
    if(limit_size)// check if we exceeded the size limit already:
    {
      size_t final_clause_extimate = i_exp_trans_.getNrOfClauses();
      list<list<vector<int> > >::const_iterator i1 = clauses.begin();
      for( ; i1 != clauses.end(); ++i1)
        for(list<vector<int> >::const_iterator i2 = i1->begin(); i2 != i1->end(); ++i2)
          final_clause_extimate += i2->size();
      if(final_clause_extimate > Options::instance().getSizeLimitForExpansion())
        return true;
    }

    const vector<int> &rename_map = i_rename_maps_[ccnt];
    const vector<int> &orig_prop_map = i_orig_prop_maps_[ccnt];

    // a separate rename map for the clause-false literals in the original
    // If a certain clause gets true or false, we need to propagate this information
    // to all copies of this clause. This is done with this rename map for the original
    // clauses (the set of original clauses may contain a clause and 20 copies of this
    // clause).
    vector<int> o_rename_clause_lits;
    o_rename_clause_lits.reserve(max_var + 1);
    // a separate rename map for the clause-false literals in the copy
    // similar to o_rename_clause_lits, but we also use this map for renaming the
    // clause-false literals consistently when changing clauses during copying.
    vector<int> c_rename_clause_lits;
    c_rename_clause_lits.reserve(max_var + 1);
    for(int cnt = 0; cnt < max_var + 1; ++cnt)
    {
      o_rename_clause_lits.push_back(cnt);
      c_rename_clause_lits.push_back(cnt);
    }

    size_t orig_size = clauses.size();
    list<list<vector<int> > >::iterator bit = clauses.begin();
    for(size_t bcnt = 0; bcnt < orig_size; ++bcnt) // iterate over CNFs
    {
      list<vector<int> > &orig_cnf = *bit;
      list<vector<int> > copy_cnf(orig_cnf);

      // do constant propagation in the original:

      // when processing the copy, we need to establish a link between the copied
      // clauses and the original clauses. The reason is that it is very likely
      // that the copy is exactly the same as the original. In this case, a shallow
      // copy (not copying the clause but only re-using its clause-false-literal)
      // suffices. The compare_with[i] contains a pointer to the clause from which
      // copy_cnf[i] has been copied. The entry is NULL if the original clause has
      // already been removed. In this case, we have to create a real copy.
      vector<vector<int>*> compare_with(orig_cnf.size(), NULL);
      bool remove_orig_cnf = false;
      CNF::ClauseIter oit = orig_cnf.begin();
      size_t orig_cnf_size = orig_cnf.size();
      for(size_t orig_cnt = 0; orig_cnt < orig_cnf_size; ++orig_cnt)
      {
        vector<int> &orig_clause = *oit;
        if(orig_clause.size() == 1) // This is a copy of something else. We rename it.
        {
          orig_clause[0] = o_rename_clause_lits[orig_clause[0]];
          if(orig_clause[0] == -1) // clause is true, can be removed
            oit = orig_cnf.erase(oit);
          else if(orig_clause[0] == 1)// clause is false, CNF can be removed
          {
            remove_orig_cnf = true;
            break;
          }
          else
            ++oit;
          continue;
        }
        bool remove_orig_clause = false;
        for(size_t lcnt = 1; lcnt < orig_clause.size(); )
        {
          int lit = orig_clause[lcnt];
          int prop_lit = lit < 0 ? -orig_prop_map[-lit] : orig_prop_map[lit];
          if(prop_lit == 1) // clause is true, can be removed
          {
            remove_orig_clause = true;
            o_rename_clause_lits[orig_clause[0]] = -1; // all copies are also true
            break;
          }
          else if(prop_lit == -1) // literal is false, can be removed
          {
            orig_clause[lcnt] = orig_clause.back();
            orig_clause.pop_back();
          }
          else
          {
            orig_clause[lcnt] = prop_lit;
            ++lcnt;
          }
        }
        if(orig_clause.size() == 1) // all literals removed; clause is false
        {
          o_rename_clause_lits[orig_clause[0]] = 1; // all copies are also false
          remove_orig_cnf = true;
          break;
        }
        else if(remove_orig_clause)
          oit = orig_cnf.erase(oit);
        else
        {
          compare_with[orig_cnt] = &orig_clause;
          ++oit;
        }
      }
      if(remove_orig_cnf)
      {
        compare_with = vector<vector<int>*>(orig_cnf_size, NULL);
        bit = clauses.erase(bit);
      }
      else
        ++bit;

      // do renaming and constant propagation in the copy:
      bool remove_copy_cnf = false;
      CNF::ClauseIter cit = copy_cnf.begin();
      size_t copy_cnf_size = copy_cnf.size();
      for(size_t copy_cnt = 0; copy_cnt < copy_cnf_size; ++copy_cnt)
      {
        vector<int> &copy_clause = *cit;

        if(copy_clause.size() == 1) // This is a copy of something else. We rename it.
        {
          copy_clause[0] = c_rename_clause_lits[copy_clause[0]];
          if(copy_clause[0] == -1) // clause is true, can be removed
            cit = copy_cnf.erase(cit);
          else if(copy_clause[0] == 1)// clause is false, CNF can be removed
          {
            remove_copy_cnf = true;
            break;
          }
          else
            ++cit;
          continue;
        }

        bool remove_copy_clause = false;
        for(size_t lcnt = 1; lcnt < copy_clause.size(); )
        {
          int lit = copy_clause[lcnt];
          int prop_lit = lit < 0 ? -rename_map[-lit] : rename_map[lit];
          if(prop_lit == 1) // clause is true, can be removed
          {
            remove_copy_clause = true;
            c_rename_clause_lits[copy_clause[0]] = -1; // all copies are also true
            break;
          }
          else if(prop_lit == -1) // literal is false, can be removed
          {
            copy_clause[lcnt] = copy_clause.back();
            copy_clause.pop_back();
          }
          else
          {
            copy_clause[lcnt] = prop_lit;
            ++lcnt;
          }
        }
        if(copy_clause.size() == 1) // all literals removed; clause is false
        {
          c_rename_clause_lits[copy_clause[0]] = 1; // all copies are also false
          remove_copy_cnf = true;
          break;
        }
        else if(remove_copy_clause)
          cit = copy_cnf.erase(cit);
        else
        {
          if(compare_with[copy_cnt] == NULL || copy_clause != *(compare_with[copy_cnt]))
          {
            // this clause is different from the original one (or the original has been
            // removed) so it gets a new clause-false-literal. In order to change the
            // clause-false-literals of all copies of this clause also consistently, we
            // need to store this information in our rename map.
            c_rename_clause_lits[copy_clause[0]] = ++max_var;
            copy_clause[0] = max_var;
          }
          else if (copy_clause.size() != 2) // unit clauses do not have to be stripped
            copy_clause.resize(1, 0);
          ++cit;
        }
      }

      // check if copy is equal to original:
      /*
      if(!remove_copy_cnf && orig_cnf.size() == copy_cnf.size())
      {
        bool equal = true;
        CNF::ClauseConstIter i1 = orig_cnf.begin();
        CNF::ClauseConstIter i2 = copy_cnf.begin();
        for(size_t clcnt = 0; clcnt < orig_cnf.size(); ++clcnt)
        {
          const vector<int> &c1 = *i1;
          const vector<int> &c2 = *i2;

          if((c2.size() == 1 && c2[0] == c1[0]) || (c2.size() != 1 && Utils::eq(c1, c2, 1)))
          {
              ++i1;
              ++i2;
          }
          else
          {
            equal = false;
            break;
          }
        }
        if(equal)
          remove_copy_cnf = true;
      }*/

      if(!remove_copy_cnf)
        clauses.push_back(copy_cnf);
    }
  }

  // Done with expansion, renaming and constant propagation. Now the actual negation of CNFs:

  // many variables may be unused because we deleted clauses. We rename the auxiliary
  // variables to avoid such 'holes'. (Very large variable indices makes Minisat slower).
  // CNF res;
  map<set<int>, int > reuse_hash_map;
  // set<set<int> > long_clauses; // does not pay off
  int new_max_var = i_max_trans_var_;
  vector<int> new_var_rename;
  new_var_rename.reserve(max_var + 1);
  for(int cnt = 0; cnt < max_var + 1; ++cnt)
    new_var_rename.push_back(cnt);
  while(!clauses.empty())
  {
    const list<vector<int> > &cnf = clauses.front();
    vector<int> one_clause_false;
    one_clause_false.reserve(cnf.size() + 1);
    for(CNF::ClauseConstIter cit = cnf.begin(); cit != cnf.end(); ++cit)
    {
      const vector<int> &clause = *cit;
      if(clause.size() == 2) // this is a unit clause, so we can use the literal itself
        one_clause_false.push_back(-clause[1]);
      else if (clause.size() == 1) // this is a copy, so we just reuse the clause-false-lit
        one_clause_false.push_back(new_var_rename[clause[0]]);
      else
      {
        set<int> clause_as_set;
        for(size_t lit_cnt = 1; lit_cnt < clause.size(); ++lit_cnt)
          clause_as_set.insert(clause[lit_cnt]);
        pair<set<int>, int> key_val_pair(clause_as_set, new_max_var + 1);
        pair<map<set<int>, int >::const_iterator, bool> mapit = reuse_hash_map.insert(key_val_pair);
        if(!mapit.second) // element was already there
        {
          one_clause_false.push_back(mapit.first->second);
          new_var_rename[clause[0]] = mapit.first->second;
        }
        else // element was newly inserted
        {
          int new_clause_false_lit = ++new_max_var;
          new_var_rename[clause[0]] = new_clause_false_lit;
          for(size_t lit_cnt = 1; lit_cnt < clause.size(); ++lit_cnt)
          {
            //res.add2LitClause(-new_clause_false_lit, -clause[lit_cnt]);
            solver_i->incAdd2LitClause(-new_clause_false_lit, -clause[lit_cnt]);
          }
          one_clause_false.push_back(new_clause_false_lit);
        }
      }
    }
    // also checking the one_clause_false-clauses for duplicates does not pay off:
    //set<int> one_clause_false_set;
    //for(size_t cnt = 0; cnt < one_clause_false.size(); ++cnt)
    //  one_clause_false_set.insert(one_clause_false[cnt]);
    //pair<set<set<int> >::iterator, bool> setit = long_clauses.insert(one_clause_false_set);
    //if(setit.second) // new element was inserted
    {
      //res.addClause(one_clause_false);
      solver_i->incAddClause(one_clause_false);
    }
    clauses.pop_front();
  }

  // cout << " win clauses: " << res.getNrOfClauses() << endl;
  // cout << " win lits: " << res.getNrOfLits() << endl;
  // cout << " all clauses: " << res.getNrOfClauses() + i_exp_trans_.getNrOfClauses() << endl;
  // cout << " all lits: " << res.getNrOfLits() + i_exp_trans_.getNrOfLits() << endl;

  return false;
}

// -------------------------------------------------------------------------------------------
void OrigUnivExpander::resetSolverCExp(SatSolver *solver_c,
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
  vector<int> nr_of_tmps_to_rename(i_.size(), 0);
  const map<int, set<VarInfo> > &deps = AIG2CNF::instance().getTmpDepsTrans();
  for(size_t i_idx = 0; i_idx < i_.size(); ++i_idx)
  {
    for(map<int, set<VarInfo> >::const_iterator i1 = deps.begin(); i1 != deps.end(); ++i1)
    {
      for(set<VarInfo>::const_iterator i2 = i1->second.begin(); i2 != i1->second.end(); ++i2)
      {
        if(i2->getKind() == VarInfo::CTRL || i2->getLitInCNF() == i_[i_idx])
        {
          nr_of_tmps_to_rename[i_idx]++;
          break;
        }
      }
    }
  }
  vector<int> sorted_i_indices;
  while(sorted_i_indices.size() < i_.size())
  {
    size_t smallest_idx = 0;
    for(size_t cnt = 1; cnt < i_.size(); ++cnt)
      if(nr_of_tmps_to_rename[cnt] < nr_of_tmps_to_rename[smallest_idx])
        smallest_idx = cnt;
    sorted_i_indices.push_back(smallest_idx);
    nr_of_tmps_to_rename[smallest_idx] = numeric_limits<size_t>::max();
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
  c_rename_maps_.push_back(i_s_to_ren_n_);

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

  solver_c->startIncrementalSession(ext_keep, false);
  solver_c->incAddUnitClause(1);
  for(list<vector<int> >::const_iterator it = trans_ands.begin(); it != trans_ands.end(); ++it)
  {
    solver_c->incAdd2LitClause(-((*it)[2]), (*it)[1]);
    solver_c->incAdd2LitClause(-((*it)[2]), (*it)[0]);
    solver_c->incAdd3LitClause((*it)[2], -((*it)[1]), -((*it)[0]));
  }
}

// -------------------------------------------------------------------------------------------
void OrigUnivExpander::addExpNxtClauseToC(const vector<int> &clause, SatSolver *solver_c)
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
bool OrigUnivExpander::initSolverIData(bool limit_size)
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

  i_max_trans_var_ = AIG2CNF::instance().getTmpDepsTrans().rbegin()->first;
  //list<int> death_row;

  i_rename_maps_.reserve(c_.size());
  i_orig_prop_maps_.reserve(c_.size());
  for(size_t idx = 0; idx < c_.size(); ++idx)
  {
    if(limit_size) // check if we already exceeded the limit:
    {
      if(trans_ands.size()*3 > Options::instance().getSizeLimitForExpansion())
        return true;
    }

    // build the rename map:
    i_rename_maps_.push_back(vector<int>());
    i_orig_prop_maps_.push_back(vector<int>());
    vector<int> &rename_map = i_rename_maps_.back();
    vector<int> &orig_prop_map = i_orig_prop_maps_.back();
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
      //if(remove_orig && remove_copy)
      //  death_row.push_back(orig_l);
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
          int new_l = ++i_max_trans_var_;
          rename_map[orig_l] = new_l;
          vector<int> new_and(3, 0);
          new_and[0] = copy_prop_r0;
          new_and[1] = copy_prop_r1;
          new_and[2] = new_l;
          trans_ands.push_back(new_and);
        }
      }
    }
  }

  for(list<vector<int> >::const_iterator it = trans_ands.begin(); it != trans_ands.end(); ++it)
  {
    i_exp_trans_.add2LitClause(-((*it)[2]), (*it)[1]);
    i_exp_trans_.add2LitClause(-((*it)[2]), (*it)[0]);
    i_exp_trans_.add3LitClause((*it)[2], -((*it)[1]), -((*it)[0]));
  }
  //cout << " AND clauses: " << i_exp_trans_.getNrOfClauses() << endl;
  //cout << " AND lits: " << i_exp_trans_.getNrOfLits() << endl;
  return false;
}
