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
/// @file OrigUnivExpander.h
/// @brief Contains the declaration of the class OrigUnivExpander.
// -------------------------------------------------------------------------------------------

#ifndef OrigUnivExpander_H__
#define OrigUnivExpander_H__

#include "defines.h"
#include "CNF.h"

class SatSolver;

// -------------------------------------------------------------------------------------------
///
/// @class OrigUnivExpander
/// @brief A utility class that can do universal expansion of given CNFs.
///
/// Universal expansion of a CNF with respect to certain variables is an expensive operation,
/// especially if there are a lot of variables to expand. Hence, this operation must be
/// implemented cleverly. For this reason, this class provides several implementations, each
/// one tailored towards a specific application (expanding CTRL-signals in a conjuction of
/// the transition relation with some CNF over the next-state variables, etc.)
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class OrigUnivExpander
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
  OrigUnivExpander();

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~OrigUnivExpander();

// -------------------------------------------------------------------------------------------
///
/// @brief The standard method for universal expansion of certain variables.
///
/// @note The newly created auxiliary variables are not registered in the VarManager for
///       performance reasons (there may be zillions of new variables introduced during the
///       expansion).
///
/// @param orig The original CNF in which we perform universal expansion.
/// @param res An empty CNF. This is where the result will be stored.
/// @param vars_to_exp The list of variables to expand.
/// @param keep The list of variables that are quantified outside of the vars_to_exp and
///        should hence not be renamed during the expansion.
/// @param rate A number between 0 and 1. It defines which fraction of the vars_to_exp should
///        be expanded. Value 0.4 means that 40% of the vars_to_exp should be expanded. Use
///        a value larger than 1 (or omit this parameter) if you want to be sure that every
///        signal is expanded.
/// @param more_deps Additional dependencies that can be exploited during expansion. This map
///        must map user-defined (temporary) variables to a set of other variables that
///        define these variables. Don't include the dependencies of the temporary variables
///        used to encode the transition relation, and don't include the dependencies of the
///        next-state variables because they are automatically read from AIG2CNF. But if you
///        have additional temporary variables, e.g., from Tseitin encoding some formula part
///        then the expansion can benefit from dependency information (it needs to copy less).
///        In any case: more_deps[i] must only contain variables in keep or vars_to_exp.
  static void univExpand(const CNF &orig, CNF &res, const vector<int> &vars_to_exp,
                         const vector<int> &keep, float rate = 1.1,
                         const map<int, set<int> > *more_deps = NULL);

// -------------------------------------------------------------------------------------------
///
/// @brief Performs the reset of solver_i in LearnSynthSAT::computeWinningRegionPlainExp().
///
/// In LearnSynthSAT::computeWinningRegionPlainExp(), solver_i needs to check
///  exists x,i: forall c: exists x',t: W(x) & T(x,i,c,x',t) & !W(x')
/// where W is the winning region (a CNF over the state variables x) and T is the transition
/// relation with t being auxiliary variables.
/// This method expands the control signals c of the above formula and puts the resulting
/// formula into the SAT solver solver_i.
/// This method is highly optimized for this purpose. For instance, the expansion of the
/// transition relation is done only once and reused for different winning regions.
/// #univExpand() could also be used in principle, but this method has more optimizations
/// exploiting the structure of the formula that needs to be expanded.
///
/// @note The newly created auxiliary variables are not registered in the VarManager for
///       performance reasons (there may be zillions of new variables introduced during the
///       expansion).
///
/// @param win_reg The winning region with which the solver should be initialized.
/// @param solver_i The solver into which the expanded CNF should be store.
/// @param limit_size If set to true, then this method aborts if the CNF size limit defined
///        by Options::getSizeLimitForExpansion() is exceeded. This is supposed to prevent
///        running out of memory during the expansion. If this flag is set to false, or
///        omitted, then the size limit does not apply.
/// @return True if the size limit for the expansion has been exceeded. False if everything
///         went OK (or nothing was checked because limit_size was set to false).
  bool resetSolverIExp(const CNF &win_reg, SatSolver *solver_i, bool limit_size = false);

// -------------------------------------------------------------------------------------------
///
/// @brief Performs the reset of solver_c in LearnSynthSAT::computeWinningRegionPlainExp().
///
/// In LearnSynthSAT::computeWinningRegionPlainExp(), solver_c needs to check
///  exists x: forall i: exists x',t: W(x) & T(x,i,c,x',t) & W(x')
/// where W is the winning region (a CNF over the state variables x) and T is the transition
/// relation with t being auxiliary variables.
/// This method expands some of the input signals i in the transition relation T, and puts
/// the resulting formula into solver_c. At the same time, this method also constructs a
/// renaming map so clauses of W(x') can be added later on (using the method
/// #addExpNxtClauseToC()).
/// This method is highly optimized for exactly this purpose. #univExpand() could also be
/// used in principle, but this method has more optimizations exploiting the structure
/// of the transition relation.
///
/// @note The newly created auxiliary variables are not registered in the VarManager for
///       performance reasons (there may be zillions of new variables introduced during the
///       expansion).
///
/// @param solver_c The solver into which the expanded transition relation should be stored.
/// @param keep This method declares the current-state variables, the uncontrollable inputs,
///        and the next-state variables (in their various copys) as reusable. If you
///        also want other variables to be reusable (e.g. previous-state variables),
///        then they can be enumerated in this vector.
/// @param max_nr_of_signals_to_expand For solver_c, expansion is very costly (more than for
///        solver_i). Hence, we usually only expand a few input variables. This parameter
///        defines the maximum number of input signals to expand. Low values are recommended.
  void resetSolverCExp(SatSolver *solver_c,
                       const vector<int> &keep,
                       size_t max_nr_of_signals_to_expand = 1);

// -------------------------------------------------------------------------------------------
///
/// @brief Adds next-state clauses to solver_c after initialization with #resetSolverCExp().
///
/// After #resetSolverCExp() has been called to initialize solver_c, this method can be used
/// to add next-state clauses to all expansions of the next-state variables.
///
/// @param clause A clause over the current-state variables. It is added as clause over
///        (all expansions of) the next state variables in solver_c.
/// @param solver_c The solver to which the next-state copies of this clause should be added.
  void addExpNxtClauseToC(const vector<int> &clause, SatSolver *solver_c);

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief A helper function for #resetSolverIExp().
///
/// Some work in #resetSolverIExp() needs to be done again and again when this method is
/// called repeatedly. For instance, the expansion of the transition relation is always the
/// same. This method performs these repeated tasks only once, and stores the resulting
/// information in fields of this class. This way, they can then be reused by
/// #resetSolverIExp() whenever needed. All fields defined by this method start with 'i_'.
///
/// @param limit_size If set to true, then this method aborts if the CNF size limit defined
///        by Options::getSizeLimitForExpansion() is exceeded. This is supposed to prevent
///        running out of memory during the expansion. If this flag is set to false, or
///        omitted, then the size limit does not apply.
/// @return True if the size limit for the expansion has been exceeded during the
///         initialization already. False if everything went OK.
  bool initSolverIData(bool limit_size = false);

// -------------------------------------------------------------------------------------------
///
/// @brief The list of present-state variables.
///
/// This list is often used, so we keep it as a field to increase readability of the code.
  const vector<int> &s_;

// -------------------------------------------------------------------------------------------
///
/// @brief The list of (uncontrollable) input variables.
///
/// This list is often used, so we keep it as a field to increase readability of the code.
  const vector<int> &i_;

// -------------------------------------------------------------------------------------------
///
/// @brief The list of control signals (controllable input variables).
///
/// This list is often used, so we keep it as a field to increase readability of the code.
  const vector<int> &c_;

// -------------------------------------------------------------------------------------------
///
/// @brief The list of next-state variables.
///
/// This list is often used, so we keep it as a field to increase readability of the code.
  const vector<int> &n_;

// -------------------------------------------------------------------------------------------
///
/// @brief The expansion of the transition relation for #resetSolverIExp().
///
/// We do not expand the transition relation again and again, but we do this only once and
/// reuse it. The only thing we expand freshly each time is the next state copy of the
/// (negation of the) winning region.
/// All fields starting with 'i_' (like this one) are initialized by #initSolverIData() and
/// used by #resetSolverIExp().
  CNF i_exp_trans_;

// -------------------------------------------------------------------------------------------
///
/// @brief The maximum variable index occurring in i_exp_trans_.
///
/// That is, for #resetSolverIExp(), i_max_trans_var_ + 1 is the next fresh CNF variable.
/// All fields starting with 'i_' (like this one) are initialized by #initSolverIData() and
/// used by #resetSolverIExp().
  int i_max_trans_var_;

// -------------------------------------------------------------------------------------------
///
/// @brief A map, renaming present-state vars to a renamed version of the next-state copy.
///
/// #resetSolverIExp() gets the winning region, and needs to expand and negate the next-state
/// copy of the winning region. This map does two renamings in one go: (1) it renames present
/// state variables to their corresponding next-state copy, and (2) it further renames the
/// next-state copy of state variables to the auxiliary variables defining them in the
/// transition relation. Originally, the transition relation CNF has special clauses that do
/// this renaming. We remove these clauses and do the renaming with a map. This is more
/// efficient. Otherwise, the renaming-clauses of the transition relation would get copied
/// many many many times, which increases the size of the expanded CNFs unnecessarily.
/// All fields starting with 'i_' (like this one) are initialized by #initSolverIData() and
/// used by #resetSolverIExp().
  vector<int> i_s_to_ren_n_;

// -------------------------------------------------------------------------------------------
///
/// @brief The rename maps resulting from the expansion of the transition relation.
///
/// i_rename_maps_[c_idx] defines how to rename variables when expanding the control signal
/// c_[c_idx]. This rename map does not only do renaming, but also constant propagation.
/// If a certain variable is renamed to 1, then it is set to TRUE. If it is renamed to -1,
/// then it is set to FALSE.
  vector<vector<int> > i_rename_maps_;

// -------------------------------------------------------------------------------------------
///
/// @brief The rename maps for constant propagation in the original during expansion.
///
/// i_orig_prop_maps_[c_idx] defines how to rename variables in the original when creating
/// the copy due to expansion of the control signal c_[c_idx]. A variable can only be renamed
/// to itself, to 1 (TRUE) and to -1 (FALSE). That is, this is no real renaming but
/// essentially only constant propagation.
  vector<vector<int> > i_orig_prop_maps_;

// -------------------------------------------------------------------------------------------
///
/// @brief The rename maps for the next-state variables for adding clauses to solver_c.
///
/// c_rename_maps_[i] is the rename map for present-state variables to the i-th copy of the
/// next state variables.
  vector<vector<int> > c_rename_maps_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  OrigUnivExpander(const OrigUnivExpander &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  OrigUnivExpander& operator=(const OrigUnivExpander &other);

};

#endif // OrigUnivExpander_H__
