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
#include <cstring>

#include "8086_registers.hpp"


namespace msemu::cpu8086
{

constexpr const char* horizontal       = "\u2500";
constexpr const char* left_top         = "\u250c";
constexpr const char* right_top        = "\u2510";
constexpr const char* left_bottom      = "\u2514";
constexpr const char* right_bottom     = "\u2518";
constexpr const char* cross_top        = "\u252c";
constexpr const char* cross_bottom     = "\u2534";
constexpr const char* vertical         = "\u2502";
constexpr const char* left_top_bottom  = "\u251c";
constexpr const char* left_top_right   = "\u2534";
constexpr const char* right_top_bottom = "\u2524";

void puts_many(const char* str, std::size_t times, bool newline = true);
void print_table_top(std::size_t columns, std::size_t size, bool newline = true);
void print_table_bottom(std::size_t columns, std::size_t size);

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
                          std::size_t ip);

void get_disassembly_line(char* line, std::size_t max_size, size_t& program_counter, auto& memory_)
{
    uint8_t pc = memory_.template read<uint8_t>(program_counter);

    uint8_t data[6] = {};
    for (uint8_t i = 1; i < sizeof(data); ++i)
    {
        data[i - 1] = memory_.template read<uint8_t>(program_counter + i);
    }

    char command[30];
    uint8_t size = opcode_to_command(command, sizeof(command), pc, data, program_counter);

    std::memset(line, 0, max_size);
    char cursor;
    if (program_counter == Register::ip())
    {
        cursor = '>';
    }
    else
    {
        cursor = ' ';
    }
    if (size == 1)
    {
        snprintf(line, max_size, " %c %8lx: %02x        | %s", cursor, program_counter, pc, command);
    }
    if (size == 2)
    {
        snprintf(line, max_size, " %c %8lx: %02x %02x     | %s", cursor, program_counter, pc, data[0],
                 command);
    }
    if (size == 3)
    {
        snprintf(line, max_size, " %c %8lx: %02x %02x %02x  | %s", cursor, program_counter, pc, data[0],
                 data[1], command);
    }
    program_counter += size;
}

void dump(const char* error_msg, auto& memory_)
{
    constexpr const char* clear_screen = "\033[H\033[2J\033[3J";

    printf(clear_screen);
    printf("IP: %x\n", Register::ip());

    print_table_top(3, 15, false);
    char disasm[255];
    std::size_t pc = Register::ip();
    if (pc < 6)
        pc = 0;
    else
        pc -= 6;

    get_disassembly_line(disasm, sizeof(disasm), pc, memory_);
    printf("%s\n", disasm);
    char line[3][20] = {"REG  H  L  ", "Segments", "Pointers"};
    print_table_row(3, 15, line, false);
    get_disassembly_line(disasm, sizeof(disasm), pc, memory_);
    printf("%s\n", disasm);

    sprintf(line[0], "A  %-4x", Register::ax());
    sprintf(line[1], "SS: %-4x", Register::ss());
    sprintf(line[2], "SP: %-4x", Register::sp());
    print_table_row(3, 15, line, false);

    get_disassembly_line(disasm, sizeof(disasm), pc, memory_);
    printf("%s\n", disasm);

    sprintf(line[0], "B  %-4x", Register::bx());
    sprintf(line[1], "DS: %-4x", Register::ds());
    sprintf(line[2], "BP: %-4x", Register::bp());
    print_table_row(3, 15, line, false);

    get_disassembly_line(disasm, sizeof(disasm), pc, memory_);
    printf("%s\n", disasm);


    sprintf(line[0], "C  %-4x", Register::cx());
    sprintf(line[1], "ES: %-4x", Register::es());
    sprintf(line[2], "SI: %-4x", Register::si());
    print_table_row(3, 15, line, false);

    get_disassembly_line(disasm, sizeof(disasm), pc, memory_);
    printf("%s\n", disasm);


    sprintf(line[0], "D  %-4x", Register::dx());
    std::memset(line[1], 0, sizeof(line[1]));
    sprintf(line[1], "CS: %-4x", Register::cs());
    sprintf(line[2], "DI: %-4x", Register::di());
    print_table_row(3, 15, line, false);

    get_disassembly_line(disasm, sizeof(disasm), pc, memory_);
    printf("%s\n", disasm);


    puts_many(left_top_bottom, 1, false);
    puts_many(horizontal, 15, false);
    puts_many(left_top_right, 1, false);
    puts_many(horizontal, 4, false);
    puts_many(" FLAGS ", 1, false);
    puts_many(horizontal, 4, false);
    puts_many(left_top_right, 1, false);
    puts_many(horizontal, 15, false);
    puts_many(right_top_bottom, 1, false);

    get_disassembly_line(disasm, sizeof(disasm), pc, memory_);
    printf("%s\n", disasm);


    printf("%s  OF   DF   IF   TF   SF   ZF   AF   PF   CF   %s", vertical, vertical);

    get_disassembly_line(disasm, sizeof(disasm), pc, memory_);
    printf("%s\n", disasm);

    printf("%s  %1d    %1d    %1d    %1d    %1d    %1d    %1d    %1d    %1d    %s", vertical,
           Register::flags().o(), Register::flags().d(), Register::flags().i(), Register::flags().t(),
           Register::flags().s(), Register::flags().z(), Register::flags().ax(), Register::flags().p(),
           Register::flags().cy(), vertical);

    get_disassembly_line(disasm, sizeof(disasm), pc, memory_);
    printf("%s\n", disasm);

    print_table_bottom(0, 47);

    if (strlen(error_msg))
    {
        printf("ERROR: %s\n", error_msg);
    }
}

} // namespace msemu::cpu8086