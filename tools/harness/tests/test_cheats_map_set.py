"""A cheat a map's script sets ends with the map; one set from the console stays."""
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run, structure

CASES = ['map-on', 'map-off', 'console-kept', 'console-after-map', 'first-before', 'next-map', 'not-a-cheat']
CONTROLS = {
    'never-undone': ('values[index] = cheats_before_map[index];', 'values[index] = values[index];', 'map-on'),
    'console-undone': ('cheats_set_by_map[index] = FALSE;', '', 'console-after-map'),
    'last-before': ('else if (!cheats_set_by_map[index])', 'else', 'first-before'),
    'kept-marks': ('csmemset(cheats_set_by_map, 0, sizeof(cheats_set_by_map));', '', 'next-map'),
    'any-address': ('(boolean const *)address < values || (boolean const *)address >= values + CHEAT_COUNT',
                    'FALSE', 'not-a-cheat'),
}


def generated(fault=None):
    source = read('source/game/cheats.c')
    code = '\n'.join(function(source, name) for name in ('cheats_dispose_from_old_map', 'cheats_note_set'))
    if fault:
        before, after, _ = CONTROLS[fault]
        code = mutated(code, before, after)
    types = structure(read('source/game/cheats.h'), 'cheat_globals') + ';\n'
    types += re.search(r'#define CHEAT_COUNT .*', source).group(0) + '\n'
    return (('types.inc', types), ('under_test.inc', code))


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('cheats_map_set', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('fault', sorted(CONTROLS))
def test_negative_control(fault):
    status, output = run(build('cheats_map_set', generated(fault)), CONTROLS[fault][2])
    assert status == CHECK_FAILED, output
