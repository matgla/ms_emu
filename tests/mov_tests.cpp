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

namespace msemu::cpu8086
{

struct MemoryOp
{
    std::size_t address;
    std::vector<uint8_t> data;
};

struct MovFromMemToRegParams
{
    std::string name;
    std::vector<std::vector<uint8_t>> commands;
    uint16_t Registers::*reg;
    std::vector<MemoryOp> memory_ops;
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
    Memory<1024 * 128> memory_;
    CpuForTest sut_;
};

TEST_P(MovFromMemToRegTests, MovFromMemoryToRegisterCmd)
{
    auto data        = GetParam();
    uint16_t address = 0;

    for (const auto& op : data.memory_ops)
    {
        memory_.write(op.address, op.data);
    }

    Registers expected{};
    for (std::size_t i = 0; i < data.commands.size(); ++i)
    {
        auto& opcode = data.commands[i];
        memory_.write(address, opcode);

        (expected.*data.reg) = data.expects[i];

        address += static_cast<uint16_t>(opcode.size());

        expected.ip = address;

        sut_.step();
        EXPECT_EQ(expected, sut_.get_registers());
    }
}

inline std::string
    generate_test_case_name(const ::testing::TestParamInfo<MovFromMemToRegParams>& info)
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
                                .reg = &Registers::ax,
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
                                .reg = &Registers::ax,
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
                            },
                            MovFromMemToRegParams{
                                .name = "0xb0",
                                .commands =
                                    {
                                        {0xb0, 0x10},
                                        {0xb0, 0xfa},
                                    },
                                .reg        = &Registers::ax,
                                .memory_ops = {},
                                .expects    = {0x10, 0xfa},
                            },
                            MovFromMemToRegParams{
                                .name = "0xb1",
                                .commands =
                                    {
                                        {0xb1, 0xe0},
                                        {0xb1, 0xfa},
                                    },
                                .reg        = &Registers::cx,
                                .memory_ops = {},
                                .expects    = {0xe0, 0xfa},
                            },
                            MovFromMemToRegParams{
                                .name = "0xb2",
                                .commands =
                                    {
                                        {0xb2, 0xe0},
                                        {0xb2, 0xfa},
                                    },
                                .reg        = &Registers::dx,
                                .memory_ops = {},
                                .expects    = {0xe0, 0xfa},
                            },
                            MovFromMemToRegParams{
                                .name = "0xb3",
                                .commands =
                                    {
                                        {0xb3, 0xe0},
                                        {0xb3, 0xfa},
                                    },
                                .reg        = &Registers::bx,
                                .memory_ops = {},
                                .expects    = {0xe0, 0xfa},
                            },
                            MovFromMemToRegParams{
                                .name = "0xb4",
                                .commands =
                                    {
                                        {0xb4, 0x10},
                                        {0xb4, 0xfa},
                                    },
                                .reg        = &Registers::ax,
                                .memory_ops = {},
                                .expects    = {0x1000, 0xfa00},
                            },
                            MovFromMemToRegParams{
                                .name = "0xb5",
                                .commands =
                                    {
                                        {0xb5, 0xe0},
                                        {0xb5, 0xfa},
                                    },
                                .reg        = &Registers::cx,
                                .memory_ops = {},
                                .expects    = {0xe000, 0xfa00},
                            },
                            MovFromMemToRegParams{
                                .name = "0xb6",
                                .commands =
                                    {
                                        {0xb6, 0xe0},
                                        {0xb6, 0xfa},
                                    },
                                .reg        = &Registers::dx,
                                .memory_ops = {},
                                .expects    = {0xe000, 0xfa00},
                            },
                            MovFromMemToRegParams{
                                .name = "0xb7",
                                .commands =
                                    {
                                        {0xb7, 0xe0},
                                        {0xb7, 0xfa},
                                    },
                                .reg        = &Registers::bx,
                                .memory_ops = {},
                                .expects    = {0xe000, 0xfa00},
                            },
                            MovFromMemToRegParams{
                                .name = "0xb8",
                                .commands =
                                    {
                                        {0xb8, 0xce, 0xfa},
                                        {0xb8, 0xef, 0xbe},
                                    },
                                .reg        = &Registers::ax,
                                .memory_ops = {},
                                .expects    = {0xface, 0xbeef},
                            },
                            MovFromMemToRegParams{
                                .name = "0xb9",
                                .commands =
                                    {
                                        {0xb9, 0xcd, 0xab},
                                        {0xb9, 0x01, 0xef},
                                    },
                                .reg        = &Registers::cx,
                                .memory_ops = {},
                                .expects    = {0xabcd, 0xef01},
                            },
                            MovFromMemToRegParams{
                                .name = "0xba",
                                .commands =
                                    {
                                        {0xba, 0x34, 0x12},
                                        {0xba, 0x78, 0x56},
                                    },
                                .reg        = &Registers::dx,
                                .memory_ops = {},
                                .expects    = {0x1234, 0x5678},
                            },
                            MovFromMemToRegParams{
                                .name = "0xbb",
                                .commands =
                                    {
                                        {0xbb, 0x10, 0x29},
                                        {0xbb, 0xfa, 0x12},
                                    },
                                .reg        = &Registers::bx,
                                .memory_ops = {},
                                .expects    = {0x2910, 0x12fa},
                            },
                            MovFromMemToRegParams{
                                .name = "0xbc",
                                .commands =
                                    {
                                        {0xbc, 0xce, 0xfa},
                                        {0xbc, 0xef, 0xbe},
                                    },
                                .reg        = &Registers::sp,
                                .memory_ops = {},
                                .expects    = {0xface, 0xbeef},
                            },
                            MovFromMemToRegParams{
                                .name = "0xbd",
                                .commands =
                                    {
                                        {0xbd, 0xcd, 0xab},
                                        {0xbd, 0x01, 0xef},
                                    },
                                .reg        = &Registers::bp,
                                .memory_ops = {},
                                .expects    = {0xabcd, 0xef01},
                            },
                            MovFromMemToRegParams{
                                .name = "0xbe",
                                .commands =
                                    {
                                        {0xbe, 0x34, 0x12},
                                        {0xbe, 0x78, 0x56},
                                    },
                                .reg        = &Registers::si,
                                .memory_ops = {},
                                .expects    = {0x1234, 0x5678},
                            },
                            MovFromMemToRegParams{
                                .name = "0xbf",
                                .commands =
                                    {
                                        {0xbf, 0x10, 0x29},
                                        {0xbf, 0xfa, 0x12},
                                    },
                                .reg        = &Registers::di,
                                .memory_ops = {},
                                .expects    = {0x2910, 0x12fa},
                            }),


                        generate_test_case_name);


} // namespace msemu::cpu8086
