"""Network co-op's Iron: a new death reverts, and the levels it leaves out."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run

CASES = ['listed', 'either-case', 'spaces', 'semicolons', 'second', 'prefix', 'longer', 'empty',
         'new-death', 'no-new-death', 'revert-back', 'death-respawned']
CONTROLS = {
    'any-length': ('if (end - start != level_length)\n\t\t\tcontinue;', '', 'longer'),
    'spaces-kept': ("while (end > start && (end[-1] == ' ' || end[-1] == '\\t'))\n\t\t\tend--;", '', 'spaces'),
    'case-kept': ("(a >= 'A' && a <= 'Z' ? a - 'A' + 'a' : a) != (b >= 'A' && b <= 'Z' ? b - 'A' + 'a' : b)",
                  'a != b', 'either-case'),
    'first-only': ('continue;\n\t\tfor', 'return FALSE;\n\t\tfor', 'second'),
    'deaths-any': ('deaths > deaths_before', 'deaths != deaths_before', 'revert-back'),
    'dead-any': ('&& someone_dead', '', 'death-respawned'),
}


def generated(fault=None):
    code = function(read('port/linux/game/network_coop.c'), 'network_coop_level_listed') + '\n' + \
        function(read('source/game/players.c'), 'players_coop_iron_new_death')
    if fault:
        before, after, _ = CONTROLS[fault]
        code = mutated(code, before, after)
    return (('under_test.inc', code),)


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('coop_iron', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('fault', sorted(CONTROLS))
def test_negative_control(fault):
    status, output = run(build('coop_iron', generated(fault)), CONTROLS[fault][2])
    assert status == CHECK_FAILED, output
