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
using AddressGenerators = std::array<uint32_t (*)(uint16_t address, std::optional<uint8_t>&), 8>;
using Costs             = std::array<uint8_t, 8>;

struct Modes
{
    std::array<AddressGenerators, 3> modes;
    std::array<Costs, 4> costs;
};

enum class AccessCost : uint8_t
{
    Direct,
    RegisterIndirect,
    RegisterRelative,
    BpDiOrBxSi,
    BpSiOrBxDi,
    BpDiDispOrBxSiDisp,
    BpSiDispOrBxDiDisp
};

constexpr uint8_t get_cost(const AccessCost c)
{
    switch (c)
    {
        case AccessCost::Direct:
            return 5;
        case AccessCost::RegisterIndirect:
            return 5;
        case AccessCost::RegisterRelative:
            return 6;
        case AccessCost::BpDiOrBxSi:
            return 7;
        case AccessCost::BpSiOrBxDi:
            return 8;
        case AccessCost::BpDiDispOrBxSiDisp:
            return 11;
        case AccessCost::BpSiDispOrBxDiDisp:
            return 12;
    }
    return 0;
}


static inline uint32_t get_code_address(const uint32_t address, std::optional<uint8_t>& segment_register)
{
    if (segment_register)
    {
        const uint32_t section_modifier = get_segment_register_by_id(*segment_register);
        return (section_modifier << 4) + address;
    }
    return (static_cast<uint32_t>(Register::cs()) << 4) + address;
}

static inline uint32_t get_data_address(const uint32_t address, std::optional<uint8_t>& segment_register)
{
    if (segment_register)
    {
        const uint32_t section_modifier = get_segment_register_by_id(*segment_register);
        return (section_modifier << 4) + address;
    }
    return (static_cast<uint32_t>(Register::ds()) << 4) + address;
}

static inline uint32_t get_stack_address(const uint32_t address, std::optional<uint8_t>& segment_register)
{
    if (segment_register)
    {
        const uint32_t section_modifier = get_segment_register_by_id(*segment_register);
        return (section_modifier << 4) + address;
    }
    
    return (static_cast<uint32_t>(Register::ss()) << 4) + address;
}

constexpr static inline Modes modes{
    .modes =
        {
            AddressGenerators{
                [](uint16_t, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_data_address(static_cast<uint32_t>(Register::bx()) +
                                                static_cast<uint32_t>(Register::si()),
                                            segment_register);
                },
                [](uint16_t, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_data_address(static_cast<uint32_t>(Register::bx()) +
                                                static_cast<uint32_t>(Register::di()),
                                            segment_register);
                },
                [](uint16_t, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_stack_address(static_cast<uint32_t>(Register::bp()) +
                                                 static_cast<uint32_t>(Register::si()),
                                             segment_register);
                },
                [](uint16_t, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_stack_address(static_cast<uint32_t>(Register::bp()) +
                                                 static_cast<uint32_t>(Register::di()),
                                             segment_register);
                },
                [](uint16_t, std::optional<uint8_t>& segment_register) -> uint32_t
                { return get_data_address(Register::si(), segment_register); },
                [](uint16_t, std::optional<uint8_t>& segment_register) -> uint32_t
                { return get_data_address(Register::di(), segment_register); },
                [](uint16_t address, std::optional<uint8_t>& segment_register) -> uint32_t
                { return get_stack_address(address, segment_register); },
                [](uint16_t, std::optional<uint8_t>& segment_register) -> uint32_t
                { return get_data_address(Register::bx(), segment_register); },
            },
            {
                [](uint16_t address, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_data_address(static_cast<uint32_t>(Register::bx()) +
                                                static_cast<uint32_t>(Register::si()) +
                                                static_cast<uint32_t>(address),
                                            segment_register);
                },
                [](uint16_t address, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_data_address(static_cast<uint32_t>(Register::bx()) +
                                                static_cast<uint32_t>(Register::di()) +
                                                static_cast<uint32_t>(address),
                                            segment_register);
                },
                [](uint16_t address, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_stack_address(static_cast<uint32_t>(Register::bp()) +
                                                 static_cast<uint32_t>(Register::si()) +
                                                 static_cast<uint32_t>(address),
                                             segment_register);
                },
                [](uint16_t address, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_stack_address(static_cast<uint32_t>(Register::bp()) +
                                                 static_cast<uint32_t>(Register::di()) +
                                                 static_cast<uint32_t>(address),
                                             segment_register);
                },
                [](uint16_t address, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_data_address(static_cast<uint32_t>(Register::si()) +
                                                static_cast<uint32_t>(address),
                                            segment_register);
                },
                [](uint16_t address, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_data_address(static_cast<uint32_t>(Register::di()) +
                                                static_cast<uint32_t>(address),
                                            segment_register);
                },
                [](uint16_t address, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_stack_address(static_cast<uint32_t>(Register::bp()) +
                                                 static_cast<uint32_t>(address),
                                             segment_register);
                },
                [](uint16_t address, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_data_address(static_cast<uint32_t>(Register::bx()) +
                                                static_cast<uint32_t>(address),
                                            segment_register);
                },
            },
            {
                [](uint16_t address, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_data_address(static_cast<uint32_t>(Register::bx()) +
                                                static_cast<uint32_t>(Register::si()) +
                                                static_cast<uint32_t>(address),
                                            segment_register);
                },
                [](uint16_t address, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_data_address(static_cast<uint32_t>(Register::bx()) +
                                                static_cast<uint32_t>(Register::di()) +
                                                static_cast<uint32_t>(address),
                                            segment_register);
                },
                [](uint16_t address, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_stack_address(static_cast<uint32_t>(Register::bp()) +
                                                 static_cast<uint32_t>(Register::si()) +
                                                 static_cast<uint32_t>(address),
                                             segment_register);
                },
                [](uint16_t address, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_stack_address(static_cast<uint32_t>(Register::bp()) +
                                                 static_cast<uint32_t>(Register::di()) +
                                                 static_cast<uint32_t>(address),
                                             segment_register);
                },
                [](uint16_t address, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_data_address(static_cast<uint32_t>(Register::si()) +
                                                static_cast<uint32_t>(address),
                                            segment_register);
                },
                [](uint16_t address, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_data_address(static_cast<uint32_t>(Register::di()) +
                                                static_cast<uint32_t>(address),
                                            segment_register);
                },
                [](uint16_t address, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_stack_address(static_cast<uint32_t>(Register::bp()) +
                                                 static_cast<uint32_t>(address),
                                             segment_register);
                },
                [](uint16_t address, std::optional<uint8_t>& segment_register) -> uint32_t
                {
                    return get_data_address(static_cast<uint32_t>(Register::bx()) +
                                                static_cast<uint32_t>(address),
                                            segment_register);
                },
            },

        },
    .costs = {
        Costs{7, 8, 8, 7, 5, 5, 6, 5},
        Costs{11, 12, 12, 11, 9, 9, 9, 9},
        Costs{11, 12, 12, 11, 9, 9, 9, 9},
        Costs{6, 6, 6, 6, 6, 6, 6, 6},
    }};

} // namespace cpu8086
} // namespace msemu
