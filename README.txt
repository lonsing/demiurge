README for Demiurge Version 1.1.0
=================================

This file contains important information about this distribution of the tool
Demiurge.

License
-------

This is free software; you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later
version.
This software is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
A copy of the GNU Lesser General Public License is contained in the file
LICENSE. See also http://www.gnu.org/licenses/.

What is Demiurge?
-----------------

Demiurge is a tool to synthesize a correct-by-construction implementation of a
reactive system from a declarative safety specification fully automatically. The
specification that serves as input can only contain properties expressing that
'something bad never happens'. However, many other specification formats can be
reduced to pure safety specifications (e.g., by setting a bound on the reaction
time as explained in the paper 'Ruediger Ehlers: Symbolic bounded synthesis,
Formal Methods in System Design 40(2): 232-262 (2012)').

Demiurge implements different synthesis methods in different back-ends. Most of
these synthesis methods are based on SAT- or QBF-solving. A description of some
of the implemented algorithms can be found in the paper: 'Roderick Bloem, Robert
Koenighofer, Martina Seidl: SAT-Based Synthesis Methods for Safety Specs' to
appear in VMCAI'14.

Demiurge is not only a tool, but also a framework for implementing new synthesis
algorithms. A lot of infrastructure can be re-used. This includes not only the
parsing of input files but also classes to conveniently manipulate Boolean
formulas in conjunctive normal form, interfaces to SAT- and QBF-solver
(different solvers can be used via a unified interface), and so on. Most of the
infrastructure currently available targets SAT- and QBF-based synthesis methods.
But there is no reason not include BDD-based methods in new back-ends.

Input format:
The input to this tool is a safety specification in AIGER format (see
http://fmv.jku.at/aiger/). It is defined in the same way as for the safety track
of the hardware model checking competition (see http://fmv.jku.at/hwmcc13/): it
has a certain number of state bits that are updated depending on the inputs of
the system. There is exactly one output bit which signals an error. This output
can depend on the current state and input. The tools of the hardware model
checking competition answer the question: 'can this output ever become true?'.
Our synthesis tool distinguishes two kinds of inputs: uncontrollable inputs and
control signals. They are distinguished by their name. Control signals have a
name that starts with 'controllable_'. The question answered by demiurge is:
'can the control signals be implemented in such a way that the error output
never becomes true?' If yes, then we compute such an implementation. This is
compatible with the input format suggested for the upcoming synthesis
competition (see http://www.syntcomp.org/).

Output format:
The output of the synthesis tool is a circuit in AIGER format as well. It is a
copy of the the input file with one important change: the control signals are no
inputs to the circuit any more. Instead, they are computed inside the circuit
based on the current state and the uncontrollable inputs. Hence, for a sanity
check, the output of our tool can be passed on to a verification tool compatible
with the requirements of the hardware model checking competition.

Structure of this Archive
-------------------------

tool/            Contains the the tool and its source code.
tool/ext_tools/  Contains scripts to download all third-party software that is
                 required to compile and run demiurge (see the installation
                 instructions for details).
tool/src/        Contains the actual source code.
tool/doc/        Contains the source code documentation created by Doxygen.
                 In order to build the Doxygen documentation, execute the
                 command 'make doc' from a shell in tool/. The entry point for
                 the HTML documentation is tool/doc/doxygen.html.
experiments/     contains scripts and benchmarks to run experiments with the
                 tool. The scripts performance_test_XYZ.sh are exactly the
                 scripts that were used to obtain the results for the VMCAI'14
                 paper.
experiments/benchmarks/: contains example input files for the tool. Most of them
                 are parameterized by some means. A rule of thumb is: the higher
                 the number in the benchmark, the more complex the benchmark.
                 Most of the benchmarks have been created from a Verilog file.
                 Have a look at the comment at the end of the AIGER files to see
                 how it was created (Verilog source plus sequence of commands
                 to get the AIGER file). Benchmarks where the name ends with
                 'y' have been optimized with the tool 'abc' (see
                 http://www.eecs.berkeley.edu/~alanmi/abc/). For benchmarks
                 where the name ends with 'n', this optimization step was
                 skipped.
experiments/results/: This is the directory where the performance testing
                 scripts put their results. The subdirectory vmcai_results/
                 contains the output of the scripts when run on a Intel Xeon
                 E5430 CPU with 4 cores running at 2.66 GHz, and a 64 bit
                 Linux. These are exactly the results as published at VMCAI'14.
                 The file experiments/results/vmcai_results/vmcai_results.xls
                 summarizes all results with spreadsheet tables. The scripts
                 XYZ_log_to_table.py are able to transform log-files as produced
                 by the scripts performance_test_XYZ.sh into CSV files (which
                 can be easily imported into spreadsheets).
bdd_tool/        Contains a BDD-based implementation written in Python. It is
                 used as a baseline for comparison in our VMCAI'14 paper. It has
                 been created by students in the course of an internal synthesis
                 competition for a lecture on synthesis.

Installation Instructions
-------------------------
Pre-compiled Linux binaries of the tool can be found in
tool/build/src/demiurge-bin and tool/build/src/demiurge-debug. At the moment, we
support Unix-based operating systems only. In order to compile these binaries
yourself you need to:
 - Make sure that GNU make, the GNU C compiler gcc, and the GNU C++ compiler
   g++ is installed on your system. On Debian-based Linux systems (such as
   Ubuntu), you can do this by typing
     shell> sudo apt-get install build-essential
   in a shell.
 - Make sure that 'cmake' is installed on your system. On Debian-based Linux
   systems, you can do this by typing
     shell> sudo apt-get install cmake
 - Create a new directory in which all third-party software should be installed.
   We will call this directory <third_party_install> in the following. It should
   be different to tool/ext_tools/.
 - Set the environment variable DEMIURGETP to point to this directory. In the
   'bash' shell, this can be done with the command:
     bash> export DEMIURGETP=<third_party_install>
   In order to avoid setting this environment variable every time, you can also
   add the above line to the file ~/.bashrc (when using the bash shell, for
   other shells, this works similar).
 - Open a bash in the directory tool/ext_tools and execute the script
   install_all.sh. This script calls all other scripts in this directory. The
   scripts download and compile third-party software like SAT- and QBF-solvers.
   Make sure that you are connected to the Internet while executing this script.
 - Open a shell in the directory tool/ and type 'make'. This will compile our
   source code and link it against the third-party libraries.
 - The executables tool/build/src/demiurge-bin and tool/build/src/demiurge-debug
   should have been created and you are done. demiurge-debug is like
   demiurge-bin but performs many debug-checks during operation.
 - Execute the program (tool/build/src/demiurge-bin) with the option '--help' to
   get a list of command-line options and their meaning.
 - Execute the command 'make doc' in the directory tool/ to create the doxygen
   documentation of the source code. The entry point for the HTML documentation
   is tool/doc/doxygen.html.
 - If you want to create a new class, type 'make class'. This will create a
   skeleton of a .h and .cpp file and adds it to tool/src/extrasources.make
   (so that the makefile will compile it).

Changes
-------
1.1.0: New methods for computing the winning region using incremental QBF,
       several new methods for extracting circuits from the winning region,
       updated third-party libraries and tools to newer versions.
       Except for cleaning up the code and a small bug-fix in circuit
       extraction method nr 19 (using an optimization to exploit auxiliary
       variables in the transition relation) this is the version that has
       been sent to the SYNTComp 2014 competition
       (see http://www.syntcomp.org/).
1.0.0: Basic version: several methods for computing the winning region, circuit
       extraction can only be done using QBFCert

In case of any questions, do not hesitate to contact the authors (E.g.
robert.koenighofer@iaik.tugraz.at).
 