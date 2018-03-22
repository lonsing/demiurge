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
/// @file main.cpp
/// @brief Contains the entry point to this application.
// -------------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------
/// @mainpage Overview
///
/// @section intro_sec Introduction
///
/// Demiurge is a tool to synthesize a correct-by-construction implementation of a reactive
/// system from a declarative safety specification fully automatically. The specification
/// that serves as input can only contain properties expressing that 'something bad never
/// happens'. However, many other specification formats can be reduced to pure safety
/// specifications (e.g., by setting a bound on the reaction time as explained in the paper
/// 'Ruediger Ehlers: Symbolic bounded synthesis, Formal Methods in System Design 40(2):
/// 232-262 (2012)').
///
/// Demiurge implements different synthesis methods in different back-ends. Most of
/// these synthesis methods are based on SAT- or QBF-solving. A description of some
/// of the implemented algorithms can be found in the paper: 
///   Roderick Bloem, Robert Koenighofer, Martina Seidl: "SAT-Based Synthesis 
///   Methods for Safety Specs" in VMCAI'14.  
/// Different methods for synthesizing circuits from already computed strategies are
/// discussed in the paper:
///   Roderick Bloem, Uwe Egly, Patrick Klampfl, Robert Koenighofer, Florian 
///   Lonsing: "SAT-based methods for circuit synthesis" in FMCAD'14.
/// Demiurge is not only a tool, but also a framework for implementing new synthesis
/// algorithms. A lot of infrastructure can be re-used. This includes not only the
/// parsing of input files but also classes to conveniently manipulate Boolean
/// formulas in conjunctive normal form, interfaces to SAT- and QBF solvers
/// (different solvers can be used via a unified interface), and so on. Most of the
/// infrastructure currently available targets SAT- and QBF-based synthesis methods.
/// But there is no reason not include BDD-based methods in new back-ends.
///
/// Demiurge is not only a tool, but also a framework for implementing new synthesis
/// algorithms. A lot of infrastructure can be re-used. This includes not only the parsing
/// of input files but also classes to conveniently manipulate Boolean formulas in
/// conjunctive normal form, interfaces to SAT- and QBF solvers (different solvers can be used
/// via a unified interface), and so on. Most of the infrastructure currently available
/// targets SAT- and QBF-based synthesis methods. But there is no reason not include
/// BDD-based methods in new back-ends.
///
/// @subsection input_sec Input of the Tool
/// The input to this tool is a safety specification in
/// <a href="http://fmv.jku.at/aiger/">AIGER</a> format. It is defined in the same way as for
/// the safety track of the <a href="http://fmv.jku.at/hwmcc13/">hardware model checking
/// competition</a>: it has a certain number of state bits that are updated depending on the
/// inputs of the system. There is exactly one output bit which signals an error. This output
/// can depend on the current state and input. The tools of the hardware model checking
/// competition answer the question: 'can this output ever become true?'. Our synthesis tool
/// distinguishes two kinds of inputs: uncontrollable inputs and control signals. They are
/// distinguished by their name. Control signals have a name that starts with 'controllable_'.
/// The question answered by demiurge is: 'can the control signals be implemented in such a
/// way that the error output never becomes true?' If yes, then we compute such an
/// implementation. This is compatible with the input format for the
/// <a href="http://www.syntcomp.org/">synthesis competition</a>.
///
/// @subsection output_sec Output of the Tool
///
/// The output of the synthesis tool is a circuit in AIGER format as well. It is a copy of the
/// the input file with one important change: the control signals are no inputs to the circuit
/// any more. Instead, they are computed inside the circuit based on the current state and the
/// uncontrollable inputs. Hence, for a sanity check, the output of our tool can be passed on
/// to a verification tool compatible with the requirements of the hardware model checking
/// competition.
///
/// \section install_sec Installation and Usage
///
/// At the moment, we support Unix-based operating systems only. In order to compile the
/// tool, you need to:
/// <ol>
///  <li> Make sure that GNU make, the GNU C compiler gcc, and the GNU C++ compiler
///       g++ is installed on your system. On Debian-based Linux systems (such as
///       Ubuntu), you can do this by typing
///         @verbatim shell> sudo apt-get install build-essential @endverbatim
///       in a shell.
///  <li> Make sure that 'cmake' is installed on your system. On Debian-based Linux
///       systems, you can do this by typing
///         @verbatim shell> sudo apt-get install cmake @endverbatim
///  <li> Create a new directory in which all third-party software should be installed.
///       We will call this directory @c \<third_party_install\> in
///       the following. It should be different to tool/ext_tools/.
///  <li> Set the environment variable DEMIURGETP to point to this directory. In the
///       'bash' shell, this can be done with the command:
///         @verbatim bash> export DEMIURGETP=<third_party_install>  @endverbatim
///       In order to avoid setting this environment variable every time, you can also
///       add the above line to the file @c ~/.bashrc (when using the bash shell, for other
///       shells, this works similar).
///  <li> Open a bash in the directory @c tool/ext_tools and execute the script
///       @c install_all.sh. This script calls all other scripts in this directory. The
///       scripts download and compile third-party software like SAT- and QBF-solvers.
///       Make sure that you are connected to the Internet while executing this script.
///  <li> Open a shell in the directory @c tool/ and type 'make'. This will compile our
///       source code and link it against the third-party libraries.
///  <li> The executables @c tool/build/src/demiurge-bin and
///       @c tool/build/src/demiurge-debug should have been created and you are done.
///       @c demiurge-debug is like @c demiurge-bin but performs many debug-checks during
///       operation.
///  <li> Execute the program (@c tool/build/src/demiurge-bin) with the option '--help' to
///       get a list of command-line options and their meaning.
/// </ol>
///
/// \section archit_sec Architecture
///
/// You can easily extend demiurge with new synthesis methods. The architecture of the tool
/// is simple: Different synthesis methods are implemented in different @link BackEnd
/// BackEnds @endlink. Simply create a new back-end and implement its BackEnd::run() method.
/// The class Options is a singleton that gives you access to the command-line parameters.
/// The class AIG2CNF provides you with CNF representations of the different parts of the
/// specification (the transition relation, initial state, safe states). The VarManager
/// contains all variables of the specification and allows you to create new (unique)
/// variables. Other utility classes and methods that could be useful for implementing your
/// new back-end are:
/// <ul>
///  <li> The Logger allows you to print messages (which can be enabled and disabled with
///       command-line options).
///  <li> The Options::getSATSolver() gives you an instance of the SAT-solver selected by the
///       user.
///  <li> The Options::getQBFSolver() gives you an instance of the QBF-solver selected by the
///       user.
///  <li> The Stopwatch allows you to measure execution times of different parts of your
///       algorithm.
///  <li> The FileUtils, StringUtils, and Utils provide other small utility functions that
///       are potentially useful for all back-ends.
/// </ul>
// -------------------------------------------------------------------------------------------

#include "defines.h"
#include "Options.h"
#include "AIG2CNF.h"
#include "Logger.h"
#include "Stopwatch.h"
#include "VarManager.h"
#include "BackEnd.h"

extern "C" {
 #include "aiger.h"
}

#include <stdio.h>
#include <sys/resource.h>


// -------------------------------------------------------------------------------------------
///
/// @brief The entry point of the program.
///
/// @param argc The number of command line arguments.
/// @param argv The command line arguments. argv_[0] is the name of the process, so the
///        real arguments start with argv_[1].
/// @return 10 in case of realizability, 20 in case of unrealizability, and other values in
///         case of an error.
int main (int argc, char **argv)
{
  PointInTime start_time = Stopwatch::start();
  bool quit = Options::instance().parse(argc, argv);
  if(quit)
    return 0;

  L_INF("Parsing the input file ...");
  const char *error = NULL;
  aiger *aig = aiger_init();
  const string &file = Options::instance().getAigInFileName();
  error = aiger_open_and_read_from_file (aig, file.c_str());
  MASSERT(error == NULL, "Could not open AIGER file " << file << " (" << error << ").");
  MASSERT(aig->num_outputs == 1, "Strange number of outputs in AIGER file.");
  AIG2CNF::instance().initFromAig(aig);
  aiger_reset(aig);
  if(VarManager::instance().getVarsOfType(VarInfo::CTRL).size() == 0)
  {
    L_ERR("No controllable variables found.");
    return -1;
  }

  L_INF("Starting the synthesizer ...");
  BackEnd *back_end = Options::instance().getBackEnd();
  bool realizable = back_end->run();
  delete back_end;

  double cpu_time = Stopwatch::getCPUTimeSec(start_time);
  size_t real_time = Stopwatch::getRealTimeSec(start_time);
  L_LOG("Overall execution time: " << cpu_time << " sec CPU time.");
  L_LOG("Overall execution time: " << real_time << " sec real time.");

  // The synthesis competition requires the following output:
  if(Options::instance().doRealizabilityOnly())
  {
    if(realizable)
      cout << "REALIZABLE" << endl;
    else
      cout << "UNREALIZABLE" << endl;
  }
  else if(! realizable) // if realizable, then only the circuit should be printed to stdout
    cout << "UNREALIZABLE" << endl;

  Utils::debugPrintCurrentMemUsage();
  struct rusage rusage;
  getrusage( RUSAGE_SELF, &rusage );
  L_DBG("Max memory usage: "<< rusage.ru_maxrss << " kB.");

  return realizable ? 10 : 20;
}
