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

namespace msemu::cpu8086
{

struct Registers
{
    uint16_t ax;
    uint16_t bx;
    uint16_t cx;
    uint16_t dx;
    uint16_t si; // source index
    uint16_t di; // destination index
    uint16_t bp; // base pointer
    uint16_t sp; // stack pointer
    uint16_t ip; // instruction pointer
    uint16_t cs; // code segment
    uint16_t ds; // data segment
    uint16_t es; // extra segment
    uint16_t ss; // stack segment
    struct
    {
        uint16_t res_0 : 1;
        uint16_t res_1 : 1;
        uint16_t res_2 : 1;
        uint16_t res_3 : 1;
        uint16_t o : 1; // overflow
        uint16_t d : 1; // direction
        uint16_t i : 1; // interrupt
        uint16_t t : 1; // trap
        uint16_t s : 1; // sign
        uint16_t z : 1; // zero
        uint16_t res_4 : 1;
        uint16_t a : 1; // adjust
        uint16_t res_5 : 1;
        uint16_t p : 1; // parity
        uint16_t res_6 : 1;
        uint16_t c : 1; // carry
    } flags;
};

enum class RegisterPart
{
    low,
    high,
    whole
};

namespace _detail
{
template <RegisterPart where>
constexpr uint16_t register_mask()
{
    if constexpr (where == RegisterPart::low)
    {
        return 0xff;
    }
    if constexpr (where == RegisterPart::high)
    {
        return 0xff00;
    }
    return 0xffff;
}

template <RegisterPart where>
constexpr uint16_t register_offset()
{
    if constexpr (where == RegisterPart::low)
    {
        return 0;
    }
    if constexpr (where == RegisterPart::high)
    {
        return 8;
    }
    return 0;
}
} // namespace _detail

template <RegisterPart where>
constexpr inline void set_register(uint16_t& reg, uint16_t value)
{
    if constexpr (where == RegisterPart::low)
    {
        *reinterpret_cast<uint8_t*>(&reg) = static_cast<uint8_t>(value);
    }
    else if constexpr (where == RegisterPart::high)
    {
        reinterpret_cast<uint8_t*>(&reg)[1] = static_cast<uint8_t>(value);
    }
    else
    {
        reg = value;
    }
}

template <RegisterPart where, typename T = uint16_t>
constexpr inline T get_register(uint16_t reg)
{
    if constexpr (where == RegisterPart::low)
    {
        return reinterpret_cast<uint8_t*>(&reg)[0];
    }
    else if constexpr (where == RegisterPart::high)
    {
        return reinterpret_cast<uint8_t*>(&reg)[1];
    }
    else
    {
        return reg;
    }
}


} // namespace msemu::cpu8086
