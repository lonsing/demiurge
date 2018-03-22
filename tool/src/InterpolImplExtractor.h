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
/// @file InterpolImplExtractor.h
/// @brief Contains the declaration of the class InterpolImplExtractor.
// -------------------------------------------------------------------------------------------

#ifndef InterpolImplExtractor_H__
#define InterpolImplExtractor_H__

#include "defines.h"
#include "CNFImplExtractor.h"

// -------------------------------------------------------------------------------------------
///
/// @class InterpolImplExtractor
/// @brief Supposed to extract circuits using interpolation; implemented in a separate branch.
///
/// This class has been implemented in a separate branch by a student (Patrick Klapfl).
/// It is not part of the release. Contact us if you would like to get the source code.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class InterpolImplExtractor : public CNFImplExtractor
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
  InterpolImplExtractor();

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~InterpolImplExtractor();

// -------------------------------------------------------------------------------------------
///
/// @brief Supposed to extract circuits using interpolation; implemented in a separate branch.
///
/// @param winning_region The winning region from which the circuit should be extracted.
/// @param neg_winning_region The negation of the winning region.
  virtual void run(const CNF &winning_region, const CNF &neg_winning_region);

protected:

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  InterpolImplExtractor(const InterpolImplExtractor &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  InterpolImplExtractor& operator=(const InterpolImplExtractor &other);

};

#endif // InterpolImplExtractor_H__
