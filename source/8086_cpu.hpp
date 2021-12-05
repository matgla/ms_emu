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
#include "core_dump.hpp"
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
        : last_instruction_cost_{0}
        , error_msg_{}
        , memory_{memory}
    {
        for (uint16_t i = 0; i < 256; ++i)
        {
            set_opcode(static_cast<uint8_t>(i), &Cpu::_unimpl);
        }

        for (uint16_t i = 0; i < 8; ++i)
        {
            set_grp1_opcode(static_cast<uint8_t>(i), &Cpu::_unimpl_extra);
            set_grp2_opcode(static_cast<uint8_t>(i), &Cpu::_unimpl_extra);
            set_grp3a_opcode(static_cast<uint8_t>(i), &Cpu::_unimpl_extra);
            set_grp3b_opcode(static_cast<uint8_t>(i), &Cpu::_unimpl_extra);
            set_grp4_opcode(static_cast<uint8_t>(i), &Cpu::_unimpl_extra);
            set_grp5_opcode(static_cast<uint8_t>(i), &Cpu::_unimpl_extra);
        }


        // add group
        // set_opcode(0x14, &Cpu::_adc_imm_to_acc<uint8_t>);

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

        // jumps - unconditional
        set_opcode(0xeb, &Cpu::_jump_short<uint8_t>);
        set_opcode(0xe9, &Cpu::_jump_short<uint16_t>);
        set_opcode(0xea, &Cpu::_jump_far);

        set_grp5_opcode(0x04, &Cpu::_jump_short_modrm);
        set_grp5_opcode(0x05, &Cpu::_jump_far_modrm);


        set_opcode(0xff, &Cpu::_grp5_process);
        set_opcode(0xc3, &Cpu::_unimpl);

        reset();
#ifdef DUMP_CORE_STATE
        dump(error_msg_, memory_);
#endif
    }

    void step()
    {
        const auto* op = &opcodes_[memory_.template read<uint8_t>(Register::ip())];
        (this->*op->impl)();
#ifdef DUMP_CORE_STATE
        dump(error_msg_, memory_);
#endif
    }

    void reset()
    {
        Register::reset();
    }

protected:
    // configuration
    void set_opcode(uint8_t id, void (Cpu::*fun)(void))
    {
        opcodes_[id].impl = fun;
    }

    void set_grp1_opcode(const uint8_t id, void (Cpu::*fun)(const ModRM))
    {
        grp1_opcodes_[id].impl = fun;
    }

    void set_grp2_opcode(const uint8_t id, void (Cpu::*fun)(const ModRM))
    {
        grp2_opcodes_[id].impl = fun;
    }

    void set_grp3a_opcode(const uint8_t id, void (Cpu::*fun)(const ModRM))
    {
        grp3a_opcodes_[id].impl = fun;
    }

    void set_grp3b_opcode(const uint8_t id, void (Cpu::*fun)(const ModRM))
    {
        grp3b_opcodes_[id].impl = fun;
    }

    void set_grp4_opcode(const uint8_t id, void (Cpu::*fun)(const ModRM))
    {
        grp4_opcodes_[id].impl = fun;
    }

    void set_grp5_opcode(const uint8_t id, void (Cpu::*fun)(const ModRM))
    {
        grp5_opcodes_[id].impl = fun;
    }


    // core emulation

    void _unimpl()
    {
        snprintf(error_msg_, sizeof(error_msg_), "Opcode: 0x%x is unimplemented!\n",
                 memory_.template read<uint8_t>(Register::ip()));
        last_instruction_cost_ = 0;
    }

    void _unimpl_extra(const ModRM mod)
    {
        Register::decrement_ip(2);
        snprintf(error_msg_, sizeof(error_msg_), "Opcode: 0x%x is unimplemented!, modrm: 0x%02x\n",
                 memory_.template read<uint8_t>(Register::ip()), static_cast<uint8_t>(mod));
        last_instruction_cost_ = 0;
    }


    void _grp5_process()
    {
        Register::increment_ip(1);
        const ModRM mod = memory_.template read<uint8_t>(Register::ip());
        Register::increment_ip(1);
        const auto* op = &grp5_opcodes_[mod.reg];
        (this->*op->impl)(mod);
    }

    template <typename T>
    void _jump_short()
    {
        Register::increment_ip(1);
        T address = memory_.template read<T>(Register::ip());
        Register::increment_ip(sizeof(T));
        address = static_cast<T>(address + Register::ip());
        Register::ip(address);
        last_instruction_cost_ = 15;
    }

    void _jump_far()
    {
        Register::increment_ip(1);
        const uint16_t ip_address = memory_.template read<uint16_t>(Register::ip());
        Register::increment_ip(2);
        const uint16_t cs_address = memory_.template read<uint16_t>(Register::ip());
        Register::increment_ip(2);

        Register::ip(ip_address);
        Register::cs(cs_address);
        last_instruction_cost_ = 15;
    }

    void _jump_short_modrm(const ModRM mod)
    {
        const uint16_t disp   = process_modrm(mod);
        const uint16_t offset = read_modmr<uint16_t, 18, 11>(mod, disp);
        Register::ip(offset);
    }

    void _jump_far_modrm(const ModRM mod)
    {
        const uint16_t disp     = process_modrm(mod);
        const auto from_address = modes.modes[mod.mod][mod.rm](disp);
        last_instruction_cost_  = static_cast<uint8_t>(12 + modes.costs[mod.mod][mod.rm]);
        const uint16_t ip       = memory_.template read<uint16_t>(from_address);
        const uint16_t cs       = memory_.template read<uint16_t>(from_address + 2);
        Register::ip(ip);
        Register::cs(cs);
        last_instruction_cost_ = static_cast<uint8_t>(24 + modes.costs[mod.mod][mod.rm]);
    }


    template <typename T>
    inline T read_reg_mem(const ModRM mod, const uint16_t offset)
    {
        if (mod.mod < 3)
        {
            const auto from_address = modes.modes[mod.mod][mod.rm](offset);
            last_instruction_cost_  = 12 + modes.costs[mod.mod][mod.rm];
            return memory_.template read<uint16_t>(from_address);
        }

        last_instruction_cost_ = 2;
        return get_register_by_id<T>(mod.rm);
    }

    template <typename T>
    inline void write_modmr(const ModRM mod, const uint16_t offset, const T value)
    {
        if (mod.mod < 3)
        {
            const auto to_address = modes.modes[mod.mod][mod.rm](offset);
            memory_.write(to_address, value);
            last_instruction_cost_ = 13 + modes.costs[mod.mod][mod.rm];
            return;
        }

        set_register_by_id<T>(mod.rm, value);
        last_instruction_cost_ = 2;
    }

    template <typename T>
    inline void write_modmr_imm(const ModRM mod, const uint16_t offset, const T value)
    {
        if (mod.mod < 3)
        {
            const auto to_address = modes.modes[mod.mod][mod.rm](offset);
            memory_.write(to_address, value);
            last_instruction_cost_ = 14 + modes.costs[mod.mod][mod.rm];
            return;
        }
        set_register_by_id<T>(mod.rm, value);
        last_instruction_cost_ = 4;
    }

    template <typename T, uint8_t mem_cost = 12, uint8_t reg_cost = 2>
    inline T read_modmr(const ModRM mod, const uint16_t offset)
    {
        if (mod.mod < 3)
        {
            const auto from_address = modes.modes[mod.mod][mod.rm](offset);
            last_instruction_cost_  = static_cast<uint8_t>(mem_cost + modes.costs[mod.mod][mod.rm]);
            return memory_.template read<T>(from_address);
        }

        last_instruction_cost_ = reg_cost;
        return get_register_by_id<T>(mod.rm);
    }

    template <uint32_t reg, typename T>
    void _mov_imm_to_reg()
    {
        Register::increment_ip(1);
        const T data = memory_.template read<T>(Register::ip());
        Register::increment_ip(sizeof(T));
        set_register_by_id<T, reg>(data);
        last_instruction_cost_ = 4;
    }

    template <uint32_t reg, typename T>
    void _mov_mem_to_reg()
    {
        Register::increment_ip(1);
        const uint16_t address = memory_.template read<uint16_t>(Register::ip());
        Register::increment_ip(2);

        const T value = memory_.template read<T>(address);

        set_register_by_id<T, reg>(value);
        if constexpr (reg == Register::ax_id || reg == Register::al_id || reg == Register::ah_id)
        {
            last_instruction_cost_ = 14;
        }
        else
        {
            last_instruction_cost_ = 12 + get_cost(AccessCost::Direct);
        }
    }

    template <uint32_t reg, typename T>
    void _mov_reg_to_mem()
    {
        Register::increment_ip(1);
        const uint16_t address = memory_.template read<uint16_t>(Register::ip());
        Register::increment_ip(2);
        const T value = get_register_by_id<T, reg>();
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

    inline std::pair<uint16_t, ModRM> process_modrm() const
    {
        const ModRM mod = memory_.template read<uint8_t>(Register::ip());
        Register::increment_ip(1);
        return std::pair<uint16_t, ModRM>(process_modrm(mod), mod);
    }

    inline uint16_t process_modrm(const ModRM mod) const
    {
        uint16_t offset = 0;
        if ((mod.mod == 0 && mod.rm == 0x06) || mod.mod == 2)
        {
            offset = memory_.template read<uint16_t>(Register::ip());
            Register::increment_ip(2);
        }
        else if (mod.mod == 1)
        {
            offset = memory_.template read<uint8_t>(Register::ip());
            Register::increment_ip(1);
        }
        return offset;
    }

    template <typename T>
    void _mov_byte_reg_to_modmr()
    {
        Register::increment_ip(1);

        const auto [offset, mod] = process_modrm();

        const T value = get_register_by_id<T>(mod.reg);
        write_modmr<T>(mod, offset, value);
    }


    template <typename T>
    void _mov_byte_modmr_to_reg()
    {
        Register::increment_ip(1);
        const auto [offset, mod] = process_modrm();
        const T value            = read_modmr<T>(mod, offset);
        set_register_by_id<T>(mod.reg, value);
    }

    template <typename T>
    void _mov_byte_imm_to_modmr()
    {
        Register::increment_ip(1);
        const auto [offset, mod] = process_modrm();

        const T value = memory_.template read<T>(Register::ip());
        Register::increment_ip(sizeof(T));

        write_modmr_imm<T>(mod, offset, value);
    }

    void _mov_sreg_to_modrm()
    {
        Register::increment_ip(1);
        const auto [offset, mod] = process_modrm();
        const uint16_t value     = get_segment_register_by_id(mod.reg);
        write_modmr(mod, offset, value);
    }

    void _mov_modrm_to_sreg()
    {
        Register::increment_ip(1);
        const auto [offset, mod] = process_modrm();

        const uint16_t value = read_reg_mem<uint16_t>(mod, offset);
        set_segment_register_by_id(mod.reg, value);
    }

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

    struct ExtraInstruction
    {
        typedef void (Cpu::*fun)(const ModRM);
        fun impl;
    };

    Instruction* op_;
    uint8_t last_instruction_cost_;
    char error_msg_[100];
    static inline Instruction opcodes_[256];
    static inline ExtraInstruction grp1_opcodes_[8];
    static inline ExtraInstruction grp2_opcodes_[8];
    static inline ExtraInstruction grp3a_opcodes_[8];
    static inline ExtraInstruction grp3b_opcodes_[8];
    static inline ExtraInstruction grp4_opcodes_[8];
    static inline ExtraInstruction grp5_opcodes_[8];
    MemoryType& memory_;
};

} // namespace cpu8086
} // namespace msemu
