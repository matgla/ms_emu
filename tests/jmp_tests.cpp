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
namespace
{
struct MemoryOp
{
    uint32_t address;
    std::vector<uint8_t> data;
};

struct TestCase
{
    TestCase(const Registers& regs, const Registers& expect, const std::vector<uint8_t>& command_data,
             const uint8_t instruction_cost, const MemoryOp& memop = MemoryOp{},
             const std::source_location loc = std::source_location::current())
        : regs_init(regs)
        , expect(expect)
        , data(command_data)
        , cost(instruction_cost)
        , memory_op(memop)
        , location(loc)
    {
    }


    Registers regs_init;
    Registers expect;
    std::vector<uint8_t> data;
    uint8_t cost;
    MemoryOp memory_op;
    std::source_location location;
};

std::string print_test_case_info(const TestCase& data, std::string error)
{
    std::stringstream str;
    str << "TC location: " << data.location.file_name() << ":" << data.location.line() << std::endl;
    if (error.size())
    {
        str << "ERR: " << error << std::endl;
    }
    return str.str();
}

struct JmpTestsParams
{
    uint8_t cmd;
    std::vector<TestCase> cases;
    std::string name;
};
} // namespace

class JmpTests : public ParametrizedTestBase<JmpTestsParams>
{
};

TEST_P(JmpTests, ProcessCmd)
{
    const JmpTestsParams& param = GetParam();
    for (const auto& test : param.cases)
    {
        std::vector<uint8_t> cmd;
        cmd.push_back(param.cmd);
        std::copy(test.data.begin(), test.data.end(), std::back_inserter(cmd));

        bus_.clear();
        bus_.write(test.regs_init.ip, cmd);

        if (!test.memory_op.data.empty())
        {
            bus_.write(test.memory_op.address, test.memory_op.data);
        }

        sut_.set_registers(test.regs_init);
        sut_.step();
        EXPECT_EQ(sut_.get_registers(), test.expect) << print_test_case_info(test, sut_.get_error());
        EXPECT_EQ(sut_.last_instruction_cost(), test.cost) << print_test_case_info(test, sut_.get_error());
    }
}

const std::vector<TestCase> modrm_jump_short_mod0{
    TestCase{
        Registers{.bx = 0x1020, .si = 0x2010},
        Registers{.bx = 0x1020, .si = 0x2010, .ip = 0xbaac},
        {},
        25,
        MemoryOp{.address = 0x3030, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.bx = 0x1020, .di = 0x2010},
        Registers{.bx = 0x1020, .di = 0x2010, .ip = 0xbaac},
        {},
        26,
        MemoryOp{.address = 0x3030, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.si = 0x2010, .bp = 0x1020},
        Registers{.si = 0x2010, .bp = 0x1020, .ip = 0xbaac},
        {},
        26,
        MemoryOp{.address = 0x3030, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.di = 0x2010, .bp = 0x1020},
        Registers{.di = 0x2010, .bp = 0x1020, .ip = 0xbaac},
        {},
        25,
        MemoryOp{.address = 0x3030, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.si = 0x2010},
        Registers{.si = 0x2010, .ip = 0xbaac},
        {},
        23,
        MemoryOp{.address = 0x2010, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.di = 0x2010},
        Registers{.di = 0x2010, .ip = 0xbaac},
        {},
        23,
        MemoryOp{.address = 0x2010, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{},
        Registers{.ip = 0xbaac},
        {0x30, 0x20},
        24,
        MemoryOp{.address = 0x2030, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.bx = 0x1020},
        Registers{.bx = 0x1020, .ip = 0xbaac},
        {},
        23,
        MemoryOp{.address = 0x1020, .data = {0xac, 0xba}},
    },
};

const std::vector<TestCase> modrm_jump_short_mod1{
    TestCase{
        Registers{.bx = 0x1020, .si = 0x2010},
        Registers{.bx = 0x1020, .si = 0x2010, .ip = 0xbaac},
        {0x05},
        29,
        MemoryOp{.address = 0x3035, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.bx = 0x1020, .di = 0x2010},
        Registers{.bx = 0x1020, .di = 0x2010, .ip = 0xbaac},
        {0x05},
        30,
        MemoryOp{.address = 0x3035, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.si = 0x2010, .bp = 0x1020},
        Registers{.si = 0x2010, .bp = 0x1020, .ip = 0xbaac},
        {0x05},
        30,
        MemoryOp{.address = 0x3035, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.di = 0x2010, .bp = 0x1020},
        Registers{.di = 0x2010, .bp = 0x1020, .ip = 0xbaac},
        {0x05},
        29,
        MemoryOp{.address = 0x3035, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.si = 0x2010},
        Registers{.si = 0x2010, .ip = 0xbaac},
        {0x05},
        27,
        MemoryOp{.address = 0x2015, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.di = 0x2010},
        Registers{.di = 0x2010, .ip = 0xbaac},
        {0x05},
        27,
        MemoryOp{.address = 0x2015, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.bp = 0x2030},
        Registers{.bp = 0x2030, .ip = 0xbaac},
        {0x20},
        27,
        MemoryOp{.address = 0x2050, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.bx = 0x1020},
        Registers{.bx = 0x1020, .ip = 0xbaac},
        {0x05},
        27,
        MemoryOp{.address = 0x1025, .data = {0xac, 0xba}},
    },
};


const std::vector<TestCase> modrm_jump_short_mod2{
    TestCase{
        Registers{.bx = 0x1020, .si = 0x2010},
        Registers{.bx = 0x1020, .si = 0x2010, .ip = 0xbaac},
        {0x05, 0x10},
        29,
        MemoryOp{.address = 0x4035, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.bx = 0x1020, .di = 0x2010},
        Registers{.bx = 0x1020, .di = 0x2010, .ip = 0xbaac},
        {0x05, 0x10},
        30,
        MemoryOp{.address = 0x4035, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.si = 0x2010, .bp = 0x1020},
        Registers{.si = 0x2010, .bp = 0x1020, .ip = 0xbaac},
        {0x05, 0x10},
        30,
        MemoryOp{.address = 0x4035, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.di = 0x2010, .bp = 0x1020},
        Registers{.di = 0x2010, .bp = 0x1020, .ip = 0xbaac},
        {0x05, 0x10},
        29,
        MemoryOp{.address = 0x4035, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.si = 0x2010},
        Registers{.si = 0x2010, .ip = 0xbaac},
        {0x05, 0x10},
        27,
        MemoryOp{.address = 0x3015, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.di = 0x2010},
        Registers{.di = 0x2010, .ip = 0xbaac},
        {0x05, 0x10},
        27,
        MemoryOp{.address = 0x3015, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.bp = 0x2030},
        Registers{.bp = 0x2030, .ip = 0xbaac},
        {0x20, 0x10},
        27,
        MemoryOp{.address = 0x3050, .data = {0xac, 0xba}},
    },
    TestCase{
        Registers{.bx = 0x1020},
        Registers{.bx = 0x1020, .ip = 0xbaac},
        {0x05, 0x10},
        27,
        MemoryOp{.address = 0x2025, .data = {0xac, 0xba}},
    },
};

const std::vector<TestCase> modrm_jump_short_mod3{
    TestCase{
        Registers{.ax = 0x1020},
        Registers{.ax = 0x1020, .ip = 0x1020},
        {},
        11,
    },
    TestCase{
        Registers{.cx = 0x1020},
        Registers{.cx = 0x1020, .ip = 0x1020},
        {},
        11,
    },
    TestCase{
        Registers{.dx = 0x2010},
        Registers{.dx = 0x2010, .ip = 0x2010},
        {},
        11,
    },
    TestCase{
        Registers{.bx = 0x2010},
        Registers{.bx = 0x2010, .ip = 0x2010},
        {},
        11,
    },
    TestCase{
        Registers{.sp = 0x2010},
        Registers{.sp = 0x2010, .ip = 0x2010},
        {},
        11,
    },
    TestCase{
        Registers{.bp = 0x2010},
        Registers{.bp = 0x2010, .ip = 0x2010},
        {},
        11,
    },
    TestCase{
        Registers{.si = 0x2030},
        Registers{.si = 0x2030, .ip = 0x2030},
        {},
        11,
    },
    TestCase{
        Registers{.di = 0x1020},
        Registers{.di = 0x1020, .ip = 0x1020},
        {},
        11,
    },
};

const std::vector<TestCase> modrm_jump_far_mod0{
    TestCase{
        Registers{.bx = 0x1020, .si = 0x2010},
        Registers{.bx = 0x1020, .si = 0x2010, .ip = 0xbaac, .cs = 0x2010},
        {},
        31,
        MemoryOp{.address = 0x3030, .data = {0xac, 0xba, 0x10, 0x20}},
    },
    TestCase{
        Registers{.bx = 0x1020, .di = 0x2010},
        Registers{.bx = 0x1020, .di = 0x2010, .ip = 0xbaac, .cs = 0x1020},
        {},
        32,
        MemoryOp{.address = 0x3030, .data = {0xac, 0xba, 0x20, 0x10}},
    },
    TestCase{
        Registers{.si = 0x2010, .bp = 0x1020},
        Registers{.si = 0x2010, .bp = 0x1020, .ip = 0xbaac, .cs = 0x1020},
        {},
        32,
        MemoryOp{.address = 0x3030, .data = {0xac, 0xba, 0x20, 0x10}},
    },
    TestCase{
        Registers{.di = 0x2010, .bp = 0x1020},
        Registers{.di = 0x2010, .bp = 0x1020, .ip = 0xbaac, .cs = 0x1020},
        {},
        31,
        MemoryOp{.address = 0x3030, .data = {0xac, 0xba, 0x20, 0x10}},
    },
    TestCase{
        Registers{.si = 0x2010},
        Registers{.si = 0x2010, .ip = 0xbaac, .cs = 0x1020},
        {},
        29,
        MemoryOp{.address = 0x2010, .data = {0xac, 0xba, 0x20, 0x10}},
    },
    TestCase{
        Registers{.di = 0x2010},
        Registers{.di = 0x2010, .ip = 0xbaac, .cs = 0x1020},
        {},
        29,
        MemoryOp{.address = 0x2010, .data = {0xac, 0xba, 0x20, 0x10}},
    },
    TestCase{
        Registers{},
        Registers{.ip = 0xbaac, .cs = 0x1020},
        {0x30, 0x20},
        30,
        MemoryOp{.address = 0x2030, .data = {0xac, 0xba, 0x20, 0x10}},
    },
    TestCase{
        Registers{.bx = 0x1020},
        Registers{.bx = 0x1020, .ip = 0xbaac, .cs = 0x1020},
        {},
        29,
        MemoryOp{.address = 0x1020, .data = {0xac, 0xba, 0x20, 0x10}},
    },
};

const std::vector<TestCase> modrm_jump_far_mod1{
    TestCase{
        Registers{.bx = 0x1020, .si = 0x2010},
        Registers{.bx = 0x1020, .si = 0x2010, .ip = 0xbaac, .cs = 0x1234},
        {0x05},
        35,
        MemoryOp{.address = 0x3035, .data = {0xac, 0xba, 0x34, 0x12}},
    },
    TestCase{
        Registers{.bx = 0x1020, .di = 0x2010},
        Registers{.bx = 0x1020, .di = 0x2010, .ip = 0xbaac, .cs = 0x1234},
        {0x05},
        36,
        MemoryOp{.address = 0x3035, .data = {0xac, 0xba, 0x34, 0x12}},
    },
    TestCase{
        Registers{.si = 0x2010, .bp = 0x1020},
        Registers{.si = 0x2010, .bp = 0x1020, .ip = 0xbaac, .cs = 0x1234},
        {0x05},
        36,
        MemoryOp{.address = 0x3035, .data = {0xac, 0xba, 0x34, 0x12}},
    },
    TestCase{
        Registers{.di = 0x2010, .bp = 0x1020},
        Registers{.di = 0x2010, .bp = 0x1020, .ip = 0xbaac, .cs = 0x1234},
        {0x05},
        35,
        MemoryOp{.address = 0x3035, .data = {0xac, 0xba, 0x34, 0x12}},
    },
    TestCase{
        Registers{.si = 0x2010},
        Registers{.si = 0x2010, .ip = 0xbaac, .cs = 0x1234},
        {0x05},
        33,
        MemoryOp{.address = 0x2015, .data = {0xac, 0xba, 0x34, 0x12}},
    },
    TestCase{
        Registers{.di = 0x2010},
        Registers{.di = 0x2010, .ip = 0xbaac, .cs = 0x1234},
        {0x05},
        33,
        MemoryOp{.address = 0x2015, .data = {0xac, 0xba, 0x34, 0x12}},
    },
    TestCase{
        Registers{.bp = 0x2030},
        Registers{.bp = 0x2030, .ip = 0xbaac, .cs = 0x1234},
        {0x20},
        33,
        MemoryOp{.address = 0x2050, .data = {0xac, 0xba, 0x34, 0x12}},
    },
    TestCase{
        Registers{.bx = 0x1020},
        Registers{.bx = 0x1020, .ip = 0xbaac, .cs = 0x1234},
        {0x05},
        33,
        MemoryOp{.address = 0x1025, .data = {0xac, 0xba, 0x34, 0x12}},
    },
};


const std::vector<TestCase> modrm_jump_far_mod2{
    TestCase{
        Registers{.bx = 0x1020, .si = 0x2010},
        Registers{.bx = 0x1020, .si = 0x2010, .ip = 0xbaac, .cs = 0xface},
        {0x05, 0x10},
        35,
        MemoryOp{.address = 0x4035, .data = {0xac, 0xba, 0xce, 0xfa}},
    },
    TestCase{
        Registers{.bx = 0x1020, .di = 0x2010},
        Registers{.bx = 0x1020, .di = 0x2010, .ip = 0xbaac, .cs = 0xface},
        {0x05, 0x10},
        36,
        MemoryOp{.address = 0x4035, .data = {0xac, 0xba, 0xce, 0xfa}},
    },
    TestCase{
        Registers{.si = 0x2010, .bp = 0x1020},
        Registers{.si = 0x2010, .bp = 0x1020, .ip = 0xbaac, .cs = 0xface},
        {0x05, 0x10},
        36,
        MemoryOp{.address = 0x4035, .data = {0xac, 0xba, 0xce, 0xfa}},
    },
    TestCase{
        Registers{.di = 0x2010, .bp = 0x1020},
        Registers{.di = 0x2010, .bp = 0x1020, .ip = 0xbaac, .cs = 0xface},
        {0x05, 0x10},
        35,
        MemoryOp{.address = 0x4035, .data = {0xac, 0xba, 0xce, 0xfa}},
    },
    TestCase{
        Registers{.si = 0x2010},
        Registers{.si = 0x2010, .ip = 0xbaac, .cs = 0xface},
        {0x05, 0x10},
        33,
        MemoryOp{.address = 0x3015, .data = {0xac, 0xba, 0xce, 0xfa}},
    },
    TestCase{
        Registers{.di = 0x2010},
        Registers{.di = 0x2010, .ip = 0xbaac, .cs = 0xface},
        {0x05, 0x10},
        33,
        MemoryOp{.address = 0x3015, .data = {0xac, 0xba, 0xce, 0xfa}},
    },
    TestCase{
        Registers{.bp = 0x2030},
        Registers{.bp = 0x2030, .ip = 0xbaac, .cs = 0xface},
        {0x20, 0x10},
        33,
        MemoryOp{.address = 0x3050, .data = {0xac, 0xba, 0xce, 0xfa}},
    },
    TestCase{
        Registers{.bx = 0x1020},
        Registers{.bx = 0x1020, .ip = 0xbaac, .cs = 0xface},
        {0x05, 0x10},
        33,
        MemoryOp{.address = 0x2025, .data = {0xac, 0xba, 0xce, 0xfa}},
    },
};

const std::vector<std::vector<TestCase>> modrm_jump_short_data{
    modrm_jump_short_mod0,
    modrm_jump_short_mod1,
    modrm_jump_short_mod2,
    modrm_jump_short_mod3,
};

const std::vector<std::vector<TestCase>> modrm_jump_far_data{
    modrm_jump_far_mod0,
    modrm_jump_far_mod1,
    modrm_jump_far_mod2,
};

JmpTestsParams generate_data_for_modrm_jump(ModRM limit, uint8_t cmd, const auto& data, uint8_t second_opcode,
                                            const std::string& name)
{
    limit.mod++;
    limit.rm++;
    limit.reg++;

    JmpTestsParams params;
    params.cmd   = cmd;
    params.cases = {};
    params.name  = name;
    ModRM mod    = 0;
    do
    {
        mod.reg             = 0;
        const auto& rm_data = data[mod.mod];
        do
        {
            mod.rm = 0;
            do
            {
                params.cases.push_back(rm_data[mod.rm]);
                ModRM mod_byte = mod;
                mod_byte.reg   = second_opcode & 0x7;
                params.cases.back().data.insert(params.cases.back().data.begin(), mod_byte);
                ++mod.rm;
            } while (mod.rm != limit.rm);
            ++mod.reg;
        } while (mod.reg != limit.reg);
        ++mod.mod;
    } while (mod.mod != limit.mod);

    return params;
}

auto get_jmp_test_parameters()
{
    return ::testing::Values(
        JmpTestsParams{
            // short jumps
            .cmd = 0xeb,
            .cases =
                {
                    TestCase{{.ip = 0x00}, {.ip = 0x06}, {0x04}, 15},
                    TestCase{{.ip = 0x04}, {.ip = 0x00}, {0xfa}, 15},
                    TestCase{{.ip = 0x09}, {.ip = 0x0d}, {0x02}, 15},
                    TestCase{{.ip = 0x11}, {.ip = 0x09}, {0xf6}, 15},
                },
            .name = "0xeb",
        },
        JmpTestsParams{
            // near jumps
            .cmd = 0xe9,
            .cases =
                {
                    TestCase{{.ip = 0x09}, {.ip = 0x00}, {0xf4, 0xff}, 15},
                    TestCase{{.ip = 0x0c}, {.ip = 0xffff}, {0xf0, 0xff}, 15},
                    TestCase{{.ip = 0x0f}, {.ip = 0x5000}, {0xee, 0x4f}, 15},
                    TestCase{{.ip = 0x12}, {.ip = 0x1234}, {0x1f, 0x12}, 15},
                },
            .name = "0xe9",
        },
        JmpTestsParams{
            // far jumps
            .cmd = 0xea,
            .cases =
                {
                    TestCase{{.ip = 0x19}, {.ip = 0x1e, .cs = 0x08}, {0x1e, 0x00, 0x08, 0x00}, 15},
                    TestCase{{.ip = 0x1e}, {.ip = 0x1234, .cs = 0x15}, {0x34, 0x12, 0x15, 0x00}, 15},
                    TestCase{{.ip = 0x23}, {.ip = 0xffff, .cs = 0xffff}, {0xff, 0xff, 0xff, 0xff}, 15},
                },
            .name = "0xea",
        },
        generate_data_for_modrm_jump(ModRM(3, 0, 7), 0xff, modrm_jump_short_data, 0x04, "0xff_4"),
        generate_data_for_modrm_jump(ModRM(2, 0, 7), 0xff, modrm_jump_far_data, 0x05, "0xff_5"));
}

inline void PrintTo(const JmpTestsParams& reg, ::std::ostream* os)
{
    *os << "name: " << get_name(reg.cmd) << std::endl;
}

INSTANTIATE_TEST_CASE_P(OpcodesJmp, JmpTests, get_jmp_test_parameters(),
                        generate_test_case_name<JmpTestsParams>);

} // namespace msemu::cpu8086
