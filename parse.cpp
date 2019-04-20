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
#include <iostream>

#include <err.h>
#include <sys/stat.h>

#include "gen.h"
#include "globs.h"
#include "y.tab.h"

static char const *const NPC_FILE = "/npc_desc";
static char const *const OBJ_FILE = "/obj_desc";

static char const *const color_map_r[] = {
	"BLACK",
	"BLUE",
	"CYAN",
	"GREEN",
	"MAGENTA",
	"RED",
	"WHITE",
	"YELLOW"
};

static char const *const type_map_r[] = {
	"AMMUNITION",
	"AMULET",
	"ARMOR",
	"BOOK",
	"BOOTS",
	"CLOAK",
	"CONTAINER",
	"FLASK",
	"FOOD",
	"GLOVES",
	"GOLD",
	"HELMET",
	"LIGHT",
	"OFFHAND",
	"RANGED",
	"RING",
	"SCROLL",
	"WAND",
	"WEAPON"
};

extern int	yyparse();
extern FILE	*yyin;

std::vector<npc> npcs_parsed;
std::vector<obj> objs_parsed;

constexpr static int const line_max = 77;

void
parse_npc_file()
{
	struct stat st;
	std::string const path = opal_path() + NPC_FILE;

	if (stat(path.c_str(), &st) == -1) {
		err(1, "npc description file");
	}

	yyin = fopen(path.c_str(), "r");

	if (yyin == NULL) {
		err(1, "npc file fopen");
	}

	if (yyparse() != 0) {
		errx(1, "yyparse npcs");
	}

	if (fclose(yyin) == EOF) {
		err(1, "npc fclose");
	}
}

void
parse_obj_file()
{
	struct stat st;
	std::string const path = opal_path() + OBJ_FILE;

	if (stat(path.c_str(), &st) == -1) {
		err(1, "object description file");
	}

	yyin = fopen(path.c_str(), "r");

	if (yyin == NULL) {
		err(1, "object file fopen");
	}

	if (yyparse() != 0) {
		errx(1, "yyparse objs");
	}

	if (fclose(yyin) == EOF) {
		err(1, "object fclose");
	}
}
