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
/// @file TemplateSynth.h
/// @brief Contains the declaration of the class TemplateSynth.
// -------------------------------------------------------------------------------------------

#ifndef TemplateSynth_H__
#define TemplateSynth_H__

#include "CNF.h"
#include "BackEnd.h"

class QBFSolver;

// -------------------------------------------------------------------------------------------
///
/// @class TemplateSynth
/// @brief Implements a template-based synthesis using a QBF solver.
///
/// We are searching for a winning region W(x) such that:
/// <ol>
///  <li> I(x) => W(x): every initial state is contained in the winning region
///  <li> W(x) => P(x): every state of the winning region is safe
///  <li> forall x,i: exists c,x': W(x) => (T(x,i,c,x') & W(x')): from every state in the
///       winning region we can enforce to stay in the winning region by setting the c-signals
///       appropriately.
/// </ol>
/// In this class, we encode the search for such a winning region symbolically. We define a
/// template W(x,k) for the winning region. The variables k are template parameters. Their
/// values define a concrete function W(x). Then we simply solve the query
/// <br/> &nbsp;
/// exists k: forall x,i: exists c,x': I(x) => W(x,k) &
///                                    W(x,k) => P(x) &
///                                    W(x,k) => (T(x,i,c,x') & W(x',k))
/// <br/>
/// with a single QBF-solver call. The ask the solver for a satisfying assignment to the
/// variables k. These values define a concrete function W(x), which is our winning region.
/// The difficult question is how to define a generic template W(x,k) for the winning region.
/// At the moment, only one possibility is implemented, namely defining W(x,k) as a
/// parameterized CNF over the state variables x with parameters k. But there are many other
/// options like parameterized And-Inverter-Graphs with parameters defining the
/// interconnects, etc.
/// The CNF template fixes a maximum number N of clauses. It has three groups of parameters.
/// <ul>
///  <li> The parameters kc[i] (with 1 <= i <= N) define if clause i occurs in the CNF or not.
///  <li> The parameters kv[i][j] (with 1 <= i <= N, 1 <= j <= |x|) define if state variable
///       xj occurs in clause i or not.
///  <li> The parameters kn[i][j] (with 1 <= i <= N, 1 <= j <= |x|) define if state variable
///       xj occurs in clause i only negated or unnegated. If kv[i][j]=false, then kn[i][j]
///       is irrelevant.
/// </ul>
/// The union of all these parameters forms k. Concrete values for all k define a concrete
/// CNF formula over the state variables x (i.e., a concrete winning region). We chose N in
/// the following way: we start with N=1. On failure, we increase N (multiply by 4).
///
/// Finally the QBFCertImplExtractor is used to extract a circuit from the winning region.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.0.0
class TemplateSynth : public BackEnd
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
  TemplateSynth();

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~TemplateSynth();

// -------------------------------------------------------------------------------------------
///
/// @brief Executes this back-end.
///
/// The back-end works as explained in the description of the class.
///
/// @return True if the specification was realizable, false otherwise.
  virtual bool run();

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Computes the winning region as instantiation of a generic template.
///
/// This works as explained in the description of the class.
///
/// @return True if the specification was realizable, false otherwise.
  bool computeWinningRegion();

// -------------------------------------------------------------------------------------------
///
/// @brief Computes the winning region as instantiation of a generic CNF template.
///
/// This works as explained in the description of the class.
///
/// @param nr_of_clauses The number N of clauses to use in the template. Choosing a good value
///        for this parameter is difficult. If it is chosen to low, then we may not find a
///        solution even if one exists. If it is chosen to high, then we waste computational
///        resources.
/// @return True if a solution is found with the given number of clauses, false otherwise.
///         If this method returns false, then this does not mean that the specification is
///         unrealizable. It may be that nr_of_clauses has been chosen too low.
  bool findWinRegCNFTempl(size_t nr_of_clauses);

// -------------------------------------------------------------------------------------------
///
/// @brief The resulting winning region.
  CNF winning_region_;

// -------------------------------------------------------------------------------------------
///
/// @brief The QBF-solver to use for solving the queries.
  QBFSolver *qbf_solver_;

};

#endif // TemplateSynth_H__
