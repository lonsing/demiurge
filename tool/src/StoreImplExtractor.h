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
/// @file StoreImplExtractor.h
/// @brief Contains the declaration of the class StoreImplExtractor.
// -------------------------------------------------------------------------------------------

#ifndef StoreImplExtractor_H__
#define StoreImplExtractor_H__

#include "defines.h"
#include "CNFImplExtractor.h"

// -------------------------------------------------------------------------------------------
///
/// @class StoreImplExtractor
/// @brief A dummy CNFImplExtractor that only stores the winning region into a file.
///
/// This class is manly used to save computation time when benchmarking relation
/// determinization approaches: instead of computing the winning region each time, we can
/// store it into a file once and then just load it. This class is able to store a winning
/// region into a file. It does not really extract a circuit.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class StoreImplExtractor : public CNFImplExtractor
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
  StoreImplExtractor();

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~StoreImplExtractor();

// -------------------------------------------------------------------------------------------
///
/// @brief Stores the winning region into a file.
///
/// This method does not really extract a circuit. It only stores the winning region into a
/// file. This method is used to save computation time when benchmarking relation
/// determinization approaches: instead of computing the winning region each and every time,
/// we simply store it into a file and load it from there again.
///
/// @param winning_region The winning region to store.
  virtual void extractCircuit(const CNF &winning_region);

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Dummy method, does not do anything.
///
/// @param winning_region The winning region from which the circuit should be extracted.
/// @param neg_winning_region The negation of the winning region.
  virtual void run(const CNF &winning_region, const CNF &neg_winning_region);

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  StoreImplExtractor(const StoreImplExtractor &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  StoreImplExtractor& operator=(const StoreImplExtractor &other);

};

#endif // StoreImplExtractor_H__
