/*
HANDICAP.C (test)

The real multiplayer handicaps of port/linux/game/handicap.c, with the real
damage multiplier of source/game/game_engine.c that divides the damage a
player takes by their handicap: which player's handicap counts (the damaged
one's, the host's), how names match, how the table is read again, and what
the console takes. test_handicap.py takes the constants, structures and
globals (config.inc), player_name_character_ascii (players.c) and the code
(under_test.inc).
*/

#include "harness.h"

#include <math.h>
#include <stdarg.h>
#include <wchar.h>

#define csstrlen(text) ((long)strlen(text))
#define csmemmove memmove
#define csstrcmp strcmp
#define csstrcpy strcpy
#define csstrncpy strncpy
#define NUMBEROF(array) ((long)(sizeof(array) / sizeof((array)[0])))
#define PIN(value, low, high) ((value) < (low) ? (low) : ((value) > (high) ? (high) : (value)))

/* ---------- the fake world */

/* the players (game/players.h): what the handicaps read of them */
struct player_datum
{
	short identifier;
	short local_player_index;
	wchar_t name[12];
	boolean quit_out_of_game;
};

struct data_iterator
{
	struct data_array *data;
	short absolute_index;
	long datum_index;
};

static struct data_array *player_data;
/* the players' data is valid (a map is loaded); in the menus it is not, and
reading it halts the game (data.c's data->valid) */
static boolean players_valid = TRUE;

static boolean handicap_players_valid(void)
{
	return player_data != NULL && players_valid;
}

static void *datum_try_and_get(struct data_array *array, long datum_index)
{
	struct datum_header *header;

	CHECK(players_valid, "the players read in the menus");
	if (datum_index == NONE || DATUM_INDEX_TO_ABSOLUTE_INDEX(datum_index) >= array->maximum_count)
		return NULL;
	header = datum_get(array, datum_index);
	return header->identifier && (unsigned short)header->identifier == (unsigned long)datum_index >> 16 ? header : NULL;
}

static void data_iterator_new(struct data_iterator *iterator, struct data_array *array)
{
	CHECK(players_valid, "the players read in the menus");
	iterator->data = array;
	iterator->absolute_index = 0;
	iterator->datum_index = NONE;
}

static void *data_iterator_next(struct data_iterator *iterator)
{
	while (iterator->absolute_index < iterator->data->maximum_count)
	{
		struct datum_header *header = (struct datum_header *)(iterator->data->data +
			iterator->absolute_index * iterator->data->size);

		iterator->absolute_index++;
		if (header->identifier)
		{
			iterator->datum_index = ((long)(unsigned short)header->identifier << 16) | (iterator->absolute_index - 1);
			return header;
		}
	}
	return NULL;
}

/* a player of the name (UTF-16 code units, ending in 0) */
static long player_new(unsigned short const *name)
{
	long player_index = datum_new(player_data);
	struct player_datum *player = datum_get(player_data, player_index);
	long index;

	for (index = 0; index < 11 && name[index]; index++)
		player->name[index] = name[index];
	player->local_player_index = NONE;
	return player_index;
}

static long player_new_ascii(char const *name)
{
	unsigned short wide[12] = { 0 };
	long index;

	for (index = 0; index < 11 && name[index]; index++)
		wide[index] = (unsigned char)name[index];
	return player_new(wide);
}

/* the host's config.toml's [handicap]: keys (UTF-8) and their numbers */
static struct { char const *key; double value; } table[16];
static short table_count;
static boolean table_readable = TRUE;
static short table_reads;

static void table_set(char const *key, double value)
{
	table[table_count].key = key;
	table[table_count].value = value;
	table_count++;
}

static int config_table_numbers(char const *section,
	void (*each)(void *context, char const *key, double value, int line), void *context)
{
	short index;

	table_reads++;
	CHECK(!strcmp(section, "handicap"), "read [%s], not [handicap]", section);
	if (!table_readable)
		return -1;
	for (index = 0; index < table_count; index++)
		each(context, table[index].key, table[index].value, index + 1);
	return table_count;
}

static void platform_log(char const *format, ...)
{
	(void)format;
}

/* the console (main/console.h): what it showed, a line each */
static char console_text[4096];

static void console_print(char const *format, va_list arguments)
{
	size_t length = strlen(console_text);

	vsnprintf(console_text + length, sizeof(console_text) - length - 1, format, arguments);
	strcat(console_text, "\n");
}

static void console_printf(boolean clear, char const *format, ...)
{
	va_list arguments;

	(void)clear;
	va_start(arguments, format);
	console_print(format, arguments);
	va_end(arguments);
}

static void console_warning(char const *format, ...)
{
	va_list arguments;

	va_start(arguments, format);
	console_print(format, arguments);
	va_end(arguments);
}

/* the console's writes into config.toml's [handicap] */
static struct { char old_key[64]; boolean has_old_key; char key[64]; double value; } writes[8];
static short write_count;

static int config_table_write_number(char const *section, char const *old_key, char const *key, double value)
{
	CHECK(!strcmp(section, "handicap"), "wrote [%s], not [handicap]", section);
	CHECK(write_count < 8, "too many writes");
	writes[write_count].has_old_key = old_key != NULL;
	snprintf(writes[write_count].old_key, sizeof(writes[write_count].old_key), "%s", old_key ? old_key : "");
	snprintf(writes[write_count].key, sizeof(writes[write_count].key), "%s", key);
	writes[write_count].value = value;
	write_count++;
	return 1;
}

/* this machine is a client of the distributed netcode */
static boolean distributed_client;

static boolean network_game_distributed_client(void)
{
	return distributed_client;
}

/* the gametype (game_engine.c): its health, and its traits (none here) */
static int game_engine_running_flag = 1;
static void *game_engine = &game_engine_running_flag;
static struct { struct { real health; } universal_variant; } global_variant = { { 1.0f } };

static boolean game_engine_test_trait(long player_index, long trait)
{
	(void)player_index;
	(void)trait;
	return FALSE;
}

#include "config.inc"
#include "under_test.inc"

/* ---------- the cases */

/* text with its bytes past ASCII as \xHH (a wrong name is no UTF-8) */
static char const *escaped(char const *text)
{
	static char result[256];
	long length = 0;

	for (; *text && length < (long)sizeof(result) - 5; text++)
	{
		if ((unsigned char)*text < 0x80)
			result[length++] = *text;
		else
			length += snprintf(result + length, sizeof(result) - (size_t)length, "\\x%02X", (unsigned char)*text);
	}
	result[length] = 0;
	return result;
}

static boolean near(real a, real b)
{
	return fabsf(a - b) < 0.0001f;
}

int main(int argc, char **argv)
{
	char const *case_name = argc > 1 ? argv[1] : "";

	player_data = game_state_data_new("players", 16, sizeof(struct player_datum));

	/* the damaged player's handicap divides the damage they take; the
	attacker's changes nothing of what they deal */
	CASE("damaged-player-divides")
	{
		long novice = player_new_ascii("Novice");
		long ace = player_new_ascii("Ace");
		real multiplier;

		table_set("Novice", 2.0);
		table_set("Ace", 0.5);
		handicap_game_starting();
		multiplier = game_engine_get_damage_multiplier(ace, novice);
		CHECK(near(multiplier, 0.5f), "the ace hits the novice x%g, not x0.5", multiplier);
		multiplier = game_engine_get_damage_multiplier(novice, ace);
		CHECK(near(multiplier, 2.0f), "the novice hits the ace x%g, not x2", multiplier);
		multiplier = game_engine_get_damage_multiplier(NONE, novice);
		CHECK(near(multiplier, 0.5f), "the world hits the novice x%g, not x0.5", multiplier);
		multiplier = game_engine_get_damage_multiplier(novice, NONE);
		CHECK(near(multiplier, 1.0f), "the novice hits no player's object x%g, not x1", multiplier);
		return 0;
	}
	/* a handicap multiplies the gametype's health */
	CASE("with-gametype-health")
	{
		long novice = player_new_ascii("Novice");
		long other = player_new_ascii("Other");
		real multiplier;

		global_variant.universal_variant.health = 2.0f;
		table_set("Novice", 2.0);
		handicap_game_starting();
		multiplier = game_engine_get_damage_multiplier(other, novice);
		CHECK(near(multiplier, 0.25f), "x%g, not x0.25 (health x2, handicap x2)", multiplier);
		multiplier = game_engine_get_damage_multiplier(novice, other);
		CHECK(near(multiplier, 0.5f), "x%g, not x0.5 (health x2, no handicap)", multiplier);
		return 0;
	}
	/* a client deals no damage (the host's is dealt): its own handicaps are
	never used, nor read */
	CASE("client-uses-none")
	{
		long novice = player_new_ascii("Novice");
		real multiplier;

		table_set("Novice", 2.0);
		distributed_client = TRUE;
		handicap_game_starting();
		multiplier = game_engine_get_damage_multiplier(NONE, novice);
		CHECK(near(multiplier, 1.0f), "a client's damage x%g, not x1", multiplier);
		CHECK(table_reads == 0, "a client read config.toml %d times", table_reads);
		return 0;
	}
	/* a name matches in either case, a letter with a mark as its plain
	letter, with spaces in it; another name does not */
	CASE("names-match")
	{
		static unsigned short const bjorn[] = { 'B', 'j', 0xF6, 'r', 'n', 0 };
		long player_bjorn = player_new(bjorn);
		long player_one = player_new_ascii("Player One");
		long player_ace = player_new_ascii("ACE");
		long player_aces = player_new_ascii("Aces");

		table_set("bj\xC3\xB6rn", 3.0);
		table_set(" player one ", 1.5);
		table_set("ace", 0.75);
		handicap_game_starting();
		CHECK(near(handicap_damage_multiplier(player_bjorn), 1.0f / 3.0f), "Bjorn x%g",
			1.0f / handicap_damage_multiplier(player_bjorn));
		CHECK(near(handicap_damage_multiplier(player_one), 1.0f / 1.5f), "Player One x%g",
			1.0f / handicap_damage_multiplier(player_one));
		CHECK(near(handicap_damage_multiplier(player_ace), 1.0f / 0.75f), "ACE x%g",
			1.0f / handicap_damage_multiplier(player_ace));
		CHECK(near(handicap_damage_multiplier(player_aces), 1.0f), "Aces x%g (only Ace has one)",
			1.0f / handicap_damage_multiplier(player_aces));
		return 0;
	}
	/* values out of range are pinned; ones that are no multiple, unnamed
	ones and a name given twice (the first counts) are left out */
	CASE("values-checked")
	{
		long big = player_new_ascii("Big");
		long small = player_new_ascii("Small");
		long zero = player_new_ascii("Zero");
		long twice = player_new_ascii("Twice");

		table_set("Big", 10.0);
		table_set("Small", 0.1);
		table_set("Zero", 0.0);
		table_set("Twice", 2.0);
		table_set("TWICE", 3.0);
		table_set("  ", 2.0);
		table_set("Nan", NAN);
		handicap_game_starting();
		CHECK(near(1.0f / handicap_damage_multiplier(big), 4.0f), "10 is x%g, not x4",
			1.0f / handicap_damage_multiplier(big));
		CHECK(near(1.0f / handicap_damage_multiplier(small), 0.25f), "0.1 is x%g, not x0.25",
			1.0f / handicap_damage_multiplier(small));
		CHECK(near(handicap_damage_multiplier(zero), 1.0f), "0 is x%g, not none",
			1.0f / handicap_damage_multiplier(zero));
		CHECK(near(1.0f / handicap_damage_multiplier(twice), 2.0f), "a name given twice is x%g, not the first's x2",
			1.0f / handicap_damage_multiplier(twice));
		CHECK(handicap_entry_count == 3, "%d names kept, not 3", handicap_entry_count);
		return 0;
	}
	/* the table is read again as each game starts, and a player's handicap
	is looked up again then (and for a new player in the slot) */
	CASE("read-again")
	{
		long novice = player_new_ascii("Novice");
		long newcomer;

		table_set("Novice", 2.0);
		table_set("Newcomer", 4.0);
		handicap_game_starting();
		CHECK(near(1.0f / handicap_damage_multiplier(novice), 2.0f), "x%g, not x2",
			1.0f / handicap_damage_multiplier(novice));
		table[0].value = 3.0;
		handicap_game_starting();
		CHECK(near(1.0f / handicap_damage_multiplier(novice), 3.0f), "after the file changed x%g, not x3",
			1.0f / handicap_damage_multiplier(novice));
		datum_delete(player_data, novice);
		newcomer = player_new_ascii("Newcomer");
		CHECK(DATUM_INDEX_TO_ABSOLUTE_INDEX(newcomer) == DATUM_INDEX_TO_ABSOLUTE_INDEX(novice), "not the same slot");
		CHECK(near(1.0f / handicap_damage_multiplier(newcomer), 4.0f), "the slot's new player x%g, not x4",
			1.0f / handicap_damage_multiplier(newcomer));
		table_readable = FALSE;
		handicap_game_starting();
		CHECK(near(handicap_damage_multiplier(newcomer), 1.0f), "with no file x%g, not none",
			1.0f / handicap_damage_multiplier(newcomer));
		return 0;
	}
	/* the console's multiples */
	CASE("console-multiples")
	{
		static struct { char const *text; double value; } const valid[] =
		{
			{ "2", 2.0 }, { "1.5", 1.5 }, { "x2", 2.0 }, { "2x", 2.0 }, { "150%", 1.5 }, { ".5", 0.5 },
		};
		static char const *const invalid[] = { "", "0", "-1", "abc", "2y", "x", "%", "nan", "inf", "1e999" };
		unsigned index;

		for (index = 0; index < sizeof(valid) / sizeof(valid[0]); index++)
		{
			double value = 0.0;

			CHECK(handicap_parse_multiple(valid[index].text, &value), "\"%s\" refused", valid[index].text);
			CHECK(fabs(value - valid[index].value) < 1e-9, "\"%s\" is %g, not %g", valid[index].text, value,
				valid[index].value);
		}
		for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index++)
		{
			double value = 0.0;

			CHECK(!handicap_parse_multiple(invalid[index], &value), "\"%s\" taken as %g", invalid[index], value);
		}
		return 0;
	}
	/* what the console's text names: a name (with spaces, or quoted), and
	the multiple after it */
	CASE("console-command")
	{
		static struct { char const *text, *name, *multiple; } const commands[] =
		{
			{ "", "", "" },
			{ " ", "", "" },
			{ " Novice 2", "Novice", "2" },
			{ " Novice", "Novice", "" },
			{ " Player One 1.5", "Player One", "1.5" },
			{ " Player One", "Player One", "" },
			{ " \"Player One\" 150%)", "Player One", "150%" },
			{ " \"Player One\")", "Player One", "" },
			{ " Nov x2 )", "Nov", "x2" },
		};
		unsigned index;

		for (index = 0; index < sizeof(commands) / sizeof(commands[0]); index++)
		{
			char name[64];
			char multiple[32];

			handicap_parse_command(commands[index].text, name, sizeof(name), multiple, sizeof(multiple));
			CHECK(!strcmp(name, commands[index].name), "\"%s\" names \"%s\", not \"%s\"", commands[index].text, name,
				commands[index].name);
			CHECK(!strcmp(multiple, commands[index].multiple), "\"%s\" gives \"%s\", not \"%s\"", commands[index].text,
				multiple, commands[index].multiple);
		}
		return 0;
	}
	/* the console's player: the whole name, else the one it begins */
	CASE("console-player")
	{
		long player_ace = player_new_ascii("Ace");
		long player_aces = player_new_ascii("Aces");
		long player_novice = player_new_ascii("Novice");
		short matches;

		CHECK(handicap_player_named("ace", &matches) == player_ace, "\"ace\" is not Ace (the whole name)");
		CHECK(handicap_player_named("aceS", &matches) == player_aces, "\"aceS\" is not Aces");
		CHECK(handicap_player_named("nov", &matches) == player_novice, "\"nov\" is not Novice");
		CHECK(handicap_player_named("a", &matches) == NONE && matches == 2, "\"a\" names one of two");
		CHECK(handicap_player_named("zed", &matches) == NONE && matches == 0, "\"zed\" names someone");
		return 0;
	}
	/* a name in UTF-8 as the console writes it into the file, read back as
	the same player's */
	CASE("name-written")
	{
		static unsigned short const name[] = { 'B', 'j', 0xF6, 'r', 'n', ' ', 0x263A, 0xD83D, 0xDE00, 0 };
		long player_index = player_new(name);
		struct player_datum *player = datum_get(player_data, player_index);
		char key[64];

		handicap_player_name_utf8(player, key, sizeof(key));
		CHECK(!strcmp(key, "Bj\xC3\xB6rn \xE2\x98\xBA\xF0\x9F\x98\x80"), "written as \"%s\"", escaped(key));
		table_set(key, 2.0);
		handicap_game_starting();
		CHECK(near(1.0f / handicap_damage_multiplier(player_index), 2.0f), "read back x%g, not x2",
			1.0f / handicap_damage_multiplier(player_index));
		return 0;
	}

	/* in the menus (no map, the players' data not valid) the console's
	command and its completion read no players, and a name is set as typed */
	CASE("menus-no-players")
	{
		char names[4][HANDICAP_NAME_TEXT_SIZE];
		short matches;

		player_new_ascii("Novice");
		table_set("Ace", 0.5);
		players_valid = FALSE;
		CHECK(handicap_matching_player_names("", names, 4) == 0, "names completed in the menus");
		CHECK(handicap_player_named("Nov", &matches) == NONE, "a player found in the menus");
		CHECK(handicap_command(""), "the list refused");
		CHECK(strstr(console_text, "Ace: x0.5"), "the file's names not listed: %s", console_text);
		CHECK(handicap_command(" Novice 2"), "the setting refused: %s", console_text);
		CHECK(write_count == 1 && !strcmp(writes[0].key, "Novice") && writes[0].value == 2.0,
			"not written as typed: %d writes", write_count);
		CHECK(handicap_command(" novice"), "the showing refused: %s", console_text);
		CHECK(strstr(console_text, "Novice: x2 (not in the game)"), "not shown: %s", console_text);
		return 0;
	}
	/* in a game: the command names a player by the start of their name, sets
	their handicap at once and writes it under the file's own key */
	CASE("console-sets")
	{
		long novice = player_new_ascii("Novice");

		table_set("NOVICE", 1.5);
		handicap_game_starting();
		CHECK(near(1.0f / handicap_damage_multiplier(novice), 1.5f), "x%g, not x1.5",
			1.0f / handicap_damage_multiplier(novice));
		CHECK(handicap_command(" nov 3"), "refused: %s", console_text);
		CHECK(near(1.0f / handicap_damage_multiplier(novice), 3.0f), "x%g after the command, not x3",
			1.0f / handicap_damage_multiplier(novice));
		CHECK(write_count == 1 && writes[0].has_old_key && !strcmp(writes[0].old_key, "NOVICE") &&
			!strcmp(writes[0].key, "NOVICE") && writes[0].value == 3.0, "written as %s (old %s) = %g",
			writes[0].key, writes[0].old_key, writes[0].value);
		CHECK(handicap_command(" \"Ace\" 50%"), "refused: %s", console_text);
		CHECK(write_count == 2 && !writes[1].has_old_key && !strcmp(writes[1].key, "Ace") && writes[1].value == 0.5,
			"a new name written as %s = %g", writes[1].key, writes[1].value);
		CHECK(!handicap_command(" nov two"), "\"two\" taken");
		CHECK(handicap_command(" nov 9") && near(1.0f / handicap_damage_multiplier(novice), 4.0f), "9 is x%g, not x4",
			1.0f / handicap_damage_multiplier(novice));
		return 0;
	}
	/* a client's command changes nothing: the host's handicaps count */
	CASE("client-console")
	{
		player_new_ascii("Novice");
		distributed_client = TRUE;
		CHECK(!handicap_command(" Novice 2"), "a client's command taken");
		CHECK(write_count == 0, "a client wrote config.toml");
		return 0;
	}

	fprintf(stderr, "no case \"%s\"\n", case_name);
	return 2;
}
