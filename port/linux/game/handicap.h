/*
HANDICAP.H

Multiplayer handicaps (handicap.c): each player's health and shields as a
multiple of the gametype's, which the host sets by name. What the damage
(game_engine.c), the start of a game (game_engine.c), the console (hs.c) and
its completion (console.c) ask of it.
*/

#ifndef __HANDICAP_H
#define __HANDICAP_H
#pragma once

/* ---------- constants */

enum
{
	/* a player's name in ASCII, with its end */
	HANDICAP_NAME_TEXT_SIZE = 16,
};

/* ---------- prototypes/HANDICAP.C */

/* what damage to the player (a datum index, or NONE) is multiplied by: one
over its handicap; 1 on a client of the distributed netcode, where the
host's damage is dealt */
real handicap_damage_multiplier(long player_index);
/* a game begins: config.toml's [handicap] is read again */
void handicap_game_starting(void);
/* the console's "handicap" (every player's), "handicap <name>" (one's) and
"handicap <name> <multiple>" (sets it, now and in config.toml); text is what
follows the command */
boolean handicap_command(char const *text);
/* the names of the game's players that begin with the text (the command's
completion: console.c); how many */
short handicap_matching_player_names(char const *text, char (*names)[HANDICAP_NAME_TEXT_SIZE],
	short maximum_count);

#endif // __HANDICAP_H
