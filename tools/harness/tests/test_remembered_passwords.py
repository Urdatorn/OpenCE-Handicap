"""The server browser's remembered passwords: the file's lines, and the newest kept first."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run, structure

CASES = ['parse', 'parse-upper', 'parse-short', 'parse-no-space', 'parse-bad-digit',
         'add', 'replace', 'forget', 'full']
CONTROLS = {
    'no-space-needed': ("line[2 * P2P_LISTING_SIGNING_KEY_SIZE] == ' ' &&", '', 'parse-no-space'),
    'upper-refused': ("if (digit >= 'A' && digit <= 'F')\n\t\treturn digit - 'A' + 10;", '', 'parse-upper'),
    'duplicates': ('if (!memcmp(entries[index].host, host, P2P_LISTING_SIGNING_KEY_SIZE))\n\t\t\tcontinue;', '',
                   'replace'),
    'forget-kept': ('if (!key)\n\t\treturn kept;', 'if (!key)\n\t\treturn count;', 'forget'),
    'overflow': ('if (kept == MAXIMUM_REMEMBERED_PASSWORDS)\n\t\tkept--;', '', 'full'),
}


def generated(fault=None):
    source = read('port/linux/src/p2p_lobby.c')
    code = '\n'.join(function(source, name) for name in (
        'remembered_hex_digit', 'remembered_hex_bytes', 'remembered_password_parse', 'remembered_passwords_update'))
    if fault:
        before, after, _ = CONTROLS[fault]
        code = mutated(code, before, after)
    types = structure(source, 'remembered_password') + ';\n'
    return (('types.inc', types), ('under_test.inc', code))


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('remembered_passwords', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('fault', sorted(CONTROLS))
def test_negative_control(fault):
    status, output = run(build('remembered_passwords', generated(fault)), CONTROLS[fault][2])
    assert status == CHECK_FAILED, output
