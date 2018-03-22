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
/// @file Options.cpp
/// @brief Contains the definition of the class Options.
// -------------------------------------------------------------------------------------------

#include "Options.h"
#include "Logger.h"
#include "DepQBFExt.h"
#include "DepQBFApi.h"
#include "QuBEExt.h"
#include "LingelingApi.h"
#include "MiniSatApi.h"
#include "PicoSatApi.h"
#include "StringUtils.h"
#include "EPRSynthesizer.h"
#include "IFM13Synth.h"
#include "TemplateSynth.h"
#include "LearnSynthQBF.h"
#include "LearnSynthQBFInd.h"
#include "LearnSynthQBFInc.h"
#include "LearnSynthSAT.h"
#include "ParallelLearner.h"
#include "LoadSynth.h"
#include "QBFCertImplExtractor.h"
#include "LearningImplExtractor.h"
#include "InterpolImplExtractor.h"
#include "StoreImplExtractor.h"

#include <sys/stat.h>
#include <unistd.h>

// -------------------------------------------------------------------------------------------
const string Options::VERSION = string("1.1.0");

const string Options::TP_VAR = string("DEMIURGETP");

// -------------------------------------------------------------------------------------------
Options *Options::instance_ = NULL;

// -------------------------------------------------------------------------------------------
Options &Options::instance()
{
  if(instance_ == NULL)
    instance_ = new Options;
  MASSERT(instance_ != NULL, "Could not create Options instance.");
  return *instance_;
}

// -------------------------------------------------------------------------------------------
bool Options::parse(int argc, char **argv)
{
  for(int arg_count = 1; arg_count < argc; ++arg_count)
  {
    string arg(argv[arg_count]);
    if(arg == "--help" || arg == "-h")
    {
      printHelp();
      return true;
    }
    else if(arg == "--version" || arg == "-v")
    {
      cout << "demiurge version " << Options::VERSION << endl;
      return true;
    }
    else if(arg == "--real_only" || arg == "-r")
    {
      real_only_ = true;
    }
    else if(arg.find("--in=") == 0)
    {
      aig_in_file_name_ = arg.substr(5, string::npos);
    }
    else if(arg == "-i")
    {
      ++arg_count;
      if(arg_count >= argc)
      {
        cerr << "Option -i must be followed by a filename." << endl;
        return true;
      }
      aig_in_file_name_ = string(argv[arg_count]);
    }
    else if(arg.find("--out=") == 0)
    {
      aig_out_file_name_ = arg.substr(6, string::npos);
    }
    else if(arg == "-o")
    {
      ++arg_count;
      if(arg_count >= argc)
      {
        cerr << "Option -o must be followed by a filename." << endl;
        return true;
      }
      aig_out_file_name_ = string(argv[arg_count]);
    }
    else if(arg.find("--print=") == 0)
    {
      print_string_ = arg.substr(8, string::npos);
    }
    else if(arg == "-p")
    {
      ++arg_count;
      if(arg_count >= argc)
      {
        cerr << "Option -p must be followed by a string indicating what to print." << endl;
        return true;
      }
      print_string_ = string(argv[arg_count]);
    }
    else if(arg.find("--tmp=") == 0)
    {
      tmp_dir_ = arg.substr(6, string::npos);
    }
    else if(arg == "-t")
    {
      ++arg_count;
      if(arg_count >= argc)
      {
        cerr << "Option -t must be followed by a directory name." << endl;
        return true;
      }
      tmp_dir_ = string(argv[arg_count]);
    }
    else if(arg.find("--backend=") == 0)
    {
      back_end_ = arg.substr(10, string::npos);
      StringUtils::toLowerCaseIn(back_end_);
    }
    else if(arg == "-b")
    {
      ++arg_count;
      if(arg_count >= argc)
      {
        cerr << "Option -b must be followed by a back-end name." << endl;
        return true;
      }
      back_end_ = string(argv[arg_count]);
      StringUtils::toLowerCaseIn(back_end_);
    }
    else if(arg.find("--circuit=") == 0)
    {
      circuit_extraction_name_ = arg.substr(10, string::npos);
      StringUtils::toLowerCaseIn(circuit_extraction_name_);
    }
    else if(arg == "-c")
    {
      ++arg_count;
      if(arg_count >= argc)
      {
        cerr << "Option -c must be followed by a circuit extraction method name." << endl;
        return true;
      }
      circuit_extraction_name_ = string(argv[arg_count]);
      StringUtils::toLowerCaseIn(circuit_extraction_name_);
    }
    else if(arg.find("--mode=") == 0)
    {
      istringstream iss(arg.substr(7, string::npos));
      iss >> mode_;
    }
    else if(arg == "-m")
    {
      ++arg_count;
      if(arg_count >= argc)
      {
        cerr << "Option -m must be followed by an integer number." << endl;
        return true;
      }
      istringstream iss(argv[arg_count]);
      iss >> mode_;
    }
    else if(arg.find("--circuit_mode=") == 0)
    {
      istringstream iss(arg.substr(15, string::npos));
      iss >> circuit_extraction_mode_;
    }
    else if(arg == "-n")
    {
      ++arg_count;
      if(arg_count >= argc)
      {
        cerr << "Option -n must be followed by an integer number." << endl;
        return true;
      }
      istringstream iss(argv[arg_count]);
      iss >> circuit_extraction_mode_;
    }
    else if(arg.find("--qbf_sv=") == 0)
    {
      qbf_solver_ = arg.substr(9, string::npos);
      StringUtils::toLowerCaseIn(qbf_solver_);
      if(qbf_solver_ != "depqbf_ext" &&
         qbf_solver_ != "depqbf_api" &&
         qbf_solver_ != "blo_dep_api" &&
         qbf_solver_ != "qube_ext")
      {
        cerr << "Unknown QBF solver '" << qbf_solver_ <<"'." << endl;
        return true;
      }
    }
    else if(arg == "-q")
    {
      ++arg_count;
      if(arg_count >= argc)
      {
        cerr << "Option -q must be followed by a QBF solver name." << endl;
        return true;
      }
      qbf_solver_ = string(argv[arg_count]);
      StringUtils::toLowerCaseIn(qbf_solver_);
      if(qbf_solver_ != "depqbf_ext" &&
         qbf_solver_ != "depqbf_api" &&
         qbf_solver_ != "blo_dep_api" &&
         qbf_solver_ != "qube_ext")
      {
        cerr << "Unknown QBF solver '" << qbf_solver_ <<"'." << endl;
        return true;
      }
    }
    else if(arg.find("--sat_sv=") == 0)
    {
      sat_solver_ = arg.substr(9, string::npos);
      StringUtils::toLowerCaseIn(sat_solver_);
      if(sat_solver_ != "lin_api" &&
         sat_solver_ != "min_api" &&
         sat_solver_ != "pic_api")
      {
        cerr << "Unknown SAT solver '" << sat_solver_ <<"'." << endl;
        return true;
      }
    }
    else if(arg == "-s")
    {
      ++arg_count;
      if(arg_count >= argc)
      {
        cerr << "Option -s must be followed by a SAT solver name." << endl;
        return true;
      }
      sat_solver_ = string(argv[arg_count]);
      StringUtils::toLowerCaseIn(sat_solver_);
      if(sat_solver_ != "lin_api" &&
         sat_solver_ != "min_api" &&
         sat_solver_ != "pic_api")
      {
        cerr << "Unknown SAT solver '" << sat_solver_ <<"'." << endl;
        return true;
      }
    }
    else if(arg.find("--extr_sat_sv=") == 0)
    {
      circuit_sat_solver_ = arg.substr(14, string::npos);
      StringUtils::toLowerCaseIn(circuit_sat_solver_);
      if(circuit_sat_solver_ != "lin_api" &&
         circuit_sat_solver_ != "min_api" &&
         circuit_sat_solver_ != "pic_api")
      {
        cerr << "Unknown SAT solver '" << circuit_sat_solver_ <<"'." << endl;
        return true;
      }
    }
    else if(arg == "-e")
    {
      ++arg_count;
      if(arg_count >= argc)
      {
        cerr << "Option -t must be followed by a SAT solver name." << endl;
        return true;
      }
      circuit_sat_solver_ = string(argv[arg_count]);
      StringUtils::toLowerCaseIn(circuit_sat_solver_);
      if(circuit_sat_solver_ != "lin_api" &&
         circuit_sat_solver_ != "min_api" &&
         circuit_sat_solver_ != "pic_api")
      {
        cerr << "Unknown SAT solver '" << circuit_sat_solver_ <<"'." << endl;
        return true;
      }
    }
  }
  if(aig_in_file_name_ == "")
  {
    cerr << "No input file given." << endl;
    return true;
  }
  initLogger();
  if(aig_out_file_name_ == "")
  {
    L_WRN("No output file given. Defaulting to \"res_"<< aig_in_file_name_ << "\".");
    aig_out_file_name_ = "res_" + aig_in_file_name_;
  }
  return false;
}

// -------------------------------------------------------------------------------------------
const string& Options::getAigInFileName() const
{
  return aig_in_file_name_;
}

// -------------------------------------------------------------------------------------------
const string Options::getAigInFileNameOnly() const
{
  size_t name_start = aig_in_file_name_.find_last_of("\\/");
  if(name_start == string::npos)
    name_start = 0;
  else
    name_start++;
  // the following line also works if '.' is not found:
  size_t len = aig_in_file_name_.find_last_of(".") - name_start;
  return aig_in_file_name_.substr(name_start, len);
}

// -------------------------------------------------------------------------------------------
const string& Options::getAigOutFileName() const
{
  return aig_out_file_name_;
}

// -------------------------------------------------------------------------------------------
const string& Options::getBackEndName() const
{
  return back_end_;
}

// -------------------------------------------------------------------------------------------
BackEnd* Options::getBackEnd() const
{
  if(back_end_ == "epr")
    return new EPRSynthesizer();

  if(back_end_ == "learn")
  {
    if(mode_ == 0 || mode_ == 1 || mode_ == 4)
      return new LearnSynthQBF(getCircuitExtractor());
    else if(mode_ == 2 || mode_ == 3 || mode_ == 5 || mode_ == 6)
      return new LearnSynthQBFInd(getCircuitExtractor());
    else // m == 7, 8, 9, or 10
      return new LearnSynthQBFInc(getCircuitExtractor());
  }
  if(back_end_ == "learn_sat")
    return new LearnSynthSAT(getCircuitExtractor());
  if(back_end_.find("lp") == 0)
  {
    string rest = back_end_.substr(2);
    size_t nr_of_threads = thread::hardware_concurrency();
    if(rest != "")
    {
      istringstream iss(rest);
      iss >> nr_of_threads;
    }
    return new ParallelLearner(nr_of_threads, getCircuitExtractor());
  }
  if(back_end_ == "templ")
    return new TemplateSynth(getCircuitExtractor());
  if(back_end_ == "ifm")
    return new IFM13Synth(getCircuitExtractor());
  if(back_end_ == "load")
    return new LoadSynth(getCircuitExtractor());
  L_ERR("Unknown back-end '" << back_end_ <<"'.");
  exit(-1);
}

// -------------------------------------------------------------------------------------------
int Options::getBackEndMode() const
{
  return mode_;
}

// -------------------------------------------------------------------------------------------
const string& Options::getCircuitExtractionName() const
{
  return circuit_extraction_name_;
}

// -------------------------------------------------------------------------------------------
int Options::getCircuitExtractionMode() const
{
  return circuit_extraction_mode_;
}

// -------------------------------------------------------------------------------------------
CNFImplExtractor* Options::getCircuitExtractor() const
{
  if(circuit_extraction_name_ == "qbfcert")
    return new QBFCertImplExtractor();
  if(circuit_extraction_name_ == "learn")
    return new LearningImplExtractor();
  if(circuit_extraction_name_ == "interpol")
    return new InterpolImplExtractor();
  if(circuit_extraction_name_ == "store")
    return new StoreImplExtractor();
  L_ERR("Unknown circuit extractor '" << circuit_extraction_name_ <<"'.");
  exit(-1);
}

// -------------------------------------------------------------------------------------------
string Options::getTmpDirName() const
{
  struct stat st;
  if(stat(tmp_dir_.c_str(), &st) != 0)
  {
    int fail = mkdir(tmp_dir_.c_str(), 0777);
    MASSERT(fail == 0, "Could not create directory for temporary files.");
  }
  return tmp_dir_;
}

// -------------------------------------------------------------------------------------------
string Options::getUniqueTmpFileName(string start) const
{
  static int unique_counter = 0;
  ostringstream res;
  res << getTmpDirName() << start << "_" << getAigInFileNameOnly();
  res << "_" << getpid() << "_" << unique_counter++;
  return res.str();
}

// -------------------------------------------------------------------------------------------
string Options::getTPDirName() const
{
  // in the competition, setting environment variables is difficult:
  //return string("./third_party_install/");
  const char *tp_env = getenv(Options::TP_VAR.c_str());
  MASSERT(tp_env != NULL, "You have not set the variable " << Options::TP_VAR);
  return string(tp_env);
}

// -------------------------------------------------------------------------------------------
QBFSolver* Options::getQBFSolver() const
{
  if(qbf_solver_ == "depqbf_ext")
    return new DepQBFExt;
  else if(qbf_solver_ == "depqbf_api")
    return new DepQBFApi(false);
  else if(qbf_solver_ == "blo_dep_api")
    return new DepQBFApi(true);
  else if(qbf_solver_ == "qube_ext")
    return new QuBEExt;
  MASSERT(false, "Unknown QBF solver name.");
  return NULL;
}

// -------------------------------------------------------------------------------------------
SatSolver* Options::getSATSolver(bool rand_models, bool min_cores) const
{
  if(sat_solver_ == "lin_api")
    return new LingelingApi(rand_models, min_cores);
  if(sat_solver_ == "min_api")
    return new MiniSatApi(rand_models, min_cores);
  if(sat_solver_ == "pic_api")
    return new PicoSatApi(rand_models, min_cores);
  MASSERT(false, "Unknown SAT solver name.");
  return NULL;
}

// -------------------------------------------------------------------------------------------
SatSolver* Options::getSATSolverExtr(bool rand_models, bool min_cores) const
{
  if(circuit_sat_solver_ == "")
    return getSATSolver(rand_models, min_cores);
  if(circuit_sat_solver_ == "lin_api")
    return new LingelingApi(rand_models, min_cores);
  if(circuit_sat_solver_ == "min_api")
    return new MiniSatApi(rand_models, min_cores);
  if(circuit_sat_solver_ == "pic_api")
    return new PicoSatApi(rand_models, min_cores);
  MASSERT(false, "Unknown SAT solver name.");
  return NULL;
}

// -------------------------------------------------------------------------------------------
bool Options::doRealizabilityOnly() const
{
  return real_only_;
}

// -------------------------------------------------------------------------------------------
void Options::printHelp() const
{
  cout << "Usage: demiurge-bin [options]"                                           << endl;
  cout                                                                              << endl;
  cout << "Options:"                                                                << endl;
  cout << "  -h, --help"                                                            << endl;
  cout << "                 Show this help message and exit."                       << endl;
  cout << "  -v, --version"                                                         << endl;
  cout << "                 Print version information and exit."                    << endl;
  cout << "  -i INPUT_FILE, --in=INPUT_FILE"                                        << endl;
  cout << "                 The AIGER file defining the synthesis problem."         << endl;
  cout << "                 It can be:"                                             << endl;
  cout << "                  - a binary AIGER file (header starts with 'aig'),"     << endl;
  cout << "                  - an ASCII AIGER file (header starts with 'aag'),"     << endl;
  cout << "                  - a compressed file (INPUT_FILE ends with '.gz')."     << endl;
  cout << "  -o OUTPUT_FILE, --out=OUTPUT_FILE"                                     << endl;
  cout << "                 The AIGER file in which the solution should be stored." << endl;
  cout << "                 The format is determined from OUTPUT_FILE's suffix:"    << endl;
  cout << "                  - '.aag' produces an ASCII AIGER file,"                << endl;
  cout << "                  - '.aag.gz' produces a compressed ASCII AIGER file,"   << endl;
  cout << "                  - other suffixes produce a (compressed) binary file."  << endl;
  cout << "                  Write 'stdout' for OUTPUT_FILE to have the"            << endl;
  cout << "                  synthesis result printed on the standard output."      << endl;
  cout << "                  The default is 'stdout'."                              << endl;
  cout << "  -p PRINT, --print=PRINT"                                               << endl;
  cout << "                 A string indicating which messages to print. Every"     << endl;
  cout << "                 character activates a certain type of message. The"     << endl;
  cout << "                 order and case of the characters does not matter."      << endl;
  cout << "                 Possible characters are:"                               << endl;
  cout << "                 E:      Enables the printing of error messages."        << endl;
  cout << "                 W:      Enables the printing of warnings."              << endl;
  cout << "                 R:      Enables the printing of results."               << endl;
  cout << "                 D:      Enables the printing of debugging messages."    << endl;
  cout << "                 I:      Enables the printing of extra information"      << endl;
  cout << "                         (such as progress information)."                << endl;
  cout << "                 L:      Enables the printing of statistics"             << endl;
  cout << "                         (such as performance measures)."                << endl;
  cout << "                 The default is 'EWRI'."                                 << endl;
  cout << "  -t DIR, --tmp=DIR"                                                     << endl;
  cout << "                 The name of a directory for temporary files. This "     << endl;
  cout << "                 directory will be created if it does not exist. "       << endl;
  cout << "                 The default is './tmp'."                                << endl;
  cout << "  -b BACKEND, --backend=BACKEND"                                         << endl;
  cout << "                 The back-end to be used for synthesis. Different"       << endl;
  cout << "                 back-ends implement different synthesis methods."       << endl;
  cout << "                 The following back-ends are available:"                 << endl;
  cout << "                 learn: Computes the winning region in CNF by learning " << endl;
  cout << "                        clauses iteratively using a QBF solver. "        << endl;
  cout << "                        Circuit extraction is done as specified by the " << endl;
  cout << "                        option '-c' (or --circuit)."                     << endl;
  cout << "                 learn_sat: Like 'learn' but uses 2 SAT solvers instead" << endl;
  cout << "                        of one QBF solver for the computation of the"    << endl;
  cout << "                        winning region."                                 << endl;
  cout << "                        Circuit extraction is done as specified by the " << endl;
  cout << "                        option '-c' (or --circuit)."                     << endl;
  cout << "                 lp<nr>:Computes the winning region by learning "        << endl;
  cout << "                        clauses iteratively with several parallel"       << endl;
  cout << "                        threads. <nr> defines the desired number of"     << endl;
  cout << "                        threads. If <nr> is omitted, then the number of" << endl;
  cout << "                        threads is set equal to the number of cores of"  << endl;
  cout << "                        the CPU. "                                       << endl;
  cout << "                        Circuit extraction is done as specified by the " << endl;
  cout << "                        option '-c' (or --circuit)."                     << endl;
  cout << "                 templ: Computes the winning region as an instantiation" << endl;
  cout << "                        of a template using a QBF solver. QBFCert is "   << endl;
  cout << "                        then used to extract the circuit."               << endl;
  cout << "                        Circuit extraction is done as specified by the " << endl;
  cout << "                        option '-c' (or --circuit)."                     << endl;
  cout << "                 epr:   Reduces the problem to EPR (experimental). "     << endl;
  cout << "                 ifm:   Implements the approach of the paper 'Solving "  << endl;
  cout << "                        Games Using Incremental Induction' by         "  << endl;
  cout << "                        A. Morgenstern, M. Gesell, and K. Schneider, "   << endl;
  cout << "                        published at IFM'13. "                           << endl;
  cout << "                        Circuit extraction is done as specified by the " << endl;
  cout << "                        option '-c' (or --circuit)."                     << endl;
  cout << "                 load:  Loads the winning region from a DIMACS file "    << endl;
  cout << "                        instead of computing it. The circuit extraction" << endl;
  cout << "                        Method specified with the option '-c' is then"   << endl;
  cout << "                        run on the loaded winning region. This can be"   << endl;
  cout << "                        useful for saving computation time when"         << endl;
  cout << "                        benchmarking circuit extraction methods. Run "   << endl;
  cout << "                        the tool with '-c save' to save the winning"     << endl;
  cout << "                        region of a run in DIMACS format first. "        << endl;
  cout << "                 The default is 'learn'."                                << endl;
  cout << "  -m MODE , --mode=MODE"                                                 << endl;
  cout << "                 Some back-ends can be used in several modes (certain"   << endl;
  cout << "                 optimizations or heuristics enabled or disabled, etc.)."<< endl;
  cout << "                 MODE is an integer number to select one such mode. "    << endl;
  cout << "                 The following modes are supported: "                    << endl;
  cout << "                 Back-end 'learn': "                                     << endl;
  cout << "                   0: standard mode"                                     << endl;
  cout << "                   1: use 2 SAT-solvers for counterexample computation"  << endl;
  cout << "                   2: use optimization RG (ignore unreachable states "   << endl;
  cout << "                      during generalization)"                            << endl;
  cout << "                   3: use optimization RG and RC (also ignore"           << endl;
  cout << "                      unreachable counterexample-states)"                << endl;
  cout << "                   4: compute all counterexample-generalizations instead"<< endl;
  cout << "                      of just one"                                       << endl;
  cout << "                   5: compute all generalizations and use RG"            << endl;
  cout << "                   6: compute all generalizations and use both RG and RC"<< endl;
  cout << "                   7: like mode 0 but uses DepQBF with incremental QBF"  << endl;
  cout << "                      solving and a lazy update of the next-state copy"  << endl;
  cout << "                      of the winning region. "                           << endl;
  cout << "                   8: like mode 7 but uses push/pop instead of updating"<< endl;
  cout << "                      the negation of the next-state copy of the winning"<< endl;
  cout << "                      region lazily."                                    << endl;
  cout << "                   9: like mode 7 but uses assumptions to handle the "   << endl;
  cout << "                      the negation of the next-state copy of the "       << endl;
  cout << "                      winning region incrementally, instead of updating" << endl;
  cout << "                      it lazily."                                        << endl;
  cout << "                  10: like mode 4 but uses DepQBF with incremental QBF"  << endl;
  cout << "                      solving and a lazy update of the next-state copy"  << endl;
  cout << "                      of the winning region. "                           << endl;
  cout << "                  11: like mode 10 but uses push/pop instead of updating"<< endl;
  cout << "                      the negation of the next-state copy of the winning"<< endl;
  cout << "                      region lazily."                                    << endl;
  cout << "                  12: like mode 10 but uses assumptions to handle the "  << endl;
  cout << "                      the negation of the next-state copy of the "       << endl;
  cout << "                      winning region incrementally, instead of updating" << endl;
  cout << "                      it lazily."                                        << endl;
  cout << "                 Back-end 'learn_sat': "                                 << endl;
  cout << "                   0: standard mode"                                     << endl;
  cout << "                   1: use optimization RG (ignore unreachable states "   << endl;
  cout << "                      during generalization)"                            << endl;
  cout << "                   2: use optimization RG and RC (also ignore"           << endl;
  cout << "                      unreachable counterexample-states)"                << endl;
  cout << "                 Back-end 'lp<nr>': "                                    << endl;
  cout << "                   0: standard mode"                                     << endl;
  cout << "                   1: use optimization RG (ignore unreachable states "   << endl;
  cout << "                      during generalization)"                            << endl;
  cout << "                 The default is 0."                                      << endl;
  cout << "  -c METHOD, --circuit=METHOD"                                           << endl;
  cout << "                 Most of the back-ends can be used with various circuit" << endl;
  cout << "                 extraction methods (computing an implementation of the" << endl;
  cout << "                 of the specification from the winning region). This "   << endl;
  cout << "                 parameter defines which circuit extraction method "     << endl;
  cout << "                 shall be used. "                                        << endl;
  cout << "                 The following methods are supported: "                  << endl;
  cout << "                 qbfcert: Uses the QBFCert framework to compute the "    << endl;
  cout << "                        circuits as Skolem functions in a QBF."          << endl;
  cout << "                 learn: Computes a circuit with CNF-learning for one "   << endl;
  cout << "                        control signal after the other. A QBF-solver is" << endl;
  cout << "                        used to compute the clauses of the output "      << endl;
  cout << "                        functions."                                      << endl;
  cout << "                 interpol: Computes a circuit with interpolation using " << endl;
  cout << "                        a SAT solver. (Under development, only"          << endl;
  cout << "                        available in a different branch of code)."       << endl;
  cout << "                 store: Stores the winning region into a DIMACS file "   << endl;
  cout << "                        instead of extracting a circuit. This can be"    << endl;
  cout << "                        useful for saving computation time when"         << endl;
  cout << "                        benchmarking circuit extraction methods: you"    << endl;
  cout << "                        do not have to re-compute the winning region"    << endl;
  cout << "                        again and again. Saved winning regions can be"   << endl;
  cout << "                        loaded later with the option '-b load' later."   << endl;
  cout << "                 The default is 'learn'."                                << endl;
  cout << "  -n MODE , --circuit-mode=MODE"                                         << endl;
  cout << "                 Some circuit extraction methods can be used in several" << endl;
  cout << "                 modes (certain optimizations or heuristics enabled or"  << endl;
  cout << "                 disabled, etc.). MODE is an integer number to select"   << endl;
  cout << "                 one such mode. The following modes are supported: "     << endl;
  cout << "                 Method 'qbfcert': "                                     << endl;
  cout << "                   0: standard mode"                                     << endl;
  cout << "                 Method 'learn': "                                       << endl;
  cout << "                   0: uses a QBF Solver."                                << endl;
  cout << "                   1: uses the QBF Solver DepQBF incrementally."         << endl;
  cout << "                   2: uses a SAT Solver, no second minimization run,"    << endl;
  cout << "                      no minimal UNSAT-cores."                           << endl;
  cout << "                   3: uses a SAT Solver, no second minimization run,"    << endl;
  cout << "                      but minimal UNSAT-cores."                          << endl;
  cout << "                   4: uses a SAT Solver, a second minimization run,"     << endl;
  cout << "                      no minimal UNSAT-cores."                           << endl;
  cout << "                   5: uses a SAT Solver, a second minimization run,"     << endl;
  cout << "                      and minimal UNSAT-cores."                          << endl;
  cout << "                   6: like mode 2 but with activation variables for "    << endl;
  cout << "                      more extensive incremental solving."               << endl;
  cout << "                   7: like mode 3 but with activation variables for "    << endl;
  cout << "                      more extensive incremental solving."               << endl;
  cout << "                   8: like mode 4 but with activation variables for "    << endl;
  cout << "                      more extensive incremental solving."               << endl;
  cout << "                   9: like mode 5 but with activation variables for "    << endl;
  cout << "                      more extensive incremental solving."               << endl;
  cout << "                   10:like mode 6 but with even more activation"         << endl;
  cout << "                      variables for even more incrementality."           << endl;
  cout << "                   11:like mode 7 but with even more activation"         << endl;
  cout << "                      variables for even more incrementality."           << endl;
  cout << "                   12:like mode 8 but with even more activation"         << endl;
  cout << "                      variables for even more incrementality."           << endl;
  cout << "                   13:like mode 9 but with even more activation"         << endl;
  cout << "                      variables for even more incrementality."           << endl;
  cout << "                   14:like mode 2 but with a dependency optimization"    << endl;
  cout << "                      allowing ctrl-signals to depend on auxiliary"      << endl;
  cout << "                      variables from the transition relation."           << endl;
  cout << "                   15:like mode 3 but with a dependency optimization"    << endl;
  cout << "                      allowing ctrl-signals to depend on auxiliary"      << endl;
  cout << "                      variables from the transition relation."           << endl;
  cout << "                   16:like mode 4 but with a dependency optimization"    << endl;
  cout << "                      allowing ctrl-signals to depend on auxiliary"      << endl;
  cout << "                      variables from the transition relation."           << endl;
  cout << "                   17:like mode 5 but with a dependency optimization"    << endl;
  cout << "                      allowing ctrl-signals to depend on auxiliary"      << endl;
  cout << "                      variables from the transition relation."           << endl;
  cout << "                   18:like mode 14 but with an extended dependency"      << endl;
  cout << "                      optimization where ctrl-signals are allowed to"    << endl;
  cout << "                      depend on more other ctrl-signals."                << endl;
  cout << "                   19:like mode 15 but with an extended dependency"      << endl;
  cout << "                      optimization where ctrl-signals are allowed to"    << endl;
  cout << "                      depend on more other ctrl-signals."                << endl;
  cout << "                   20:like mode 16 but with an extended dependency"      << endl;
  cout << "                      optimization where ctrl-signals are allowed to"    << endl;
  cout << "                      depend on more other ctrl-signals."                << endl;
  cout << "                   21:like mode 17 but with an extended dependency"      << endl;
  cout << "                      optimization where ctrl-signals are allowed to"    << endl;
  cout << "                      depend on more other ctrl-signals."                << endl;
  cout << "                   22:like mode 18 but with more incremental solving."   << endl;
  cout << "                   23:like mode 19 but with more incremental solving."   << endl;
  cout << "                   24:like mode 20 but with more incremental solving."   << endl;
  cout << "                   25:like mode 21 but with more incremental solving."   << endl;
  cout << "                 The default is 0."                                      << endl;
  cout << "  -r, --real_only"                                                       << endl;
  cout << "                 Only check realizability (compute the winning region)"  << endl;
  cout << "                 but do not synthesize a circuit."                       << endl;
  cout << "  -q QBF_SOLVER, --qbf_sv=QBF_SOLVER"                                    << endl;
  cout << "                 The QBF solver to use."                                 << endl;
  cout << "                 The following QBF solvers are available:"               << endl;
  cout << "                 depqbf_ext: Uses the DepQBF solver in an external"      << endl;
  cout << "                        process, communicating via files."               << endl;
  cout << "                 depqbf_api: Uses the DepQBF solver via its API. "       << endl;
  cout << "                 blo_dep_api: Uses Bloqqer and DepQBF via an API. "      << endl;
  cout << "                 qube_ext: Uses the QuBE solver in an external process," << endl;
  cout << "                        communicating via files, and DepQBF if a model"  << endl;
  cout << "                        is required (QuBE cannot create models)."        << endl;
  cout << "                 The default is 'depqbf_api'."                           << endl;
  cout << "  -s SAT_SOLVER, --sat_sv=SAT_SOLVER"                                    << endl;
  cout << "                 The SAT solver to use."                                 << endl;
  cout << "                 The following SAT solvers are available:"               << endl;
  cout << "                 lin_api: Uses the Lingeling solver via its API."        << endl;
  cout << "                 min_api: Uses the MiniSat solver via its API."          << endl;
  cout << "                 pic_api: Uses the PicoSat solver via its API."          << endl;
  cout << "                 The default is 'lin_api'."                              << endl;
  cout << "  -e SAT_SOLVER, --extr_sat_sv=SAT_SOLVER"                               << endl;
  cout << "                 The SAT solver to use for circuit extraction."          << endl;
  cout << "                 (if you want it to be different from the solver used"   << endl;
  cout << "                 for computing the winning region)."                     << endl;
  cout << "                 The following SAT solvers are available:"               << endl;
  cout << "                 lin_api: Uses the Lingeling solver via its API."        << endl;
  cout << "                 min_api: Uses the MiniSat solver via its API."          << endl;
  cout << "                 pic_api: Uses the PicoSat solver via its API."          << endl;
  cout << "                 The default is: same as with -s."                       << endl;
  cout << "Have fun!"                                                               << endl;
}

// -------------------------------------------------------------------------------------------
void Options::initLogger() const
{
  Logger &logger = Logger::instance();
  if(print_string_.find("E") != string::npos || print_string_.find("e") != string::npos)
    logger.enable(Logger::ERR);
  else
    logger.disable(Logger::ERR);
  if(print_string_.find("W") != string::npos || print_string_.find("w") != string::npos)
    logger.enable(Logger::WRN);
  else
    logger.disable(Logger::WRN);
  if(print_string_.find("R") != string::npos || print_string_.find("r") != string::npos)
    logger.enable(Logger::RES);
  else
    logger.disable(Logger::RES);
  if(print_string_.find("I") != string::npos || print_string_.find("i") != string::npos)
    logger.enable(Logger::INF);
  else
    logger.disable(Logger::INF);
  if(print_string_.find("D") != string::npos || print_string_.find("d") != string::npos)
    logger.enable(Logger::DBG);
  else
    logger.disable(Logger::DBG);
  if(print_string_.find("L") != string::npos || print_string_.find("l") != string::npos)
    logger.enable(Logger::LOG);
  else
    logger.disable(Logger::LOG);
}

// -------------------------------------------------------------------------------------------
Options::Options():
    aig_in_file_name_(),
    aig_out_file_name_("stdout"),
    print_string_("ERWI"),
    tmp_dir_("./tmp"),
    back_end_("learn"),
    mode_(0),
    circuit_extraction_name_("learn"),
    qbf_solver_("depqbf_api"),
    sat_solver_("lin_api"),
    circuit_sat_solver_(""),
    real_only_(false)
{
  // nothing to be done
}

// -------------------------------------------------------------------------------------------
Options::~Options()
{
  // nothing to be done
}
