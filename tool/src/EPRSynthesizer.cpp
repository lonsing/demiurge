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
/// @file EPRSynthesizer.cpp
/// @brief Contains the definition of the class EPRSynthesizer.
// -------------------------------------------------------------------------------------------

#include "EPRSynthesizer.h"
#include "Options.h"
#include "StringUtils.h"
#include "FileUtils.h"
#include "Logger.h"

extern "C" {
 #include "aiger.h"
}

// -------------------------------------------------------------------------------------------
EPRSynthesizer::EPRSynthesizer() : BackEnd(), next_clause_nr_(0)
{
  const char *tp_env = getenv(Options::TP_VAR.c_str());
  MASSERT(tp_env != NULL, "You have not set the variable " << Options::TP_VAR);
  path_to_iprover_ = string(tp_env) + "/iprover/iprover/iproveropt";
  ostringstream unique_string;
  unique_string << this;
  string temp_dir = Options::instance().getTmpDirName();
  in_file_name_ = temp_dir + "epr_query_" + unique_string.str() + ".tptp";
  out_file_name_ = temp_dir + "epr_answer_" + unique_string.str() + ".out";
}

// -------------------------------------------------------------------------------------------
EPRSynthesizer::~EPRSynthesizer()
{
  // nothing to be done
}

// -------------------------------------------------------------------------------------------
bool EPRSynthesizer::run()
{
  tptp_query_.clear();
  const char *error = NULL;
  aiger *aig = aiger_init();
  const string &file = Options::instance().getAigInFileName();
  error = aiger_open_and_read_from_file (aig, file.c_str());
  MASSERT(error == NULL, "Could not open AIGER file " << file << " (" << error << ").");
  MASSERT(aig->num_outputs == 1, "Strange number of outputs in AIGER file.");

  // 'names' maps AIGER literals to corresponding EPR formulas.
  vector<string> names(2*(aig->maxvar+1), "");
  names[0] = "p(0)";
  names[1] = "p(1)";
  // For every AIGER literal, deps stores the inputs and state bits on which it depends.
  map<unsigned, set<string> > deps;
  set<string> emptyset;
  deps[0] = emptyset;
  deps[1] = emptyset;
  // The prefix 'X_' means current state, 'Y_' means next state, 'I_' means
  // uncontrollable input:
  string all_inputs_and_states = "X_err";
  set<string> all_inputs_and_states_set;
  all_inputs_and_states_set.insert("X_err");
  string all_states = "X_err";
  string all_next_states = "Y_err";

  // Analyze the uncontrollable inputs and fill the data structures (names, deps, ...)
  for(unsigned cnt = 0; cnt < aig->num_inputs; ++cnt)
  {
    string aig_name;
    if(aig->inputs[cnt].name != NULL)
      aig_name = aig->inputs[cnt].name;
    string aig_name_lower = StringUtils::toLowerCase(aig_name);
    if(aig_name_lower.find("controllable_") != 0)
    {
      unsigned lit = aig->inputs[cnt].lit;
      ostringstream oss;
      oss << lit;
      names[lit] = "p(I_" + oss.str() + ")";
      names[lit + 1] = "~p(I_" + oss.str() + ")";
      all_inputs_and_states += ",I_" + oss.str();
      all_inputs_and_states_set.insert("I_" + oss.str());
      set<string> dep_set;
      dep_set.insert("I_" + oss.str());
      deps[lit] = dep_set;
      deps[lit + 1] = dep_set;
    }
  }
  // Analyze the state variables and fill the data structures (names, deps, ...)
  for(unsigned cnt = 0; cnt < aig->num_latches; ++cnt)
  {
    // present state:
    unsigned lit = aig->latches[cnt].lit;
    ostringstream oss;
    oss << lit;
    names[lit] = "p(X_" + oss.str() + ")";
    names[lit + 1] = "~p(X_" + oss.str() + ")";
    all_inputs_and_states += ",X_" + oss.str();
    all_inputs_and_states_set.insert("X_" + oss.str());
    all_states += ",X_" + oss.str();
    all_next_states += ",Y_" + oss.str();
    set<string> dep_set;
    dep_set.insert("X_" + oss.str());
    deps[lit] = dep_set;
    deps[lit + 1] = dep_set;
  }

  // Analyze the uncontrollable inputs and fill the data structures (names, deps, ...)
  for(unsigned cnt = 0; cnt < aig->num_inputs; ++cnt)
  {
    string aig_name;
    if(aig->inputs[cnt].name != NULL)
      aig_name = aig->inputs[cnt].name;
    string aig_name_lower = StringUtils::toLowerCase(aig_name);
    if(aig_name_lower.find("controllable_") == 0)
    {
      unsigned lit = aig->inputs[cnt].lit;
      ostringstream oss;
      oss << "csk_" << lit  << "("  << all_inputs_and_states << ")";
      names[lit] = oss.str();
      names[lit + 1] = "~" + oss.str();
      deps[lit] = all_inputs_and_states_set;
      deps[lit + 1] = all_inputs_and_states_set;
    }
  }

  // let's find out on which inputs and state bits the outputs of the AND gates
  // depend:
  bool something_found = true;
  while(something_found)
  {
    something_found = false;
    for (unsigned cnt = 0; cnt < aig->num_ands; cnt++)
    {
      unsigned lit = aig->ands[cnt].lhs;
      unsigned rhs0 = aig->ands[cnt].rhs0;
      unsigned rhs1 = aig->ands[cnt].rhs1;
      if(deps.count(lit) == 0 && deps.count(rhs0) != 0 && deps.count(rhs1) != 0)
      {
        something_found = true;
        set<string> lit_deps(deps[rhs0]);
        lit_deps.insert(deps[rhs1].begin(), deps[rhs1].end());
        deps[lit] = lit_deps;
        deps[lit+1] = lit_deps;
        ostringstream oss;
        oss << "tmpsk_" << lit;
        if(!lit_deps.empty())
          oss << "(";
        for(set<string>::const_iterator it = lit_deps.begin(); it != lit_deps.end(); ++it)
        {
          if(it == lit_deps.begin())
            oss << *it;
          else
            oss << "," << *it;
        }
        if(!lit_deps.empty())
          oss << ")";
        names[lit] = oss.str();
        names[lit+1] = "~" + oss.str();
      }
    }
  }

#ifndef NDEBUG
  for (unsigned cnt = 0; cnt < aig->num_ands; cnt++)
  {
    if(names[aig->ands[cnt].lhs] == "")
    {
      L_ERR("and gate " << aig->ands[cnt].lhs << " has no name.");
      MASSERT(false, "Some AND gate has no name.");
    }
    else if(names[aig->ands[cnt].lhs + 1] == "")
    {
      L_ERR("neg and gate " << aig->ands[cnt].lhs << " has no name.");
      MASSERT(false, "Some AND gate has no name.");
    }
  }
#endif

  string pres_win = "win(" + all_states + ")";
  if(all_states.size() == 0)
    pres_win = "win";
  string next_win = "win(" + all_next_states + ")";
  if(all_next_states.size() == 0)
    next_win = "win";

  // 1. initial implies winning I(x) -> P(x):
  string initial_implies_winning = "p(X_err)";
  for(unsigned cnt = 0; cnt < aig->num_latches; ++cnt)
    initial_implies_winning += " | " + names[aig->latches[cnt].lit];
  initial_implies_winning += " | " + pres_win;
  addClause(initial_implies_winning, "The initial state is winning: I(X) -> W(X)");

  // 2. winning implies safe W(x) -> P(x):
  string c = "All winning states are safe: W(X) -> P(X)";
  addClause("~" + pres_win + " | ~p(X_err)", c);

  // 3. encode the transition relation t <-> T(x,i,c,x')
  // 3.1 AND gates:
  for (unsigned cnt = 0; cnt < aig->num_ands; cnt++)
  {
    unsigned lhs = aig->ands[cnt].lhs;
    unsigned rhs0 = aig->ands[cnt].rhs0;
    unsigned rhs1 = aig->ands[cnt].rhs1;
    // (lhs --> rhs1) AND (lhs --> rhs0)
    ostringstream gate_name;
    gate_name << "AND Gate (AIG" << lhs << " = AIG" << rhs0 << " & AIG" << rhs1 << "), ";
    c = gate_name.str() + "rule 1: l->r0";
    addClause(names[neg(lhs)] + " | " + names[rhs0], c);
    c = gate_name.str() + "rule 2: l->r1";
    addClause(names[neg(lhs)] + " | " + names[rhs1], c);
    // (!lhs --> (!rhs1 OR !rhs0)
    c = gate_name.str() + "rule 3: !l -> (!r0 | !r1)";
    addClause(names[lhs] + " | " + names[neg(rhs0)] + " | " + names[neg(rhs1)], c);
  }

  // 3.2 next state variables are set correctly:
  string t = "tsk(" + all_inputs_and_states + "," + all_next_states +")";
  // all_equal_clause contains: err_eq_name AND eq_name1 AND eq_name2 AND ... --> t
  string all_equal_clause = t;

  // 3.2.1 fake error latch:
  unsigned err_aig = aig->outputs[0].lit;
  string err_eq_name = "teqsk_err(Y_err";
  for(set<string>::const_iterator it = deps[err_aig].begin(); it != deps[err_aig].end(); ++it)
    err_eq_name += "," + *it;
  err_eq_name += ")";
  c = "teqsk_err <-> (Y_err <-> err_out), case 1: (!Y_err & !error) -> teqsk_err";
  addClause("p(Y_err) | " + names[err_aig] + " | " + err_eq_name, c);
  c = "teqsk_err <-> (Y_err <-> err_out), case 2: (!Y_err & error) -> !teqsk_err";
  addClause("p(Y_err) | " + names[neg(err_aig)] + " | ~" + err_eq_name, c);
  c = "teqsk_err <-> (Y_err <-> err_out), case 3: (Y_err & !error) -> !teqsk_err";
  addClause("~p(Y_err) | " + names[err_aig] + " | ~" + err_eq_name, c);
  c = "teqsk_err <-> (Y_err <-> err_out), case 4: (Y_err & error) -> teqsk_err";
  addClause("~p(Y_err) | " + names[neg(err_aig)] + " | " + err_eq_name, c);
  // err_eq_name is false --> t is false
  c = "!teqsk_err -> !t";
  addClause(err_eq_name + " | ~" + t, c);
  all_equal_clause += " | ~" + err_eq_name;

  // 3.2.2 real latches:
  for(unsigned cnt = 0; cnt < aig->num_latches; ++cnt)
  {
    unsigned n_aig = aig->latches[cnt].next;
    unsigned p_aig = aig->latches[cnt].lit;
    ostringstream ps;
    ps << p_aig;
    string eq_name = "teqsk_" + ps.str() + "(Y_" + ps.str();
    for(set<string>::const_iterator it = deps[n_aig].begin(); it != deps[n_aig].end(); ++it)
      eq_name += "," + *it;
    eq_name += ")";

    // eq_name is true    iff   n_aig is equal to p_aig
    string c0 = "teqsk_" + ps.str() + " <-> (Y_" + ps.str() + " <-> " + names[n_aig] + ")";
    c = c0 + ", case 1: (!Y_" + ps.str() + " & !" + names[n_aig] + ") -> teqsk_" + ps.str();
    addClause("p(Y_" + ps.str() + ") | " + names[n_aig] + " | " + eq_name, c);
    c = c0 + ", case 2: (!Y_" + ps.str() + " & " + names[n_aig] + ") -> !teqsk_" + ps.str();
    addClause("p(Y_" + ps.str() + ") | " + names[neg(n_aig)] + " | ~" + eq_name, c);
    c = c0 + ", case 2: (Y_" + ps.str() + " & !" + names[n_aig] + ") -> !teqsk_" + ps.str();
    addClause("~p(Y_" + ps.str() + ") | " + names[n_aig] + " | ~" + eq_name, c);
    c = c0 + ", case 2: (Y_" + ps.str() + " & " + names[n_aig] + ") -> teqsk_" + ps.str();
    addClause("~p(Y_" + ps.str() + ") | " + names[neg(n_aig)] + " | " + eq_name, c);
    // eq_name is false --> t is false
    c = "!teqsk_" + ps.str() + " -> !t";
    addClause(eq_name + " | ~" + t, c);
    all_equal_clause += " | ~" + eq_name;
  }
  c = "(all teqsk_* are true) -> (t is true)";
  addClause(all_equal_clause, c);

  // 4. W(x) && T(x,i,c,x') --> W(x')
  c = "W(X) && T(X,I,c(X,I),Y) --> W(Y)";
  addClause("~" + pres_win + " | ~" + t + " | " + next_win, c);

  tptp_query_ << "cnf(rule_true,axiom, p(1))." << endl;
  tptp_query_ << "cnf(rule_false,axiom, ~p(0))." << endl;

  // Now we are done creatint the EPR formula. Let's start the solver:

  FileUtils::writeFile(in_file_name_, tptp_query_.str());
  string solver_command = path_to_iprover_;
  solver_command += " --out_options none";
  solver_command += " --fof false"; // assumes cnf
  // a schedule which works well for verification problems encoded in the EPR fragment:
  solver_command += " --schedule verification_epr";
  solver_command += " --sat_epr_types true"; // using EPR types in sat mode
  // solver_command += " --sat_mode true";
  // solver_command += " --sat_finite_models true";
  // solver_command += " --sat_out_model small"; // <small|pos|neg|implied|debug|intel|none>
  solver_command += " " + in_file_name_ + " > " + out_file_name_;
  int ret = system(solver_command.c_str());
  ret = WEXITSTATUS(ret);
  MASSERT(ret == 0, "Strange exit code from iprover.");

  string result_file_content;
  FileUtils::readFile(out_file_name_, result_file_content);
  remove(in_file_name_.c_str());
  remove(out_file_name_.c_str());
  if(result_file_content.find("SZS status Unsatisfiable") != string::npos)
  {
    L_RES("The specification is unrealizable.");
    return false;
  }
  if(result_file_content.find("SZS status Satisfiable") != string::npos)
  {
    L_RES("The specification is realizable.");
    if(!Options::instance().doRealizabilityOnly())
    {
      MASSERT(false, "TODO: circuit extraction not yet implemented.");
    }
    return true;
  }
  L_ERR("Strange response from iprover.");
  return false;
}
// -------------------------------------------------------------------------------------------
void EPRSynthesizer::addClause(const string &clause, string comment)
{
  tptp_query_ << "% --------------------------------------------------------" << endl;
  if(comment != "")
    tptp_query_ << "% " << comment << endl;
  tptp_query_ << "cnf(cl_" << next_clause_nr_ << ", plain, (";
  tptp_query_ << clause;
  tptp_query_ << "))." << endl << endl;
  next_clause_nr_++;
}

// -------------------------------------------------------------------------------------------
unsigned EPRSynthesizer::neg(unsigned aig_lit)
{
  return aig_lit ^ 1U;
}

