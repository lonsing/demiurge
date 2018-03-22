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
/// @file IFM13Synth.h
/// @brief Contains the declaration of the class IFM13Synth.
// -------------------------------------------------------------------------------------------

#ifndef IFM13Synth_H__
#define IFM13Synth_H__

#include "defines.h"
#include "CNF.h"
#include "BackEnd.h"
#include "IFMProofObligation.h"

class SatSolver;
class CNFImplExtractor;

// -------------------------------------------------------------------------------------------
///
/// @class IFM13Synth
/// @brief Implements the SAT-based synthesis method published at IFM'13.
///
/// This class is a re-implementation of the SAT-based synthesis method published in the
/// paper: "Andreas Morgenstern, Manuel Gesell, Klaus Schneider: Solving Games Using
/// Incremental Induction, IFM 2013, pages 177-191". It is based on the description given in
/// the paper with only a few additional optimizations following the paper
/// "Niklas Een, Alan Mishchenko, Robert K. Brayton: Efficient implementation of property
/// directed reachability, FMCAD 2011, pages 125-134". All these optimizations are clearly
/// marked in the source code. In order to understand the code in this class, you should read
/// the IFM'13 paper first. After that, you will find a one-to-one correspondence between
/// the functions described in the paper and the methods of this class.
///
/// The IFM'13 publication only describes how to determine realizability, but not how extract
/// a winning region from this computation. Our implementation also extracts a winning region
/// (the set R[i] such that R[i] = R[i-1]).
///
/// @note This is not the original implementation of the IFM'13 paper, it is a
///       re-implementation. The re-implementation is based on the paper only. The original
///       implementation may contain more optimizations.
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class IFM13Synth : public BackEnd
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// @param impl_extractor The engine to use for circuit extraction. It will be deleted by
///        this class.
  IFM13Synth(CNFImplExtractor *impl_extractor);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~IFM13Synth();

// -------------------------------------------------------------------------------------------
///
/// @brief Executes the back-end (which is implemented as described in the IFM'13 paper).
///
/// This method is the workhorse of this class.  It implements the SAT-based synthesis method
/// published in the paper: "Andreas Morgenstern, Manuel Gesell, Klaus Schneider: Solving
/// Games Using Incremental Induction, IFM 2013, pages 177-191". It is based on the
/// description given in the paper with only a few additional optimizations following the
/// paper "Niklas Een, Alan Mishchenko, Robert K. Brayton: Efficient implementation of
/// property directed reachability, FMCAD 2011, pages 125-134". All these optimizations are
/// clearly marked in the source code. In order to understand the code in this class, you
/// should read the IFM'13 paper first. After that, you will find a one-to-one correspondence
/// between the functions described in the paper and the methods of this class.
///
/// The IFM'13 publication only describes how to determine realizability, but not how extract
/// a winning region from this computation. Our implementation also extracts a winning region
/// (the set R[i] such that R[i] = R[i-1]).
///
/// @return True if the specification was realizable, false otherwise.
  virtual bool run();

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Computes the set of states from which the specification can be fulfilled.
///
/// @return True if the specification is realizable, false otherwise.
  bool computeWinningRegion();

// -------------------------------------------------------------------------------------------
///
/// @brief Propagates clauses forward and searches for equivalent clause sets.
///
/// Similar to IC3, this method propagates clauses of frames forward. It also checks if two
/// adjacent clauses sets become syntactically equal.
///
/// @param max_level The maximum frame level until which propagation should be performed.
/// @return The number N returned by this method has the following meaning: N=0 means that
///         no equal clause sets have been found. N > 0 means that R[N] = R[N-1].
  size_t propagateBlockedStates(size_t max_level);

// -------------------------------------------------------------------------------------------
///
/// @brief Analyzes the rank of the passed state.
///
/// There are two possible conclusions: (a) the rank of the state is > level, (b) the rank of
/// the state is <= level. In order to analyze a certain state, this method may analyze
/// some of its successor states first (using a queue of proof obligations internally). This
/// method is always called for the initial state only.
///
/// @param state_cube A full cube over the state variables, describing the state that should
///        be analyzed.
/// @param level The level for the analysis (this method checks if the rank of the state is
///        > level or <= level).
/// @return IS_GREATER (an alias for true) if the rank of the state is > level. IS_LOSE (an
///         alias for false) otherwise.
  bool recBlockCube(const vector<int> &state_cube, size_t level);

// -------------------------------------------------------------------------------------------
///
/// @brief Marks a state-input-pair as blocked.
///
/// The meaning is: we want to store the fact that a certain state-input pair is not
/// helpful for the antagonist (the party controlling the inputs i) in trying to move from
/// R[level] to R[level-1]. Hence, we update U[level] := U[level] & !state_in_cube. We also
/// update the U-sets of all smaller levels in the same way for syntactic containment
/// reasons (as described in the paper).
///
/// @param state_in_cube The state-input pair as a cube over present state variables and
///        uncontrollable inputs. The cube is potentially incomplete, i.e., can represent
///        a larger set of state-input pairs.
/// @param level The level in which the transition should be blocked (i.e., the state-input
///        pair should by marked as useless for the antagonist).
  void addBlockedTransition(const vector<int> &state_in_cube, size_t level);

// -------------------------------------------------------------------------------------------
///
/// @brief Removes a state(-cube) from the frame R[level].
///
/// This method updates R[level] := R[level] & !state_cube. state_cube is a potentially
/// incomplete state-cube. We also update the R-sets of all smaller levels in the same way
/// for syntactic containment reasons (as described in the paper).
///
/// @param state_cube The state-cube that should be removed from R[level]. The cube is
///        potentially incomplete, i.e., can represent a larger set of states.
/// @param level The level in which the state should be blocked (i.e., removed form the
///        corresponding frame R[level]).
  void addBlockedState(const vector<int> &state_cube, size_t level);

// -------------------------------------------------------------------------------------------
///
/// @brief Checks if a state is contained in a certain frame.
///
/// The passed state cube is complete, i.e., contains all state variables. Hence, containment
/// can be (and is) checked syntactically. We simply check if state_cube satisfies all clauses
/// of R[level].
///
/// @param state_cube A full cube over the present state variables.
/// @param level The index i of a certain frame R[i].
/// @return False if state_cube satisfies R[level], true otherwise.
  bool isBlocked(const vector<int> &state_cube, size_t level);

// -------------------------------------------------------------------------------------------
///
/// @brief Removes a state(-cube) from the current over-approximation of the winning region.
///
/// This method updates W := W & !state_cube. state_cube is a potentially incomplete
/// state-cube.
///
/// @param state_cube The state-cube that should be removed from the current
///        over-approximation of the winning region. The cube is potentially incomplete, i.e.,
//         can represent a larger set of states.
  void addLose(const vector<int> &state_cube);

// -------------------------------------------------------------------------------------------
///
/// @brief Checks if a state satisfies the current over-approximation of the winning region.
///
/// The passed state cube is complete, i.e., contains all state variables. Hence, containment
/// can be (and is) checked syntactically. We simply check if state_cube satisfies all clauses
/// of W (stored in @link #win_ win_ @endlink).
///
/// @param state_cube A full cube over the present state variables.
/// @return False if state_cube satisfies W, true otherwise.
  bool isLose(const vector<int> &state_cube);

// -------------------------------------------------------------------------------------------
///
/// @brief Generalizes and excludes a blocked transition.
///
/// @param state_in_cube The state-input cube (the transition) that should be generalized and
///        and blocked.
/// @param ctrl_cube The control signals that can be chosen by the protagonist in order to
///        avoid ending up in R[level-1].
/// @param level The frame in which the transition should be blocked.
/// @return False if state_cube satisfies W, true otherwise.
  void genAndBlockTrans(const vector<int> &state_in_cube,
                        const vector<int> &ctrl_cube,
                        size_t level);

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the frame R[index] in CNF.
///
/// If the frame with this index does not yet exists, it is created.
///
/// @param index The index of the requested frame.
/// @return The frame R[index] in CNF.
  CNF& getR(size_t index);

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the blocked transitions U[index] in CNF.
///
/// If the set of blocked transitions with this index does not yet exists, it is created.
/// This function is actually only needed for debugging purposes.
///
/// @param index The index of the requested set of blocked transitions.
/// @return The set U[index] of blocked transitions in CNF.
  CNF& getU(size_t index);

// -------------------------------------------------------------------------------------------
///
/// @brief Returns a solver with CNF: U[index] & T & R[index-1]'.
///
/// This solver is used to search for moves the antagonist can make in order to reach
/// R[index-1]. If the solver for this index does not yet exists, it is created.
///
/// @param index The index of the requested solver.
/// @return A solver with CNF: U[index] & T & R[index-1]'. getGotoNextLowerSolver(0) returns
///         NULL.
  SatSolver* getGotoNextLowerSolver(size_t index);

// -------------------------------------------------------------------------------------------
///
/// @brief Returns a solver with CNF: T & R[index-1]'.
///
/// This solver is used to generalize blocked transitions. If the solver for this index does
/// not yet exists, it is created.
///
/// @param index The index of the requested solver.
/// @return A solver with CNF: T & R[index-1]'. getGenBlockTransSolver(0) returns NULL.
  SatSolver* getGenBlockTransSolver(size_t index);

// -------------------------------------------------------------------------------------------
///
/// @brief Takes a list of proof obligations and removes the element with lowest rank.
///
/// @param queue A list of proof obligations.
/// @return The element that has been removed from the list. It is always the element
///         (among the elements) with minimal rank.
  IFMProofObligation popMin(list<IFMProofObligation> &queue);

// -------------------------------------------------------------------------------------------
///
/// @brief A debugging utility to print all frames.
///
/// Do this for small examples only.
  void debugPrintRs() const;

// -------------------------------------------------------------------------------------------
///
/// @brief A debugging utility check invariants of the data structures.
///
/// The checks are only performed in debug-mode. In release-mode, this function does nothing.
///
/// @param k The maximum level up until which the invariants should be checked.
  void debugCheckInvariants(size_t k);

// -------------------------------------------------------------------------------------------
///
/// @brief The frames R[] of the algorithm. Each frame represents a set of states in CNF.
///
/// You should use @link #getR getR() @endlink to access the frames. The reason is that this
/// method initializes frames if they do not yet exist.
  vector<CNF> r_;

// -------------------------------------------------------------------------------------------
///
/// @brief The blocked transitions U[] of the algorithm.
///
/// These sets are maintained only in debug mode. In release-mode they are only present
/// inside the goto_next_lower_solvers_.
/// You should use @link #getU getU() @endlink to access the frames. The reason is that this
/// method initializes elements if they do not yet exist.
  vector<CNF> u_;

// -------------------------------------------------------------------------------------------
///
/// @brief The current over-approximation of the winning region W (for the protagonist).
  CNF win_;

// -------------------------------------------------------------------------------------------
///
/// @brief The solvers storing the CNF: U[k] & T & R[k-1]'.
///
/// These solvers are used to search for moves the antagonist can make in order to reach
/// R[k-1].
/// You should use @link #getGotoNextLowerSolver getGotoNextLowerSolver() @endlink to access
/// these solvers. The reason is that this method initializes the solvers if they do not yet
/// exist. goto_next_lower_solvers_[0] is always NULL.
  vector<SatSolver*> goto_next_lower_solvers_;

// -------------------------------------------------------------------------------------------
///
/// @brief The solvers storing the CNF: T & R[k-1]'.
///
/// You should use @link #getGenBlockTransSolver getGenBlockTransSolver() @endlink to access
/// these solvers. The reason is that this method initializes the solvers if they do not yet
/// exist. gen_block_trans_solvers_[0] is always NULL.
  vector<SatSolver*> gen_block_trans_solvers_;

// -------------------------------------------------------------------------------------------
///
/// @brief A solver that stores the CNF: T & W'.
  SatSolver *goto_win_solver_; // contains T & W'

// -------------------------------------------------------------------------------------------
///
/// @brief The final winning region as computed by @link #computeWinningRegion @endlink.
  CNF winning_region_;

// -------------------------------------------------------------------------------------------
///
/// @brief The negated winning region as computed by @link #computeWinningRegion @endlink.
  CNF neg_winning_region_;

// -------------------------------------------------------------------------------------------
///
/// @brief The vector of present- and next-state variables, and uncontrollable inputs.
  vector<int> sin_;

// -------------------------------------------------------------------------------------------
///
/// @brief The vector of present- and next-state variables, and all inputs.
  vector<int> sicn_;

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
  IFM13Synth(const IFM13Synth &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  IFM13Synth& operator=(const IFM13Synth &other);

};

#endif // IFM13Synth_H__
