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
#include <cstring>
#include <span>

namespace msemu
{

template <std::size_t Size>
class Memory
{
public:
    Memory()
        : memory_{}
    {
    }

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

    template <typename T>
    inline T read(std::size_t address) const
    {
        if constexpr (sizeof(T) == 1)
        {
            return memory_[address];
        }
        else
        {
            return static_cast<uint16_t>(memory_[address + 1] << 8 | (memory_[address]));
        }
    }

    void write(std::size_t address, uint8_t data)
    {
        memory_[address] = data;
    }

    void write(std::size_t address, uint16_t data)
    {
        memory_[address]     = static_cast<uint8_t>(data & 0xff);
        memory_[address + 1] = static_cast<uint8_t>((data >> 8) & 0xff);
    }

    void write(std::size_t address, const std::span<const uint8_t> data)
    {
        std::memcpy(&memory_[address], data.data(), data.size());
    }

    void read(std::size_t address, std::span<uint8_t> data)
    {
        std::memcpy(data.data(), &memory_[address], data.size());
    }

    void clear()
    {
        std::memset(memory_.data(), 0, Size);
    }

private:
    std::array<uint8_t, Size> memory_;
};

} // namespace msemu
