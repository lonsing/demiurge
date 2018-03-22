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
/// @file UnivExpander.h
/// @brief Contains the declaration of the class UnivExpander.
// -------------------------------------------------------------------------------------------

#ifndef UnivExpander_H__
#define UnivExpander_H__

#include "defines.h"
#include "CNF.h"

class SatSolver;

// -------------------------------------------------------------------------------------------
///
/// @struct CNFAnd
/// @brief A compact representation of an AND gate over CNF variables.
struct CNFAnd
{
  /// @brief The first input of the AND gate.
  int r0;
  /// @brief The second input of the AND gate.
  int r1;
  /// @brief The output of the AND gate.
  int l;
};

// -------------------------------------------------------------------------------------------
///
/// @typedef struct CNFAnd CNFAnd
/// @brief An abbreviation for the CNFAnd struct.
typedef struct CNFAnd CNFAnd;

// -------------------------------------------------------------------------------------------
///
/// @class UnivExpander
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
class UnivExpander
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
  UnivExpander();

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~UnivExpander();

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
/// @param win_reg The winning region with which the solver should be initialized. This
///        winning region is compressed (redundant clauses are removed) inside this function
///        and, hence, modified.
/// @param solver_i The solver into which the expanded CNF should be stored.
/// @param limit_size If set to true, then this method aborts if the CNF size limit defined
///        by Options::getSizeLimitForExpansion() is exceeded. This is supposed to prevent
///        running out of memory during the expansion. If this flag is set to false, or
///        omitted, then the size limit does not apply.
/// @return True if the size limit for the expansion has been exceeded. False if everything
///         went OK (or nothing was checked because limit_size was set to false).
  bool resetSolverIExp(CNF &win_reg, SatSolver *solver_i, bool limit_size = false);

// -------------------------------------------------------------------------------------------
///
/// @brief Same as the previous function but resets multiple solvers simultaneously.
///
/// @param win_reg The winning region with which the solver should be initialized. This
///        winning region is compressed (redundant clauses are removed) inside this function
///        and, hence, modified.
/// @param solvers The list of solvers into which the expanded CNF should be stored.
/// @param limit_size If set to true, then this method aborts if the CNF size limit defined
///        by Options::getSizeLimitForExpansion() is exceeded. This is supposed to prevent
///        running out of memory during the expansion. If this flag is set to false, or
///        omitted, then the size limit does not apply.
/// @return True if the size limit for the expansion has been exceeded. False if everything
///         went OK (or nothing was checked because limit_size was set to false).
  bool resetSolverIExp(CNF &win_reg, vector<SatSolver*> solvers, bool limit_size = false);

// -------------------------------------------------------------------------------------------
///
/// @brief Initializes solver_c in LearnSynthSAT::computeWinningRegionPlainExp().
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
  void initSolverCExp(SatSolver *solver_c,
                      const vector<int> &keep,
                      size_t max_nr_of_signals_to_expand = 1);

// -------------------------------------------------------------------------------------------
///
/// @brief Adds next-state clauses to solver_c after initialization with #initSolverCExp().
///
/// After #initSolverCExp() has been called to initialize solver_c, this method can be used
/// to add next-state clauses to all expansions of the next-state variables.
///
/// @param clause A clause over the current-state variables. It is added as clause over
///        (all expansions of) the next state variables in solver_c.
/// @param solver_c The solver to which the next-state copies of this clause should be added.
  void addExpNxtClauseToC(const vector<int> &clause, SatSolver *solver_c);

// -------------------------------------------------------------------------------------------
///
/// @brief Resets solver_c in LearnSynthSAT::computeWinningRegionPlainExp().
///
/// After many many iterations of LearnSynthSAT::computeWinningRegionPlainExp(), solver_c
/// may contain many redundant clauses, which only slow down the solving process. So, from
/// time to time, we may just reset the solver again (i.e., set it into the state after
/// calling #initSolverCExp) to get rid of all these redundant clauses.
///
/// @param solver_c The solver to reset. After calling this method, solver_c will be in the
///        same state as if #initSolverCExp() had just been called.
  void resetSolverCExp(SatSolver *solver_c);

// -------------------------------------------------------------------------------------------
///
/// @brief Initializes the solver for LearningImplExtractor::runLearningExp().
///
/// This method initializes the SAT solver for circuit extraction using universal expansion
/// in order to use a SAT solver instead of a QBF solver. However, this approach appears to
/// be very slow. The solver is filled with the CNF for T(x,i,c,x') & !W(x'), expanded over
/// some but not all control signals.
///
/// @param nxt_win_reg The next-state copy of the winning region, i.e., W(x').
/// @param solver The solver in which the expanded CNF should be put.
/// @param to_exp The set of control signals to expand.
  void extrExp(const CNF &nxt_win_reg,
               SatSolver *solver,
               const vector<int> to_exp) const;

// -------------------------------------------------------------------------------------------
///
/// @brief If the integer pointed to becomes != 0, the expansion aborts.
///
/// This is useful in the parallelization: if one thread already found a solution, it makes
/// sense to abort the expansion (because the expansion can take quite long).
///
/// @param abort_if A pointer to an integer. If this integer ever becomes != 0, the expansion
///        aborts. If this pointer is NULL, we never abort.
  void setAbortCondition(volatile int *abort_if);

// -------------------------------------------------------------------------------------------
///
/// @brief Destroys the stored data structures to save space.
  void cleanup();

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
/// @brief Simplifies the fully expanded AIGER graph using ABC.
///
/// This is a helper for #initSolverIData(). If the transition relation is already fully
/// expanded, then this method can be used to rewrite this expansion further to shring the
/// formula.
/// This method modifies i_trans_ands_, i_max_trans_var_,  and i_rename_maps_.
  void simplifyTransAndsWithAbc();

// -------------------------------------------------------------------------------------------
///
/// @brief Returns true of we call abort, and false otherwise.
///
/// This is useful in the parallelization: if one thread already found a solution, it makes
/// sense to abort the expansion (because the expansion can take quite long).
///
/// @see #setAbortCondition
/// @return true of we call abort, and false otherwise.
  bool shallAbort() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Deletes all data structures that are needed for expansion in solver_i.
///
/// These data structures can consume quite some memory, so we do not want to have them
/// lying around uselessly.
  void cleanupIData();

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
/// We store the expansion of the transition relation here as a list of AND gates. Storing
/// it as CNF would waste memory (this expansion can be very big).
  vector<CNFAnd> i_trans_ands_;

// -------------------------------------------------------------------------------------------
///
/// @brief The maximum variable index occurring in i_trans_ands_.
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
/// i_rename_maps_[cnt] defines how to rename state variables to the obtain the
/// renamed version of the next-state variables for expansion number cnt.
/// i_rename_maps_[c1][c2] gives the c1'th renaming of the state variable with
/// index (not value!) c2. We use the index to save memory. This rename map
/// does not only do renaming, but also constant propagation.
/// If a certain variable is renamed to 1, then it is set to TRUE. If it is renamed to -1,
/// then it is set to FALSE.
/// If there are n control signals, then this vector could contain 2^n rename maps, one for
/// each expansion, in the worst case. For certain benchmarks, this rename map alone can
/// consume all our memory (while i_trans_ands_ still stays small). Hence, if this
/// i_rename_maps_ grows too large, we do not build it fully. Instead, we store further
/// renamings (to be applied to i_rename_maps_) in i_copy_maps_ and i_orig_prop_maps_ and
/// apply them only when needed.
  vector<vector<int> > i_rename_maps_;

// -------------------------------------------------------------------------------------------
///
/// @brief Additional rename maps to be applied to i_rename_maps_.
///
/// For small benchmarks, this field is not used. For small benchmarks, all the renaming is
/// stored in i_rename_maps_. However, for large benchmarks with many control signals,
/// i_rename_maps_ can grow very large and eat up all our memory.  For such cases, we do not
/// build up i_rename_maps_ completely, but store the remaining renamings (to be applied to
/// i_rename_maps_) in i_copy_maps_ and i_orig_prop_maps_.
  vector<vector<int> > i_copy_maps_;

// -------------------------------------------------------------------------------------------
///
/// @brief Additional rename maps to be applied to i_rename_maps_.
///
/// For small benchmarks, this field is not used. For small benchmarks, all the renaming is
/// stored in i_rename_maps_. However, for large benchmarks with many control signals,
/// i_rename_maps_ can grow very large and eat up all our memory.  For such cases, we do not
/// build up i_rename_maps_ completely, but store the remaining renamings (to be applied to
/// i_rename_maps_) in i_copy_maps_ and i_orig_prop_maps_.
  vector<vector<int> > i_orig_prop_maps_;


// -------------------------------------------------------------------------------------------
///
/// @brief All the final copies of the next-state variables.
  set<int> i_occ_;

// -------------------------------------------------------------------------------------------
///
/// @brief The almost-final copies of the next-state variables.
///
/// This set is only used to overwrite renamings more efficiently. When we overwrite the
/// renamings, then we need to update the last renaming map. The almost-final copies of the
/// next-state variables are exactly the variables that are used as indices in the final
/// rename map.
  set<int> i_prev_occ_;

// -------------------------------------------------------------------------------------------
///
/// @brief All variables that occur (as values) in i_rename_maps_.
  set<int> i_occ_in_i_rename_maps_;

// -------------------------------------------------------------------------------------------
///
/// @brief The rename maps for the next-state variables for adding clauses to solver_c.
///
/// c_rename_maps_[i] is the rename map for present-state variables to the i-th copy of the
/// next state variables.
  vector<vector<int> > c_rename_maps_;

// -------------------------------------------------------------------------------------------
///
/// @brief The CNF to put into the solver when #resetSolverCExp() is called.
///
/// This CNF is computed by #initSolverCExp() and contains the expanded transition relation
/// for solver_c.
  CNF c_cnf_;

// -------------------------------------------------------------------------------------------
///
/// @brief The CNF to put into the solver when #resetSolverCExp() is called.
///
/// This CNF is computed by #initSolverCExp() and contains the variables that should not
/// be optimized away by the solver (because they will be needed in the incremental use of
/// the solver).
  vector<int> c_keep_;

// -------------------------------------------------------------------------------------------
///
/// @brief If this pointer points to some integer that is not 0, expansion aborts.
  volatile int* abort_if_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  UnivExpander(const UnivExpander &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  UnivExpander& operator=(const UnivExpander &other);

};

#endif // UnivExpander_H__
