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

#include "16_bit_modrm.hpp"
#include "8086_modrm.hpp"

namespace msemu
{
namespace cpu8086
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


Cpu::Instruction Cpu::opcodes_[255];

void Cpu::reset()
{
    std::memset(&regs_, 0, sizeof(regs_));
    op_ = &opcodes_[memory_.read8(0)];
}

Cpu::Cpu(MemoryBase& memory)
    : memory_(memory)
{
    for (uint16_t i = 0; i <= 255; ++i)
    {
        set_opcode(static_cast<uint8_t>(i), &Cpu::_unimpl);
    }
    set_opcode(0x00, &Cpu::_unimpl);

    // mov group
    set_opcode(0xa0, &Cpu::_mov_mem_to_reg<&Registers::ax, RegisterPart::low>);
    set_opcode(0xa1, &Cpu::_mov_mem_to_reg<&Registers::ax, RegisterPart::whole>);
    set_opcode(0xa2, &Cpu::_mov_reg_to_mem<&Registers::ax, RegisterPart::low>);
    set_opcode(0xa3, &Cpu::_mov_reg_to_mem<&Registers::ax, RegisterPart::whole>);


    set_opcode(0xb0, &Cpu::_mov_imm_to_reg<&Registers::ax, RegisterPart::low>);
    set_opcode(0xb1, &Cpu::_mov_imm_to_reg<&Registers::cx, RegisterPart::low>);
    set_opcode(0xb2, &Cpu::_mov_imm_to_reg<&Registers::dx, RegisterPart::low>);
    set_opcode(0xb3, &Cpu::_mov_imm_to_reg<&Registers::bx, RegisterPart::low>);
    set_opcode(0xb4, &Cpu::_mov_imm_to_reg<&Registers::ax, RegisterPart::high>);
    set_opcode(0xb5, &Cpu::_mov_imm_to_reg<&Registers::cx, RegisterPart::high>);
    set_opcode(0xb6, &Cpu::_mov_imm_to_reg<&Registers::dx, RegisterPart::high>);
    set_opcode(0xb7, &Cpu::_mov_imm_to_reg<&Registers::bx, RegisterPart::high>);

    set_opcode(0xb8, &Cpu::_mov_imm_to_reg<&Registers::ax, RegisterPart::whole>);
    set_opcode(0xb9, &Cpu::_mov_imm_to_reg<&Registers::cx, RegisterPart::whole>);
    set_opcode(0xba, &Cpu::_mov_imm_to_reg<&Registers::dx, RegisterPart::whole>);
    set_opcode(0xbb, &Cpu::_mov_imm_to_reg<&Registers::bx, RegisterPart::whole>);
    set_opcode(0xbc, &Cpu::_mov_imm_to_reg<&Registers::sp, RegisterPart::whole>);
    set_opcode(0xbd, &Cpu::_mov_imm_to_reg<&Registers::bp, RegisterPart::whole>);
    set_opcode(0xbe, &Cpu::_mov_imm_to_reg<&Registers::si, RegisterPart::whole>);
    set_opcode(0xbf, &Cpu::_mov_imm_to_reg<&Registers::di, RegisterPart::whole>);

    set_opcode(0x89, &Cpu::_mov_byte_modmr_to_reg);
    set_opcode(0x8c, &Cpu::_mov_sreg_to_reg);
    set_opcode(0x8e, &Cpu::_mov_reg_to_sreg);
    set_opcode(0xc3, &Cpu::_unimpl);

    reset();
#ifdef DUMP_CORE_STATE
    dump();
#endif
}

void Cpu::set_opcode(uint8_t id, uint8_t (Cpu::*fun)())
{
    opcodes_[id].impl = fun;
}

uint8_t Cpu::read_byte() const
{
    return memory_.read8(regs_.ip);
}

void Cpu::step()
{
    op_          = &opcodes_[memory_.read8(regs_.ip)];
    uint8_t size = (this->*op_->impl)();
    regs_.ip += size;
#ifdef DUMP_CORE_STATE
    dump();
#endif
}

uint8_t Cpu::_add()
{
    return 1;
}

uint16_t& Cpu::get_sreg(uint8_t id)
{
    switch (id)
    {
        case 0:
            return regs_.es;
        case 1:
            return regs_.cs;
        case 2:
            return regs_.ss;
        case 3:
            return regs_.ds;
    }

    static uint16_t dummy;
    return dummy;
}
uint16_t& Cpu::get_reg16_from_mod(uint8_t id)
{
    switch (id)
    {
        case 0:
            return regs_.ax;
        case 1:
            return regs_.cx;
        case 2:
            return regs_.dx;
        case 3:
            return regs_.bx;
        case 4:
            return regs_.sp;
        case 5:
            return regs_.bp;
        case 6:
            return regs_.si;
        case 7:
            return regs_.di;
    }

    static uint16_t dummy;
    return dummy;
}


uint8_t Cpu::_mov_reg_to_sreg()
{
    uint8_t data = memory_.read8(regs_.ip + 1);
    if (get_mod(data) == 0x3)
    {
        uint8_t sreg = get_reg_op(data) & 0x3;
        uint8_t reg  = get_rm(data);

        get_sreg(sreg) = get_reg16_from_mod(reg);
    }
    return 1;
}

uint8_t Cpu::_mov_sreg_to_reg()
{
    uint8_t data = memory_.read8(regs_.ip + 1);
    if (get_mod(data) == 0x3)
    {
        uint8_t sreg = get_reg_op(data) & 0x3;
        uint8_t reg  = get_rm(data);

        get_reg16_from_mod(reg) = get_sreg(sreg);
    }
    return 3;
}

template <uint16_t Registers::*reg, RegisterPart part>
uint8_t Cpu::_mov_imm_to_reg()
{
    uint16_t data;
    if constexpr (part == RegisterPart::whole)
    {
        data = memory_.read16(regs_.ip + 1);
    }
    else
    {
        data = memory_.read8(regs_.ip + 1);
    }

    set_register<part>(regs_.*reg, data);

    return part == RegisterPart::whole ? 3 : 2;
}


template <uint16_t Registers::*reg, RegisterPart part>
uint8_t Cpu::_mov_mem_to_reg()
{
    const uint16_t address = memory_.read16(regs_.ip + 1);
    if constexpr (part == RegisterPart::whole)
    {
        set_register<part>(regs_.*reg, memory_.read16(address));
    }
    else
    {
        set_register<part>(regs_.*reg, memory_.read8(address));
    }
    return 3;
}

template <uint16_t Registers::*reg, RegisterPart part>
uint8_t Cpu::_mov_reg_to_mem()
{
    const uint16_t address = memory_.read16(regs_.ip + 1);
    if constexpr (part == RegisterPart::whole)
    {
        memory_.write16(address, get_register<part>(regs_.*reg));
    }
    else
    {
        memory_.write8(address, static_cast<uint8_t>(get_register<part>(regs_.*reg)));
    }
    return 3;
}

uint8_t Cpu::_mov_byte_modmr_to_reg()
{
    uint8_t modmr = memory_.read8(regs_.ip + 1);

    const ModRM mod(modmr);
    uint8_t mode    = mod.mod == 0x01 || mod.mod == 0x02 ? 1 : mod.mod;
    uint16_t offset = 0;
    if (mode == 1)
    {
        offset = memory_.read8(regs_.ip + 1);
    }
    else if (mode == 2)
    {
        offset = memory_.read16(regs_.ip + 1);
    }
    auto from_address = modes.modes[mode][mod.rm](offset);
    auto to_reg       = modes.reg8[mod.reg];

    if (mod.reg > 3)
    {

        set_register<RegisterPart::high>(*(regs_.*to_reg), memory_.read8(from_address));
    }
    else
    {
        set_register<RegisterPart::low>(*(regs_.*to_reg), memory_.read8(from_address));
    }
}


uint8_t Cpu::_unimpl()
{
    snprintf(error_msg_, sizeof(error_msg_), "Opcode: 0x%x is unimplemented!\n",
             memory_.read8(regs_.ip));
    return 1;
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

uint8_t opcode_to_command(char* line, std::size_t max_size, std::size_t opcode,
                          uint8_t data[2])
{
    char mod0_mapping[8][9]  = {"[bx+si]", "[bx+di]", "[bp+si]", "[bp+di]",
                               "[si]",    "[di]",    "disp16",  "[bx]"};
    char reg8_mapping[8][3]  = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
    char reg16_mapping[8][3] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
    char sreg_mapping[4][3]  = {"es", "cs", "ss", "ds"};
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
            snprintf(line, max_size, "mov %s,0x%02x", reg8_mapping[opcode & 0xf],
                     data[0]);
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
            snprintf(line, max_size, "mov %s,0x%02x%02x",
                     reg16_mapping[(opcode & 0xf) - 8], data[1], data[0]);
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

void Cpu::get_disassembly_line(char* line, std::size_t max_size,
                               std::size_t& program_counter) const
{
    uint8_t pc = memory_.read8(program_counter);

    uint8_t data[6] = {};
    for (uint8_t i = 1; i < sizeof(data); ++i)
    {
        data[i - 1] = memory_.read8(program_counter + i);
    }

    char command[30];
    uint8_t size = opcode_to_command(command, sizeof(command), pc, data);

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
        snprintf(line, max_size, " %c %8lx: %02x        | %s", cursor, program_counter,
                 pc, command);
    }
    if (size == 2)
    {
        snprintf(line, max_size, " %c %8lx: %02x %02x     | %s", cursor, program_counter,
                 pc, data[0], command);
    }
    if (size == 3)
    {
        snprintf(line, max_size, " %c %8lx: %02x %02x %02x  | %s", cursor,
                 program_counter, pc, data[0], data[1], command);
    }
    program_counter += size;
}

void Cpu::dump() const
{
    constexpr const char* clear_screen = "\033[H\033[2J\033[3J";

    printf(clear_screen);
    printf("IP: %x\n", regs_.ip);

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

    sprintf(line[0], "A  %-2x %-2x", get_register<RegisterPart::high>(regs_.ax),
            get_register<RegisterPart::low>(regs_.ax));
    sprintf(line[1], "SS: %-4x", regs_.ss);
    sprintf(line[2], "SP: %-4x", regs_.sp);
    print_table_row(3, 15, line, false);

    get_disassembly_line(disasm, sizeof(disasm), pc);
    printf("%s\n", disasm);

    sprintf(line[0], "B  %-2x %-2x", get_register<RegisterPart::high>(regs_.bx),
            get_register<RegisterPart::low>(regs_.bx));
    sprintf(line[1], "DS: %-4x", regs_.ds);
    sprintf(line[2], "BP: %-4x", regs_.bp);
    print_table_row(3, 15, line, false);

    get_disassembly_line(disasm, sizeof(disasm), pc);
    printf("%s\n", disasm);


    sprintf(line[0], "C  %-2x %-2x", get_register<RegisterPart::high>(regs_.cx),
            get_register<RegisterPart::low>(regs_.cx));
    sprintf(line[1], "ES: %-4x", regs_.es);
    sprintf(line[2], "SI: %-4x", regs_.si);
    print_table_row(3, 15, line, false);

    get_disassembly_line(disasm, sizeof(disasm), pc);
    printf("%s\n", disasm);


    sprintf(line[0], "D  %-2x %-2x", get_register<RegisterPart::high>(regs_.dx),
            get_register<RegisterPart::low>(regs_.dx));
    std::memset(line[1], 0, sizeof(line[1]));
    sprintf(line[1], "CS: %-4x", regs_.cs);
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

    printf("%s  %1d    %1d    %1d    %1d    %1d    %1d    %1d    %1d    %1d    %s",
           vertical, regs_.flags.o, regs_.flags.d, regs_.flags.i, regs_.flags.t,
           regs_.flags.s, regs_.flags.z, regs_.flags.a, regs_.flags.p, regs_.flags.c,
           vertical);

    get_disassembly_line(disasm, sizeof(disasm), pc);
    printf("%s\n", disasm);

    print_table_bottom(0, 47);

    if (strlen(error_msg_))
    {
        printf("ERROR: %s\n", error_msg_);
    }
}

} // namespace cpu8086
} // namespace msemu
