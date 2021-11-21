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

#include "memory.hpp"

namespace msemu
{

class Cpu8086
{
    struct Registers
    {
        union
        {
            union
            {
                struct
                {
                    uint8_t al;
                    uint8_t ah;
                } a_base;
                uint16_t ax; // primary accumulator
            };
            union
            {
                struct
                {
                    uint8_t bl;
                    uint8_t bh;
                } b_base;
                uint16_t bx; // base accumulator
            };
            union
            {
                struct
                {
                    uint8_t cl;
                    uint8_t ch;
                } c_base;
                uint16_t cx; // counter accumulator
            };
            union
            {
                struct
                {
                    uint8_t dl;
                    uint8_t dh;
                } d_base;
                uint16_t dx; // extended accumulator
            };
            uint16_t regs_16[4];
            uint8_t regs_8[8];
        };
        uint16_t si; // source index
        uint16_t di; // destination index
        uint16_t bp; // base pointer
        uint16_t sp; // stack pointer
        uint16_t ip; // instruction pointer
        uint16_t cs; // code segment
        uint16_t ds; // data segment
        uint16_t es; // extra segment
        uint16_t ss; // stack segment
        struct
        {
            uint16_t res_0 : 1;
            uint16_t res_1 : 1;
            uint16_t res_2 : 1;
            uint16_t res_3 : 1;
            uint16_t o : 1; // overflow
            uint16_t d : 1; // direction
            uint16_t i : 1; // interrupt
            uint16_t t : 1; // trap
            uint16_t s : 1; // sign
            uint16_t z : 1; // zero
            uint16_t res_4 : 1;
            uint16_t a : 1; // adjust
            uint16_t res_5 : 1;
            uint16_t p : 1; // parity
            uint16_t res_6 : 1;
            uint16_t c : 1; // carry
        } flags;
    };

public:
    Cpu8086(MemoryBase& memory);

    void step();
    void reset();

private:
    void dump() const;

    uint8_t read_byte() const;
    void set_opcode(uint8_t id, void (Cpu8086::*fun)(void), const char* name, uint8_t size);


    void _add();
    void _unimpl();

    template <typename T, T* (Cpu8086::*setter)()>
    void _mov();

    constexpr static uint8_t al_offset = 0;
    constexpr static uint8_t ah_offset = 1;
    constexpr static uint8_t bl_offset = 2;
    constexpr static uint8_t bh_offset = 3;
    constexpr static uint8_t cl_offset = 4;
    constexpr static uint8_t ch_offset = 5;
    constexpr static uint8_t dl_offset = 6;
    constexpr static uint8_t dh_offset = 7;

    template <uint8_t reg_offset>
    void _mov8_to_reg();

    template <uint8_t from, uint8_t to>
    void _mov_from_reg();

    struct MoveOperand
    {
        uint8_t& from;
        uint8_t& to;
    };

    struct Instruction
    {
        typedef void (Cpu8086::*fun)(void);
        uint8_t size;
        fun impl;
        const char* name;
    };

    Instruction* op_;
    Registers regs_;
    static Instruction opcodes_[255];
    MemoryBase& memory_;
};


} // namespace msemu
