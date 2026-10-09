/*
SERVER_COUNTDOWN.C (test)

The real rule of source/networking/network_server_manager.c for when the
lobby's countdown may run (and START NOW start the game): the host's
machine with no other machine in its system link or internet game yet may
start it, its split screen players on whichever teams; once another machine
joins, the Xbox's rules (both teams have players, two players at least);
offline split screen keeps them. test_server_countdown.py takes the code
(under_test.inc).
*/

#include "harness.h"

#define MAXIMUM_NETWORK_PLAYER_COUNT 16
#define MAXIMUM_NETWORK_MACHINE_COUNT 8
#define NUMBER_OF_MULTIPLAYER_TEAMS 2
#define _network_client_machine_validated_bit 0

struct network_player { boolean valid; char team_index; char machine_index; };
struct network_game_server_client_machine { void *connection; unsigned long flags; long machine_index; };
struct network_game_server
{
	struct
	{
		struct { struct { boolean teams; } universal_variant; } variant;
		struct network_player players[MAXIMUM_NETWORK_PLAYER_COUNT];
		short player_count;
		char minimum_players;
	} game;
	struct network_game_server_client_machine client_machines[MAXIMUM_NETWORK_MACHINE_COUNT];
};

static boolean accept_remote_connections = TRUE;

static boolean network_player_is_valid(struct network_player *player)
{
	return player->valid;
}

static boolean network_game_should_accept_remote_connections(void)
{
	return accept_remote_connections;
}

static boolean network_game_server_client_machine_is_joined_to_game(struct network_game_server *server,
	struct network_game_server_client_machine *machine)
{
	(void)server;
	return TEST_FLAG(machine->flags, _network_client_machine_validated_bit);
}

/* (the game's code indexes by a char team index) */
#pragma clang diagnostic ignored "-Wchar-subscripts"
#include "under_test.inc"

static struct network_game_server server;
static int connection;

static void machine(long index)
{
	server.client_machines[index].connection = &connection;
	server.client_machines[index].flags = FLAG(_network_client_machine_validated_bit);
	server.client_machines[index].machine_index = index;
}

static void player(char machine_index, char team_index)
{
	struct network_player *added = &server.game.players[server.game.player_count++];

	added->valid = TRUE;
	added->machine_index = machine_index;
	added->team_index = team_index;
}

int main(int argc, char **argv)
{
	char const *case_name = argc > 1 ? argv[1] : "";
	long index;

	for (index = 0; index < MAXIMUM_NETWORK_MACHINE_COUNT; index++)
		server.client_machines[index].machine_index = NONE;
	server.game.minimum_players = 2;
	server.game.variant.universal_variant.teams = TRUE;
	machine(0);

	/* the host and their split screen player, on one team of a team game,
	waiting for friends to join in progress */
	CASE("host-split-screen-one-team")
	{
		player(0, 0);
		player(0, 0);
		CHECK(server_ok_to_countdown(&server), "the host's split screen players cannot start");
		return 0;
	}
	CASE("host-split-screen-free-for-all")
	{
		server.game.variant.universal_variant.teams = FALSE;
		player(0, 0);
		player(0, 1);
		CHECK(server_ok_to_countdown(&server), "a free-for-all of the host's players cannot start");
		return 0;
	}
	CASE("host-alone")
	{
		player(0, 0);
		CHECK(server_ok_to_countdown(&server), "the host alone cannot start");
		return 0;
	}
	/* once a friend's machine joins: both teams have players */
	CASE("friend-joined-one-team")
	{
		machine(1);
		player(0, 0);
		player(0, 0);
		player(1, 0);
		CHECK(!server_ok_to_countdown(&server), "one team of two machines starts a team game");
		server.game.players[2].team_index = 1;
		CHECK(server_ok_to_countdown(&server), "two teams of two machines cannot start");
		return 0;
	}
	/* a machine that joined adds its players before the countdown */
	CASE("friend-joined-no-players")
	{
		machine(1);
		player(0, 0);
		player(0, 1);
		CHECK(!server_ok_to_countdown(&server), "started without the joined machine's players");
		return 0;
	}
	/* offline split screen: no one joins later, the Xbox's rules */
	CASE("offline-split-screen-one-team")
	{
		accept_remote_connections = FALSE;
		player(0, 0);
		player(0, 0);
		CHECK(!server_ok_to_countdown(&server), "offline split screen starts a team game on one team");
		return 0;
	}

	fprintf(stderr, "no case \"%s\"\n", case_name);
	return 2;
}
