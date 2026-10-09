"""The real rule of network_server_manager.c for when the lobby's countdown may run (and START NOW start the game):
the host's machine alone in a system link or internet game, with any number of split screen players on any teams,
may start it; once another machine joins, both teams need players; offline split screen keeps the Xbox's rules."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run  # noqa: E402

CASES = ["host-split-screen-one-team", "host-split-screen-free-for-all", "host-alone", "friend-joined-one-team",
         "friend-joined-no-players", "offline-split-screen-one-team"]
FUNCTIONS = ("network_game_server_get_client_machine_count", "server_needs_more_teams",
             "server_has_a_player_on_each_machine", "server_has_enough_machines", "server_alone",
             "server_ok_to_countdown")

# a fault, the function it is in, and the case that must catch it
NEGATIVE_CONTROLS = {
    "one-player-only": (("(server->game.player_count > 1 &&", "(0 &&"),
                        "server_alone", "host-split-screen-one-team"),
    "offline-too": (("network_game_should_accept_remote_connections() &&", "1 &&"), "server_alone",
                    "offline-split-screen-one-team"),
    "friends-too": (("network_game_server_get_client_machine_count(server) == 1", "1"), "server_alone",
                    "friend-joined-one-team"),
}


def generated(fault=None, faulty_function=None):
    source = read("source/networking/network_server_manager.c")
    text = ""
    for name in FUNCTIONS:
        code = function(source, name)
        if fault and name == faulty_function:
            code = mutated(code, *fault)
        text += code + "\n"
    return (("under_test.inc", text),)


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("server_countdown", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, faulty_function, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("server_countdown", generated(fault, faulty_function)), case)
    assert status == CHECK_FAILED, f"network_server_manager.c with a fault ({control}) passed '{case}'"
