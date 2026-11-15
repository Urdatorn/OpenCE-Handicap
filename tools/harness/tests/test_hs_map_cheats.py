"""A map's scripts may turn the cheats off (a constant false), never on."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, enum_with, function, mutated, read, run, structure

CASES = ['cheat-off', 'cheat-on', 'cheat-from-global', 'cheat-from-call', 'other-global']
CONTROLS = {
    'any-value': ('!value->boolean_value', 'TRUE', 'cheat-on'),
    'any-global': ('!csstrncmp(name, "cheat_", 6)', 'TRUE', 'other-global'),
    'variable-value': ('!TEST_FLAG(value->flags, _hs_syntax_node_variable_bit) &&', '', 'cheat-from-global'),
    'call-value': ('TEST_FLAG(value->flags, _hs_syntax_node_primitive_bit) &&', '', 'cheat-from-call'),
}


def generated(fault=None):
    code = function(read('source/hs/hs.c'), 'hs_set_turns_cheat_off')
    if fault:
        before, after, _ = CONTROLS[fault]
        code = mutated(code, before, after)
    types = structure(read('source/hs/hs_scenario_definitions.h'), 'hs_syntax_node') + ';\n'
    types += enum_with(read('source/hs/hs_compile.c'), '_hs_syntax_node_variable_bit') + ';\n'
    types += enum_with(read('source/hs/hs.h'), '_hs_type_boolean') + ';\n'
    return (('types.inc', types), ('under_test.inc', code))


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('hs_map_cheats', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('fault', sorted(CONTROLS))
def test_negative_control(fault):
    status, output = run(build('hs_map_cheats', generated(fault)), CONTROLS[fault][2])
    assert status == CHECK_FAILED, output
