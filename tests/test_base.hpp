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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cpu8086_for_test.hpp"
#include "memory.hpp"

namespace msemu::cpu8086
{

template <typename TestData>
class TestBase : public ::testing::TestWithParam<TestData>
{
public:
    TestBase()
        : memory_()
        , sut_(memory_)
    {
    }

protected:
    using MemoryType = Memory<1024 * 128>;
    MemoryType memory_;
    cpu8086::CpuForTest<MemoryType> sut_;
};

// inline std::string generate_test_case_name(const auto& info)
// {
//     return info.param.name;
// }

} // namespace msemu::cpu8086
