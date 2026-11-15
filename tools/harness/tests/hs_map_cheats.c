#include "harness.h"
#include <string.h>
#include "types.inc"
struct hs_external_global_definition { char const *name; };
static struct hs_external_global_definition globals[] = {
    { "cheat_omnipotent" }, { "rasterizer_far_clip_distance" } };
#define hs_global_external_get(i) (&globals[i])
#define csstrncmp strncmp
#include "under_test.inc"
int main(int argc, char **argv)
{
    CHECK(argc == 2, "case");
    char const *case_name = argv[1];
    /* (a constant false, as hs_compile makes (set cheat_omnipotent false)) */
    struct hs_syntax_node value = { 0 };
    value.type = _hs_type_boolean;
    value.flags = FLAG(_hs_syntax_node_primitive_bit);
    value.boolean_value = FALSE;
    CASE("cheat-off") { CHECK(hs_set_turns_cheat_off(0, &value), "a cheat turned off refused"); }
    else CASE("cheat-on") {
        value.boolean_value = TRUE;
        CHECK(!hs_set_turns_cheat_off(0, &value), "a cheat turned on allowed");
    }
    else CASE("cheat-from-global") {
        /* (a global's value, whatever it holds when the script runs) */
        value.flags |= FLAG(_hs_syntax_node_variable_bit);
        CHECK(!hs_set_turns_cheat_off(0, &value), "a cheat set from a global allowed");
    }
    else CASE("cheat-from-call") {
        /* (a function call's result) */
        value.flags = 0;
        CHECK(!hs_set_turns_cheat_off(0, &value), "a cheat set from a call allowed");
    }
    else CASE("other-global") { CHECK(!hs_set_turns_cheat_off(1, &value), "another global set false allowed"); }
    else CHECK(0, "unknown case %s", case_name);
    return 0;
}
