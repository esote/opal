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
#ifndef TURN_H
#define TURN_H

#include <ncurses.h>

enum turn_exit {
	TURN_DEATH,
	TURN_NEXT,
	TURN_NONE,
	TURN_QUIT,
	TURN_WIN,
};

enum turn_exit	turn_engine(WINDOW *const, unsigned int const, unsigned int const);

#endif /* TURN_H */
