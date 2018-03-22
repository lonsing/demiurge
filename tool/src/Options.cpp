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

#include <sys/stat.h>

// -------------------------------------------------------------------------------------------
const string Options::VERSION = string("1.0.0");

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
  if(back_end_ == "learn")
  {
    if(mode_ == 0 || mode_ == 1 || mode_ == 4)
      return new LearnSynthQBF();
    else if(mode_ == 2 || mode_ == 3 || mode_ == 5 || mode_ == 6)
      return new LearnSynthQBFInd();
    else // m == 7 or 8 (experimental)
      return new LearnSynthQBFInc();
  }
  if(back_end_ == "learn_sat")
    return new LearnSynthSAT();
  if(back_end_.find("lp") == 0)
  {
    string rest = back_end_.substr(2);
    size_t nr_of_threads = thread::hardware_concurrency();
    if(rest != "")
    {
      istringstream iss(rest);
      iss >> nr_of_threads;
    }
    return new ParallelLearner(nr_of_threads);
  }
  if(back_end_ == "templ")
    return new TemplateSynth();
  if(back_end_ == "epr")
    return new EPRSynthesizer();
  if(back_end_ == "ifm")
    return new IFM13Synth();
  L_ERR("Unknown back-end '" << back_end_ <<"'.");
  exit(-1);
}

// -------------------------------------------------------------------------------------------
int Options::getBackEndMode() const
{
  return mode_;
}

// -------------------------------------------------------------------------------------------
string Options::getTmpDirName() const
{
  string tmp_dir_name = "tmp/";
  struct stat st;
  if(stat(tmp_dir_name.c_str(), &st) != 0)
  {
    int fail = mkdir(tmp_dir_name.c_str(), 0777);
    MASSERT(fail == 0, "Could not create directory for temporary files.");
  }
  return tmp_dir_name;
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
  cout << "  -b BACKEND, --backend=BACKEND"                                         << endl;
  cout << "                 The back-end to be used for synthesis. Different"       << endl;
  cout << "                 back-ends implement different synthesis methods."       << endl;
  cout << "                 The following back-ends are available:"                 << endl;
  cout << "                 learn: Computes the winning region in CNF by learning " << endl;
  cout << "                        clauses iteratively using a QBF solver. QBFCert" << endl;
  cout << "                        is then used to extract the circuit."            << endl;
  cout << "                 learn_sat: Like 'learn' but uses 2 SAT solvers instead" << endl;
  cout << "                        of one QBF solver for the computation of the"    << endl;
  cout << "                        winning region."                                 << endl;
  cout << "                 lp<nr>:Computes the winning region by learning "        << endl;
  cout << "                        clauses iteratively with several parallel"       << endl;
  cout << "                        threads. <nr> defines the desired number of"     << endl;
  cout << "                        threads. If <nr> is omitted, then the number of" << endl;
  cout << "                        threads is set equal to the number of cores of"  << endl;
  cout << "                        the CPU. QBFCert is finally used to extract a"   << endl;
  cout << "                        circuit (this part is not parallel)."            << endl;
  cout << "                 templ: Computes the winning region as an instantiation" << endl;
  cout << "                        of a template using a QBF solver. QBFCert is "   << endl;
  cout << "                        then used to extract the circuit."               << endl;
  cout << "                 epr:   Reduces the problem to EPR (experimental). "     << endl;
  cout << "                 ifm:   Implements the approach of the paper 'Solving "  << endl;
  cout << "                        Games Using Incremental Induction' by         "  << endl;
  cout << "                        A. Morgenstern, M. Gesell, and K. Schneider, "   << endl;
  cout << "                        published at IFM'13. QBFCert is then used to "   << endl;
  cout << "                        extract the circuit."                            << endl;
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
  cout << "                   7: like mode 0 but uses a special version of DepQBF"  << endl;
  cout << "                      that can compute unsatisfiable cores. This mode "  << endl;
  cout << "                      is still experimental. "                           << endl;
  cout << "                   8: like mode 4 but uses a special version of DepQBF"  << endl;
  cout << "                      that can compute unsatisfiable cores. This mode "  << endl;
  cout << "                      is still experimental. "                           << endl;
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
    aig_out_file_name_(),
    print_string_("ERWI"),
    back_end_("learn"),
    mode_(0),
    qbf_solver_("depqbf_api"),
    sat_solver_("lin_api"),
    real_only_(false)
{
  // nothing to be done
}

// -------------------------------------------------------------------------------------------
Options::~Options()
{
  // nothing to be done
}
