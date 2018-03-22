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
/// @file LoadSynth.h
/// @brief Contains the declaration of the class LoadSynth.
// -------------------------------------------------------------------------------------------

#ifndef LoadSynth_H__
#define LoadSynth_H__

#include "defines.h"
#include "BackEnd.h"

class CNFImplExtractor;

// -------------------------------------------------------------------------------------------
///
/// @class LoadSynth
/// @brief A dummy back-end which loads the winning region from a file.
///
/// This back-end is used to reduce the computation time when benchmarking relation
/// determinization approaches. It loads the winning region from a file instead of computing
/// it. The StoreImplExtractor can save the winning region into a file. We assume that this
/// has been done before.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.1.0
class LoadSynth : public BackEnd
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// @param impl_extractor The engine to use for circuit extraction. It will be deleted by
///        this class.
  LoadSynth(CNFImplExtractor *impl_extractor);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~LoadSynth();

// -------------------------------------------------------------------------------------------
///
/// @brief Loads the winning region from a file and runs the circuit extractor.
///
/// This method is used to reduce the computation time when benchmarking relation
/// determinization approaches. It loads the winning region from a file instead of computing
/// it. Then it runs the circuit extraction. The StoreImplExtractor can save the winning
/// region into a file. We assume that this has been done before.
///
/// @return Always true.
  virtual bool run();

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief The engine to use for circuit extraction.
///
/// It will be deleted by this class (in the destructor).
  CNFImplExtractor *impl_extractor_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  LoadSynth(const LoadSynth &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  LoadSynth& operator=(const LoadSynth &other);

};

#endif // LoadSynth_H__
