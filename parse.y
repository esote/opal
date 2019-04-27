%{
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
#include <cstring>
#include <unordered_map>

#include <err.h>

#include "globs.h"

extern int	yylex();

static void	yyerror(char const *const);

static dice	parse_dice(char *const);
static uint64_t	parse_dice_value(char *const);
static uint8_t	parse_rrty(char *const);

bool in_n;
bool in_o;

npc c_npc;
obj c_obj;

static int constexpr line_max = 77;

static std::unordered_map<std::string, int> const color_map = {
	{"BLACK", COLOR_PAIR(COLOR_BLACK)},
	{"BLUE", COLOR_PAIR(COLOR_BLUE)},
	{"CYAN", COLOR_PAIR(COLOR_CYAN)},
	{"GREEN", COLOR_PAIR(COLOR_GREEN)},
	{"MAGENTA", COLOR_PAIR(COLOR_MAGENTA)},
	{"RED", COLOR_PAIR(COLOR_RED)},
	{"WHITE", COLOR_PAIR(COLOR_WHITE)},
	{"YELLOW", COLOR_PAIR(COLOR_YELLOW)}
};

static std::unordered_map<std::string, uint16_t> const ability_map = {
	{"BOSS", BOSS},
	{"DESTROY", DESTROY},
	{"ERRATIC", ERRATIC},
	{"PASS", PASS},
	{"PICKUP", PICKUP},
	{"SMART", SMART},
	{"TELE", TELE},
	{"TUNNEL", TUNNEL},
	{"UNIQ", UNIQ}
};

static std::unordered_map<type, char> const type_symb_map = {
	{ammunition, '/'},
	{amulet, '"'},
	{armor, '['},
	{book, '?'},
	{boots, '\\'},
	{cloak, '('},
	{container, '%'},
	{flask, '!'},
	{food, ','},
	{gloves, '{'},
	{gold, '$'},
	{helmet, ']'},
	{light, '_'},
	{offhand, ')'},
	{ranged, '}'},
	{ring, '='},
	{scroll_type, '~'},
	{wand, '-'},
	{weapon, '|'}
};


static std::unordered_map<std::string, type> const type_map = {
	{"AMMUNITION", ammunition},
	{"AMULET", amulet},
	{"ARMOR", armor},
	{"BOOK", book},
	{"BOOTS", boots},
	{"CLOAK", cloak},
	{"CONTAINER", container},
	{"FLASK", flask},
	{"FOOD", food},
	{"GLOVES", gloves},
	{"GOLD", gold},
	{"HELMET", helmet},
	{"LIGHT", light},
	{"OFFHAND", offhand},
	{"RANGED", ranged},
	{"RING", ring},
	{"SCROLL", scroll_type},
	{"WAND", wand},
	{"WEAPON", weapon}
};

%}

%union {
	char *str;
}

%token BEGIN_NPC_FILE BEGIN_OBJ_FILE
%token BEGIN_NPC BEGIN_OBJ END
%token COLOR DAM DESC DESC_END NAME RRTY SPEED
%token ABIL HP SYMB
%token ART ATTR DEF DODGE HIT TYPE VAL WEIGHT

%token <str> ABILS
%token <str> BOOLEAN
%token <str> COLORS
%token <str> DESC_INNER
%token <str> STR
%token <str> TYPES

%%

file
	: BEGIN_NPC_FILE npcs
	| BEGIN_OBJ_FILE objs
	;

npcs
	: npc
	| npcs npc
	;

npc
	: BEGIN_NPC npc_keywords END
	;

npc_keywords
	: npc_keyword
	| npc_keywords npc_keyword
	;

npc_keyword
	: ABIL abil
	| COLOR COLORS	{ c_npc.color = color_map.at($2); }
	| DAM STR	{ c_npc.dam = parse_dice($2); }
	| DESC desc DESC_END
	| HP STR	{ c_npc.hp = parse_dice_value($2); }
	| NAME name
	| RRTY STR	{ c_npc.rrty = parse_rrty($2); }
	| SPEED STR	{ c_npc.speed = parse_dice_value($2); }
	| SYMB STR	{ c_npc.symb = $2[0]; }
	;

abil
	: ABILS		{ c_npc.type |= ability_map.at($1); }
	| abil ABILS	{ c_npc.type |= ability_map.at($2); }
	;

objs
	: obj
	| objs obj
	;

obj
	: BEGIN_OBJ obj_keywords END
	;

obj_keywords
	: obj_keyword
	| obj_keywords obj_keyword
	;

obj_keyword
	: ART BOOLEAN	{ c_obj.art = std::strcmp($2, "TRUE") == 0; }
	| ATTR STR	{ c_obj.attr = parse_dice_value($2); }
	| COLOR COLORS	{ c_obj.color = color_map.at($2); }
	| DAM STR	{ c_obj.dam = parse_dice($2); }
	| DEF STR	{ c_obj.def = parse_dice_value($2); }
	| DESC desc DESC_END
	| DODGE STR	{ c_obj.dodge = parse_dice_value($2); }
	| HIT STR	{ c_obj.hit = parse_dice_value($2); }
	| NAME name
	| RRTY STR	{ c_obj.rrty = parse_rrty($2); }
	| SPEED STR	{ c_obj.speed = parse_dice_value($2); }
	| TYPE TYPES	{
				c_obj.obj_type = type_map.at($2);
				c_obj.symb = type_symb_map.at(c_obj.obj_type);
			}
	| VAL STR	{ c_obj.val = parse_dice_value($2); }
	| WEIGHT STR	{ c_obj.val = parse_dice_value($2); }
	;

name
	: STR		{
				if (in_n) c_npc.name += $1;
				if (in_o) c_obj.name += $1;
			}
	| name STR	{
				if (in_n) c_npc.name = c_npc.name + " " + $2;
				if (in_o) c_obj.name = c_obj.name + " " + $2;
			}
	;

desc
	: DESC_INNER		{
					if (strlen($1) > line_max + 1) {
						yyerror("line too long");
					}
					if (in_n) c_npc.desc += $1;
					if (in_o) c_obj.desc += $1;
				}
	| desc DESC_INNER	{
					if (strlen($2) > line_max + 1) {
						yyerror("line too long");
					}
					if (in_n) c_npc.desc += $2;
					if (in_o) c_obj.desc += $2;
				}
	;

%%

static void
yyerror(char const *const s)
{
	errx(1, "%s", s);
}


static dice
parse_dice(char *const s)
{
	dice d;
	char *p, *last;

	p = strtok_r(s, "+", &last);

	if (p == NULL) {
		errx(1, "dice formatted incorrectly");
	}

	d.base = (uint64_t)std::atoll(p);

	p = strtok_r(NULL, "d", &last);

	if (p == NULL) {
		errx(1, "dice formatted incorrectly");
	}

	d.dice = (uint64_t)std::atoll(p);

	p = strtok_r(NULL, "\0", &last);

	if (p == NULL) {
		errx(1, "dice format missing '+'");
	}

	d.sides = (uint64_t)std::atoll(p);

	if (d.sides == 1 && d.dice != 0) {
		errx(1, "instead of using multiple 1-sided die, add them as "
			"the base");
	}

	return d;
}

static uint64_t
parse_dice_value(char *const s)
{
	dice const d = parse_dice(s);
	return rr.rand_dice<uint64_t>(d.base, d.dice, d.sides);
}

static uint8_t
parse_rrty(char *const s)
{
	uint8_t const rrty = (uint8_t)std::atoi(s);

	if (rrty == 0 || rrty > 100) {
		errx(1, "rrty '%d' out of bounds [1, 100]", rrty);
	}

	return rrty;
}
