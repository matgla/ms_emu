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

#include "8086_registers.hpp"
#include "memory.hpp"

namespace msemu
{
namespace cpu8086
{
class Cpu
{
public:
    Cpu(MemoryBase& memory);

    void step();
    void reset();

protected:
    uint16_t& get_sreg(uint8_t id);
    uint16_t& get_reg16_from_mod(uint8_t id);

    void get_disassembly_line(char* line, std::size_t max_size,
                              std::size_t& program_counter) const;
    void dump() const;

    uint8_t read_byte() const;
    void set_opcode(uint8_t id, uint8_t (Cpu::*fun)(void));


    uint8_t _add();
    uint8_t _unimpl();

    template <uint16_t Registers::*reg, RegisterPart part>
    uint8_t _mov_imm_to_reg();
    template <uint16_t Registers::*reg, RegisterPart part>
    uint8_t _mov_mem_to_reg();

    uint8_t _mov_byte_modmr_to_reg();

    uint8_t _mov_sreg_to_reg();
    uint8_t _mov_reg_to_sreg();

    template <uint16_t Registers::*reg, RegisterPart part>
    uint8_t _mov_reg_to_mem();


    struct MoveOperand
    {
        uint8_t& from;
        uint8_t& to;
    };

    struct Instruction
    {
        typedef uint8_t (Cpu::*fun)(void);
        fun impl;
    };

    Instruction* op_;
    Registers regs_;
    char error_msg_[100];
    static Instruction opcodes_[255];
    MemoryBase& memory_;
};

} // namespace cpu8086
} // namespace msemu
