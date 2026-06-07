// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2019, The TurtleCoin Developers
// Copyright (c) 2019, The Karbo Developers
//
// This file is part of Karbo.
//
// Karbo is free software: you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Karbo is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Karbo. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <random>

namespace Random
{
    /* Used to obtain a random seed */
    static thread_local std::random_device device;

    /* Generator, seeded with the random device */
    static thread_local std::mt19937 gen(device());

    /* The distribution to get numbers for - in this case, uint8_t */
    static std::uniform_int_distribution<int> distribution{0, std::numeric_limits<uint8_t>::max()};

    /**
     * Generate n random bytes (uint8_t), and place them in *result. Result should be large
     * enough to contain the bytes.
     */
    inline void randomBytes(size_t n, uint8_t *result)
    {
        for (size_t i = 0; i < n; i++)
        {
            result[i] = distribution(gen);
        }
    }

    /**
     * Generate n random bytes (uint8_t), and return them in a vector.
     */
    inline std::vector<uint8_t> randomBytes(size_t n)
    {
        std::vector<uint8_t> result;

        result.reserve(n);

        for (size_t i = 0; i < n; i++)
        {
            result.push_back(distribution(gen));
        }

        return result;
    }

    /**
     * Generate a random value of the type specified, in the full range of the
     * type
     */
    template <typename T>
    T randomValue()
    {
        std::uniform_int_distribution<T> distribution{
            std::numeric_limits<T>::min(), std::numeric_limits<T>::max()
        };

        return distribution(gen);
    }

    /**
     * Generate a random value of the type specified, in the range [min, max]
     * Note that both min, and max, are included in the results. Therefore,
     * randomValue(0, 100), will generate numbers between 0 and 100.
     *
     * Note that min must be <= max, or undefined behaviour will occur.
     */
    template <typename T>
    T randomValue(T min, T max)
    {
        std::uniform_int_distribution<T> distribution{min, max};
        return distribution(gen);
    }

    /**
     * Obtain the generator used internally. Helpful for passing to functions
     * like std::shuffle.
     */
    inline std::mt19937 generator()
    {
        return gen;
    }
}
