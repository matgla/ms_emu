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

#include "8086_cpu.hpp"

#include <cstdio>
#include <cstring>
#include <cwchar>

namespace msemu
{

Cpu8086::Instruction Cpu8086::opcodes_[255];

void Cpu8086::reset()
{
    std::memset(&regs_, 0, sizeof(regs_));
    op_ = &opcodes_[memory_.read8(0)];
}

Cpu8086::Cpu8086(MemoryBase& memory)
    : memory_(memory)
{
    for (uint16_t i = 0; i <= 255; ++i)
    {
        set_opcode(static_cast<uint8_t>(i), &Cpu8086::_unimpl, "-", 1);
    }
    set_opcode(0x00, &Cpu8086::_unimpl, "add", 2);

    // mov group
    set_opcode(0xb0, &Cpu8086::_mov8_to_reg<al_offset>, "mov", 2);
    set_opcode(0xb1, &Cpu8086::_mov8_to_reg<cl_offset>, "mov", 2);
    set_opcode(0xb2, &Cpu8086::_mov8_to_reg<dl_offset>, "mov", 2);
    set_opcode(0xb3, &Cpu8086::_mov8_to_reg<bl_offset>, "mov", 2);
    set_opcode(0xb4, &Cpu8086::_mov8_to_reg<ah_offset>, "mov", 2);
    set_opcode(0xb5, &Cpu8086::_mov8_to_reg<ch_offset>, "mov", 2);
    set_opcode(0xb6, &Cpu8086::_mov8_to_reg<dh_offset>, "mov", 2);
    set_opcode(0xb7, &Cpu8086::_mov8_to_reg<bh_offset>, "mov", 2);

    set_opcode(0xb8, &Cpu8086::_mov8_to_reg<al_offset>, "mov", 3);
    set_opcode(0xb9, &Cpu8086::_mov8_to_reg<cl_offset>, "mov", 3);
    set_opcode(0xba, &Cpu8086::_mov8_to_reg<dl_offset>, "mov", 3);
    set_opcode(0xbb, &Cpu8086::_mov8_to_reg<bl_offset>, "mov", 3);
    set_opcode(0xbc, &Cpu8086::_mov8_to_reg<ah_offset>, "mov", 3);
    set_opcode(0xbd, &Cpu8086::_mov8_to_reg<ch_offset>, "mov", 3);
    set_opcode(0xbe, &Cpu8086::_mov8_to_reg<dh_offset>, "mov", 3);
    set_opcode(0xbf, &Cpu8086::_mov8_to_reg<bh_offset>, "mov", 3);

    set_opcode(0x8e, &Cpu8086::_mov8_to_reg<al_offset>, "mov", 2);


    reset();
#ifdef DUMP_CORE_STATE
    dump();
#endif
}

void Cpu8086::set_opcode(uint8_t id, void (Cpu8086::*fun)(), const char* name, uint8_t size)
{
    opcodes_[id].size = size;
    opcodes_[id].impl = fun;
    opcodes_[id].name = name;
}

uint8_t Cpu8086::read_byte() const
{
    return memory_.read8(regs_.ip);
}

void Cpu8086::step()
{
    op_ = &opcodes_[memory_.read8(regs_.ip)];
    (this->*op_->impl)();
#ifdef DUMP_CORE_STATE
    dump();
#endif
}

void Cpu8086::_add()
{
}

template <uint8_t reg_offset>
void Cpu8086::_mov8_to_reg()
{
    regs_.regs_8[reg_offset] = memory_.read8(regs_.ip + 1);
    regs_.ip += op_->size;
}

void Cpu8086::_unimpl()
{
    printf("Opcode: 0x%x [%s] is unimplemented!\n", regs_.ip, op_->name);
}

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

void opcode_to_command(char* line, std::size_t max_size, std::size_t opcode, uint8_t data[2])
{
    char reg8_mapping[8][3]  = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
    char reg16_mapping[8][3] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};

    switch (opcode)
    {
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
}

void Cpu8086::get_disassembly_line(char* line, std::size_t max_size, std::size_t& program_counter) const
{
    uint8_t pc = memory_.read8(program_counter);

    uint8_t size = opcodes_[pc].size;


    uint8_t data[2] = {};
    for (uint8_t i = 1; i < size; ++i)
    {
        data[i - 1] = memory_.read8(program_counter + i);
    }

    char command[30];
    opcode_to_command(command, sizeof(command), pc, data);

    std::memset(line, 0, max_size);
    char cursor;
    if (program_counter == regs_.ip)
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

void Cpu8086::dump() const
{
    constexpr const char* clear_screen = "\033[H\033[2J\033[3J";

    printf(clear_screen);
    printf("P: %s\n", horizontal);
    printf("\n");

    print_table_top(3, 15, false);
    char disasm[255];
    std::size_t pc = regs_.ip;
    if (pc < 6)
        pc = 0;
    else
        pc -= 6;

    get_disassembly_line(disasm, sizeof(disasm), pc);
    printf("%s\n", disasm);
    char line[3][20] = {"REG  H  L  ", "Segments", "Pointers"};
    print_table_row(3, 15, line, false);
    get_disassembly_line(disasm, sizeof(disasm), pc);
    printf("%s\n", disasm);

    sprintf(line[0], "A  %-2x %-2x", regs_.acc.ax.h, regs_.acc.ax.l);
    sprintf(line[1], "SS: %-4x", regs_.ss);
    sprintf(line[2], "SP: %-4x", regs_.sp);
    print_table_row(3, 15, line, false);

    get_disassembly_line(disasm, sizeof(disasm), pc);
    printf("%s\n", disasm);

    sprintf(line[0], "B  %-2x %-2x", regs_.acc.bx.h, regs_.acc.bx.l);
    sprintf(line[1], "DS: %-4x", regs_.ds);
    sprintf(line[2], "BP: %-4x", regs_.bp);
    print_table_row(3, 15, line, false);

    get_disassembly_line(disasm, sizeof(disasm), pc);
    printf("%s\n", disasm);


    sprintf(line[0], "C  %-2x %-2x", regs_.acc.cx.h, regs_.acc.cx.l);
    sprintf(line[1], "ES: %-4x", regs_.es);
    sprintf(line[2], "SI: %-4x", regs_.si);
    print_table_row(3, 15, line, false);

    get_disassembly_line(disasm, sizeof(disasm), pc);
    printf("%s\n", disasm);


    sprintf(line[0], "D  %-2x %-2x", regs_.acc.dx.h, regs_.acc.dx.l);
    std::memset(line[1], 0, sizeof(line[1]));
    sprintf(line[2], "DI: %-4x", regs_.di);
    print_table_row(3, 15, line, false);

    get_disassembly_line(disasm, sizeof(disasm), pc);
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

    get_disassembly_line(disasm, sizeof(disasm), pc);
    printf("%s\n", disasm);


    printf("%s  OF   DF   IF   TF   SF   ZF   AF   PF   CF   %s", vertical, vertical);

    get_disassembly_line(disasm, sizeof(disasm), pc);
    printf("%s\n", disasm);

    printf("%s  %1d    %1d    %1d    %1d    %1d    %1d    %1d    %1d    %1d    %s", vertical, regs_.flags.o,
           regs_.flags.d, regs_.flags.i, regs_.flags.t, regs_.flags.s, regs_.flags.z, regs_.flags.a,
           regs_.flags.p, regs_.flags.c, vertical);

    get_disassembly_line(disasm, sizeof(disasm), pc);
    printf("%s\n", disasm);

    print_table_bottom(0, 47);
}

} // namespace msemu
