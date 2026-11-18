#ifndef SERVER_CHAT_GAMES_H
#define SERVER_CHAT_GAMES_H

/* what a player's chat line did as one of server.chat_games' words */
enum
{
	/* not one: the line is said */
	_server_chat_game_none,
	/* the game is played next: notice says so to every machine */
	_server_chat_game_played,
	/* the game cannot be played: notice says why, to the player alone */
	_server_chat_game_refused,
};

/* the entry that games (server.chat_games: "word=entry, ...") gives a chat
line that is "!word" and nothing more, the word in either case: TRUE with
it in entry (entry_size bytes), else FALSE */
boolean server_chat_game_find(char const *games, char const *text, char *entry, long entry_size);

/* (the host of a dedicated server; network_chat.c) a player's line: one of
the values above, notice (of notice_size bytes) written unless none. name
is the player's, as the chat writes it */
short server_chat_game(char const *text, char const *name, char *notice, long notice_size);

#endif
