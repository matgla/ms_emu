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

#include "8086_registers.hpp"

namespace msemu
{
namespace cpu8086
{
using AddressGenerators = std::array<uint32_t (*)(Registers&, uint16_t address), 8>;

struct Modes
{
    std::array<AddressGenerators, 3> modes;
    std::array<uint16_t Registers::*, 8> reg16;
    std::array<uint16_t Registers::*, 8> reg8;
};

constexpr static inline Modes modes{
    .modes =
        {
            AddressGenerators{
                [](Registers& reg, uint16_t) -> uint32_t {
                    return static_cast<uint32_t>(reg.bx) + static_cast<uint32_t>(reg.si);
                },
                [](Registers& reg, uint16_t) -> uint32_t {
                    return static_cast<uint32_t>(reg.bx) + static_cast<uint32_t>(reg.di);
                },
                [](Registers& reg, uint16_t) -> uint32_t {
                    return static_cast<uint32_t>(reg.bp) + static_cast<uint32_t>(reg.si);
                },
                [](Registers& reg, uint16_t) -> uint32_t {
                    return static_cast<uint32_t>(reg.bp) + static_cast<uint32_t>(reg.di);
                },
                [](Registers& reg, uint16_t) -> uint32_t { return reg.si; },
                [](Registers& reg, uint16_t) -> uint32_t { return reg.di; },
                [](Registers&, uint16_t address) -> uint32_t { return address; },
                [](Registers& reg, uint16_t) -> uint32_t { return reg.bx; },
            },
            {
                [](Registers& reg, uint16_t address) -> uint32_t {
                    return static_cast<uint32_t>(reg.bx) + static_cast<uint32_t>(reg.si) +
                           static_cast<uint32_t>(address);
                },
                [](Registers& reg, uint16_t address) -> uint32_t {
                    return static_cast<uint32_t>(reg.bx) + static_cast<uint32_t>(reg.di) +
                           static_cast<uint32_t>(address);
                },
                [](Registers& reg, uint16_t address) -> uint32_t {
                    return static_cast<uint32_t>(reg.bp) + static_cast<uint32_t>(reg.si) +
                           static_cast<uint32_t>(address);
                },
                [](Registers& reg, uint16_t address) -> uint32_t {
                    return static_cast<uint32_t>(reg.bp) + static_cast<uint32_t>(reg.di) +
                           static_cast<uint32_t>(address);
                },
                [](Registers& reg, uint16_t address) -> uint32_t {
                    return static_cast<uint32_t>(reg.si) + static_cast<uint32_t>(address);
                },
                [](Registers& reg, uint16_t address) -> uint32_t {
                    return static_cast<uint32_t>(reg.di) + static_cast<uint32_t>(address);
                },
                [](Registers& reg, uint16_t address) -> uint32_t {
                    return static_cast<uint32_t>(reg.bp) + static_cast<uint32_t>(address);
                },
                [](Registers& reg, uint16_t address) -> uint32_t {
                    return static_cast<uint32_t>(reg.bx) + static_cast<uint32_t>(address);
                },
            },
        },
    .reg16 = {&Registers::ax, &Registers::cx, &Registers::dx, &Registers::bx,
              &Registers::sp, &Registers::bp, &Registers::si, &Registers::di},
    .reg8  = {&Registers::ax, &Registers::cx, &Registers::dx, &Registers::bx,
             &Registers::ax, &Registers::cx, &Registers::dx, &Registers::bx}};

} // namespace cpu8086
} // namespace msemu
