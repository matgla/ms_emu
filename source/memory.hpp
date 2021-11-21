/*
 *   Copyright (c) 2021 Mateusz Stadnik

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace msemu
{

class MemoryBase
{
public:
    virtual uint8_t read8(std::size_t address) const = 0;
};


template <std::size_t Size>
class Memory : public MemoryBase
{
public:
    void load_from_file(const char* file)
    {
        printf("Load file to memory: %s\n", file);
        FILE* f = fopen(file, "rb");
        if (f == nullptr)
        {
            printf("ERR: Can't load file!\n");
            return;
        }
        fseek(f, 0, SEEK_END);
        std::size_t size = static_cast<std::size_t>(ftell(f));
        fseek(f, 0, SEEK_SET);

        fread(&memory_[0], 1, size, f);
        fclose(f);
    }

    uint8_t read8(std::size_t address) const override
    {
        return memory_[address];
    }

private:
    std::array<uint8_t, Size> memory_;
};

} // namespace msemu
