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

#include "16_bit_modrm.hpp"
#include "8086_modrm.hpp"
#include "8086_registers.hpp"
#include "memory.hpp"

namespace msemu
{
namespace cpu8086
{

struct InstructionCost
{
    uint8_t size;
    uint8_t cycles;
};

template <typename MemoryType>
class Cpu
{
public:
    Cpu(MemoryType& memory)
        : op_{}
        , regs_{}
        , last_instruction_cost_{0}
        , error_msg_{}
        , memory_{memory}
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

        set_opcode(0xc6, &Cpu::_mov_byte_imm_to_modmr<uint8_t>);
        set_opcode(0xc7, &Cpu::_mov_byte_imm_to_modmr<uint16_t>);


        set_opcode(0x8a, &Cpu::_mov_byte_modmr_to_reg<uint8_t>);
        set_opcode(0x8b, &Cpu::_mov_byte_modmr_to_reg<uint16_t>);
        set_opcode(0x88, &Cpu::_mov_byte_reg_to_modmr<uint8_t>);
        set_opcode(0x89, &Cpu::_mov_byte_reg_to_modmr<uint16_t>);


        set_opcode(0x8c, &Cpu::_mov_sreg_to_reg);
        set_opcode(0x8e, &Cpu::_mov_reg_to_sreg);
        set_opcode(0xc3, &Cpu::_unimpl);

        reset();
#ifdef DUMP_CORE_STATE
        dump();
#endif
    }

    void step()
    {
        op_                  = &opcodes_[memory_.template read<uint8_t>(regs_.ip)];
        InstructionCost cost = (this->*op_->impl)();
        regs_.ip += cost.size;

        // TODO: align cycles
        last_instruction_cost_ = cost.cycles;
#ifdef DUMP_CORE_STATE
        dump();
#endif
    }

    void reset()
    {
        std::memset(&regs_, 0, sizeof(regs_));
        op_ = &opcodes_[memory_.template read<uint8_t>(0)];
    }

protected:
    // configuration
    void set_opcode(uint8_t id, InstructionCost (Cpu::*fun)(void))
    {
        opcodes_[id].impl = fun;
    }

    // debug
    void get_disassembly_line(char* line, std::size_t max_size,
                              std::size_t& program_counter) const;
    void dump() const;


    // core emulation

    InstructionCost _unimpl()
    {
        snprintf(error_msg_, sizeof(error_msg_), "Opcode: 0x%x is unimplemented!\n",
                 memory_.template read<uint8_t>(regs_.ip));
        return {.size = 1, .cycles = 0};
    }

    template <uint16_t Registers::*reg, RegisterPart part>
    InstructionCost _mov_imm_to_reg()
    {
        using DataType      = RegisterDataType<part>::type;
        const DataType data = memory_.template read<DataType>(regs_.ip + 1);
        set_register<part>(regs_.*reg, data);

        if constexpr (part == RegisterPart::whole)
            return {.size = 3, .cycles = 4};
        else
            return {.size = 2, .cycles = 4};
    }

    template <uint16_t Registers::*reg, RegisterPart part>
    InstructionCost _mov_mem_to_reg()
    {
        const uint16_t address = memory_.template read<uint16_t>(regs_.ip + 1);
        if constexpr (part == RegisterPart::whole)
        {
            set_register<part>(regs_.*reg, memory_.template read<uint16_t>(address));
        }
        else
        {
            set_register<part>(regs_.*reg, memory_.template read<uint8_t>(address));
        }
        if constexpr (reg == &Registers::ax)
        {
            return {.size = 3, .cycles = 10};
        }
        else
        {
            return {.size = 3, .cycles = 8 + get_cost(AccessCost::Direct)};
        }
    }
    template <uint16_t Registers::*reg, RegisterPart part>
    InstructionCost _mov_reg_to_mem()
    {
        const uint16_t address = memory_.template read<uint16_t>(regs_.ip + 1);
        if constexpr (part == RegisterPart::whole)
        {
            memory_.write(address, get_register<part>(regs_.*reg));
        }
        else
        {
            memory_.write(address, static_cast<uint8_t>(get_register<part>(regs_.*reg)));
        }
        if constexpr (reg == &Registers::ax)
        {
            return {.size = 3, .cycles = 10};
        }
        else
        {
            return {.size = 3, .cycles = 9 + get_cost(AccessCost::Direct)};
        }
    }

    template <typename T>
    InstructionCost _mov_byte_reg_to_modmr()
    {
        uint8_t modmr = memory_.template read<uint8_t>(regs_.ip + 1);
        const ModRM mod(modmr);
        uint8_t mode             = mod.mod == 0x01 || mod.mod == 0x02 ? 1 : mod.mod;
        uint16_t offset          = 0;
        uint8_t instruction_size = 2;
        if (mod.mod == 0 && mod.rm == 0x06)
        {
            offset           = memory_.template read<uint16_t>(regs_.ip + 2);
            instruction_size = 4;
        }
        else if (mod.mod == 1)
        {
            offset           = memory_.template read<uint8_t>(regs_.ip + 2);
            instruction_size = 3;
        }
        else if (mod.mod == 2)
        {
            offset           = memory_.template read<uint16_t>(regs_.ip + 2);
            instruction_size = 4;
        }
        else if (mod.mod == 3)
        {
            offset           = 0;
            instruction_size = 2;
        }

        if constexpr (std::is_same_v<uint8_t, T>)
        {
            auto from_reg = modes.reg8[mod.reg];
            if (mode < 3)
            {
                auto to_address = modes.modes[mode][mod.rm](regs_, offset);

                if (mod.reg > 3)
                {
                    memory_.write(to_address, get_register<RegisterPart::high, uint8_t>(
                                                  regs_.*from_reg));
                }
                else
                {
                    memory_.write(to_address, get_register<RegisterPart::low, uint8_t>(
                                                  regs_.*from_reg));
                }
            }
            else // reg to reg
            {
                auto to_reg = modes.reg8[mod.rm];
                auto from_value =
                    mod.reg > 3
                        ? get_register<RegisterPart::high, uint8_t>(regs_.*from_reg)
                        : get_register<RegisterPart::low, uint8_t>(regs_.*from_reg);


                if (mod.rm > 3)
                {
                    set_register<RegisterPart::high>(regs_.*to_reg, from_value);
                }
                else
                {
                    set_register<RegisterPart::low>(regs_.*to_reg, from_value);
                }
            }
        }
        else
        {
            auto from_reg = modes.reg16[mod.reg];
            if (mode < 3)
            {
                auto to_address = modes.modes[mode][mod.rm](regs_, offset);
                const auto value =
                    get_register<RegisterPart::whole, uint16_t>(regs_.*from_reg);
                memory_.write(to_address, value);
            }
            else // reg to reg
            {
                auto to_reg     = modes.reg16[mod.rm];
                auto from_value = get_register<RegisterPart::whole>(regs_.*from_reg);


                set_register<RegisterPart::whole>(regs_.*to_reg, from_value);
            }
        }

        return {.size   = instruction_size,
                .cycles = static_cast<uint8_t>(9 + modes.costs[mod.mod][mod.rm])};
    }

    template <typename T>
    InstructionCost _mov_byte_modmr_to_reg()
    {
        uint8_t modmr = memory_.template read<uint8_t>(regs_.ip + 1);
        const ModRM mod(modmr);
        uint8_t mode             = mod.mod == 0x01 || mod.mod == 0x02 ? 1 : mod.mod;
        uint16_t offset          = 0;
        uint8_t instruction_size = 2;
        if (mod.mod == 0 && mod.rm == 0x06)
        {
            offset           = memory_.template read<uint16_t>(regs_.ip + 2);
            instruction_size = 4;
        }
        else if (mod.mod == 1)
        {
            offset           = memory_.template read<uint8_t>(regs_.ip + 2);
            instruction_size = 3;
        }
        else if (mod.mod == 2)
        {
            offset           = memory_.template read<uint16_t>(regs_.ip + 2);
            instruction_size = 4;
        }
        else if (mod.mod == 3)
        {
            offset           = 0;
            instruction_size = 2;
        }


        auto to_reg = modes.reg8[mod.reg];
        if constexpr (std::is_same_v<uint16_t, T>)
        {
            to_reg = modes.reg16[mod.reg];
        }
        if (mode < 3)
        {
            auto from_address = modes.modes[mode][mod.rm](regs_, offset);
            if constexpr (std::is_same_v<uint8_t, T>)
            {

                if (mod.reg > 3)
                {
                    set_register<RegisterPart::high>(
                        regs_.*to_reg, memory_.template read<uint8_t>(from_address));
                }
                else
                {
                    set_register<RegisterPart::low>(
                        regs_.*to_reg, memory_.template read<uint8_t>(from_address));
                }
            }
            else
            {
                set_register<RegisterPart::whole>(
                    regs_.*to_reg, memory_.template read<uint16_t>(from_address));
            }
        }
        else // reg to reg
        {
            if constexpr (std::is_same_v<uint8_t, T>)
            {
                auto from_reg   = modes.reg8[mod.rm];
                auto from_value = mod.rm > 3
                                      ? get_register<RegisterPart::high>(regs_.*from_reg)
                                      : get_register<RegisterPart::low>(regs_.*from_reg);
                if (mod.reg > 3)
                {
                    set_register<RegisterPart::high>(regs_.*to_reg, from_value);
                }
                else
                {
                    set_register<RegisterPart::low>(regs_.*to_reg, from_value);
                }
            }
            else
            {
                auto from_reg    = modes.reg16[mod.rm];
                const auto value = get_register<RegisterPart::whole>(regs_.*from_reg);
                set_register<RegisterPart::whole>(regs_.*to_reg, value);
            }
        }
        return {.size   = instruction_size,
                .cycles = static_cast<uint8_t>(8 + modes.costs[mod.mod][mod.rm])};
    }

    template <typename T>
    InstructionCost _mov_byte_imm_to_modmr()
    {
        uint8_t ip_offset = 1;
        uint8_t modmr     = memory_.template read<uint8_t>(regs_.ip + ip_offset);
        const ModRM mod(modmr);
        uint8_t mode             = mod.mod == 0x01 || mod.mod == 0x02 ? 1 : mod.mod;
        uint16_t offset          = 0;
        uint8_t instruction_size = 0;
        if constexpr (std::is_same_v<uint16_t, T>)
        {
            instruction_size = 4;
        }
        else
        {
            instruction_size = 3;
        }

        ip_offset += 1;
        if (mod.mod == 0 && mod.rm == 0x06)
        {
            offset = memory_.template read<uint16_t>(regs_.ip + ip_offset);
            ip_offset += 2;
            if constexpr (std::is_same_v<T, uint16_t>)
            {
                instruction_size = 6;
            }
            else
            {
                instruction_size = 5;
            }
        }
        else if (mod.mod == 1)
        {
            offset = memory_.template read<uint8_t>(regs_.ip + ip_offset);
            ip_offset += 1;
            if constexpr (std::is_same_v<T, uint16_t>)
            {
                instruction_size = 5;
            }
            else
            {
                instruction_size = 4;
            }
        }
        else if (mod.mod == 2)
        {
            offset = memory_.template read<uint16_t>(regs_.ip + ip_offset);
            ip_offset += 2;
            if constexpr (std::is_same_v<T, uint16_t>)
            {
                instruction_size = 6;
            }
            else
            {
                instruction_size = 5;
            }
        }
        else if (mod.mod == 3)
        {
            offset = 0;
            if constexpr (std::is_same_v<T, uint16_t>)
            {
                instruction_size = 4;
            }
            else
            {
                instruction_size = 3;
            }
        }
        T data = {};
        if constexpr (std::is_same_v<T, uint16_t>)
        {
            data = memory_.template read<uint16_t>(regs_.ip + ip_offset);
        }
        else
        {
            data = memory_.template read<uint8_t>(regs_.ip + ip_offset);
        }

        uint8_t operation_cost = 0;
        if (mode < 3)
        {
            auto to_address = modes.modes[mode][mod.rm](regs_, offset);
            if constexpr (std::is_same_v<T, uint16_t>)
            {
                memory_.write(to_address, data);
            }
            else
            {
                memory_.write(to_address, data);
            }


            operation_cost = 10 + modes.costs[mod.mod][mod.rm];
        }
        else // imm to reg
        {

            if constexpr (std::is_same_v<uint8_t, T>)
            {
                auto to_reg = modes.reg8[mod.rm];
                if (mod.rm > 3)
                {
                    set_register<RegisterPart::high>(regs_.*to_reg, data);
                }
                else
                {
                    set_register<RegisterPart::low>(regs_.*to_reg, data);
                }
            }
            else
            {
                auto to_reg = modes.reg16[mod.rm];

                set_register<RegisterPart::whole>(regs_.*to_reg, data);
            }
            operation_cost = 4;
        }

        return {.size = instruction_size, .cycles = operation_cost};
    }

    InstructionCost _mov_reg_to_sreg()
    {
        uint8_t modmr = memory_.template read<uint8_t>(regs_.ip + 1);
        const ModRM mod(modmr);
        uint8_t mode             = mod.mod == 0x01 || mod.mod == 0x02 ? 1 : mod.mod;
        uint16_t offset          = 0;
        uint8_t instruction_size = 2;
        if (mod.mod == 0 && mod.rm == 0x06)
        {
            offset           = memory_.template read<uint16_t>(regs_.ip + 2);
            instruction_size = 4;
        }
        else if (mod.mod == 1)
        {
            offset           = memory_.template read<uint8_t>(regs_.ip + 2);
            instruction_size = 3;
        }
        else if (mod.mod == 2)
        {
            offset           = memory_.template read<uint16_t>(regs_.ip + 2);
            instruction_size = 4;
        }
        else if (mod.mod == 3)
        {
            offset           = 0;
            instruction_size = 2;
        }


        auto from_sreg = modes.sreg[mod.reg];
        uint8_t cost   = 2;
        if (mode < 3)
        {
            auto to_address  = modes.modes[mode][mod.rm](regs_, offset);
            const auto value = get_register<RegisterPart::whole>(regs_.*from_sreg);
            memory_.write(to_address, value);
            cost = 9 + modes.costs[mod.mod][mod.rm];
        }
        else // reg to reg
        {
            auto to_reg      = modes.reg16[mod.rm];
            const auto value = get_register<RegisterPart::whole>(regs_.*from_sreg);
            set_register<RegisterPart::whole>(regs_.*to_reg, value);
        }
        return {.size = instruction_size, .cycles = cost};
    }

    InstructionCost _mov_sreg_to_reg()
    {
        const uint8_t modmr = memory_.template read<uint8_t>(regs_.ip + 1);
        const ModRM mod(modmr);
        uint8_t mode             = mod.mod == 0x01 || mod.mod == 0x02 ? 1 : mod.mod;
        uint16_t offset          = 0;
        uint8_t instruction_size = 2;
        if (mod.mod == 0 && mod.rm == 0x06)
        {
            offset           = memory_.template read<uint16_t>(regs_.ip + 2);
            instruction_size = 4;
        }
        else if (mod.mod == 1)
        {
            offset           = memory_.template read<uint8_t>(regs_.ip + 2);
            instruction_size = 3;
        }
        else if (mod.mod == 2)
        {
            offset           = memory_.template read<uint16_t>(regs_.ip + 2);
            instruction_size = 4;
        }
        else if (mod.mod == 3)
        {
            offset           = 0;
            instruction_size = 2;
        }


        auto to_sreg = modes.sreg[mod.reg];
        uint8_t cost = 2;
        if (mode < 3)
        {
            auto from_address = modes.modes[mode][mod.rm](regs_, offset);
            set_register<RegisterPart::whole>(
                regs_.*to_sreg, memory_.template read<uint16_t>(from_address));
            cost = 9 + modes.costs[mod.mod][mod.rm];
        }
        else // reg to reg
        {
            auto from_reg    = modes.reg16[mod.rm];
            const auto value = get_register<RegisterPart::whole>(regs_.*from_reg);
            set_register<RegisterPart::whole>(regs_.*to_sreg, value);
        }
        return {.size = instruction_size, .cycles = cost};
    }


    struct MoveOperand
    {
        uint8_t& from;
        uint8_t& to;
    };

    struct Instruction
    {
        typedef InstructionCost (Cpu::*fun)(void);
        fun impl;
    };

    Instruction* op_;
    Registers regs_;
    uint8_t last_instruction_cost_;
    char error_msg_[100];
    static inline Instruction opcodes_[255];
    MemoryType& memory_;
};

} // namespace cpu8086
} // namespace msemu
