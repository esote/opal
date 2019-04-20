/*
 * OPAL's playable almost indefectibly.
 * Copyright (C) 2019  Esote
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#include <endian.h>
#undef _BSD_SOURCE
#undef _DEFAULT_SOURCE

#include <sys/stat.h>

#include <err.h>

#include "floor.h"
#include "globs.h"

static bool	save_things(FILE *const);
static bool	load_things(FILE *const);

static void	init_fresh();
static void	place_player();
static int	valid_player(int const, int const);

static char const *const DIRECTORY = "/.opal";
static char const *const FILEPATH = "/dungeon";
static int constexpr DF_L = 8;

static char const *const MARK = "OPAL-DUNGEON";
static int constexpr MARK_L = 12;

static int constexpr NEW_ROOM_COUNT = 8;
static int constexpr ROOM_RETRIES = 150;

static std::vector<stair> stairs_up;
static std::vector<stair> stairs_dn;

static uint16_t room_count;
static std::vector<room> rooms;

static uint16_t stair_up_count;
static uint16_t stair_dn_count;

tile tiles[HEIGHT][WIDTH];

std::string
opal_path()
{
	char *home;
#ifdef _GNU_SOURCE
	home = secure_getenv("HOME");
#else
	home = getenv("HOME");
#endif

	if (home == NULL) {
#ifdef _GNU_SOURCE
		errx(1, "getenv");
#else
		errx(1, "secure_getenv");
#endif
	}

	return std::string(home) + DIRECTORY;
}

bool
save_dungeon()
{
	struct stat st;
	FILE *f;
	bool ret;

	std::string path = opal_path();

	if (stat(path.c_str(), &st) == -1) {
		if (errno == ENOENT) {
			if (mkdir(path.c_str(), 0700) == -1) {
				err(1, "save mkdir");
			}
		} else {
			err(1, "save stat");
		}
	}

	path += FILEPATH;

	if (!(f = fopen(path.c_str(), "w"))) {
		err(1, "save fopen");
	}

	ret = save_things(f);

	if (fclose(f) == EOF) {
		err(1, "save fclose, (save_things=%d)", ret);
	}

	return ret;
}

bool
load_dungeon()
{
	FILE *f;
	bool ret;
	std::string path = opal_path() + FILEPATH;

	if (!(f = fopen(path.c_str(), "r"))) {
		err(1, "load fopen");
	}

	ret = load_things(f);

	if (fclose(f) == EOF) {
		err(1, "load fclose, (load_things=%d)", ret);
	}

	return ret;
}

void
clear_tiles()
{
	for (uint8_t i = 0; i < HEIGHT; ++i) {
		for (uint8_t j = 0; j < WIDTH; ++j) {
			tiles[i][j] = {};
			tiles[i][j].x = j;
			tiles[i][j].y = i;

			if (i == 0 || j == 0 || i == HEIGHT - 1
				|| j == WIDTH - 1) {
				tiles[i][j].h = std::numeric_limits<uint8_t>::max();
				tiles[i][j].d = std::numeric_limits<int32_t>::max();
				tiles[i][j].dt = std::numeric_limits<int32_t>::max();
			} else {
				tiles[i][j].c = ROCK;
				tiles[i][j].h = rr.rrand<uint8_t>(1,
					std::numeric_limits<uint8_t>::max() - 1);
			}
		}
	}
}

void
arrange_new()
{
	room_count = NEW_ROOM_COUNT;
	stair_up_count = rr.rrand<uint16_t>(1, (uint16_t)((room_count / 4) + 1));
	stair_dn_count = rr.rrand<uint16_t>(1, (uint16_t)((room_count / 4) + 1));

	rooms.resize(room_count);
	stairs_up.resize(stair_up_count);
	stairs_dn.resize(stair_dn_count);

	init_fresh();
}

void
arrange_loaded()
{
	for (auto const &r : rooms) {
		draw_room(r);
	}

	for (auto const &s : stairs_up) {
		tiles[s.y][s.x].c = STAIR_UP;
	}

	for (auto const &s : stairs_dn) {
		tiles[s.y][s.x].c = STAIR_DN;
	}

	for (int i = 1; i < HEIGHT - 1; ++i) {
		for (int j = 1; j < WIDTH - 1; ++j) {
			if (tiles[i][j].h == 0 && tiles[i][j].c == ROCK) {
				tiles[i][j].c = CORRIDOR;
			}
		}
	}
}

void
arrange_renew()
{
	clear_tiles();

	rooms.clear();
	stairs_up.clear();
	stairs_dn.clear();

	arrange_new();
}

static bool
save_things(FILE *const f)
{
	uint32_t const ver = htobe32(0);
	uint32_t const filesize = htobe32((uint32_t)(1708 + (room_count * 4)
		+ (stair_up_count * 2) + (stair_dn_count * 2)));

	/* type marker */
	if (fwrite(MARK, MARK_L, 1, f) != 1) {
		return false;
	}

	/* version */
	if (fwrite(&ver, sizeof(uint32_t), 1, f) != 1) {
		return false;
	}

	/* size */
	if (fwrite(&filesize, sizeof(uint32_t), 1, f) != 1) {
		return false;
	}

	/* player coords */
	if (fwrite(&player.x, sizeof(uint8_t), 1, f) != 1) {
		return false;
	}
	if (fwrite(&player.y, sizeof(uint8_t), 1, f) != 1) {
		return false;
	}

	/* hardness */
	for (std::size_t i = 0; i < HEIGHT; ++i) {
		for (std::size_t j = 0; j < WIDTH; ++j) {
			if (fwrite(&tiles[i][j].h, sizeof(uint8_t), 1, f) != 1) {
				return false;
			}
		}
	}

	/* room num */
	room_count = htobe16(room_count);
	if (fwrite(&room_count, sizeof(uint16_t), 1, f) != 1) {
		return false;
	}
	room_count = be16toh(room_count);

	/* room data */
	for (auto const &r : rooms) {
		if (fwrite(&r, sizeof(room), 1, f) != 1) {
			return false;
		}
	}

	/* stairs_up num */
	stair_up_count = htobe16(stair_up_count);
	if (fwrite(&stair_up_count, sizeof(uint16_t), 1, f) != 1) {
		return false;
	}
	stair_up_count = be16toh(stair_up_count);

	/* stars_up coords */
	for (auto const &s : stairs_up) {
		if (fwrite(&s, sizeof(stair), 1, f) != 1) {
			return false;
		}
	}

	/* stairs_dn num */
	stair_dn_count = htobe16(stair_dn_count);
	if (fwrite(&stair_dn_count, sizeof(uint16_t), 1, f) != 1) {
		return false;
	}
	stair_dn_count = be16toh(stair_dn_count);

	/* stairs_dn coords */
	for (auto const &s : stairs_dn) {
		if (fwrite(&s, sizeof(stair), 1, f) != 1) {
			return false;
		}
	}

	return true;
}

static bool
load_things(FILE *const f)
{
	/* skip type marker, version, and size */
	if (fseek(f, MARK_L + 2 * sizeof(uint32_t), SEEK_SET) == -1) {
		return false;
	}

	/* player coords */
	if (fread(&player.x, sizeof(uint8_t), 1, f) != 1) {
		return false;
	}
	if (fread(&player.y, sizeof(uint8_t), 1, f) != 1) {
		return false;
	}

	/* hardness */
	for (std::size_t i = 0; i < HEIGHT; ++i) {
		for (std::size_t j = 0; j < WIDTH; ++j) {
			if (fread(&tiles[i][j].h, sizeof(uint8_t), 1, f) != 1) {
				return false;
			}
		}
	}

	/* room num */
	if (fread(&room_count, sizeof(uint16_t), 1, f) != 1) {
		return false;
	}
	room_count = be16toh(room_count);

	rooms.resize(room_count);

	/* room data */
	for (auto &r : rooms) {
		if (fread(&r, sizeof(room), 1, f) != 1) {
			return false;
		}
	}

	/* stair_up num */
	if (fread(&stair_up_count, sizeof(uint16_t), 1, f) != 1) {
		return false;
	}
	stair_up_count = be16toh(stair_up_count);

	stairs_up.resize(stair_up_count);

	/* stair_up coords */
	for (auto &s : stairs_up) {
		if (fread(&s, sizeof(stair), 1, f) != 1) {
			return false;
		}
	}

	/* stair_dn num */
	if (fread(&stair_dn_count, sizeof(uint16_t), 1, f) != 1) {
		return false;
	}
	stair_dn_count = be16toh(stair_dn_count);

	stairs_dn.resize(stair_dn_count);

	/* stair_dn_coords */
	for (auto &s : stairs_dn) {
		if (fread(&s, sizeof(stair), 1, f) != 1) {
			return false;
		}
	}

	return true;
}

static void
init_fresh()
{
	std::size_t i = 0;
	std::size_t retries = 0;

	for (auto it = rooms.begin(); it != rooms.end()
		&& retries < ROOM_RETRIES; ++it) {
		if (!gen_room(*it)) {
			retries++;
			it--;
		} else {
			i++;
			draw_room(*it);
		}
	}

	if (i < room_count) {
		if (i == 0) {
			errx(1, "unable to place any rooms");
		}

		room_count = (uint16_t)i;
		rooms.resize(room_count);
	}

	for (i = 0; i < room_count - 1U; ++i) {
		gen_corridor(rooms[i], rooms[i+1]);
	}

	for (auto &s : stairs_up) {
		gen_stair(s, true);
	}

	for (auto &s : stairs_dn) {
		gen_stair(s, true);
	}

	place_player();
}

static void
place_player()
{
	uint8_t x, y;

	do {
		x = rr.rrand<uint8_t>(1, WIDTH - 2);
		y = rr.rrand<uint8_t>(1, HEIGHT - 2);
	} while (!valid_player(y, x));

	player.x = x;
	player.y = y;
}

static int
valid_player(int const y, int const x)
{
	return tiles[y][x].h == 0
		&& tiles[y + 1][x].h == 0 && tiles[y - 1][x].h == 0
		&& tiles[y][x + 1].h == 0 && tiles[y][x - 1].h == 0;
}
