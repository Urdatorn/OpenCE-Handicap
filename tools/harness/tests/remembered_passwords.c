#include "harness.h"
#include <string.h>
#define P2P_LISTING_SIGNING_KEY_SIZE 32
#define P2P_PASSWORD_KEY_SIZE 32
#define MAXIMUM_REMEMBERED_PASSWORDS 64
#include "types.inc"
/* (the overflow control writes one past the entries: room for it, seen) */
static struct remembered_password entries[MAXIMUM_REMEMBERED_PASSWORDS + 1];
#include "under_test.inc"
static unsigned char host[32], key[32];
static void fill(unsigned char *bytes, int value) { memset(bytes, value, 32); }
/* a line of a host's key of 0x11s and a password's key of 0xab */
static void line_of(char *line, char host_digit, char key_digit)
{
    memset(line, host_digit, 64);
    line[64] = ' ';
    memset(line + 65, key_digit, 64);
    line[129] = '\n';
    line[130] = 0;
}
int main(int argc, char **argv)
{
    CHECK(argc == 2, "case");
    char const *case_name = argv[1];
    char line[160];
    struct remembered_password entry;
    CASE("parse") {
        line_of(line, '1', 'a');
        CHECK(remembered_password_parse(line, &entry), "a line refused");
        CHECK(entry.host[0] == 0x11 && entry.host[31] == 0x11 && entry.key[0] == 0xaa && entry.key[31] == 0xaa,
            "the keys misread");
    }
    else CASE("parse-upper") {
        line_of(line, 'F', 'A');
        CHECK(remembered_password_parse(line, &entry) && entry.host[0] == 0xff && entry.key[0] == 0xaa,
            "capitals refused");
    }
    else CASE("parse-short") {
        line_of(line, '1', 'a');
        line[100] = '\n'; line[101] = 0;
        CHECK(!remembered_password_parse(line, &entry), "a short key taken");
    }
    else CASE("parse-no-space") {
        line_of(line, '1', 'a');
        line[64] = 'x';
        CHECK(!remembered_password_parse(line, &entry), "keys without the space taken");
    }
    else CASE("parse-bad-digit") {
        line_of(line, '1', 'a');
        line[10] = 'g';
        CHECK(!remembered_password_parse(line, &entry), "a key with a letter past f taken");
    }
    else CASE("add") {
        int count;
        fill(host, 1); fill(key, 2);
        count = remembered_passwords_update(entries, 0, host, key);
        fill(host, 3); fill(key, 4);
        count = remembered_passwords_update(entries, count, host, key);
        CHECK(count == 2 && entries[0].host[0] == 3 && entries[1].host[0] == 1, "not the newest first");
    }
    else CASE("replace") {
        int count;
        fill(host, 1); fill(key, 2);
        count = remembered_passwords_update(entries, 0, host, key);
        fill(host, 3);
        count = remembered_passwords_update(entries, count, host, key);
        fill(host, 1); fill(key, 9);
        count = remembered_passwords_update(entries, count, host, key);
        CHECK(count == 2, "a host kept twice (%d)", count);
        CHECK(entries[0].host[0] == 1 && entries[0].key[0] == 9 && entries[1].host[0] == 3, "the new key not first");
    }
    else CASE("forget") {
        int count;
        fill(host, 1); fill(key, 2);
        count = remembered_passwords_update(entries, 0, host, key);
        fill(host, 3);
        count = remembered_passwords_update(entries, count, host, key);
        count = remembered_passwords_update(entries, count, host, NULL);
        CHECK(count == 1 && entries[0].host[0] == 1, "a forgotten host kept (%d)", count);
    }
    else CASE("full") {
        int count = 0, index;
        for (index = 0; index < MAXIMUM_REMEMBERED_PASSWORDS; index++) {
            fill(host, index + 1); fill(key, 0);
            count = remembered_passwords_update(entries, count, host, key);
        }
        fill(host, 200);
        count = remembered_passwords_update(entries, count, host, key);
        CHECK(count == MAXIMUM_REMEMBERED_PASSWORDS, "past the most (%d)", count);
        CHECK(entries[0].host[0] == 200 && entries[MAXIMUM_REMEMBERED_PASSWORDS - 1].host[0] == 2,
            "not the oldest dropped");
    }
    else CHECK(0, "unknown case %s", case_name);
    return 0;
}
