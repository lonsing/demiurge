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
/// @file IFMProofObligation.h
/// @brief Contains the declaration of the class IFMProofObligation.
// -------------------------------------------------------------------------------------------

#ifndef IFMProofObligation_H__
#define IFMProofObligation_H__

#include "defines.h"

// -------------------------------------------------------------------------------------------
///
/// @class IFMProofObligation
/// @brief A helper-class for IFM13Synth, representing an open proof obligation.
///
/// This is a simple container without any intelligence.
/// Basically, a proof obligation is a state together with a level. The intuitive meaning
/// is that we need to check if the rank of the state is greater than the level or not.
/// In addition, this class also stores information about the predecessor-state that was
/// responsible for adding the current state as a proof obligation. This enables some
/// optimizations.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.1.0
class IFMProofObligation
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor omitting the predecessor.
///
/// @param state_cube A full cube over the state-variables. It represents the state in
///        question.
/// @param state_level The level of the proof obligation (the proof obligation means: check
///        if the rank of the state greater than this level or not).
  IFMProofObligation(const vector<int> &state_cube, size_t state_level);

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor also setting a predecessor.
///
/// @param state_cube A full cube over the state-variables. It represents the state in
///        question.
/// @param state_level The level of the proof obligation (the proof obligation means: check
///        if the rank of the state greater than this level or not).
/// @param pre_state_in_cube A full cube over the state variables and the uncontrollable
///        inputs. It defines the predecessor of state_cube and the input that was used to
///        go from this predecessor to the state_cube.
/// @param pre_ctrl_cube The control signal values (in form of a full cube) that were used
///        to traverse from the predecessor to the current state.
  IFMProofObligation(const vector<int> &state_cube,
                     size_t state_level,
                     const vector<int> &pre_state_in_cube,
                     const vector<int> &pre_ctrl_cube);

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// Makes a (deep) copy of the IFMProofObligation.
///
/// @param other The source for creating the (deep) copy.
  IFMProofObligation(const IFMProofObligation &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// Makes this object a deep copy of another object.
///
/// @param other The source for creating the (deep) copy.
/// @return The result of the assignment, i.e, *this.
  IFMProofObligation& operator=(const IFMProofObligation &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~IFMProofObligation();

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the state in question in form of a full cube over the state variables.
///
/// @return The state in question in form of a full cube over the state variables.
  const vector<int>& getState() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the level in question.
///
/// The proof obligation means: check if the rank of the state greater than this level or not.
///
/// @return The level in question.
  size_t getLevel() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Checks if a preprocessor was set.
///
/// @return True if a predecessor was set (either via the constructor or with @link #setPre
///         setPre() @endlink. The methods @link #getPreStateInCube getPreStateInCube()
///         @endlink and @link #getPreCtrlCube getPreCtrlCube() @endlink only return useful
///         values if this method returns true.
  bool hasPre() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the predecessor together with the inputs of the transition.
///
/// @return A full cube over the state variables and the uncontrollable inputs. It defines
///         the predecessor of state in question and the input that was used to go from this
///         predecessor to the state in question.
  const vector<int>& getPreStateInCube() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the control values of the transition from the predecessor to the state.
///
/// @return The control signal values (in form of a full cube) that were used to traverse
///         from the predecessor to the current state.
  const vector<int>& getPreCtrlCube() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Sets the predecessor and the transition to the current state.
///
/// Setting the predecessor can also be done with the right constructor. This method does
/// the same thing, but after the object has been created.
///
/// @param pre_state_in_cube A full cube over the state variables and the uncontrollable
///        inputs. It defines the predecessor of state_cube and the input that was used to
///        go from this predecessor to the state_cube.
/// @param pre_ctrl_cube The control signal values (in form of a full cube) that were used
///        to traverse from the predecessor to the current state.
  void setPre(const vector<int> &pre_state_in_cube,  const vector<int> &pre_ctrl_cube);

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief A full cube over the state-variables. It represents the state in question.
  vector<int> state_cube_;

// -------------------------------------------------------------------------------------------
///
/// @brief The level of the proof obligation.
///
/// The proof obligation means: check if the rank of the state greater than this level or not.
  size_t state_level_;

// -------------------------------------------------------------------------------------------
///
/// @brief The predecessor of #state_cube_ and the inputs of the transition.
///
/// This is a full cube over the state variables and the uncontrollable inputs. It defines
/// the predecessor of state_cube and the input that was used to go from this predecessor to
/// the #state_cube_.
  vector<int> pre_state_in_cube_;

// -------------------------------------------------------------------------------------------
///
/// @brief The control signal values of the transition to #state_cube_.
///
/// The control signal values (in form of a full cube) that were used to traverse from the
/// predecessor to the current state.
  vector<int> pre_ctrl_cube_;

private:

};

#endif // IFMProofObligation_H__
