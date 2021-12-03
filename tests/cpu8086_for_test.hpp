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

#include <iomanip>
#include <ostream>

#include "8086_cpu.hpp"

namespace msemu
{
namespace cpu8086
{

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
struct Registers
{
    union
    {
        struct
        {
            uint8_t al;
            uint8_t ah;
        };
        uint16_t ax;
    };
    union
    {
        struct
        {
            uint8_t bl;
            uint8_t bh;
        };
        uint16_t bx;
    };
    union
    {
        struct
        {
            uint8_t cl;
            uint8_t ch;
        };
        uint16_t cx;
    };
    union
    {
        struct
        {
            uint8_t dl;
            uint8_t dh;
        };
        uint16_t dx;
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
#pragma GCC diagnostic pop

template <typename MemoryType>
class CpuForTest : public Cpu<MemoryType>
{
public:
    using Cpu<MemoryType>::Cpu;

    void struct_to_reg(const Registers& r)
    {
        Register::ax(r.ax);
        Register::bx(r.bx);
        Register::cx(r.cx);
        Register::dx(r.dx);
        Register::si(r.si);
        Register::di(r.di);
        Register::bp(r.bp);
        Register::sp(r.sp);
        Register::ip(r.ip);
        Register::cs(r.cs);
        Register::ds(r.ds);
        Register::es(r.es);
        Register::ss(r.ss);
    }

    Registers reg_to_struct() const
    {
        return {.ax = Register::ax(),
                .bx = Register::bx(),
                .cx = Register::cx(),
                .dx = Register::dx(),
                .si = Register::si(),
                .di = Register::di(),
                .bp = Register::bp(),
                .sp = Register::sp(),
                .ip = Register::ip(),
                .cs = Register::cs(),
                .ds = Register::ds(),
                .es = Register::es(),
                .ss = Register::ss()};
    }


    const Registers get_registers() const
    {
        return reg_to_struct();
    }

    void set_registers(const Registers& r)
    {
        struct_to_reg(r);
    }

    bool has_error() const
    {
        return strlen(this->error_msg_) > 0;
    }

    std::string get_error() const
    {
        return this->error_msg_;
    }

    uint8_t last_instruction_cost() const
    {
        return this->last_instruction_cost_;
    }
};

inline bool operator==(const Registers& lhs, const Registers& rhs)
{
    return lhs.ax == rhs.ax && lhs.bx == rhs.bx && lhs.cx == rhs.cx && lhs.dx == rhs.dx && lhs.si == rhs.si &&
           lhs.di == rhs.di && lhs.bp == rhs.bp && lhs.sp == rhs.sp && lhs.ip == rhs.ip && lhs.cs == rhs.cs &&
           lhs.ds == rhs.ds && lhs.es == rhs.es && lhs.ss == rhs.ss && lhs.flags.o == rhs.flags.o &&
           lhs.flags.d == rhs.flags.d && lhs.flags.i == rhs.flags.i && lhs.flags.t == rhs.flags.t &&
           lhs.flags.s == rhs.flags.s && lhs.flags.z == rhs.flags.z && lhs.flags.a == rhs.flags.a &&
           lhs.flags.p == rhs.flags.p && lhs.flags.c == rhs.flags.c;
}

inline void PrintTo(const Registers& reg, ::std::ostream* os)
{
    *os << std::endl;
    char line[255];
    snprintf(line, sizeof(line), "ax: %04x bx: %04x cx: %04x dx: %04x\n", reg.ax, reg.bx, reg.cx, reg.dx);
    *os << "    " << line;
    snprintf(line, sizeof(line), "si: %04x di: %04x bp: %04x sp: %04x ip: %04x\n", reg.si, reg.di, reg.bp,
             reg.sp, reg.ip);
    *os << "    " << line;
    snprintf(line, sizeof(line), "cs: %04x ds: %04x es: %04x ss: %04x\n", reg.cs, reg.ds, reg.es, reg.ss);
    *os << "    " << line;

    snprintf(line, sizeof(line), "Flags o: %1d d: %1d i: %1d t: %1d s: %1d z: %1d a: %1d p: %1d c: %1d\n",
             reg.flags.o, reg.flags.d, reg.flags.i, reg.flags.t, reg.flags.s, reg.flags.z, reg.flags.a,
             reg.flags.p, reg.flags.c);
    *os << "    " << line;
}

} // namespace cpu8086
} // namespace msemu
