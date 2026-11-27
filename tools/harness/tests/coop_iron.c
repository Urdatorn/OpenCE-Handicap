#include "harness.h"
#include <string.h>
#include "under_test.inc"
static char const ff[] = "custom_maps\\Firefight-Descent";
int main(int argc, char **argv)
{
    CHECK(argc == 2, "case");
    char const *case_name = argv[1];
    CASE("listed") { CHECK(network_coop_level_listed(ff, ff), "the level not found"); }
    else CASE("either-case") {
        CHECK(network_coop_level_listed("CUSTOM_MAPS\\firefight-descent", ff), "case not ignored");
    }
    else CASE("spaces") { CHECK(network_coop_level_listed(" a30 ,  custom_maps\\Firefight-Descent  ", ff),
        "spaces not left out"); }
    else CASE("semicolons") { CHECK(network_coop_level_listed("a30;b40", "b40"), "';' not a split"); }
    else CASE("second") { CHECK(network_coop_level_listed("a300,b40", "b40"), "the second name not found"); }
    else CASE("prefix") { CHECK(!network_coop_level_listed("custom_maps\\Firefight", ff), "a name's start listed"); }
    else CASE("longer") { CHECK(!network_coop_level_listed("a300", "a30"), "a longer name listed"); }
    else CASE("empty") {
        CHECK(!network_coop_level_listed("", "a30") && !network_coop_level_listed(NULL, "a30") &&
            !network_coop_level_listed(",,", "a30"), "nothing listed found");
    }
    else CASE("new-death") { CHECK(players_coop_iron_new_death(3, 2, TRUE), "a new death not Iron's"); }
    else CASE("no-new-death") { CHECK(!players_coop_iron_new_death(2, 2, TRUE), "an old death reverted again"); }
    else CASE("revert-back") {
        /* (a revert takes the deaths back to the checkpoint's) */
        CHECK(!players_coop_iron_new_death(1, 2, TRUE), "a revert taken for a death");
    }
    else CASE("death-respawned") {
        /* (deaths that went up with everyone alive again: nothing to lose) */
        CHECK(!players_coop_iron_new_death(3, 2, FALSE), "no one dead, yet reverted");
    }
    else CHECK(0, "unknown case %s", case_name);
    return 0;
}
