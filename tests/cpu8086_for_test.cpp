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

#include "cpu8086_for_test.hpp"

namespace msemu
{
namespace cpu8086
{
Registers& CpuForTest::get_registers()
{
    return regs_;
}

const Registers& CpuForTest::get_registers() const
{
    return regs_;
}

bool CpuForTest::has_error() const
{
    return strlen(error_msg_) > 0;
}

std::string CpuForTest::get_error() const
{
    return error_msg_;
}

} // namespace cpu8086
} // namespace msemu