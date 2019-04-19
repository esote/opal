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
#ifndef GLOBS_H
#define GLOBS_H

#include <cstdint>
#include <ncurses.h>
#include <vector>

#include "rand.h"

int constexpr WIDTH = 80;
int constexpr HEIGHT = 21;

int constexpr TUNNEL_STRENGTH = 85;

char constexpr PLAYER = '@';
char constexpr ROOM = '.';
char constexpr CORRIDOR = '#';
char constexpr ROCK = ' ';
char constexpr STAIR_UP = '<';
char constexpr STAIR_DN = '>';

enum color {
	black,
	blue,
	cyan,
	green,
	magenta,
	red,
	white,
	yellow
};

uint16_t constexpr SMART = 1 << 0;
uint16_t constexpr TELE = 1 << 1;
uint16_t constexpr TUNNEL = 1 << 2;
uint16_t constexpr ERRATIC = 1 << 3;

uint16_t constexpr PLAYER_TYPE = 1 << 9;

uint16_t constexpr BOSS = 1 << 4;
uint16_t constexpr DESTROY = 1 << 5;
uint16_t constexpr PASS = 1 << 6;
uint16_t constexpr PICKUP = 1 << 7;
uint16_t constexpr UNIQ = 1 << 8;

struct dice {
	uint64_t	base;
	uint64_t	dice;
	uint64_t	sides;
};

struct dungeon_thing {
	std::string	desc;
	std::string	name;
	uint64_t	speed;
	dice		dam;

	/* first color as ncurses COLOR_PAIR(COLOR_*) value */
	int		color;

	unsigned int	symb;
	uint8_t		rrty;
	uint8_t		x;
	uint8_t		y;
	bool		done;

	dungeon_thing() = default;

	dungeon_thing(dungeon_thing const &t)
	{
		color = t.color;
		desc = t.desc;
		name = t.name;
		dam = t.dam;
		symb = t.symb;
		rrty = t.rrty;
		speed = t.speed;
		x = 0;
		y = 0;
		done = false;
	}
};

struct npc : dungeon_thing {
	uint64_t	hp;
	uint64_t	p_count;
	uint64_t	turn;
	uint16_t	type;
	bool		dead;

	npc() = default;

	npc(npc const &n) : dungeon_thing(n)
	{
		type = n.type;
		hp = n.hp;
		turn = 0;
		p_count = 0;
		dead = n.dead;
	}
};

enum type {
	ammunition,
	amulet,
	armor,
	book,
	boots,
	cloak,
	container,
	flask,
	food,
	gloves,
	gold,
	helmet,
	light,
	offhand,
	ranged,
	ring,
	scroll_type, /* avoid conflict with ncurses */
	wand,
	weapon
};

struct obj : dungeon_thing {
	uint64_t	def;
	uint64_t	dodge;
	uint64_t	hit;
	uint64_t	val;
	uint64_t	weight;
	uint64_t	attr;
	type		obj_type;
	bool		art;

	obj() = default;

	obj(obj const &o) : dungeon_thing(o)
	{
		def = o.def;
		dodge = o.dodge;
		hit = o.hit;
		val = o.val;
		weight = o.weight;
		attr = o.attr;
		obj_type = o.obj_type;
		art = o.art;
	}
};

struct room {
	uint8_t	x;
	uint8_t	y;
	uint8_t	size_x;
	uint8_t	size_y;
};

struct stair {
	uint8_t	x;
	uint8_t	y;
};

struct tile {
	/* turn engine */
	npc	*n;
	obj	*o;

	uint8_t	h; /* hardness */
	chtype	c; /* character */

	uint8_t	x;
	uint8_t	y;

	/* dijkstra distance cost */
	int32_t	d;
	int32_t	dt;

	/* dijkstra valid node */
	bool	vd;
	bool	vdt;

	/* visited by PC */
	bool	v;
};

extern ranged_random rr;

extern npc player;

extern tile tiles[HEIGHT][WIDTH];

extern std::vector<npc> npcs_parsed;
extern std::vector<obj> objs_parsed;

#endif /* GLOBS_H */
