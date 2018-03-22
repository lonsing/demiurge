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
/// @file CNFImplExtractor.h
/// @brief Contains the declaration of the class CNFImplExtractor.
// -------------------------------------------------------------------------------------------

#ifndef CNFImplExtractor_H__
#define CNFImplExtractor_H__

#include "defines.h"

struct aiger;
class CNF;

// -------------------------------------------------------------------------------------------
///
/// @class CNFImplExtractor
/// @brief An interface for all classes that can construct circuits from a CNF winning region.
///
/// This class is abstract, i.e., objects of this class cannot be instantiated. Instantiate
/// one of the derived classes instead. Derived classes have to implement the run() method.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.1.0
class CNFImplExtractor
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
  CNFImplExtractor();

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~CNFImplExtractor();

// -------------------------------------------------------------------------------------------
///
/// @brief Extracts a circuit from the winning region.
///
/// How this is done (interpolation, learning, etc.) depends on the concrete implementation
/// of this abstract class. The result is written into the output file specified by the user.
///
/// @param winning_region The winning region from which the circuit should be extracted. For
///        circuit extraction we need both the winning region and its negation. The negation
///        is computed inside this function. If you already have a negation, use the other
///        circuit extraction method of this class.
  virtual void extractCircuit(const CNF &winning_region);

// -------------------------------------------------------------------------------------------
///
/// @brief Extracts a circuit from the winning region and its negation.
///
/// How this is done (interpolation, learning, etc.) depends on the concrete implementation
/// of this abstract class. The result is written into the output file specified by the user.
///
/// @param winning_region The winning region from which the circuit should be extracted.
/// @param neg_winning_region The negation of the winning region.
  virtual void extractCircuit(const CNF &winning_region, const CNF &neg_winning_region);

// -------------------------------------------------------------------------------------------
///
/// @brief Logs statistical information about circuit extraction.
///
/// This method logs the overall execution time plus whatever detailed information is
/// collected by the concrete implementation of this class.
  virtual void logStatistics();

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Runs the circuit extractor.
///
/// This is the workhorse of the circuit extractor. The methods #extractCircuit mainly
/// forward the call to this method.
///
/// @param winning_region The winning region from which the circuit should be extracted.
/// @param neg_winning_region The negation of the winning region.
  virtual void run(const CNF &winning_region, const CNF &neg_winning_region) = 0;

// -------------------------------------------------------------------------------------------
///
/// @brief Logs statistical information about circuit extraction.
///
/// Override this method to provide more detailed statistical information. Per default, the
/// the method #logStatistics logs the overall execution time, and then calls this method.
  virtual void logDetailedStatistics();

// -------------------------------------------------------------------------------------------
///
/// @brief Optimizes a combinatorial AIGER circuit with ABC.
///
/// @note The returned structure must be deleted by the caller.
/// @param circuit The circuit to optimize.
/// @return The optimized version of the circuit. The returned aiger structure must be
///         deleted by the caller.
  aiger* optimizeWithABC(aiger *circuit) const;

// -------------------------------------------------------------------------------------------
///
/// @brief Takes a circuit defining the control signals and inserts it into the spec.
///
/// This method takes as argument an aiger circuit of the following form:
/// <ul>
///  <li> The circuit is combinational, i.e., has no latches.
///  <li> The inputs of the circuit are: the uncontrollable inputs from the spec (in the
///       order in which they appear in the spec) followed by the state-signals (the
///       output of the latches in the spec, in the order in which they appear in the spec).
///  <li> The outputs of the circuit are the controllable inputs in the order in which they
///       are defined in the spec.
/// </ul>
/// @param standalone_circuit A circuit defining the control signals.
/// @return The number of additional AND gates introduced into the spec.
  size_t insertIntoSpec(aiger *standalone_circuit) const;

// -------------------------------------------------------------------------------------------
///
/// @brief The circuit extraction time in CPU-seconds.
  double extraction_cpu_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief Like #extraction_cpu_time_ but real-time.
  size_t extraction_real_time_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  CNFImplExtractor(const CNFImplExtractor &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  CNFImplExtractor& operator=(const CNFImplExtractor &other);

};

#endif // CNFImplExtractor_H__
