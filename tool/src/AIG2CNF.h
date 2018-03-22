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
/// @file AIG2CNF.h
/// @brief Contains the declaration of the class AIG2CNF.
// -------------------------------------------------------------------------------------------

#ifndef AIG2CNF_H__
#define AIG2CNF_H__

#include "defines.h"
#include "CNF.h"

struct aiger;

// -------------------------------------------------------------------------------------------
///
/// @class AIG2CNF
/// @brief Transforms an AIGER specification into CNF.
///
/// This class creates CNF formulas representing the transition relation as well as the
/// initial state, and the set of safe states from an AIGER specification. It does not parse
/// the AIGER file itself, but expects that this has already been done with the AIGER
/// utility tools (see http://fmv.jku.at/aiger/). This class then analyzes the resulting
/// 'aiger' structure to build the corresponding CNFs. It is compatible with AIGER version
/// 1.9.4.
/// This class can parse AIGER structures that are compatible with input format for the
/// synthesis competition (see http://www.syntcomp.org/). That is, the AIGER structure is
/// expected to have exactly one output: This output signals that an error occurred. The
/// synthesis problem addressed by this tool is to prevent the error-output form over
/// becoming TRUE. Controllable inputs are identified via their name. If an input signal
/// has a name that starts with 'controllable_', then it is considered as controllable.
///
/// The resulting transition relation is a CNF T(x,i,c,x') that talks about the current
/// state bits x, the uncontrollable inputs i, the controllable inputs c, and the next
/// state copies x' of the state bits x. The transition relation is
/// <ul>
///  <li> complete, i.e., forall x,i,c: exists x':  T(x,i,c,x').
///  <li> deterministic, i.e., forall x,i,c,x1',x2':  (T(x,i,c,x1') AND T(x,i,c,x2')) implies
///       (x1' = x2')
/// </ul>
/// This means: for every state and input, the next state is uniquely defined.
///
/// There is only one initial state. It is the state where all state-bits are set to FALSE.
/// (This may change with newer AIGER versions in the future).
///
/// The set of safe states is identified by one special state bit. This special state bit
/// does not exist in the original AIGER file but is introduced by this class. This
/// transformation (encoded in the transition relation) is supposed to make the life of the
/// synthesis algorithms easier.
///
/// This class is implemented as a Singleton. That is, you cannot instantiate objects of
/// this class with the constructor. Use the method @link #instance instance() @endlink to
/// obtain the one and only instance of this class.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.0.0
class AIG2CNF
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
  static AIG2CNF& instance();

// -------------------------------------------------------------------------------------------
///
/// @brief Initializes this class from an aiger structure.
///
/// This method actually builds the CNFs formulas representing the transition relation as
/// well as the initial state, and the set of safe states from an AIGER specification.
/// This is where all the magic happens. This method is called once in beginning before
/// a back-end is started. The back-ends can then access the constructed CNFs. This method
/// also initializes the VarManager with the correct sets of variables.
///
/// This method can parse AIGER structures that are compatible with input format for the
/// synthesis competition (see http://www.syntcomp.org/). That is, the AIGER structure is
/// expected to have exactly one output: This output signals that an error occurred. The
/// synthesis problem addressed by this tool is to prevent the error-output form over
/// becoming TRUE. Controllable inputs are identified via their name. If an input signal
/// has a name that starts with 'controllable_', then it is considered as controllable.
///
/// The resulting transition relation is a CNF T(x,i,c,x') that talks about the current
/// state bits x, the uncontrollable inputs i, the controllable inputs c, and the next
/// state copies x' of the state bits x. The transition relation is
/// <ul>
///  <li> complete, i.e., forall x,i,c: exists x':  T(x,i,c,x').
///  <li> deterministic, i.e., forall x,i,c,x1',x2':  (T(x,i,c,x1') AND T(x,i,c,x2')) implies
///       (x1' = x2')
/// </ul>
/// This means: for every state and input, the next state is uniquely defined.
///
/// There is only one initial state. It is the state where all state-bits are set to FALSE.
/// (This may change with newer AIGER versions in the future).
///
/// The set of safe states is identified by one special state bit. This special state bit
/// does not exist in the original AIGER file but is introduced by this class. This
/// transformation (encoded in the transition relation) is supposed to make the life of the
/// synthesis algorithms easier.
///
/// @pre aig != NULL
/// @pre aig->num_outputs == 1
/// @param aig The aiger structure as parsed by the AIGER utilities. The current
///        implementation assumes version 1.9.4 of the AIGER utilities.
  void initFromAig(aiger *aig);

// -------------------------------------------------------------------------------------------
///
/// @brief Clears all the CNFs constructed by @link #initFromAig initFromAig() @endlink.
  void clear();

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the transition relation in CNF.
///
/// The returned transition relation is a CNF T(x,i,c,x') that talks about the current
/// state bits x, the uncontrollable inputs i, the controllable inputs c, and the next
/// state copies x' of the state bits x. The transition relation is
/// <ul>
///  <li> complete, i.e., forall x,i,c: exists x':  T(x,i,c,x').
///  <li> deterministic, i.e., forall x,i,c,x1',x2':  (T(x,i,c,x1') AND T(x,i,c,x2')) implies
///       (x1' = x2')
/// </ul>
/// This means: for every state and input, the next state is uniquely defined.
///
/// @return The transition relation in CNF.
  const CNF& getTrans() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns a CNF saying that the transition holds iff a certain literal is true.
///
/// The returned CNF asserts T(x,i,c,x') <-> l, where l is the literal returned by
/// @link #getT getT() @endlink.
/// This can be useful if you need the negated transition relation, if you want to use the
/// the transition relation in some complicated formula, or if your formula should contain
/// the transition relation several times (e.g. A & T | B & !T) .
///
/// @return A CNF saying that the transition holds iff a certain literal is true.
  const CNF& getTransEqT() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the literal assigned by @link #getTransEqT getTransEqT() @endlink.
///
/// The method @link #getTransEqT getTransEqT() @endlink returns a CNF that asserts
/// T(x,i,c,x') <-> l, where l is the literal returned by this method.
/// This can be useful if you need the negated transition relation, if you want to use the
/// the transition relation in some complicated formula, or if your formula should contain
/// the transition relation several times (e.g. A & T | B & !T) .
///
/// @return The literal assigned by @link #getTransEqT getTransEqT() @endlink
  int getT() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns a CNF representing the set of safe states.
///
/// The set of safe states is actually characterized by one single state bit, namely the
/// states where the variable VarManager::instance().getPresErrorStateVar() is FALSE.
/// The returned CNF asserts exactly this.
///
/// @return A CNF representing the set of safe states.
  const CNF& getSafeStates() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns a CNF representing the set of unsafe states.
///
/// The set of unsafe states is actually characterized by one single state bit, namely the
/// states where the variable VarManager::instance().getPresErrorStateVar() is TRUE.
/// The returned CNF asserts exactly this.
///
/// @return A CNF representing the set of unsafe states.
  const CNF& getUnsafeStates() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns a CNF saying that the next state is safe.
///
/// The set of safe next states is actually characterized by one single state bit, namely the
/// next states where the variable VarManager::instance().getNextErrorStateVar() is FALSE.
/// The returned CNF asserts exactly this.
///
/// @return A CNF saying that the next state is safe.
  const CNF& getNextSafeStates() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns a CNF saying that the next state is unsafe.
///
/// The set of unsafe next states is actually characterized by one single state bit, namely
/// the next states where the variable VarManager::instance().getNextErrorStateVar() is TRUE.
/// The returned CNF asserts exactly this.
///
/// @return A CNF saying that the next state is unsafe.
  const CNF& getNextUnsafeStates() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns a CNF representing the initial state.
///
/// There is exactly one initial state (this is a restriction imposed by the AIGER format
/// which serves as input for our tool). Furthermore, the initial state is always
/// characterized by having all state bits set to FALSE. However, this may change with
/// future AIGER versions.
///
/// @return A CNF representing the initial state.
  const CNF& getInitial() const;

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief The transition relation in CNF.
///
/// The transition relation is a CNF T(x,i,c,x') that talks about the current
/// state bits x, the uncontrollable inputs i, the controllable inputs c, and the next
/// state copies x' of the state bits x. The transition relation is
/// <ul>
///  <li> complete, i.e., forall x,i,c: exists x':  T(x,i,c,x').
///  <li> deterministic, i.e., forall x,i,c,x1',x2':  (T(x,i,c,x1') AND T(x,i,c,x2')) implies
///       (x1' = x2')
/// </ul>
/// This means: for every state and input, the next state is uniquely defined.
  CNF trans_;

// -------------------------------------------------------------------------------------------
///
/// @brief A CNF saying that the transition holds iff @link t_ t_ @endlink is true.
///
/// This CNF asserts T(x,i,c,x') <-> @link t_ t_ @endlink.
/// This can be useful if you need the negated transition relation, if you want to use the
/// the transition relation in some complicated formula, or if your formula should contain
/// the transition relation several times (e.g. A & T | B & !T) .
  CNF trans_eq_t_;

// -------------------------------------------------------------------------------------------
///
/// @brief The literal assigned by @link #trans_eq_t_ trans_eq_t_ @endlink.
///
/// The CNF @link #trans_eq_t_ trans_eq_t_ @endlink asserts
/// T(x,i,c,x') <-> @link t_ t_ @endlink.
  int t_;

// -------------------------------------------------------------------------------------------
///
/// @brief A CNF representing the set of safe states.
///
/// The set of safe states is actually characterized by one single state bit, namely the
/// states where the variable VarManager::instance().getPresErrorStateVar() is FALSE.
/// This CNF asserts exactly this.
  CNF safe_;

// -------------------------------------------------------------------------------------------
///
/// @brief A CNF representing the set of unsafe states.
///
/// The set of unsafe states is actually characterized by one single state bit, namely the
/// states where the variable VarManager::instance().getPresErrorStateVar() is TRUE.
/// This CNF asserts exactly this.
  CNF unsafe_;

// -------------------------------------------------------------------------------------------
///
/// @brief A CNF saying that the next state is safe.
///
/// The set of safe next states is actually characterized by one single state bit, namely the
/// next states where the variable VarManager::instance().getNextErrorStateVar() is FALSE.
/// This CNF asserts exactly this.
  CNF next_safe_;

// -------------------------------------------------------------------------------------------
///
/// @brief A CNF saying that the next state is unsafe.
///
/// The set of unsafe next states is actually characterized by one single state bit, namely
/// the next states where the variable VarManager::instance().getNextErrorStateVar() is TRUE.
/// This CNF asserts exactly this.
  CNF next_unsafe_;

// -------------------------------------------------------------------------------------------
///
/// @brief A CNF representing the initial state.
///
/// There is exactly one initial state (this is a restriction imposed by the AIGER format
/// which serves as input for our tool). Furthermore, the initial state is always
/// characterized by having all state bits set to FALSE. However, this may change with
/// future AIGER versions.
  CNF initial_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// The constructor is disabled (set private) as this method is implemented as a Singleton.
/// Use the method @link #instance instance() @endlink to obtain the one and only instance of
/// this class.
  AIG2CNF();

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
///
/// The destructor is disabled (set private) as this method is implemented as a Singleton.
/// One cannot instantiate objects of this class, so there is no need to be able to delete
/// them.
  virtual ~AIG2CNF();

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  AIG2CNF(const AIG2CNF &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  AIG2CNF &operator= (const AIG2CNF &other);

// -------------------------------------------------------------------------------------------
///
/// @brief The one and only instance of this class.
  static AIG2CNF *instance_;
};


#endif // AIG2CNF_H__
