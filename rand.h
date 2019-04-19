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
#ifndef RAND_H
#define RAND_H

#include <random>
#include <string>

class ranged_random {
	std::mt19937 gen;
public:
	long unsigned int seed;

	ranged_random();

	explicit ranged_random(std::string const &);

	explicit ranged_random(long unsigned int const);

	template<typename T> T
	rrand(T a, T b)
	{
		std::uniform_int_distribution<T> dis(a, b);

		return dis(gen);
	}

	template<typename T> T
	rand_dice(T base, T dice, T sides)
	{
		T total = base;
		for (size_t i = 0; i < dice; ++i) {
			total = static_cast<T>(total + rrand<T>(1, sides));
		}
		return total;
	}
};

#endif /* RAND_H */
