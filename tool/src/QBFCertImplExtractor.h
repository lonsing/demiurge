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
/// @file QBFCertImplExtractor.h
/// @brief Contains the declaration of the class QBFCertImplExtractor.
// -------------------------------------------------------------------------------------------

#ifndef QBFCertImplExtractor_H__
#define QBFCertImplExtractor_H__

#include "defines.h"

class CNF;

// -------------------------------------------------------------------------------------------
///
/// @class QBFCertImplExtractor
/// @brief Given a winning region in CNF, this method extracts a circuit using QBFCert.
///
/// QBFCert (see http://fmv.jku.at/qbfcert/) is a framework for computing Skolem or Herbrand
/// functions as witnesses for quantified Boolean formulas. We use it to compute circuits
/// from a winning region. Let W be the winning region. Then we can compute a circuit as a
/// skolem function for the c-signals in:
///   <br/> &nbsp; forall x,i: exists c,x': (!W) | (T & W') <br/>
/// However, for performance reasons, we actually compute Herbrand functions for the
/// c-signals in the formula:
///   <br/> &nbsp; exists x,i: forall c: exists x': W & T & !W' <br/>
/// QBFCert outputs the result in AIGER format. Hence, all we have to do in order to obtain
/// the final output of our tool is to embed this AIGER-circuits for the c-signals in the
/// input AIGER file.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.0.0
class QBFCertImplExtractor
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
  QBFCertImplExtractor();

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~QBFCertImplExtractor();

// -------------------------------------------------------------------------------------------
///
/// @brief Extracts a circuit from the winning region.
///
/// The result is written into the output file specified by the user.
/// This method uses QBFCert to compute a solution.
/// QBFCert (see http://fmv.jku.at/qbfcert/) is a framework for computing Skolem or Herbrand
/// functions as witnesses for quantified Boolean formulas. We use it to compute circuits
/// from a winning region. Let W be the winning region. Then we can compute a circuit as a
/// skolem function for the c-signals in:
///   <br/> &nbsp; forall x,i: exists c,x': (!W) | (T & W') <br/>
/// However, for performance reasons, we actually compute Herbrand functions for the
/// c-signals in the formula:
///   <br/> &nbsp; exists x,i: forall c: exists x': W & T & !W' <br/>
/// QBFCert outputs the result in AIGER format. Hence, all we have to do in order to obtain
/// the final output of our tool is to embed this AIGER-circuits for the c-signals in the
/// input AIGER file.
///
/// @param winning_region The winning region from which the circuit should be extracted. For
///        circuit extraction we need both the winning region and its negation. The negation
///        is computed inside this function. If you already have a negation, use the other
///        method.
  void extractCircuit(const CNF &winning_region);

// -------------------------------------------------------------------------------------------
///
/// @brief Extracts a circuit from the winning region and its negation.
///
/// The result is written into the output file specified by the user.
/// This method should be preferred if you have the winning region and its negation
/// available. The other method computes the negation by itself and may introduce quite a
/// lot of temporary variables.
///
/// This method uses QBFCert to compute a solution.
/// QBFCert (see http://fmv.jku.at/qbfcert/) is a framework for computing Skolem or Herbrand
/// functions as witnesses for quantified Boolean formulas. We use it to compute circuits
/// from a winning region. Let W be the winning region. Then we can compute a circuit as a
/// skolem function for the c-signals in:
///   <br/> &nbsp; forall x,i: exists c,x': (!W) | (T & W') <br/>
/// However, for performance reasons, we actually compute Herbrand functions for the
/// c-signals in the formula:
///   <br/> &nbsp; exists x,i: forall c: exists x': W & T & !W' <br/>
/// QBFCert outputs the result in AIGER format. Hence, all we have to do in order to obtain
/// the final output of our tool is to embed this AIGER-circuits for the c-signals in the
/// input AIGER file.
///
/// @param winning_region The winning region from which the circuit should be extracted.
/// @param neg_winning_region The negation of the winning region.
  void extractCircuit(const CNF &winning_region, const CNF &neg_winning_region);

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Parses the answer of QBFCert.
///
/// QBFCert is called in an external process and produces a an AIGER file that contains the
/// result (the circuits for the c-signals). This method parses the result. The main thing
/// this method does is that it renames variables. The AIGER file returned by QBFCert uses
/// the variable indices as they occur in the CNFs. We must map them back to the original
/// variable indices that were used in the AIGER input file.
///
/// @param answer The string-representation of the AIGER file produced by QBFCert.
/// @param and_gates An empty vector. This method writes the AND-gates to define the
///        c-signals into this vector.
/// @return The maximal variable index that is used in the resulting AND gates.
  size_t parseQBFCertAnswer(const string &answer,
                            vector<vector<int> > &and_gates) const;

// -------------------------------------------------------------------------------------------
///
/// @brief Dumps the final system into the output file specified by the user.
///
/// All we have to do in order to obtain the final output of our tool is to embed the
/// AIGER-circuits returned by QBFCert into input AIGER file. That is, the c-inputs have to
/// be removed and the AND-gates defining the c-signals have to be inserted. This is done
/// by this method.
///
/// @param max_var_idx The maximal variable index that is used in the resulting AND gates
///        (as returned by #parseQBFCertAnswer).
/// @param and_gates An empty vector. This method writes the AND-gates to define the
///        c-signals into this vector.
  void dumpResAigerLib(size_t max_var_idx, const vector<vector<int> > &and_gates);
};

#endif // QBFCertImplExtractor_H__
