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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <optional>
#include <source_location>
#include <vector>

#include "cpu8086_for_test.hpp"

namespace msemu::cpu8086
{

struct MemoryOp
{
    std::size_t address;
    std::vector<uint8_t> data;
};

struct TestData
{
    std::vector<uint8_t> cmd;
    MemoryOp memop;

    std::optional<Registers> init;
    std::optional<Registers> expect;
    std::optional<MemoryOp> expect_memory;
    uint8_t cycles;
    std::source_location location;
};

TestData add(const TestData& t, std::source_location l = std::source_location::current())
{
    TestData x = t;
    x.location = l;
    return x;
}

struct MovFromMemToRegParams
{
    std::string name;
    std::vector<TestData> data;
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

void stringify_array(const auto& data, auto& str)
{
    for (std::size_t i = 0; i < data.size(); ++i)
    {
        str << "0x" << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<uint32_t>(data[i]);
        if (i != data.size() - 1)
        {
            str << ", ";
        }
    }
}

void find_and_replace(std::string& data, const std::string& from, const std::string& to)
{
    std::size_t pos = data.find(from);
    while (pos != std::string::npos)
    {
        data.erase(pos, from.size());
        data.insert(pos, to);
        pos = data.find(from, pos + to.size());
    }
}

std::string print_cycles_info(const TestData& data)
{
    std::stringstream str;
    str << "Location: " << data.location.file_name() << ":" << data.location.line()
        << std::endl;
    return str.str();
}

std::string print_test_case_info(const TestData& data, std::string error, int number)
{
    std::stringstream str;

    str << "TC number: " << number << std::endl;
    str << "TC location: " << data.location.file_name() << ":" << data.location.line()
        << std::endl;
    str << "error msg: " << error << std::endl;

    str << "{" << std::endl;

    str << "    cmd: {";
    stringify_array(data.cmd, str);
    str << "}" << std::endl;

    str << "    memop: {" << std::endl;
    str << "        address: 0x" << data.memop.address << std::endl;
    str << "        data: {";
    stringify_array(data.memop.data, str);
    str << "}," << std::endl;
    str << "    }" << std::endl;

    str << "    init: {";
    if (data.init)
    {
        std::stringstream inits;
        PrintTo(*(data.init), &inits);
        std::string is = inits.str();
        find_and_replace(is, "    ", "         ");
        str << is;
        str << "    }" << std::endl;
    }
    else
    {
        str << "}" << std::endl;
    }
    str << "    expect: {";
    if (data.expect)
    {
        std::stringstream expects;
        PrintTo(*(data.expect), &expects);
        std::string is = expects.str();
        find_and_replace(is, "    ", "         ");
        str << is;
        str << "    }" << std::endl;
    }
    else
    {
        str << "}" << std::endl;
    }

    str << "    expect_memory: {";
    if (data.expect_memory)
    {
        str << std::endl;
        str << "        address: 0x" << data.expect_memory->address << std::endl;
        str << "        data: {";
        stringify_array(data.expect_memory->data, str);
        str << "}," << std::endl;
        str << "    }" << std::endl;
    }
    else
    {
        str << "}" << std::endl;
    }


    str << "}" << std::endl;
    return str.str();
}

TEST_P(MovFromMemToRegTests, MovFromMemoryToRegisterCmd)
{
    auto data = GetParam();
    int i     = 0;
    for (const auto& test_data : data.data)
    {
        if (test_data.memop.data.size())
        {
            memory_.write(test_data.memop.address, test_data.memop.data);
        }

        if (test_data.init)
        {
            std::memcpy(&sut_.get_registers(), &(*test_data.init), sizeof(Registers));
        }
        auto& opcode = test_data.cmd;
        memory_.write(sut_.get_registers().ip, opcode);
        sut_.step();

        if (test_data.expect)
        {
            EXPECT_EQ(test_data.expect, sut_.get_registers())
                << print_test_case_info(test_data, sut_.get_error(), i);
        }
        if (test_data.expect_memory)
        {
            std::vector<uint8_t> from_memory(test_data.expect_memory->data.size());
            memory_.read(test_data.expect_memory->address, from_memory);
            EXPECT_THAT(from_memory,
                        ::testing::ElementsAreArray(test_data.expect_memory->data))
                << print_test_case_info(test_data, sut_.get_error(), i);
        }

        EXPECT_EQ(sut_.last_instruction_cost(), test_data.cycles)
            << print_cycles_info(test_data);

        ++i;
    }
}

inline std::string
    generate_test_case_name(const ::testing::TestParamInfo<MovFromMemToRegParams>& info)
{
    return info.param.name;
}

inline void PrintTo(const MovFromMemToRegParams& reg, ::std::ostream* os)
{
    *os << "name: " << reg.name << std::endl;
}

auto get_mov_values()
{
    return ::testing::Values(
        MovFromMemToRegParams{
            .name = "0xa0",
            .data =
                {
                    add({
                        .cmd    = {0xa0, 0x01, 0x20},
                        .memop  = MemoryOp{.address = 0x2001, .data = {0xf0}},
                        .expect = Registers{.ax = 0xf0, .ip = 3},
                        .cycles = 0x0a,
                    }),
                    add({
                        .cmd    = {0xa0, 0x10, 0x22},
                        .memop  = MemoryOp{.address = 0x2210, .data = {0xef}},
                        .expect = Registers{.ax = 0xef, .ip = 6},
                        .cycles = 0x0a,
                    }),
                },
        },
        MovFromMemToRegParams{
            .name = "0xa1",
            .data =
                {
                    add({
                        .cmd    = {0xa1, 0x01, 0x0a},
                        .memop  = MemoryOp{.address = 0x0a01, .data = {0xce, 0xfa}},
                        .expect = Registers{.ax = 0xface, .ip = 3},
                        .cycles = 0x0a,
                    }),
                    add({
                        .cmd    = {0xa1, 0x10, 0x10},
                        .memop  = MemoryOp{.address = 0x1010, .data = {0xeb, 0xbe}},
                        .expect = Registers{.ax = 0xbeeb, .ip = 6},
                        .cycles = 0x0a,
                    }),
                },
        },
        MovFromMemToRegParams{
            .name = "0xa2",
            .data =
                {
                    add({
                        .cmd           = {0xa2, 0x01, 0x20},
                        .init          = Registers{.ax = 0xab},
                        .expect        = Registers{.ax = 0xab, .ip = 3},
                        .expect_memory = MemoryOp{.address = 0x2001, .data = {0xab}},
                        .cycles        = 0x0a,
                    }),
                    add({
                        .cmd           = {0xa2, 0x10, 0x22},
                        .init          = Registers{.ax = 0xef},
                        .expect        = Registers{.ax = 0xef, .ip = 3},
                        .expect_memory = MemoryOp{.address = 0x2210, .data = {0xef}},
                        .cycles        = 0x0a,
                    }),
                },
        },
        MovFromMemToRegParams{
            .name = "0xa3",
            .data =
                {
                    add({
                        .cmd    = {0xa3, 0x01, 0x20},
                        .init   = Registers{.ax = 0xabcd},
                        .expect = Registers{.ax = 0xabcd, .ip = 3},
                        .expect_memory =
                            MemoryOp{.address = 0x2001, .data = {0xcd, 0xab}},
                        .cycles = 0x0a,
                    }),
                    add(
                        {
                            .cmd    = {0xa3, 0x10, 0x22},
                            .init   = Registers{.ax = 0xdeef},
                            .expect = Registers{.ax = 0xdeef, .ip = 3},
                            .expect_memory =
                                MemoryOp{.address = 0x2210, .data = {0xef, 0xde}},
                            .cycles = 0x0a,
                        }),
                },
        },
        MovFromMemToRegParams{
            .name = "0xb0",
            .data =
                {
                    add(
                        {
                            .cmd    = {0xb0, 0xe0},
                            .expect = Registers{.ax = 0xe0, .ip = 2},
                            .cycles = 0x04,
                        }),
                    add(
                        {
                            .cmd    = {0xb0, 0xfa},
                            .expect = Registers{.ax = 0xfa, .ip = 4},
                            .cycles = 0x04,
                        }),
                },

        },
        MovFromMemToRegParams{
            .name = "0xb1",
            .data =
                {
                    add(
                        {
                            .cmd    = {0xb1, 0xe0},
                            .expect = Registers{.cx = 0xe0, .ip = 2},
                            .cycles = 0x04,
                        }),
                    add(
                        {
                            .cmd    = {0xb1, 0x10},
                            .expect = Registers{.cx = 0x10, .ip = 4},
                            .cycles = 0x04,
                        }),
                },

        },
        MovFromMemToRegParams{
            .name = "0xb2",
            .data =
                {
                    add(
                        {
                            .cmd    = {0xb2, 0x0a},
                            .expect = Registers{.dx = 0x0a, .ip = 2},
                            .cycles = 0x04,
                        }),
                    add(
                        {
                            .cmd    = {0xb2, 0xbc},
                            .expect = Registers{.dx = 0xbc, .ip = 4},
                            .cycles = 0x04,
                        }),
                },

        },
        MovFromMemToRegParams{
            .name = "0xb3",
            .data =
                {
                    add(
                        {
                            .cmd    = {0xb3, 0x01},
                            .expect = Registers{.bx = 0x01, .ip = 2},
                            .cycles = 0x04,
                        }),
                    add(
                        {
                            .cmd    = {0xb3, 0xab},
                            .expect = Registers{.bx = 0xab, .ip = 4},
                            .cycles = 0x04,
                        }),
                },

        },
        MovFromMemToRegParams{
            .name = "0xb4",
            .data =
                {
                    add(
                        {
                            .cmd    = {0xb4, 0x0a},
                            .expect = Registers{.ax = 0x0a00, .ip = 2},
                            .cycles = 0x04,
                        }),
                    add(
                        {
                            .cmd    = {0xb4, 0x10},
                            .expect = Registers{.ax = 0x1000, .ip = 4},
                            .cycles = 0x04,
                        }),
                },

        },
        MovFromMemToRegParams{
            .name = "0xb5",
            .data =
                {
                    add(
                        {
                            .cmd    = {0xb5, 0xee},
                            .expect = Registers{.cx = 0xee00, .ip = 2},
                            .cycles = 0x04,
                        }),
                    add(
                        {
                            .cmd    = {0xb5, 0xab},
                            .expect = Registers{.cx = 0xab00, .ip = 4},
                            .cycles = 0x04,
                        }),
                },

        },
        MovFromMemToRegParams{
            .name = "0xb6",
            .data =
                {
                    add(
                        {
                            .cmd    = {0xb6, 0xfa},
                            .expect = Registers{.dx = 0xfa00, .ip = 2},
                            .cycles = 0x04,
                        }),
                    add(
                        {
                            .cmd    = {0xb6, 0x12},
                            .expect = Registers{.dx = 0x1200, .ip = 4},
                            .cycles = 0x04,
                        }),
                },

        },
        MovFromMemToRegParams{
            .name = "0xb7",
            .data =
                {
                    add(
                        {
                            .cmd    = {0xb7, 0xb7},
                            .expect = Registers{.bx = 0xb700, .ip = 2},
                            .cycles = 0x04,
                        }),
                    add(
                        {
                            .cmd    = {0xb7, 0x10},
                            .expect = Registers{.bx = 0x1000, .ip = 4},
                            .cycles = 0x04,
                        }),
                },

        },
        MovFromMemToRegParams{
            .name = "0xb8",
            .data =
                {
                    add(
                        {
                            .cmd    = {0xb8, 0xce, 0xfa},
                            .expect = Registers{.ax = 0xface, .ip = 3},
                            .cycles = 0x04,
                        }),
                    add(
                        {
                            .cmd    = {0xb8, 0xef, 0xbe},
                            .expect = Registers{.ax = 0xbeef, .ip = 6},
                            .cycles = 0x04,
                        }),
                },

        },
        MovFromMemToRegParams{
            .name = "0xb9",
            .data =
                {
                    add(
                        {
                            .cmd    = {0xb9, 0xcd, 0xab},
                            .expect = Registers{.cx = 0xabcd, .ip = 3},
                            .cycles = 0x04,
                        }),
                    add(
                        {
                            .cmd    = {0xb9, 0x01, 0xef},
                            .expect = Registers{.cx = 0xef01, .ip = 6},
                            .cycles = 0x04,
                        }),
                },

        },
        MovFromMemToRegParams{
            .name = "0xba",
            .data =
                {
                    add(
                        {
                            .cmd    = {0xba, 0xcd, 0xab},
                            .expect = Registers{.dx = 0xabcd, .ip = 3},
                            .cycles = 0x04,
                        }),
                    add(
                        {
                            .cmd    = {0xba, 0x01, 0xef},
                            .expect = Registers{.dx = 0xef01, .ip = 6},
                            .cycles = 0x04,
                        }),
                },

        },
        MovFromMemToRegParams{
            .name = "0xbb",
            .data =
                {
                    add(
                        {
                            .cmd    = {0xbb, 0xcd, 0xab},
                            .expect = Registers{.bx = 0xabcd, .ip = 3},
                            .cycles = 0x04,
                        }),
                    add(
                        {
                            .cmd    = {0xbb, 0x01, 0xef},
                            .expect = Registers{.bx = 0xef01, .ip = 6},
                            .cycles = 0x04,
                        }),
                },

        },
        MovFromMemToRegParams{
            .name = "0xbc",
            .data =
                {
                    add(
                        {
                            .cmd    = {0xbc, 0xcd, 0xab},
                            .expect = Registers{.sp = 0xabcd, .ip = 3},
                            .cycles = 0x04,
                        }),
                    add(
                        {
                            .cmd    = {0xbc, 0x01, 0xef},
                            .expect = Registers{.sp = 0xef01, .ip = 6},
                            .cycles = 0x04,
                        }),
                },

        },
        MovFromMemToRegParams{
            .name = "0xbd",
            .data =
                {
                    add(
                        {
                            .cmd    = {0xbd, 0xcd, 0xab},
                            .expect = Registers{.bp = 0xabcd, .ip = 3},
                            .cycles = 0x04,
                        }),
                    add(
                        {
                            .cmd    = {0xbd, 0x01, 0xef},
                            .expect = Registers{.bp = 0xef01, .ip = 6},
                            .cycles = 0x04,
                        }),
                },

        },
        MovFromMemToRegParams{
            .name = "0xbe",
            .data =
                {
                    add(
                        {
                            .cmd    = {0xbe, 0xcd, 0xab},
                            .expect = Registers{.si = 0xabcd, .ip = 3},
                            .cycles = 0x04,
                        }),
                    add(
                        {
                            .cmd    = {0xbe, 0x01, 0xef},
                            .expect = Registers{.si = 0xef01, .ip = 6},
                            .cycles = 0x04,
                        }),
                },

        },
        MovFromMemToRegParams{
            .name = "0xbf",
            .data =
                {
                    add(
                        {
                            .cmd    = {0xbf, 0xcd, 0xab},
                            .expect = Registers{.di = 0xabcd, .ip = 3},
                            .cycles = 0x04,
                        }),
                    add(
                        {
                            .cmd    = {0xbf, 0x01, 0xef},
                            .expect = Registers{.di = 0xef01, .ip = 6},
                            .cycles = 0x04,
                        }),
                },

        },
        MovFromMemToRegParams{
            .name = "0x8a",
            .data =
                {
                    add(
                        {
                            .cmd   = {0x8a, 0x00},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.ax = 0xfa00, .bx = 0x80, .si = 0x04},
                            .expect =
                                Registers{.ax = 0xfa3a, .bx = 0x80, .si = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x01},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.ax = 0x3a, .bx = 0x80, .di = 0x04, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x02},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.ax = 0x3a, .si = 0x04, .bp = 0x80, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x03},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.ax = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.ax = 0xfa3a, .di = 0x80, .bp = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x04},
                            .memop  = MemoryOp{.address = 0xff, .data = {0x3a}},
                            .init   = Registers{.ax = 0xfa00, .si = 0xff},
                            .expect = Registers{.ax = 0xfa3a, .si = 0xff, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x05},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.ax = 0xfa00, .di = 0x84},
                            .expect = Registers{.ax = 0xfa3a, .di = 0x84, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x06, 0x11, 0x20},
                            .memop  = MemoryOp{.address = 0x2011, .data = {0x3a}},
                            .init   = Registers{},
                            .expect = Registers{.ax = 0x3a, .ip = 4},
                            .cycles = 0x0e,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x07},
                            .memop  = MemoryOp{.address = 0xab, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.ax = 0x3a, .bx = 0xab, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x08},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .cx = 0xfa00, .si = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .cx = 0xfa3a, .si = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x09},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .cx = 0x3a, .di = 0x04, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x0a},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.cx = 0x3a, .si = 0x04, .bp = 0x80, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x0b},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.cx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.cx = 0xfa3a, .di = 0x80, .bp = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x0c},
                            .memop  = MemoryOp{.address = 0xff, .data = {0x3a}},
                            .init   = Registers{.cx = 0xfa00, .si = 0xff},
                            .expect = Registers{.cx = 0xfa3a, .si = 0xff, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x0d},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.cx = 0xfa00, .di = 0x84},
                            .expect = Registers{.cx = 0xfa3a, .di = 0x84, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x0e, 0x11, 0x20},
                            .memop  = MemoryOp{.address = 0x2011, .data = {0x3a}},
                            .init   = Registers{},
                            .expect = Registers{.cx = 0x3a, .ip = 4},
                            .cycles = 0x0e,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x0f},
                            .memop  = MemoryOp{.address = 0xab, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0xab, .cx = 0x3a, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x10},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .dx = 0xfa00, .si = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .dx = 0xfa3a, .si = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x11},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .dx = 0x3a, .di = 0x04, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x12},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.dx = 0x3a, .si = 0x04, .bp = 0x80, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x13},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.dx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.dx = 0xfa3a, .di = 0x80, .bp = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x14},
                            .memop  = MemoryOp{.address = 0xff, .data = {0x3a}},
                            .init   = Registers{.dx = 0xfa00, .si = 0xff},
                            .expect = Registers{.dx = 0xfa3a, .si = 0xff, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x15},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.dx = 0xfa00, .di = 0x84},
                            .expect = Registers{.dx = 0xfa3a, .di = 0x84, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x16, 0x11, 0x20},
                            .memop  = MemoryOp{.address = 0x2011, .data = {0x3a}},
                            .init   = Registers{},
                            .expect = Registers{.dx = 0x3a, .ip = 4},
                            .cycles = 0x0e,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x17},
                            .memop  = MemoryOp{.address = 0xab, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0xab, .dx = 0x3a, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x18},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.bx = 0x80, .si = 0x04},
                            .expect = Registers{.bx = 0x3a, .si = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x19},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.bx = 0x80, .di = 0x04},
                            .expect = Registers{.bx = 0x3a, .di = 0x04, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x1a},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.bx = 0x3a, .si = 0x04, .bp = 0x80, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x1b},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.bx = 0xfa3a, .di = 0x80, .bp = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x1c},
                            .memop  = MemoryOp{.address = 0xff, .data = {0x3a}},
                            .init   = Registers{.bx = 0xfa00, .si = 0xff},
                            .expect = Registers{.bx = 0xfa3a, .si = 0xff, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x1d},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.bx = 0xfa00, .di = 0x84},
                            .expect = Registers{.bx = 0xfa3a, .di = 0x84, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x1e, 0x11, 0x20},
                            .memop  = MemoryOp{.address = 0x2011, .data = {0x3a}},
                            .init   = Registers{},
                            .expect = Registers{.bx = 0x3a, .ip = 4},
                            .cycles = 0x0e,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x1f},
                            .memop  = MemoryOp{.address = 0xab, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0x3a, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x20},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.ax = 0xfa00, .bx = 0x80, .si = 0x04},
                            .expect =
                                Registers{.ax = 0x3a00, .bx = 0x80, .si = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x21},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.ax = 0x3a00, .bx = 0x80, .di = 0x04, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x22},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.ax = 0x3a00, .si = 0x04, .bp = 0x80, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x23},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.ax = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.ax = 0x3a00, .di = 0x80, .bp = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x24},
                            .memop  = MemoryOp{.address = 0xff, .data = {0x3a}},
                            .init   = Registers{.ax = 0xfa00, .si = 0xff},
                            .expect = Registers{.ax = 0x3a00, .si = 0xff, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x25},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.ax = 0xfa00, .di = 0x84},
                            .expect = Registers{.ax = 0x3a00, .di = 0x84, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x26, 0x11, 0x20},
                            .memop  = MemoryOp{.address = 0x2011, .data = {0x3a}},
                            .init   = Registers{},
                            .expect = Registers{.ax = 0x3a00, .ip = 4},
                            .cycles = 0x0e,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x27},
                            .memop  = MemoryOp{.address = 0xab, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.ax = 0x3a00, .bx = 0xab, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x28},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .cx = 0xfa00, .si = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .cx = 0x3a00, .si = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x29},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .cx = 0x3a00, .di = 0x04, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x2a},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.cx = 0x3a00, .si = 0x04, .bp = 0x80, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x2b},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.cx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.cx = 0x3a00, .di = 0x80, .bp = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x2c},
                            .memop  = MemoryOp{.address = 0xff, .data = {0x3a}},
                            .init   = Registers{.cx = 0xfa00, .si = 0xff},
                            .expect = Registers{.cx = 0x3a00, .si = 0xff, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x2d},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.cx = 0xfa00, .di = 0x84},
                            .expect = Registers{.cx = 0x3a00, .di = 0x84, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x2e, 0x11, 0x20},
                            .memop  = MemoryOp{.address = 0x2011, .data = {0x3a}},
                            .init   = Registers{},
                            .expect = Registers{.cx = 0x3a00, .ip = 4},
                            .cycles = 0x0e,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x2f},
                            .memop  = MemoryOp{.address = 0xab, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0xab, .cx = 0x3a00, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x30},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .dx = 0xfa00, .si = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .dx = 0x3a00, .si = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x31},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .dx = 0x3a00, .di = 0x04, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x32},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.dx = 0x3a00, .si = 0x04, .bp = 0x80, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x33},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.dx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.dx = 0x3a00, .di = 0x80, .bp = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x34},
                            .memop  = MemoryOp{.address = 0xff, .data = {0x3a}},
                            .init   = Registers{.dx = 0xfa00, .si = 0xff},
                            .expect = Registers{.dx = 0x3a00, .si = 0xff, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x35},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.dx = 0xfa00, .di = 0x84},
                            .expect = Registers{.dx = 0x3a00, .di = 0x84, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x36, 0x11, 0x20},
                            .memop  = MemoryOp{.address = 0x2011, .data = {0x3a}},
                            .init   = Registers{},
                            .expect = Registers{.dx = 0x3a00, .ip = 4},
                            .cycles = 0x0e,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x37},
                            .memop  = MemoryOp{.address = 0xab, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0xab, .dx = 0x3a00, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x38},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.bx = 0x80, .si = 0x04},
                            .expect = Registers{.bx = 0x3a80, .si = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x39},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.bx = 0x80, .di = 0x04},
                            .expect = Registers{.bx = 0x3a80, .di = 0x04, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x3a},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.bx = 0x3a00, .si = 0x04, .bp = 0x80, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x3b},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.bx = 0x3a00, .di = 0x80, .bp = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x3c},
                            .memop  = MemoryOp{.address = 0xff, .data = {0x3a}},
                            .init   = Registers{.bx = 0xfa00, .si = 0xff},
                            .expect = Registers{.bx = 0x3a00, .si = 0xff, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x3d},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.bx = 0xfa00, .di = 0x84},
                            .expect = Registers{.bx = 0x3a00, .di = 0x84, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x3e, 0x11, 0x20},
                            .memop  = MemoryOp{.address = 0x2011, .data = {0x3a}},
                            .init   = Registers{},
                            .expect = Registers{.bx = 0x3a00, .ip = 4},
                            .cycles = 0x0e,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x3f},
                            .memop  = MemoryOp{.address = 0xab, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0x3aab, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x40, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.ax = 0xfa00, .bx = 0x80, .si = 0x04},
                            .expect =
                                Registers{.ax = 0xfa3a, .bx = 0x80, .si = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x41, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.ax = 0x3a, .bx = 0x80, .di = 0x04, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x42, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.ax = 0x3a, .si = 0x04, .bp = 0x80, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x43, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.ax = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.ax = 0xfa3a, .di = 0x80, .bp = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x44, 0x01},
                            .memop  = MemoryOp{.address = 0x100, .data = {0x3a}},
                            .init   = Registers{.ax = 0xfa00, .si = 0xff},
                            .expect = Registers{.ax = 0xfa3a, .si = 0xff, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x45, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.ax = 0xfa00, .di = 0x84},
                            .expect = Registers{.ax = 0xfa3a, .di = 0x84, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x46, 0x20},
                            .memop  = MemoryOp{.address = 0x60, .data = {0x3a}},
                            .init   = Registers{.bp = 0x40},
                            .expect = Registers{.ax = 0x3a, .bp = 0x40, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x47, 0x20},
                            .memop  = MemoryOp{.address = 0xcb, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.ax = 0x3a, .bx = 0xab, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x48, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .cx = 0xfa00, .si = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .cx = 0xfa3a, .si = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x49, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .cx = 0x3a, .di = 0x04, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x4a, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.cx = 0x3a, .si = 0x04, .bp = 0x80, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x4b, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.cx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.cx = 0xfa3a, .di = 0x80, .bp = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x4c, 0x01},
                            .memop  = MemoryOp{.address = 0x100, .data = {0x3a}},
                            .init   = Registers{.cx = 0xfa00, .si = 0xff},
                            .expect = Registers{.cx = 0xfa3a, .si = 0xff, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x4d, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.cx = 0xfa00, .di = 0x84},
                            .expect = Registers{.cx = 0xfa3a, .di = 0x84, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x4e, 0x89},
                            .memop  = MemoryOp{.address = 0x99, .data = {0x3a}},
                            .init   = Registers{.bp = 0x10},
                            .expect = Registers{.cx = 0x3a, .bp = 0x10, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x4f, 0x01},
                            .memop  = MemoryOp{.address = 0xac, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0xab, .cx = 0x3a, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x50, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .dx = 0xfa00, .si = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .dx = 0xfa3a, .si = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x51, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .dx = 0x3a, .di = 0x04, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x52, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.dx = 0x3a, .si = 0x04, .bp = 0x80, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x53, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.dx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.dx = 0xfa3a, .di = 0x80, .bp = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x54, 0x01},
                            .memop  = MemoryOp{.address = 0x100, .data = {0x3a}},
                            .init   = Registers{.dx = 0xfa00, .si = 0xff},
                            .expect = Registers{.dx = 0xfa3a, .si = 0xff, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x55, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.dx = 0xfa00, .di = 0x84},
                            .expect = Registers{.dx = 0xfa3a, .di = 0x84, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x56, 0x40},
                            .memop  = MemoryOp{.address = 0x60, .data = {0x3a}},
                            .init   = Registers{.bp = 0x20},
                            .expect = Registers{.dx = 0x3a, .bp = 0x20, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x57, 0x1},
                            .memop  = MemoryOp{.address = 0xac, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0xab, .dx = 0x3a, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x58, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.bx = 0x80, .si = 0x04},
                            .expect = Registers{.bx = 0x3a, .si = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x59, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.bx = 0x80, .di = 0x04},
                            .expect = Registers{.bx = 0x3a, .di = 0x04, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x5a, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.bx = 0x3a, .si = 0x04, .bp = 0x80, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x5b, 0x4},
                            .memop = MemoryOp{.address = 0x88, .data = {0x3a}},
                            .init  = Registers{.bx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.bx = 0xfa3a, .di = 0x80, .bp = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x5c, 0x1},
                            .memop  = MemoryOp{.address = 0x100, .data = {0x3a}},
                            .init   = Registers{.bx = 0xfa00, .si = 0xff},
                            .expect = Registers{.bx = 0xfa3a, .si = 0xff, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x5d, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.bx = 0xfa00, .di = 0x84},
                            .expect = Registers{.bx = 0xfa3a, .di = 0x84, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x5e, 0x25},
                            .memop  = MemoryOp{.address = 0x55, .data = {0x3a}},
                            .init   = Registers{.bp = 0x30},
                            .expect = Registers{.bx = 0x3a, .bp = 0x30, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x5f, 0x01},
                            .memop  = MemoryOp{.address = 0xac, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0x3a, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x60, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.ax = 0xfa00, .bx = 0x80, .si = 0x04},
                            .expect =
                                Registers{.ax = 0x3a00, .bx = 0x80, .si = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x61, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.ax = 0x3a00, .bx = 0x80, .di = 0x04, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x62, 0x1},
                            .memop = MemoryOp{.address = 0x85, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.ax = 0x3a00, .si = 0x04, .bp = 0x80, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x63, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.ax = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.ax = 0x3a00, .di = 0x80, .bp = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x64, 0x1},
                            .memop  = MemoryOp{.address = 0x100, .data = {0x3a}},
                            .init   = Registers{.ax = 0xfa00, .si = 0xff},
                            .expect = Registers{.ax = 0x3a00, .si = 0xff, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x65, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.ax = 0xfa00, .di = 0x84},
                            .expect = Registers{.ax = 0x3a00, .di = 0x84, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x66, 0x11},
                            .memop  = MemoryOp{.address = 0x41, .data = {0x3a}},
                            .init   = Registers{.bp = 0x30},
                            .expect = Registers{.ax = 0x3a00, .bp = 0x30, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x67, 0x01},
                            .memop  = MemoryOp{.address = 0xac, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.ax = 0x3a00, .bx = 0xab, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x68, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .cx = 0xfa00, .si = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .cx = 0x3a00, .si = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x69, 0x01},
                            .memop = MemoryOp{.address = 0x85, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .cx = 0x3a00, .di = 0x04, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x6a, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.cx = 0x3a00, .si = 0x04, .bp = 0x80, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x6b, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.cx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.cx = 0x3a00, .di = 0x80, .bp = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x6c, 0x01},
                            .memop  = MemoryOp{.address = 0x100, .data = {0x3a}},
                            .init   = Registers{.cx = 0xfa00, .si = 0xff},
                            .expect = Registers{.cx = 0x3a00, .si = 0xff, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x6d, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.cx = 0xfa00, .di = 0x84},
                            .expect = Registers{.cx = 0x3a00, .di = 0x84, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x6e, 0x20},
                            .memop  = MemoryOp{.address = 0x30, .data = {0x3a}},
                            .init   = Registers{.bp = 0x10},
                            .expect = Registers{.cx = 0x3a00, .bp = 0x10, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x6f, 0x1},
                            .memop  = MemoryOp{.address = 0xac, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0xab, .cx = 0x3a00, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x70, 0x1},
                            .memop = MemoryOp{.address = 0x85, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .dx = 0xfa00, .si = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .dx = 0x3a00, .si = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x71, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .dx = 0x3a00, .di = 0x04, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x72, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.dx = 0x3a00, .si = 0x04, .bp = 0x80, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x73, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.dx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.dx = 0x3a00, .di = 0x80, .bp = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x74, 0x01},
                            .memop  = MemoryOp{.address = 0x100, .data = {0x3a}},
                            .init   = Registers{.dx = 0xfa00, .si = 0xff},
                            .expect = Registers{.dx = 0x3a00, .si = 0xff, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x75, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.dx = 0xfa00, .di = 0x84},
                            .expect = Registers{.dx = 0x3a00, .di = 0x84, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x76, 0x20},
                            .memop  = MemoryOp{.address = 0x40, .data = {0x3a}},
                            .init   = Registers{.bp = 0x20},
                            .expect = Registers{.dx = 0x3a00, .bp = 0x20, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x77, 0x1},
                            .memop  = MemoryOp{.address = 0xac, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0xab, .dx = 0x3a00, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x78, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.bx = 0x80, .si = 0x04},
                            .expect = Registers{.bx = 0x3a80, .si = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x79, 0x20},
                            .memop  = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init   = Registers{.bx = 0x80, .di = 0x04},
                            .expect = Registers{.bx = 0x3a80, .di = 0x04, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x7a, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.bx = 0x3a00, .si = 0x04, .bp = 0x80, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x7b, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.bx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.bx = 0x3a00, .di = 0x80, .bp = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x7c, 0x01},
                            .memop  = MemoryOp{.address = 0x100, .data = {0x3a}},
                            .init   = Registers{.bx = 0xfa00, .si = 0xff},
                            .expect = Registers{.bx = 0x3a00, .si = 0xff, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x7d, 0x01},
                            .memop  = MemoryOp{.address = 0x85, .data = {0x3a}},
                            .init   = Registers{.bx = 0xfa00, .di = 0x84},
                            .expect = Registers{.bx = 0x3a00, .di = 0x84, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x7e, 0x20},
                            .memop  = MemoryOp{.address = 0x65, .data = {0x3a}},
                            .init   = Registers{.bp = 0x45},
                            .expect = Registers{.bx = 0x3a00, .bp = 0x45, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x7f, 0x1},
                            .memop  = MemoryOp{.address = 0xac, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0x3aab, .ip = 3},
                            .cycles = 0x11,
                        }),

                },
        },
        MovFromMemToRegParams{
            .name = "0x88",
            .data =
                {
                    add(
                        {
                            .cmd  = {0x88, 0x00},
                            .init = Registers{.ax = 0x3a, .bx = 0x80, .si = 0x04},
                            .expect =
                                Registers{.ax = 0x3a, .bx = 0x80, .si = 0x04, .ip = 2},
                            .expect_memory = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .cycles        = 0x10,
                        }),
                    add(
                        {
                            .cmd  = {0x88, 0x01},
                            .init = Registers{.ax = 0x3a, .bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.ax = 0x3a, .bx = 0x80, .di = 0x04, .ip = 2},
                            .expect_memory = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .cycles        = 0x11,
                        }),
                    add(
                        {
                            .cmd  = {0x88, 0x02},
                            .init = Registers{.ax = 0x3a, .si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.ax = 0x3a, .si = 0x04, .bp = 0x80, .ip = 2},
                            .expect_memory = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .cycles        = 0x11,
                        }),
                    add(
                        {
                            .cmd  = {0x88, 0x03},
                            .init = Registers{.ax = 0x3a, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.ax = 0x3a, .di = 0x80, .bp = 0x04, .ip = 2},
                            .expect_memory = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .cycles        = 0x10,
                        }),
                    add(
                        {
                            .cmd           = {0x88, 0x04},
                            .init          = Registers{.ax = 0x3a, .si = 0xff},
                            .expect        = Registers{.ax = 0x3a, .si = 0xff, .ip = 2},
                            .expect_memory = MemoryOp{.address = 0xff, .data = {0x3a}},
                            .cycles        = 0x0e,
                        }),
                    add(
                        {
                            .cmd           = {0x88, 0x05},
                            .init          = Registers{.ax = 0x3a, .di = 0x84},
                            .expect        = Registers{.ax = 0x3a, .di = 0x84, .ip = 2},
                            .expect_memory = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .cycles        = 0x0e,
                        }),
                    add(
                        {
                            .cmd           = {0x88, 0x06, 0x11, 0x20},
                            .init          = Registers{.ax = 0x3a},
                            .expect        = Registers{.ax = 0x3a, .ip = 4},
                            .expect_memory = MemoryOp{.address = 0x2011, .data = {0x3a}},
                            .cycles        = 0x0f,
                        }),
                    add(
                        {
                            .cmd           = {0x88, 0x07},
                            .init          = Registers{.ax = 0x3a, .bx = 0xab},
                            .expect        = Registers{.ax = 0x3a, .bx = 0xab, .ip = 2},
                            .expect_memory = MemoryOp{.address = 0xab, .data = {0x3a}},
                            .cycles        = 0x0e,
                        }),
                    add(
                        {
                            .cmd  = {0x88, 0x08},
                            .init = Registers{.bx = 0x80, .cx = 0x3a, .si = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .cx = 0x3a, .si = 0x04, .ip = 2},
                            .expect_memory = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .cycles        = 0x10,
                        }),
                    add(
                        {
                            .cmd  = {0x88, 0x09},
                            .init = Registers{.bx = 0x80, .cx = 0x3a, .di = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .cx = 0x3a, .di = 0x04, .ip = 2},
                            .expect_memory = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .cycles        = 0x11,
                        }),
                    add(
                        {
                            .cmd  = {0x88, 0x0a},
                            .init = Registers{.cx = 0x3a, .si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.cx = 0x3a, .si = 0x04, .bp = 0x80, .ip = 2},
                            .expect_memory = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .cycles        = 0x11,
                        }),
                    add(
                        {
                            .cmd  = {0x88, 0x0b},
                            .init = Registers{.cx = 0x3a, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.cx = 0x3a, .di = 0x80, .bp = 0x04, .ip = 2},
                            .expect_memory = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .cycles        = 0x10,
                        }),
                    add(
                        {
                            .cmd           = {0x88, 0x0c},
                            .init          = Registers{.cx = 0x3a, .si = 0xff},
                            .expect        = Registers{.cx = 0x3a, .si = 0xff, .ip = 2},
                            .expect_memory = MemoryOp{.address = 0xff, .data = {0x3a}},
                            .cycles        = 0x0e,
                        }),
                    add(
                        {
                            .cmd           = {0x88, 0x0d},
                            .init          = Registers{.cx = 0x3a, .di = 0x84},
                            .expect        = Registers{.cx = 0x3a, .di = 0x84, .ip = 2},
                            .expect_memory = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .cycles        = 0x0e,
                        }),
                    add(
                        {
                            .cmd           = {0x88, 0x0e, 0x11, 0x20},
                            .init          = Registers{.cx = 0x3a},
                            .expect        = Registers{.cx = 0x3a, .ip = 4},
                            .expect_memory = MemoryOp{.address = 0x2011, .data = {0x3a}},
                            .cycles        = 0x0f,
                        }),
                    add(
                        {
                            .cmd           = {0x88, 0x0f},
                            .init          = Registers{.bx = 0xab, .cx = 0x3a},
                            .expect        = Registers{.bx = 0xab, .cx = 0x3a, .ip = 2},
                            .expect_memory = MemoryOp{.address = 0xab, .data = {0x3a}},
                            .cycles        = 0x0e,
                        }),
                    add(
                        {
                            .cmd  = {0x88, 0x10},
                            .init = Registers{.bx = 0x80, .dx = 0x3a, .si = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .dx = 0x3a, .si = 0x04, .ip = 2},
                            .expect_memory = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .cycles        = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x11},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .dx = 0x3a, .di = 0x04, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x12},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.dx = 0x3a, .si = 0x04, .bp = 0x80, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x13},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.dx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.dx = 0xfa3a, .di = 0x80, .bp = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x14},
                            .memop  = MemoryOp{.address = 0xff, .data = {0x3a}},
                            .init   = Registers{.dx = 0xfa00, .si = 0xff},
                            .expect = Registers{.dx = 0xfa3a, .si = 0xff, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x15},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.dx = 0xfa00, .di = 0x84},
                            .expect = Registers{.dx = 0xfa3a, .di = 0x84, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x16, 0x11, 0x20},
                            .memop  = MemoryOp{.address = 0x2011, .data = {0x3a}},
                            .init   = Registers{},
                            .expect = Registers{.dx = 0x3a, .ip = 4},
                            .cycles = 0x0e,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x17},
                            .memop  = MemoryOp{.address = 0xab, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0xab, .dx = 0x3a, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x18},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.bx = 0x80, .si = 0x04},
                            .expect = Registers{.bx = 0x3a, .si = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x19},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.bx = 0x80, .di = 0x04},
                            .expect = Registers{.bx = 0x3a, .di = 0x04, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x1a},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.bx = 0x3a, .si = 0x04, .bp = 0x80, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x1b},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.bx = 0xfa3a, .di = 0x80, .bp = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x1c},
                            .memop  = MemoryOp{.address = 0xff, .data = {0x3a}},
                            .init   = Registers{.bx = 0xfa00, .si = 0xff},
                            .expect = Registers{.bx = 0xfa3a, .si = 0xff, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x1d},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.bx = 0xfa00, .di = 0x84},
                            .expect = Registers{.bx = 0xfa3a, .di = 0x84, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x1e, 0x11, 0x20},
                            .memop  = MemoryOp{.address = 0x2011, .data = {0x3a}},
                            .init   = Registers{},
                            .expect = Registers{.bx = 0x3a, .ip = 4},
                            .cycles = 0x0e,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x1f},
                            .memop  = MemoryOp{.address = 0xab, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0x3a, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x20},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.ax = 0xfa00, .bx = 0x80, .si = 0x04},
                            .expect =
                                Registers{.ax = 0x3a00, .bx = 0x80, .si = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x21},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.ax = 0x3a00, .bx = 0x80, .di = 0x04, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x22},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.ax = 0x3a00, .si = 0x04, .bp = 0x80, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x23},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.ax = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.ax = 0x3a00, .di = 0x80, .bp = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x24},
                            .memop  = MemoryOp{.address = 0xff, .data = {0x3a}},
                            .init   = Registers{.ax = 0xfa00, .si = 0xff},
                            .expect = Registers{.ax = 0x3a00, .si = 0xff, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x25},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.ax = 0xfa00, .di = 0x84},
                            .expect = Registers{.ax = 0x3a00, .di = 0x84, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x26, 0x11, 0x20},
                            .memop  = MemoryOp{.address = 0x2011, .data = {0x3a}},
                            .init   = Registers{},
                            .expect = Registers{.ax = 0x3a00, .ip = 4},
                            .cycles = 0x0e,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x27},
                            .memop  = MemoryOp{.address = 0xab, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.ax = 0x3a00, .bx = 0xab, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x28},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .cx = 0xfa00, .si = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .cx = 0x3a00, .si = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x29},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .cx = 0x3a00, .di = 0x04, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x2a},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.cx = 0x3a00, .si = 0x04, .bp = 0x80, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x2b},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.cx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.cx = 0x3a00, .di = 0x80, .bp = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x2c},
                            .memop  = MemoryOp{.address = 0xff, .data = {0x3a}},
                            .init   = Registers{.cx = 0xfa00, .si = 0xff},
                            .expect = Registers{.cx = 0x3a00, .si = 0xff, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x2d},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.cx = 0xfa00, .di = 0x84},
                            .expect = Registers{.cx = 0x3a00, .di = 0x84, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x2e, 0x11, 0x20},
                            .memop  = MemoryOp{.address = 0x2011, .data = {0x3a}},
                            .init   = Registers{},
                            .expect = Registers{.cx = 0x3a00, .ip = 4},
                            .cycles = 0x0e,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x2f},
                            .memop  = MemoryOp{.address = 0xab, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0xab, .cx = 0x3a00, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x30},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .dx = 0xfa00, .si = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .dx = 0x3a00, .si = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x31},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .dx = 0x3a00, .di = 0x04, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x32},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.dx = 0x3a00, .si = 0x04, .bp = 0x80, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x33},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.dx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.dx = 0x3a00, .di = 0x80, .bp = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x34},
                            .memop  = MemoryOp{.address = 0xff, .data = {0x3a}},
                            .init   = Registers{.dx = 0xfa00, .si = 0xff},
                            .expect = Registers{.dx = 0x3a00, .si = 0xff, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x35},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.dx = 0xfa00, .di = 0x84},
                            .expect = Registers{.dx = 0x3a00, .di = 0x84, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x36, 0x11, 0x20},
                            .memop  = MemoryOp{.address = 0x2011, .data = {0x3a}},
                            .init   = Registers{},
                            .expect = Registers{.dx = 0x3a00, .ip = 4},
                            .cycles = 0x0e,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x37},
                            .memop  = MemoryOp{.address = 0xab, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0xab, .dx = 0x3a00, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x38},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.bx = 0x80, .si = 0x04},
                            .expect = Registers{.bx = 0x3a80, .si = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x39},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.bx = 0x80, .di = 0x04},
                            .expect = Registers{.bx = 0x3a80, .di = 0x04, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x3a},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.bx = 0x3a00, .si = 0x04, .bp = 0x80, .ip = 2},
                            .cycles = 0x10,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x3b},
                            .memop = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init  = Registers{.bx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.bx = 0x3a00, .di = 0x80, .bp = 0x04, .ip = 2},
                            .cycles = 0x0f,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x3c},
                            .memop  = MemoryOp{.address = 0xff, .data = {0x3a}},
                            .init   = Registers{.bx = 0xfa00, .si = 0xff},
                            .expect = Registers{.bx = 0x3a00, .si = 0xff, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x3d},
                            .memop  = MemoryOp{.address = 0x84, .data = {0x3a}},
                            .init   = Registers{.bx = 0xfa00, .di = 0x84},
                            .expect = Registers{.bx = 0x3a00, .di = 0x84, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x3e, 0x11, 0x20},
                            .memop  = MemoryOp{.address = 0x2011, .data = {0x3a}},
                            .init   = Registers{},
                            .expect = Registers{.bx = 0x3a00, .ip = 4},
                            .cycles = 0x0e,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x3f},
                            .memop  = MemoryOp{.address = 0xab, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0x3aab, .ip = 2},
                            .cycles = 0x0d,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x40, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.ax = 0xfa00, .bx = 0x80, .si = 0x04},
                            .expect =
                                Registers{.ax = 0xfa3a, .bx = 0x80, .si = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x41, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.ax = 0x3a, .bx = 0x80, .di = 0x04, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x42, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.ax = 0x3a, .si = 0x04, .bp = 0x80, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x43, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.ax = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.ax = 0xfa3a, .di = 0x80, .bp = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x44, 0x01},
                            .memop  = MemoryOp{.address = 0x100, .data = {0x3a}},
                            .init   = Registers{.ax = 0xfa00, .si = 0xff},
                            .expect = Registers{.ax = 0xfa3a, .si = 0xff, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x45, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.ax = 0xfa00, .di = 0x84},
                            .expect = Registers{.ax = 0xfa3a, .di = 0x84, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x46, 0x20},
                            .memop  = MemoryOp{.address = 0x60, .data = {0x3a}},
                            .init   = Registers{.bp = 0x40},
                            .expect = Registers{.ax = 0x3a, .bp = 0x40, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x47, 0x20},
                            .memop  = MemoryOp{.address = 0xcb, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.ax = 0x3a, .bx = 0xab, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x48, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .cx = 0xfa00, .si = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .cx = 0xfa3a, .si = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x49, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .cx = 0x3a, .di = 0x04, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x4a, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.cx = 0x3a, .si = 0x04, .bp = 0x80, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x4b, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.cx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.cx = 0xfa3a, .di = 0x80, .bp = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x4c, 0x01},
                            .memop  = MemoryOp{.address = 0x100, .data = {0x3a}},
                            .init   = Registers{.cx = 0xfa00, .si = 0xff},
                            .expect = Registers{.cx = 0xfa3a, .si = 0xff, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x4d, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.cx = 0xfa00, .di = 0x84},
                            .expect = Registers{.cx = 0xfa3a, .di = 0x84, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x4e, 0x89},
                            .memop  = MemoryOp{.address = 0x99, .data = {0x3a}},
                            .init   = Registers{.bp = 0x10},
                            .expect = Registers{.cx = 0x3a, .bp = 0x10, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x4f, 0x01},
                            .memop  = MemoryOp{.address = 0xac, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0xab, .cx = 0x3a, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x50, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .dx = 0xfa00, .si = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .dx = 0xfa3a, .si = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x51, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .dx = 0x3a, .di = 0x04, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x52, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.dx = 0x3a, .si = 0x04, .bp = 0x80, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x53, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.dx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.dx = 0xfa3a, .di = 0x80, .bp = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x54, 0x01},
                            .memop  = MemoryOp{.address = 0x100, .data = {0x3a}},
                            .init   = Registers{.dx = 0xfa00, .si = 0xff},
                            .expect = Registers{.dx = 0xfa3a, .si = 0xff, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x55, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.dx = 0xfa00, .di = 0x84},
                            .expect = Registers{.dx = 0xfa3a, .di = 0x84, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x56, 0x40},
                            .memop  = MemoryOp{.address = 0x60, .data = {0x3a}},
                            .init   = Registers{.bp = 0x20},
                            .expect = Registers{.dx = 0x3a, .bp = 0x20, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x57, 0x1},
                            .memop  = MemoryOp{.address = 0xac, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0xab, .dx = 0x3a, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x58, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.bx = 0x80, .si = 0x04},
                            .expect = Registers{.bx = 0x3a, .si = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x59, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.bx = 0x80, .di = 0x04},
                            .expect = Registers{.bx = 0x3a, .di = 0x04, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x5a, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.bx = 0x3a, .si = 0x04, .bp = 0x80, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x5b, 0x4},
                            .memop = MemoryOp{.address = 0x88, .data = {0x3a}},
                            .init  = Registers{.bx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.bx = 0xfa3a, .di = 0x80, .bp = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x5c, 0x1},
                            .memop  = MemoryOp{.address = 0x100, .data = {0x3a}},
                            .init   = Registers{.bx = 0xfa00, .si = 0xff},
                            .expect = Registers{.bx = 0xfa3a, .si = 0xff, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x5d, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.bx = 0xfa00, .di = 0x84},
                            .expect = Registers{.bx = 0xfa3a, .di = 0x84, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x5e, 0x25},
                            .memop  = MemoryOp{.address = 0x55, .data = {0x3a}},
                            .init   = Registers{.bp = 0x30},
                            .expect = Registers{.bx = 0x3a, .bp = 0x30, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x5f, 0x01},
                            .memop  = MemoryOp{.address = 0xac, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0x3a, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x60, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.ax = 0xfa00, .bx = 0x80, .si = 0x04},
                            .expect =
                                Registers{.ax = 0x3a00, .bx = 0x80, .si = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x61, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.ax = 0x3a00, .bx = 0x80, .di = 0x04, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x62, 0x1},
                            .memop = MemoryOp{.address = 0x85, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.ax = 0x3a00, .si = 0x04, .bp = 0x80, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x63, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.ax = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.ax = 0x3a00, .di = 0x80, .bp = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x64, 0x1},
                            .memop  = MemoryOp{.address = 0x100, .data = {0x3a}},
                            .init   = Registers{.ax = 0xfa00, .si = 0xff},
                            .expect = Registers{.ax = 0x3a00, .si = 0xff, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x65, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.ax = 0xfa00, .di = 0x84},
                            .expect = Registers{.ax = 0x3a00, .di = 0x84, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x66, 0x11},
                            .memop  = MemoryOp{.address = 0x41, .data = {0x3a}},
                            .init   = Registers{.bp = 0x30},
                            .expect = Registers{.ax = 0x3a00, .bp = 0x30, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x67, 0x01},
                            .memop  = MemoryOp{.address = 0xac, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.ax = 0x3a00, .bx = 0xab, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x68, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .cx = 0xfa00, .si = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .cx = 0x3a00, .si = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x69, 0x01},
                            .memop = MemoryOp{.address = 0x85, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .cx = 0x3a00, .di = 0x04, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x6a, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.cx = 0x3a00, .si = 0x04, .bp = 0x80, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x6b, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.cx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.cx = 0x3a00, .di = 0x80, .bp = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x6c, 0x01},
                            .memop  = MemoryOp{.address = 0x100, .data = {0x3a}},
                            .init   = Registers{.cx = 0xfa00, .si = 0xff},
                            .expect = Registers{.cx = 0x3a00, .si = 0xff, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x6d, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.cx = 0xfa00, .di = 0x84},
                            .expect = Registers{.cx = 0x3a00, .di = 0x84, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x6e, 0x20},
                            .memop  = MemoryOp{.address = 0x30, .data = {0x3a}},
                            .init   = Registers{.bp = 0x10},
                            .expect = Registers{.cx = 0x3a00, .bp = 0x10, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x6f, 0x1},
                            .memop  = MemoryOp{.address = 0xac, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0xab, .cx = 0x3a00, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x70, 0x1},
                            .memop = MemoryOp{.address = 0x85, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .dx = 0xfa00, .si = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .dx = 0x3a00, .si = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x71, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.bx = 0x80, .di = 0x04},
                            .expect =
                                Registers{.bx = 0x80, .dx = 0x3a00, .di = 0x04, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x72, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.dx = 0x3a00, .si = 0x04, .bp = 0x80, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x73, 0x20},
                            .memop = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init  = Registers{.dx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.dx = 0x3a00, .di = 0x80, .bp = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x74, 0x01},
                            .memop  = MemoryOp{.address = 0x100, .data = {0x3a}},
                            .init   = Registers{.dx = 0xfa00, .si = 0xff},
                            .expect = Registers{.dx = 0x3a00, .si = 0xff, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x75, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.dx = 0xfa00, .di = 0x84},
                            .expect = Registers{.dx = 0x3a00, .di = 0x84, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x76, 0x20},
                            .memop  = MemoryOp{.address = 0x40, .data = {0x3a}},
                            .init   = Registers{.bp = 0x20},
                            .expect = Registers{.dx = 0x3a00, .bp = 0x20, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x77, 0x1},
                            .memop  = MemoryOp{.address = 0xac, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0xab, .dx = 0x3a00, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x78, 0x10},
                            .memop  = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init   = Registers{.bx = 0x80, .si = 0x04},
                            .expect = Registers{.bx = 0x3a80, .si = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x79, 0x20},
                            .memop  = MemoryOp{.address = 0xa4, .data = {0x3a}},
                            .init   = Registers{.bx = 0x80, .di = 0x04},
                            .expect = Registers{.bx = 0x3a80, .di = 0x04, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x7a, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.si = 0x04, .bp = 0x80},
                            .expect =
                                Registers{.bx = 0x3a00, .si = 0x04, .bp = 0x80, .ip = 3},
                            .cycles = 0x14,
                        }),
                    add(
                        {
                            .cmd   = {0x8a, 0x7b, 0x10},
                            .memop = MemoryOp{.address = 0x94, .data = {0x3a}},
                            .init  = Registers{.bx = 0xfa00, .di = 0x80, .bp = 0x04},
                            .expect =
                                Registers{.bx = 0x3a00, .di = 0x80, .bp = 0x04, .ip = 3},
                            .cycles = 0x13,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x7c, 0x01},
                            .memop  = MemoryOp{.address = 0x100, .data = {0x3a}},
                            .init   = Registers{.bx = 0xfa00, .si = 0xff},
                            .expect = Registers{.bx = 0x3a00, .si = 0xff, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x7d, 0x01},
                            .memop  = MemoryOp{.address = 0x85, .data = {0x3a}},
                            .init   = Registers{.bx = 0xfa00, .di = 0x84},
                            .expect = Registers{.bx = 0x3a00, .di = 0x84, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x7e, 0x20},
                            .memop  = MemoryOp{.address = 0x65, .data = {0x3a}},
                            .init   = Registers{.bp = 0x45},
                            .expect = Registers{.bx = 0x3a00, .bp = 0x45, .ip = 3},
                            .cycles = 0x11,
                        }),
                    add(
                        {
                            .cmd    = {0x8a, 0x7f, 0x1},
                            .memop  = MemoryOp{.address = 0xac, .data = {0x3a}},
                            .init   = Registers{.bx = 0xab},
                            .expect = Registers{.bx = 0x3aab, .ip = 3},
                            .cycles = 0x11,
                        }),

                },
        });
}

INSTANTIATE_TEST_CASE_P(MovTests_1, MovFromMemToRegTests, get_mov_values(),
                        generate_test_case_name);

} // namespace msemu::cpu8086
