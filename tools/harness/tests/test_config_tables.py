"""The real port_config.c, whole, with the real tomlc17: config.toml's tables of names ([handicap]) are read afresh,
written a line at a time without changing the rest of the file, are no misspelt settings, and are written into a new
file and added to an older one."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, mutated, read, run  # noqa: E402

CASES = ["read-numbers", "read-none", "write-replaces", "write-adds", "write-numbers", "write-escapes", "defaults",
         "not-unknown", "added-to-old-file"]

SDL_STUB = """#include <stdbool.h>
#include <stddef.h>
const char *SDL_GetBasePath(void);
void *SDL_LoadFile(const char *path, size_t *size);
bool SDL_SaveFile(const char *path, const void *data, size_t size);
void SDL_free(void *data);
"""
PLATFORM_STUB = "void platform_log(const char *format, ...);\n"

# a fault in port_config.c and the case that must catch it
NEGATIVE_CONTROLS = {
    "replaced-in-any-section": (("in_section = !strcmp(current, section);", "in_section = 1;"), "write-replaces"),
    "escapes-not-read": (("if (quote == '\"' && character == '\\\\' && line < end)\n\t\t\t\tcharacter = *line++;\n", ""),
                         "write-escapes"),
    "escapes-not-written": (("\t\tif (*character == '\"' || *character == '\\\\')\n\t\t\t*end++ = '\\\\';\n", ""),
                            "write-escapes"),
    "integer-written": (("strcat(number, \".0\");", ";"), "write-numbers"),
    "added-after-blank-lines": (("if (in_section && !written && config_line_blank(line, line_end))", "if (0)"),
                                "write-adds"),
    "handicap-unknown": (("\t\tif (config_name_table(table.u.tab.key[section_index]))\n\t\t\tcontinue;\n", ""),
                         "not-unknown"),
    "not-in-defaults": (("\tfor (index = 0; index < NUMBER_OF_CONFIG_NAME_TABLES; index++)\n"
                         "\t\tconfig_append_name_table(&text, &config_name_tables[index]);\n", ""), "defaults"),
    "not-added-to-old-files": (("if (toml_get(table, name_table->section).type != TOML_UNKNOWN)", "if (1)"),
                               "added-to-old-file"),
    "added-again": (("if (toml_get(table, name_table->section).type != TOML_UNKNOWN)", "if (0)"),
                    "added-to-old-file"),
}


def generated(fault=None):
    source = read("port/linux/src/port_config.c").replace("#include <SDL3/SDL.h>", '#include "sdl_stub.h"')
    if fault:
        source = mutated(source, *fault)
    return (("platform.h", PLATFORM_STUB), ("sdl_stub.h", SDL_STUB),
            ("port_config.h", read("port/linux/src/port_config.h")),
            ("tomlc17.h", read("port/third_party/tomlc17/tomlc17.h")),
            ("tomlc17.inc", read("port/third_party/tomlc17/tomlc17.c")), ("port_config.inc", source))


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("config_tables", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("config_tables", generated(fault)), case)
    assert status == CHECK_FAILED, f"port_config.c with a fault ({control}) passed '{case}': the test cannot see it"
