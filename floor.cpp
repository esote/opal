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
#include "globs.h"

static bool	valid_room(room const &);
static int	valid_corridor_x(int const, int const);
static int	valid_corridor_y(int const, int const);
static int	valid_stair(int const, int const);

static int constexpr MINROOMH = 4;
static int constexpr MINROOMW = 3;
static int constexpr MAXROOMH = 8;
static int constexpr MAXROOMW = 15;

bool
gen_room(room &r)
{
	r.x = rr.rrand<uint8_t>(1, WIDTH - 2);
	r.y = rr.rrand<uint8_t>(1, HEIGHT - 2);
	r.size_x = rr.rrand<uint8_t>(MINROOMW, MAXROOMW);
	r.size_y = rr.rrand<uint8_t>(MINROOMH, MAXROOMH);

	return valid_room(r);
}

void
draw_room(room const &r)
{
	for (int i = r.x; i < r.x + r.size_x; ++i) {
		for (int j = r.y; j < r.y + r.size_y; ++j) {
			tiles[j][i].c = ROOM;
			tiles[j][i].h = 0;
		}
	}
}


void
gen_corridor(room const &r1, room const &r2)
{
	for (int i = std::min(r1.x, r2.x); i <= std::max(r1.x, r2.x); ++i) {
		if (valid_corridor_y(r1.y, i)) {
			tiles[r1.y][i].c = CORRIDOR;
			tiles[r1.y][i].h = 0;
		}
	}

	for (int i = std::min(r1.y, r2.y); i <= std::max(r1.y, r2.y); ++i) {
		if (valid_corridor_x(i, r2.x)) {
			tiles[i][r2.x].c = CORRIDOR;
			tiles[i][r2.x].h = 0;
		}
	}
}

void
gen_stair(stair &s, bool const up)
{
	uint8_t x, y;

	do {
		x = rr.rrand<uint8_t>(1, WIDTH - 2);
		y = rr.rrand<uint8_t>(1, HEIGHT - 2);
	} while (!valid_stair(y, x));

	tiles[y][x].c = up ? STAIR_UP : STAIR_DN;
	tiles[y][x].h = 0;

	s.x = x;
	s.y = y;
}

static bool
valid_room(room const &r)
{
	for (int i = r.x - 1; i <= r.x + r.size_x + 1; ++i) {
		for (int j = r.y - 1; j <= r.y + r.size_y + 1; ++j) {
			if (tiles[j][i].c != ROCK) {
				return false;
			}
		}
	}

	return true;
}

static int
valid_corridor_x(int const y, int const x)
{
	return tiles[y][x].c == ROCK
		&& tiles[y][x + 1].c != CORRIDOR
		&& tiles[y][x - 1].c != CORRIDOR;
}

static int
valid_corridor_y(int const y, int const x)
{
	return tiles[y][x].c == ROCK
		&& tiles[y + 1][x].c != CORRIDOR
		&& tiles[y - 1][x].c != CORRIDOR;
}

static int
valid_stair(int const y, int const x)
{
	return (tiles[y][x].c == ROCK || tiles[y][x].c == ROOM)
		&& (tiles[y + 1][x].c == CORRIDOR
		|| tiles[y - 1][x].c == CORRIDOR
		|| tiles[y][x + 1].c == CORRIDOR
		|| tiles[y][x - 1].c == CORRIDOR);
}
