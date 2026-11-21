#include "harness.h"
#include <string.h>
#define csmemset memset
#include "types.inc"
/* (another global just past the cheats; the notes have room for its index,
so that one taken for a cheat is seen in them, not written past them) */
static struct { struct cheat_globals cheat; boolean other[4]; } world;
#define cheat world.cheat
static boolean cheats_set_by_map[2 * CHEAT_COUNT];
static boolean cheats_before_map[2 * CHEAT_COUNT];
#include "under_test.inc"
/* (as hs_evaluate_set does: the value before, the set, then the note) */
static void set(boolean *address, boolean value, boolean by_map)
{
    boolean before = *address;

    *address = value;
    cheats_note_set(address, before, by_map);
}
int main(int argc, char **argv)
{
    CHECK(argc == 2, "case");
    char const *case_name = argv[1];
    CASE("map-on") {
        set(&cheat.deathless_player, TRUE, TRUE);
        cheats_dispose_from_old_map();
        CHECK(!cheat.deathless_player, "a map's cheat kept after it");
    }
    else CASE("map-off") {
        cheat.omnipotent = TRUE;
        set(&cheat.omnipotent, FALSE, TRUE);
        cheats_dispose_from_old_map();
        CHECK(cheat.omnipotent, "a cheat the map turned off not given back");
    }
    else CASE("console-kept") {
        set(&cheat.medusa, TRUE, FALSE);
        cheats_dispose_from_old_map();
        CHECK(cheat.medusa, "a console cheat undone");
    }
    else CASE("console-after-map") {
        set(&cheat.deathless_player, TRUE, TRUE);
        set(&cheat.deathless_player, FALSE, FALSE);
        set(&cheat.medusa, TRUE, TRUE);
        set(&cheat.medusa, TRUE, FALSE);
        cheats_dispose_from_old_map();
        CHECK(!cheat.deathless_player, "the console's setting after the map's undone");
        CHECK(cheat.medusa, "the console's setting after the map's undone");
    }
    else CASE("first-before") {
        set(&cheat.deathless_player, TRUE, TRUE);
        set(&cheat.deathless_player, TRUE, TRUE);
        cheats_dispose_from_old_map();
        CHECK(!cheat.deathless_player, "not the value before the map's first set");
    }
    else CASE("next-map") {
        set(&cheat.deathless_player, TRUE, TRUE);
        cheats_dispose_from_old_map();
        /* (turned on by nothing the next map's scripts did) */
        cheat.deathless_player = TRUE;
        cheats_dispose_from_old_map();
        CHECK(cheat.deathless_player, "the map before's set undone again");
    }
    else CASE("not-a-cheat") {
        set(&world.other[0], TRUE, TRUE);
        CHECK(!memchr(cheats_set_by_map, TRUE, sizeof(cheats_set_by_map)), "another global taken for a cheat");
    }
    else CHECK(0, "unknown case %s", case_name);
    return 0;
}
