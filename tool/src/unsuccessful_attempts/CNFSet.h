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

#ifndef CNFSet_H__
#define CNFSet_H__

#include "defines.h"

class CNF;

struct aiger;

class CNFSet
{
public:
  CNFSet();
  CNFSet(const CNFSet &other);
  CNFSet(const CNF &other);
  virtual CNFSet& operator=(const CNFSet &other);
  virtual ~CNFSet();
  void clear();
  void addClause(const vector<int> &clause);
  void addNegClauseAsCube(const vector<int> &clause);
  void addCube(const vector<int> &cube);
  void addNegCubeAsClause(const vector<int> &cube);
  void add1LitClause(int lit1);
  void add2LitClause(int lit1, int lit2);
  void add3LitClause(int lit1, int lit2, int lit3);
  void add4LitClause(int lit1, int lit2, int lit3, int lit4);
  void addCNFSet(const CNFSet& cnf);
  void swapWith(set<set<int> > &clauses);
  size_t getNrOfClauses() const;
  size_t getNrOfLits() const;
  string toString() const;
  CNF toCNF() const;
  const set<set<int> >& getClauses() const;
  bool isSatBy(const vector<int> &cube) const;
  bool isSubset(const vector<int> &c1, const vector<int> &c2) const;

  bool operator==(const CNFSet &other) const;

  typedef set<set<int> >::const_iterator ClauseConstIter;
  typedef set<set<int> >::iterator ClauseIter;

protected:
  set<set<int> > clauses_;

};

#endif // CNFSet_H__
