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
class CpuForTest : public Cpu
{
public:
    using Cpu::Cpu;
    const Registers& get_registers() const;
    Registers& get_registers();
};

inline bool operator==(const Registers& lhs, const Registers& rhs)
{
    return lhs.ax == rhs.ax && lhs.bx == rhs.bx && lhs.cx == rhs.cx && lhs.dx == rhs.dx &&
           lhs.si == rhs.si && lhs.di == rhs.di && lhs.bp == rhs.bp && lhs.sp == rhs.sp &&
           lhs.ip == rhs.ip && lhs.cs == rhs.cs && lhs.ds == rhs.ds && lhs.es == rhs.es &&
           lhs.ss == rhs.ss && lhs.flags.o == rhs.flags.o && lhs.flags.d == rhs.flags.d &&
           lhs.flags.i == rhs.flags.i && lhs.flags.t == rhs.flags.t &&
           lhs.flags.s == rhs.flags.s && lhs.flags.z == rhs.flags.z &&
           lhs.flags.a == rhs.flags.a && lhs.flags.p == rhs.flags.p &&
           lhs.flags.c == rhs.flags.c;
}

inline void PrintTo(const Registers& reg, ::std::ostream* os)
{
    *os << std::endl;
    char line[255];
    snprintf(line, sizeof(line), "ax: %04x bx: %04x cx: %04x dx: %04x\n", reg.ax, reg.bx,
             reg.cx, reg.dx);
    *os << "    " << line;
    snprintf(line, sizeof(line), "si: %04x di: %04x bp: %04x sp: %04x ip: %04x\n", reg.si,
             reg.di, reg.bp, reg.sp, reg.ip);
    *os << "    " << line;
    snprintf(line, sizeof(line), "cs: %04x ds: %04x es: %04x ss: %04x\n", reg.cs, reg.ds,
             reg.es, reg.ss);
    *os << "    " << line;

    snprintf(line, sizeof(line),
             "Flags o: %1d d: %1d i: %1d t: %1d s: %1d z: %1d a: %1d p: %1d c: %1d\n",
             reg.flags.o, reg.flags.d, reg.flags.i, reg.flags.t, reg.flags.s, reg.flags.z,
             reg.flags.a, reg.flags.p, reg.flags.c);
    *os << "    " << line;
}

} // namespace cpu8086
} // namespace msemu
