/*
 Copyright (c) 2021 Mateusz Stadnik

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>

#include <string_view>


namespace msemu
{

template <typename MemoryType, uint32_t address_start>
class Device
{
public:
    Device(std::string_view name)
        : name_(name)
        , memory_{}
    {
    }

    void print() const
    {
        printf("%s start: 0x%08x, end: 0x%08x\n", name_.data(), start_address, end_address);
    }

    std::string_view name() const
    {
        return name_;
    }

    MemoryType& memory()
    {
        return memory_;
    }

    const MemoryType& memory() const
    {
        return memory_;
    }

    std::span<const uint8_t> span() const
    {
        return memory_.span();
    }

    std::span<uint8_t> span()
    {
        return memory_.span();
    }

    void clear()
    {
        memory_.clear();
    }

    constexpr static uint32_t start_address = address_start;
    constexpr static uint32_t end_address   = address_start + MemoryType::size;

private:
    std::string_view name_;
    MemoryType memory_;
};

} // namespace msemu
