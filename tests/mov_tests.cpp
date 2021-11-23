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

#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "cpu8086_for_test.hpp"

namespace msemu
{

struct MemoryWrites
{
    std::size_t address;
    std::vector<uint8_t> data;
};

struct MovFromMemToRegParams
{
    std::string name;
    std::vector<std::vector<uint8_t>> commands;
    uint16_t Cpu8086::Registers::*reg;
    std::vector<MemoryWrites> memory_ops;
    std::vector<uint16_t> expects;
};

class MovFromMemToRegTests : public ::testing::TestWithParam<MovFromMemToRegParams>
{
public:
    MovFromMemToRegTests()
        : memory_()
        , sut_(memory_)
    {
    }

protected:
    msemu::Memory<1024 * 128> memory_;
    msemu::Cpu8086ForTest sut_;
};

TEST_P(MovFromMemToRegTests, MovFromMemoryToRegisterCmd)
{
    auto data        = GetParam();
    uint16_t address = 0;

    for (const auto& op : data.memory_ops)
    {
        memory_.write(op.address, op.data);
    }

    for (std::size_t i = 0; i < data.commands.size(); ++i)
    {
        auto& opcode = data.commands[i];
        memory_.write(address, opcode);

        Cpu8086::Registers expected{};
        (expected.*data.reg) = data.expects[i];

        address += static_cast<uint16_t>(opcode.size());

        expected.ip = address;

        sut_.step();
        EXPECT_EQ(expected, sut_.get_registers());
    }
}

inline std::string generate_test_case_name(
    const ::testing::TestParamInfo<msemu::MovFromMemToRegParams>& info)
{
    return info.param.name;
}

INSTANTIATE_TEST_CASE_P(MovTests_1, MovFromMemToRegTests,
                        ::testing::Values(
                            MovFromMemToRegParams{
                                .name = "0xa0",
                                .commands =
                                    {
                                        {0xa0, 0x01, 0x20},
                                        {0xa0, 0x10, 0x22},
                                    },
                                .reg = &Cpu8086::Registers::ax,
                                .memory_ops =
                                    {
                                        {
                                            .address = 0x2001,
                                            .data    = {0xf0},
                                        },
                                        {
                                            .address = 0x2210,
                                            .data    = {0xef},
                                        },
                                    },
                                .expects = {0xf0, 0xef},
                            },
                            MovFromMemToRegParams{
                                .name = "0xa1",
                                .commands =
                                    {
                                        {0xa1, 0x01, 0x0a},
                                        {0xa1, 0x10, 0x10},
                                    },
                                .reg = &Cpu8086::Registers::ax,
                                .memory_ops =
                                    {
                                        {
                                            .address = 0x0a01,
                                            .data    = {0xce, 0xfa},
                                        },
                                        {
                                            .address = 0x1010,
                                            .data    = {0xeb, 0xbe},
                                        },
                                    },
                                .expects = {0xface, 0xbeeb},
                            }),
                        generate_test_case_name);


// TEST_P(MovFromMemoryToRegTests, Mem8ToAl_0xa0)
// {
//     std::array<uint8_t, 3> opcode({0xa0, 0x01, 0x20});
//     memory_.write(0x00, opcode);
//     opcode[2] = 0x22;
//     memory_.write(0x03, opcode);

//     memory_.write8(0x2001, 0xF0);
//     memory_.write16(0x2201, 0xef);
//     sut_.step();
//     Cpu8086::Registers expected{};
//     expected.ax = 0x00f0;
//     expected.ip = 0x3;

//     EXPECT_EQ(sut_.get_registers(), expected);

//     expected.ax = 0x00ef;
//     expected.ip = 0x6;

//     sut_.step();
//     EXPECT_EQ(sut_.get_registers(), expected);
// }

// TEST_F(MovTests, Mem16ToAx_0xa1)
// {
// std::array<uint8_t, 3> opcode({0xa1, 0x01, 0x0a});
// memory_.write(0x00, opcode);
// opcode[2] = 0x10;
// memory_.write(0x03, opcode);
//
// memory_.write16(0x0a01, 0xface);
// memory_.write16(0x1001, 0xfade);
// sut_.step();
// Cpu8086::Registers expected{};
// expected.ax = 0xface;
// expected.ip = 0x3;
//
// EXPECT_EQ(sut_.get_registers(), expected);
//
// expected.ax = 0xfade;
// expected.ip = 0x6;
//
// sut_.step();
// EXPECT_EQ(sut_.get_registers(), expected);
// }
//
} // namespace msemu
