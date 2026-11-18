"""server.chat_games: a chat line "!word" gives that word's rotation entry."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run

CASES = ['found', 'either-case', 'trailing-spaces', 'more-words', 'other-word', 'prefix', 'longer-word',
         'second-pair', 'semicolons', 'rule-equals', 'trimmed', 'too-long', 'no-bang', 'bare-bang', 'empty',
         'no-entry']
CONTROLS = {
    'more-words-played': ('if (!word_length || *after)', 'if (!word_length)', 'more-words'),
    'prefix-played': ('key_end - start != word_length', 'key_end - start < word_length', 'prefix'),
    'spaced-word': ('while (key_end > start && chat_games_space(key_end[-1]))\n\t\t\t\tkey_end--;', '', 'found'),
    'case-kept': ('chat_games_lower(start[index]) != chat_games_lower(word[index])',
                  'start[index] != word[index]', 'either-case'),
    'last-equals': ('equals = start + index;\n\t\t\t\tbreak;', 'equals = start + index;', 'rule-equals'),
    'first-pair-only': ('continue;\n\t\t}\n\t\tfor', 'return FALSE;\n\t\t}\n\t\tfor', 'second-pair'),
    'overflow': ('end - start >= entry_size', 'end - start > entry_size', 'too-long'),
}


def generated(fault=None):
    source = read('port/linux/game/server_chat_games.c')
    code = '\n'.join(function(source, name) for name in
                     ('chat_games_lower', 'chat_games_space', 'server_chat_game_find'))
    if fault:
        before, after, _ = CONTROLS[fault]
        code = mutated(code, before, after)
    return (('under_test.inc', code),)


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('server_chat_games', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('fault', sorted(CONTROLS))
def test_negative_control(fault):
    status, output = run(build('server_chat_games', generated(fault)), CONTROLS[fault][2])
    assert status == CHECK_FAILED, output
