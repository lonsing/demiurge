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
/// @file VarManager.h
/// @brief Contains the declaration of the class VarManager.
// -------------------------------------------------------------------------------------------

#ifndef VarManager_H__
#define VarManager_H__

#include "defines.h"
#include "VarInfo.h"

struct aiger;

// -------------------------------------------------------------------------------------------
///
/// @class VarManager
/// @brief Stores and maintains all Boolean variables that have (ever) been created.
///
/// This is mainly a container which stores all Boolean variables that have been created. It
/// also provides methods to create new variables, to retrieve variables of different kinds,
/// and to get additional information to variables.
///
/// This class is implemented as a Singleton. That is, you cannot instantiate objects of
/// this class with the constructor. Use the method @link #instance instance() @endlink to
/// obtain the one and only instance of this class.
class VarManager
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the one and only instance of this class.
///
/// This class is implemented as a Singleton. That is, you cannot instantiate objects of
/// this class with the constructor. Use this method to obtain the one and only instance of
/// this class.
///
/// @return The one and only instance of this class.
  static VarManager& instance();

// -------------------------------------------------------------------------------------------
///
/// @brief Pushes the current state of this object onto a stack.
///
/// The methods #pop() and #resetToLastPush() can then be used later to restore the pushed
/// state again. This can be convenient for the following reason. Certain methods may create
/// tons of temporary variables, which are not used after a certain point. All these temporary
/// variables may slow down this class and may consume quite a bit of memory. The temporary
/// variables can be discarded by calling a #push() before and a #pop() or #resetToLastPush()
/// after the code block that produces all the temporary variables. But there are other
/// scenarios, of course.
  static void push();

// -------------------------------------------------------------------------------------------
///
/// @brief Restores the VarManager to the last push and removes the data from the stack.
  static void pop();

// -------------------------------------------------------------------------------------------
///
/// @brief Restores the VarManager to the last push but does not pop the stack.
///
/// That is, you can call #resetToLastPush() several times to restore the state of the
/// VarManager. E.g.
/// @code
///  VarManager::instance().push()
///   // do something
///  VarManager::instance().resetToLastPush()
///   // do something else
///  VarManager::instance().resetToLastPush()
///   // and so on
/// @endcode
/// This is not possible with #pop().
  static void resetToLastPush();

// -------------------------------------------------------------------------------------------
///
/// @brief Initializes the VarManager from the AIGER input file structure.
///
/// That is, this method creates variables for all signals that occur in the AIGER circuit.
/// An additional vector defines which signals of the AIGER circuit are actually going to be
/// used.
///
/// @param aig An AIGER structure defining a circuit. This is typically the content of the
///        AIGER input file. We create variables for the signals occurring in this structure.
/// @param refs A vector defining which signals are actually going to be used. It maps AIGER
///        literals to either true or false. If a literal is mapped to true, we will create
///        a variable for it, otherwise not.
  void initFromAig(aiger *aig, const vector<bool>& refs);

// -------------------------------------------------------------------------------------------
///
/// @brief Completely clears the VarManager. All variables are gone then.
  void clear();

// -------------------------------------------------------------------------------------------
///
/// @brief Creates and returns a fresh variable of type VarInfo::TMP.
///
/// @return The (CNF representation of the) newly created fresh temporary variable.
  int createFreshTmpVar();

// -------------------------------------------------------------------------------------------
///
/// @brief Creates and returns a fresh named variable of type VarInfo::TMP.
///
/// @param name The name of the newly created temporary variable.
/// @return The (CNF representation of the) newly created fresh temporary variable.
  int createFreshTmpVar(const string& name);

// -------------------------------------------------------------------------------------------
///
/// @brief Creates and returns a fresh variable of type VarInfo::TEMPL_PARAMS.
///
/// @return The (CNF representation of the) newly created fresh parameter variable.
  int createFreshTemplParam();

// -------------------------------------------------------------------------------------------
///
/// @brief Creates and returns a fresh named variable of type VarInfo::TEMPL_PARAMS.
///
/// @param name The name of the newly created parameter variable.
/// @return The (CNF representation of the) newly created fresh parameter variable.
  int createFreshTemplParam(const string& name);

// -------------------------------------------------------------------------------------------
///
/// @brief Creates and returns a fresh variable of type VarInfo::PREV.
///
/// @return The (CNF representation of the) newly created fresh previous-time-step variable.
  int createFreshPrevVar();

// -------------------------------------------------------------------------------------------
///
/// @brief Creates and returns a fresh named variable of type VarInfo::PREV.
///
/// @param name The name of the newly created previous-time-step variable.
/// @return The (CNF representation of the) newly created fresh previous-time-step variable.
  int createFreshPrevVar(const string& name);

// -------------------------------------------------------------------------------------------
///
/// @brief Returns additional information to a CNF variable.
///
/// @param var_in_cnf The CNF representation of the variable for which additional information
///        is requested.  It is an error if the requested variable does not exist.
/// @return Additional information to a CNF variable.
  const VarInfo& getInfo(int var_in_cnf) const;

// -------------------------------------------------------------------------------------------
///
/// @brief Takes an AIGER literal and returns the corresponding CNF-literal.
///
/// @param aig_lit Some AIGER literal.
/// @return The corresponding CNF literal.
  int aigLitToCnfLit(unsigned aig_lit) const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the CNF representation of all variables of a given kind.
///
/// @param var_kind The requested variable kind.
/// @return The CNF representation of all variables of the given kind.
  const vector<int>& getVarsOfType(VarInfo::VarKind var_kind) const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the CNF representation of all variables that are not of type VarInfo::TMP.
///
/// @return The CNF representation of all variables that are not of type VarInfo::TMP.
  vector<int> getAllNonTempVars() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the present-state variable saying that we have a safety error now.
///
/// In the AIGER input file, the error is an output. It can depend on the current state and
/// the input. In order to have a notion of safe and unsafe states, we 'latch' this error
/// output and make it part of the state space. This is supposed to make the life of the
/// synthesis algorithm easier.
///
/// @return The (CNF representation of the) present-state variable saying that we have a
///         safety error now.
  int getPresErrorStateVar() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the next-state variable saying that we have a safety error next.
///
/// In the AIGER input file, the error is an output. It can depend on the current state and
/// the input. In order to have a notion of safe and unsafe states, we 'latch' this error
/// output and make it part of the state space. This is supposed to make the life of the
/// synthesis algorithm easier.
///
/// @return The (CNF representation of the) next-state variable saying that we have a
///         safety error in the next time step. This is the next-state copy of the variable
///         returned by #getPresErrorStateVar().
  int getNextErrorStateVar() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the maximum index of all CNF variables.
///
/// @return The maximum index of all CNF variables. That is, if the value N is returned, then
///         this means that there exists the CNF-variables 1,2,3,...,N. The exists no
///         CNF-variable with index greater than N.
  int getMaxCNFVar() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the maximum index of all AIGER variables.
///
/// @return The maximum index of all AIGER variables. That is, if the value N is returned,
///         then this means that there exists the AIGER-variables 1,2,3,...,N. The exists no
///         AIGER-variable with index greater than N.
  int getMaxAIGVar() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns a string containing all (or most) information stored in this object.
///
/// This is mainly useful for debugging.
///
/// @return A string containing all (or most) information stored in this object.
  string toString();

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief All variables created so far.
///
/// The variables are stored in a map where the key is the CNF-representation of the
/// variable, and the value is a VarInfo object storing additional information to this
/// variable.
  map<int, VarInfo> vars_;

// -------------------------------------------------------------------------------------------
///
/// @brief The maximum index of all CNF variables.
///
/// That means: there exists the CNF-variables 1,2,3,...,max_cnf_var_. The exists no
/// CNF-variable with index greater than max_cnf_var_.
  int max_cnf_var_;

// -------------------------------------------------------------------------------------------
///
/// @brief The maximum index of all AIGER variables.
///
/// That means: there exists the AIGER-variables 1,2,3,...,max_aig_var_. The exists no
/// AIGER-variable with index greater than max_aig_var_.
  int max_aig_var_;

// -------------------------------------------------------------------------------------------
///
/// @brief A map from AIGER literals to CNF literals.
///
/// This map is implemented as a vector because the number of AIGER literals is defined by
/// the input file and does not change.
  vector<int> aig_to_cnf_lit_map_;

// -------------------------------------------------------------------------------------------
///
/// @brief The CNF representation of all (uncontrollable) inputs (type VarManager::INPUT).
  vector<int> inputs_;

// -------------------------------------------------------------------------------------------
///
/// @brief The CNF representation of all control signals (type VarManager::CTRL).
  vector<int> ctrl_vars_;

// -------------------------------------------------------------------------------------------
///
/// @brief The CNF representation of all state variables (type VarManager::PRES_STATE).
  vector<int> pres_state_vars_;

// -------------------------------------------------------------------------------------------
///
/// @brief The CNF representation of all next-state variables (type VarManager::NEXT_STATE).
///
/// #pres_state_vars_ and #next_state_vars_ have the same length. #next_state_vars_
/// contains the next-state copies of all #pres_state_vars_.
  vector<int> next_state_vars_;

// -------------------------------------------------------------------------------------------
///
/// @brief The CNF representation of all temporary variables (type VarManager::TMP).
  vector<int> tmp_vars_;

// -------------------------------------------------------------------------------------------
///
/// @brief The CNF representation of all parameters (type VarManager::TEMPL_PARAMS).
  vector<int> templs_params_;

// -------------------------------------------------------------------------------------------
///
/// @brief The CNF representation of all previous-step variables (type VarManager::PREV).
  vector<int> prev_vars_;


// -------------------------------------------------------------------------------------------
///
/// @typedef map<int, VarInfo>::const_iterator VarsConstIter
/// @brief A type for a const-iterator over the variable map.
  typedef map<int, VarInfo>::const_iterator VarsConstIter;

// -------------------------------------------------------------------------------------------
///
/// @typedef map<int, VarInfo>::iterator VarIter
/// @brief A type for an iterator over the variable map.
  typedef map<int, VarInfo>::iterator VarIter;


private:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// The constructor is disabled (set private) as this method is implemented as a Singleton.
/// Use the method @link #instance instance() @endlink to obtain the one and only instance of
/// this class.
  VarManager();

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
///
/// The destructor is disabled (set private) as this method is implemented as a Singleton.
/// One cannot instantiate objects of this class, so there is no need to be able to delete
/// them.
  virtual ~VarManager();

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  VarManager(const VarManager &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  VarManager &operator= (const VarManager &other);

// -------------------------------------------------------------------------------------------
///
/// A stack for the instances of this object.
///
/// This is a singleton. However, we allow #push() and #pop(), so we store the stack of
/// VarManager configurations on a stack. The current VarManager is always located at the
/// top of the stack (the end of the vector).
  static vector<VarManager*> stack_;

};

#endif // VarManager_H__
