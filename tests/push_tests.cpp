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

#include <optional>
#include <source_location>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "16_bit_modrm.hpp"

#include "test_base.hpp"

namespace msemu::cpu8086
{

struct MemoryOp
{
    uint32_t address;
    std::vector<uint8_t> data;
};

struct TestCase
{
    TestCase(const Registers& regs, const Registers& expect, const MemoryOp& expect_memory,
             const uint8_t instruction_cost, const std::source_location& test_location,
             const std::source_location loc = std::source_location::current())
        : regs_init(regs)
        , expect(expect)
        , expect_memory(expect_memory)
        , cost(instruction_cost)
        , location(test_location)
        , expect_location(loc)
    {
    }


    Registers regs_init;
    Registers expect;
    MemoryOp expect_memory;
    uint8_t cost;
    MemoryOp memory_op;
    std::source_location location;
    std::source_location expect_location;
};

std::string print_test_case_info(const TestCase& data, std::string error)
{
    std::stringstream str;
    str << "TC location    : " << data.location.file_name() << ":" << data.location.line() << std::endl;
    str << "Expect location: " << data.expect_location.file_name() << ":" << data.expect_location.line()
        << std::endl;
    if (error.size())
    {
        str << "ERR: " << error << std::endl;
    }
    return str.str();
}

struct PushTestsParams
{
    uint8_t cmd;
    std::vector<TestCase> cases;
    std::string name;
};

class PushTests : public TestBase<PushTestsParams>
{
};

TEST_P(PushTests, ProcessCmd)
{
    const PushTestsParams& param = GetParam();
    bus_.clear();
    for (const auto& test : param.cases)
    {
        std::vector<uint8_t> cmd;
        cmd.push_back(param.cmd);

        const uint32_t address = static_cast<uint32_t>((test.regs_init.cs << 4) + test.regs_init.ip);
        bus_.write(address, cmd);

        if (!test.memory_op.data.empty())
        {
            bus_.write(test.memory_op.address, test.memory_op.data);
        }

        sut_.set_registers(test.regs_init);
        sut_.step();
        EXPECT_EQ(sut_.get_registers(), test.expect) << print_test_case_info(test, sut_.get_error());
        EXPECT_EQ(sut_.last_instruction_cost(), test.cost) << print_test_case_info(test, sut_.get_error());
        std::vector<uint8_t> from_memory(test.expect_memory.data.size());
        bus_.read(test.expect_memory.address, from_memory);
        EXPECT_THAT(from_memory, ::testing::ElementsAreArray(test.expect_memory.data))
            << print_test_case_info(test, sut_.get_error());
    }
}

PushTestsParams generate_push_data(uint8_t cmd, uint16_t Registers::*reg, uint8_t cost,
                                   const std::source_location loc = std::source_location::current())
{
    Registers init_1{.sp = 0xfff0, .ip = 0x00};
    Registers expect_1{.sp = 0xfff0 - 2, .ip = 0x01};
    init_1.*reg   = 0xabcd;
    expect_1.*reg = 0xabcd;

    Registers init_2   = expect_1;
    init_2.*reg        = 0x1223;
    Registers expect_2 = {.sp = 0xfff0 - 4, .ip = 0x02};
    expect_2.*reg      = 0x1223;

    Registers init_3   = {.sp = 0x0ff0, .ip = 0x02, .ss = 0x0400};
    init_3.*reg        = 0x1223;
    Registers expect_3 = {.sp = 0x0ff0 - 2, .ip = 0x03, .ss = 0x0400};
    expect_3.*reg      = 0x1223;

    Registers init_4   = expect_3;
    init_4.*reg        = 0x1122;
    Registers expect_4 = {.sp = 0x0ff0 - 4, .ip = 0x04, .ss = 0x0400};
    expect_4.*reg      = 0x1122;
    return PushTestsParams{
        // push ax
        .cmd = cmd,
        .cases =
            {
                TestCase{init_1, expect_1, {.address = 0xfff0 - 2, .data = {0xcd, 0xab}}, cost, loc},
                TestCase{
                    init_2, expect_2, {.address = 0xfff0 - 4, .data = {0x23, 0x12, 0xcd, 0xab}}, cost, loc},
                TestCase{init_3, expect_3, {.address = 0x4ff0 - 2, .data = {0x23, 0x12}}, cost, loc},
                TestCase{
                    init_4, expect_4, {.address = 0x4ff0 - 4, .data = {0x22, 0x11, 0x23, 0x12}}, cost, loc},
            },
        .name = get_name(cmd),
    };
}

PushTestsParams generate_push_data_for_cs(uint8_t cmd,
                                          const std::source_location loc = std::source_location::current())
{
    Registers init_1{.sp = 0xfff0, .ip = 0x00};
    Registers expect_1{.sp = 0xfff0 - 2, .ip = 0x01};
    init_1.cs   = 0x0010;
    expect_1.cs = 0x0010;

    Registers init_2   = expect_1;
    init_2.cs          = 0x0020;
    Registers expect_2 = {.sp = 0xfff0 - 4, .ip = 0x02};
    expect_2.cs        = 0x0020;

    Registers init_3   = {.sp = 0x0ff0, .ip = 0x02, .ss = 0x0400};
    init_3.cs          = 0x0030;
    Registers expect_3 = {.sp = 0x0ff0 - 2, .ip = 0x03, .ss = 0x0400};
    expect_3.cs        = 0x0030;

    Registers init_4   = expect_3;
    init_4.cs          = 0x0040;
    Registers expect_4 = {.sp = 0x0ff0 - 4, .ip = 0x04, .ss = 0x0400};
    expect_4.cs        = 0x0040;
    return PushTestsParams{
        // push ax
        .cmd = cmd,
        .cases =
            {
                TestCase{init_1, expect_1, {.address = 0xfff0 - 2, .data = {0x10, 0x00}}, 14, loc},
                TestCase{
                    init_2, expect_2, {.address = 0xfff0 - 4, .data = {0x20, 0x00, 0x10, 0x00}}, 14, loc},
                TestCase{init_3, expect_3, {.address = 0x4ff0 - 2, .data = {0x30, 0x00}}, 14, loc},
                TestCase{
                    init_4, expect_4, {.address = 0x4ff0 - 4, .data = {0x40, 0x00, 0x30, 0x00}}, 14, loc},
            },
        .name = get_name(cmd),
    };
}


PushTestsParams generate_push_data_for_sp(uint8_t cmd,
                                          const std::source_location loc = std::source_location::current())
{
    Registers init_1{.sp = 0x1000, .ip = 0x00};
    Registers expect_1{.sp = 0x1000 - 2, .ip = 0x01};

    Registers init_2   = expect_1;
    Registers expect_2 = {.sp = 0x1000 - 4, .ip = 0x02};

    Registers init_3   = {.sp = 0x0100, .ip = 0x02, .ss = 0x0400};
    Registers expect_3 = {.sp = 0x0100 - 2, .ip = 0x03, .ss = 0x0400};
    Registers init_4   = expect_3;
    Registers expect_4 = {.sp = 0x0100 - 4, .ip = 0x04, .ss = 0x0400};

    return PushTestsParams{
        // push ax
        .cmd = cmd,
        .cases =
            {
                TestCase{init_1, expect_1, {.address = 0x1000 - 2, .data = {0x00, 0x10}}, 15, loc},
                TestCase{
                    init_2, expect_2, {.address = 0x1000 - 4, .data = {0xfe, 0x0f, 0x00, 0x10}}, 15, loc},
                TestCase{init_3, expect_3, {.address = 0x4100 - 2, .data = {0x00, 0x01}}, 15, loc},
                TestCase{
                    init_4, expect_4, {.address = 0x4100 - 4, .data = {0xfe, 0x00, 0x00, 0x01}}, 15, loc},
            },
        .name = get_name(cmd),
    };
}

PushTestsParams generate_push_data_for_ss(uint8_t cmd,
                                          const std::source_location loc = std::source_location::current())
{
    Registers init_1{.sp = 0x0100, .ip = 0x00, .ss = 0x0200};
    Registers expect_1{.sp = 0x0100 - 2, .ip = 0x01, .ss = 0x0200};

    Registers init_2   = expect_1;
    Registers expect_2 = {.sp = 0x0100 - 4, .ip = 0x02, .ss = 0x0200};

    return PushTestsParams{
        .cmd = cmd,
        .cases =
            {
                TestCase{init_1, expect_1, {.address = 0x2100 - 2, .data = {0x00, 0x02}}, 14, loc},
                TestCase{
                    init_2, expect_2, {.address = 0x2100 - 4, .data = {0x00, 0x02, 0x00, 0x02}}, 14, loc},
            },
        .name = get_name(cmd),
    };
}


auto get_push_test_parameters()
{
    return ::testing::Values(
        generate_push_data(0x50, &Registers::ax, 15), generate_push_data(0x51, &Registers::cx, 15),
        generate_push_data(0x52, &Registers::dx, 15), generate_push_data(0x53, &Registers::bx, 15),
        generate_push_data_for_sp(0x54), generate_push_data(0x55, &Registers::bp, 15),
        generate_push_data(0x56, &Registers::si, 15), generate_push_data(0x57, &Registers::di, 15),
        generate_push_data(0x06, &Registers::es, 14), generate_push_data_for_cs(0x0e),
        generate_push_data_for_ss(0x16), generate_push_data(0x1e, &Registers::ds, 14)
        // generate_push_data(0xff, &Registers::es), generate_push_data(0x0e, &Registers::cs),


    );
}

inline void PrintTo(const PushTestsParams& reg, ::std::ostream* os)
{
    *os << "name: " << get_name(reg.cmd) << std::endl;
}

INSTANTIATE_TEST_CASE_P(OpcodesPush, PushTests, get_push_test_parameters(),
                        generate_test_case_name<PushTestsParams>);

} // namespace msemu::cpu8086
