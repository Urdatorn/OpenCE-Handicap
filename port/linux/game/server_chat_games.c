/*
SERVER_CHAT_GAMES.C

A dedicated server's players start the games its owner names: with
server.chat_games = "firefight=custom_maps\Firefight-Descent:coop", a
player who says "!firefight" in the chat (the whole line, in either case)
has that entry played as sv_map plays it (server_director_interpose): at
once, or after the game whose start was sent, and the rotation goes on
after it. The host takes the line and sends it on to no one
(network_chat_handle_up), as it does "!switch"; any client's chat sends
it, so the players need no build of their own. Every machine is told who
asked for what, and the server's log has it; an entry that cannot be played
is told to the player alone. A game that never ends (a firefight) is left
by the rotation's next game: "!endgame"'s vote, or sv_map_next.
*/

#include "cseries.h"
#include "server_chat_games.h"
#include "server_director.h"
#include "../src/port_config.h" /* port: port/linux/src/port_config.c */
#include "../src/server_log.h" /* port: port/linux/src/server_log.c */
#include "../src/server_rotation.h" /* port: port/linux/src/server_rotation.c */

#include <stdio.h>

/* ---------- private code */

static char chat_games_lower(
	char c)
{
	return (char)(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
}

static boolean chat_games_space(
	char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/* ---------- public code */

boolean server_chat_game_find(
	char const *games,
	char const *text,
	char *entry,
	long entry_size)
{
	char const *word;
	long word_length;

	if (!games || !text || text[0] != '!' || entry_size <= 0)
		return FALSE;
	word = text + 1;
	word_length = 0;
	while (word[word_length] && !chat_games_space(word[word_length]))
		word_length++;
	/* (the word and nothing more: "!firefight now" is talk) */
	{
		char const *after = word + word_length;

		while (chat_games_space(*after))
			after++;
		if (!word_length || *after)
			return FALSE;
	}
	while (*games)
	{
		char const *start = games;
		char const *end;
		char const *equals = NULL;
		long index;

		while (*games && *games != ',' && *games != ';')
			games++;
		end = games;
		if (*games)
			games++;
		while (start < end && chat_games_space(*start))
			start++;
		while (end > start && chat_games_space(end[-1]))
			end--;
		for (index = 0; start + index < end; index++)
		{
			if (start[index] == '=')
			{
				equals = start + index;
				break;
			}
		}
		if (!equals)
			continue;
		/* (the pair's word, its spaces trimmed) */
		{
			char const *key_end = equals;

			while (key_end > start && chat_games_space(key_end[-1]))
				key_end--;
			if (key_end - start != word_length)
				continue;
		}
		for (index = 0; index < word_length; index++)
		{
			if (chat_games_lower(start[index]) != chat_games_lower(word[index]))
				break;
		}
		if (index < word_length)
			continue;
		/* (the entry, its spaces trimmed) */
		start = equals + 1;
		while (start < end && chat_games_space(*start))
			start++;
		if (start == end || end - start >= entry_size)
			return FALSE;
		csmemcpy(entry, start, end - start);
		entry[end - start] = 0;
		return TRUE;
	}

	return FALSE;
}

short server_chat_game(
	char const *text,
	char const *name,
	char *notice,
	long notice_size)
{
	char entry[SERVER_ENTRY_TEXT_SIZE];
	char reason[SERVER_REASON_SIZE];

	if (!server_chat_game_find(config_string("server.chat_games"), text, entry, sizeof(entry)))
		return _server_chat_game_none;
	if (!server_director_interpose(entry, reason, sizeof(reason)))
	{
		snprintf(notice, notice_size, "%s cannot be played: %s", entry, reason);
		server_log(_server_log_warning, "chat", "%s asked for %s (%s), which cannot be played: %s",
			name, text, entry, reason);
		return _server_chat_game_refused;
	}
	snprintf(notice, notice_size, "%s asked for %s: %s", name, text, reason);
	server_log(_server_log_info, "chat", "%s asked for %s: %s (%s)", name, text, entry, reason);
	return _server_chat_game_played;
}
