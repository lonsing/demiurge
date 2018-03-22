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

#include "CNFSet.h"
#include "VarManager.h"
#include "Logger.h"
#include "CNF.h"

extern "C" {
 #include "aiger.h"
}

// -------------------------------------------------------------------------------------------
CNFSet::CNFSet() : clauses_()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
CNFSet::CNFSet(const CNFSet &other): clauses_(other.clauses_)
{
  // nothing to be done
}

// -------------------------------------------------------------------------------------------
CNFSet::CNFSet(const CNF &other)
{
  const list<vector<int> >& clauses = other.getClauses();
  for(CNF::ClauseConstIter it = clauses.begin(); it != clauses.end(); ++it)
  {
    set<int> clause_set(it->begin(), it->end());
    clauses_.insert(clause_set);
  }
}

// -------------------------------------------------------------------------------------------
CNFSet& CNFSet::operator=(const CNFSet &other)
{
  clauses_ = other.clauses_;
  return *this;
}

// -------------------------------------------------------------------------------------------
CNFSet::~CNFSet()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
void CNFSet::clear()
{
  clauses_.clear();
}

// -------------------------------------------------------------------------------------------
void CNFSet::addClause(const vector<int> &clause)
{
  set<int> clause_set(clause.begin(), clause.end());
  clauses_.insert(clause_set);
}

// -------------------------------------------------------------------------------------------
void CNFSet::addNegClauseAsCube(const vector<int> &clause)
{
  for(size_t cnt = 0; cnt < clause.size(); ++cnt)
    add1LitClause(-clause[cnt]);
}

// -------------------------------------------------------------------------------------------
void CNFSet::addCube(const vector<int> &cube)
{
  for(size_t cnt = 0; cnt < cube.size(); ++cnt)
    add1LitClause(cube[cnt]);
}

// -------------------------------------------------------------------------------------------
void CNFSet::addNegCubeAsClause(const vector<int> &cube)
{
  set<int> neg_cube;
  for(size_t cnt = 0; cnt < cube.size(); ++cnt)
    neg_cube.insert(-cube[cnt]);
  clauses_.insert(neg_cube);
}

// -------------------------------------------------------------------------------------------
void CNFSet::add1LitClause(int lit1)
{
  set<int> clause;
  clause.insert(lit1);
  clauses_.insert(clause);
}

// -------------------------------------------------------------------------------------------
void CNFSet::add2LitClause(int lit1, int lit2)
{
  set<int> clause;
  clause.insert(lit1);
  clause.insert(lit2);
  clauses_.insert(clause);
}

// -------------------------------------------------------------------------------------------
void CNFSet::add3LitClause(int lit1, int lit2, int lit3)
{
  set<int> clause;
  clause.insert(lit1);
  clause.insert(lit2);
  clause.insert(lit3);
  clauses_.insert(clause);
}

// -------------------------------------------------------------------------------------------
void CNFSet::add4LitClause(int lit1, int lit2, int lit3, int lit4)
{
  set<int> clause;
  clause.insert(lit1);
  clause.insert(lit2);
  clause.insert(lit3);
  clause.insert(lit4);
  clauses_.insert(clause);
}

// -------------------------------------------------------------------------------------------
void CNFSet::addCNFSet(const CNFSet& cnf)
{
  clauses_.insert(cnf.clauses_.begin(), cnf.clauses_.end());
}

// -------------------------------------------------------------------------------------------
void CNFSet::swapWith(set<set<int> > &clauses)
{
  clauses_.swap(clauses);
}

// -------------------------------------------------------------------------------------------
size_t CNFSet::getNrOfClauses() const
{
  return clauses_.size();
}

// -------------------------------------------------------------------------------------------
size_t CNFSet::getNrOfLits() const
{
  size_t nr_of_lits = 0;
  for(ClauseConstIter it = clauses_.begin(); it != clauses_.end(); ++it)
    nr_of_lits += it->size();
  return nr_of_lits;
}

// -------------------------------------------------------------------------------------------
string CNFSet::toString() const
{
  ostringstream str;
  for(ClauseConstIter it = clauses_.begin(); it != clauses_.end(); ++it)
  {
    for(set<int>::const_iterator it2 = it->begin(); it2 != it->end(); ++it2)
      str << (*it2) << " ";
    str << "0" << endl;
  }
  return str.str();
}

// -------------------------------------------------------------------------------------------
CNF CNFSet::toCNF() const
{
  CNF res;
  for(ClauseConstIter it = clauses_.begin(); it != clauses_.end(); ++it)
  {
    vector<int> clause_vec(it->begin(), it->end());
    res.addClause(clause_vec);
  }
  return res;
}

// -------------------------------------------------------------------------------------------
const set<set<int> >& CNFSet::getClauses() const
{
  return clauses_;
}

// -------------------------------------------------------------------------------------------
bool CNFSet::isSatBy(const vector<int> &cube) const
{
  for(ClauseConstIter it = clauses_.begin(); it != clauses_.end(); ++it)
  {
    bool satisfied = false;
    for(set<int>::const_iterator it2 = it->begin(); it2 != it->end(); ++it2)
    {
      int lit_in_clause = *it2;
      for(size_t c2 = 0; c2 < cube.size(); ++c2)
      {
        if(lit_in_clause == cube[c2])
        {
          satisfied = true;
          break;
        }
      }
    }
    if(!satisfied)
      return false;
  }
  return true;
}

// -------------------------------------------------------------------------------------------
bool CNFSet::operator==(const CNFSet &other) const
{
  return clauses_ == other.clauses_;
}
