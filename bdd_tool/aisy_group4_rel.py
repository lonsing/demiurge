#!/usr/bin/env python2.7
# coding=utf-8

"""
An example of synthesis tool from Aiger http://fmv.jku.at/aiger/ circuits format.
Implementations of some functions are omitted to give you chance to implement them.

Basic stuff is left: parsing, and some helper functions.

Installation requirements:
  - pycudd library: http://bears.ece.ucsb.edu/pycudd.html
  - swig library: http://www.swig.org/
  - (probably) python2.7 headers

After installing pycudd library add cudd libraries into your LD_LIBRARY_PATH:

export LD_LIBRARY_PATH=/path/to/pycudd2.0.2/cudd-2.4.2/lib

To run:

./aisy.py -h

Some self-testing functionality is included in ``run_status_tests.py``.

More extensive tests are provided by Robert in script ``performance_test.sh``.
This script also runs model checker to check the results.

More details you will find in the code, on web-page of the course.

Email me in case questions/suggestions/bugs: ayrat.khalimov at gmail

----------------------
"""


import argparse
import pycudd
import time
from aiger_swig.aiger_wrap import *
from aiger_swig.aiger_wrap import aiger

#don't change status numbers since they are used by the performance script
EXIT_STATUS_REALIZABLE = 10
EXIT_STATUS_UNREALIZABLE = 20


#: :type: aiger
spec = None

#: :type: DdManager
cudd = None

# error output can be latched or unlatched, latching makes things look like in lectures, lets emulate this
#: :type: aiger_symbol
error_fake_latch = None


# cache
lit_to_bdd = dict()
bdd_to_lit = dict()


transition_bdd = None
init_state_bdd = None
not_error_bdd = None
win_region = None
strategy = None
func_by_var = None

rel_det_start_wall = None
rel_det_start_cpu = None


def next_lit():
    """ :return: next possible to add to the spec literal """
    return (int(spec.maxvar) + 1) * 2


def negated(lit):
    return lit ^ 1


def is_negated(l):
    return (l & 1) == 1


def strip_lit(l):
    return l & ~1


def introduce_error_latch_if():
    global error_fake_latch
    if error_fake_latch:
        return

    error_fake_latch = aiger_symbol()
    #: :type: aiger_symbol
    error_symbol = get_err_symbol()

    error_fake_latch.lit = next_lit()
    error_fake_latch.name = 'fake_error_latch'
    error_fake_latch.next = error_symbol.lit


def iterate_latches_and_error():
    introduce_error_latch_if()

    for i in range(int(spec.num_latches)):
        yield get_aiger_symbol(spec.latches, i)

    yield error_fake_latch


def parse_into_spec(aiger_file_name):
    global spec

    spec = aiger_init()

    err = aiger_open_and_read_from_file(spec, aiger_file_name)

    assert not err, err

    introduce_error_latch_if()


def init_cudd():
    global cudd
    cudd = pycudd.DdManager()
    cudd.SetDefault()
    #cudd.AutodynEnable(5) # SIFT_CONV
    cudd.AutodynEnable(4) # SIFT


def get_lit_type(stripped_lit):
    if stripped_lit == error_fake_latch.lit:
        return None, error_fake_latch, None

    input_ = aiger_is_input(spec, stripped_lit)
    latch_ = aiger_is_latch(spec, stripped_lit)
    and_ = aiger_is_and(spec, strip_lit(stripped_lit))

    return input_, latch_, and_


def get_primed_variable_as_bdd(lit):
    stripped_lit = strip_lit(lit)
    return cudd.IthVar(stripped_lit + 1)  # we know that odd vars cannot be used as names of latches/inputs


def make_bdd_eq(value1, value2):
    return (value1 & value2) | (~value1 & ~value2)


def get_err_symbol():
    assert spec.num_outputs == 1
    return spec.outputs  # swig return one element instead of array, to iterate over array use iterate_symbols


def get_cube(variables):
    assert len(variables)

    cube = cudd.One()
    for v in variables:
        cube &= v
    return cube


def get_bdd_for_value(lit):
    """ Convert AIGER literal lit into BDD.
    In case of latches - return corresponding BDD variable, for AND/NOT gates -- function.

    :param lit: 'signed' value of literal
    :return: BDD representation of the input literal
    """
    stripped_lit = strip_lit(lit)
    
    # query cache
    if lit in lit_to_bdd:
        return lit_to_bdd[lit]
    
    intput, latch, and_gate = get_lit_type(stripped_lit)
    
    if intput or latch:
        result = cudd.IthVar(stripped_lit)
    elif and_gate:
        result = get_bdd_for_value(and_gate.rhs0) & get_bdd_for_value(and_gate.rhs1)
    else: # 0 literal, 1 literal and errors
        result = cudd.Zero()

    # cache result
    lit_to_bdd[stripped_lit] = result
    bdd_to_lit[result] = stripped_lit

    if is_negated(lit):
        result = ~result
        lit_to_bdd[lit] = result
        bdd_to_lit[result] = lit

    return result


def _get_bdd_vars(filter_func):
    var_bdds = []

    for i in range(int(spec.num_inputs)):
        input_aiger_symbol = get_aiger_symbol(spec.inputs, i)
        if filter_func(input_aiger_symbol.name.strip()):
            out_var_bdd = get_bdd_for_value(input_aiger_symbol.lit)
            var_bdds.append(out_var_bdd)

    return var_bdds


def get_controllable_inputs_bdds():
    return _get_bdd_vars(lambda name: name.startswith('controllable'))


def get_uncontrollable_inputs_bdds():
    return _get_bdd_vars(lambda name: not name.startswith('controllable'))


def get_all_latches_as_bdds():
    bdds = [get_bdd_for_value(l.lit) for l in iterate_latches_and_error()]
    return bdds


def prime_latches_in_bdd(states_bdd):
    latch_bdds = get_all_latches_as_bdds()
    num_latches = len(latch_bdds)
    #: :type: DdArray
    next_var_array = pycudd.DdArray(num_latches)
    curr_var_array = pycudd.DdArray(num_latches)

    for l_bdd in latch_bdds:
        #: :type: DdNode
        l_bdd = l_bdd
        curr_var_array.Push(l_bdd)

        lit = l_bdd.NodeReadIndex()
        primed_l_bdd = get_primed_variable_as_bdd(lit)
        next_var_array.Push(primed_l_bdd)

    primed_states_bdd = states_bdd.SwapVariables(curr_var_array, next_var_array, num_latches)

    return primed_states_bdd


def compose_transition_bdd():
    """ :return: BDD representing transition function of spec: ``T(x,i,c,x')``
    """
    bdd = cudd.One()
    for x in iterate_latches_and_error():
        bdd &= make_bdd_eq(get_primed_variable_as_bdd(x.lit), get_bdd_for_value(x.next))

    return bdd


def compose_init_state_bdd():
    bdd = cudd.One()
    for x in iterate_latches_and_error():
        bdd &= ~get_bdd_for_value(x.lit)

    return bdd


def pre_sys_bdd(dst_states_bdd, transition_bdd):
    """ Calculate predecessor states of given states.

    :return: BDD representation of predecessor states

    :hints: if current states are not primed they should be primed before calculation (why?)
    :hints: calculation of ``∃o t(a,b,o)`` using cudd: ``t.ExistAbstract(get_cube(o))``
    :hints: calculation of ``∀i t(a,b,i)`` using cudd: ``t.UnivAbstract(get_cube(i))``
    """
    
    primed_states = prime_latches_in_bdd(dst_states_bdd)
    abstract_bdd = get_cube(get_controllable_inputs_bdds()) & prime_latches_in_bdd(get_cube(get_all_latches_as_bdds()))
    
    pre_bdd = transition_bdd.AndAbstract( primed_states, abstract_bdd)
    pre_bdd = pre_bdd.UnivAbstract(get_cube(get_uncontrollable_inputs_bdds()))
    
    return pre_bdd


def suc_sys_bdd(src_states_bdd, transition_bdd):
    abstract_bdd = get_cube(get_controllable_inputs_bdds() + get_uncontrollable_inputs_bdds() + get_all_latches_as_bdds())

    suc_bdd = transition_bdd.AndAbstract(src_states_bdd, abstract_bdd)

    return prime_latches_in_bdd(suc_bdd)


def calc_win_region(init_state_bdd, transition_bdd, not_error_bdd):
    """ Calculate a winning region for safety game.
    :return: BDD representing the winning region
    """
    prev_states = cudd.Zero()
    safe_states = cudd.One()
    while safe_states != prev_states:
        prev_states = safe_states
        safe_states = not_error_bdd & pre_sys_bdd(safe_states, transition_bdd)
    
    if (safe_states & init_state_bdd) == cudd.Zero():
        return cudd.Zero()
    
    return safe_states


def calc_reachable_states(init_state_bdd, transition_bdd):
    prev_states = cudd.One()
    reachable_states = cudd.Zero()
    while reachable_states != prev_states:
        prev_states = reachable_states
        reachable_states = init_state_bdd | suc_sys_bdd(reachable_states, transition_bdd)
    
    return reachable_states


def get_nondet_strategy(win_region_bdd, transition_bdd):
    """ Get non-deterministic strategy from winning region.
    If system outputs controllable values that satisfy this strategy, then the system wins.
    That is, non-deterministic strategy represents all possible values of outputs
    in particular state that leads to win:

    ``strategy(x,i,c) = ∃x' W(x) & W(x') & T(x,i,c,x')``

    (Why system cannot lose if adhere to this strategy?)

    :return: non deterministic strategy bdd
    :note: The strategy is still not-deterministic. Determinization step is done later.
    """

    abstract_bdd = prime_latches_in_bdd(get_cube(get_all_latches_as_bdds())) & get_bdd_for_value(error_fake_latch.lit)
    
    return transition_bdd.AndAbstract(win_region_bdd & prime_latches_in_bdd(win_region_bdd),abstract_bdd)


def extract_output_funcs(strategy):
    """ Calculate BDDs for output functions given non-deterministic winning strategy.

    :param strategy: non-deterministic winning strategy
    :return: dictionary ``controllable_variable_bdd -> func_bdd``
    :hint: to calculate Cofactors in cudd: ``bdd.Cofactor(var_as_bdd)`` or ``bdd.Cofactor(~var_as_bdd)``
    :hint: to calculate Restrict in cudd: ``func.Restrict(care_set)``
           (on care_set: ``func.Restrict(care_set) <-> func``)
    """

    res = dict()
    x_vars = get_all_latches_as_bdds() + get_uncontrollable_inputs_bdds()
    y_vars = get_controllable_inputs_bdds()
    inputs = get_controllable_inputs_bdds() + get_uncontrollable_inputs_bdds()

    r = calc_reachable_states(init_state_bdd, strategy)

    for y in y_vars:
        rho_prime = strategy
        other_y_vars = [x for x in y_vars if x != y]
        if other_y_vars:
            rho_prime = strategy.ExistAbstract(get_cube(other_y_vars))
        
        p = rho_prime.Cofactor(y)
        n = rho_prime.Cofactor(~y)
        
        p = (p & ~n) & r
        n = (n & ~p) & r

        for i in inputs:
            p_prime = p.ExistAbstract(i)
            n_prime = n.ExistAbstract(i)
            if p_prime & n_prime == cudd.Zero():
                p = p_prime
                n = n_prime

        # strategy minimizing
        c = (p & ~n) | (~p & n)
        res[y] = p.Restrict(c)

        # BEGIN added by RK
        strategy=strategy.Compose(res[y],y.NodeReadIndex())
        # END added by RK

    return res


def synthesize():
    """ Calculate winning region and extract output functions.

    :return: - if realizable: dictionary: controllable_variable_bdd -> func_bdd
             - if not: None
    """
    global rel_det_start_wall, rel_det_start_cpu, transition_bdd, init_state_bdd, not_error_bdd, win_region, strategy, func_by_var

    #: :type: DdNode
    transition_bdd = compose_transition_bdd()

    #: :type: DdNode
    init_state_bdd = compose_init_state_bdd()

    #: :type: DdNode
    not_error_bdd = ~get_bdd_for_value(error_fake_latch.lit)
    win_region = calc_win_region(init_state_bdd, transition_bdd, not_error_bdd)

    if win_region == cudd.Zero():
        return None

    rel_det_start_wall = time.time()
    rel_det_start_cpu = time.clock()

    strategy = get_nondet_strategy(win_region, transition_bdd)
    
    func_by_var = extract_output_funcs(strategy)

    return func_by_var


def optimizing_and(conjunct1, conjunct2):
    if conjunct1 == 0 or conjunct2 == 0:
        return 0

    if conjunct1 == conjunct2 == 1:
        return 1

    if conjunct1 == 1:
        return conjunct2

    if conjunct2 == 1:
        return conjunct1

    fresh_var = next_lit() 
    aiger_add_and(spec, fresh_var, conjunct1, conjunct2)
    return fresh_var


def walk(a_bdd):
    """
    Walk given BDD node (recursively).
    If given input BDD requires intermediate AND gates for its representation, the function adds them.
    Literal representing given input BDD is `not` added to the spec.

    :returns: literal representing input BDD
    :hint: - current literal of BDD can be accessed with: ``node.NodeReadIndex()``
           - 'then-node': ``node.T()``
           - 'else-node': ``node.E()``
    :hint: to check if node is constant: ``node.IsConstant()``
    :warning: variables in cudd nodes may be complemented, check with: ``node.IsComplement()``
    """

    # handle constants 0 and 1
    if a_bdd.IsConstant():
        return 1 - a_bdd.IsComplement()

    # query cache
    if a_bdd in bdd_to_lit:
        return bdd_to_lit[a_bdd]

    not_a_bdd = ~a_bdd
    if not_a_bdd in bdd_to_lit:
        return negated(bdd_to_lit[not_a_bdd])

    current_node_lit = a_bdd.NodeReadIndex()
    result = optimizing_and(negated(optimizing_and(current_node_lit, walk(a_bdd.T()))), negated(optimizing_and(negated(current_node_lit), walk(a_bdd.E()))))

    if not a_bdd.IsComplement():
        result = negated(result)

    # cache result
    lit_to_bdd[result] = a_bdd
    bdd_to_lit[a_bdd] = result
    lit_to_bdd[negated(result)] = not_a_bdd
    bdd_to_lit[not_a_bdd] = negated(result)
    
    return result


def model_to_aiger(c_bdd, func_bdd):
    """ Update aiger spec with a definition of ``c_bdd``

    :hint: you will need to translate BDD into and-not gates, this is done in stub function ``walk``
    """
    #: :type: DdNode
    c_bdd = c_bdd
    c_lit = c_bdd.NodeReadIndex()

    func_as_aiger_lit = walk(func_bdd)

    aiger_redefine_input_as_and(spec, c_lit, func_as_aiger_lit, func_as_aiger_lit)


def main(aiger_file_name, out_file_name):
    """ Open aiger file, synthesize the circuit and write the result to output file.

    :returns: boolean value 'is realizable?'
    """
    global rel_det_start_wall, rel_det_start_cpu, bdd_to_lit
    init_cudd()

    parse_into_spec(aiger_file_name)

    func_by_var = synthesize()

    if func_by_var:
        #BEGIN added by RK
        # The rules for the synthesis competition do not allow to reuse
        # nodes of the transition relation. Hence, we must clear the cache
        # that has been used for parsing the input file.
        bdd_to_lit = dict()
        #END added by RK
        cudd.ReduceHeap(5,0)  # SIFT_CONV
        
        for (c_bdd, func_bdd) in func_by_var.items():
            model_to_aiger(c_bdd, func_bdd)
    
        if out_file_name:
            aiger_open_and_write_to_file(spec, out_file_name)
        else:
            res, string = aiger_write_to_string(spec, aiger_ascii_mode, 65536)
            assert res != 0, 'writing failure'
            print(string)

        elapsed_wall_time = time.time() - rel_det_start_wall
        elapsed_cpu_time = time.clock() - rel_det_start_cpu
        print "Time for relation determinization: %.2f (%.2f)" % (elapsed_cpu_time, elapsed_wall_time)

        return True

    return False

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Aiger Format Based Simple Synthesizer')
    parser.add_argument('aiger', metavar='aiger', type=str, help='input specification in AIGER format')
    parser.add_argument('--out', '-o', metavar='out', type=str, required=False, default=None,
                        help='output file in AIGER format (if realizable)')

    args = parser.parse_args()

    is_realizable = main(args.aiger, args.out)
    exit([EXIT_STATUS_UNREALIZABLE, EXIT_STATUS_REALIZABLE][is_realizable])

