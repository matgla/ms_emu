/*
 Copyright (c) 2021 Mateusz Stadnik

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "test_base.hpp"

#include <iomanip>
#include <sstream>

namespace msemu::cpu8086
{

std::string get_name(uint8_t command)
{
    std::stringstream str;
    str << std::hex << std::setfill('0') << std::setw(2) << "0x" << static_cast<uint32_t>(command);
    return str.str();
}


} // namespace msemu::cpu8086
