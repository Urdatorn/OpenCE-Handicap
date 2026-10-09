"""The real multiplayer handicaps of handicap.c with the real damage multiplier of game_engine.c: the damaged player's
handicap divides the damage they take, only on the host, by a name matched in either case and without marks, read
again as each game starts."""

import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, constant, function, mutated, read, run  # noqa: E402

CASES = ["damaged-player-divides", "with-gametype-health", "client-uses-none", "names-match", "values-checked",
         "read-again", "console-multiples", "console-command", "console-player", "name-written", "menus-no-players",
         "console-sets", "client-console"]
FUNCTIONS = ("handicap_lower", "handicap_trim", "handicap_fold_key", "handicap_player_name",
             "handicap_player_name_utf8", "handicap_name_begins_with", "handicap_entry_named", "handicap_finite",
             "handicap_pin", "handicap_add_entry", "handicap_load", "handicap_of_player",
             "handicap_damage_multiplier", "handicap_game_starting", "handicap_player_named",
             "handicap_parse_multiple", "handicap_parse_command", "handicap_matching_player_names", "handicap_list",
             "handicap_set", "handicap_command")

# a fault, the file and function it is in, and the case that must catch it
NEGATIVE_CONTROLS = {
    "no-handicap": (("\tresult *= handicap_damage_multiplier(damaged_player_index);\n", ""),
                    "game_engine_get_damage_multiplier", "damaged-player-divides"),
    "attackers-handicap": (("handicap_damage_multiplier(damaged_player_index)",
                            "handicap_damage_multiplier(damaging_player_index)"),
                           "game_engine_get_damage_multiplier", "damaged-player-divides"),
    "handicap-multiplies": (("return 1.0f / handicap_of_player(player_index);",
                             "return handicap_of_player(player_index);"),
                            "handicap_damage_multiplier", "damaged-player-divides"),
    "client-uses-its-own": (("if (player_index == NONE || network_game_distributed_client())",
                             "if (player_index == NONE)"), "handicap_damage_multiplier", "client-uses-none"),
    "case-sensitive": (("handicap_lower(player_name_character_ascii((wchar_t)code))",
                        "player_name_character_ascii((wchar_t)code)"), "handicap_fold_key", "names-match"),
    "spaces-kept": (("\thandicap_trim(folded);\n", ""), "handicap_fold_key", "names-match"),
    "unpinned": (("return PIN((real)value, MINIMUM_HANDICAP, MAXIMUM_HANDICAP);", "return (real)value;"),
                 "handicap_pin", "values-checked"),
    "last-name-counts": (("if (handicap_entry_named(folded) != NONE)", "if (0)"), "handicap_add_entry",
                         "values-checked"),
    "kept-after-reading": (("cached->generation == handicap_generation", "1"), "handicap_of_player",
                           "read-again"),
    "slot-not-player": (("cached->player_index == player_index", "1"), "handicap_of_player", "read-again"),
    "percent-as-is": (("\t\tresult /= 100.0;\n", ""), "handicap_parse_multiple", "console-multiples"),
    "infinite-taken": (("!handicap_finite(result) || ", ""), "handicap_parse_multiple", "console-multiples"),
    "multiple-in-name": (("if (last_space && handicap_parse_multiple(last_space + 1, &value))", "if (0)"),
                         "handicap_parse_command", "console-command"),
    "completion-in-menus": (("\tif (!handicap_players_valid())\n\t\treturn 0;\n", ""),
                            "handicap_matching_player_names", "menus-no-players"),
    "named-in-menus": (("if (!handicap_players_valid() || !text[0])", "if (!text[0])"), "handicap_player_named",
                       "menus-no-players"),
    "listed-in-menus": (("if (handicap_players_valid())", "if (1)"), "handicap_list", "menus-no-players"),
    "set-not-at-once": (("\thandicap_generation++;\n\tif (config_table_write_number(", "\tif (config_table_write_number("),
                        "handicap_set", "console-sets"),
    "spelling-replaced": (("old_key = entry->key;", "old_key = NULL;"), "handicap_set", "console-sets"),
    "client-sets": (("\tif (network_game_distributed_client())\n\t{\n\t\tconsole_warning(", "\tif (0)\n\t{\n\t\tconsole_warning("),
                    "handicap_command", "client-console"),
    "prefix-over-whole": (("if (exact == 1)", "if (exact == -1)"), "handicap_player_named", "console-player"),
    "surrogates-split": (("code = 0x10000 + ((code - 0xD800) << 10) + ((unsigned short)player->name[index + 1] - 0xDC00);\n"
                          "\t\t\tindex++;", ";"), "handicap_player_name_utf8", "name-written"),
}


def generated(fault=None, faulty_function=None):
    handicap = read("port/linux/game/handicap.c")
    header = read("port/linux/game/handicap.h")
    game_engine = read("source/game/game_engine.c")
    match = re.search(r"/\* ---------- constants \*/\n(.*?)/\* ---------- names \*/", handicap, re.S)
    assert match, "handicap.c's constants, structures and globals not found"
    config = f"#define HANDICAP_NAME_TEXT_SIZE {constant(header, 'HANDICAP_NAME_TEXT_SIZE')}\n" + match.group(1)
    text = function(read("source/game/players.c"), "player_name_character_ascii") + "\n"
    for name in FUNCTIONS + ("game_engine_get_damage_multiplier",):
        code = function(game_engine if name == "game_engine_get_damage_multiplier" else handicap, name)
        if fault and name == faulty_function:
            code = mutated(code, *fault)
        text += code + "\n"
    return (("config.inc", config), ("under_test.inc", text))


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("handicap", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, faulty_function, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("handicap", generated(fault, faulty_function)), case)
    assert status == CHECK_FAILED, f"handicap.c with a fault ({control}) passed '{case}': the test cannot see it"
