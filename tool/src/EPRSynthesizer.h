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
/// @file EPRSynthesizer.h
/// @brief Contains the declaration of the class EPRSynthesizer.
// -------------------------------------------------------------------------------------------

#ifndef EPRSynthesizer_H__
#define EPRSynthesizer_H__

#include "defines.h"
#include "BackEnd.h"

// -------------------------------------------------------------------------------------------
///
/// @class EPRSynthesizer
/// @brief Implements a synthesis method based on reduction to EPR.
///
/// EPR (Effectively Propositional Logic) is a subset of first-order logic. EPR formulas are
/// of the form exists A: forall B: F, where A and B are sets of domain variables, and F is a
/// function-free formula in CNF. F can contain predicates over the variables of A and B.
/// These predicates are implicitly existentially quantified.
///
/// This class creates the following EPR formula:
/// forall x,i,x': (I(x) => W(x)) AND
///                (W(x) => P(x)) AND
///                (W(x) AND T(x,i,C(x,i),x') => W(x'))
/// Here, x,i, and x' are actually domain variables (and not Boolean variables), but there is
/// a one-to-one mapping.
/// W is a fresh predicate over the state variables. It represents the winning region.
/// C(x,i) is a set of predicates, one for each control signal. These predicates represent
/// the implementation of the control signals. The challenging part in constructing this
/// EPR formula is actually an efficient encoding of the transition relation T in EPR. In the
/// AIGER representation (and also in the CNF representation) the transition relation T uses
/// many temporary variables. The problem is that they are all quantified existentially,
/// which means that they have to be skolemized with predicates to get a valid EPR formula.
/// We analyze the structure of the AIGER graph in order to find out the minimal dependencies
/// for all these predicates. This is supposed to increase the performance.
/// Then the class uses iProver (http://www.cs.man.ac.uk/~korovink/iprover/) to solve the
/// formula. iProver cannot only give a YES/No answer for the satisfiability question. It
/// can also construct functions for the predicates. That is, iProver gives us the winning
/// region together with implementations for all control signals. It solves the synthesis
/// problem completely (and does not only compute a winning region).
///
/// @todo: At the moment we only construct the formula, and parse back the answer to the
///        realizability question. Parsing back the implementation for the controllable
///        signals remains to be done. (The iProver page promises to release a new version
///        with more output formats soon.)
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class EPRSynthesizer : public BackEnd
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
  EPRSynthesizer();

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~EPRSynthesizer();

// -------------------------------------------------------------------------------------------
///
/// @brief Executes the back-end (constructs an EPR formula an passes it to iProver).
///
/// This method just forwards the work to one of the following methods (runWith...()).
/// Each of these methods uses a different encoding of the problem into EPR.
///
/// @todo: At the moment we only construct the formula, and parse back the answer to the
///        realizability question. Parsing back the implementation for the controllable
///        signals remains to be done. (The iProver page promises to release a new version
///        with more output formats soon.)
///
/// @return True if the specification was realizable, false otherwise.
  virtual bool run();

// -------------------------------------------------------------------------------------------
///
/// @brief Executes the back-end with an EPR encoding that uses many skolem functions.
///
/// This method is the workhorse of this class.  It constructs the following EPR formula
/// in TPTP format:
/// forall x,i,x': (I(x) => W(x)) AND
///                (W(x) => P(x)) AND
///                (W(x) AND T(x,i,C(x,i),x') => W(x'))
/// Here, x,i, and x' are actually domain variables (and not Boolean variables), but there is
/// a one-to-one mapping.
/// W is a fresh predicate over the state variables. It represents the winning region.
/// C(x,i) is a set of predicates, one for each control signal. These predicates represent
/// the implementation of the control signals. The challenging part in constructing this
/// EPR formula is actually an efficient encoding of the transition relation T in EPR. In the
/// AIGER representation (and also in the CNF representation) the transition relation T uses
/// many temporary variables. The problem is that they are all quantified existentially,
/// which means that they have to be skolemized with predicates to get a valid EPR formula.
/// We analyze the structure of the AIGER graph in order to find out the minimal dependencies
/// for all these predicates. This is supposed to increase the performance.
/// After constructing the EPR formula this method then calls iProver
/// (http://www.cs.man.ac.uk/~korovink/iprover/) to solve it. iProver cannot only give a
/// YES/No answer for the satisfiability question. It
/// can also construct functions for the predicates. That is, iProver gives us the winning
/// region together with implementations for all control signals. It solves the synthesis
/// problem completely (and does not only compute a winning region).
///
/// @todo: At the moment we only construct the formula, and parse back the answer to the
///        realizability question. Parsing back the implementation for the controllable
///        signals remains to be done. (The iProver page promises to release a new version
///        with more output formats soon.)
///
/// @return True if the specification was realizable, false otherwise.
  virtual bool runWithSkolem();

// -------------------------------------------------------------------------------------------
///
/// @brief Executes the back-end with an EPR encoding that uses less skolem functions.
///
/// This method is almost identical to runWithSkolem(). The main difference is the encoding
/// of the transition relation. In the AIGER representation (and also in the CNF
/// representation) the transition relation T uses many temporary variables. They are
/// implicitly quantified existentially on the innermost level. While #runWithSkolem()
/// introduces skolem functions for each temporary variables, this method follows a different
/// approach: it uses universally quantified variables for the temporary variables, and
/// assumes that these universally quantified variables follow the rules of an AND gate
/// as an assumption (the left-hand side of an implication).
///
/// Unfortunately, this encoding seems to be slower.
///
/// @todo: At the moment we only construct the formula, and parse back the answer to the
///        realizability question. Parsing back the implementation for the controllable
///        signals remains to be done. (The iProver page promises to release a new version
///        with more output formats soon.)
///
/// @return True if the specification was realizable, false otherwise.
  virtual bool runWithLessSkolem();


protected:

// -------------------------------------------------------------------------------------------
///
/// @brief A helper that adds a clause to the EPR formula in TPTP format.
///
/// The clause is assigned a unique ID, and is added to the tptp_query_ in a nicely formatted
/// form, together with an optional comment.
///
/// @param clause A string representation of the clause to add.
/// @param comment A comment (useful for debugging). This parameter is optional.
  void addClause(const string &clause, string comment = "");

// -------------------------------------------------------------------------------------------
///
/// @brief A helper to negate an AIGER literal.
///
/// @param aig_lit An AIGER literal.
/// @return The negation of the passed AIGER literal.
  unsigned neg(unsigned aig_lit);

// -------------------------------------------------------------------------------------------
///
/// @brief The EPR formula in TPTP format.
  ostringstream tptp_query_;

// -------------------------------------------------------------------------------------------
///
/// @brief A monotonic counter to assign unique IDs to clauses.
  size_t next_clause_nr_;

// -------------------------------------------------------------------------------------------
///
/// @brief The path to the iProver executable.
  string path_to_iprover_;

// -------------------------------------------------------------------------------------------
///
/// @brief The name of the input file (containing the EPR formula) that is solved by iProver.
  string in_file_name_;

// -------------------------------------------------------------------------------------------
///
/// @brief The name of the file containing the response from iProver.
  string out_file_name_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  EPRSynthesizer(const EPRSynthesizer &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  EPRSynthesizer& operator=(const EPRSynthesizer &other);

};

#endif // EPRSynthesizer_H__
