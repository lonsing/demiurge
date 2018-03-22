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
/// @file LearningImplExtractor.h
/// @brief Contains the declaration of the class LearningImplExtractor.
// -------------------------------------------------------------------------------------------

#ifndef LearningImplExtractor_H__
#define LearningImplExtractor_H__

#include "defines.h"
#include "CNFImplExtractor.h"
#include "LearningExtractorStatistics.h"

class QBFSolver;

// -------------------------------------------------------------------------------------------
///
/// @class LearningImplExtractor
/// @brief Implements different learning-based methods for circuit extraction.
///
/// This class implements different learning-based methods for circuit extraction. All
/// methods are based on the CNF learning algorithm from the paper
///   Ruediger Ehlers, Robert Koenighofer, Georg Hofferek: Symbolically synthesizing small
///   circuits. FMCAD 2012: 91.2.0
/// Some methods use QBF-solving, some incremental QBF solving, and some incremental SAT
/// solving with various optimizations. The method that should be used is defined by the
/// command-line option --circuit-mode or -n.
///
/// The general flow is as follows.
/// <ol>
///  <li> We compute a combinational (no latches) stand-alone circuit that computes the
///       control signals based on the uncontrollable inputs and state variables in AIGER
///       format.
///  <li> We optimize this stand-alone circuit using ABC.
///  <li> The optimized circuit is inserted into the original specification as requited by
///       the rules of the SYNTComp competition.
/// </ol>
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class LearningImplExtractor : public CNFImplExtractor
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
  LearningImplExtractor();

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~LearningImplExtractor();

// -------------------------------------------------------------------------------------------
///
/// @brief Runs this circuit extractor.
///
/// Depending on the selected method (using the command --circuit-mode or -n), one of the
/// methods runLearningX is called with appropriate parameters. Finally, ABC is used to
/// optimize the resulting circuit, and the implementation is inserted into the specification
/// (as required by the SYNTComp rules).
///
/// @param winning_region The winning region from which the circuit should be extracted.
/// @param neg_winning_region The negation of the winning region.
  virtual void run(const CNF &winning_region, const CNF &neg_winning_region);

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Implements circuit extraction with non-incremental QBF-based CNF learning.
///
/// This is a more or less straightforward implementation of the CNF learning method from the
/// paper
///   Ruediger Ehlers, Robert Koenighofer, Georg Hofferek: Symbolically synthesizing small
///   circuits. FMCAD 2012: 91.2.0
/// using a QBF solver.
///
/// @param win_region The winning region from which the circuit should be extracted.
/// @param neg_win_region The negation of the winning region.
  void runLearningQBF(const CNF &win_region, const CNF &neg_win_region);

// -------------------------------------------------------------------------------------------
///
/// @brief Implements circuit extraction with incremental QBF-based CNF learning.
///
/// This method is very similar to #runLearningQBF. The difference is that this one uses
/// incremental QBF solving using DepQBF via its API. This method will always use DepQBF,
/// no matter which solver the user selects via command-line arguments.
///
/// @param win_region The winning region from which the circuit should be extracted.
/// @param neg_win_region The negation of the winning region.
  void runLearningQBFInc(const CNF &win_region, const CNF &neg_win_region);

// -------------------------------------------------------------------------------------------
///
/// @brief Same as runLearningQBFInc, but using universal expansion and a SAT solver.
///
/// Although using SAT solvers instead of QBF appears to pay off when computing the winning
/// region, this does not seem to hold true here: this method is relatively slow.
///
/// @param win_region The winning region from which the circuit should be extracted.
/// @param neg_win_region The negation of the winning region.
  void runLearningExp(const CNF &win_region, const CNF &neg_win_region);

// -------------------------------------------------------------------------------------------
///
/// @brief Uses SAT-based CNF learning with Jiang-resubstitution.
///
/// This method also implements the same CNF learning algorithm as #runLearningQBF. The
/// difference is that this method uses a SAT solver instead of a QBF solver. In order to
/// avoid solving a quantifier alternation, the method handles output signals just like
/// inputs (temporarily) as explained in the paper:
///    Jie-Hong Roland Jiang, Hsuan-Po Lin, Wei-Lun Hung: Interpolating functions from
///    large Boolean relations. ICCAD 2009: 779-784.
/// Once a working solution has been computed, this method can also run a second minimization
/// round, where it tries to simplify the solution further. This second optimization round
/// can be enabled or disabled.
///
/// @param win_region The winning region from which the circuit should be extracted.
/// @param neg_win_region The negation of the winning region.
/// @param second_run A flag to control whether a second optimization round should be
///        performed after a working solution is already available. This costs time, but can
///        lead to smaller circuits. If the parameter is omitted, then this step will NOT be
///        performed. Personal feeling: it usually does not pay off.
/// @param min_cores A flag that controls whether the SAT solver should compute minimal
///        unsatisfiable cores, or just regular ones. Especially if the second_run flag is
///        true, it may make sense to set this flag to false to be faster in the first round.
///        However, often having min_cores=false makes things slower (and leads to larger
///        circuits). That's why true is the default value.
  void runLearningJiangSAT(const CNF &win_region,
                           const CNF &neg_win_region,
                           bool second_run = false,
                           bool min_cores = true);

// -------------------------------------------------------------------------------------------
///
/// @brief Like #runLearningJiangSAT but with more aggressive incremental solving.
///
/// The method #runLearningJiangSAT uses incremental solving, but only it has two different
/// solver instances per control signal: one for computing problematic situations, and one
/// for generalizing them using an unsatisfiable core computation. This method uses
/// incremental soling in a more aggressive way. It uses only two solver instances, one
/// for computing problematic situations across all control signals, and one for
/// generalizing them across all iterations.  This is achieved by introducing activation
/// variables that activate or deactivate different parts of the formula, depending on the
/// control signal we are currently synthesizing.
///
/// @param win_region The winning region from which the circuit should be extracted.
/// @param neg_win_region The negation of the winning region.
/// @param second_run A flag to control whether a second optimization round should be
///        performed after a working solution is already available. This costs time, but can
///        lead to smaller circuits. If the parameter is omitted, then this step will NOT be
///        performed. Personal feeling: it usually does not pay off.
/// @param min_cores A flag that controls whether the SAT solver should compute minimal
///        unsatisfiable cores, or just regular ones. Especially if the second_run flag is
///        true, it may make sense to set this flag to false to be faster in the first round.
///        However, often having min_cores=false makes things slower (and leads to larger
///        circuits). That's why true is the default value.
  void runLearningJiangSATInc1(const CNF &win_region,
                               const CNF &neg_win_region,
                               bool second_run = false,
                               bool min_cores = true);

// -------------------------------------------------------------------------------------------
///
/// @brief Like #runLearningJiangSATInc1 but with even more aggressive incremental solving.
///
/// While the method #runLearningJiangSATInc1 uses two solver instances to compute all
/// control signals, this method uses only one instance. This is achieved by introducing even
/// more activation variables.
///
/// @param win_region The winning region from which the circuit should be extracted.
/// @param neg_win_region The negation of the winning region.
/// @param second_run A flag to control whether a second optimization round should be
///        performed after a working solution is already available. This costs time, but can
///        lead to smaller circuits. If the parameter is omitted, then this step will NOT be
///        performed. Personal feeling: it usually does not pay off.
/// @param min_cores A flag that controls whether the SAT solver should compute minimal
///        unsatisfiable cores, or just regular ones. Especially if the second_run flag is
///        true, it may make sense to set this flag to false to be faster in the first round.
///        However, often having min_cores=false makes things slower (and leads to larger
///        circuits). That's why true is the default value.
  void runLearningJiangSATInc2(const CNF &win_region,
                               const CNF &neg_win_region,
                               bool second_run = false,
                               bool min_cores = true);

// -------------------------------------------------------------------------------------------
///
/// @brief Uses SAT-based CNF learning with Jiang-resubstitution and temporary variables.
///
/// This method is very similar to #runLearningJiangSAT. The only difference is that this
/// method also allows control signals to depend on temporary variables from the original
/// input file (the specification), as long as these temporary variables do not depend on
/// the respective control signal. Since the rules for SYNTComp do not allow such dependencies
/// in the final output, they are removed again simply by copying the respective parts of the
/// original input file before producing the output file.
///
/// @param win_region The winning region from which the circuit should be extracted.
/// @param neg_win_region The negation of the winning region.
/// @param second_run A flag to control whether a second optimization round should be
///        performed after a working solution is already available. This costs time, but can
///        lead to smaller circuits. If the parameter is omitted, then this step will NOT be
///        performed. Personal feeling: it usually does not pay off.
/// @param min_cores A flag that controls whether the SAT solver should compute minimal
///        unsatisfiable cores, or just regular ones. Especially if the second_run flag is
///        true, it may make sense to set this flag to false to be faster in the first round.
///        However, often having min_cores=false makes things slower (and leads to larger
///        circuits). That's why true is the default value.
  void runLearningJiangSATTmp(const CNF &win_region,
                              const CNF &neg_win_region,
                              bool second_run = false,
                              bool min_cores = true);

// -------------------------------------------------------------------------------------------
///
/// @brief Like #runLearningJiangSATTmp, but also allows dependencies on control signals.
///
/// This method is very similar to #runLearningJiangSATTmp. The only difference is that this
/// method also even more dependencies between control signals. If a certain already computed
/// control signal A does not depend on a yet to compute control signal B, then B is allowed
/// to depend on A.
///
/// @param win_region The winning region from which the circuit should be extracted.
/// @param neg_win_region The negation of the winning region.
/// @param second_run A flag to control whether a second optimization round should be
///        performed after a working solution is already available. This costs time, but can
///        lead to smaller circuits. If the parameter is omitted, then this step will NOT be
///        performed. Personal feeling: it usually does not pay off.
/// @param min_cores A flag that controls whether the SAT solver should compute minimal
///        unsatisfiable cores, or just regular ones. Especially if the second_run flag is
///        true, it may make sense to set this flag to false to be faster in the first round.
///        However, often having min_cores=false makes things slower (and leads to larger
///        circuits). That's why true is the default value.
  void runLearningJiangSATTmpCtrl(const CNF &win_region,
                                  const CNF &neg_win_region,
                                  bool second_run = false,
                                  bool min_cores = true);

// -------------------------------------------------------------------------------------------
///
/// @brief Combines #runLearningJiangSATTmpCtrl and runLearningJiangSATInc2.
///
/// This method is like #runLearningJiangSATTmpCtrl, but uses incremental solving more
/// aggressively, just like #runLearningJiangSATInc2. This is achieved by introducing
/// acitvation variables that enable or disable different parts of the formula.
///
/// @param win_region The winning region from which the circuit should be extracted.
/// @param neg_win_region The negation of the winning region.
/// @param second_run A flag to control whether a second optimization round should be
///        performed after a working solution is already available. This costs time, but can
///        lead to smaller circuits. If the parameter is omitted, then this step will NOT be
///        performed. Personal feeling: it usually does not pay off.
/// @param min_cores A flag that controls whether the SAT solver should compute minimal
///        unsatisfiable cores, or just regular ones. Especially if the second_run flag is
///        true, it may make sense to set this flag to false to be faster in the first round.
///        However, often having min_cores=false makes things slower (and leads to larger
///        circuits). That's why true is the default value.
  void runLearningJiangSATTmpCtrlInc2(const CNF &win_region,
                                      const CNF &neg_win_region,
                                      bool second_run = false,
                                      bool min_cores = true);

// -------------------------------------------------------------------------------------------
///
/// @brief Creates a CNF that assigns a variable to the result of some other CNF.
///
/// This method creates a CNF that for (var <-> impl), where var is a variable and impl
/// is a CNF. It is used to say that a certain control signal is now defined by a function
/// that has been computed in CNF form.
///
/// @param var The variable to set equal to the result of the CNF impl.
/// @param impl The CNF that should define the value of var.
/// @return A CNF representation of (var <-> impl).
  CNF makeEq(int var, CNF impl);

// -------------------------------------------------------------------------------------------
///
/// @brief An alternative implementation of #makeEq, which seems to be slower.
///
/// @param var The variable to set equal to the result of the CNF impl.
/// @param impl The CNF that should define the value of var.
/// @return A CNF representation of (var <-> impl).
  CNF makeEq2(int var, CNF impl);

// -------------------------------------------------------------------------------------------
///
/// @brief Adds the solution for one output function to the AIGER circuit we have so far.
///
/// The AIGER circuit we have so far is built up in #standalone_circuit_. This method adds
/// an output function (in CNF) for one control signal.
///
/// @param ctrl_var The control signal for which we would like to add an output function.
/// @param solution The output function (in CNF) defining the control signal.
  void addToStandAloneAiger(int ctrl_var, const CNF &solution);


// -------------------------------------------------------------------------------------------
///
/// @brief Transforms a CNF literal into an AIGER literal as used in #standalone_circuit_.
///
/// @param cnf_lit The literal as it occurs in CNF formulas.
/// @return The corresponding AIGER index of the literal as it is used in the
///         #standalone_circuit_.
  int cnfToAig(int cnf_lit);

// -------------------------------------------------------------------------------------------
///
/// @brief Creates the AND between two AIGER literals and returns the resulting literal.
///
/// This method extends the #standalone_circuit_ with logic to compute the AND between two
/// AIGER literals. The literal that represents the result of the AND is returned.
///
/// @param in1 The first operand of the AND: an AIGER literal from #standalone_circuit_.
/// @param in2 The second operand of the AND: an AIGER literal from #standalone_circuit_.
/// @return The literal that represents the result of the AND.
  int makeAnd(int in1, int in2);

// -------------------------------------------------------------------------------------------
///
/// @brief Creates the OR between two AIGER literals and returns the resulting literal.
///
/// This method extends the #standalone_circuit_ with logic to compute the OR between two
/// AIGER literals. The literal that represents the result of the OR is returned.
///
/// @param in1 The first operand of the OR: an AIGER literal from #standalone_circuit_.
/// @param in2 The second operand of the OR: an AIGER literal from #standalone_circuit_.
/// @return The literal that represents the result of the OR.
  int makeOr(int in1, int in2);

// -------------------------------------------------------------------------------------------
///
/// @brief Copies missing AND gates from the spec.
///
/// This method is only used for an optimization in the methods #runLearningJiangSATTmp,
/// #runLearningJiangSATTmpCtrl, and #runLearningJiangSATTmpCtrlInc2. These methods allow
/// solutions to depend on temporary variables that appear in the original specification.
/// The rules for SYNTComp do not allow to refer to such temporary variables from the
/// specification in the final solution. Hence, this method simply copies the AND gates from
/// the specification into the #standalone_circuit_.
  void insertMissingAndFromTrans();

// -------------------------------------------------------------------------------------------
///
/// @brief Logs the detailed statistics collected in the LearningExtractorStatistics.
  void logDetailedStatistics();

// -------------------------------------------------------------------------------------------
///
/// @brief The QBF solver to use (as selected by the user with command-line arguments).
  QBFSolver *qbf_solver_;

// -------------------------------------------------------------------------------------------
///
/// @brief The next unused AIGER literal in #standalone_circuit_.
  int next_free_aig_lit_;

// -------------------------------------------------------------------------------------------
///
/// @brief The synthesis result as stand-alone circuit.
///
/// This AIGER circuit is combinatorial, i.e., has no latches. The inputs of the circuit are:
/// the uncontrollable inputs from the spec (in the order in which they appear in the spec)
/// followed by the state-signals (the output of the latches in the spec, in the order in
/// which they appear in the spec). The outputs of the circuit are the controllable inputs
/// in the order in which they are defined in the spec.
///
/// This circuit will be built up by one of the methods runLearningX. Then, it will be
/// optimized using ABC. Finally, it will be inserted into the original specification, as
/// required by the rules for SYNTComp.
  aiger *standalone_circuit_;

// -------------------------------------------------------------------------------------------
///
/// @brief A map from variables as they occur in CNFs to the corresponding AIGER literals.
///
/// This vector maps variables as they occur in CNFs to the corresponding AIGER literals as
/// they occur in the #standalone_circuit_. Use #cnfToAig to transform literals.
///
/// @note The AIGER literal numbering scheme in the #standalone_circuit_ is different from the
///       numbering scheme in the original specification. The reason is that we want to avoid
///       big 'holes' of unused AIGER literals right from the beginning.
  vector<int> cnf_var_to_standalone_aig_var_;

// -------------------------------------------------------------------------------------------
///
/// @brief The list of uncontrollable input variables followed by the present state variables.
///
/// This vector contains the CNF version of the uncontrollable input variables followed by
/// the present state variables. This vector of variables is used often, and thus stored here
/// as a field.
  vector<int> ip_;

// -------------------------------------------------------------------------------------------
///
/// @brief The list of uncontrollable inputs, state variables, and control signals.
///
/// This vector contains the CNF version of the uncontrollable input variables followed by
/// the present state variables, followed by the control signals. This vector of variables is
/// used often, and thus stored here as a field.
  vector<int> ipc_;

// -------------------------------------------------------------------------------------------
///
/// @brief A helper for collecting performance data and statistics.
  LearningExtractorStatistics statistics;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  LearningImplExtractor(const LearningImplExtractor &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  LearningImplExtractor& operator=(const LearningImplExtractor &other);

};

#endif // LearningImplExtractor_H__
