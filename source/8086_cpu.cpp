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
        set_opcode(static_cast<uint8_t>(i), &Cpu8086::_unimpl, "-", 0);
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
        puts("");
}
void Cpu8086::dump() const
{
    constexpr const char* clear_screen = "\033[H\033[2J\033[3J";

    printf(clear_screen);
    const char* horizontal = "\u2500";
    const char* left_top   = "\u250c";
    const char* right_top  = "\u2510";
    const char* cross_top  = "\u252c";
    const char* vertical   = "\u2503";
    printf("P: %s\n", horizontal);
    printf("\n");
    printf("Current instruction: %x %s\n", regs_.ip, op_->name);
    puts_many(left_top, 1, false);
    puts_many(horizontal, 15, false);
    puts_many(cross_top, 1, false);
    puts_many(horizontal, 15, false);
    puts_many(cross_top, 1, false);
    puts_many(horizontal, 15, false);
    puts_many(right_top, 1);

    puts_many(vertical, 1, false);
    printf("  Reg  H  L    |    Segments    |   Pointers    |\n");
    printf("|    A  %-2x %-2x   |    SS: %-4x    |   SP: %-4x    |\n", regs_.acc.ax.h, regs_.acc.ax.l,
           regs_.ss, regs_.sp);
    printf("|    B  %-2x %-2x   |    DS: %-4x    |   BP: %-4x    |\n", regs_.acc.bx.h, regs_.acc.bx.l,
           regs_.ds, regs_.bp);
    printf("|    C  %-2x %-2x   |    ES: %-4x    |   SI: %-4x    |\n", regs_.acc.cx.h, regs_.acc.cx.l,
           regs_.es, regs_.si);
    printf("|    D  %-2x %-2x   |                |   DI: %-4x    |\n", regs_.acc.dx.h, regs_.acc.dx.l,
           regs_.di);
    printf("--------------------- FLAGS ----------------------\n");
    printf("|   OF   DF   IF   TF   SF   ZF   AF   PF   CF   |\n");
    printf("|   %1d    %1d    %1d    %1d    %1d    %1d    %1d    %1d    %1d    |\n", regs_.flags.o,
           regs_.flags.d, regs_.flags.i, regs_.flags.t, regs_.flags.s, regs_.flags.z, regs_.flags.a,
           regs_.flags.p, regs_.flags.c);
    printf("--------------------------------------------------\n");
}


} // namespace msemu
