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
#include <algorithm>
#include <cinttypes>
#include <functional>
#include <limits>
#include <new>
#include <queue>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

#include <err.h>

#include "dijk.h"
#include "globs.h"
#include "turn.h"

static bool	valid_thing(uint8_t const, uint8_t const);

static double		distance(uint8_t const, uint8_t const, uint8_t const, uint8_t const);
static unsigned int	subu32(unsigned int const, unsigned int const);
static uint64_t		subu64(uint64_t const, uint64_t const);

static bool	pc_visible(int const, int const);

static void	npc_obj_or_tile(WINDOW *const, uint8_t const, uint8_t const);

static uint64_t	effective_dam();
static uint64_t	combat(npc &, npc &);

static void	move_redraw(WINDOW *const, npc &, uint8_t const, uint8_t const);
static void	move_logic(WINDOW *const, npc &, uint8_t const, uint8_t const);
static void	move_tunnel(WINDOW *const, npc &, uint8_t const, uint8_t const);

static void	move_straight(WINDOW *const, npc &);
static void	move_dijk_nontunneling(WINDOW *const, npc &);
static void	move_dijk_tunneling(WINDOW *const, npc &);

static std::optional<std::pair<uint8_t, uint8_t>>	gen_npc();
static std::optional<std::pair<uint8_t, uint8_t>>	gen_obj();

static void	npc_list(WINDOW *const, std::vector<npc *> const &);

static void	defog(WINDOW *const);

static void	crosshair(WINDOW *const, uint8_t const, uint8_t const);
static bool	inspect(WINDOW *const, bool const);

static bool	viewable(int const, int const);
static void	pc_viewbox(WINDOW *const, int const);

static void	try_carry(uint8_t const, uint8_t const);

static void	equip_list(WINDOW *const, bool const);

static void	carry_to_equip(int const);
static void	equip_to_carry(int const, std::optional<std::string> &);

static void	thing_details(WINDOW *const, dungeon_thing const &);

enum pc_action {
	PC_DEFOG,
	PC_NEXT,
	PC_NONE,
	PC_NPC_LIST,
	PC_QUIT,
	PC_RETRY,
	PC_TELE
};

static enum pc_action	turn_npc(WINDOW *const, WINDOW *const, npc &);
static enum pc_action	turn_pc(WINDOW *const, WINDOW *const, npc &);

enum carry_action {
	CARRY_DROP,
	CARRY_INSPECT,
	CARRY_LIST,
	CARRY_REMOVE,
	CARRY_WEAR
};

static void	carry_list(WINDOW *const, carry_action const);

struct equip {
	std::optional<obj>	amulet;
	std::optional<obj>	armor;
	std::optional<obj>	boots;
	std::optional<obj>	cloak;
	std::optional<obj>	gloves;
	std::optional<obj>	helmet;
	std::optional<obj>	light;
	std::optional<obj>	offhand;
	std::optional<obj>	ranged;
	std::optional<obj>	ring_left;
	std::optional<obj>	ring_right;
	std::optional<obj>	weapon;
};

static char const *const type_map_name[] = {
	"ammunition",
	"amulet",
	"armor",
	"book",
	"boots",
	"cloak",
	"container",
	"flask",
	"food",
	"gloves",
	"gold",
	"helmet",
	"light",
	"offhand",
	"ranged",
	"ring",
	"scroll",
	"wand",
	"weapon"
};

static bool type_map_equip[] = {
	false,
	true,
	true,
	false,
	true,
	true,
	false,
	false,
	false,
	true,
	false,
	true,
	true,
	true,
	true,
	true,
	false,
	false,
	true
};

struct compare_npc {
	bool constexpr
	operator() (npc const &x, npc const &y) const
	{
		return x.turn > y.turn;
	}
};

/* minimum distance from the PC an NPC can be placed */
static double constexpr CUTOFF = 4.0;
static int constexpr PERSISTANCE = 5;
static int constexpr KEY_ESC = 27;
static int constexpr DEFAULT_LUMINANCE = 5;
static unsigned int constexpr RETRIES = 150;
static int constexpr PC_CARRY_MAX = 10;

static std::optional<obj> pc_carry[PC_CARRY_MAX];
static equip pc_equip;

enum turn_exit
turn_engine(WINDOW *const win, unsigned int const numnpcs,
	unsigned int const numobjs)
{
	std::priority_queue<npc, std::vector<std::reference_wrapper<npc>>, compare_npc> heap;
	std::vector<npc *> npcs;
	std::vector<obj *> objs;
	unsigned int real_num = 0;

	WINDOW *sep;

	uint64_t turn;
	enum turn_exit ret = TURN_NONE;

	try {
		npcs.resize(numnpcs);
		objs.resize(numobjs);
	} catch (std::bad_alloc const &) {
		err(1, "resize npcs and objs");
	}

	tiles[player.y][player.x].n = &player;

	wattron(win, player.color);
	(void)mvwaddch(win, player.y, player.x, player.symb);
	wattroff(win, player.color);

	heap.push(player);

	for (auto &n : npcs) {
		size_t i;
		unsigned int retries = 0;
		do {
			i = rr.rrand<size_t>(0, npcs_parsed.size() - 1);
			retries++;
		} while (retries < RETRIES && (npcs_parsed[i].done
			|| npcs_parsed[i].rrty <= rr.rrand<uint8_t>(0, 99)));

		if (retries == RETRIES) {
			break;
		}

		std::optional<std::pair<uint8_t, uint8_t>> coords = gen_npc();

		if (!coords.has_value()) {
			break;
		}

		real_num++;

		n = new npc(npcs_parsed[i]);

		if (n->type & UNIQ) {
			n->done = true;
			npcs_parsed[i].done = true;
		}

		n->x = coords->first;
		n->y = coords->second;

		tiles[n->y][n->x].n = n;

		heap.push(*n);
	}

	if (real_num != numnpcs) {
		npcs.resize(real_num);
	}

	real_num = 0;

	for (auto &o : objs) {
		size_t i = 0;
		unsigned int retries = 0;
		do {
			i = rr.rrand<size_t>(0, objs_parsed.size() - 1);
			retries++;
		} while (retries < RETRIES && (objs_parsed[i].done
			|| objs_parsed[i].rrty <= rr.rrand<uint8_t>(0, 99)));

		if (retries == RETRIES) {
			break;
		}

		std::optional<std::pair<uint8_t, uint8_t>> coords = gen_obj();

		if (!coords.has_value()) {
			break;
		}

		real_num++;

		o = new obj(objs_parsed[i]);

		if (o->art) {
			o->done = true;
			objs_parsed[i].done = true;
		}

		o->x = coords->first;
		o->y = coords->second;

		tiles[o->y][o->x].o = o;
	}

	if (real_num != numobjs) {
		objs.resize(real_num);
	}

	dijkstra();

	if ((sep = newwin(HEIGHT, WIDTH, 0, 0)) == NULL) {
		errx(1, "newwin sep");
	}

	if (keypad(sep, true) == ERR) {
		errx(1, "keypad sep");
	}

	(void)box(sep, 0, 0);

	pc_viewbox(win, DEFAULT_LUMINANCE);

	(void)mvwprintw(win, HEIGHT - 1, 2,
		"[ hp: %" PRIu64 " ]", player.hp);

	while (!heap.empty()) {
		npc &n = heap.top();
		heap.pop();

		if (n.type & PLAYER_TYPE && wrefresh(win) == ERR) {
			errx(1, "turn_engine wrefresh");
		}

		if (n.hp == 0) {
			if (n.type & PLAYER_TYPE) {
				ret = TURN_DEATH;
				goto exit;
			} else if (n.type & BOSS) {
				ret = TURN_WIN;
				goto exit;
			} else {
				continue;
			}
		}

		turn = n.turn + 1;
		n.turn = turn + 1000/n.speed;

		retry:
		if (touchwin(win) == ERR) {
			errx(1, "touchwin");
		}

		switch(turn_npc(win, sep, n)) {
		case PC_DEFOG:
			defog(sep);
			goto retry;
		case PC_NEXT:
			ret = TURN_NEXT;
			goto exit;
		case PC_NONE:
			break;
		case PC_NPC_LIST:
			npc_list(sep, npcs);
			goto retry;
		case PC_QUIT:
			ret = TURN_QUIT;
			goto exit;
		case PC_RETRY:
			goto retry;
		case PC_TELE:
			if (inspect(win, true)) {
				break;
			} else {
				goto retry;
			}
		}

		heap.push(n);
	}

	exit:

	for (auto &n : npcs) {
		delete n;
	}

	for (auto &o : objs) {
		delete o;
	}

	if (delwin(sep) == ERR) {
		errx(1, "turn_engine delwin sep");
	}

	return ret;
}

static bool
valid_thing(uint8_t const y, uint8_t const x)
{
	if (tiles[y][x].h != 0) {
		return false;
	}

	return distance(player.x, player.y, x, y) > CUTOFF;
}

static double
distance(uint8_t const x0, uint8_t const y0, uint8_t const x1, uint8_t const y1)
{
	int const dx = x1 - x0;
	int const dy = y1 - y0;
	return std::sqrt(dx * dx + dy * dy);
}

/*
 * From my branchfree saturating arithmetic library.
 * See: https://github.com/esote/bsa
 */
static unsigned int
subu32(unsigned int const a, unsigned int const b)
{
	unsigned int res = a - b;
	res &= (unsigned int) (-(unsigned int) (res <= a));
	return res;
}

static uint64_t
subu64(uint64_t const a, uint64_t const b)
{
	uint64_t res = a - b;
	res &= (uint64_t) (-(uint64_t) (res <= a));
	return res;
}

/* Bresenham's line algorithm */
static bool
pc_visible(int const x1, int const y1)
{
	int x0 = player.x;
	int y0 = player.y;

	int const dx = std::abs(x1 - x0);
	int const dy = std::abs(y1 - y0);
	int const sx = x0 < x1 ? 1 : -1;
	int const sy = y0 < y1 ? 1 : -1;

	int err = (dx > dy ? dx : -dy) / 2;

	while (1) {
		if (tiles[y0][x0].h != 0) {
			return false;
		}

		if (x0 == x1 && y0 == y1) {
			break;
		}

		int e2 = err;

		if (e2 > -dx) {
			err -= dy;
			x0 += sx;
		}

		if (e2 < dy) {
			err += dx;
			y0 += sy;
		}
	}

	return true;
}

static void
npc_obj_or_tile(WINDOW *const win, uint8_t const y, uint8_t const x)
{
	if (tiles[y][x].n != NULL) {
		wattron(win, tiles[y][x].n->color);
		(void)mvwaddch(win, y, x, tiles[y][x].n->symb);
		wattroff(win, tiles[y][x].n->color);
	} else if (tiles[y][x].o != NULL) {
		wattron(win, tiles[y][x].o->color);
		(void)mvwaddch(win, y, x, tiles[y][x].o->symb);
		wattroff(win, tiles[y][x].o->color);
	} else {
		(void)mvwaddch(win, y, x, tiles[y][x].c);
	}
}

static uint64_t
effective_dam()
{
	int const length = 12;
	std::optional<obj> const equip[] = {
		pc_equip.amulet,
		pc_equip.armor,
		pc_equip.boots,
		pc_equip.cloak,
		pc_equip.gloves,
		pc_equip.helmet,
		pc_equip.light,
		pc_equip.offhand,
		pc_equip.ranged,
		pc_equip.ring_left,
		pc_equip.ring_right,
		pc_equip.weapon
	};

	uint64_t dam = rr.rand_dice<uint64_t>(player.dam.base, player.dam.dice,
		player.dam.sides);

	for (int i = 0; i < length; ++i) {
		if (equip[i].has_value()) {
			dam += rr.rand_dice<uint64_t>(equip[i]->dam.base,
				equip[i]->dam.dice, equip[i]->dam.sides);
		}
	}

	return dam;
}

static uint64_t
combat(npc &n1, npc &n2)
{
	uint64_t n1_dam = rr.rand_dice<uint64_t>(n1.dam.base, n1.dam.dice,
		n1.dam.sides);
	uint64_t n2_hp = n2.hp;

	if (n1.type & PLAYER_TYPE) {
		n1_dam = effective_dam();
	} else {
		n2_hp = player.hp;
	}

	n2.hp = subu64(n2_hp, n1_dam);

	return n1_dam;
}

static void
move_redraw(WINDOW *const win, npc &n, uint8_t const y, uint8_t const x)
{
	tiles[n.y][n.x].n = NULL;
	tiles[y][x].n = &n;

	if (tiles[n.y][n.x].v || n.type & PLAYER_TYPE) {
		npc_obj_or_tile(win, n.y, n.x);
	}

	if (tiles[y][x].v) {
		wattron(win, n.color);
		(void)mvwaddch(win, y, x, n.symb);
		wattroff(win, n.color);
	}

	n.y = y;
	n.x = x;
}

static void
move_logic(WINDOW *const win, npc &n, uint8_t const y, uint8_t const x)
{
	if (n.y == y && n.x == x) {
		return;
	}

	/* move to empty tile */
	if (tiles[y][x].n == NULL) {
		move_redraw(win, n, y, x);
		return;
	}

	/* npc-pc combat */
	if (n.type & PLAYER_TYPE || tiles[y][x].n->type & PLAYER_TYPE) {
		uint64_t dam = combat(n, *tiles[y][x].n);

		(void)box(win, 0, 0);
		(void)mvwprintw(win, HEIGHT - 1, 2,
			"[ hp: %" PRIu64 " ]", player.hp);

		if (n.type & PLAYER_TYPE) {
			(void)mvwprintw(win, HEIGHT - 1, WIDTH / 4,
				"[ delt %" PRIu64 " damage ]", dam);
		} else {
			(void)mvwprintw(win, HEIGHT - 1, WIDTH / 4,
				"[ received %" PRIu64 " damage ]", dam);
		}

		if (tiles[y][x].n->hp == 0) {
			tiles[y][x].n->dead = true;
			tiles[y][x].n = NULL;
			npc_obj_or_tile(win, y, x);
		}

		return;
	}

	/* npc-to-npc */
	for (int i = -1; i <= 1; ++i) {
		for (int j = -1; j <= 1; ++j) {
			uint8_t tx = (uint8_t)(tiles[y][x].n->x + i);
			uint8_t ty = (uint8_t)(tiles[y][x].n->y + j);

			if (tx == 0 || ty == 0 || tx >= WIDTH - 1
				|| ty >= HEIGHT - 1) {
				continue;
			}

			if (tiles[ty][tx].n == NULL && tiles[ty][tx].h == 0) {
				/* move to tiles[y][x].n to ty, tx */
				move_redraw(win, *tiles[y][x].n, ty, tx);
				move_redraw(win, n, y, x);
				return;
			}
		}
	}

	/* swap tiles[y][x].n with n */
	move_redraw(win, *tiles[y][x].n, n.y, n.x);
	move_redraw(win, n, y, x);
}

static void
move_tunnel(WINDOW *const win, npc &n, uint8_t const y, uint8_t const x)
{
	if (tiles[y][x].h == UINT8_MAX) {
		return;
	}

	tiles[y][x].h = (uint8_t)subu32(tiles[y][x].h, TUNNEL_STRENGTH);

	dijkstra();

	if (tiles[y][x].h != 0) {
		return;
	}

	if (tiles[y][x].c == ROCK) {
		tiles[y][x].c = CORRIDOR;
	}

	move_logic(win, n, y, x);
}

static void
move_straight(WINDOW *const win, npc &n)
{
	double min = std::numeric_limits<double>::max();
	uint8_t minx = n.x;
	uint8_t miny = n.y;

	for (int i = -1; i <= 1; ++i) {
		for (int j = -1; j <= 1; ++j) {
			uint8_t x = (uint8_t)(n.x + i);
			uint8_t y = (uint8_t)(n.y + j);

			if (!(n.type & TUNNEL) && tiles[y][x].h != 0) {
				continue;
			}

			double dist = distance(player.x, player.y, x, y);

			if (dist < min) {
				min = dist;
				minx = x;
				miny = y;
			}
		}
	}

	if (n.type & TUNNEL) {
		move_tunnel(win, n, miny, minx);
	} else {
		move_logic(win, n, miny, minx);
	}
}

static void
move_dijk_nontunneling(WINDOW *const win, npc &n)
{
	int32_t min_d = tiles[n.y][n.x].d;
	uint8_t minx = n.x;
	uint8_t miny = n.y;

	for (int i = -1; i <= 1; ++i) {
		for (int j = -1; j <= 1; ++j) {
			uint8_t x = (uint8_t)(n.x + i);
			uint8_t y = (uint8_t)(n.y + j);


			if (tiles[y][x].h != 0) {
				continue;
			}

			if (tiles[y][x].d < min_d) {
				min_d = tiles[y][x].d;
				minx = x;
				miny = y;
			}
		}
	}

	move_logic(win, n, miny, minx);
}

static void
move_dijk_tunneling(WINDOW *const win, npc &n)
{
	int32_t min_dt = tiles[n.y][n.x].dt;
	uint8_t minx = n.x;
	uint8_t miny = n.y;

	for (int i = -1; i <= 1; ++i) {
		for (int j = -1; j <= 1; ++j) {
			uint8_t x = (uint8_t)(n.x + i);
			uint8_t y = (uint8_t)(n.y + j);

			if (tiles[y][x].dt < min_dt) {
				min_dt = tiles[y][x].dt;
				minx = x;
				miny = y;
			}
		}
	}

	move_tunnel(win, n, miny, minx);
}

static std::optional<std::pair<uint8_t, uint8_t>>
gen_npc()
{
	uint8_t x, y;
	size_t retries = 0;

	do {
		x = rr.rrand<uint8_t>(1, WIDTH - 2);
		y = rr.rrand<uint8_t>(1, HEIGHT - 2);
		retries++;
	} while (retries < RETRIES && (!valid_thing(y, x)
		|| tiles[y][x].n != NULL));

	if (retries == RETRIES) {
		return {};
	}

	return std::make_pair(x, y);
}

static std::optional<std::pair<uint8_t, uint8_t>>
gen_obj()
{
	uint8_t x, y;
	size_t retries = 0;

	do {
		x = rr.rrand<uint8_t>(1, WIDTH - 2);
		y = rr.rrand<uint8_t>(1, HEIGHT - 2);
		retries++;
	} while (retries < RETRIES && (!valid_thing(y, x)
		|| tiles[y][x].o != NULL));

	if (retries == RETRIES) {
		return {};
	}

	return std::make_pair(x, y);
}

static enum pc_action
turn_npc(WINDOW *const win, WINDOW *const sep, npc &n)
{
	if (n.type & PLAYER_TYPE) {
		pc_viewbox(win, DEFAULT_LUMINANCE);
		return turn_pc(win, sep, n);
	}

	if (n.type & ERRATIC && rr.rrand<int>(0, 1) == 0) {
		uint8_t y, x;

		do {
			y = (uint8_t)(n.y + rr.rrand<int>(-1, 1));
			x = (uint8_t)(n.x + rr.rrand<int>(-1, 1));
		} while (!(n.type & TUNNEL) && tiles[y][x].h != 0);

		if (n.type & TUNNEL) {
			move_tunnel(win, n, y, x);
		} else {
			move_logic(win, n, y, x);
		}

		return PC_NONE;
	}

	uint16_t const basic_type = n.type & 0xF;

	switch(basic_type) {
	case 0x0:
	case 0x8:
	case 0x4:
	case 0xC:
		/* straight line and tunnel if can see player */
		if (pc_visible(n.x, n.y)) {
			move_straight(win, n);
		}
		break;
	case 0x2:
	case 0xA:
	case 0x6:
	case 0xE:
		/* straight line and tunnel, telepathic towards player */
		move_straight(win, n);
		break;
	case 0x1:
	case 0x9:
	case 0x3:
	case 0xB:
		/* nontunneling dijk, remembered location or telepathic */
		if (n.type & TELE || pc_visible(n.x, n.y)) {
			n.p_count = PERSISTANCE;
		}

		if (n.p_count != 0) {
			move_dijk_nontunneling(win, n);
			n.p_count--;
		}
		break;
	case 0x5:
	case 0xD:
	case 0x7:
	case 0xF:
		/* tunneling dijk, remembered location or telepathic */
		if (n.type & TELE || pc_visible(n.x, n.y)) {
			n.p_count = PERSISTANCE;
		}

		if (n.p_count != 0) {
			move_dijk_tunneling(win, n);
			n.p_count--;
		}
		break;
	default:
		errx(1, "turn_npc invalid npc type %d", n.type);
	}

	return PC_NONE;
}

static enum pc_action
turn_pc(WINDOW *const win, WINDOW *const sep, npc &n)
{
	uint8_t y = n.y;
	uint8_t x = n.x;
	bool exit = false;

	while (!exit) {
		exit = true;
		switch(wgetch(win)) {
		case ERR:
			errx(1, "turn_pc wgetch ERR");
			break;
		case KEY_HOME:
		case KEY_A1:
		case '7':
		case 'y':
			/* up left */
			y--;
			x--;
			break;
		case KEY_UP:
		case '8':
		case 'k':
			/* up */
			y--;
			break;
		case KEY_PPAGE:
		case KEY_A3:
		case '9':
		case 'u':
			/* up right */
			y--;
			x++;
			break;
		case KEY_RIGHT:
		case '6':
		case 'l':
			/* right */
			x++;
			break;
		case KEY_NPAGE:
		case KEY_C3:
		case '3':
		case 'n':
			/* down right */
			y++;
			x++;
			break;
		case KEY_DOWN:
		case '2':
		case 'j':
			/* down */
			y++;
			break;
		case KEY_END:
		case KEY_C1:
		case '1':
		case 'b':
			/* down left */
			y++;
			x--;
			break;
		case KEY_LEFT:
		case '4':
		case 'h':
			/* left */
			x--;
			break;
		case KEY_B2:
		case ' ':
		case '5':
		case '.':
			/* rest */
			//return PC_NONE;
			break;
		case '>':
			/* go down stairs */
			if (tiles[y][x].c == STAIR_DN) {
				return PC_NEXT;
			} else {
				exit = false;
			}
			break;
		case '<':
			/* go up stairs */
			if (tiles[y][x].c == STAIR_UP) {
				return PC_NEXT;
			} else {
				exit = false;
			}
			break;
		case 'm':
			return PC_NPC_LIST;
		case 'Q':
		case 'q':
			return PC_QUIT;
		case 'f':
			return PC_DEFOG;
		case 'g':
			return PC_TELE;
		case 'i':
			carry_list(sep, CARRY_LIST);
			return PC_RETRY;
		case 'e':
			equip_list(sep, false);
			return PC_RETRY;
		case 'w':
			carry_list(sep, CARRY_WEAR);
			return PC_RETRY;
		case 't':
			equip_list(sep, true);
			return PC_RETRY;
		case 'd':
			carry_list(sep, CARRY_DROP);
			return PC_RETRY;
		case 'x':
			carry_list(sep, CARRY_REMOVE);
			return PC_RETRY;
		case 'L':
			inspect(win, false);
			return PC_RETRY;
		case 'I':
			carry_list(sep, CARRY_INSPECT);
			return PC_RETRY;
		default:
			exit = false;
		}
	}

	if (tiles[y][x].h == 0) {
		move_logic(win, n, y, x);
		try_carry(y, x);
		dijkstra();
	}

	return PC_NONE;
}

static void
npc_list(WINDOW *const nwin, std::vector<npc *> const &npcs)
{
	std::vector<npc>::size_type cpos = 0;

	while (1) {
		if (werase(nwin) == ERR) {
			errx(1, "npc_list erase");
		}

		(void)box(nwin, 0, 0);

		(void)mvwprintw(nwin, HEIGHT - 1, 2,
			"[ arrow keys to scroll; ESC to exit ]");

		std::size_t i;
		for (i = 0; i < HEIGHT - 2 && i + cpos < npcs.size(); ++i) {
			npc *n = npcs[i + cpos];

			if (n->dead) {
				(void)mvwprintw(nwin, static_cast<int>(i + 1U),
					2, "%u.\t'%c'\t(dead)\t\t%s", i + cpos,
					n->symb, n->name.c_str());
				continue;
			}

			int dx = player.x - n->x;
			int dy = player.y - n->y;

			(void)mvwprintw(nwin, static_cast<int>(i + 1U), 2,
				"%u.\t'%c'\t%d %s and %d %s\t%s", i + cpos,
				n->symb, abs(dy), dy > 0 ? "north" : "south",
				abs(dx), dx > 0 ? "west" : "east",
				n->name.c_str());
		}

		for (; i < HEIGHT - 2; ++i) {
			(void)mvwaddch(nwin, static_cast<int>(i + 1U), 2, '~');
		}

		if (wrefresh(nwin) == ERR) {
			errx(1, "npc_list wrefresh");
		}

		switch(wgetch(nwin)) {
		case ERR:
			errx(1, "npc_list wgetch ERR");
			return;
		case KEY_UP:
			if (--cpos > npcs.size()) {
				cpos = 0;
			}
			break;
		case KEY_DOWN:
			if (++cpos > npcs.size() - 1) {
				cpos = npcs.size() - 1;
			}
			break;
		case KEY_ESC:
			return;
		default:
			break;
		}
	}
}

static void
defog(WINDOW *const win)
{
	for (uint8_t x = 1; x < WIDTH - 1; ++x) {
		for (uint8_t y = 1; y < HEIGHT - 1; ++y) {
			npc_obj_or_tile(win, y, x);
		}
	}

	wattron(win, player.color);
	(void)mvwaddch(win, player.y, player.x, player.symb);
	wattroff(win, player.color);

	(void)mvwprintw(win, HEIGHT - 1, 2, "[ press any key to exit ]");

	if (wrefresh(win) == ERR) {
		errx(1, "defog wrefresh");
	}

	(void)wgetch(win);
}

static void
crosshair(WINDOW *const win, uint8_t const y, uint8_t const x)
{
	for (int i = 1; i < HEIGHT - 1; ++i) {
		if (i != y) {
			(void)mvwaddch(win, i, x, ACS_VLINE);
		}
	}

	for (int i = 1; i < WIDTH - 1; ++i) {
		if (i != x) {
			(void)mvwaddch(win, y, i, ACS_HLINE);
		}
	}

	(void)mvwaddch(win, y, 0, ACS_LTEE);
	(void)mvwaddch(win, y, WIDTH - 1, ACS_RTEE);
	(void)mvwaddch(win, 0, x, ACS_TTEE);
	(void)mvwaddch(win, HEIGHT - 1, x, ACS_BTEE);

	(void)mvwaddch(win, y + 1, x + 0, ACS_TTEE);
	(void)mvwaddch(win, y - 1, x + 0, ACS_BTEE);
	(void)mvwaddch(win, y + 0, x - 1, ACS_RTEE);
	(void)mvwaddch(win, y + 0, x + 1, ACS_LTEE);

	(void)mvwaddch(win, y - 1, x - 1, ACS_ULCORNER);
	(void)mvwaddch(win, y - 1, x + 1, ACS_URCORNER);
	(void)mvwaddch(win, y + 1, x - 1, ACS_LLCORNER);
	(void)mvwaddch(win, y + 1, x + 1, ACS_LRCORNER);
}

static bool
inspect(WINDOW *const win, bool const teleport)
{
	WINDOW *twin;
	uint8_t y = player.y;
	uint8_t x = player.x;
	bool ret = true;

	while (1) {
		if ((twin = dupwin(win)) == NULL) {
			errx(1, "inspect dupwin");
		}

		if (touchwin(twin) == ERR) {
			errx(1, "inspect touchwin");
		}

		crosshair(twin, y, x);

		if (teleport) {
			(void)mvwprintw(twin, HEIGHT - 1, 2,
				"[ PC control keys; 'r' for random location; "
				"'g' or 't' to teleport; ESC to exit ]");
		} else {
			(void)mvwprintw(twin, HEIGHT - 1, 2,
				"[ PC control keys; 'g' or 't' to inspect; "
				"ESC to exit ]");
		}


		if (wrefresh(twin) == ERR) {
			errx(1, "inspect wrefresh");
		}

		switch(wgetch(win)) {
		case ERR:
			errx(1, "inspect wgetch ERR");
			break;
		case KEY_HOME:
		case KEY_A1:
		case '7':
		case 'y':
			/* up left */
			y--;
			x--;
			break;
		case KEY_UP:
		case '8':
		case 'k':
			/* up */
			y--;
			break;
		case KEY_PPAGE:
		case KEY_A3:
		case '9':
		case 'u':
			/* up right */
			y--;
			x++;
			break;
		case KEY_RIGHT:
		case '6':
		case 'l':
			/* right */
			x++;
			break;
		case KEY_NPAGE:
		case KEY_C3:
		case '3':
		case 'n':
			/* down right */
			y++;
			x++;
			break;
		case KEY_DOWN:
		case '2':
		case 'j':
			/* down */
			y++;
			break;
		case KEY_END:
		case KEY_C1:
		case '1':
		case 'b':
			/* down left */
			y++;
			x--;
			break;
		case KEY_LEFT:
		case '4':
		case 'h':
			/* left */
			x--;
			break;
		case 'r':
			if (teleport) {
				/* random teleport location */
				x = rr.rrand<uint8_t>(2, WIDTH - 1);
				y = rr.rrand<uint8_t>(2, HEIGHT - 1);
			}

			break;
		case 't':
		case 'g':
			if (teleport && tiles[y][x].n == NULL) {
				/* complete teleport */
				tiles[y][x].v = true;
				move_logic(win, player, y, x);
				goto exit;
			}

			if (!teleport && tiles[y][x].n != NULL) {
				thing_details(twin, *tiles[y][x].n);
			}

			break;
		case KEY_ESC:
			ret = false;
			goto exit;
		default:
			break;
		}

		if (x >= WIDTH - 1) {
			x = WIDTH - 2;
		} else if (x < 1) {
			x = 1;
		}

		if (y >= HEIGHT - 1) {
			y = HEIGHT - 2;
		} else if (y < 1) {
			y = 1;
		}
	}

	exit:

	if (delwin(twin) == ERR) {
		errx(1, "inspect delwin");
	}

	return ret;
}

static bool
viewable(int const y, int const x)
{
	return !tiles[y][x].v && pc_visible(x, y)
		&& (pc_visible(x - 1, y + 0)
		|| pc_visible(x + 1, y + 0)
		|| pc_visible(x + 0, y - 1)
		|| pc_visible(x + 0, y + 1)
		|| pc_visible(x + 1, y + 1)
		|| pc_visible(x - 1, y - 1)
		|| pc_visible(x - 1, y + 1)
		|| pc_visible(x + 1, y - 1));
}

static void
pc_viewbox(WINDOW *const win, int const lum)
{
	uint8_t const start_x = (uint8_t)subu32(player.x + 1, lum);
	uint8_t const end_x = (uint8_t)(player.x + lum);

	uint8_t const start_y = (uint8_t)subu32(player.y + 1, lum);
	uint8_t const end_y = (uint8_t)(player.y + lum);

	for (uint8_t i = start_x; i <= end_x && i < WIDTH - 1; ++i) {
		for (uint8_t j = start_y; j <= end_y && j < HEIGHT - 1; ++j) {
			if (!viewable(j, i)) {
				continue;
			}

			tiles[j][i].v = true;
			npc_obj_or_tile(win, j, i);
		}
	}
}

static void
try_carry(uint8_t const y, uint8_t const x)
{
	if (tiles[y][x].o == NULL) {
		return;
	}

	for (int i = 0; i < PC_CARRY_MAX; ++i) {
		if (!pc_carry[i].has_value()) {
			pc_carry[i] = *tiles[y][x].o;
			tiles[y][x].o = NULL;
			return;
		}
	}
}

static void
carry_list(WINDOW *const cwin, carry_action const action)
{
	std::optional<std::string> error;
	do {
		if (werase(cwin) == ERR) {
			errx(1, "carry_list erase");
		}

		if (action == CARRY_REMOVE) {
			wattron(cwin, COLOR_PAIR(COLOR_RED));
		}

		(void)box(cwin, 0, 0);

		switch (action) {
		case CARRY_DROP:
			(void)mvwprintw(cwin, HEIGHT - 1, 2,
				"[ 0-9 to drop, ESC to exit ]");
			break;
		case CARRY_INSPECT:
			(void)mvwprintw(cwin, HEIGHT - 1, 2,
				"[ 0-9 to inspect, ESC to exit ]");
			break;
		case CARRY_REMOVE:
			(void)mvwprintw(cwin, HEIGHT - 1, 2,
				"[ 0-9 to REMOVE, ESC to exit ]");
			break;
		case CARRY_LIST:
			(void)mvwprintw(cwin, HEIGHT - 1, 2,
				"[ press any key to exit ]");
			break;
		case CARRY_WEAR:
			(void)mvwprintw(cwin, HEIGHT - 1, 2,
				"[ 0-9 to equip, ESC to exit ]");
			break;
		}

		if (error.has_value()) {
			(void)mvwprintw(cwin, 0, 2, "[ error: %s ]",
				error->c_str());
			error.reset();
		}

		if (action == CARRY_REMOVE) {
			wattroff(cwin, COLOR_PAIR(COLOR_RED));
		}


		for (int i = 0; i < PC_CARRY_MAX; ++i) {
			if (pc_carry[i].has_value()) {
				wattron(cwin, pc_carry[i]->color);
				(void)mvwprintw(cwin, i + 5, 2,
					"%d. %s: \t'%c'\t%s", i,
					type_map_name[pc_carry[i]->obj_type],
					pc_carry[i]->symb,
					pc_carry[i]->name.c_str());
				wattroff(cwin, pc_carry[i]->color);
			} else {
				(void)mvwprintw(cwin, i + 5, 2, "%u.", i);
			}
		}

		int const ch = wgetch(cwin);

		if (action == CARRY_LIST) {
			return;
		}

		switch(ch) {
		case ERR:
			errx(1, "carry_list wgetch ERR");
			return;
		case KEY_ESC:
			return;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			int const i = ch - '0';

			if (!pc_carry[i].has_value()) {
				error = std::string("slot ") + std::to_string(i)
					+ " has no item";
				break;
			}

			if (action == CARRY_WEAR) {
				if (!type_map_equip[pc_carry[i]->obj_type]) {
					error = std::string("item in slot ")
						+ std::to_string(i)
						+ " cannot be eqipped";
					break;
				}

				carry_to_equip(i);
			} else if (action == CARRY_DROP) {
				tiles[player.y][player.x].o = &(*pc_carry[i]);
				pc_carry[i].reset();
			} else if (action == CARRY_REMOVE) {
				pc_carry[i].reset();
			} else if (action == CARRY_INSPECT) {
				thing_details(cwin, *pc_carry[i]);
			}

			break;
		}
	} while (1);
}

static void
print_equipped(WINDOW *const ewin, int const i, char const *const name,
	char const ch, std::optional<obj> const &item)
{
	if (item.has_value()) {
		wattron(ewin, item->color);
		(void)mvwprintw(ewin, i, 2, "%s\t%c.\t'%c'\t%s", name, ch,
			item->symb, item->name.c_str());
		wattroff(ewin, item->color);
	} else {
		(void)mvwprintw(ewin, i, 2, "%s\t%c.", name, ch);
	}
}

static void
equip_list(WINDOW *const ewin, bool const take)
{
	std::optional<std::string> error;
	int const length = 12;
	std::tuple<std::optional<obj> const *const, char const *const, char> const equip[] = {
		{ &pc_equip.amulet,	"amulet",	'a' },
		{ &pc_equip.armor,	"armor\t",	'b' },
		{ &pc_equip.boots,	"boots\t",	'c' },
		{ &pc_equip.cloak,	"cloak\t",	'd' },
		{ &pc_equip.gloves,	"gloves",	'e' },
		{ &pc_equip.helmet,	"helmet",	'f' },
		{ &pc_equip.light,	"light\t",	'g' },
		{ &pc_equip.offhand,	"offhand",	'h' },
		{ &pc_equip.ranged,	"ranged",	'i' },
		{ &pc_equip.ring_left,	"left ring",	'j' },
		{ &pc_equip.ring_right,	"right ring",	'k' },
		{ &pc_equip.weapon,	"weapon",	'l' }
	};

	do {
		if (werase(ewin) == ERR) {
			errx(1, "equip_list erase");
		}

		(void)box(ewin, 0, 0);

		if (take) {
			(void)mvwprintw(ewin, HEIGHT - 1, 2,
				"[ a-l to take off, ESC to exit ]");
		} else {
			(void)mvwprintw(ewin, HEIGHT - 1, 2,
				"[ press any key to exit ]");
		}

		if (error.has_value()) {
			(void)mvwprintw(ewin, 0, 2, "[ error: %s ]",
				error->c_str());
			error.reset();
		}

		for (int i = 0; i < length; ++i) {
			print_equipped(ewin, i + 4, std::get<1>(equip[i]),
				std::get<2>(equip[i]), *std::get<0>(equip[i]));
		}

		int const ch = wgetch(ewin);

		if (!take) {
			return;
		}

		switch(ch) {
		case ERR:
			errx(1, "equip_list wgetch ERR");
			return;
		case KEY_ESC:
			return;
		default:
			equip_to_carry(ch, error);
			break;
		}
	} while (1);
}

static void
carry_to_equip(int const i)
{
	std::optional<obj> *equip_slot;

	switch(pc_carry[i]->obj_type) {
	case amulet:
		equip_slot = &pc_equip.amulet;
		break;
	case armor:
		equip_slot = &pc_equip.armor;
		break;
	case boots:
		equip_slot = &pc_equip.boots;
		break;
	case cloak:
		equip_slot = &pc_equip.cloak;
		break;
	case gloves:
		equip_slot = &pc_equip.gloves;
		break;
	case helmet:
		equip_slot = &pc_equip.helmet;
		break;
	case light:
		equip_slot = &pc_equip.light;
		break;
	case offhand:
		equip_slot = &pc_equip.offhand;
		break;
	case ranged:
		equip_slot = &pc_equip.ranged;
		break;
	case ring:
		if (pc_equip.ring_right.has_value()) {
			equip_slot = &pc_equip.ring_left;
		} else {
			equip_slot = &pc_equip.ring_right;
		}
		break;
	case weapon:
		equip_slot = &pc_equip.weapon;
		break;
	default:
		errx(1, "carry_to_equip bad swap");
	}

	std::swap(pc_carry[i], *equip_slot);
}

static void
equip_to_carry(int const i, std::optional<std::string> &error)
{
	std::optional<obj> *equip_slot;

	switch(i) {
	case 'a':
		equip_slot = &pc_equip.amulet;
		break;
	case 'b':
		equip_slot = &pc_equip.armor;
		break;
	case 'c':
		equip_slot = &pc_equip.boots;
		break;
	case 'd':
		equip_slot = &pc_equip.cloak;
		break;
	case 'e':
		equip_slot = &pc_equip.gloves;
		break;
	case 'f':
		equip_slot = &pc_equip.helmet;
		break;
	case 'g':
		equip_slot = &pc_equip.light;
		break;
	case 'h':
		equip_slot = &pc_equip.offhand;
		break;
	case 'i':
		equip_slot = &pc_equip.ranged;
		break;
	case 'j':
		equip_slot = &pc_equip.ring_left;
		break;
	case 'k':
		equip_slot = &pc_equip.ring_right;
		break;
	case 'l':
		equip_slot = &pc_equip.weapon;
		break;
	default:
		return;
	}

	if (!equip_slot->has_value()) {
		error = std::string("slot ") + (char)i + " has no item";
	}

	for (int j = 0; j < PC_CARRY_MAX; ++j) {
		if (!pc_carry[j].has_value()) {
			std::swap(pc_carry[j], *equip_slot);
			return;
		}
	}

	error = "no open slots in carry bag";
}

static void
thing_details(WINDOW *const win, dungeon_thing const &d)
{
	std::stringstream ss(d.desc);
	std::string tmp;

	std::vector<std::string> lines;
	std::vector<std::string>::size_type cpos = 0;

	lines.push_back(std::string("Symbol: '") + (char)d.symb + "'\tName: "
		+ d.name);
	lines.push_back("");

	while (std::getline(ss, tmp, '\n')) {
		lines.push_back(tmp);
	}

	while (1) {
		if (werase(win) == ERR) {
			errx(1, "thing_details erase");
		}

		(void)box(win, 0, 0);

		(void)mvwprintw(win, HEIGHT - 1, 2,
			"[ arrow keys to scroll; ESC to exit ]");

		std::size_t i;
		for(i = 0; i < HEIGHT - 2 && i + cpos < lines.size(); ++i) {
			(void)mvwprintw(win, static_cast<int>(i + 1U), 2,
				lines[i + cpos].c_str());
		}

		for (; i < HEIGHT - 2; ++i) {
			(void)mvwaddch(win, static_cast<int>(i + 1U), 2, '~');
		}

		if (wrefresh(win) == ERR) {
			errx(1, "thing_details wrefresh");
		}

		switch(wgetch(win)) {
		case ERR:
			errx(1, "thing_details wgetch ERR");
			return;
		case KEY_UP:
			if (--cpos > lines.size()) {
				cpos = 0;
			}
			break;
		case KEY_DOWN:
			if (++cpos > lines.size() - 1) {
				cpos = lines.size() - 1;
			}
			break;
		case KEY_ESC:
			return;
		default:
			break;
		}
	}
}
