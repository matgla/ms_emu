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

template <uint32_t Size>
class Memory
{
public:
    static constexpr uint32_t size = Size;

    Memory()
        : memory_{}
    {
    }

    std::span<uint8_t> span()
    {
        return memory_;
    }

    std::span<const uint8_t> span() const
    {
        return memory_;
    }

    void clear()
    {
        memory_ = {};
    }

private:
    std::array<uint8_t, Size> memory_;
};

class MemoryView
{
public:
    MemoryView(const std::span<uint8_t>& span, const uint32_t start_address)
        : memory_(span)
        , start_address_(start_address)
    {
    }

    uint32_t size() const
    {
        return static_cast<uint32_t>(memory_.size());
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

        auto bytes = fread(&memory_[0], 1, size, f);
        static_cast<void>(bytes);
        fclose(f);
    }

    template <typename T>
    inline T read(uint32_t address) const
    {
        address -= start_address_;
        if constexpr (sizeof(T) == 1)
        {
            if (address >= memory_.size())
            {
                return 0;
            }
            return static_cast<T>(memory_[address]);
        }
        else
        {
            if (address + 1 >= memory_.size())
            {
                return 0;
            }
            return static_cast<T>(memory_[address + 1] << 8 | (memory_[address]));
        }
    }

    void write(uint32_t address, uint8_t data)
    {
        address -= start_address_;
        memory_[address] = data;
    }

    void write(uint32_t address, uint16_t data)
    {
        address -= start_address_;
        memory_[address]     = static_cast<uint8_t>(data & 0xff);
        memory_[address + 1] = static_cast<uint8_t>((data >> 8) & 0xff);
    }

    void write(uint32_t address, const std::span<const uint8_t> data)
    {
        address -= start_address_;
        std::memcpy(&memory_[address], data.data(), data.size());
    }

    void read(uint32_t address, std::span<uint8_t> data) const
    {
        address -= start_address_;
        std::memcpy(data.data(), &memory_[address], data.size());
    }

    void clear()
    {
        std::memset(memory_.data(), 0, memory_.size_bytes());
    }


private:
    std::span<uint8_t> memory_;
    const uint32_t start_address_;
};

class ConstMemoryView
{
public:
    ConstMemoryView(const std::span<const uint8_t>& span, const uint32_t start_address)
        : memory_(span)
        , start_address_(start_address)
    {
    }

    template <typename T>
    inline T read(std::size_t address) const
    {
        if constexpr (sizeof(T) == 1)
        {
            return memory_[address - start_address_];
        }
        else
        {
            return static_cast<uint16_t>(memory_[address - start_address_ + 1] << 8 |
                                         (memory_[address - start_address_]));
        }
    }

    void read(std::size_t address, std::span<uint8_t> data) const
    {
        std::memcpy(data.data(), &memory_[address - start_address_], data.size());
    }

private:
    std::span<const uint8_t> memory_;
    const uint32_t start_address_;
};


} // namespace msemu
