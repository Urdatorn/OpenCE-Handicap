/*
CONFIG_TABLES.C (test)

The real port/linux/src/port_config.c, whole, with the real tomlc17, on a
config.toml in a folder of its own: its tables of names ([handicap], read by
port/linux/game/handicap.c), which hold names of the player's choosing
rather than settings. They are read afresh, written a line at a time
without changing the rest of the file, are no misspelt settings, and are
written into a new file and added to an older one. test_config_tables.py
takes the code (port_config.inc, tomlc17.inc) and the headers.
*/

#include "harness.h"

#include <stdarg.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---------- the fake world */

/* what the code logged, a line each */
static char log_text[1 << 16];

void platform_log(const char *format, ...)
{
	size_t length = strlen(log_text);
	va_list arguments;

	va_start(arguments, format);
	vsnprintf(log_text + length, sizeof(log_text) - length - 1, format, arguments);
	va_end(arguments);
	strcat(log_text, "\n");
}

/* SDL (sdl_stub.h): the folder config.toml is in, and its files */
#include "sdl_stub.h"

static char base_path[512];

const char *SDL_GetBasePath(void)
{
	return base_path;
}

void *SDL_LoadFile(const char *path, size_t *size)
{
	FILE *file = fopen(path, "rb");
	char *data;
	long length;

	if (!file)
		return NULL;
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, 0, SEEK_SET);
	data = malloc((size_t)length + 1);
	*size = fread(data, 1, (size_t)length, file);
	fclose(file);
	return data;
}

bool SDL_SaveFile(const char *path, const void *data, size_t size)
{
	FILE *file = fopen(path, "wb");
	bool written;

	if (!file)
		return false;
	written = fwrite(data, 1, size, file) == size;
	return fclose(file) == 0 && written;
}

void SDL_free(void *data)
{
	free(data);
}

bool SDL_GetPathInfo(const char *path, SDL_PathInfo *info)
{
	struct stat status;

	if (stat(path, &status))
		return false;
	if (info)
	{
		info->size = status.st_size;
		info->modify_time = status.st_mtime;
	}
	return true;
}

/* the command line: none (no --server, no --config) */
int posix_command_line_argument(int index, char *buffer, unsigned int size)
{
	(void)index;
	(void)buffer;
	(void)size;
	return 0;
}

/* (a dedicated server's file made its user's alone: none here) */
int posix_server_private_file(const char *path)
{
	(void)path;
	return 1;
}

/* (tomlc17 is C11; the harness compiles C99, which clang gives _Static_assert) */
#include <assert.h>
#ifndef static_assert
#define static_assert _Static_assert
#endif
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "tomlc17.inc"
#pragma clang diagnostic pop
#include "port_config.inc"

/* ---------- the file */

static char config_file_path[600];

static void config_file_write(const char *text)
{
	FILE *file = fopen(config_file_path, "wb");

	CHECK(file, "cannot write %s", config_file_path);
	fputs(text, file);
	fclose(file);
}

/* the file as it is now; free() it */
static char *config_file_text(void)
{
	size_t size = 0;
	char *text = SDL_LoadFile(config_file_path, &size);

	CHECK(text, "no %s", config_file_path);
	text[size] = 0;
	return text;
}

static int occurrences(const char *text, const char *part)
{
	int count = 0;

	for (text = strstr(text, part); text; text = strstr(text + 1, part))
		count++;
	return count;
}

/* the numbers config_table_numbers gives */
static struct { char key[300]; double value; } read_entries[16];
static int read_count;

static void read_each(void *context, const char *key, double value, int line)
{
	(void)context;
	(void)line;
	CHECK(read_count < 16, "too many");
	snprintf(read_entries[read_count].key, sizeof(read_entries[read_count].key), "%s", key);
	read_entries[read_count].value = value;
	read_count++;
}

static int read_table(void)
{
	read_count = 0;
	return config_table_numbers("handicap", read_each, NULL);
}

/* the text after writing a key into it, which must be the expected */
static void check_write(const char *before, const char *old_key, const char *key, double value, const char *expected)
{
	char *after;

	config_file_write(before);
	CHECK(config_table_write_number("handicap", old_key, key, value), "not written");
	after = config_file_text();
	CHECK(!strcmp(after, expected), "writing %s = %g into\n---\n%s---\ngave\n---\n%s---\nnot\n---\n%s---", key, value,
		before, after, expected);
	free(after);
}

static const char example[] =
	"# Halo settings\n"
	"\n"
	"[display]\n"
	"vsync = true\n"
	"Ace = 1\n"
	"\n"
	"[handicap]\n"
	"# a comment\n"
	"\"Player One\" = 2.0\n"
	"Ace = 0.75 # the best\n"
	"'Literal' = 3\n"
	"Bad = \"two\"\n"
	"\n"
	"[audio]\n"
	"music_volume = 0.5\n";

static char folder[] = "/tmp/halo-config-tables-XXXXXX";

static void remove_folder(void)
{
	remove(config_file_path);
	rmdir(folder);
}

int main(int argc, char **argv)
{
	char const *case_name = argc > 1 ? argv[1] : "";

	CHECK(mkdtemp(folder), "no folder");
	atexit(remove_folder);
	snprintf(base_path, sizeof(base_path), "%s/", folder);
	snprintf(config_file_path, sizeof(config_file_path), "%sconfig.toml", base_path);

	/* the section's numbers, in the file's order; a value that is none is
	left out (and logged) */
	CASE("read-numbers")
	{
		config_file_write(example);
		CHECK(read_table() == 3, "%d numbers, not 3", read_count);
		CHECK(!strcmp(read_entries[0].key, "Player One") && read_entries[0].value == 2.0, "first %s = %g",
			read_entries[0].key, read_entries[0].value);
		CHECK(!strcmp(read_entries[1].key, "Ace") && read_entries[1].value == 0.75, "second %s = %g",
			read_entries[1].key, read_entries[1].value);
		CHECK(!strcmp(read_entries[2].key, "Literal") && read_entries[2].value == 3.0, "third %s = %g",
			read_entries[2].key, read_entries[2].value);
		CHECK(strstr(log_text, "Bad should be a number"), "Bad not reported: %s", log_text);
		return 0;
	}
	/* no file, or one that is no TOML: -1; none in the file: 0 */
	CASE("read-none")
	{
		CHECK(read_table() == -1, "no file read as %d", read_count);
		config_file_write("[display]\nvsync = true\n");
		CHECK(read_table() == 0, "no section read as %d", read_count);
		config_file_write("[handicap\nAce = 1\n");
		CHECK(read_table() == -1, "a broken file read as %d", read_count);
		config_file_write("handicap = 2\n");
		CHECK(read_table() == 0, "handicap not a section read as %d", read_count);
		return 0;
	}
	/* a key's line is replaced, in its own section only, the rest kept as
	it was */
	CASE("write-replaces")
	{
		static const char expected[] =
			"# Halo settings\n"
			"\n"
			"[display]\n"
			"vsync = true\n"
			"Ace = 1\n"
			"\n"
			"[handicap]\n"
			"# a comment\n"
			"\"Player One\" = 2.0\n"
			"\"Ace\" = 1.5\n"
			"'Literal' = 3\n"
			"Bad = \"two\"\n"
			"\n"
			"[audio]\n"
			"music_volume = 0.5\n";
		static const char expected_quoted[] =
			"# Halo settings\n"
			"\n"
			"[display]\n"
			"vsync = true\n"
			"Ace = 1\n"
			"\n"
			"[handicap]\n"
			"# a comment\n"
			"\"Player One\" = 0.5\n"
			"Ace = 0.75 # the best\n"
			"'Literal' = 3\n"
			"Bad = \"two\"\n"
			"\n"
			"[audio]\n"
			"music_volume = 0.5\n";

		check_write(example, "Ace", "Ace", 1.5, expected);
		check_write(example, "Player One", "Player One", 0.5, expected_quoted);
		return 0;
	}
	/* a new key goes at the end of the section's lines, before its blank
	lines and the next section; the section, if there is none, at the end */
	CASE("write-adds")
	{
		static const char expected[] =
			"# Halo settings\n"
			"\n"
			"[display]\n"
			"vsync = true\n"
			"Ace = 1\n"
			"\n"
			"[handicap]\n"
			"# a comment\n"
			"\"Player One\" = 2.0\n"
			"Ace = 0.75 # the best\n"
			"'Literal' = 3\n"
			"Bad = \"two\"\n"
			"\"Novice\" = 2.0\n"
			"\n"
			"[audio]\n"
			"music_volume = 0.5\n";

		check_write(example, NULL, "Novice", 2.0, expected);
		check_write("[display]\nvsync = true\n", NULL, "Novice", 2.0,
			"[display]\nvsync = true\n\n[handicap]\n\"Novice\" = 2.0\n");
		check_write("[display]\nvsync = true", NULL, "Novice", 2.0,
			"[display]\nvsync = true\n\n[handicap]\n\"Novice\" = 2.0\n");
		check_write("[handicap]\n# names\n\n\n", NULL, "Novice", 2.0, "[handicap]\n# names\n\"Novice\" = 2.0\n\n\n");
		check_write("[handicap]\r\nAce = 1\r\n\r\n[audio]\r\n", NULL, "Novice", 2.0,
			"[handicap]\r\nAce = 1\r\n\"Novice\" = 2.0\n\r\n[audio]\r\n");
		return 0;
	}
	/* the numbers as TOML reads them: with a point */
	CASE("write-numbers")
	{
		check_write("[handicap]\n", NULL, "A", 2.0, "[handicap]\n\"A\" = 2.0\n");
		check_write("[handicap]\n", NULL, "A", 1.5, "[handicap]\n\"A\" = 1.5\n");
		check_write("[handicap]\n", NULL, "A", 0.25, "[handicap]\n\"A\" = 0.25\n");
		check_write("[handicap]\n", NULL, "a=b", 4.0, "[handicap]\n\"a=b\" = 4.0\n");
		CHECK(read_table() == 1 && read_entries[0].value == 4.0, "read back %g", read_entries[0].value);
		return 0;
	}
	/* a name with quotes and backslashes is written as TOML reads it, and
	its line is found again */
	CASE("write-escapes")
	{
		char *text;

		check_write("[handicap]\n", NULL, "Mr \"Q\" \\o/", 2.0, "[handicap]\n\"Mr \\\"Q\\\" \\\\o/\" = 2.0\n");
		CHECK(read_table() == 1 && !strcmp(read_entries[0].key, "Mr \"Q\" \\o/"), "read back as %s",
			read_entries[0].key);
		CHECK(config_table_write_number("handicap", "Mr \"Q\" \\o/", "Mr \"Q\" \\o/", 3.0), "not written");
		text = config_file_text();
		CHECK(occurrences(text, "Mr") == 1, "written again as another line:\n%s", text);
		free(text);
		CHECK(read_table() == 1 && read_entries[0].value == 3.0, "read back %g", read_entries[0].value);
		return 0;
	}
	/* a new file has the section, empty, with its comment; reading the
	settings reports none of its keys as unknown, and does report others */
	CASE("defaults")
	{
		char *text;

		CHECK(config_boolean("display.vsync"), "vsync");
		text = config_file_text();
		CHECK(occurrences(text, "\n[handicap]\n") == 1, "no [handicap] in the new file:\n%s", text);
		CHECK(strstr(text, "# \"Player One\" = 2.0\n"), "no example in the new file");
		free(text);
		CHECK(read_table() == 0, "%d names in a new file", read_count);
		CHECK(!strstr(log_text, "unknown"), "a new file has unknown settings: %s", log_text);
		return 0;
	}
	CASE("not-unknown")
	{
		config_file_write("[display]\nvsync = true\ntypo = 1\n\n[handicap]\nAce = 1.0\n");
		config_boolean("display.vsync");
		CHECK(strstr(log_text, "unknown setting display.typo"), "the misspelt setting not reported: %s", log_text);
		CHECK(!strstr(log_text, "handicap.Ace"), "a handicap reported as a setting: %s", log_text);
		return 0;
	}
	/* an older file gets the section, once, at its end, its settings kept */
	CASE("added-to-old-file")
	{
		char *text;
		toml_result_t result;
		char *again;

		config_file_write("[display]\nvsync = false\n");
		CHECK(!config_boolean("display.vsync"), "vsync not kept");
		text = config_file_text();
		CHECK(occurrences(text, "\n[handicap]\n") == 1, "[handicap] not added once:\n%s", text);
		CHECK(strstr(text, "vsync = false\n"), "vsync not kept");
		result = toml_parse(text, (int)strlen(text));
		CHECK(result.ok, "not TOML: %s", result.errmsg);
		again = config_add_missing(text, result.toptab);
		CHECK(!again, "added again:\n%s", again);
		toml_free(result);
		free(text);
		return 0;
	}

	fprintf(stderr, "no case \"%s\"\n", case_name);
	return 2;
}
