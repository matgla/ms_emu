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

#include "core_dump.hpp"

#include <cstdio>

namespace msemu::cpu8086
{

namespace
{
constexpr uint8_t get_mod(uint8_t byte)
{
    return (byte >> 6) & 0x03;
}

constexpr uint8_t get_rm(uint8_t byte)
{
    return byte & 0x07;
}

constexpr uint8_t get_reg_op(uint8_t byte)
{
    return (byte >> 3) & 0x07;
}


} // namespace

void puts_many(const char* str, std::size_t times, bool newline = true)
{
    for (std::size_t i = 0; i < times; ++i)
    {
        fputs(str, stdout);
    }
    if (newline)
    {
        puts("");
    }
}

void print_table_top(std::size_t columns, std::size_t size, bool newline = true)
{
    puts_many(left_top, 1, false);
    for (std::size_t column = 0; column < columns - 1; ++column)
    {
        puts_many(horizontal, size, false);
        puts_many(cross_top, 1, false);
    }
    puts_many(horizontal, size, false);
    puts_many(right_top, 1, newline);
}

void print_table_bottom(std::size_t columns, std::size_t size)
{
    if (columns == 0)
    {
        columns = 1;
    }

    puts_many(left_bottom, 1, false);
    for (std::size_t column = 0; column < columns - 1; ++column)
    {
        puts_many(horizontal, size, false);
        puts_many(cross_bottom, 1, false);
    }
    puts_many(horizontal, size, false);
    puts_many(right_bottom, 1);
}


template <typename T>
void print_table_row(std::size_t columns, size_t size, const T& data, bool newline = true)
{
    puts_many(vertical, 1, false);
    for (std::size_t column = 0; column < columns; ++column)
    {
        const char* p   = data[column];
        std::size_t len = (size - strlen(p)) / 2;
        puts_many(" ", size == len * 2 + strlen(p) ? len : len + 1, false);
        puts_many(p, 1, false);

        puts_many(" ", len, false);
        puts_many(vertical, 1, false);
    }
    puts_many("", 1, newline);
}

uint8_t opcode_to_command(char* line, std::size_t max_size, std::size_t opcode, uint8_t data[2],
                          std::size_t ip)
{
    char mod0_mapping[8][9]  = {"[bx+si]", "[bx+di]", "[bp+si]", "[bp+di]", "[si]", "[di]", "disp16", "[bx]"};
    char reg8_mapping[8][3]  = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
    char reg16_mapping[8][3] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
    char sreg_mapping[4][3]  = {"es", "cs", "ss", "ds"};
    switch (opcode)
    {
        case 0xeb:
        {
            const uint8_t address = static_cast<uint8_t>(ip + 2u + data[0]);
            snprintf(line, max_size, "jmp 0x%02x", address);
            return 2;
        }
        break;
        case 0x48:
        case 0x49:
        case 0x4a:
        case 0x4b:
        case 0x4c:
        case 0x4d:
        case 0x4e:
        case 0x4f:
        {
            snprintf(line, max_size, "dec %s", reg16_mapping[(opcode & 0xf) - 8]);
        }
        break;
        case 0x88:
        {
            uint8_t regop = get_reg_op(data[0]);
            uint8_t reg   = get_rm(data[0]);
            char* address = nullptr;
            uint8_t size  = 2;
            if (get_mod(data[0]) == 0)
            {
                address = mod0_mapping[reg];
            }
            else if (get_mod(data[0]) == 3)
            {
                address = reg8_mapping[reg];
            }
            const char* reg_d = reg8_mapping[regop];
            snprintf(line, max_size, "mov %s,%s", reg_d, address);
            return size;
        }
        case 0x89:
        {
            uint8_t regop = get_reg_op(data[0]);
            uint8_t reg   = get_rm(data[0]);
            char* address = nullptr;
            if (get_mod(data[0]) == 0)
            {
                address = mod0_mapping[reg];
            }
            const char* reg_d = reg16_mapping[regop];
            snprintf(line, max_size, "mov %s,%s", address, reg_d);
        }
        break;
        case 0x8e:
        {
            snprintf(line, max_size, "mov %s,%s", sreg_mapping[get_reg_op(data[0]) & 0x3],
                     reg16_mapping[get_rm(data[0])]);
            return 2;
        }
        break;
        case 0xb0:
        case 0xb1:
        case 0xb2:
        case 0xb3:
        case 0xb4:
        case 0xb5:
        case 0xb6:
        case 0xb7:
        {
            snprintf(line, max_size, "mov %s,0x%02x", reg8_mapping[opcode & 0xf], data[0]);
        }
        break;
        case 0xb8:
        case 0xb9:
        case 0xba:
        case 0xbb:
        case 0xbc:
        case 0xbd:
        case 0xbe:
        case 0xbf:
        {
            snprintf(line, max_size, "mov %s,0x%02x%02x", reg16_mapping[(opcode & 0xf) - 8], data[1],
                     data[0]);
            return 3;
        }
        break;
        case 0xc3:
        {
            snprintf(line, max_size, "ret");
        }
        break;
        case 0xcc:
        case 0xcd:
        {
            snprintf(line, max_size, "int %02x", data[0]);
        }
        break;
        default:
        {
            snprintf(line, max_size, "- - -");
        }
    }
    return 1;
}

}