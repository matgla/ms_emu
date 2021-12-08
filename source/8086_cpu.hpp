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

#include <iostream>

namespace msemu
{
namespace cpu8086
{

template <typename BusType>
class Cpu
{
public:
    Cpu(BusType& bus)
        : last_instruction_cost_{0}
        , error_msg_{}
        , bus_{bus}
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

        // modifiers
        set_opcode(0x26, &Cpu::_set_section_offset<Register::es_id>);
        set_opcode(0x36, &Cpu::_set_section_offset<Register::ss_id>);
        set_opcode(0x2e, &Cpu::_set_section_offset<Register::cs_id>);
        set_opcode(0x3e, &Cpu::_set_section_offset<Register::ds_id>);

        // add group
        // set_opcode(0x14, &Cpu::_adc_imm_to_acc<uint8_t>);

        set_opcode(0x31, &Cpu::_xor_modrm_from_reg);

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
        set_opcode(0xeb, &Cpu::_jump_short<int8_t>);
        set_opcode(0xe9, &Cpu::_jump_short<int16_t>);
        set_opcode(0xea, &Cpu::_jump_far);

        set_grp5_opcode(0x04, &Cpu::_jump_short_modrm);
        set_grp5_opcode(0x05, &Cpu::_jump_far_modrm);

        // push
        set_opcode(0x50, &Cpu::_push_register_16<Register::ax_id>);
        set_opcode(0x51, &Cpu::_push_register_16<Register::cx_id>);
        set_opcode(0x52, &Cpu::_push_register_16<Register::dx_id>);
        set_opcode(0x53, &Cpu::_push_register_16<Register::bx_id>);
        set_opcode(0x54, &Cpu::_push_register_16<Register::sp_id>);
        set_opcode(0x55, &Cpu::_push_register_16<Register::bp_id>);
        set_opcode(0x56, &Cpu::_push_register_16<Register::si_id>);
        set_opcode(0x57, &Cpu::_push_register_16<Register::di_id>);

        set_opcode(0x06, &Cpu::_push_segmentation_register<Register::es_id>);
        set_opcode(0x0e, &Cpu::_push_segmentation_register<Register::cs_id>);
        set_opcode(0x16, &Cpu::_push_segmentation_register<Register::ss_id>);
        set_opcode(0x1e, &Cpu::_push_segmentation_register<Register::ds_id>);

        set_grp5_opcode(0x06, &Cpu::_push_modrm);
        // pop
        set_opcode(0x58, &Cpu::_pop_register_16<Register::ax_id>);
        set_opcode(0x59, &Cpu::_pop_register_16<Register::cx_id>);
        set_opcode(0x5a, &Cpu::_pop_register_16<Register::dx_id>);
        set_opcode(0x5b, &Cpu::_pop_register_16<Register::bx_id>);
        set_opcode(0x5c, &Cpu::_pop_register_16<Register::sp_id>);
        set_opcode(0x5d, &Cpu::_pop_register_16<Register::bp_id>);
        set_opcode(0x5e, &Cpu::_pop_register_16<Register::si_id>);
        set_opcode(0x5f, &Cpu::_pop_register_16<Register::di_id>);
        set_opcode(0x8f, &Cpu::_pop_modrm);

        set_opcode(0x07, &Cpu::_pop_segmentation_register<Register::es_id>);
        set_opcode(0x17, &Cpu::_pop_segmentation_register<Register::ss_id>);
        set_opcode(0x1f, &Cpu::_pop_segmentation_register<Register::ds_id>);

        set_opcode(0xfc, &Cpu::_cld);

        set_opcode(0xff, &Cpu::_grp5_process);
        set_opcode(0xc3, &Cpu::_unimpl);

        reset();
#ifdef DUMP_CORE_STATE
        dump(error_msg_, bus_);
#endif
    }

    void jump_to_bios()
    {
        Register::cs(0xf000);
        Register::ip(0x0100);

#ifdef DUMP_CORE_STATE
        dump(error_msg_, bus_);
#endif
    }

    inline uint32_t calculate_code_address() const
    {
        return static_cast<uint32_t>(Register::cs() << 4 | Register::ip());
    }

    inline uint32_t calculate_data_address(const uint32_t address) const
    {
        return static_cast<uint32_t>(Register::ds() << 4 | address);
    }

    inline uint32_t calculate_stack_address(const uint32_t address) const
    {
        return static_cast<uint32_t>(Register::ss() << 4 | address);
    }

    void step()
    {
        const auto* op = &opcodes_[bus_.template read<uint8_t>(calculate_code_address())];
        (this->*op->impl)();
#ifdef DUMP_CORE_STATE
        dump(error_msg_, bus_);
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
                 bus_.template read<uint8_t>(calculate_code_address()));
        last_instruction_cost_ = 0;
    }

    void _unimpl_extra(const ModRM mod)
    {
        Register::decrement_ip(2);
        snprintf(error_msg_, sizeof(error_msg_), "Opcode: 0x%x is unimplemented!, modrm: 0x%02x\n",
                 bus_.template read<uint8_t>(calculate_code_address()), static_cast<uint8_t>(mod));
        last_instruction_cost_ = 0;
    }


    void _grp5_process()
    {
        Register::increment_ip(1);
        const ModRM mod = bus_.template read<uint8_t>(calculate_code_address());
        Register::increment_ip(1);
        const auto* op = &grp5_opcodes_[mod.reg];
        (this->*op->impl)(mod);
    }

    template <typename T>
    void _jump_short()
    {
        Register::increment_ip(1);
        const T offset = bus_.template read<T>(calculate_code_address());
        Register::increment_ip(sizeof(T));
        const uint16_t address = static_cast<uint16_t>(static_cast<int>(Register::ip()) + offset);
        Register::ip(address);
        last_instruction_cost_ = 15;
    }

    void _jump_far()
    {
        Register::increment_ip(1);
        const uint16_t ip_address = bus_.template read<uint16_t>(calculate_code_address());
        Register::increment_ip(2);
        const uint16_t cs_address = bus_.template read<uint16_t>(calculate_code_address());
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
        const auto from_address = calculate_memory_address(mod, disp);
        last_instruction_cost_  = static_cast<uint8_t>(12 + modes.costs[mod.mod][mod.rm]);
        const uint16_t ip       = bus_.template read<uint16_t>(from_address);
        const uint16_t cs       = bus_.template read<uint16_t>(from_address + 2);
        Register::ip(ip);
        Register::cs(cs);
        last_instruction_cost_ = static_cast<uint8_t>(24 + modes.costs[mod.mod][mod.rm]);
    }

    inline uint32_t calculate_memory_address(const ModRM mod, const uint16_t offset)
    {
        return modes.modes[mod.mod][mod.rm](offset, section_offset_);
    }

    template <typename T>
    inline T read_reg_mem(const ModRM mod, const uint16_t offset)
    {
        if (mod.mod < 3)
        {
            const auto from_address = calculate_memory_address(mod, offset);

            last_instruction_cost_ = 12 + modes.costs[mod.mod][mod.rm];
            return bus_.template read<uint16_t>(from_address);
        }

        last_instruction_cost_ = 2;
        return get_register_by_id<T>(mod.rm);
    }

    template <typename T, uint8_t mem_cost = 13, uint8_t reg_cost = 2>
    inline void write_modmr(const ModRM mod, const uint16_t offset, const T value)
    {
        if (mod.mod < 3)
        {
            const auto to_address = calculate_memory_address(mod, offset);
            bus_.write(to_address, value);
            last_instruction_cost_ = mem_cost + modes.costs[mod.mod][mod.rm];
            return;
        }

        set_register_by_id<T>(mod.rm, value);
        last_instruction_cost_ = reg_cost;
    }

    template <typename T>
    inline void write_modmr_imm(const ModRM mod, const uint16_t offset, const T value)
    {
        if (mod.mod < 3)
        {
            const auto to_address = calculate_memory_address(mod, offset);
            bus_.write(to_address, value);
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
            const auto from_address = calculate_memory_address(mod, offset);
            last_instruction_cost_  = static_cast<uint8_t>(mem_cost + modes.costs[mod.mod][mod.rm]);
            return bus_.template read<T>(from_address);
        }

        last_instruction_cost_ = reg_cost;
        return get_register_by_id<T>(mod.rm);
    }

    template <uint32_t reg, typename T>
    void _mov_imm_to_reg()
    {
        Register::increment_ip(1);
        const T data = bus_.template read<T>(calculate_code_address());
        Register::increment_ip(sizeof(T));
        set_register_by_id<T, reg>(data);
        last_instruction_cost_ = 4;
    }

    template <uint32_t reg, typename T>
    void _mov_mem_to_reg()
    {
        Register::increment_ip(1);
        const uint16_t address = bus_.template read<uint16_t>(calculate_code_address());
        Register::increment_ip(2);

        const T value = bus_.template read<T>(calculate_data_address(address));

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
        const uint16_t address = bus_.template read<uint16_t>(calculate_code_address());
        Register::increment_ip(2);
        const T value = get_register_by_id<T, reg>();
        bus_.write(calculate_data_address(address), value);

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
        const ModRM mod = bus_.template read<uint8_t>(calculate_code_address());
        Register::increment_ip(1);
        return std::pair<uint16_t, ModRM>(process_modrm(mod), mod);
    }

    inline uint16_t process_modrm(const ModRM mod) const
    {
        uint16_t offset = 0;
        if ((mod.mod == 0 && mod.rm == 0x06) || mod.mod == 2)
        {
            offset = bus_.template read<uint16_t>(calculate_code_address());
            Register::increment_ip(2);
        }
        else if (mod.mod == 1)
        {
            offset = bus_.template read<uint8_t>(calculate_code_address());
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

        const T value = bus_.template read<T>(calculate_code_address());
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
        const uint16_t value     = read_reg_mem<uint16_t>(mod, offset);
        set_segment_register_by_id(mod.reg, value);
    }

    template <uint32_t reg>
    void _push_register_16()
    {
        Register::increment_ip(1);
        const uint16_t value = get_register_16_by_id<reg>();
        Register::decrement_sp(2);
        const uint16_t sp = Register::sp();
        bus_.write(calculate_stack_address(sp), value);
        last_instruction_cost_ = 15;
    }

    template <uint32_t reg>
    void _pop_register_16()
    {
        Register::increment_ip(1);
        const uint16_t sp    = Register::sp();
        const uint16_t value = bus_.template read<uint16_t>(calculate_stack_address(sp));
        set_register_16_by_id<reg>(value);
        Register::increment_sp(2);
        last_instruction_cost_ = 12;
    }


    template <uint32_t reg>
    void _push_segmentation_register()
    {
        Register::increment_ip(1);
        const uint16_t value = get_segment_register_by_id<reg>();
        Register::decrement_sp(2);
        const uint16_t sp      = Register::sp();
        last_instruction_cost_ = 14;

        bus_.write(calculate_stack_address(sp), value);
    }

    void _push_modrm(const ModRM mod)
    {
        const auto disp      = process_modrm(mod);
        const uint16_t value = read_modmr<uint16_t, 24, 15>(mod, disp);
        Register::decrement_sp(2);
        const uint16_t sp = Register::sp();
        bus_.write(calculate_stack_address(sp), value);
    }

    void _pop_modrm()
    {
        Register::increment_ip(1);
        const auto [disp, mod] = process_modrm();
        const uint16_t sp      = Register::sp();
        const uint16_t value   = bus_.template read<uint16_t>(calculate_stack_address(sp));
        write_modmr<uint16_t, 25, 12>(mod, disp, value);
        Register::increment_sp(2);
    }


    template <uint32_t reg>
    void _pop_segmentation_register()
    {
        Register::increment_ip(1);
        const uint16_t sp    = Register::sp();
        const uint16_t value = bus_.template read<uint16_t>(calculate_stack_address(sp));
        set_segment_register_by_id<reg>(value);
        Register::increment_sp(2);

        last_instruction_cost_ = 12;
    }

    void _cld()
    {
        Register::increment_ip(1);
        Register::flags().d(false);
    }

    void _xor_modrm_from_reg()
    {
        Register::increment_ip(1);
        const auto [offset, mod] = process_modrm();
        const uint16_t v1        = get_register_16_by_id(mod.reg);
        const uint16_t v2        = read_modmr<uint8_t>(mod, offset);
        set_register_16_by_id(mod.reg, v1 ^ v2);
    }

    template <uint32_t reg_id>
    void _set_section_offset()
    {
        Register::increment_ip(1);
        section_offset_ = reg_id;
        const auto* op  = &opcodes_[bus_.template read<uint8_t>(calculate_code_address())];
        (this->*op->impl)();
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
    std::optional<uint8_t> section_offset_;
    char error_msg_[100];
    static inline Instruction opcodes_[256];
    static inline ExtraInstruction grp1_opcodes_[8];
    static inline ExtraInstruction grp2_opcodes_[8];
    static inline ExtraInstruction grp3a_opcodes_[8];
    static inline ExtraInstruction grp3b_opcodes_[8];
    static inline ExtraInstruction grp4_opcodes_[8];
    static inline ExtraInstruction grp5_opcodes_[8];
    BusType& bus_;
};

} // namespace cpu8086
} // namespace msemu
