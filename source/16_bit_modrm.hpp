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

#include <cstdint>

namespace msemu
{
struct ModRM
{
    uint8_t rm : 3;
    uint8_t reg : 3;
    uint8_t mod : 2;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    constexpr ModRM(uint8_t v)
        : rm{static_cast<uint8_t>(v & 0x07u)}
        , reg{static_cast<uint8_t>((v >> 3) & 0x07u)}
        , mod{static_cast<uint8_t>((v >> 6) & 0x03u)}
    {
    }

    constexpr ModRM(uint8_t mod, uint8_t reg, uint8_t rm)
        : rm(rm & 0x07u)
        , reg(reg & 0x07u)
        , mod(mod & 0x03u)
    {
    }
#pragma GCC diagnostic pop
    operator uint8_t() const
    {
        return *reinterpret_cast<const uint8_t*>(this);
    }
};

} // namespace msemu
