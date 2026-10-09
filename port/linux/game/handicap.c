/*
HANDICAP.C

Multiplayer handicaps, so that players of different skill can play
together: each player's health and shields as a multiple of the gametype's
(its HEALTH option), which the host sets by the player's name, in
config.toml's [handicap] ("Player One" = 2.0) or with the console's
"handicap <name> <multiple>". As the gametype's health is, a handicap is a
divisor of the damage the player takes (game_engine_get_damage_multiplier):
2.0 takes half the damage, to the shields and the body alike, so the HUD's
meters, which show what is left of each, go down half as fast, and the
shields recharge as they always do.

The host alone deals damage (port/linux/NETCODE.md), so only its handicaps
count: nothing is sent, the clients need not have them (nor this build), and
a client's own are not used. In a game on one machine (split screen) that
machine's count.

The table is read again as each game starts, so that the host can edit the
file between games; what the console sets is written into the file as well,
and counts at once.
*/

#include "cseries.h"
#include "memory/data.h"
#include "game/players.h"
#include "main/console.h"
#include "networking/network_game_globals.h"
#include "handicap.h"

#include <stdio.h>
#include <stdlib.h>

/* port/linux/src/port_config.c's */
int config_table_numbers(char const *section,
	void (*each)(void *context, char const *key, double value, int line), void *context);
int config_table_write_number(char const *section, char const *old_key, char const *key, double value);
void platform_log(char const *format, ...);

/* ---------- constants */

#define HANDICAP_SECTION "handicap"
/* as the gametype's health (game_variant_verify) */
#define MINIMUM_HANDICAP 0.25f
#define MAXIMUM_HANDICAP 4.0f

enum
{
	/* the names config.toml's [handicap] can give */
	MAXIMUM_HANDICAP_ENTRIES = 256,
	/* a name's key as the file has it (UTF-8), with its end */
	HANDICAP_KEY_SIZE = 64,
	/* the players whose handicaps are kept (by absolute index); the others'
	are looked up each time */
	HANDICAP_CACHED_PLAYERS = 256,
};

/* ---------- structures */

struct handicap_entry
{
	/* as the file has it */
	char key[HANDICAP_KEY_SIZE];
	/* in ASCII, in lower case (handicap_fold_key) */
	char folded[HANDICAP_KEY_SIZE];
	real handicap;
};

struct handicap_cached_player
{
	/* the player's datum index (its identifier tells a new player of the
	slot from the one before) */
	long player_index;
	unsigned long generation;
	real handicap;
};

/* ---------- globals */

static struct handicap_entry handicap_entries[MAXIMUM_HANDICAP_ENTRIES];
static short handicap_entry_count;
static boolean handicap_loaded;
/* moves with each change of the entries, so that the players' handicaps are
looked up again */
static unsigned long handicap_generation = 1;
static struct handicap_cached_player handicap_cache[HANDICAP_CACHED_PLAYERS];

/* ---------- names */

static char handicap_lower(
	char character)
{
	return character >= 'A' && character <= 'Z' ? character - 'A' + 'a' : character;
}

/* the text without its spaces before and after */
static void handicap_trim(
	char *text)
{
	long length = csstrlen(text);
	long start = 0;

	while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t'))
		length--;
	text[length] = 0;
	while (text[start] == ' ' || text[start] == '\t')
		start++;
	if (start)
		csmemmove(text, text + start, length - start + 1);
}

/* a name of the file (UTF-8) as the players' are compared: each character
in ASCII (player_name_character_ascii: a letter with a mark its plain
letter, as the host's commands read a name), in lower case, without spaces
before and after */
static void handicap_fold_key(
	char const *key,
	char *folded,
	long size)
{
	unsigned char const *character = (unsigned char const *)key;
	long length = 0;

	while (*character && length < size - 1)
	{
		unsigned long code = *character++;
		short continuation = 0;

		if (code >= 0xF0)
		{
			code &= 0x07;
			continuation = 3;
		}
		else if (code >= 0xE0)
		{
			code &= 0x0F;
			continuation = 2;
		}
		else if (code >= 0xC0)
		{
			code &= 0x1F;
			continuation = 1;
		}
		while (continuation-- > 0 && (*character & 0xC0) == 0x80)
			code = (code << 6) | (*character++ & 0x3F);
		/* (beyond a name's 16-bit characters: a surrogate pair, two "?") */
		if (code > 0xFFFF)
		{
			folded[length++] = '?';
			if (length < size - 1)
				folded[length++] = '?';
			continue;
		}
		folded[length++] = handicap_lower(player_name_character_ascii((wchar_t)code));
	}
	folded[length] = 0;
	handicap_trim(folded);
}

/* a player's name in ASCII (player_name_character_ascii), at most size */
static void handicap_player_name(
	struct player_datum const *player,
	char *text,
	long size)
{
	long index;

	for (index = 0; index < (long)NUMBEROF(player->name) && player->name[index] && index < size - 1; index++)
		text[index] = player_name_character_ascii(player->name[index]);
	text[index] = 0;
}

/* a player's name in UTF-8, as the console writes it into the file */
static void handicap_player_name_utf8(
	struct player_datum const *player,
	char *text,
	long size)
{
	long length = 0;
	long index;

	for (index = 0; index < (long)NUMBEROF(player->name) && player->name[index]; index++)
	{
		unsigned long code = (unsigned short)player->name[index];

		if (code >= 0xD800 && code < 0xDC00 && index + 1 < (long)NUMBEROF(player->name) &&
			(unsigned short)player->name[index + 1] >= 0xDC00 && (unsigned short)player->name[index + 1] < 0xE000)
		{
			code = 0x10000 + ((code - 0xD800) << 10) + ((unsigned short)player->name[index + 1] - 0xDC00);
			index++;
		}
		if (code < 0x80 && length + 1 < size)
		{
			text[length++] = (char)code;
		}
		else if (code < 0x800 && length + 2 < size)
		{
			text[length++] = (char)(0xC0 | (code >> 6));
			text[length++] = (char)(0x80 | (code & 0x3F));
		}
		else if (code >= 0x800 && code < 0x10000 && length + 3 < size)
		{
			text[length++] = (char)(0xE0 | (code >> 12));
			text[length++] = (char)(0x80 | ((code >> 6) & 0x3F));
			text[length++] = (char)(0x80 | (code & 0x3F));
		}
		else if (code >= 0x10000 && length + 4 < size)
		{
			text[length++] = (char)(0xF0 | (code >> 18));
			text[length++] = (char)(0x80 | ((code >> 12) & 0x3F));
			text[length++] = (char)(0x80 | ((code >> 6) & 0x3F));
			text[length++] = (char)(0x80 | (code & 0x3F));
		}
		else
		{
			break;
		}
	}
	text[length] = 0;
}

/* whether a name (in ASCII) begins with the text, in either case; and is it */
static boolean handicap_name_begins_with(
	char const *name,
	char const *text,
	boolean whole)
{
	for (; *text; name++, text++)
	{
		if (!*name || handicap_lower(*name) != handicap_lower(*text))
			return FALSE;
	}
	return !whole || !*name;
}

/* ---------- the table */

/* the entry of a name (in ASCII, in either case), NONE for none */
static short handicap_entry_named(
	char const *name)
{
	char folded[HANDICAP_KEY_SIZE];
	short index;

	handicap_fold_key(name, folded, sizeof(folded));
	for (index = 0; index < handicap_entry_count; index++)
	{
		if (!csstrcmp(handicap_entries[index].folded, folded))
			return index;
	}
	return NONE;
}

/* not infinite nor NaN (the port's math.h has no isfinite) */
static boolean handicap_finite(
	double value)
{
	return value == value && value - value == 0.0;
}

static real handicap_pin(
	double value)
{
	return PIN((real)value, MINIMUM_HANDICAP, MAXIMUM_HANDICAP);
}

/* (config_table_numbers) a name of the file's [handicap] */
static void handicap_add_entry(
	void *context,
	char const *key,
	double value,
	int line)
{
	struct handicap_entry *entry;
	char folded[HANDICAP_KEY_SIZE];

	(void)context;
	if ((long)csstrlen(key) >= HANDICAP_KEY_SIZE)
	{
		platform_log("config.toml line %d: [handicap] %s is longer than a player's name; left out", line, key);
		return;
	}
	handicap_fold_key(key, folded, sizeof(folded));
	if (!folded[0])
	{
		platform_log("config.toml line %d: [handicap] needs a player's name; left out", line);
		return;
	}
	if (!handicap_finite(value) || value <= 0.0)
	{
		platform_log("config.toml line %d: [handicap] %s should be a multiple above 0; left out", line, key);
		return;
	}
	if (handicap_entry_named(folded) != NONE)
	{
		platform_log("config.toml line %d: [handicap] %s is named before; left out", line, key);
		return;
	}
	if (handicap_entry_count >= MAXIMUM_HANDICAP_ENTRIES)
	{
		platform_log("config.toml line %d: [handicap] has more than %d names; %s left out", line,
			MAXIMUM_HANDICAP_ENTRIES, key);
		return;
	}
	entry = &handicap_entries[handicap_entry_count++];
	csstrcpy(entry->key, key);
	csstrcpy(entry->folded, folded);
	entry->handicap = handicap_pin(value);
	if ((double)entry->handicap != value)
	{
		platform_log("config.toml line %d: [handicap] %s's %g is not %g to %g; %g", line, key, value,
			MINIMUM_HANDICAP, MAXIMUM_HANDICAP, entry->handicap);
	}
}

static void handicap_load(
	void)
{
	short index;

	handicap_entry_count = 0;
	config_table_numbers(HANDICAP_SECTION, handicap_add_entry, NULL);
	handicap_loaded = TRUE;
	handicap_generation++;
	for (index = 0; index < handicap_entry_count; index++)
		platform_log("handicap: %s x%g", handicap_entries[index].key, handicap_entries[index].handicap);
}

/* ---------- the players */

/* whether there are players to look at: the players' data is valid only
while a map is loaded (the console and its completion also run in the
menus) */
static boolean handicap_players_valid(
	void)
{
	return player_data != NULL && player_data->valid;
}

/* the player's handicap (a datum index), 1 for none */
static real handicap_of_player(
	long player_index)
{
	struct handicap_cached_player *cached = NULL;
	struct player_datum *player;
	char name[HANDICAP_NAME_TEXT_SIZE];
	long absolute_index;
	short entry_index;
	real handicap = 1.0f;

	if (player_index == NONE || !handicap_players_valid())
		return 1.0f;
	if (!handicap_loaded)
		handicap_load();
	absolute_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	if (absolute_index < HANDICAP_CACHED_PLAYERS)
	{
		cached = &handicap_cache[absolute_index];
		if (cached->player_index == player_index && cached->generation == handicap_generation)
			return cached->handicap;
	}
	player = (struct player_datum *)datum_try_and_get(player_data, player_index);
	if (!player)
		return 1.0f;
	handicap_player_name(player, name, sizeof(name));
	entry_index = handicap_entry_named(name);
	if (entry_index != NONE)
		handicap = handicap_entries[entry_index].handicap;
	if (cached)
	{
		cached->player_index = player_index;
		cached->generation = handicap_generation;
		cached->handicap = handicap;
		if (handicap != 1.0f)
			platform_log("handicap: %s's health and shields x%g", name, handicap);
	}
	return handicap;
}

real handicap_damage_multiplier(
	long player_index)
{
	if (player_index == NONE || network_game_distributed_client())
		return 1.0f;
	return 1.0f / handicap_of_player(player_index);
}

void handicap_game_starting(
	void)
{
	/* (a client's are not used: the host deals the damage) */
	if (!network_game_distributed_client())
		handicap_load();
}

short handicap_matching_player_names(
	char const *text,
	char (*names)[HANDICAP_NAME_TEXT_SIZE],
	short maximum_count)
{
	struct data_iterator iterator;
	struct player_datum *player;
	short count = 0;

	if (!handicap_players_valid())
		return 0;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL && count < maximum_count)
	{
		if (player->quit_out_of_game)
			continue;
		handicap_player_name(player, names[count], HANDICAP_NAME_TEXT_SIZE);
		if (handicap_name_begins_with(names[count], text, FALSE))
			count++;
	}
	return count;
}

/* the player of the name (the whole of it, else the one whose name begins
with it), NONE for none; *matches how many begin with it */
static long handicap_player_named(
	char const *text,
	short *matches)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long found = NONE;
	long exact_found = NONE;
	short exact = 0;

	*matches = 0;
	if (!handicap_players_valid() || !text[0])
		return NONE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		char name[HANDICAP_NAME_TEXT_SIZE];

		if (player->quit_out_of_game)
			continue;
		handicap_player_name(player, name, sizeof(name));
		if (handicap_name_begins_with(name, text, TRUE))
		{
			exact++;
			exact_found = iterator.datum_index;
		}
		if (handicap_name_begins_with(name, text, FALSE))
		{
			(*matches)++;
			found = iterator.datum_index;
		}
	}
	if (exact == 1)
	{
		*matches = 1;
		return exact_found;
	}
	return *matches == 1 ? found : NONE;
}

/* ---------- the console */

/* a multiple as the console takes it: "2", "1.5", "x2", "2x" or "150%" */
static boolean handicap_parse_multiple(
	char const *text,
	double *value)
{
	char *end;
	double result;

	if (*text == 'x' || *text == 'X')
		text++;
	if (!(*text >= '0' && *text <= '9') && *text != '.')
		return FALSE;
	result = strtod(text, &end);
	if (end == text)
		return FALSE;
	if (*end == '%')
	{
		result /= 100.0;
		end++;
	}
	else if (*end == 'x' || *end == 'X')
	{
		end++;
	}
	if (*end || !handicap_finite(result) || result <= 0.0)
		return FALSE;
	*value = result;
	return TRUE;
}

/* what the command's text names: the name, and the multiple after it if
there is one ("Player One 2", "\"Player One\" 2") */
static void handicap_parse_command(
	char const *text,
	char *name,
	long name_size,
	char *multiple,
	long multiple_size)
{
	char buffer[128];
	long length;

	name[0] = 0;
	multiple[0] = 0;
	csstrncpy(buffer, text, sizeof(buffer));
	buffer[sizeof(buffer) - 1] = 0;
	/* (an expression's closing parenthesis) */
	length = csstrlen(buffer);
	while (length > 0 && (buffer[length - 1] == ' ' || buffer[length - 1] == '\t' || buffer[length - 1] == ')'))
		length--;
	buffer[length] = 0;
	handicap_trim(buffer);
	if (buffer[0] == '"')
	{
		char *close = buffer + 1;

		while (*close && *close != '"')
			close++;
		if (*close)
		{
			csstrncpy(multiple, close + 1, multiple_size);
			multiple[multiple_size - 1] = 0;
			*close = 0;
		}
		csstrncpy(name, buffer + 1, name_size);
		name[name_size - 1] = 0;
	}
	else
	{
		char *last_space = NULL;
		double value;

		for (length = 0; buffer[length]; length++)
		{
			if (buffer[length] == ' ' || buffer[length] == '\t')
				last_space = &buffer[length];
		}
		if (last_space && handicap_parse_multiple(last_space + 1, &value))
		{
			csstrncpy(multiple, last_space + 1, multiple_size);
			multiple[multiple_size - 1] = 0;
			*last_space = 0;
		}
		csstrncpy(name, buffer, name_size);
		name[name_size - 1] = 0;
	}
	handicap_trim(name);
	handicap_trim(multiple);
}

/* every player's handicap (or, with none in a game, every name's) */
static void handicap_list(
	void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	short count = 0;
	short index;

	if (handicap_players_valid())
	{
		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			char name[HANDICAP_NAME_TEXT_SIZE];

			if (player->quit_out_of_game)
				continue;
			handicap_player_name(player, name, sizeof(name));
			console_printf(FALSE, "%s: x%g", name, handicap_of_player(iterator.datum_index));
			count++;
		}
	}
	if (count)
		return;
	for (index = 0; index < handicap_entry_count; index++)
		console_printf(FALSE, "%s: x%g", handicap_entries[index].key, handicap_entries[index].handicap);
	if (!handicap_entry_count)
		console_printf(FALSE, "handicap: none (config.toml's [handicap], or \"handicap <name> <multiple>\")");
}

/* sets the handicap of the name (the file's key, UTF-8), now and in the file */
static void handicap_set(
	char const *key,
	char const *shown_name,
	real handicap)
{
	short entry_index = handicap_entry_named(key);
	char const *old_key = NULL;
	struct handicap_entry *entry;

	if (entry_index != NONE)
	{
		entry = &handicap_entries[entry_index];
		/* (the file's own spelling stays) */
		old_key = entry->key;
	}
	else if (handicap_entry_count < MAXIMUM_HANDICAP_ENTRIES)
	{
		entry = &handicap_entries[handicap_entry_count++];
		csstrncpy(entry->key, key, sizeof(entry->key));
		entry->key[sizeof(entry->key) - 1] = 0;
		handicap_fold_key(entry->key, entry->folded, sizeof(entry->folded));
	}
	else
	{
		console_warning("handicap: more than %d names", MAXIMUM_HANDICAP_ENTRIES);
		return;
	}
	entry->handicap = handicap;
	handicap_generation++;
	if (config_table_write_number(HANDICAP_SECTION, old_key, entry->key, handicap))
		console_printf(FALSE, "%s: x%g (and in config.toml)", shown_name, handicap);
	else
		console_warning("%s: x%g, but config.toml cannot be written: until the next game", shown_name, handicap);
	platform_log("handicap: %s x%g (the console)", entry->key, handicap);
}

boolean handicap_command(
	char const *text)
{
	char name[64];
	char multiple[32];
	double value;
	short matches;
	long player_index;
	struct player_datum *player = NULL;

	if (network_game_distributed_client())
	{
		console_warning("handicap: only the host's handicaps count (its config.toml's [handicap])");
		return FALSE;
	}
	if (!handicap_loaded)
		handicap_load();
	handicap_parse_command(text, name, sizeof(name), multiple, sizeof(multiple));
	if (!name[0])
	{
		handicap_list();
		return TRUE;
	}
	if (multiple[0] && !handicap_parse_multiple(multiple, &value))
	{
		console_warning("handicap: %s is not a multiple (2, 1.5, 150%%)", multiple);
		return FALSE;
	}
	player_index = handicap_player_named(name, &matches);
	if (matches > 1)
	{
		console_warning("handicap: %d players' names begin with \"%s\": give more of it", matches, name);
		return FALSE;
	}
	if (player_index != NONE)
		player = (struct player_datum *)datum_try_and_get(player_data, player_index);
	if (!multiple[0])
	{
		short entry_index;

		if (player)
		{
			char shown_name[HANDICAP_NAME_TEXT_SIZE];

			handicap_player_name(player, shown_name, sizeof(shown_name));
			console_printf(FALSE, "%s: x%g", shown_name, handicap_of_player(player_index));
			return TRUE;
		}
		entry_index = handicap_entry_named(name);
		if (entry_index == NONE)
		{
			console_warning("handicap: no player's name begins with \"%s\", nor is it in config.toml", name);
			return FALSE;
		}
		console_printf(FALSE, "%s: x%g (not in the game)", handicap_entries[entry_index].key,
			handicap_entries[entry_index].handicap);
		return TRUE;
	}
	if (handicap_pin(value) != (real)value)
		console_warning("handicap: %g to %g; %g", MINIMUM_HANDICAP, MAXIMUM_HANDICAP, handicap_pin(value));
	if (player)
	{
		char key[HANDICAP_KEY_SIZE];
		char shown_name[HANDICAP_NAME_TEXT_SIZE];

		handicap_player_name_utf8(player, key, sizeof(key));
		handicap_player_name(player, shown_name, sizeof(shown_name));
		handicap_set(key, shown_name, handicap_pin(value));
	}
	else
	{
		/* (one not in the game now: the name as typed, for when they join) */
		handicap_set(name, name, handicap_pin(value));
		console_printf(FALSE, "(no player of the game is named \"%s\" now)", name);
	}
	return TRUE;
}
