#include "harness.h"
#include <string.h>
#define csmemcpy memcpy
#include "under_test.inc"
static char const games[] = " firefight = custom_maps\\Firefight-Descent:coop , rockets=prisoner:rockets+lives=3";
static char entry[64];
static boolean find(char const *list, char const *text, long size)
{
    memset(entry, '#', sizeof(entry));
    return server_chat_game_find(list, text, entry, size);
}
int main(int argc, char **argv)
{
    CHECK(argc == 2, "case");
    char const *case_name = argv[1];
    CASE("found") {
        CHECK(find(games, "!firefight", sizeof(entry)), "not found");
        CHECK(!strcmp(entry, "custom_maps\\Firefight-Descent:coop"), "entry %s", entry);
    }
    else CASE("either-case") { CHECK(find(games, "!FireFight", sizeof(entry)), "case refused"); }
    else CASE("trailing-spaces") { CHECK(find(games, "!firefight  ", sizeof(entry)), "trailing spaces refused"); }
    else CASE("more-words") { CHECK(!find(games, "!firefight now", sizeof(entry)), "talk played"); }
    else CASE("other-word") { CHECK(!find(games, "!switch", sizeof(entry)), "another word played"); }
    else CASE("prefix") { CHECK(!find(games, "!fire", sizeof(entry)), "a word's start played"); }
    else CASE("longer-word") { CHECK(!find(games, "!firefights", sizeof(entry)), "a longer word played"); }
    else CASE("second-pair") {
        CHECK(find(games, "!rockets", sizeof(entry)), "second pair not found");
        CHECK(!strcmp(entry, "prisoner:rockets+lives=3"), "entry %s", entry);
    }
    else CASE("semicolons") {
        CHECK(find("a=bloodgulch:ctf;b=ratrace:slayer", "!b", sizeof(entry)), "semicolon pair not found");
        CHECK(!strcmp(entry, "ratrace:slayer"), "entry %s", entry);
    }
    else CASE("rule-equals") {
        /* (the word ends at the first '=': an entry's rules have their own) */
        CHECK(!find("lives=prisoner:slayer+lives=3", "!lives=prisoner:slayer+lives", sizeof(entry)),
            "a word with '=' played");
        CHECK(find("lives=prisoner:slayer+lives=3", "!lives", sizeof(entry)), "not found");
        CHECK(!strcmp(entry, "prisoner:slayer+lives=3"), "entry %s", entry);
    }
    else CASE("trimmed") {
        CHECK(find("x=  ratrace:slayer  ", "!x", sizeof(entry)), "not found");
        CHECK(!strcmp(entry, "ratrace:slayer"), "entry '%s'", entry);
    }
    else CASE("too-long") {
        /* (an entry of 14 characters and its end need 15 bytes) */
        CHECK(!find("x=ratrace:slayer", "!x", 14), "an entry past its buffer");
        CHECK(entry[13] == '#', "written past");
        CHECK(find("x=ratrace:slayer", "!x", 15), "an entry that fits refused");
    }
    else CASE("no-bang") { CHECK(!find(games, "firefight", sizeof(entry)), "a line without ! played"); }
    else CASE("bare-bang") { CHECK(!find("=bloodgulch:ctf", "!", sizeof(entry)), "a bare ! played"); }
    else CASE("empty") { CHECK(!find("", "!firefight", sizeof(entry)) && !find(NULL, "!firefight", sizeof(entry)),
        "no games played"); }
    else CASE("no-entry") { CHECK(!find("x=, y", "!x", sizeof(entry)) && !find("x=, y", "!y", sizeof(entry)),
        "a word without an entry played"); }
    else CHECK(0, "unknown case %s", case_name);
    return 0;
}
