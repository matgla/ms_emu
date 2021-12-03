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

template <typename MemoryType>
class Cpu
{
public:
    Cpu(MemoryType& memory)
        : op_{}
        , last_instruction_cost_{0}
        , error_msg_{}
        , memory_{memory}
    {
        for (uint16_t i = 0; i <= 255; ++i)
        {
            set_opcode(static_cast<uint8_t>(i), &Cpu::_unimpl);
        }

        // mov group
        set_opcode(0xa0, &Cpu::_mov_mem_to_reg<Register::al_id, uint8_t>);
        set_opcode(0xa1, &Cpu::_mov_mem_to_reg<Register::ax_id, uint16_t>);
        set_opcode(0xa2, &Cpu::_mov_reg_to_mem<Register::al_id, uint8_t>);
        set_opcode(0xa3, &Cpu::_mov_reg_to_mem<Register::ax_id, uint16_t>);


        set_opcode(0xb0, &Cpu::_mov_imm_to_reg<Register::al_id, uint8_t>);
        set_opcode(0xb1, &Cpu::_mov_imm_to_reg<Register::cl_id, uint8_t>);
        set_opcode(0xb2, &Cpu::_mov_imm_to_reg<Register::dl_id, uint8_t>);
        set_opcode(0xb3, &Cpu::_mov_imm_to_reg<Register::bl_id, uint8_t>);

        set_opcode(0xb4, &Cpu::_mov_imm_to_reg<Register::ah_id, uint8_t>);
        set_opcode(0xb5, &Cpu::_mov_imm_to_reg<Register::ch_id, uint8_t>);
        set_opcode(0xb6, &Cpu::_mov_imm_to_reg<Register::dh_id, uint8_t>);
        set_opcode(0xb7, &Cpu::_mov_imm_to_reg<Register::bh_id, uint8_t>);

        set_opcode(0xb8, &Cpu::_mov_imm_to_reg<Register::ax_id, uint16_t>);
        set_opcode(0xb9, &Cpu::_mov_imm_to_reg<Register::cx_id, uint16_t>);
        set_opcode(0xba, &Cpu::_mov_imm_to_reg<Register::dx_id, uint16_t>);
        set_opcode(0xbb, &Cpu::_mov_imm_to_reg<Register::bx_id, uint16_t>);

        set_opcode(0xbc, &Cpu::_mov_imm_to_reg<Register::sp_id, uint16_t>);
        set_opcode(0xbd, &Cpu::_mov_imm_to_reg<Register::bp_id, uint16_t>);
        set_opcode(0xbe, &Cpu::_mov_imm_to_reg<Register::si_id, uint16_t>);
        set_opcode(0xbf, &Cpu::_mov_imm_to_reg<Register::di_id, uint16_t>);

        set_opcode(0xc6, &Cpu::_mov_byte_imm_to_modmr<uint8_t>);
        set_opcode(0xc7, &Cpu::_mov_byte_imm_to_modmr<uint16_t>);
        set_opcode(0x8a, &Cpu::_mov_byte_modmr_to_reg<uint8_t>);
        set_opcode(0x8b, &Cpu::_mov_byte_modmr_to_reg<uint16_t>);
        set_opcode(0x88, &Cpu::_mov_byte_reg_to_modmr<uint8_t>);
        set_opcode(0x89, &Cpu::_mov_byte_reg_to_modmr<uint16_t>);
        set_opcode(0x8c, &Cpu::_mov_sreg_to_modrm);
        set_opcode(0x8e, &Cpu::_mov_modrm_to_sreg);

        set_opcode(0xc3, &Cpu::_unimpl);

        reset();
#ifdef DUMP_CORE_STATE
        dump();
#endif
    }

    void step()
    {
        op_ = &opcodes_[memory_.template read<uint8_t>(Register::ip())];
        (this->*op_->impl)();
#ifdef DUMP_CORE_STATE
        dump();
#endif
    }

    void reset()
    {
        Register::reset();
        op_ = &opcodes_[memory_.template read<uint8_t>(0)];
    }

protected:
    // configuration
    void set_opcode(uint8_t id, void (Cpu::*fun)(void))
    {
        opcodes_[id].impl = fun;
    }

    // debug
    void get_disassembly_line(char* line, std::size_t max_size, std::size_t& program_counter) const;
    void dump() const;


    // core emulation

    void _unimpl()
    {
        snprintf(error_msg_, sizeof(error_msg_), "Opcode: 0x%x is unimplemented!\n",
                 memory_.template read<uint8_t>(Register::ip()));
        last_instruction_cost_ = 0;
    }

    template <uint32_t reg, typename T>
    void _mov_imm_to_reg()
    {
        Register::increment_ip(1);
        const T data = memory_.template read<T>(Register::ip());
        Register::increment_ip(sizeof(T));
        set_register_by_id<reg>(data);
        last_instruction_cost_ = 4;
    }

    template <uint32_t reg, typename T>
    void _mov_mem_to_reg()
    {
        Register::increment_ip(1);
        const uint16_t address = memory_.template read<uint16_t>(Register::ip());
        Register::increment_ip(2);

        const T value = memory_.template read<T>(address);

        set_register_by_id<reg>(value);
        if constexpr (reg == Register::ax_id || reg == Register::al_id || reg == Register::ah_id)
        {
            last_instruction_cost_ = 14;
        }
        else
        {
            last_instruction_cost_ = 13 + get_cost(AccessCost::Direct);
        }
    }

    template <uint32_t reg, typename T>
    void _mov_reg_to_mem()
    {
        Register::increment_ip(1);
        const uint16_t address = memory_.template read<uint16_t>(Register::ip());
        Register::increment_ip(2);
        const T value = get_register_by_id<reg, T>();
        memory_.write(address, value);

        if constexpr (reg == Register::al_id || reg == Register::ah_id || reg == Register::ax_id)
        {
            last_instruction_cost_ = 14;
        }
        else
        {
            last_instruction_cost_ = 13 + get_cost(AccessCost::Direct);
        }
    }

    template <typename T>
    void _mov_byte_reg_to_modmr()
    {
        Register::increment_ip(1);
        uint8_t modmr = memory_.template read<uint8_t>(Register::ip());
        Register::increment_ip(1);
        const ModRM mod(modmr);
        uint8_t mode    = mod.mod == 0x01 || mod.mod == 0x02 ? 1 : mod.mod;
        uint16_t offset = 0;
        if (mod.mod == 0 && mod.rm == 0x06)
        {
            offset = memory_.template read<uint16_t>(Register::ip());
            Register::increment_ip(2);
        }
        else if (mod.mod == 1)
        {
            offset = memory_.template read<uint8_t>(Register::ip());
            Register::increment_ip(1);
        }
        else if (mod.mod == 2)
        {
            offset = memory_.template read<uint16_t>(Register::ip());
            Register::increment_ip(2);
        }
        else if (mod.mod == 3)
        {
            offset = 0;
        }

        uint32_t from_reg = 0;
        uint32_t to_reg   = 0;

        if constexpr (std::is_same_v<uint8_t, T>)
        {
            from_reg = mod8_id(mod.reg);
            to_reg   = mod8_id(mod.rm);
        }
        else
        {
            from_reg = mod16_id(mod.reg);
            to_reg   = mod16_id(mod.rm);
        }


        if (mode < 3)
        {
            auto to_address = modes.modes[mode][mod.rm](offset);

            const T value = get_register_by_id<T>(from_reg);
            memory_.write(to_address, value);

            last_instruction_cost_ = static_cast<uint16_t>(13 + modes.costs[mod.mod][mod.rm]);
        }
        else // reg to reg
        {
            const T value = get_register_by_id<T>(from_reg);
            set_register_by_id(to_reg, value);
            last_instruction_cost_ = 2;
        }
    }

    uint32_t mod8_id(uint8_t m)
    {
        switch (m)
        {
            case 0:
                return Register::al_id;
            case 1:
                return Register::cl_id;
            case 2:
                return Register::dl_id;
            case 3:
                return Register::bl_id;
            case 4:
                return Register::ah_id;
            case 5:
                return Register::ch_id;
            case 6:
                return Register::dh_id;
            case 7:
                return Register::bh_id;
        }
        return 0;
    }

    uint32_t mod16_id(uint8_t m)
    {
        switch (m)
        {
            case 0:
                return Register::ax_id;
            case 1:
                return Register::cx_id;
            case 2:
                return Register::dx_id;
            case 3:
                return Register::bx_id;
            case 4:
                return Register::sp_id;
            case 5:
                return Register::bp_id;
            case 6:
                return Register::si_id;
            case 7:
                return Register::di_id;
        }
        return 0;
    }

    uint32_t sreg_id(uint8_t m)
    {
        switch (m)
        {
            case 0:
                return Register::es_id;
            case 1:
                return Register::cs_id;
            case 2:
                return Register::ss_id;
            case 3:
                return Register::ds_id;
        }
        return 0;
    }

    template <typename T>
    void _mov_byte_modmr_to_reg()
    {
        Register::increment_ip(1);
        uint8_t modmr = memory_.template read<uint8_t>(Register::ip());
        Register::increment_ip(1);
        const ModRM mod(modmr);
        uint8_t mode    = mod.mod == 0x01 || mod.mod == 0x02 ? 1 : mod.mod;
        uint16_t offset = 0;
        if (mod.mod == 0 && mod.rm == 0x06)
        {
            offset = memory_.template read<uint16_t>(Register::ip());
            Register::increment_ip(2);
        }
        else if (mod.mod == 1)
        {
            offset = memory_.template read<uint8_t>(Register::ip());
            Register::increment_ip(1);
        }
        else if (mod.mod == 2)
        {
            offset = memory_.template read<uint16_t>(Register::ip());
            Register::increment_ip(2);
        }
        else if (mod.mod == 3)
        {
            offset = 0;
        }


        auto to_reg = mod8_id(mod.reg);
        if constexpr (std::is_same_v<uint16_t, T>)
        {
            to_reg = mod16_id(mod.reg);
        }
        if (mode < 3)
        {
            auto from_address = modes.modes[mode][mod.rm](offset);
            if constexpr (std::is_same_v<uint8_t, T>)
            {
                const uint8_t v = memory_.template read<uint8_t>(from_address);
                set_register_by_id(to_reg, v);
            }
            else
            {
                const uint16_t v = memory_.template read<uint16_t>(from_address);
                set_register_by_id(to_reg, v);
            }
            last_instruction_cost_ = static_cast<uint8_t>(12 + modes.costs[mod.mod][mod.rm]);
        }
        else // reg to reg
        {
            if constexpr (std::is_same_v<uint8_t, T>)
            {
                auto from_reg = mod8_id(mod.rm);
                uint8_t v     = get_register_by_id<uint8_t>(from_reg);
                set_register_by_id(to_reg, v);
            }
            else
            {
                auto from_reg = mod16_id(mod.rm);
                uint16_t v    = get_register_by_id<uint16_t>(from_reg);
                set_register_by_id(to_reg, v);
            }
            last_instruction_cost_ = 2;
        }
    }

    template <typename T>
    void _mov_byte_imm_to_modmr()
    {
        Register::increment_ip(1);
        uint8_t modmr = memory_.template read<uint8_t>(Register::ip());
        Register::increment_ip(1);
        const ModRM mod(modmr);
        uint8_t mode    = mod.mod == 0x01 || mod.mod == 0x02 ? 1 : mod.mod;
        uint16_t offset = 0;
        if (mod.mod == 0 && mod.rm == 0x06)
        {
            offset = memory_.template read<uint16_t>(Register::ip());
            Register::increment_ip(2);
        }
        else if (mod.mod == 1)
        {
            offset = memory_.template read<uint8_t>(Register::ip());
            Register::increment_ip(1);
        }
        else if (mod.mod == 2)
        {
            offset = memory_.template read<uint16_t>(Register::ip());
            Register::increment_ip(2);
        }
        else if (mod.mod == 3)
        {
            offset = 0;
        }
        T data = {};
        if constexpr (std::is_same_v<T, uint16_t>)
        {
            data = memory_.template read<uint16_t>(Register::ip());
            Register::increment_ip(2);
        }
        else
        {
            data = memory_.template read<uint8_t>(Register::ip());
            Register::increment_ip(1);
        }

        if (mode < 3)
        {
            auto to_address = modes.modes[mode][mod.rm](offset);
            memory_.write(to_address, data);
            last_instruction_cost_ = 14 + modes.costs[mod.mod][mod.rm];
        }
        else // imm to reg
        {
            if constexpr (std::is_same_v<uint8_t, T>)
            {
                auto to_reg = mod8_id(mod.rm);
                set_register_by_id(to_reg, data);
            }
            else
            {
                auto to_reg = mod16_id(mod.rm);
                set_register_by_id(to_reg, data);
            }
            last_instruction_cost_ = 4;
        }
    }

    void _mov_modrm_to_sreg()
    {
        Register::increment_ip(1);
        uint8_t modmr = memory_.template read<uint8_t>(Register::ip());
        Register::increment_ip(1);
        const ModRM mod(modmr);
        uint8_t mode    = mod.mod == 0x01 || mod.mod == 0x02 ? 1 : mod.mod;
        uint16_t offset = 0;
        if (mod.mod == 0 && mod.rm == 0x06)
        {
            offset = memory_.template read<uint16_t>(Register::ip());
            Register::increment_ip(2);
        }
        else if (mod.mod == 1)
        {
            offset = memory_.template read<uint8_t>(Register::ip());
            Register::increment_ip(1);
        }
        else if (mod.mod == 2)
        {
            offset = memory_.template read<uint16_t>(Register::ip());
            Register::increment_ip(2);
        }
        else if (mod.mod == 3)
        {
            offset = 0;
        }
        auto from_sreg = sreg_id(mod.reg);

        if (mode < 3)
        {
            auto to_address      = modes.modes[mode][mod.rm](offset);
            const uint16_t value = get_register_by_id<uint16_t>(from_sreg);
            memory_.write(to_address, value);
            last_instruction_cost_ = 13 + modes.costs[mod.mod][mod.rm];
        }
        else // reg to reg
        {
            auto to_reg      = mod16_id(mod.rm);
            const auto value = get_register_by_id<uint16_t>(from_sreg);
            set_register_by_id(to_reg, value);
            last_instruction_cost_ = 2;
        }
    }

    void _mov_sreg_to_modrm()
    {
        Register::increment_ip(1);
        const uint8_t modmr = memory_.template read<uint8_t>(Register::ip());
        Register::increment_ip(1);
        const ModRM mod(modmr);
        uint8_t mode    = mod.mod == 0x01 || mod.mod == 0x02 ? 1 : mod.mod;
        uint16_t offset = 0;
        if (mod.mod == 0 && mod.rm == 0x06)
        {
            offset = memory_.template read<uint16_t>(Register::ip());
            Register::increment_ip(2);
        }
        else if (mod.mod == 1)
        {
            offset = memory_.template read<uint8_t>(Register::ip());
            Register::increment_ip(1);
        }
        else if (mod.mod == 2)
        {
            offset = memory_.template read<uint16_t>(Register::ip());
            Register::increment_ip(2);
        }
        else if (mod.mod == 3)
        {
            offset = 0;
        }
        //
        //
        auto to_sreg = sreg_id(mod.reg);
        if (mode < 3)
        {
            auto from_address = modes.modes[mode][mod.rm](offset);
            const auto value  = memory_.template read<uint16_t>(from_address);
            set_register_by_id(to_sreg, value);
            last_instruction_cost_ = 12 + modes.costs[mod.mod][mod.rm];
        }
        else // reg to reg
        {
            auto from_reg        = mod16_id(mod.rm);
            const uint16_t value = get_register_by_id<uint16_t>(from_reg);
            set_register_by_id(to_sreg, value);
            last_instruction_cost_ = 2;
        }
    }
    //

    struct MoveOperand
    {
        uint8_t& from;
        uint8_t& to;
    };

    struct Instruction
    {
        typedef void (Cpu::*fun)(void);
        fun impl;
    };

    Instruction* op_;
    uint8_t last_instruction_cost_;
    char error_msg_[100];
    static inline Instruction opcodes_[255];
    MemoryType& memory_;
};

} // namespace cpu8086
} // namespace msemu
