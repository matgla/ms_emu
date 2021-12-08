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
    TestCase(const Registers& regs, const Registers& expect, const MemoryOp& expect_memory,
             const uint8_t instruction_cost, const std::source_location& test_location,
             const std::vector<uint8_t>& append_cmd = {}, const MemoryOp& memory_init = {},
             const std::optional<std::source_location> init_loc = {},
             const std::source_location loc                     = std::source_location::current())
        : regs_init(regs)
        , expect(expect)
        , expect_memory(expect_memory)
        , cost(instruction_cost)
        , location(test_location)
        , append_cmd(append_cmd)
        , memory_init(memory_init)
        , init_loc(init_loc)
        , expect_location(loc)
    {
    }


    Registers regs_init;
    Registers expect;
    MemoryOp expect_memory;
    uint8_t cost;
    MemoryOp memory_op;
    std::source_location location;
    std::vector<uint8_t> append_cmd;
    MemoryOp memory_init;
    std::optional<std::source_location> init_loc;
    std::source_location expect_location;
};

std::string print_test_case_info(const TestCase& data, std::string error)
{
    std::stringstream str;
    str << "TC location     : " << data.location.file_name() << ":" << data.location.line() << std::endl;
    str << "Expect location : " << data.expect_location.file_name() << ":" << data.expect_location.line()
        << std::endl;
    if (data.init_loc)
    {
        str << "Init location   : " << data.init_loc->file_name() << ":" << data.init_loc->line()
            << std::endl;
    }

    if (error.size())
    {
        str << "ERR: " << error << std::endl;
    }
    return str.str();
}

struct PopTestsParams
{
    std::vector<uint8_t> cmd;
    std::vector<TestCase> cases;
    std::string name;
};


PopTestsParams generate_pop_data(uint8_t cmd, uint16_t Registers::*reg, uint8_t cost,
                                 const std::source_location loc = std::source_location::current())
{
    Registers init_1{.sp = 0xfff0 - 4, .ip = 0x00};
    Registers expect_1{.sp = 0xfff0 - 2, .ip = 0x01};
    expect_1.*reg = 0xabcd;

    Registers init_2   = expect_1;
    Registers expect_2 = {.sp = 0xfff0, .ip = 0x02};
    expect_2.*reg      = 0x1223;

    Registers init_3   = {.sp = 0x0ff0 - 4, .ip = 0x02, .ss = 0x0400};
    Registers expect_3 = {.sp = 0x0ff0 - 2, .ip = 0x03, .ss = 0x0400};
    expect_3.*reg      = 0x1223;

    Registers init_4   = expect_3;
    Registers expect_4 = {.sp = 0x0ff0, .ip = 0x04, .ss = 0x0400};
    expect_4.*reg      = 0x1122;
    return PopTestsParams{
        // pop ax
        .cmd = {cmd},
        .cases =
            {
                TestCase{init_1, expect_1, {.address = 0xfff0 - 4, .data = {0xcd, 0xab}}, cost, loc},
                TestCase{
                    init_2, expect_2, {.address = 0xfff0 - 2, .data = {0x23, 0x12, 0xcd, 0xab}}, cost, loc},
                TestCase{init_3, expect_3, {.address = 0x4ff0 - 4, .data = {0x23, 0x12}}, cost, loc},
                TestCase{
                    init_4, expect_4, {.address = 0x4ff0 - 2, .data = {0x22, 0x11, 0x23, 0x12}}, cost, loc},
            },
        .name = get_name(cmd),
    };
}

PopTestsParams generate_pop_data_for_sp(uint8_t cmd,
                                        const std::source_location loc = std::source_location::current())
{
    Registers init_1{.sp = 0x1000 - 4, .ip = 0x00};
    Registers expect_1{.sp = 0xabcd + 2, .ip = 0x01};

    Registers init_2{.sp = 0x1000 - 2, .ip = 0x01};
    Registers expect_2 = {.sp = 0x1020 + 2, .ip = 0x02};

    Registers init_3   = {.sp = 0x0100 - 4, .ip = 0x02, .ss = 0x0400};
    Registers expect_3 = {.sp = 0xefcd + 2, .ip = 0x03, .ss = 0x0400};
    Registers init_4   = {.sp = 0x0100 - 2, .ip = 0x03, .ss = 0x0400};
    Registers expect_4 = {.sp = 0x3010 + 2, .ip = 0x04, .ss = 0x0400};

    return PopTestsParams{
        // pop ax
        .cmd = {cmd},
        .cases =
            {
                TestCase{init_1, expect_1, {.address = 0x1000 - 4, .data = {0xcd, 0xab}}, 12, loc},
                TestCase{init_2, expect_2, {.address = 0x1000 - 2, .data = {0x20, 0x10}}, 12, loc},
                TestCase{init_3, expect_3, {.address = 0x4100 - 4, .data = {0xcd, 0xef}}, 12, loc},
                TestCase{init_4, expect_4, {.address = 0x4100 - 2, .data = {0x10, 0x30}}, 12, loc},
            },
        .name = get_name(cmd),
    };
}

PopTestsParams generate_pop_data_for_ss(uint8_t cmd,
                                        const std::source_location loc = std::source_location::current())
{
    Registers init_1{.sp = 0x0100 - 4, .ip = 0x00, .ss = 0x0200};
    Registers expect_1{.sp = 0x0100 - 2, .ip = 0x01, .ss = 0x0200};

    Registers init_2   = expect_1;
    Registers expect_2 = {.sp = 0x0100, .ip = 0x02, .ss = 0x0200};

    return PopTestsParams{
        .cmd = {cmd},
        .cases =
            {
                TestCase{init_1, expect_1, {.address = 0x2100 - 4, .data = {0x00, 0x02}}, 12, loc},
                TestCase{
                    init_2, expect_2, {.address = 0x2100 - 2, .data = {0x00, 0x02, 0x00, 0x02}}, 12, loc},
            },
        .name = get_name(cmd),
    };
}

struct InitModRMPopTest
{
    InitModRMPopTest(const Registers& reg, const MemoryOp& mem, uint8_t cost,
                     const std::vector<uint8_t>& data,
                     const std::source_location loc = std::source_location::current())
        : reg_init(reg)
        , mem_init(mem)
        , cost(cost)
        , data(data)
        , loc(loc)
    {
    }
    Registers reg_init;
    MemoryOp mem_init;
    uint8_t cost;
    const std::vector<uint8_t> data;
    std::source_location loc;
};

const std::vector<InitModRMPopTest> pop_mod0 = {
    InitModRMPopTest(Registers{.bx = 0x0010, .si = 0x0100, .sp = 0x0fff},
                     MemoryOp{.address = 0x0110, .data = {0xab, 0xcd}}, 32, {}),
    InitModRMPopTest(Registers{.bx = 0x0010, .di = 0x0100, .sp = 0x0fff},
                     MemoryOp{.address = 0x0110, .data = {0xab, 0xcd}}, 32, {}),
    InitModRMPopTest(Registers{.si = 0x0100, .bp = 0x0010, .sp = 0x0fff},
                     MemoryOp{.address = 0x0110, .data = {0xab, 0xcd}}, 32, {}),
    InitModRMPopTest(Registers{.di = 0x0100, .bp = 0x0010, .sp = 0x0fff},
                     MemoryOp{.address = 0x0110, .data = {0xab, 0xcd}}, 31, {}),
    InitModRMPopTest(Registers{.si = 0x0110, .sp = 0x0fff}, MemoryOp{.address = 0x0110, .data = {0xab, 0xcd}},
                     29, {}),
    InitModRMPopTest(Registers{.di = 0x0110, .sp = 0x0fff}, MemoryOp{.address = 0x0110, .data = {0xab, 0xcd}},
                     29, {}),
    InitModRMPopTest(Registers{.sp = 0x0fff}, MemoryOp{.address = 0x0110, .data = {0xab, 0xcd}}, 30,
                     {0x10, 0x01}),
    InitModRMPopTest(Registers{.bx = 0x0110, .sp = 0x0fff}, MemoryOp{.address = 0x0110, .data = {0xab, 0xcd}},
                     29, {}),
};

const std::vector<InitModRMPopTest> pop_mod1 = {
    InitModRMPopTest(Registers{.bx = 0x0010, .si = 0x0100, .sp = 0x0fff},
                     MemoryOp{.address = 0x0130, .data = {0xab, 0xcd}}, 35, {0x20}),
    InitModRMPopTest(Registers{.bx = 0x0010, .di = 0x0100, .sp = 0x0fff},
                     MemoryOp{.address = 0x0130, .data = {0xab, 0xcd}}, 36, {0x20}),
    InitModRMPopTest(Registers{.si = 0x0100, .bp = 0x0010, .sp = 0x0fff},
                     MemoryOp{.address = 0x0130, .data = {0xab, 0xcd}}, 36, {0x20}),
    InitModRMPopTest(Registers{.di = 0x0100, .bp = 0x0010, .sp = 0x0fff},
                     MemoryOp{.address = 0x0130, .data = {0xab, 0xcd}}, 35, {0x20}),
    InitModRMPopTest(Registers{.si = 0x0110, .sp = 0x0fff}, MemoryOp{.address = 0x0130, .data = {0xab, 0xcd}},
                     33, {0x20}),
    InitModRMPopTest(Registers{.di = 0x0110, .sp = 0x0fff}, MemoryOp{.address = 0x0130, .data = {0xab, 0xcd}},
                     33, {0x20}),
    InitModRMPopTest(Registers{.bp = 0x0110, .sp = 0x0fff}, MemoryOp{.address = 0x0130, .data = {0xab, 0xcd}},
                     33, {0x20}),
    InitModRMPopTest(Registers{.bx = 0x0110, .sp = 0x0fff}, MemoryOp{.address = 0x0130, .data = {0xab, 0xcd}},
                     33, {0x20}),
};

const std::vector<InitModRMPopTest> pop_mod2 = {
    InitModRMPopTest(Registers{.bx = 0x0010, .si = 0x0100, .sp = 0x0fff},
                     MemoryOp{.address = 0x0230, .data = {0xab, 0xcd}}, 35, {0x20, 0x01}),
    InitModRMPopTest(Registers{.bx = 0x0010, .di = 0x0100, .sp = 0x0fff},
                     MemoryOp{.address = 0x0230, .data = {0xab, 0xcd}}, 36, {0x20, 0x01}),
    InitModRMPopTest(Registers{.si = 0x0100, .bp = 0x0010, .sp = 0x0fff},
                     MemoryOp{.address = 0x0230, .data = {0xab, 0xcd}}, 36, {0x20, 0x01}),
    InitModRMPopTest(Registers{.di = 0x0100, .bp = 0x0010, .sp = 0x0fff},
                     MemoryOp{.address = 0x0230, .data = {0xab, 0xcd}}, 35, {0x20, 0x01}),
    InitModRMPopTest(Registers{.si = 0x0110, .sp = 0x0fff}, MemoryOp{.address = 0x0230, .data = {0xab, 0xcd}},
                     33, {0x20, 0x01}),
    InitModRMPopTest(Registers{.di = 0x0110, .sp = 0x0fff}, MemoryOp{.address = 0x0230, .data = {0xab, 0xcd}},
                     33, {0x20, 0x01}),
    InitModRMPopTest(Registers{.bp = 0x0110, .sp = 0x0fff}, MemoryOp{.address = 0x0230, .data = {0xab, 0xcd}},
                     33, {0x20, 0x01}),
    InitModRMPopTest(Registers{.bx = 0x0110, .sp = 0x0fff}, MemoryOp{.address = 0x0230, .data = {0xab, 0xcd}},
                     33, {0x20, 0x01}),
};

const std::vector<InitModRMPopTest> pop_mod3 = {
    InitModRMPopTest(Registers{.ax = 0xcdba, .sp = 0x0fff}, MemoryOp{}, 15, {}),
    InitModRMPopTest(Registers{.cx = 0xcdba, .sp = 0x0fff}, MemoryOp{}, 15, {}),
    InitModRMPopTest(Registers{.dx = 0xcdba, .sp = 0x0fff}, MemoryOp{}, 15, {}),
    InitModRMPopTest(Registers{.bx = 0xcdba, .sp = 0x0fff}, MemoryOp{}, 15, {}),
    InitModRMPopTest(Registers{.sp = 0x0fff}, MemoryOp{}, 15, {}),
    InitModRMPopTest(Registers{.bp = 0xcdba, .sp = 0x0fff}, MemoryOp{}, 15, {}),
    InitModRMPopTest(Registers{.si = 0xcdba, .sp = 0x0fff}, MemoryOp{}, 15, {}),
    InitModRMPopTest(Registers{.di = 0xcdba, .sp = 0x0fff}, MemoryOp{}, 15, {}),
};

const std::vector<std::vector<InitModRMPopTest>> pop_modmr_data{pop_mod0, pop_mod1, pop_mod2, pop_mod3};

uint8_t get_size(const ModRM mod)
{
    if (mod.mod == 0 && mod.rm == 6)
    {
        return 4;
    }
    else if (mod.mod == 0)
    {
        return 2;
    }
    else if (mod.mod == 1)
    {
        return 3;
    }
    else if (mod.mod == 2)
    {
        return 4;
    }
    else
    {
        return 2;
    }
    return 0;
}

PopTestsParams generate_pop_data_modrm(const std::source_location loc = std::source_location::current())
{
    PopTestsParams params{.cmd = {0x8f}, .name = "0x8f"};
    ModRM mod = 0;

    mod.mod = 0;
    do
    {
        mod.reg = 6;
        mod.rm  = 0;
        do
        {
            const auto& test = pop_modmr_data[mod.mod][mod.rm];

            Registers init_regs   = test.reg_init;
            uint8_t cost          = test.cost;
            Registers expect_regs = init_regs;
            expect_regs.ip += get_size(mod);
            MemoryOp init_mem   = test.mem_init;
            MemoryOp expect_mem = {.address = expect_regs.sp};
            if (mod.rm == 4 && mod.mod == 3)
            {
                expect_mem.data.push_back(0xff);
                expect_mem.data.push_back(0x0f);
            }
            else if (mod.mod == 3)
            {
                expect_mem.data.push_back(0xba);
                expect_mem.data.push_back(0xcd);
            }

            expect_regs.sp += 2;
            std::copy(init_mem.data.begin(), init_mem.data.end(), std::back_inserter(expect_mem.data));

            std::vector<uint8_t> data = {mod};
            std::copy(test.data.begin(), test.data.end(), std::back_insert_iterator(data));
            TestCase case1{init_regs, expect_regs, expect_mem, cost, loc, data, init_mem, test.loc};
            params.cases.push_back(case1);

            init_regs = expect_regs;
            expect_regs.ip += get_size(mod);

            if (init_mem.data.size())
            {
                init_mem.data[0] -= 0x20;
                init_mem.data[1] -= 0x20;
                std::copy(init_mem.data.begin(), init_mem.data.end(), std::back_inserter(expect_mem.data));
            }
            expect_mem.address = expect_regs.sp;
            expect_mem.data[0] -= 0x20;
            expect_mem.data[1] -= 0x20;

            if (init_regs.sp == 0 && mod.mod == 3)
            {
                expect_mem.data.push_back(0xff);
                expect_mem.data.push_back(0x0f);
            }
            else if (mod.mod == 3)
            {
                expect_mem.data.push_back(0xba);
                expect_mem.data.push_back(0xcd);
            }

            expect_regs.sp += 2;
            data = {};
            std::copy(test.data.begin(), test.data.end(), std::back_insert_iterator(data));

            TestCase case2{init_regs, expect_regs, expect_mem, cost, loc, {mod}, init_mem, test.loc};
            params.cases.push_back(case2);
            ++mod.rm;
        } while (mod.rm != 1);
        ++mod.mod;
    } while (mod.mod != 1);


    return params;
}

} // namespace

class PopTests : public TestBase<PopTestsParams>
{
};

TEST_P(PopTests, ProcessCmd)
{
    const PopTestsParams& param = GetParam();
    bus_.clear();
    for (const auto& test : param.cases)
    {
        const uint32_t address = static_cast<uint32_t>((test.regs_init.cs << 4) + test.regs_init.ip);

        std::vector<uint8_t> cmd;
        std::copy(param.cmd.begin(), param.cmd.end(), std::back_inserter(cmd));
        std::copy(test.append_cmd.begin(), test.append_cmd.end(), std::back_inserter(cmd));
        bus_.write(address, cmd);

        std::cerr << "Test write to: " << std::hex << test.expect_memory.address
                  << ", v: " << static_cast<uint16_t>(test.expect_memory.data[0]) << std::endl;
        bus_.write(test.expect_memory.address, test.expect_memory.data);

        if (!test.memory_op.data.empty())
        {
            bus_.write(test.memory_op.address, test.memory_op.data);
        }

        sut_.set_registers(test.regs_init);
        sut_.step();
        EXPECT_EQ(sut_.get_registers(), test.expect) << print_test_case_info(test, sut_.get_error());
        EXPECT_EQ(sut_.last_instruction_cost(), test.cost) << print_test_case_info(test, sut_.get_error());
        std::vector<uint8_t> from_memory(test.memory_init.data.size());
        bus_.read(test.memory_init.address, from_memory);
        EXPECT_THAT(from_memory, ::testing::ElementsAreArray(test.memory_init.data))
            << print_test_case_info(test, sut_.get_error());
    }
}

auto get_pop_test_parameters()
{
    return ::testing::Values(
        generate_pop_data(0x58, &Registers::ax, 12), generate_pop_data(0x59, &Registers::cx, 12),
        generate_pop_data(0x5a, &Registers::dx, 12), generate_pop_data(0x5b, &Registers::bx, 12),
        generate_pop_data_for_sp(0x5c), generate_pop_data(0x5d, &Registers::bp, 12),
        generate_pop_data(0x5e, &Registers::si, 12), generate_pop_data(0x5f, &Registers::di, 12),
        generate_pop_data(0x07, &Registers::es, 12), generate_pop_data_for_ss(0x17),
        generate_pop_data(0x1f, &Registers::ds, 12), generate_pop_data_modrm());
}

inline void PrintTo(const PopTestsParams& reg, ::std::ostream* os)
{
    *os << "name: " << get_name(reg.cmd[0]) << std::endl;
}

INSTANTIATE_TEST_CASE_P(OpcodesPop, PopTests, get_pop_test_parameters(),
                        generate_test_case_name<PopTestsParams>);

} // namespace msemu::cpu8086
