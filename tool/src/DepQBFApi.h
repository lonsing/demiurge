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
/// @file DepQBFApi.h
/// @brief Contains the declaration of the class DepQBFApi.
// -------------------------------------------------------------------------------------------

#ifndef DepQBFApi_H__
#define DepQBFApi_H__

#include "defines.h"
#include "QBFSolver.h"

class QDPLL;

// -------------------------------------------------------------------------------------------
///
/// @class DepQBFApi
/// @brief Interfaces the DepQBF QBF-solver via its API.
///
/// This class represents an interface to the QBF-solver DepQBF (see
/// https://github.com/lonsing/depqbf). For a given Quantified Boolean formula (a CNF with
/// a quantifier prefix), this class is able to determine satisfiability. Furthermore, in
/// case of satisfiability, it can extract satisfying assignments for variables quantified
/// existentially on the outermost level.
///
/// This interface to DepQBF can operated in two modes: with and without Bloqqer as
/// pre-processor.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.0.0
class DepQBFApi : public QBFSolver
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// @param use_bloqqer True if bloqqer should be used as pre-processor, false if no
///        pre-processing should be used. If this parameter is omitted, no pre-processing is
///        used.
  DepQBFApi(bool use_bloqqer = false);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~DepQBFApi();

// -------------------------------------------------------------------------------------------
///
/// @brief Checks if a given QBF is satisfiable (with quantification over variable kinds).
///
/// The QBF consists of a quantifier prefix and a Boolean formula in CNF. The quantifier
/// prefix assigns an existential or universal quantifier to every kind of variable.
///
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to every kind of variable that
///        occurs in the cnf.
/// @param cnf A Boolean formula in CNF.
/// @return True if the QBF is satisfiable, false otherwise.
  virtual bool isSat(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                     const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief Checks if a given QBF is satisfiable (with quantification over variable sets).
///
/// The QBF consists of a quantifier prefix and a Boolean formula in CNF. The quantifier
/// prefix assigns an existential or universal quantifier to different sets variable.
///
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to different sets of variables
///        occurring in the cnf.
/// @param cnf A Boolean formula in CNF.
/// @return True if the QBF is satisfiable, false otherwise.
  virtual bool isSat(const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                     const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief Checks satisfiability and extracts a model (quantifying over variable kinds).
///
/// Just like @link #isSat isSat() @endlink, this method checks a quantified Boolean formula
/// for satisfiability. If the QBF is satisfiable, this method also extracts a model
/// (a satisfying assignment) for all variables which are quantified existentially on the
/// outermost level. The model is provided as cube: negated variables in the cube are FALSE,
/// unnegated ones are TRUE.
///
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to every kind of variable that
///        occurs in the cnf.
/// @param cnf A Boolean formula in CNF.
/// @param model The resulting model in form of a cube in case of satisfiability. Negated
///        variables in the cube are FALSE, unnegated ones are TRUE. If the formula is
///        unsatisfiable (this method returns false) then this parameter is not modified.
/// @return True if the QBF is satisfiable, false otherwise.
  virtual bool isSatModel(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                          const CNF& cnf,
                          vector<int> &model);

// -------------------------------------------------------------------------------------------
///
/// @brief Checks satisfiability and extracts a model (quantifying over variable sets).
///
/// Just like @link #isSat isSat() @endlink, this method checks a quantified Boolean formula
/// for satisfiability. If the QBF is satisfiable, this method also extracts a model
/// (a satisfying assignment) for all variables which are quantified existentially on the
/// outermost level. The model is provided as cube: negated variables in the cube are FALSE,
/// unnegated ones are TRUE.
///
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to different sets of variables
///        occurring in the cnf.
/// @param cnf A Boolean formula in CNF.
/// @param model The resulting model in form of a cube in case of satisfiability. Negated
///        variables in the cube are FALSE, unnegated ones are TRUE. If the formula is
///        unsatisfiable (this method returns false) then this parameter is not modified.
/// @return True if the QBF is satisfiable, false otherwise.
  virtual bool isSatModel(const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                          const CNF& cnf,
                          vector<int> &model);

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Initializes the pre-processor with a QBF (quantifying over variable kinds).
///
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to every kind of variable that
///        occurs in the cnf.
/// @param cnf A Boolean formula in CNF.
  void initBloqqer(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                   const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief Initializes the pre-processor with a QBF (quantifying over variable sets).
///
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to different sets of variables
///        occurring in the cnf.
/// @param cnf A Boolean formula in CNF.
  void initBloqqer(const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                   const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief Initializes a DepQBF instance with a QBF (quantifying over variable kinds).
///
/// @param solver The DepQBF solver instance to initialize.
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to every kind of variable that
///        occurs in the cnf.
/// @param cnf A Boolean formula in CNF.
  void initDepQBF(QDPLL *solver,
                  const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                  const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief Initializes a DepQBF instance with a QBF (quantifying over variable sets).
///
/// @param solver The DepQBF solver instance to initialize.
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to different sets of variables
///        occurring in the cnf.
/// @param cnf A Boolean formula in CNF.
  void initDepQBF(QDPLL *solver,
                  const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                  const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief A debug function to cross-check bloqqer verdicts (SAT or UNSAT).
///
/// The QBF is also solved with DepQBF without pre-processing and the two verdicts are
/// compared. This method is used to debug bloqqer.
///
/// @param bloqqer_verdict The result returned by bloqqer. The value 10 means SAT, the value
///        20 means UNSAT.
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to every kind of variable that
///        occurs in the cnf.
/// @param cnf A Boolean formula in CNF.
  void debugCheckBloqqerVerdict(int bloqqer_verdict,
                                const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                                const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief A debug function to cross-check bloqqer verdicts (SAT or UNSAT).
///
/// The QBF is also solved with DepQBF without pre-processing and the two verdicts are
/// compared. This method is used to debug bloqqer.
///
/// @param bloqqer_verdict The result returned by bloqqer. The value 10 means SAT, the value
///        20 means UNSAT.
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to different sets of variables
///        occurring in the cnf.
/// @param cnf A Boolean formula in CNF.
  void debugCheckBloqqerVerdict(int bloqqer_verdict,
                                const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                                const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief A debug function to cross-check models produced by bloqqer.
///
/// DepQBF without preprocessing is used to check if the model found by bloqqer indeed
/// satisfies the QBF. This method is used to debug bloqqer.
///
/// @param model The satisfying assignment found by bloqqer. Negated variables in the vector
///        are FALSE, unnegated ones are TRUE.
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to every kind of variable that
///        occurs in the cnf.
/// @param cnf A Boolean formula in CNF.
  void debugCheckBloqqerModel(const vector<int> &model,
                              const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                              const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief A debug function to cross-check models produced by bloqqer.
///
/// DepQBF without preprocessing is used to check if the model found by bloqqer indeed
/// satisfies the QBF. This method is used to debug bloqqer.
///
/// @param model The satisfying assignment found by bloqqer. Negated variables in the vector
///        are FALSE, unnegated ones are TRUE.
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to different sets of variables
///        occurring in the cnf.
/// @param cnf A Boolean formula in CNF.
  void debugCheckBloqqerModel(const vector<int> &model,
                              const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                              const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief A flag indicating whether or not the pre-processor bloqqer should be used.
  bool use_bloqqer_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  DepQBFApi(const DepQBFApi &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  DepQBFApi& operator=(const DepQBFApi &other);

};

#endif // DepQBFApi_H__
