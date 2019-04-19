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
#include "rand.h"

ranged_random rr;

ranged_random::ranged_random()
{
	seed = std::random_device{}();
	gen.seed(seed);
}

ranged_random::ranged_random(std::string const &s)
{
	long unsigned int hash = 5381;

	for(auto const c : s) {
		hash = ((hash << 5)) ^ c;
	}

	seed = hash;
	gen.seed(seed);
}

ranged_random::ranged_random(long unsigned int const s)
{
	seed = s;
	gen.seed(seed);
}
