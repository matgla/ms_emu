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

#pragma once

#include <sstream>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "bus.hpp"
#include "cpu8086_for_test.hpp"
#include "device.hpp"
#include "memory.hpp"

namespace msemu::cpu8086
{

template <typename TestData>
class TestBase : public ::testing::TestWithParam<TestData>
{
    using MemoryType  = Device<Memory<1024 * 128>, 0x00000000>;
    using BiosRomType = Device<Memory<1024 * 64>, 0x000ffff0>;

public:
    TestBase()
        : bus_(MemoryType("flash"), BiosRomType("bios/rom"))
        , sut_(bus_)
    {
    }

protected:
    using BusType = Bus<MemoryType, BiosRomType>;
    BusType bus_;
    cpu8086::CpuForTest<BusType> sut_;
};

template <typename T>
inline std::string generate_test_case_name(const ::testing::TestParamInfo<T>& info)
{
    return info.param.name;
}

std::string get_name(uint8_t command);

} // namespace msemu::cpu8086
