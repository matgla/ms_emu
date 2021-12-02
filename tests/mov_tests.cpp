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
    CpuForTest<Memory<1024 * 128>> sut_;
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
    str << "cmd: {";
    stringify_array(data.cmd, str);
    str << "}" << std::endl;
    return str.str();
}

TEST_P(MovFromMemToRegTests, MovFromMemoryToRegisterCmd)
{
    auto data = GetParam();
    int i     = 0;

    sut_.set_registers(Registers{});
    for (const auto& test_data : data.data)
    {
        memory_.clear();
        if (test_data.memop.data.size())
        {
            memory_.write(test_data.memop.address, test_data.memop.data);
        }

        if (test_data.init)
        {
            sut_.set_registers(*test_data.init);
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

std::string get_name(uint8_t command)
{
    std::stringstream str;
    str << std::hex << std::setfill('0') << std::setw(2) << "0x"
        << static_cast<uint32_t>(command);
    return str.str();
}

MovFromMemToRegParams
    mem_to_reg_data(uint8_t command, uint16_t Registers::*reg, uint16_t expect,
                    const std::vector<uint8_t>& init_memory,
                    const std::source_location loc = std::source_location::current())
{
    Registers expect_1 = {.ip = 0x03};
    Registers expect_2 = {.ip = 0x06};
    expect_1.*reg      = expect;
    expect_2.*reg      = expect;

    return MovFromMemToRegParams{
        .name = get_name(command),
        .data = {
            TestData{
                .cmd      = {command, 0x01, 0x20},
                .memop    = {.address = 0x2001, .data = init_memory},
                .expect   = expect_1,
                .cycles   = 14,
                .location = loc,
            },
            TestData{
                .cmd      = {command, 0x10, 0x20},
                .memop    = {.address = 0x2010, .data = init_memory},
                .expect   = expect_2,
                .cycles   = 14,
                .location = loc,
            },
        }};
}

MovFromMemToRegParams
    reg_to_mem_data(uint8_t command, uint16_t Registers::*reg, uint16_t init_reg,
                    const std::vector<uint8_t>& expected_memory,
                    const std::source_location loc = std::source_location::current())
{
    Registers init_1   = {};
    init_1.*reg        = init_reg;
    Registers expect_1 = init_1;
    expect_1.ip        = 3;

    Registers init_2   = expect_1;
    Registers expect_2 = init_2;
    expect_2.ip        = 6;

    return MovFromMemToRegParams{
        .name = get_name(command),
        .data = {
            TestData{
                .cmd           = {command, 0x01, 0x20},
                .init          = init_1,
                .expect        = expect_1,
                .expect_memory = MemoryOp{.address = 0x2001, .data = expected_memory},
                .cycles        = 14,
                .location      = loc,
            },
            TestData{
                .cmd           = {command, 0x10, 0x20},
                .init          = init_2,
                .expect        = expect_2,
                .expect_memory = MemoryOp{.address = 0x2010, .data = expected_memory},
                .cycles        = 14,
                .location      = loc,
            },
        }};
}

MovFromMemToRegParams
    imm8_to_reg_lo(uint8_t command, uint16_t Registers::*reg,
                   const std::source_location loc = std::source_location::current())
{
    Registers expect_1  = {.ip = 2};
    Registers expect_2  = {.ip = 4};
    const uint8_t data1 = 0xab;
    expect_1.*reg       = 0x12ab;

    const uint8_t data2 = 0x12;
    expect_2.*reg       = 0x3412;

    Registers init_1{};
    init_1.*reg = 0x12bb;

    Registers init_2 = expect_1;
    init_2.*reg      = 0x34bb;

    return MovFromMemToRegParams{
        .name = get_name(command),
        .data =
            {
                {
                    .cmd      = {command, data1},
                    .init     = init_1,
                    .expect   = expect_1,
                    .cycles   = 4,
                    .location = loc,
                },
                {
                    .cmd      = {command, data2},
                    .init     = init_2,
                    .expect   = expect_2,
                    .cycles   = 4,
                    .location = loc,
                },
            },

    };
}

MovFromMemToRegParams
    imm8_to_reg_hi(uint8_t command, uint16_t Registers::*reg,
                   const std::source_location loc = std::source_location::current())
{
    Registers expect_1 = {.ip = 2};
    Registers expect_2 = {.ip = 4};
    Registers init_1{};
    init_1.*reg         = 0x1245;
    const uint8_t data1 = 0xab;
    expect_1.*reg       = 0xab45;

    Registers init_2    = expect_1;
    init_2.*reg         = 0x34bb;
    const uint8_t data2 = 0x12;
    expect_2.*reg       = 0x12bb;


    return MovFromMemToRegParams{
        .name = get_name(command),
        .data =
            {
                {
                    .cmd      = {command, data1},
                    .init     = init_1,
                    .expect   = expect_1,
                    .cycles   = 4,
                    .location = loc,
                },
                {
                    .cmd      = {command, data2},
                    .init     = init_2,
                    .expect   = expect_2,
                    .cycles   = 4,
                    .location = loc,
                },
            },

    };
}

MovFromMemToRegParams
    imm8_to_reg(uint8_t command, uint16_t Registers::*reg,
                const std::source_location loc = std::source_location::current())
{
    Registers expect_1 = {.ip = 3};
    Registers expect_2 = {.ip = 6};
    Registers init_1{};
    init_1.*reg           = 0xcdef;
    const uint8_t data1_l = 0xab;
    const uint8_t data1_h = 0xcd;
    expect_1.*reg         = 0xabcd;

    Registers init_2      = expect_1;
    init_2.*reg           = 0xaabb;
    const uint8_t data2_l = 0x12;
    const uint8_t data2_h = 0x34;
    expect_2.*reg         = 0x1234;


    return MovFromMemToRegParams{
        .name = get_name(command),
        .data =
            {
                {
                    .cmd      = {command, data1_h, data1_l},
                    .init     = init_1,
                    .expect   = expect_1,
                    .cycles   = 4,
                    .location = loc,
                },
                {
                    .cmd      = {command, data2_h, data2_l},
                    .init     = init_2,
                    .expect   = expect_2,
                    .cycles   = 4,
                    .location = loc,
                },
            },

    };
}

struct ModRMInit
{
    struct Op
    {
        Registers r;
        std::vector<uint8_t> d;
        uint8_t c;
    };
    std::array<Op, 8> o;
    MemoryOp m;
    uint16_t i;
    uint16_t e;
};

const std::array<ModRMInit::Op, 8> op_mod_0{
    ModRMInit::Op{
        .r = Registers{.bx = 0x1010, .si = 0x1020},
        .c = 19,
    },
    {
        .r = Registers{.bx = 0x1010, .di = 0x1020},
        .c = 20,
    },
    {
        .r = Registers{.si = 0x1020, .bp = 0x1010},
        .c = 20,
    },
    {
        .r = Registers{.di = 0x1020, .bp = 0x1010},
        .c = 19,
    },
    {
        .r = Registers{.si = 0x2030},
        .c = 17,
    },
    {
        .r = Registers{.di = 0x2030},
        .c = 17,
    },
    {
        .r = Registers{},
        .d = {0x30, 0x20},
        .c = 18,

    },
    {
        .r = Registers{.bx = 0x2030},
        .c = 17,
    },

};

const std::array<ModRMInit::Op, 8> op_mod_1{
    ModRMInit::Op{
        .r = Registers{.bx = 0x1010, .si = 0x1020},
        .d = {0x15},
        .c = 23,
    },
    {
        .r = Registers{.bx = 0x1010, .di = 0x1020},
        .d = {0x15},
        .c = 24,
    },
    {
        .r = Registers{.si = 0x1020, .bp = 0x1010},
        .d = {0x15},
        .c = 24,
    },
    {
        .r = Registers{.di = 0x1020, .bp = 0x1010},
        .d = {0x15},
        .c = 23,
    },
    {
        .r = Registers{.si = 0x2030},
        .d = {0x15},
        .c = 21,
    },
    {
        .r = Registers{.di = 0x2030},
        .d = {0x15},
        .c = 21,
    },
    {
        .r = Registers{.bp = 0x2030},
        .d = {0x15},
        .c = 21,

    },
    {
        .r = Registers{.bx = 0x2030},
        .d = {0x15},
        .c = 21,
    },
};

const std::array<ModRMInit::Op, 8> op_mod_3{
    ModRMInit::Op{
        .r = Registers{.bx = 0x1010, .si = 0x1020},
        .d = {0x15, 0x10},
        .c = 23,
    },
    {
        .r = Registers{.bx = 0x1010, .di = 0x1020},
        .d = {0x15, 0x10},
        .c = 24,
    },
    {
        .r = Registers{.si = 0x1020, .bp = 0x1010},
        .d = {0x15, 0x10},
        .c = 24,
    },
    {
        .r = Registers{.di = 0x1020, .bp = 0x1010},
        .d = {0x15, 0x10},
        .c = 23,
    },
    {
        .r = Registers{.si = 0x2030},
        .d = {0x15, 0x10},
        .c = 21,
    },
    {
        .r = Registers{.di = 0x2030},
        .d = {0x15, 0x10},
        .c = 21,
    },
    {
        .r = Registers{.bp = 0x2030},
        .d = {0x15, 0x10},
        .c = 21,

    },
    {
        .r = Registers{.bx = 0x2030},
        .d = {0x15, 0x10},
        .c = 21,
    },
};

const std::array<ModRMInit::Op, 8> op_mod_4{
    ModRMInit::Op{
        .r = Registers{.ax = 0x3412},
        .c = 2,
    },
    {
        .r = Registers{.cx = 0x3412},
        .c = 2,
    },
    {
        .r = Registers{.dx = 0x3412},
        .c = 2,
    },
    {
        .r = Registers{.bx = 0x3412},
        .c = 2,
    },
    {
        .r = Registers{.ax = 0x1234},
        .c = 2,
    },
    {
        .r = Registers{.cx = 0x1234},
        .c = 2,
    },
    {
        .r = Registers{.dx = 0x1234},
        .c = 2,
    },
    {
        .r = Registers{.bx = 0x1234},
        .c = 2,
    },
};

constexpr uint8_t expected_data = 0x3a;
const std::vector<std::vector<ModRMInit>> modmr_test_inits{
    {
        ModRMInit{.o = op_mod_0,
                  .m = MemoryOp{.address = 0x2030, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x123a},
        ModRMInit{.o = op_mod_0,
                  .m = MemoryOp{.address = 0x2030, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x123a},
        ModRMInit{.o = op_mod_0,
                  .m = MemoryOp{.address = 0x2030, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x123a},
        ModRMInit{.o = op_mod_0,
                  .m = MemoryOp{.address = 0x2030, .data = {expected_data}},
                  .i = 0x1010,
                  .e = 0x103a},
        ModRMInit{.o = op_mod_0,
                  .m = MemoryOp{.address = 0x2030, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x3a34},
        ModRMInit{.o = op_mod_0,
                  .m = MemoryOp{.address = 0x2030, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x3a34},
        ModRMInit{.o = op_mod_0,
                  .m = MemoryOp{.address = 0x2030, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x3a34},
        ModRMInit{.o = op_mod_0,
                  .m = MemoryOp{.address = 0x2030, .data = {expected_data}},
                  .i = 0x1010,
                  .e = 0x3a10},
    },
    {
        ModRMInit{.o = op_mod_1,
                  .m = MemoryOp{.address = 0x2045, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x123a},
        ModRMInit{.o = op_mod_1,
                  .m = MemoryOp{.address = 0x2045, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x123a},
        ModRMInit{.o = op_mod_1,
                  .m = MemoryOp{.address = 0x2045, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x123a},
        ModRMInit{.o = op_mod_1,
                  .m = MemoryOp{.address = 0x2045, .data = {expected_data}},
                  .i = 0x1010,
                  .e = 0x103a},
        ModRMInit{.o = op_mod_1,
                  .m = MemoryOp{.address = 0x2045, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x3a34},
        ModRMInit{.o = op_mod_1,
                  .m = MemoryOp{.address = 0x2045, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x3a34},
        ModRMInit{.o = op_mod_1,
                  .m = MemoryOp{.address = 0x2045, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x3a34},
        ModRMInit{.o = op_mod_1,
                  .m = MemoryOp{.address = 0x2045, .data = {expected_data}},
                  .i = 0x1010,
                  .e = 0x3a10},
    },
    {
        ModRMInit{.o = op_mod_3,
                  .m = MemoryOp{.address = 0x3045, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x123a},
        ModRMInit{.o = op_mod_3,
                  .m = MemoryOp{.address = 0x3045, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x123a},
        ModRMInit{.o = op_mod_3,
                  .m = MemoryOp{.address = 0x3045, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x123a},
        ModRMInit{.o = op_mod_3,
                  .m = MemoryOp{.address = 0x3045, .data = {expected_data}},
                  .i = 0x1010,
                  .e = 0x103a},
        ModRMInit{.o = op_mod_3,
                  .m = MemoryOp{.address = 0x3045, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x3a34},
        ModRMInit{.o = op_mod_3,
                  .m = MemoryOp{.address = 0x3045, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x3a34},
        ModRMInit{.o = op_mod_3,
                  .m = MemoryOp{.address = 0x3045, .data = {expected_data}},
                  .i = 0x1234,
                  .e = 0x3a34},
        ModRMInit{.o = op_mod_3,
                  .m = MemoryOp{.address = 0x3045, .data = {expected_data}},
                  .i = 0x1010,
                  .e = 0x3a10},
    },
    {
        ModRMInit{.o = op_mod_4, .e = 0x0012},
        ModRMInit{.o = op_mod_4, .e = 0x0012},
        ModRMInit{.o = op_mod_4, .e = 0x0012},
        ModRMInit{.o = op_mod_4, .e = 0x0012},
        ModRMInit{.o = op_mod_4, .e = 0x1200},
        ModRMInit{.o = op_mod_4, .e = 0x1200},
        ModRMInit{.o = op_mod_4, .e = 0x1200},
        ModRMInit{.o = op_mod_4, .e = 0x1200},
    }};

MovFromMemToRegParams
    modmr_to_reg8(uint8_t command,
                  const std::source_location loc = std::source_location::current())
{
    const std::vector<uint16_t Registers::*> regs{
        &Registers::ax, &Registers::cx, &Registers::dx, &Registers::bx,
        &Registers::ax, &Registers::cx, &Registers::dx, &Registers::bx};

    MovFromMemToRegParams params{.name = get_name(command)};

    uint8_t modmr = 0;

    for (std::size_t i = 0; i < 4; ++i)
    {
        std::size_t index = 0;

        for (std::size_t reg_id = 0; reg_id < regs.size(); ++reg_id)
        {
            const auto& r = regs[reg_id];

            auto& init = modmr_test_inits[i][reg_id];
            for (auto& d : init.o)
            {
                params.data.push_back(TestData{
                    .cmd      = {command, modmr},
                    .memop    = init.m,
                    .init     = d.r,
                    .expect   = d.r,
                    .cycles   = d.c,
                    .location = loc,
                });

                std::copy(d.d.begin(), d.d.end(),
                          std::back_inserter(params.data.back().cmd));


                if (i < 3)
                {
                    if ((*params.data.back().init).*r == 0)
                    {
                        (*params.data.back().init).*r   = init.i;
                        (*params.data.back().expect).*r = init.e;
                    }
                    else
                    {
                        if (reg_id < 4)
                        {

                            (*params.data.back().expect).*r &= 0xff00;
                            uint16_t dat = 0x003a;
                            (*params.data.back().expect).*r |= dat;
                        }
                        else
                        {
                            (*params.data.back().expect).*r &= 0x00ff;
                            uint16_t dat = 0x3a00;
                            (*params.data.back().expect).*r |= dat;
                        }
                    }
                }
                else
                {
                    ModRM m(modmr);
                    if (m.reg == m.rm)
                    {
                        (*params.data.back().expect).*r = (*params.data.back().init).*r;
                    }
                    else if ((m.reg & 0x3) == (m.rm & 0x3))
                    {
                        uint16_t original_value = (*params.data.back().init).*r;
                        if (m.rm < 4)
                        {
                            original_value &= 0x00ff;
                            original_value |=
                                static_cast<uint16_t>((original_value << 8) & 0xffff);
                            // original_value = 0x1212;
                        }
                        else
                        {
                            original_value &= 0xff00;
                            original_value |= (original_value >> 8);
                        }
                        (*params.data.back().expect).*r = original_value;
                    }
                    else
                    {
                        (*params.data.back().expect).*r = init.e;
                    }
                }
                (*params.data.back().expect).ip =
                    static_cast<uint16_t>(params.data.back().cmd.size());
                ++modmr;
            }
            ++index;
        }
    }
    return params;
}


auto get_mov_values()
{
    return ::testing::Values(
        mem_to_reg_data(0xa0, &Registers::ax, 0x00ab, {0xab, 0xff}),
        mem_to_reg_data(0xa1, &Registers::ax, 0xface, {0xce, 0xfa}),
        reg_to_mem_data(0xa2, &Registers::ax, 0x12ab, {0xab, 0x00}),
        reg_to_mem_data(0xa3, &Registers::ax, 0xabcd, {0xcd, 0xab}),
        imm8_to_reg_lo(0xb0, &Registers::ax), imm8_to_reg_lo(0xb1, &Registers::cx),
        imm8_to_reg_lo(0xb2, &Registers::dx), imm8_to_reg_lo(0xb3, &Registers::bx),
        imm8_to_reg_hi(0xb4, &Registers::ax), imm8_to_reg_hi(0xb5, &Registers::cx),
        imm8_to_reg_hi(0xb6, &Registers::dx), imm8_to_reg_hi(0xb7, &Registers::bx),
        imm8_to_reg(0xb8, &Registers::ax), imm8_to_reg(0xb9, &Registers::cx),
        imm8_to_reg(0xba, &Registers::dx), imm8_to_reg(0xbb, &Registers::bx),
        imm8_to_reg(0xbc, &Registers::sp), imm8_to_reg(0xbd, &Registers::bp),
        imm8_to_reg(0xbe, &Registers::si), imm8_to_reg(0xbf, &Registers::di),
        modmr_to_reg8(0x8a)
        //             .data =
        //                 {
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x00},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.ax = 0xfa00, .bx =
        //                             0x80, .si = 0x04}, .expect =
        //                                 Registers{.ax = 0x103a, .bx = 0x80, .si = 0x04,
        //                                 .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x01},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.ax = 0x123a, .bx = 0x80, .di = 0x04,
        //                                 .ip = 2},
        //                             .cycles = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x02},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.ax = 0x103a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 2},
        //                             .cycles = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x03},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.ax = 0xfa00, .di =
        //                             0x80, .bp = 0x04}, .expect =
        //                                 Registers{.ax = 0x123a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x04},
        //                             .memop  = MemoryOp{.address = 0xff, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.ax = 0xfa00, .si =
        //                             0xff}, .expect = Registers{.ax = 0x103a, .si =
        //                             0xff, .ip = 2}, .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x05},
        //                             .memop  = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0xab}}, .init   = Registers{.ax = 0xfa00, .di =
        //                             0x84}, .expect = Registers{.ax = 0xab3a, .di =
        //                             0x84, .ip = 2}, .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x06, 0x11, 0x20},
        //                             .memop  = MemoryOp{.address = 0x2011, .data =
        //                             {0x3a, 0xcc}}, .init   = Registers{}, .expect =
        //                             Registers{.ax = 0xcc3a, .ip = 4}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x07},
        //                             .memop  = MemoryOp{.address = 0xab, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.ax = 0x123a, .bx = 0xab, .ip = 2},
        //                             .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x08},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0xba}}, .init  = Registers{.bx = 0x80, .cx =
        //                             0xfa00, .si = 0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0xba3a, .si = 0x04,
        //                                 .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x09},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x11}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x113a, .di = 0x04,
        //                                 .ip = 2},
        //                             .cycles = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x0a},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x30}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.cx = 0x303a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 2},
        //                             .cycles = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x0b},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x20}}, .init  = Registers{.cx = 0xfa00, .di =
        //                             0x80, .bp = 0x04}, .expect =
        //                                 Registers{.cx = 0x203a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x0c},
        //                             .memop  = MemoryOp{.address = 0xff, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.cx = 0xfa00, .si =
        //                             0xff}, .expect = Registers{.cx = 0x103a, .si =
        //                             0xff, .ip = 2}, .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x0d},
        //                             .memop  = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.cx = 0xfa00, .di =
        //                             0x84}, .expect = Registers{.cx = 0x123a, .di =
        //                             0x84, .ip = 2}, .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x0e, 0x11, 0x20},
        //                             .memop  = MemoryOp{.address = 0x2011, .data =
        //                             {0x3a, 0x11}}, .init   = Registers{}, .expect =
        //                             Registers{.cx = 0x113a, .ip = 4}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x0f},
        //                             .memop  = MemoryOp{.address = 0xab, .data = {0x3a,
        //                             0x49}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .cx = 0x493a, .ip = 2},
        //                             .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x10},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0xcc}}, .init  = Registers{.bx = 0x80, .dx =
        //                             0xfa00, .si = 0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0xcc3a, .si = 0x04,
        //                                 .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x11},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x08}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x083a, .di = 0x04,
        //                                 .ip = 2},
        //                             .cycles = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x12},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.dx = 0x103a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 2},
        //                             .cycles = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x13},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.dx = 0xfa00, .di =
        //                             0x80, .bp = 0x04}, .expect =
        //                                 Registers{.dx = 0x123a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x14},
        //                             .memop  = MemoryOp{.address = 0xff, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.dx = 0xfa00, .si =
        //                             0xff}, .expect = Registers{.dx = 0x103a, .si =
        //                             0xff, .ip = 2}, .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x15},
        //                             .memop  = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.dx = 0xfa00, .di =
        //                             0x84}, .expect = Registers{.dx = 0x103a, .di =
        //                             0x84, .ip = 2}, .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x16, 0x11, 0x20},
        //                             .memop  = MemoryOp{.address = 0x2011, .data =
        //                             {0x3a, 0x10}}, .init   = Registers{}, .expect =
        //                             Registers{.dx = 0x103a, .ip = 4}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x17},
        //                             .memop  = MemoryOp{.address = 0xab, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .dx = 0x103a, .ip = 2},
        //                             .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x18},
        //                             .memop  = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0x80, .si =
        //                             0x04}, .expect = Registers{.bx = 0x123a, .si =
        //                             0x04, .ip = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x19},
        //                             .memop  = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0x80, .di =
        //                             0x04}, .expect = Registers{.bx = 0x123a, .di =
        //                             0x04, .ip = 2}, .cycles = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x1a},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.bx = 0x123a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 2},
        //                             .cycles = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x1b},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.bx = 0xfa00, .di =
        //                             0x80, .bp = 0x04}, .expect =
        //                                 Registers{.bx = 0x123a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x1c},
        //                             .memop  = MemoryOp{.address = 0xff, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0xfa00, .si =
        //                             0xff}, .expect = Registers{.bx = 0x123a, .si =
        //                             0xff, .ip = 2}, .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x1d},
        //                             .memop  = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0xfa00, .di =
        //                             0x84}, .expect = Registers{.bx = 0x123a, .di =
        //                             0x84, .ip = 2}, .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x1e, 0x11, 0x20},
        //                             .memop  = MemoryOp{.address = 0x2011, .data =
        //                             {0x3a, 0x12}}, .init   = Registers{}, .expect =
        //                             Registers{.bx = 0x123a, .ip = 4}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x1f},
        //                             .memop  = MemoryOp{.address = 0xab, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0x123a, .ip = 2}, .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x20},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .sp = 0x103a,
        //                                 .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x21},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .sp = 0x103a,
        //                                 .ip = 2},
        //                             .cycles = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x22},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .sp = 0x103a,
        //                                 .ip = 2},
        //                             .cycles = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x23},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.di = 0x80, .bp = 0x04},
        //                             .expect =
        //                                 Registers{.di = 0x80, .bp = 0x04, .sp = 0x103a,
        //                                 .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x24},
        //                             .memop  = MemoryOp{.address = 0xff, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.si = 0xff}, .expect =
        //                             Registers{.si = 0xff, .sp = 0x103a, .ip = 2},
        //                             .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x25},
        //                             .memop  = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.di = 0x84}, .expect =
        //                             Registers{.di = 0x84, .sp = 0x103a, .ip = 2},
        //                             .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x26, 0x11, 0x20},
        //                             .memop  = MemoryOp{.address = 0x2011, .data =
        //                             {0x3a, 0x10}}, .init   = Registers{}, .expect =
        //                             Registers{.sp = 0x103a, .ip = 4}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x27},
        //                             .memop  = MemoryOp{.address = 0xab, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .sp = 0x103a, .ip = 2},
        //                             .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x28},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .bp = 0x123a,
        //                                 .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x29},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .bp = 0x123a,
        //                                 .ip = 2},
        //                             .cycles = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x2a},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x123a, .ip = 2},
        //                             .cycles = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x2b},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.di = 0x80, .bp = 0x04},
        //                             .expect =
        //                                 Registers{.di = 0x80, .bp = 0x123a, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x2c},
        //                             .memop  = MemoryOp{.address = 0xff, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.si = 0xff}, .expect =
        //                             Registers{.si = 0xff, .bp = 0x123a, .ip = 2},
        //                             .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x2d},
        //                             .memop  = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.di = 0x84}, .expect =
        //                             Registers{.di = 0x84, .bp = 0x123a, .ip = 2},
        //                             .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x2e, 0x11, 0x20},
        //                             .memop  = MemoryOp{.address = 0x2011, .data =
        //                             {0x3a, 0x12}}, .init   = Registers{}, .expect =
        //                             Registers{.bp = 0x123a, .ip = 4}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x2f},
        //                             .memop  = MemoryOp{.address = 0xab, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .bp = 0x123a, .ip = 2},
        //                             .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x30},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x20}}, .init  = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x203a, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x31},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x20}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x203a, .di = 0x04,
        //                                 .ip = 2},
        //                             .cycles = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x32},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x20}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x203a, .bp = 0x80, .ip = 2},
        //                             .cycles = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x33},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x20}}, .init  = Registers{.di = 0x80, .bp = 0x04},
        //                             .expect =
        //                                 Registers{.si = 0x203a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x34},
        //                             .memop  = MemoryOp{.address = 0xff, .data = {0x3a,
        //                             0x20}}, .init   = Registers{.si = 0xff}, .expect =
        //                             Registers{.si = 0x203a, .ip = 2}, .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x35},
        //                             .memop  = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x20}}, .init   = Registers{.di = 0x84}, .expect =
        //                             Registers{.si = 0x203a, .di = 0x84, .ip = 2},
        //                             .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x36, 0x11, 0x20},
        //                             .memop  = MemoryOp{.address = 0x2011, .data =
        //                             {0x3a, 0x20}}, .init   = Registers{}, .expect =
        //                             Registers{.si = 0x203a, .ip = 4}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x37},
        //                             .memop  = MemoryOp{.address = 0xab, .data = {0x3a,
        //                             0x20}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .si = 0x203a, .ip = 2},
        //                             .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x38},
        //                             .memop  = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x20}}, .init   = Registers{.bx = 0x80, .si =
        //                             0x04}, .expect = Registers{.bx = 0x80, .si = 0x04,
        //                             .di = 0x203a, .ip = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x39},
        //                             .memop  = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x20}}, .init   = Registers{.bx = 0x80, .di =
        //                             0x04}, .expect = Registers{.bx = 0x80, .di =
        //                             0x203a, .ip = 2}, .cycles = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x3a},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x20}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .di = 0x203a, .bp = 0x80,
        //                                 .ip = 2},
        //                             .cycles = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x3b},
        //                             .memop = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x20}}, .init  = Registers{.di = 0x80, .bp = 0x04},
        //                             .expect =
        //                                 Registers{.di = 0x203a, .bp = 0x04, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x3c},
        //                             .memop  = MemoryOp{.address = 0xff, .data = {0x3a,
        //                             0x20}}, .init   = Registers{.si = 0xff}, .expect =
        //                             Registers{.si = 0xff, .di = 0x203a, .ip = 2},
        //                             .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x3d},
        //                             .memop  = MemoryOp{.address = 0x84, .data = {0x3a,
        //                             0x20}}, .init   = Registers{.di = 0x84}, .expect =
        //                             Registers{.di = 0x203a, .ip = 2}, .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x3e, 0x11, 0x20},
        //                             .memop  = MemoryOp{.address = 0x2011, .data =
        //                             {0x3a, 0x20}}, .init   = Registers{}, .expect =
        //                             Registers{.di = 0x203a, .ip = 4}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x3f},
        //                             .memop  = MemoryOp{.address = 0xab, .data = {0x3a,
        //                             0x20}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .di = 0x203a, .ip = 2},
        //                             .cycles = 0x0d,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x40, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.ax = 0xfa00, .bx =
        //                             0x80, .si = 0x04}, .expect =
        //                                 Registers{.ax = 0x103a, .bx = 0x80, .si = 0x04,
        //                                 .ip = 3},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x41, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.ax = 0x123a, .bx = 0x80, .di = 0x04,
        //                                 .ip = 3},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x42, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.ax = 0x103a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 3},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x43, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.ax = 0xfa00, .di =
        //                             0x80, .bp = 0x04}, .expect =
        //                                 Registers{.ax = 0x123a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x44, 0x10},
        //                             .memop  = MemoryOp{.address = 0x10f, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.ax = 0xfa00, .si =
        //                             0xff}, .expect = Registers{.ax = 0x103a, .si =
        //                             0xff, .ip = 3}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x45, 0x10},
        //                             .memop  = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0xab}}, .init   = Registers{.ax = 0xfa00, .di =
        //                             0x84}, .expect = Registers{.ax = 0xab3a, .di =
        //                             0x84, .ip = 3}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x46, 0x20},
        //                             .memop  = MemoryOp{.address = 0x30, .data = {0x3a,
        //                             0xcc}}, .init   = Registers{.bp = 0x10}, .expect =
        //                             Registers{.ax = 0xcc3a, .bp = 0x10, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x47, 0x10},
        //                             .memop  = MemoryOp{.address = 0xbb, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.ax = 0x123a, .bx = 0xab, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x48, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0xba}}, .init  = Registers{.bx = 0x80, .cx =
        //                             0xfa00, .si = 0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0xba3a, .si = 0x04,
        //                                 .ip = 3},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x49, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x11}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x113a, .di = 0x04,
        //                                 .ip = 3},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x4a, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x30}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.cx = 0x303a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 3},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x4b, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x20}}, .init  = Registers{.cx = 0xfa00, .di =
        //                             0x80, .bp = 0x04}, .expect =
        //                                 Registers{.cx = 0x203a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x4c, 0x10},
        //                             .memop  = MemoryOp{.address = 0x10f, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.cx = 0xfa00, .si =
        //                             0xff}, .expect = Registers{.cx = 0x103a, .si =
        //                             0xff, .ip = 3}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x4d, 0x10},
        //                             .memop  = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.cx = 0xfa00, .di =
        //                             0x84}, .expect = Registers{.cx = 0x123a, .di =
        //                             0x84, .ip = 3}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x4e, 0x11},
        //                             .memop  = MemoryOp{.address = 0x21, .data = {0x3a,
        //                             0x11}}, .init   = Registers{.bp = 0x10}, .expect =
        //                             Registers{.cx = 0x113a, .bp = 0x10, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x4f, 0x10},
        //                             .memop  = MemoryOp{.address = 0xbb, .data = {0x3a,
        //                             0x49}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .cx = 0x493a, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x50, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0xcc}}, .init  = Registers{.bx = 0x80, .dx =
        //                             0xfa00, .si = 0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0xcc3a, .si = 0x04,
        //                                 .ip = 3},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x51, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x08}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x083a, .di = 0x04,
        //                                 .ip = 3},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x52, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.dx = 0x103a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 3},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x53, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.dx = 0xfa00, .di =
        //                             0x80, .bp = 0x04}, .expect =
        //                                 Registers{.dx = 0x123a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x54, 0x10},
        //                             .memop  = MemoryOp{.address = 0x10f, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.dx = 0xfa00, .si =
        //                             0xff}, .expect = Registers{.dx = 0x103a, .si =
        //                             0xff, .ip = 3}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x55, 0x10},
        //                             .memop  = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.dx = 0xfa00, .di =
        //                             0x84}, .expect = Registers{.dx = 0x103a, .di =
        //                             0x84, .ip = 3}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x56, 0x10},
        //                             .memop  = MemoryOp{.address = 0x40, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.bp = 0x30}, .expect =
        //                             Registers{.dx = 0x103a, .bp = 0x30, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x57, 0x10},
        //                             .memop  = MemoryOp{.address = 0xbb, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .dx = 0x103a, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x58, 0x10},
        //                             .memop  = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0x80, .si =
        //                             0x04}, .expect = Registers{.bx = 0x123a, .si =
        //                             0x04, .ip = 3}, .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x59, 0x10},
        //                             .memop  = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0x80, .di =
        //                             0x04}, .expect = Registers{.bx = 0x123a, .di =
        //                             0x04, .ip = 3}, .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x5a, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.bx = 0x123a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 3},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x5b, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.bx = 0xfa00, .di =
        //                             0x80, .bp = 0x04}, .expect =
        //                                 Registers{.bx = 0x123a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x5c, 0x10},
        //                             .memop  = MemoryOp{.address = 0x10f, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0xfa00, .si =
        //                             0xff}, .expect = Registers{.bx = 0x123a, .si =
        //                             0xff, .ip = 3}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x5d, 0x10},
        //                             .memop  = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0xfa00, .di =
        //                             0x84}, .expect = Registers{.bx = 0x123a, .di =
        //                             0x84, .ip = 3}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x5e, 0x10},
        //                             .memop  = MemoryOp{.address = 0x40, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bp = 0x30}, .expect =
        //                             Registers{.bx = 0x123a, .bp = 0x30, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x5f, 0x10},
        //                             .memop  = MemoryOp{.address = 0xbb, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0x123a, .ip = 3}, .cycles = 0x11,
        //                         }),
        // ////////////////////////////////////////////////////////////////////////////////
        // ////////////////////////////////////////////////////////////////////////////////
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x60, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .sp = 0x103a,
        //                                 .ip = 3},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x61, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .sp = 0x123a,
        //                                 .ip = 3},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x62, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .sp = 0x103a,
        //                                 .ip = 3},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x63, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.di = 0x80, .bp = 0x04},
        //                             .expect =
        //                                 Registers{.di = 0x80, .bp = 0x04, .sp = 0x123a,
        //                                 .ip = 3},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x64, 0x10},
        //                             .memop  = MemoryOp{.address = 0x10f, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.si = 0xff}, .expect =
        //                             Registers{.si = 0xff, .sp = 0x103a, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x65, 0x10},
        //                             .memop  = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0xab}}, .init   = Registers{.di = 0x84}, .expect =
        //                             Registers{.di = 0x84, .sp = 0xab3a, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x66, 0x20},
        //                             .memop  = MemoryOp{.address = 0x30, .data = {0x3a,
        //                             0xcc}}, .init   = Registers{.bp = 0x10}, .expect =
        //                             Registers{.bp = 0x10, .sp = 0xcc3a, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x67, 0x10},
        //                             .memop  = MemoryOp{.address = 0xbb, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .sp = 0x123a, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x68, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .bp = 0x103a,
        //                                 .ip = 3},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x69, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .bp = 0x103a,
        //                                 .ip = 3},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x6a, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x103a, .ip = 3},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x6b, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.di = 0x80, .bp = 0x04},
        //                             .expect =
        //                                 Registers{.di = 0x80, .bp = 0x103a, .ip = 3},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x6c, 0x10},
        //                             .memop  = MemoryOp{.address = 0x10f, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.si = 0xff}, .expect =
        //                             Registers{.si = 0xff, .bp = 0x103a, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x6d, 0x10},
        //                             .memop  = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.di = 0x84}, .expect =
        //                             Registers{.di = 0x84, .bp = 0x103a, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x6e, 0x11},
        //                             .memop  = MemoryOp{.address = 0x21, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.bp = 0x10}, .expect =
        //                             Registers{.bp = 0x103a, .ip = 3}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x6f, 0x10},
        //                             .memop  = MemoryOp{.address = 0xbb, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .bp = 0x103a, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x70, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x103a, .ip = 3},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x71, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x103a, .di = 0x04,
        //                                 .ip = 3},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x72, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x103a, .bp = 0x80, .ip = 3},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x73, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.di = 0x80, .bp = 0x04},
        //                             .expect =
        //                                 Registers{.si = 0x103a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x74, 0x10},
        //                             .memop  = MemoryOp{.address = 0x10f, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.si = 0xff}, .expect =
        //                             Registers{.si = 0x103a, .ip = 3}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x75, 0x10},
        //                             .memop  = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.di = 0x84}, .expect =
        //                             Registers{.si = 0x103a, .di = 0x84, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x76, 0x10},
        //                             .memop  = MemoryOp{.address = 0x40, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.bp = 0x30}, .expect =
        //                             Registers{.si = 0x103a, .bp = 0x30, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x77, 0x10},
        //                             .memop  = MemoryOp{.address = 0xbb, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .si = 0x103a, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x78, 0x10},
        //                             .memop  = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0x80, .si =
        //                             0x04}, .expect = Registers{.bx = 0x80, .si = 0x04,
        //                             .di = 0x123a, .ip = 3}, .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x79, 0x10},
        //                             .memop  = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0x80, .di =
        //                             0x04}, .expect = Registers{.bx = 0x80, .di =
        //                             0x123a, .ip = 3}, .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x7a, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .di = 0x123a, .bp = 0x80,
        //                                 .ip = 3},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x7b, 0x10},
        //                             .memop = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.di = 0x80, .bp = 0x04},
        //                             .expect =
        //                                 Registers{.di = 0x123a, .bp = 0x04, .ip = 3},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x7c, 0x10},
        //                             .memop  = MemoryOp{.address = 0x10f, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.si = 0xff}, .expect =
        //                             Registers{.si = 0xff, .di = 0x123a, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x7d, 0x10},
        //                             .memop  = MemoryOp{.address = 0x94, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.di = 0x84}, .expect =
        //                             Registers{.di = 0x123a, .ip = 3}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x7e, 0x10},
        //                             .memop  = MemoryOp{.address = 0x40, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bp = 0x30}, .expect =
        //                             Registers{.di = 0x123a, .bp = 0x30, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x7f, 0x10},
        //                             .memop  = MemoryOp{.address = 0xbb, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .di = 0x123a, .ip = 3},
        //                             .cycles = 0x11,
        //                         }),
        // ///////////////////////////////////////////////////////////////////////////////////
        // ///////////////////////////////////////////////////////////////////////////////////
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x80, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x0894, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.ax = 0xfa00, .bx =
        //                             0x80, .si = 0x04}, .expect =
        //                                 Registers{.ax = 0x103a, .bx = 0x80, .si = 0x04,
        //                                 .ip = 4},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x81, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x0894, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.ax = 0x123a, .bx = 0x80, .di = 0x04,
        //                                 .ip = 4},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x82, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x0894, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.ax = 0x103a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 4},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x83, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x0894, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.ax = 0xfa00, .di =
        //                             0x80, .bp = 0x04}, .expect =
        //                                 Registers{.ax = 0x123a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x84, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x90f, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.ax = 0xfa00, .si =
        //                             0xff}, .expect = Registers{.ax = 0x103a, .si =
        //                             0xff, .ip = 4}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x85, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0xab}}, .init   = Registers{.ax = 0xfa00, .di =
        //                             0x84}, .expect = Registers{.ax = 0xab3a, .di =
        //                             0x84, .ip = 4}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x86, 0x20, 0x08},
        //                             .memop  = MemoryOp{.address = 0x830, .data = {0x3a,
        //                             0xcc}}, .init   = Registers{.bp = 0x10}, .expect =
        //                             Registers{.ax = 0xcc3a, .bp = 0x10, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x87, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x8bb, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.ax = 0x123a, .bx = 0xab, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x88, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0xba}}, .init  = Registers{.bx = 0x80, .cx =
        //                             0xfa00, .si = 0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0xba3a, .si = 0x04,
        //                                 .ip = 4},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x89, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x11}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x113a, .di = 0x04,
        //                                 .ip = 4},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x8a, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x30}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.cx = 0x303a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 4},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x8b, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x20}}, .init  = Registers{.cx = 0xfa00, .di =
        //                             0x80, .bp = 0x04}, .expect =
        //                                 Registers{.cx = 0x203a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x8c, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x90f, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.si = 0xff}, .expect =
        //                             Registers{.cx = 0x103a, .si = 0xff, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x8d, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.cx = 0xfa00, .di =
        //                             0x84}, .expect = Registers{.cx = 0x123a, .di =
        //                             0x84, .ip = 4}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x8e, 0x11, 0x08},
        //                             .memop  = MemoryOp{.address = 0x821, .data = {0x3a,
        //                             0x11}}, .init   = Registers{.bp = 0x10}, .expect =
        //                             Registers{.cx = 0x113a, .bp = 0x10, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x8f, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x8bb, .data = {0x3a,
        //                             0x49}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .cx = 0x493a, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x90, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0xcc}}, .init  = Registers{.bx = 0x80, .dx =
        //                             0xfa00, .si = 0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0xcc3a, .si = 0x04,
        //                                 .ip = 4},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x91, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x08}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x083a, .di = 0x04,
        //                                 .ip = 4},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x92, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.dx = 0x103a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 4},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x93, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.dx = 0xfa00, .di =
        //                             0x80, .bp = 0x04}, .expect =
        //                                 Registers{.dx = 0x123a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x94, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x90f, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.dx = 0xfa00, .si =
        //                             0xff}, .expect = Registers{.dx = 0x103a, .si =
        //                             0xff, .ip = 4}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x95, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.dx = 0xfa00, .di =
        //                             0x84}, .expect = Registers{.dx = 0x103a, .di =
        //                             0x84, .ip = 4}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x96, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x840, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.bp = 0x30}, .expect =
        //                             Registers{.dx = 0x103a, .bp = 0x30, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x97, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x8bb, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .dx = 0x103a, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x98, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0x80, .si =
        //                             0x04}, .expect = Registers{.bx = 0x123a, .si =
        //                             0x04, .ip = 4}, .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x99, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0x80, .di =
        //                             0x04}, .expect = Registers{.bx = 0x123a, .di =
        //                             0x04, .ip = 4}, .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x9a, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.bx = 0x123a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 4},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0x9b, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.bx = 0xfa00, .di =
        //                             0x80, .bp = 0x04}, .expect =
        //                                 Registers{.bx = 0x123a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x9c, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x90f, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0xfa00, .si =
        //                             0xff}, .expect = Registers{.bx = 0x123a, .si =
        //                             0xff, .ip = 4}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x9d, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0xfa00, .di =
        //                             0x84}, .expect = Registers{.bx = 0x123a, .di =
        //                             0x84, .ip = 4}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x9e, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x840, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bp = 0x30}, .expect =
        //                             Registers{.bx = 0x123a, .bp = 0x30, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0x9f, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x8bb, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0x123a, .ip = 4}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0xa0, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .sp = 0x103a,
        //                                 .ip = 4},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0xa1, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .sp = 0x123a,
        //                                 .ip = 4},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0xa2, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .sp = 0x103a,
        //                                 .ip = 4},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0xa3, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.di = 0x80, .bp = 0x04},
        //                             .expect =
        //                                 Registers{.di = 0x80, .bp = 0x04, .sp = 0x123a,
        //                                 .ip = 4},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xa4, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x90f, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.si = 0xff}, .expect =
        //                             Registers{.si = 0xff, .sp = 0x103a, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xa5, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0xab}}, .init   = Registers{.di = 0x84}, .expect =
        //                             Registers{.di = 0x84, .sp = 0xab3a, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xa6, 0x20, 0x08},
        //                             .memop  = MemoryOp{.address = 0x830, .data = {0x3a,
        //                             0xcc}}, .init   = Registers{.bp = 0x10}, .expect =
        //                             Registers{.bp = 0x10, .sp = 0xcc3a, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xa7, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x8bb, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .sp = 0x123a, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0xa8, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .bp = 0x103a,
        //                                 .ip = 4},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0xa9, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .bp = 0x103a,
        //                                 .ip = 4},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0xaa, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x103a, .ip = 4},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0xab, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.di = 0x80, .bp = 0x04},
        //                             .expect =
        //                                 Registers{.di = 0x80, .bp = 0x103a, .ip = 4},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xac, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x90f, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.si = 0xff}, .expect =
        //                             Registers{.si = 0xff, .bp = 0x103a, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xad, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.di = 0x84}, .expect =
        //                             Registers{.di = 0x84, .bp = 0x103a, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xae, 0x11, 0x08},
        //                             .memop  = MemoryOp{.address = 0x821, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.bp = 0x10}, .expect =
        //                             Registers{.bp = 0x103a, .ip = 4}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xaf, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x8bb, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .bp = 0x103a, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0xb0, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x103a, .ip = 4},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0xb1, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x103a, .di = 0x04,
        //                                 .ip = 4},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0xb2, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x103a, .bp = 0x80, .ip = 4},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0xb3, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x10}}, .init  = Registers{.di = 0x80, .bp = 0x04},
        //                             .expect =
        //                                 Registers{.si = 0x103a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xb4, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x90f, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.si = 0xff}, .expect =
        //                             Registers{.si = 0x103a, .ip = 4}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xb5, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.di = 0x84}, .expect =
        //                             Registers{.si = 0x103a, .di = 0x84, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xb6, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x840, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.bp = 0x30}, .expect =
        //                             Registers{.si = 0x103a, .bp = 0x30, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xb7, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x8bb, .data = {0x3a,
        //                             0x10}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .si = 0x103a, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xb8, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0x80, .si =
        //                             0x04}, .expect = Registers{.bx = 0x80, .si = 0x04,
        //                             .di = 0x123a, .ip = 4}, .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xb9, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0x80, .di =
        //                             0x04}, .expect = Registers{.bx = 0x80, .di =
        //                             0x123a, .ip = 4}, .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0xba, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .di = 0x123a, .bp = 0x80,
        //                                 .ip = 4},
        //                             .cycles = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd   = {0x8b, 0xbb, 0x10, 0x08},
        //                             .memop = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x12}}, .init  = Registers{.di = 0x80, .bp = 0x04},
        //                             .expect =
        //                                 Registers{.di = 0x123a, .bp = 0x04, .ip = 4},
        //                             .cycles = 0x13,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xbc, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x90f, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.si = 0xff}, .expect =
        //                             Registers{.si = 0xff, .di = 0x123a, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xbd, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x894, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.di = 0x84}, .expect =
        //                             Registers{.di = 0x123a, .ip = 4}, .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xbe, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x840, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bp = 0x30}, .expect =
        //                             Registers{.di = 0x123a, .bp = 0x30, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xbf, 0x10, 0x08},
        //                             .memop  = MemoryOp{.address = 0x8bb, .data = {0x3a,
        //                             0x12}}, .init   = Registers{.bx = 0xab}, .expect =
        //                             Registers{.bx = 0xab, .di = 0x123a, .ip = 4},
        //                             .cycles = 0x11,
        //                         }),
        // ////////////////////////////////////////////
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xc0},
        //                             .init   = Registers{.ax = 0xface},
        //                             .expect = Registers{.ax = 0xface, .ip = 2},
        //                             .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xc1},
        //                             .init   = Registers{.cx = 0xabcd},
        //                             .expect = Registers{.ax = 0xabcd, .cx = 0xabcd, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xc2},
        //                             .init   = Registers{.dx = 0xaabb},
        //                             .expect = Registers{.ax = 0xaabb, .dx = 0xaabb, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xc3},
        //                             .init   = Registers{.bx = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .bx = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xc4},
        //                             .init   = Registers{.sp = 0xabba},
        //                             .expect = Registers{.ax = 0xabba, .sp = 0xabba, .ip
        //                             = 2}, .cycles = 0x0e,

        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xc5},
        //                             .init   = Registers{.bp = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .bp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xc6},
        //                             .init   = Registers{.si = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .si = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xc7},
        //                             .init   = Registers{.di = 0xface},
        //                             .expect = Registers{.ax = 0xface, .di = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xc8},
        //                             .init   = Registers{.ax = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .cx = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xc9},
        //                             .init   = Registers{.cx = 0x12ab},
        //                             .expect = Registers{.cx = 0x12ab, .ip = 2},
        //                             .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xca},
        //                             .init   = Registers{.dx = 0xaabb},
        //                             .expect = Registers{.cx = 0xaabb, .dx = 0xaabb, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xcb},
        //                             .init   = Registers{.bx = 0xface},
        //                             .expect = Registers{.bx = 0xface, .cx = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xcc},
        //                             .init   = Registers{.sp = 0xface},
        //                             .expect = Registers{.cx = 0xface, .sp = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,

        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xcd},
        //                             .init   = Registers{.bp = 0x1234},
        //                             .expect = Registers{.cx = 0x1234, .bp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xce},
        //                             .init   = Registers{.si = 0xface,},
        //                             .expect = Registers{.cx = 0xface, .si = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xcf},
        //                             .init   = Registers{.di = 0xfaab},
        //                             .expect = Registers{.cx = 0xfaab, .di = 0xfaab, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),

        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xd0},
        //                             .init   = Registers{.ax = 0xface},
        //                             .expect = Registers{.ax = 0xface, .dx = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xd1},
        //                             .init   = Registers{.cx = 0xabcd},
        //                             .expect = Registers{.cx = 0xabcd, .dx = 0xabcd, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xd2},
        //                             .init   = Registers{.dx = 0xaabb},
        //                             .expect = Registers{.dx = 0xaabb, .ip = 2},
        //                             .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xd3},
        //                             .init   = Registers{.bx = 0x1234},
        //                             .expect = Registers{.bx = 0x1234, .dx = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xd4},
        //                             .init   = Registers{.sp = 0xabba},
        //                             .expect = Registers{.dx = 0xabba, .sp = 0xabba, .ip
        //                             = 2}, .cycles = 0x0e,

        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xd5},
        //                             .init   = Registers{.bp = 0x1234},
        //                             .expect = Registers{.dx = 0x1234, .bp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xd6},
        //                             .init   = Registers{.si = 0x1234},
        //                             .expect = Registers{.dx = 0x1234, .si = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xd7},
        //                             .init   = Registers{.di = 0xface},
        //                             .expect = Registers{.dx = 0xface, .di = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xd8},
        //                             .init   = Registers{.ax = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .bx = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xd9},
        //                             .init   = Registers{.cx = 0x12ab},
        //                             .expect = Registers{.bx = 0x12ab, .cx = 0x12ab, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xda},
        //                             .init   = Registers{.dx = 0xaabb},
        //                             .expect = Registers{.bx = 0xaabb, .dx = 0xaabb, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xdb},
        //                             .init   = Registers{.bx = 0xface},
        //                             .expect = Registers{.bx = 0xface, .ip = 2},
        //                             .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xdc},
        //                             .init   = Registers{.sp = 0xface},
        //                             .expect = Registers{.bx = 0xface, .sp = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,

        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xdd},
        //                             .init   = Registers{.bp = 0x1234},
        //                             .expect = Registers{.bx = 0x1234, .bp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xde},
        //                             .init   = Registers{.si = 0xface,},
        //                             .expect = Registers{.bx = 0xface, .si = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xdf},
        //                             .init   = Registers{.di = 0xfaab},
        //                             .expect = Registers{.bx = 0xfaab, .di = 0xfaab, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xe0},
        //                             .init   = Registers{.ax = 0xface},
        //                             .expect = Registers{.ax = 0xface, .sp = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xe1},
        //                             .init   = Registers{.cx = 0xabcd},
        //                             .expect = Registers{.cx = 0xabcd, .sp = 0xabcd, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xe2},
        //                             .init   = Registers{.dx = 0xaabb},
        //                             .expect = Registers{.dx = 0xaabb, .sp = 0xaabb, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xe3},
        //                             .init   = Registers{.bx = 0x1234},
        //                             .expect = Registers{.bx = 0x1234, .sp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xe4},
        //                             .init   = Registers{.sp = 0xabba},
        //                             .expect = Registers{.sp = 0xabba, .ip = 2},
        //                             .cycles = 0x0e,

        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xe5},
        //                             .init   = Registers{.bp = 0x1234},
        //                             .expect = Registers{.bp = 0x1234, .sp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xe6},
        //                             .init   = Registers{.si = 0x1234},
        //                             .expect = Registers{.si = 0x1234, .sp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xe7},
        //                             .init   = Registers{.di = 0xface},
        //                             .expect = Registers{.di = 0xface, .sp = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xe8},
        //                             .init   = Registers{.ax = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .bp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xe9},
        //                             .init   = Registers{.cx = 0x12ab},
        //                             .expect = Registers{.cx = 0x12ab, .bp = 0x12ab, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xea},
        //                             .init   = Registers{.dx = 0xaabb},
        //                             .expect = Registers{.dx = 0xaabb, .bp = 0xaabb, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xeb},
        //                             .init   = Registers{.bx = 0xface},
        //                             .expect = Registers{.bx = 0xface, .bp = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xec},
        //                             .init   = Registers{.sp = 0xface},
        //                             .expect = Registers{.bp = 0xface, .sp = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,

        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xed},
        //                             .init   = Registers{.bp = 0x1234},
        //                             .expect = Registers{.bp = 0x1234, .ip = 2},
        //                             .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xee},
        //                             .init   = Registers{.si = 0xface},
        //                             .expect = Registers{.si = 0xface, .bp = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xef},
        //                             .init   = Registers{.di = 0xfaab},
        //                             .expect = Registers{.di = 0xfaab, .bp = 0xfaab, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xf0},
        //                             .init   = Registers{.ax = 0xface},
        //                             .expect = Registers{.ax = 0xface, .si = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xf1},
        //                             .init   = Registers{.cx = 0xabcd},
        //                             .expect = Registers{.cx = 0xabcd, .si = 0xabcd, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xf2},
        //                             .init   = Registers{.dx = 0xaabb},
        //                             .expect = Registers{.dx = 0xaabb, .si = 0xaabb, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xf3},
        //                             .init   = Registers{.bx = 0x1234},
        //                             .expect = Registers{.bx = 0x1234, .si = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xf4},
        //                             .init   = Registers{.sp = 0xabba},
        //                             .expect = Registers{.si = 0xabba, .sp = 0xabba, .ip
        //                             = 2}, .cycles = 0x0e,

        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xf5},
        //                             .init   = Registers{.bp = 0x1234},
        //                             .expect = Registers{.si = 0x1234, .bp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xf6},
        //                             .init   = Registers{.si = 0x1234},
        //                             .expect = Registers{.si = 0x1234, .ip = 2},
        //                             .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xf7},
        //                             .init   = Registers{.di = 0xface},
        //                             .expect = Registers{.si = 0xface, .di = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xf8},
        //                             .init   = Registers{.ax = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .di = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xf9},
        //                             .init   = Registers{.cx = 0x12ab},
        //                             .expect = Registers{.cx = 0x12ab, .di = 0x12ab, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xfa},
        //                             .init   = Registers{.dx = 0xaabb},
        //                             .expect = Registers{.dx = 0xaabb, .di = 0xaabb, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xfb},
        //                             .init   = Registers{.bx = 0xface},
        //                             .expect = Registers{.bx = 0xface, .di = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xfc},
        //                             .init   = Registers{.sp = 0xface},
        //                             .expect = Registers{.di = 0xface, .sp = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,

        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xfd},
        //                             .init   = Registers{.bp = 0x1234},
        //                             .expect = Registers{.di = 0x1234, .bp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xfe},
        //                             .init   = Registers{.si = 0xface},
        //                             .expect = Registers{.si = 0xface, .di = 0xface, .ip
        //                             = 2}, .cycles = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x8b, 0xff},
        //                             .init   = Registers{.di = 0xfaab},
        //                             .expect = Registers{.di = 0xfaab, .ip = 2},
        //                             .cycles = 0x0e,
        //                         }),
        //                 },
        //         },
        //         MovFromMemToRegParams{
        //             .name = "0x8c",
        //             .data =
        //                 {
        //                     add(
        //                         {
        //                             .cmd  = {0x8c, 0x00},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .ip = 2, .es
        //                                 = 0x1234},
        //                             .cycles        = 0x10,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x01},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .ip = 2, .es
        //                                 = 0x1234},
        //                             .cycles        = 0x11,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x02},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .ip = 2, .es
        //                                 = 0x1234},
        //                             .cycles        = 0x11,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x03},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.di = 0x04, .bp = 0x80, .ip = 2, .es
        //                                 = 0x1234},
        //                             .cycles        = 0x10,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x04},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x84}, .expect =
        //                                 Registers{.si = 0x84, .ip = 2, .es = 0x1234},
        //                             .cycles        = 0x0e,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x05},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x84}, .expect =
        //                                 Registers{.di = 0x84, .ip = 2, .es = 0x1234},
        //                             .cycles        = 0x0e,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x06, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2010, .data = {0x34,
        //                             0x12}}, .init = Registers{}, .expect =
        //                                 Registers{.ip = 4, .es = 0x1234},
        //                             .cycles        = 0x0f,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x07},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x84}, .expect =
        //                                 Registers{.bx = 0x84, .ip = 2, .es = 0x1234},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x8c, 0x08},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .ip = 2, .cs
        //                                 = 0x1234},
        //                             .cycles        = 0x10,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x09},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .ip = 2, .cs
        //                                 = 0x1234},
        //                             .cycles        = 0x11,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x0a},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .ip = 2, .cs
        //                                 = 0x1234},
        //                             .cycles        = 0x11,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x0b},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.di = 0x04, .bp = 0x80, .ip = 2, .cs
        //                                 = 0x1234},
        //                             .cycles        = 0x10,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x0c},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x84}, .expect =
        //                                 Registers{.si = 0x84, .ip = 2, .cs = 0x1234},
        //                             .cycles        = 0x0e,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x0d},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x84}, .expect =
        //                                 Registers{.di = 0x84, .ip = 2, .cs = 0x1234},
        //                             .cycles        = 0x0e,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x0e, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2010, .data = {0x34,
        //                             0x12}}, .init = Registers{}, .expect =
        //                                 Registers{.ip = 4, .cs = 0x1234},
        //                             .cycles        = 0x0f,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x0f},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x84}, .expect =
        //                                 Registers{.bx = 0x84, .ip = 2, .cs = 0x1234},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x8c, 0x10},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .ip = 2, .ss
        //                                 = 0x1234},
        //                             .cycles        = 0x10,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x11},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .ip = 2, .ss
        //                                 = 0x1234},
        //                             .cycles        = 0x11,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x12},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .ip = 2, .ss
        //                                 = 0x1234},
        //                             .cycles        = 0x11,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x13},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.di = 0x04, .bp = 0x80, .ip = 2, .ss
        //                                 = 0x1234},
        //                             .cycles        = 0x10,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x14},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x84}, .expect =
        //                                 Registers{.si = 0x84, .ip = 2, .ss = 0x1234},
        //                             .cycles        = 0x0e,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x15},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x84}, .expect =
        //                                 Registers{.di = 0x84, .ip = 2, .ss = 0x1234},
        //                             .cycles        = 0x0e,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x16, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2010, .data = {0x34,
        //                             0x12}}, .init = Registers{}, .expect =
        //                                 Registers{.ip = 4, .ss = 0x1234},
        //                             .cycles        = 0x0f,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x17},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x84}, .expect =
        //                                 Registers{.bx = 0x84, .ip = 2, .ss = 0x1234},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x8c, 0x18},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .ip = 2, .ds
        //                                 = 0x1234},
        //                             .cycles        = 0x10,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x19},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .ip = 2, .ds
        //                                 = 0x1234},
        //                             .cycles        = 0x11,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x1a},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .ip = 2, .ds
        //                                 = 0x1234},
        //                             .cycles        = 0x11,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x1b},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.di = 0x04, .bp = 0x80, .ip = 2, .ds
        //                                 = 0x1234},
        //                             .cycles        = 0x10,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x1c},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x84}, .expect =
        //                                 Registers{.si = 0x84, .ip = 2, .ds = 0x1234},
        //                             .cycles        = 0x0e,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x1d},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x84}, .expect =
        //                                 Registers{.di = 0x84, .ip = 2, .ds = 0x1234},
        //                             .cycles        = 0x0e,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x1e, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2010, .data = {0x34,
        //                             0x12}}, .init = Registers{}, .expect =
        //                                 Registers{.ip = 4, .ds = 0x1234},
        //                             .cycles        = 0x0f,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x1f},
        //                             .memop= MemoryOp{.address = 0x84, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x84}, .expect =
        //                                 Registers{.bx = 0x84, .ip = 2, .ds = 0x1234},
        //                             .cycles        = 0x0e,
        //                         }),
        // /////////////////////////////////////////////////
        //                     add(
        //                         {
        //                             .cmd  = {0x8c, 0x40, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .ip = 3, .es
        //                                 = 0x1234},
        //                             .cycles        = 0x14,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x41, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .ip = 3, .es
        //                                 = 0x1234},
        //                             .cycles        = 0x15,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x42, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .ip = 3, .es
        //                                 = 0x1234},
        //                             .cycles        = 0x15,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x43, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.di = 0x04, .bp = 0x80, .ip = 3, .es
        //                                 = 0x1234},
        //                             .cycles        = 0x14,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x44, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x84}, .expect =
        //                                 Registers{.si = 0x84, .ip = 3, .es = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x45, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x84}, .expect =
        //                                 Registers{.di = 0x84, .ip = 3, .es = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x46, 0x10},
        //                             .memop= MemoryOp{.address = 0x2020, .data = {0x34,
        //                             0x12}}, .init = Registers{.bp = 0x2010}, .expect =
        //                                 Registers{.bp = 0x2010, .ip = 3, .es = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x47, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x84}, .expect =
        //                                 Registers{.bx = 0x84, .ip = 3, .es = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x8c, 0x48, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .ip = 3, .cs
        //                                 = 0x1234},
        //                             .cycles        = 0x14,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x49, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .ip = 3, .cs
        //                                 = 0x1234},
        //                             .cycles        = 0x15,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x4a, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .ip = 3, .cs
        //                                 = 0x1234},
        //                             .cycles        = 0x15,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x4b, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.di = 0x04, .bp = 0x80, .ip = 3, .cs
        //                                 = 0x1234},
        //                             .cycles        = 0x14,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x4c, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x84}, .expect =
        //                                 Registers{.si = 0x84, .ip = 3, .cs = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x4d, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x84}, .expect =
        //                                 Registers{.di = 0x84, .ip = 3, .cs = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x4e, 0x10},
        //                             .memop= MemoryOp{.address = 0x2020, .data = {0x34,
        //                             0x12}}, .init = Registers{.bp = 0x2010}, .expect =
        //                                 Registers{.bp = 0x2010, .ip = 3, .cs = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x4f, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x84}, .expect =
        //                                 Registers{.bx = 0x84, .ip = 3, .cs = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x8c, 0x50, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .ip = 3, .ss
        //                                 = 0x1234},
        //                             .cycles        = 0x14,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x51, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .ip = 3, .ss
        //                                 = 0x1234},
        //                             .cycles        = 0x15,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x52, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .ip = 3, .ss
        //                                 = 0x1234},
        //                             .cycles        = 0x15,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x53, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.di = 0x04, .bp = 0x80, .ip = 3, .ss
        //                                 = 0x1234},
        //                             .cycles        = 0x14,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x54, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x84}, .expect =
        //                                 Registers{.si = 0x84, .ip = 3, .ss = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x55, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x84}, .expect =
        //                                 Registers{.di = 0x84, .ip = 3, .ss = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x56, 0x10},
        //                             .memop= MemoryOp{.address = 0x2020, .data = {0x34,
        //                             0x12}}, .init = Registers{.bp = 0x2010}, .expect =
        //                                 Registers{.bp = 0x2010, .ip = 3, .ss = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x57, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x84}, .expect =
        //                                 Registers{.bx = 0x84, .ip = 3, .ss = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x8c, 0x58, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .ip = 3, .ds
        //                                 = 0x1234},
        //                             .cycles        = 0x14,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x59, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .ip = 3, .ds
        //                                 = 0x1234},
        //                             .cycles        = 0x15,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x5a, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .ip = 3, .ds
        //                                 = 0x1234},
        //                             .cycles        = 0x15,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x5b, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.di = 0x04, .bp = 0x80, .ip = 3, .ds
        //                                 = 0x1234},
        //                             .cycles        = 0x14,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x5c, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x84}, .expect =
        //                                 Registers{.si = 0x84, .ip = 3, .ds = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x5d, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x84}, .expect =
        //                                 Registers{.di = 0x84, .ip = 3, .ds = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x5e, 0x10},
        //                             .memop= MemoryOp{.address = 0x2020, .data = {0x34,
        //                             0x12}}, .init = Registers{.bp = 0x2010}, .expect =
        //                                 Registers{.bp = 0x2010, .ip = 3, .ds = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x5f, 0x10},
        //                             .memop= MemoryOp{.address = 0x94, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x84}, .expect =
        //                                 Registers{.bx = 0x84, .ip = 3, .ds = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x8c, 0x80, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .ip = 4, .es
        //                                 = 0x1234},
        //                             .cycles        = 0x14,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x81, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .ip = 4, .es
        //                                 = 0x1234},
        //                             .cycles        = 0x15,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x82, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .ip = 4, .es
        //                                 = 0x1234},
        //                             .cycles        = 0x15,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x83, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.di = 0x04, .bp = 0x80, .ip = 4, .es
        //                                 = 0x1234},
        //                             .cycles        = 0x14,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x84, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x84}, .expect =
        //                                 Registers{.si = 0x84, .ip = 4, .es = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x85, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x84}, .expect =
        //                                 Registers{.di = 0x84, .ip = 4, .es = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x86, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x4020, .data = {0x34,
        //                             0x12}}, .init = Registers{.bp = 0x2010}, .expect =
        //                                 Registers{.bp = 0x2010, .ip = 4, .es = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x87, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x84}, .expect =
        //                                 Registers{.bx = 0x84, .ip = 4, .es = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x8c, 0x88, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .ip = 4, .cs
        //                                 = 0x1234},
        //                             .cycles        = 0x14,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x89, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .ip = 4, .cs
        //                                 = 0x1234},
        //                             .cycles        = 0x15,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x8a, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .ip = 4, .cs
        //                                 = 0x1234},
        //                             .cycles        = 0x15,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x8b, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.di = 0x04, .bp = 0x80, .ip = 4, .cs
        //                                 = 0x1234},
        //                             .cycles        = 0x14,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x8c, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x84}, .expect =
        //                                 Registers{.si = 0x84, .ip = 4, .cs = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x8d, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x84}, .expect =
        //                                 Registers{.di = 0x84, .ip = 4, .cs = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x8e, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x4020, .data = {0x34,
        //                             0x12}}, .init = Registers{.bp = 0x2010}, .expect =
        //                                 Registers{.bp = 0x2010, .ip = 4, .cs = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x8f, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x84}, .expect =
        //                                 Registers{.bx = 0x84, .ip = 4, .cs = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x8c, 0x90, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .ip = 4, .ss
        //                                 = 0x1234},
        //                             .cycles        = 0x14,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x91, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .ip = 4, .ss
        //                                 = 0x1234},
        //                             .cycles        = 0x15,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x92, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .ip = 4, .ss
        //                                 = 0x1234},
        //                             .cycles        = 0x15,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x93, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.di = 0x04, .bp = 0x80, .ip = 4, .ss
        //                                 = 0x1234},
        //                             .cycles        = 0x14,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x94, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x84}, .expect =
        //                                 Registers{.si = 0x84, .ip = 4, .ss = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x95, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x84}, .expect =
        //                                 Registers{.di = 0x84, .ip = 4, .ss = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x96, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x4020, .data = {0x34,
        //                             0x12}}, .init = Registers{.bp = 0x2010}, .expect =
        //                                 Registers{.bp = 0x2010, .ip = 4, .ss = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x97, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x84}, .expect =
        //                                 Registers{.bx = 0x84, .ip = 4, .ss = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x8c, 0x98, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .ip = 4, .ds
        //                                 = 0x1234},
        //                             .cycles        = 0x14,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x99, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .ip = 4, .ds
        //                                 = 0x1234},
        //                             .cycles        = 0x15,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x9a, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .ip = 4, .ds
        //                                 = 0x1234},
        //                             .cycles        = 0x15,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x9b, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x04, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.di = 0x04, .bp = 0x80, .ip = 4, .ds
        //                                 = 0x1234},
        //                             .cycles        = 0x14,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x9c, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.si = 0x84}, .expect =
        //                                 Registers{.si = 0x84, .ip = 4, .ds = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x9d, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.di = 0x84}, .expect =
        //                                 Registers{.di = 0x84, .ip = 4, .ds = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x9e, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x4020, .data = {0x34,
        //                             0x12}}, .init = Registers{.bp = 0x2010}, .expect =
        //                                 Registers{.bp = 0x2010, .ip = 4, .ds = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add(
        //                         {
        //                             .cmd  = {0x8c, 0x9f, 0x10, 0x20},
        //                             .memop= MemoryOp{.address = 0x2094, .data = {0x34,
        //                             0x12}}, .init = Registers{.bx = 0x84}, .expect =
        //                                 Registers{.bx = 0x84, .ip = 4, .ds = 0x1234},
        //                             .cycles        = 0x12,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xc0},
        //                             .init = Registers{.ax = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .ip = 2, .es =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xc1},
        //                             .init = Registers{.cx = 0x1234},
        //                             .expect = Registers{.cx = 0x1234, .ip = 2, .es =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xc2},
        //                             .init = Registers{.dx = 0x1234},
        //                             .expect = Registers{.dx = 0x1234, .ip = 2, .es =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xc3},
        //                             .init = Registers{.bx = 0x1234},
        //                             .expect = Registers{.bx = 0x1234, .ip = 2, .es =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xc4},
        //                             .init = Registers{.sp = 0x1234},
        //                             .expect = Registers{.sp = 0x1234, .ip = 2, .es =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xc5},
        //                             .init = Registers{.bp = 0x1234},
        //                             .expect = Registers{.bp = 0x1234, .ip = 2, .es =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xc6},
        //                             .init = Registers{.si = 0x1234},
        //                             .expect = Registers{.si = 0x1234, .ip = 2, .es =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xc7},
        //                             .init = Registers{.di = 0x1234},
        //                             .expect = Registers{.di = 0x1234, .ip = 2, .es =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xc8},
        //                             .init = Registers{.ax = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .ip = 2, .cs =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xc9},
        //                             .init = Registers{.cx = 0x1234},
        //                             .expect = Registers{.cx = 0x1234, .ip = 2, .cs =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xca},
        //                             .init = Registers{.dx = 0x1234},
        //                             .expect = Registers{.dx = 0x1234, .ip = 2, .cs =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xcb},
        //                             .init = Registers{.bx = 0x1234},
        //                             .expect = Registers{.bx = 0x1234, .ip = 2, .cs =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xcc},
        //                             .init = Registers{.sp = 0x1234},
        //                             .expect = Registers{.sp = 0x1234, .ip = 2, .cs =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xcd},
        //                             .init = Registers{.bp = 0x1234},
        //                             .expect = Registers{.bp = 0x1234, .ip = 2, .cs =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xce},
        //                             .init = Registers{.si = 0x1234},
        //                             .expect = Registers{.si = 0x1234, .ip = 2, .cs =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xcf},
        //                             .init = Registers{.di = 0x1234},
        //                             .expect = Registers{.di = 0x1234, .ip = 2, .cs =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xd0},
        //                             .init = Registers{.ax = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .ip = 2, .ss =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xd1},
        //                             .init = Registers{.cx = 0x1234},
        //                             .expect = Registers{.cx = 0x1234, .ip = 2, .ss =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xd2},
        //                             .init = Registers{.dx = 0x1234},
        //                             .expect = Registers{.dx = 0x1234, .ip = 2, .ss =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xd3},
        //                             .init = Registers{.bx = 0x1234},
        //                             .expect = Registers{.bx = 0x1234, .ip = 2, .ss =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xd4},
        //                             .init = Registers{.sp = 0x1234},
        //                             .expect = Registers{.sp = 0x1234, .ip = 2, .ss =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xd5},
        //                             .init = Registers{.bp = 0x1234},
        //                             .expect = Registers{.bp = 0x1234, .ip = 2, .ss =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xd6},
        //                             .init = Registers{.si = 0x1234},
        //                             .expect = Registers{.si = 0x1234, .ip = 2, .ss =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xd7},
        //                             .init = Registers{.di = 0x1234},
        //                             .expect = Registers{.di = 0x1234, .ip = 2, .ss =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xd8},
        //                             .init = Registers{.ax = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .ip = 2, .ds =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xd9},
        //                             .init = Registers{.cx = 0x1234},
        //                             .expect = Registers{.cx = 0x1234, .ip = 2, .ds =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xda},
        //                             .init = Registers{.dx = 0x1234},
        //                             .expect = Registers{.dx = 0x1234, .ip = 2, .ds =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xdb},
        //                             .init = Registers{.bx = 0x1234},
        //                             .expect = Registers{.bx = 0x1234, .ip = 2, .ds =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xdc},
        //                             .init = Registers{.sp = 0x1234},
        //                             .expect = Registers{.sp = 0x1234, .ip = 2, .ds =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xdd},
        //                             .init = Registers{.bp = 0x1234},
        //                             .expect = Registers{.bp = 0x1234, .ip = 2, .ds =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xde},
        //                             .init = Registers{.si = 0x1234},
        //                             .expect = Registers{.si = 0x1234, .ip = 2, .ds =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                         add({
        //                             .cmd = {0x8c, 0xdf},
        //                             .init = Registers{.di = 0x1234},
        //                             .expect = Registers{.di = 0x1234, .ip = 2, .ds =
        //                             0x1234}, .cycles = 0x02,
        //                         }),
        //                 }
        //         },
        //         MovFromMemToRegParams{
        //             .name = "0x8e",
        //             .data = {
        //                 add({
        //                     .cmd  = {0x8e, 0x00},
        //                     .init = Registers{.bx = 0x80, .si = 0x04, .es = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .si = 0x04, .ip = 2, .es =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x10,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x01},
        //                     .init = Registers{.bx = 0x80, .di = 0x04, .es = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .di = 0x04, .ip = 2, .es =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x11,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x02},
        //                     .init = Registers{.si = 0x04, .bp = 0x80, .es = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x04, .bp = 0x80, .ip = 2, .es =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x11,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x03},
        //                     .init = Registers{.di = 0x04, .bp = 0x80, .es = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x04, .bp = 0x80, .ip = 2, .es =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x10,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x04},
        //                     .init = Registers{.si = 0x84, .es = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x84, .ip = 2, .es = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x0e,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x05},
        //                     .init = Registers{.di = 0x84, .es = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x84, .ip = 2, .es = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x0e,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x06, 0x01, 0x08},
        //                     .init = Registers{.es = 0x1234},
        //                     .expect =
        //                         Registers{.ip = 4, .es = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x801, .data = {0x34,
        //                     0x12}}, .cycles        = 0x0f,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x07},
        //                     .init = Registers{.bx = 0x84, .es = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x84, .ip = 2, .es = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x0e,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x08},
        //                     .init = Registers{.bx = 0x80, .si = 0x04, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .si = 0x04, .ip = 2, .cs =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x10,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x09},
        //                     .init = Registers{.bx = 0x80, .di = 0x04, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .di = 0x04, .ip = 2, .cs =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x11,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x0a},
        //                     .init = Registers{.si = 0x04, .bp = 0x80, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x04, .bp = 0x80, .ip = 2, .cs =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x11,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x0b},
        //                     .init = Registers{.di = 0x04, .bp = 0x80, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x04, .bp = 0x80, .ip = 2, .cs =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x10,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x0c},
        //                     .init = Registers{.si = 0x84, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x84, .ip = 2, .cs = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x0e,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x0d},
        //                     .init = Registers{.di = 0x84, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x84, .ip = 2, .cs = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x0e,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x0e, 0x01, 0x08},
        //                     .init = Registers{.cs = 0x1234},
        //                     .expect =
        //                         Registers{.ip = 4, .cs = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x801, .data = {0x34,
        //                     0x12}}, .cycles        = 0x0f,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x0f},
        //                     .init = Registers{.bx = 0x84, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x84, .ip = 2, .cs = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x0e,
        //                 }),

        //                 add({
        //                     .cmd  = {0x8e, 0x10},
        //                     .init = Registers{.bx = 0x80, .si = 0x04, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .si = 0x04, .ip = 2, .ss =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x10,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x11},
        //                     .init = Registers{.bx = 0x80, .di = 0x04, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .di = 0x04, .ip = 2, .ss =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x11,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x12},
        //                     .init = Registers{.si = 0x04, .bp = 0x80, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x04, .bp = 0x80, .ip = 2, .ss =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x11,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x13},
        //                     .init = Registers{.di = 0x04, .bp = 0x80, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x04, .bp = 0x80, .ip = 2, .ss =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x10,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x14},
        //                     .init = Registers{.si = 0x84, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x84, .ip = 2, .ss = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x0e,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x15},
        //                     .init = Registers{.di = 0x84, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x84, .ip = 2, .ss = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x0e,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x16, 0x01, 0x08},
        //                     .init = Registers{.ss = 0x1234},
        //                     .expect =
        //                         Registers{.ip = 4, .ss = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x801, .data = {0x34,
        //                     0x12}}, .cycles        = 0x0f,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x17},
        //                     .init = Registers{.bx = 0x84, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x84, .ip = 2, .ss = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x0e,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x18},
        //                     .init = Registers{.bx = 0x80, .si = 0x04, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .si = 0x04, .ip = 2, .ds =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x10,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x19},
        //                     .init = Registers{.bx = 0x80, .di = 0x04, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .di = 0x04, .ip = 2, .ds =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x11,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x1a},
        //                     .init = Registers{.si = 0x04, .bp = 0x80, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x04, .bp = 0x80, .ip = 2, .ds =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x11,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x1b},
        //                     .init = Registers{.di = 0x04, .bp = 0x80, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x04, .bp = 0x80, .ip = 2, .ds =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x10,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x1c},
        //                     .init = Registers{.si = 0x84, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x84, .ip = 2, .ds = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x0e,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x1d},
        //                     .init = Registers{.di = 0x84, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x84, .ip = 2, .ds = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x0e,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x1e, 0x01, 0x08},
        //                     .init = Registers{.ds = 0x1234},
        //                     .expect =
        //                         Registers{.ip = 4, .ds = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x801, .data = {0x34,
        //                     0x12}}, .cycles        = 0x0f,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x1f},
        //                     .init = Registers{.bx = 0x84, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x84, .ip = 2, .ds = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x84, .data = {0x34,
        //                     0x12}}, .cycles        = 0x0e,
        //                 }),

        //                 add({
        //                     .cmd  = {0x8e, 0x40, 0x01},
        //                     .init = Registers{.bx = 0x80, .si = 0x04, .es = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .si = 0x04, .ip = 3, .es =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x85, .data = {0x34,
        //                     0x12}}, .cycles        = 0x14,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x41, 0x10},
        //                     .init = Registers{.bx = 0x80, .di = 0x04, .es = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .di = 0x04, .ip = 3, .es =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x15,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x42, 0x10},
        //                     .init = Registers{.si = 0x04, .bp = 0x80, .es = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x04, .bp = 0x80, .ip = 3, .es =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x15,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x43, 0x10},
        //                     .init = Registers{.di = 0x04, .bp = 0x80, .es = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x04, .bp = 0x80, .ip = 3, .es =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x14,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x44, 0x10},
        //                     .init = Registers{.si = 0x84, .es = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x84, .ip = 3, .es = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x45, 0x10},
        //                     .init = Registers{.di = 0x84, .es = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x84, .ip = 3, .es = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x46, 0x10},
        //                     .init = Registers{.bp = 0x801, .es = 0x1234},
        //                     .expect =
        //                         Registers{.bp = 0x801, .ip = 3, .es = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x811, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x47, 0x10},
        //                     .init = Registers{.bx = 0x84, .es = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x84, .ip = 3, .es = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x48, 0x10},
        //                     .init = Registers{.bx = 0x80, .si = 0x04, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .si = 0x04, .ip = 3, .cs =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x14,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x49, 0x10},
        //                     .init = Registers{.bx = 0x80, .di = 0x04, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .di = 0x04, .ip = 3, .cs =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x15,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x4a, 0x10},
        //                     .init = Registers{.si = 0x04, .bp = 0x80, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x04, .bp = 0x80, .ip = 3, .cs =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x15,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x4b, 0x10},
        //                     .init = Registers{.di = 0x04, .bp = 0x80, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x04, .bp = 0x80, .ip = 3, .cs =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x14,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x4c, 0x10},
        //                     .init = Registers{.si = 0x84, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x84, .ip = 3, .cs = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x4d, 0x10},
        //                     .init = Registers{.di = 0x84, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x84, .ip = 3, .cs = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x4e, 0x10},
        //                     .init = Registers{.bp = 0x801, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.bp = 0x801, .ip = 3, .cs = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x811, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x4f, 0x10},
        //                     .init = Registers{.bx = 0x84, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x84, .ip = 3, .cs = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x50, 0x01},
        //                     .init = Registers{.bx = 0x80, .si = 0x04, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .si = 0x04, .ip = 3, .ss =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x85, .data = {0x34,
        //                     0x12}}, .cycles        = 0x14,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x51, 0x10},
        //                     .init = Registers{.bx = 0x80, .di = 0x04, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .di = 0x04, .ip = 3, .ss =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x15,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x52, 0x10},
        //                     .init = Registers{.si = 0x04, .bp = 0x80, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x04, .bp = 0x80, .ip = 3, .ss =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x15,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x53, 0x10},
        //                     .init = Registers{.di = 0x04, .bp = 0x80, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x04, .bp = 0x80, .ip = 3, .ss =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x14,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x54, 0x10},
        //                     .init = Registers{.si = 0x84, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x84, .ip = 3, .ss = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x55, 0x10},
        //                     .init = Registers{.di = 0x84, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x84, .ip = 3, .ss = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x56, 0x10},
        //                     .init = Registers{.bp = 0x801, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.bp = 0x801, .ip = 3, .ss = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x811, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x57, 0x10},
        //                     .init = Registers{.bx = 0x84, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x84, .ip = 3, .ss = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x58, 0x10},
        //                     .init = Registers{.bx = 0x80, .si = 0x04, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .si = 0x04, .ip = 3, .ds =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x14,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x59, 0x10},
        //                     .init = Registers{.bx = 0x80, .di = 0x04, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .di = 0x04, .ip = 3, .ds =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x15,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x5a, 0x10},
        //                     .init = Registers{.si = 0x04, .bp = 0x80, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x04, .bp = 0x80, .ip = 3, .ds =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x15,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x5b, 0x10},
        //                     .init = Registers{.di = 0x04, .bp = 0x80, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x04, .bp = 0x80, .ip = 3, .ds =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x14,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x5c, 0x10},
        //                     .init = Registers{.si = 0x84, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x84, .ip = 3, .ds = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x5d, 0x10},
        //                     .init = Registers{.di = 0x84, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x84, .ip = 3, .ds = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x5e, 0x10},
        //                     .init = Registers{.bp = 0x801, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.bp = 0x801, .ip = 3, .ds = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x811, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x5f, 0x10},
        //                     .init = Registers{.bx = 0x84, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x84, .ip = 3, .ds = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x94, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),

        //                 add({
        //                     .cmd  = {0x8e, 0x80, 0x01, 0x10},
        //                     .init = Registers{.bx = 0x80, .si = 0x04, .es = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .si = 0x04, .ip = 4, .es =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1085, .data = {0x34,
        //                     0x12}}, .cycles        = 0x14,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x81, 0x10, 0x10},
        //                     .init = Registers{.bx = 0x80, .di = 0x04, .es = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .di = 0x04, .ip = 4, .es =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x15,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x82, 0x10, 0x10},
        //                     .init = Registers{.si = 0x04, .bp = 0x80, .es = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x04, .bp = 0x80, .ip = 4, .es =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x15,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x83, 0x10, 0x10},
        //                     .init = Registers{.di = 0x04, .bp = 0x80, .es = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x04, .bp = 0x80, .ip = 4, .es =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x14,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x84, 0x10, 0x10},
        //                     .init = Registers{.si = 0x84, .es = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x84, .ip = 4, .es = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x85, 0x10, 0x10},
        //                     .init = Registers{.di = 0x84, .es = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x84, .ip = 4, .es = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x86, 0x10, 0x10},
        //                     .init = Registers{.bp = 0x801, .es = 0x1234},
        //                     .expect =
        //                         Registers{.bp = 0x801, .ip = 4, .es = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1811, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x87, 0x10, 0x10},
        //                     .init = Registers{.bx = 0x84, .es = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x84, .ip = 4, .es = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x88, 0x10, 0x10},
        //                     .init = Registers{.bx = 0x80, .si = 0x04, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .si = 0x04, .ip = 4, .cs =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x14,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x89, 0x10, 0x10},
        //                     .init = Registers{.bx = 0x80, .di = 0x04, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .di = 0x04, .ip = 4, .cs =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x15,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x8a, 0x10, 0x10},
        //                     .init = Registers{.si = 0x04, .bp = 0x80, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x04, .bp = 0x80, .ip = 4, .cs =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x15,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x8b, 0x10, 0x10},
        //                     .init = Registers{.di = 0x04, .bp = 0x80, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x04, .bp = 0x80, .ip = 4, .cs =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x14,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x8c, 0x10, 0x10},
        //                     .init = Registers{.si = 0x84, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x84, .ip = 4, .cs = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x8d, 0x10, 0x10},
        //                     .init = Registers{.di = 0x84, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x84, .ip = 4, .cs = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x8e, 0x10, 0x10},
        //                     .init = Registers{.bp = 0x801, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.bp = 0x801, .ip = 4, .cs = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1811, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x8f, 0x10, 0x10},
        //                     .init = Registers{.bx = 0x84, .cs = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x84, .ip = 4, .cs = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x90, 0x01, 0x10},
        //                     .init = Registers{.bx = 0x80, .si = 0x04, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .si = 0x04, .ip = 4, .ss =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1085, .data = {0x34,
        //                     0x12}}, .cycles        = 0x14,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x91, 0x10, 0x10},
        //                     .init = Registers{.bx = 0x80, .di = 0x04, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .di = 0x04, .ip = 4, .ss =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x15,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x92, 0x10, 0x10},
        //                     .init = Registers{.si = 0x04, .bp = 0x80, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x04, .bp = 0x80, .ip = 4, .ss =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x15,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x93, 0x10, 0x10},
        //                     .init = Registers{.di = 0x04, .bp = 0x80, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x04, .bp = 0x80, .ip = 4, .ss =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x14,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x94, 0x10, 0x10},
        //                     .init = Registers{.si = 0x84, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x84, .ip = 4, .ss = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x95, 0x10, 0x10},
        //                     .init = Registers{.di = 0x84, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x84, .ip = 4, .ss = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x96, 0x10, 0x10},
        //                     .init = Registers{.bp = 0x801, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.bp = 0x801, .ip = 4, .ss = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1811, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x97, 0x10, 0x10},
        //                     .init = Registers{.bx = 0x84, .ss = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x84, .ip = 4, .ss = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x98, 0x10, 0x10},
        //                     .init = Registers{.bx = 0x80, .si = 0x04, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .si = 0x04, .ip = 4, .ds =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x14,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x99, 0x10, 0x10},
        //                     .init = Registers{.bx = 0x80, .di = 0x04, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x80, .di = 0x04, .ip = 4, .ds =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x15,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x9a, 0x10, 0x10},
        //                     .init = Registers{.si = 0x04, .bp = 0x80, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x04, .bp = 0x80, .ip = 4, .ds =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x15,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x9b, 0x10, 0x10},
        //                     .init = Registers{.di = 0x04, .bp = 0x80, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x04, .bp = 0x80, .ip = 4, .ds =
        //                         0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x14,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x9c, 0x10, 0x10},
        //                     .init = Registers{.si = 0x84, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.si = 0x84, .ip = 4, .ds = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x9d, 0x10, 0x10},
        //                     .init = Registers{.di = 0x84, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.di = 0x84, .ip = 4, .ds = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x9e, 0x10, 0x10},
        //                     .init = Registers{.bp = 0x801, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.bp = 0x801, .ip = 4, .ds = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1811, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),
        //                 add({
        //                     .cmd  = {0x8e, 0x9f, 0x10, 0x10},
        //                     .init = Registers{.bx = 0x84, .ds = 0x1234},
        //                     .expect =
        //                         Registers{.bx = 0x84, .ip = 4, .ds = 0x1234},
        //                     .expect_memory = MemoryOp{.address = 0x1094, .data = {0x34,
        //                     0x12}}, .cycles        = 0x12,
        //                 }),

        //                 add({
        //                     .cmd = {0x8e, 0xc0},
        //                     .init = Registers{.es = 0x1234},
        //                     .expect = Registers{.ax = 0x1234, .ip = 2, .es = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xc1},
        //                     .init = Registers{.es = 0x1234},
        //                     .expect = Registers{.cx = 0x1234, .ip = 2, .es = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xc2},
        //                     .init = Registers{.es = 0x1234},
        //                     .expect = Registers{.dx = 0x1234, .ip = 2, .es = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xc3},
        //                     .init = Registers{.es = 0x1234},
        //                     .expect = Registers{.bx = 0x1234, .ip = 2, .es = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xc4},
        //                     .init = Registers{.es = 0x1234},
        //                     .expect = Registers{.sp = 0x1234, .ip = 2, .es = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xc5},
        //                     .init = Registers{.es = 0x1234},
        //                     .expect = Registers{.bp = 0x1234, .ip = 2, .es = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xc6},
        //                     .init = Registers{.es = 0x1234},
        //                     .expect = Registers{.si = 0x1234, .ip = 2, .es = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xc7},
        //                     .init = Registers{.es = 0x1234},
        //                     .expect = Registers{.di = 0x1234, .ip = 2, .es = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xc8},
        //                     .init = Registers{.cs = 0x1234},
        //                     .expect = Registers{.ax = 0x1234, .ip = 2, .cs = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xc9},
        //                     .init = Registers{.cs = 0x1234},
        //                     .expect = Registers{.cx = 0x1234, .ip = 2, .cs = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xca},
        //                     .init = Registers{.cs = 0x1234},
        //                     .expect = Registers{.dx = 0x1234, .ip = 2, .cs = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xcb},
        //                     .init = Registers{.cs = 0x1234},
        //                     .expect = Registers{.bx = 0x1234, .ip = 2, .cs = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xcc},
        //                     .init = Registers{.cs = 0x1234},
        //                     .expect = Registers{.sp = 0x1234, .ip = 2, .cs = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xcd},
        //                     .init = Registers{.cs = 0x1234},
        //                     .expect = Registers{.bp = 0x1234, .ip = 2, .cs = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xce},
        //                     .init = Registers{.cs = 0x1234},
        //                     .expect = Registers{.si = 0x1234, .ip = 2, .cs = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xcf},
        //                     .init = Registers{.cs = 0x1234},
        //                     .expect = Registers{.di = 0x1234, .ip = 2, .cs = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xd0},
        //                     .init = Registers{.ss = 0x1234},
        //                     .expect = Registers{.ax = 0x1234, .ip = 2, .ss = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xd1},
        //                     .init = Registers{.ss = 0x1234},
        //                     .expect = Registers{.cx = 0x1234, .ip = 2, .ss = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xd2},
        //                     .init = Registers{.ss = 0x1234},
        //                     .expect = Registers{.dx = 0x1234, .ip = 2, .ss = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xd3},
        //                     .init = Registers{.ss = 0x1234},
        //                     .expect = Registers{.bx = 0x1234, .ip = 2, .ss = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xd4},
        //                     .init = Registers{.ss = 0x1234},
        //                     .expect = Registers{.sp = 0x1234, .ip = 2, .ss = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xd5},
        //                     .init = Registers{.ss = 0x1234},
        //                     .expect = Registers{.bp = 0x1234, .ip = 2, .ss = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xd6},
        //                     .init = Registers{.ss = 0x1234},
        //                     .expect = Registers{.si = 0x1234, .ip = 2, .ss = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xd7},
        //                     .init = Registers{.ss = 0x1234},
        //                     .expect = Registers{.di = 0x1234, .ip = 2, .ss = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xd8},
        //                     .init = Registers{.ds = 0x1234},
        //                     .expect = Registers{.ax = 0x1234, .ip = 2, .ds = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xd9},
        //                     .init = Registers{.ds = 0x1234},
        //                     .expect = Registers{.cx = 0x1234, .ip = 2, .ds = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xda},
        //                     .init = Registers{.ds = 0x1234},
        //                     .expect = Registers{.dx = 0x1234, .ip = 2, .ds = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xdb},
        //                     .init = Registers{.ds = 0x1234},
        //                     .expect = Registers{.bx = 0x1234, .ip = 2, .ds = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xdc},
        //                     .init = Registers{.ds = 0x1234},
        //                     .expect = Registers{.sp = 0x1234, .ip = 2, .ds = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xdd},
        //                     .init = Registers{.ds = 0x1234},
        //                     .expect = Registers{.bp = 0x1234, .ip = 2, .ds = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xde},
        //                     .init = Registers{.ds = 0x1234},
        //                     .expect = Registers{.si = 0x1234, .ip = 2, .ds = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //                 add({
        //                     .cmd = {0x8e, 0xdf},
        //                     .init = Registers{.ds = 0x1234},
        //                     .expect = Registers{.di = 0x1234, .ip = 2, .ds = 0x1234},
        //                     .cycles = 0x02,
        //                 }),
        //             }
        //         },
        //         MovFromMemToRegParams{
        //             .name = "0x88",
        //             .data =
        //                 {
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x00},
        //                             .init = Registers{.ax = 0x3a, .bx = 0x80, .si =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a, .bx = 0x80, .si = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x01},
        //                             .init = Registers{.ax = 0x3a, .bx = 0x80, .di =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a, .bx = 0x80, .di = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x02},
        //                             .init = Registers{.ax = 0x3a, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.ax = 0x3a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x03},
        //                             .init = Registers{.ax = 0x3a, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x04},
        //                             .init          = Registers{.ax = 0x3a, .si = 0xff},
        //                             .expect        = Registers{.ax = 0x3a, .si = 0xff,
        //                             .ip = 2}, .expect_memory = MemoryOp{.address =
        //                             0xff, .data = {0x3a}}, .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x05},
        //                             .init          = Registers{.ax = 0x3a, .di = 0x84},
        //                             .expect        = Registers{.ax = 0x3a, .di = 0x84,
        //                             .ip = 2}, .expect_memory = MemoryOp{.address =
        //                             0x84, .data = {0x3a}}, .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x06, 0x11, 0x20},
        //                             .init          = Registers{.ax = 0x3a},
        //                             .expect        = Registers{.ax = 0x3a, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2011, .data
        //                             = {0x3a}}, .cycles        = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x07},
        //                             .init          = Registers{.ax = 0x3a, .bx = 0xab},
        //                             .expect        = Registers{.ax = 0x3a, .bx = 0xab,
        //                             .ip = 2}, .expect_memory = MemoryOp{.address =
        //                             0xab, .data = {0x3a}}, .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x08},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a, .si = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x09},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a, .di = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x0a},
        //                             .init = Registers{.cx = 0x3a, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.cx = 0x3a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x0b},
        //                             .init = Registers{.cx = 0x3a, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.cx = 0x3a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x0c},
        //                             .init          = Registers{.cx = 0x3a, .si = 0xff},
        //                             .expect        = Registers{.cx = 0x3a, .si = 0xff,
        //                             .ip = 2}, .expect_memory = MemoryOp{.address =
        //                             0xff, .data = {0x3a}}, .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x0d},
        //                             .init          = Registers{.cx = 0x3a, .di = 0x84},
        //                             .expect        = Registers{.cx = 0x3a, .di = 0x84,
        //                             .ip = 2}, .expect_memory = MemoryOp{.address =
        //                             0x84, .data = {0x3a}}, .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x0e, 0x11, 0x20},
        //                             .init          = Registers{.cx = 0x3a},
        //                             .expect        = Registers{.cx = 0x3a, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2011, .data
        //                             = {0x3a}}, .cycles        = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x0f},
        //                             .init          = Registers{.bx = 0xab, .cx = 0x3a},
        //                             .expect        = Registers{.bx = 0xab, .cx = 0x3a,
        //                             .ip = 2}, .expect_memory = MemoryOp{.address =
        //                             0xab, .data = {0x3a}}, .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x10},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a, .si = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x11},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a, .di = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x12},
        //                             .init = Registers{.dx = 0x3a, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.dx = 0x3a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x13},
        //                             .init = Registers{.dx = 0x3a, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.dx = 0x3a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x14},
        //                             .init          = Registers{.dx = 0x3a, .si = 0xff},
        //                             .expect        = Registers{.dx = 0x3a, .si = 0xff,
        //                             .ip = 2}, .expect_memory = MemoryOp{.address =
        //                             0xff, .data = {0x3a}}, .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x15},
        //                             .init          = Registers{.dx = 0x3a, .di = 0x84},
        //                             .expect        = Registers{.dx = 0x3a, .di = 0x84,
        //                             .ip = 2}, .expect_memory = MemoryOp{.address =
        //                             0x84, .data = {0x3a}}, .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x16, 0x11, 0x20},
        //                             .init          = Registers{.dx = 0x3a},
        //                             .expect        = Registers{.dx = 0x3a, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2011, .data
        //                             = {0x3a}}, .cycles        = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x17},
        //                             .init          = Registers{.bx = 0xab, .dx = 0x3a},
        //                             .expect        = Registers{.bx = 0xab, .dx = 0x3a,
        //                             .ip = 2}, .expect_memory = MemoryOp{.address =
        //                             0xab, .data = {0x3a}}, .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x18},
        //                             .init          = Registers{.bx = 0x3a, .si = 0x04},
        //                             .expect        = Registers{.bx = 0x3a, .si = 0x04,
        //                             .ip = 2}, .expect_memory = MemoryOp{.address =
        //                             0x3e, .data = {0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x19},
        //                             .init          = Registers{.bx = 0x3a, .di = 0x04},
        //                             .expect        = Registers{.bx = 0x3a, .di = 0x04,
        //                             .ip = 2}, .expect_memory = MemoryOp{.address =
        //                             0x3e, .data = {0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x1a},
        //                             .init = Registers{.bx = 0x3a, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.bx = 0x3a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x1b},
        //                             .init = Registers{.bx = 0x3a, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x3a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x1c},
        //                             .init          = Registers{.bx = 0x3a, .si = 0xff},
        //                             .expect        = Registers{.bx = 0x3a, .si = 0xff,
        //                             .ip = 2}, .expect_memory = MemoryOp{.address =
        //                             0xff, .data = {0x3a}}, .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x1d},
        //                             .init          = Registers{.bx = 0x3a, .di = 0x84},
        //                             .expect        = Registers{.bx = 0x3a, .di = 0x84,
        //                             .ip = 2}, .expect_memory = MemoryOp{.address =
        //                             0x84, .data = {0x3a}}, .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x1e, 0x11, 0x20},
        //                             .init          = Registers{.bx = 0x3a},
        //                             .expect        = Registers{.bx = 0x3a, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2011, .data
        //                             = {0x3a}}, .cycles        = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x1f},
        //                             .init          = Registers{.bx = 0x3a},
        //                             .expect        = Registers{.bx = 0x3a, .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x3a, .data =
        //                             {0x3a}}, .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x20},
        //                             .init = Registers{.ax = 0x3a00, .bx = 0x80, .si =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a00, .bx = 0x80, .si = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x21},
        //                             .init = Registers{.ax = 0x3a00, .bx = 0x80, .di =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a00, .bx = 0x80, .di = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x22},
        //                             .init = Registers{.ax = 0x3a00, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.ax = 0x3a00, .si = 0x04, .bp = 0x80,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x23},
        //                             .init = Registers{.ax = 0x3a00, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a00, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x24},
        //                             .init          = Registers{.ax = 0x3a00, .si =
        //                             0xff}, .expect        = Registers{.ax = 0x3a00, .si
        //                             = 0xff, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xff, .data = {0x3a}}, .cycles
        //                             = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x25},
        //                             .init          = Registers{.ax = 0x3a00, .di =
        //                             0x84}, .expect        = Registers{.ax = 0x3a00, .di
        //                             = 0x84, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0x84, .data = {0x3a}}, .cycles
        //                             = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x26, 0x11, 0x20},
        //                             .init          = Registers{.ax = 0x3a00},
        //                             .expect        = Registers{.ax = 0x3a00, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2011, .data
        //                             = {0x3a}}, .cycles        = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x27},
        //                             .init          = Registers{.ax = 0x3a00, .bx =
        //                             0xab}, .expect        = Registers{.ax = 0x3a00, .bx
        //                             = 0xab, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xab, .data = {0x3a}}, .cycles
        //                             = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x28},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a00, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a00, .si = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x29},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a00, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a00, .di = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x2a},
        //                             .init = Registers{.cx = 0x3a00, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.cx = 0x3a00, .si = 0x04, .bp = 0x80,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x2b},
        //                             .init = Registers{.cx = 0x3a00, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.cx = 0x3a00, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x2c},
        //                             .init          = Registers{.cx = 0x3a00, .si =
        //                             0xff}, .expect        = Registers{.cx = 0x3a00, .si
        //                             = 0xff, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xff, .data = {0x3a}}, .cycles
        //                             = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x2d},
        //                             .init          = Registers{.cx = 0x3a00, .di =
        //                             0x84}, .expect        = Registers{.cx = 0x3a00, .di
        //                             = 0x84, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0x84, .data = {0x3a}}, .cycles
        //                             = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x2e, 0x11, 0x20},
        //                             .init          = Registers{.cx = 0x3a00},
        //                             .expect        = Registers{.cx = 0x3a00, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2011, .data
        //                             = {0x3a}}, .cycles        = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x2f},
        //                             .init          = Registers{.bx = 0xab, .cx =
        //                             0x3a00}, .expect        = Registers{.bx = 0xab, .cx
        //                             = 0x3a00, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xab, .data = {0x3a}}, .cycles
        //                             = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x30},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a00, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a00, .si = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x31},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a00, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a00, .di = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x32},
        //                             .init = Registers{.dx = 0x3a00, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.dx = 0x3a00, .si = 0x04, .bp = 0x80,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x33},
        //                             .init = Registers{.dx = 0x3a00, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.dx = 0x3a00, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x34},
        //                             .init          = Registers{.dx = 0x3a00, .si =
        //                             0xff}, .expect        = Registers{.dx = 0x3a00, .si
        //                             = 0xff, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xff, .data = {0x3a}}, .cycles
        //                             = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x35},
        //                             .init          = Registers{.dx = 0x3a00, .di =
        //                             0x84}, .expect        = Registers{.dx = 0x3a00, .di
        //                             = 0x84, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0x84, .data = {0x3a}}, .cycles
        //                             = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x36, 0x11, 0x20},
        //                             .init          = Registers{.dx = 0x3a00},
        //                             .expect        = Registers{.dx = 0x3a00, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2011, .data
        //                             = {0x3a}}, .cycles        = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x37},
        //                             .init          = Registers{.bx = 0xab, .dx =
        //                             0x3a00}, .expect        = Registers{.bx = 0xab, .dx
        //                             = 0x3a00, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xab, .data = {0x3a}}, .cycles
        //                             = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x38},
        //                             .init          = Registers{.bx = 0x3a80, .si =
        //                             0x04}, .expect        = Registers{.bx = 0x3a80, .si
        //                             = 0x04, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0x3a84, .data = {0x3a}},
        //                             .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x39},
        //                             .init          = Registers{.bx = 0x3a80, .di =
        //                             0x04}, .expect        = Registers{.bx = 0x3a80, .di
        //                             = 0x04, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0x3a84, .data = {0x3a}},
        //                             .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x3a},
        //                             .init = Registers{.bx = 0x3a00, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.bx = 0x3a00, .si = 0x04, .bp = 0x80,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x3b},
        //                             .init = Registers{.bx = 0x3a00, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x3a00, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x3c},
        //                             .init          = Registers{.bx = 0x3a00, .si =
        //                             0xff}, .expect        = Registers{.bx = 0x3a00, .si
        //                             = 0xff, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xff, .data = {0x3a}}, .cycles
        //                             = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x3d},
        //                             .init          = Registers{.bx = 0x3a00, .di =
        //                             0x84}, .expect        = Registers{.bx = 0x3a00, .di
        //                             = 0x84, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0x84, .data = {0x3a}}, .cycles
        //                             = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x3e, 0x11, 0x20},
        //                             .init          = Registers{.bx = 0x3a00},
        //                             .expect        = Registers{.bx = 0x3a00, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2011, .data
        //                             = {0x3a}}, .cycles        = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x3f},
        //                             .init          = Registers{.bx = 0x3aab},
        //                             .expect        = Registers{.bx = 0x3aab, .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x3aab, .data
        //                             = {0x3a}}, .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x40, 0x20},
        //                             .init = Registers{.ax = 0x3a, .bx = 0x80, .si =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a, .bx = 0x80, .si = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0xa4, .data =
        //                             {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x41, 0x20},
        //                             .init = Registers{.ax = 0x3a, .bx = 0x80, .di =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a, .bx = 0x80, .di = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0xa4, .data =
        //                             {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x42, 0x10},
        //                             .init = Registers{.ax = 0x3a, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.ax = 0x3a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x43, 0x20},
        //                             .init = Registers{.ax = 0x3a, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0xa4, .data =
        //                             {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x44, 0x01},
        //                             .init          = Registers{.ax = 0x3a, .si = 0xff},
        //                             .expect        = Registers{.ax = 0x3a, .si = 0xff,
        //                             .ip = 3}, .expect_memory = MemoryOp{.address =
        //                             0x100, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x45, 0x10},
        //                             .init          = Registers{.ax = 0x3a, .di = 0x84},
        //                             .expect        = Registers{.ax = 0x3a, .di = 0x84,
        //                             .ip = 3}, .expect_memory = MemoryOp{.address =
        //                             0x94, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x46, 0x20},
        //                             .init          = Registers{.ax = 0x3a, .bp = 0x40},
        //                             .expect        = Registers{.ax = 0x3a, .bp = 0x40,
        //                             .ip = 3}, .expect_memory = MemoryOp{.address =
        //                             0x60, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x47, 0x20},
        //                             .init          = Registers{.ax = 0x3a, .bx = 0xab},
        //                             .expect        = Registers{.ax = 0x3a, .bx = 0xab,
        //                             .ip = 3}, .expect_memory = MemoryOp{.address =
        //                             0xcb, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x48, 0x10},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a, .si = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x49, 0x10},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a, .di = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x4a, 0x20},
        //                             .init = Registers{.cx = 0x3a, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.cx = 0x3a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0xa4, .data =
        //                             {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x4b, 0x10},
        //                             .init = Registers{.cx = 0x3a, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.cx = 0x3a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x4c, 0x01},
        //                             .init          = Registers{.cx = 0x3a, .si = 0xff},
        //                             .expect        = Registers{.cx = 0x3a, .si = 0xff,
        //                             .ip = 3}, .expect_memory = MemoryOp{.address =
        //                             0x100, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x4d, 0x10},
        //                             .init          = Registers{.cx = 0x3a, .di = 0x84},
        //                             .expect        = Registers{.cx = 0x3a, .di = 0x84,
        //                             .ip = 3}, .expect_memory = MemoryOp{.address =
        //                             0x94, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x4e, 0x89},
        //                             .init          = Registers{.cx = 0x3a, .bp = 0x10},
        //                             .expect        = Registers{.cx = 0x3a, .bp = 0x10,
        //                             .ip = 3}, .expect_memory = MemoryOp{.address =
        //                             0x99, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x4f, 0x01},
        //                             .init          = Registers{.bx = 0xab, .cx = 0x3a},
        //                             .expect        = Registers{.bx = 0xab, .cx = 0x3a,
        //                             .ip = 3}, .expect_memory = MemoryOp{.address =
        //                             0xac, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x50, 0x10},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a, .si = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x51, 0x10},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a, .di = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x52, 0x20},
        //                             .init = Registers{.dx = 0x3a, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.dx = 0x3a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0xa4, .data =
        //                             {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x53, 0x10},
        //                             .init = Registers{.dx = 0x3a, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.dx = 0x3a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x54, 0x01},
        //                             .init          = Registers{.dx = 0x3a, .si = 0xff},
        //                             .expect        = Registers{.dx = 0x3a, .si = 0xff,
        //                             .ip = 3}, .expect_memory = MemoryOp{.address =
        //                             0x100, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x55, 0x10},
        //                             .init          = Registers{.dx = 0x3a, .di = 0x84},
        //                             .expect        = Registers{.dx = 0x3a, .di = 0x84,
        //                             .ip = 3}, .expect_memory = MemoryOp{.address =
        //                             0x94, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x56, 0x40},
        //                             .init          = Registers{.dx = 0x3a, .bp = 0x20},
        //                             .expect        = Registers{.dx = 0x3a, .bp = 0x20,
        //                             .ip = 3}, .expect_memory = MemoryOp{.address =
        //                             0x60, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x57, 0x1},
        //                             .init          = Registers{.bx = 0xab, .dx = 0x3a},
        //                             .expect        = Registers{.bx = 0xab, .dx = 0x3a,
        //                             .ip = 3}, .expect_memory = MemoryOp{.address =
        //                             0xac, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x58, 0x10},
        //                             .init          = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect        = Registers{.bx = 0x80, .si = 0x04,
        //                             .ip = 3}, .expect_memory = MemoryOp{.address =
        //                             0x94, .data = {0x80}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x59, 0x10},
        //                             .init          = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect        = Registers{.bx = 0x80, .di = 0x04,
        //                             .ip = 3}, .expect_memory = MemoryOp{.address =
        //                             0x94, .data = {0x80}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x5a, 0x20},
        //                             .init = Registers{.bx = 0x3a, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.bx = 0x3a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0xa4, .data =
        //                             {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x5b, 0x4},
        //                             .init = Registers{.bx = 0x3a, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x3a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x88, .data =
        //                             {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x5c, 0x1},
        //                             .init          = Registers{.bx = 0x3a, .si = 0xff},
        //                             .expect        = Registers{.bx = 0x3a, .si = 0xff,
        //                             .ip = 3}, .expect_memory = MemoryOp{.address =
        //                             0x100, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x5d, 0x10},
        //                             .init          = Registers{.bx = 0x3a, .di = 0x84},
        //                             .expect        = Registers{.bx = 0x3a, .di = 0x84,
        //                             .ip = 3}, .expect_memory = MemoryOp{.address =
        //                             0x94, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x5e, 0x25},
        //                             .init          = Registers{.bx = 0x3a, .bp = 0x30},
        //                             .expect        = Registers{.bx = 0x3a, .bp = 0x30,
        //                             .ip = 3}, .expect_memory = MemoryOp{.address =
        //                             0x55, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x5f, 0x01},
        //                             .init          = Registers{.bx = 0xab},
        //                             .expect        = Registers{.bx = 0xab, .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0xac, .data =
        //                             {0xab}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x60, 0x10},
        //                             .init = Registers{.ax = 0x3a00, .bx = 0x80, .si =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a00, .bx = 0x80, .si = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x61, 0x20},
        //                             .init = Registers{.ax = 0x3a00, .bx = 0x80, .di =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a00, .bx = 0x80, .di = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0xa4, .data =
        //                             {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x62, 0x1},
        //                             .init = Registers{.ax = 0x3a00, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.ax = 0x3a00, .si = 0x04, .bp = 0x80,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x85, .data =
        //                             {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x63, 0x10},
        //                             .init = Registers{.ax = 0x3a00, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a00, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x64, 0x1},
        //                             .init          = Registers{.ax = 0x3a00, .si =
        //                             0xff}, .expect        = Registers{.ax = 0x3a00, .si
        //                             = 0xff, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x100, .data = {0x3a}}, .cycles
        //                             = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x65, 0x10},
        //                             .init          = Registers{.ax = 0x3a00, .di =
        //                             0x84}, .expect        = Registers{.ax = 0x3a00, .di
        //                             = 0x84, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x94, .data = {0x3a}}, .cycles
        //                             = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x66, 0x11},
        //                             .init          = Registers{.ax = 0x3a00, .bp =
        //                             0x30}, .expect        = Registers{.ax = 0x3a00, .bp
        //                             = 0x30, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x41, .data = {0x3a}}, .cycles
        //                             = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x67, 0x01},
        //                             .init          = Registers{.ax = 0x3a00, .bx =
        //                             0xab}, .expect        = Registers{.ax = 0x3a00, .bx
        //                             = 0xab, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0xac, .data = {0x3a}}, .cycles
        //                             = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x68, 0x10},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a00, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a00, .si = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x69, 0x01},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a00, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a00, .di = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x85, .data =
        //                             {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x6a, 0x10},
        //                             .init = Registers{.cx = 0x3a00, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.cx = 0x3a00, .si = 0x04, .bp = 0x80,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x6b, 0x20},
        //                             .init = Registers{.cx = 0x3a00, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.cx = 0x3a00, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0xa4, .data =
        //                             {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x6c, 0x01},
        //                             .init          = Registers{.cx = 0x3a00, .si =
        //                             0xff}, .expect        = Registers{.cx = 0x3a00, .si
        //                             = 0xff, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x100, .data = {0x3a}}, .cycles
        //                             = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x6d, 0x10},
        //                             .init          = Registers{.cx = 0x3a00, .di =
        //                             0x84}, .expect        = Registers{.cx = 0x3a00, .di
        //                             = 0x84, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x94, .data = {0x3a}}, .cycles
        //                             = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x6e, 0x20},
        //                             .init          = Registers{.cx = 0x3a00, .bp =
        //                             0x10}, .expect        = Registers{.cx = 0x3a00, .bp
        //                             = 0x10, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x30, .data = {0x3a}}, .cycles
        //                             = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x6f, 0x1},
        //                             .init          = Registers{.bx = 0xab, .cx =
        //                             0x3a00}, .expect        = Registers{.bx = 0xab, .cx
        //                             = 0x3a00, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0xac, .data = {0x3a}}, .cycles
        //                             = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x70, 0x1},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a00, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a00, .si = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x85, .data =
        //                             {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x71, 0x10},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a00, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a00, .di = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x72, 0x10},
        //                             .init = Registers{.dx = 0x3a00, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.dx = 0x3a00, .si = 0x04, .bp = 0x80,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x73, 0x20},
        //                             .init = Registers{.dx = 0x3a00, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.dx = 0x3a00, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0xa4, .data =
        //                             {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x74, 0x01},
        //                             .init          = Registers{.dx = 0x3a00, .si =
        //                             0xff}, .expect        = Registers{.dx = 0x3a00, .si
        //                             = 0xff, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x100, .data = {0x3a}}, .cycles
        //                             = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x75, 0x10},
        //                             .init          = Registers{.dx = 0x3a00, .di =
        //                             0x84}, .expect        = Registers{.dx = 0x3a00, .di
        //                             = 0x84, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x94, .data = {0x3a}}, .cycles
        //                             = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x76, 0x20},
        //                             .init          = Registers{.dx = 0x3a00, .bp =
        //                             0x20}, .expect        = Registers{.dx = 0x3a00, .bp
        //                             = 0x20, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x40, .data = {0x3a}}, .cycles
        //                             = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x77, 0x1},
        //                             .init          = Registers{.bx = 0xab, .dx =
        //                             0x3a00}, .expect        = Registers{.bx = 0xab, .dx
        //                             = 0x3a00, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0xac, .data = {0x3a}}, .cycles
        //                             = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x78, 0x10},
        //                             .init          = Registers{.bx = 0x3a80, .si =
        //                             0x04}, .expect        = Registers{.bx = 0x3a80, .si
        //                             = 0x04, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x3a94, .data = {0x3a}},
        //                             .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x79, 0x20},
        //                             .init          = Registers{.bx = 0x3a80, .di =
        //                             0x04}, .expect        = Registers{.bx = 0x3a80, .di
        //                             = 0x04, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x3aa4, .data = {0x3a}},
        //                             .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x7a, 0x10},
        //                             .init = Registers{.bx = 0x3a00, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.bx = 0x3a00, .si = 0x04, .bp = 0x80,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x7b, 0x10},
        //                             .init = Registers{.bx = 0x3a00, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x3a00, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x7c, 0x01},
        //                             .init          = Registers{.bx = 0x3a00, .si =
        //                             0xff}, .expect        = Registers{.bx = 0x3a00, .si
        //                             = 0xff, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x100, .data = {0x3a}}, .cycles
        //                             = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x7d, 0x01},
        //                             .init          = Registers{.bx = 0x3a00, .di =
        //                             0x84}, .expect        = Registers{.bx = 0x3a00, .di
        //                             = 0x84, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x85, .data = {0x3a}}, .cycles
        //                             = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x7e, 0x20},
        //                             .init          = Registers{.bx = 0x3a00, .bp =
        //                             0x45}, .expect        = Registers{.bx = 0x3a00, .bp
        //                             = 0x45, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x65, .data = {0x3a}}, .cycles
        //                             = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x7f, 0x1},
        //                             .init          = Registers{.bx = 0x3aab},
        //                             .expect        = Registers{.bx = 0x3aab, .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x3aac, .data
        //                             = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                    add(
        //                         {
        //                             .cmd  = {0x88, 0x80, 0x20, 0x10},
        //                             .init = Registers{.ax = 0x3a, .bx = 0x80, .si =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a, .bx = 0x80, .si = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x10a4, .data
        //                             = {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x81, 0x20, 0x10},
        //                             .init = Registers{.ax = 0x3a, .bx = 0x80, .di =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a, .bx = 0x80, .di = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x10a4, .data
        //                             = {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x82, 0x10, 0x10},
        //                             .init = Registers{.ax = 0x3a, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.ax = 0x3a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1094, .data
        //                             = {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x83, 0x20, 0x10},
        //                             .init = Registers{.ax = 0x3a, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x10a4, .data
        //                             = {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x84, 0x01, 0x10},
        //                             .init          = Registers{.ax = 0x3a, .si = 0xff},
        //                             .expect        = Registers{.ax = 0x3a, .si = 0xff,
        //                             .ip = 4}, .expect_memory = MemoryOp{.address =
        //                             0x1100, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x85, 0x10, 0x10},
        //                             .init          = Registers{.ax = 0x3a, .di = 0x84},
        //                             .expect        = Registers{.ax = 0x3a, .di = 0x84,
        //                             .ip = 4}, .expect_memory = MemoryOp{.address =
        //                             0x1094, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x86, 0x20, 0x10},
        //                             .init          = Registers{.ax = 0x3a, .bp = 0x40},
        //                             .expect        = Registers{.ax = 0x3a, .bp = 0x40,
        //                             .ip = 4}, .expect_memory = MemoryOp{.address =
        //                             0x1060, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x87, 0x20, 0x10},
        //                             .init          = Registers{.ax = 0x3a, .bx = 0xab},
        //                             .expect        = Registers{.ax = 0x3a, .bx = 0xab,
        //                             .ip = 4}, .expect_memory = MemoryOp{.address =
        //                             0x10cb, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x88, 0x10, 0x10},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a, .si = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1094, .data
        //                             = {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x89, 0x10, 0x10},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a, .di = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1094, .data
        //                             = {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x8a, 0x20, 0x10},
        //                             .init = Registers{.cx = 0x3a, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.cx = 0x3a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x10a4, .data
        //                             = {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x8b, 0x10, 0x10},
        //                             .init = Registers{.cx = 0x3a, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.cx = 0x3a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1094, .data
        //                             = {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x8c, 0x01, 0x10},
        //                             .init          = Registers{.cx = 0x3a, .si = 0xff},
        //                             .expect        = Registers{.cx = 0x3a, .si = 0xff,
        //                             .ip = 4}, .expect_memory = MemoryOp{.address =
        //                             0x1100, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x8d, 0x10, 0x10},
        //                             .init          = Registers{.cx = 0x3a, .di = 0x84},
        //                             .expect        = Registers{.cx = 0x3a, .di = 0x84,
        //                             .ip = 4}, .expect_memory = MemoryOp{.address =
        //                             0x1094, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x8e, 0x89, 0x10},
        //                             .init          = Registers{.cx = 0x3a, .bp = 0x10},
        //                             .expect        = Registers{.cx = 0x3a, .bp = 0x10,
        //                             .ip = 4}, .expect_memory = MemoryOp{.address =
        //                             0x1099, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x8f, 0x01, 0x10},
        //                             .init          = Registers{.bx = 0xab, .cx = 0x3a},
        //                             .expect        = Registers{.bx = 0xab, .cx = 0x3a,
        //                             .ip = 4}, .expect_memory = MemoryOp{.address =
        //                             0x10ac, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x90, 0x10, 0x10},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a, .si = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1094, .data
        //                             = {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x91, 0x10, 0x10},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a, .di = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1094, .data
        //                             = {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x92, 0x20, 0x10},
        //                             .init = Registers{.dx = 0x3a, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.dx = 0x3a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x10a4, .data
        //                             = {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x93, 0x10, 0x10},
        //                             .init = Registers{.dx = 0x3a, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.dx = 0x3a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1094, .data
        //                             = {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x94, 0x01, 0x10},
        //                             .init          = Registers{.dx = 0x3a, .si = 0xff},
        //                             .expect        = Registers{.dx = 0x3a, .si = 0xff,
        //                             .ip = 4}, .expect_memory = MemoryOp{.address =
        //                             0x1100, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x95, 0x10, 0x10},
        //                             .init          = Registers{.dx = 0x3a, .di = 0x84},
        //                             .expect        = Registers{.dx = 0x3a, .di = 0x84,
        //                             .ip = 4}, .expect_memory = MemoryOp{.address =
        //                             0x1094, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x96, 0x40, 0x10},
        //                             .init          = Registers{.dx = 0x3a, .bp = 0x20},
        //                             .expect        = Registers{.dx = 0x3a, .bp = 0x20,
        //                             .ip = 4}, .expect_memory = MemoryOp{.address =
        //                             0x1060, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x97, 0x1, 0x10},
        //                             .init          = Registers{.bx = 0xab, .dx = 0x3a},
        //                             .expect        = Registers{.bx = 0xab, .dx = 0x3a,
        //                             .ip = 4}, .expect_memory = MemoryOp{.address =
        //                             0x10ac, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x98, 0x10, 0x10},
        //                             .init          = Registers{.bx = 0x80, .si = 0x04},
        //                             .expect        = Registers{.bx = 0x80, .si = 0x04,
        //                             .ip = 4}, .expect_memory = MemoryOp{.address =
        //                             0x1094, .data = {0x80}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x99, 0x10, 0x10},
        //                             .init          = Registers{.bx = 0x80, .di = 0x04},
        //                             .expect        = Registers{.bx = 0x80, .di = 0x04,
        //                             .ip = 4}, .expect_memory = MemoryOp{.address =
        //                             0x1094, .data = {0x80}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x9a, 0x20, 0x10},
        //                             .init = Registers{.bx = 0x3a, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.bx = 0x3a, .si = 0x04, .bp = 0x80,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x10a4, .data
        //                             = {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0x9b, 0x4, 0x10},
        //                             .init = Registers{.bx = 0x3a, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x3a, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1088, .data
        //                             = {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x9c, 0x1, 0x10},
        //                             .init          = Registers{.bx = 0x3a, .si = 0xff},
        //                             .expect        = Registers{.bx = 0x3a, .si = 0xff,
        //                             .ip = 4}, .expect_memory = MemoryOp{.address =
        //                             0x1100, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x9d, 0x10, 0x10},
        //                             .init          = Registers{.bx = 0x3a, .di = 0x84},
        //                             .expect        = Registers{.bx = 0x3a, .di = 0x84,
        //                             .ip = 4}, .expect_memory = MemoryOp{.address =
        //                             0x1094, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x9e, 0x25, 0x10},
        //                             .init          = Registers{.bx = 0x3a, .bp = 0x30},
        //                             .expect        = Registers{.bx = 0x3a, .bp = 0x30,
        //                             .ip = 4}, .expect_memory = MemoryOp{.address =
        //                             0x1055, .data = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0x9f, 0x01, 0x10},
        //                             .init          = Registers{.bx = 0xab},
        //                             .expect        = Registers{.bx = 0xab, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x10ac, .data
        //                             = {0xab}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0xa0, 0x10, 0x10},
        //                             .init = Registers{.ax = 0x3a00, .bx = 0x80, .si =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a00, .bx = 0x80, .si = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1094, .data
        //                             = {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0xa1, 0x20, 0x10},
        //                             .init = Registers{.ax = 0x3a00, .bx = 0x80, .di =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a00, .bx = 0x80, .di = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x10a4, .data
        //                             = {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0xa2, 0x1, 0x10},
        //                             .init = Registers{.ax = 0x3a00, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.ax = 0x3a00, .si = 0x04, .bp = 0x80,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1085, .data
        //                             = {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0xa3, 0x10, 0x10},
        //                             .init = Registers{.ax = 0x3a00, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a00, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1094, .data
        //                             = {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xa4, 0x1, 0x10},
        //                             .init          = Registers{.ax = 0x3a00, .si =
        //                             0xff}, .expect        = Registers{.ax = 0x3a00, .si
        //                             = 0xff, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1100, .data = {0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xa5, 0x10, 0x10},
        //                             .init          = Registers{.ax = 0x3a00, .di =
        //                             0x84}, .expect        = Registers{.ax = 0x3a00, .di
        //                             = 0x84, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1094, .data = {0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xa6, 0x11, 0x10},
        //                             .init          = Registers{.ax = 0x3a00, .bp =
        //                             0x30}, .expect        = Registers{.ax = 0x3a00, .bp
        //                             = 0x30, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1041, .data = {0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xa7, 0x01, 0x10},
        //                             .init          = Registers{.ax = 0x3a00, .bx =
        //                             0xab}, .expect        = Registers{.ax = 0x3a00, .bx
        //                             = 0xab, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x10ac, .data = {0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0xa8, 0x10, 0x10},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a00, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a00, .si = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1094, .data
        //                             = {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0xa9, 0x01, 0x10},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a00, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a00, .di = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1085, .data
        //                             = {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0xaa, 0x10, 0x10},
        //                             .init = Registers{.cx = 0x3a00, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.cx = 0x3a00, .si = 0x04, .bp = 0x80,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1094, .data
        //                             = {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0xab, 0x20, 0x10},
        //                             .init = Registers{.cx = 0x3a00, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.cx = 0x3a00, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x10a4, .data
        //                             = {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xac, 0x01, 0x10},
        //                             .init          = Registers{.cx = 0x3a00, .si =
        //                             0xff}, .expect        = Registers{.cx = 0x3a00, .si
        //                             = 0xff, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1100, .data = {0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xad, 0x10, 0x10},
        //                             .init          = Registers{.cx = 0x3a00, .di =
        //                             0x84}, .expect        = Registers{.cx = 0x3a00, .di
        //                             = 0x84, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1094, .data = {0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xae, 0x20, 0x10},
        //                             .init          = Registers{.cx = 0x3a00, .bp =
        //                             0x10}, .expect        = Registers{.cx = 0x3a00, .bp
        //                             = 0x10, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1030, .data = {0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xaf, 0x1, 0x10},
        //                             .init          = Registers{.bx = 0xab, .cx =
        //                             0x3a00}, .expect        = Registers{.bx = 0xab, .cx
        //                             = 0x3a00, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x10ac, .data = {0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0xb0, 0x1, 0x10},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a00, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a00, .si = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1085, .data
        //                             = {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0xb1, 0x10, 0x10},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a00, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a00, .di = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1094, .data
        //                             = {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0xb2, 0x10, 0x10},
        //                             .init = Registers{.dx = 0x3a00, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.dx = 0x3a00, .si = 0x04, .bp = 0x80,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1094, .data
        //                             = {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0xb3, 0x20, 0x10},
        //                             .init = Registers{.dx = 0x3a00, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.dx = 0x3a00, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x10a4, .data
        //                             = {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xb4, 0x01, 0x10},
        //                             .init          = Registers{.dx = 0x3a00, .si =
        //                             0xff}, .expect        = Registers{.dx = 0x3a00, .si
        //                             = 0xff, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1100, .data = {0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xb5, 0x10, 0x10},
        //                             .init          = Registers{.dx = 0x3a00, .di =
        //                             0x84}, .expect        = Registers{.dx = 0x3a00, .di
        //                             = 0x84, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1094, .data = {0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xb6, 0x20, 0x10},
        //                             .init          = Registers{.dx = 0x3a00, .bp =
        //                             0x20}, .expect        = Registers{.dx = 0x3a00, .bp
        //                             = 0x20, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1040, .data = {0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xb7, 0x1, 0x10},
        //                             .init          = Registers{.bx = 0xab, .dx =
        //                             0x3a00}, .expect        = Registers{.bx = 0xab, .dx
        //                             = 0x3a00, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x10ac, .data = {0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xb8, 0x10, 0x10},
        //                             .init          = Registers{.bx = 0x3a80, .si =
        //                             0x04}, .expect        = Registers{.bx = 0x3a80, .si
        //                             = 0x04, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x4a94, .data = {0x3a}},
        //                             .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xb9, 0x10, 0x10},
        //                             .init          = Registers{.bx = 0x3a80, .di =
        //                             0x04}, .expect        = Registers{.bx = 0x3a80, .di
        //                             = 0x04, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x4a94, .data = {0x3a}},
        //                             .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0xba, 0x10, 0x10},
        //                             .init = Registers{.bx = 0x3a00, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.bx = 0x3a00, .si = 0x04, .bp = 0x80,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1094, .data
        //                             = {0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x88, 0xbb, 0x10, 0x10},
        //                             .init = Registers{.bx = 0x3a00, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x3a00, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1094, .data
        //                             = {0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xbc, 0x01, 0x10},
        //                             .init          = Registers{.bx = 0x3a00, .si =
        //                             0xff}, .expect        = Registers{.bx = 0x3a00, .si
        //                             = 0xff, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1100, .data = {0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xbd, 0x01, 0x10},
        //                             .init          = Registers{.bx = 0x3a00, .di =
        //                             0x84}, .expect        = Registers{.bx = 0x3a00, .di
        //                             = 0x84, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1085, .data = {0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xbe, 0x20, 0x10},
        //                             .init          = Registers{.bx = 0x3a00, .bp =
        //                             0x45}, .expect        = Registers{.bx = 0x3a00, .bp
        //                             = 0x45, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1065, .data = {0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x88, 0xbf, 0x1, 0x10},
        //                             .init          = Registers{.bx = 0x3aab},
        //                             .expect        = Registers{.bx = 0x3aab, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x4aac, .data
        //                             = {0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xc0},
        //                             .init   = Registers{.ax = 0xface},
        //                             .expect = Registers{.ax = 0xface, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xc1},
        //                             .init   = Registers{.ax = 0xface,},
        //                             .expect = Registers{.ax = 0xface, .cx = 0xce, .ip =
        //                             2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xc2},
        //                             .init   = Registers{.ax = 0xface},
        //                             .expect = Registers{.ax = 0xface, .dx = 0xce,.ip =
        //                             2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xc3},
        //                             .init   = Registers{.ax = 0xface,},
        //                             .expect = Registers{.ax = 0xface, .bx = 0xce, .ip =
        //                             2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xc4},
        //                             .init   = Registers{.ax = 0xface},
        //                             .expect = Registers{.ax = 0xcece, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xc5},
        //                             .init   = Registers{.ax = 0xface,},
        //                             .expect = Registers{.ax = 0xface, .cx = 0xce00, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xc6},
        //                             .init   = Registers{.ax = 0xface},
        //                             .expect = Registers{.ax = 0xface, .dx = 0xce00,.ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xc7},
        //                             .init   = Registers{.ax = 0xface,},
        //                             .expect = Registers{.ax = 0xface, .bx = 0xce00, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),

        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xc8},
        //                             .init   = Registers{.ax = 0xface, .cx = 0xab},
        //                             .expect = Registers{.ax = 0xfaab, .cx = 0xab, .ip =
        //                             2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xc9},
        //                             .init   = Registers{.cx = 0xface},
        //                             .expect = Registers{.cx = 0xface, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xca},
        //                             .init   = Registers{.cx = 0xface},
        //                             .expect = Registers{.cx = 0xface, .dx = 0xce,.ip =
        //                             2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xcb},
        //                             .init   = Registers{.cx = 0xface,},
        //                             .expect = Registers{.bx = 0xce, .cx = 0xface, .ip =
        //                             2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xcc},
        //                             .init   = Registers{.cx = 0xface},
        //                             .expect = Registers{.ax = 0xce00, .cx = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xcd},
        //                             .init   = Registers{.cx = 0xface,},
        //                             .expect = Registers{.cx = 0xcece, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xce},
        //                             .init   = Registers{.cx = 0xface},
        //                             .expect = Registers{.cx = 0xface, .dx = 0xce00,.ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xcf},
        //                             .init   = Registers{.cx = 0xface,},
        //                             .expect = Registers{.bx = 0xce00, .cx = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xd0},
        //                             .init   = Registers{.ax = 0xface, .dx = 0xab},
        //                             .expect = Registers{.ax = 0xfaab, .dx = 0xab, .ip =
        //                             2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xd1},
        //                             .init   = Registers{.cx = 0xface, .dx = 0xab},
        //                             .expect = Registers{.cx = 0xfaab, .dx = 0xab, .ip =
        //                             2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xd2},
        //                             .init   = Registers{.dx = 0xface},
        //                             .expect = Registers{.dx = 0xface, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xd3},
        //                             .init   = Registers{.bx = 0x1200, .dx = 0xce,},
        //                             .expect = Registers{.bx = 0x12ce, .dx = 0xce, .ip =
        //                             2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xd4},
        //                             .init   = Registers{.dx = 0xface},
        //                             .expect = Registers{.ax = 0xce00, .dx = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xd5},
        //                             .init   = Registers{.dx = 0xface,},
        //                             .expect = Registers{.cx = 0xce00, .dx = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xd6},
        //                             .init   = Registers{.dx = 0xface},
        //                             .expect = Registers{.dx = 0xcece, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xd7},
        //                             .init   = Registers{.dx = 0xface,},
        //                             .expect = Registers{.bx = 0xce00, .dx = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),

        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xd8},
        //                             .init   = Registers{.ax = 0xface, .bx = 0xab},
        //                             .expect = Registers{.ax = 0xfaab, .bx = 0xab, .ip =
        //                             2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xd9},
        //                             .init   = Registers{.bx = 0x12, .cx = 0xface},
        //                             .expect = Registers{.bx = 0x12, .cx = 0xfa12, .ip =
        //                             2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xda},
        //                             .init   = Registers{.bx = 0x12, .dx = 0xface},
        //                             .expect = Registers{.bx = 0x12, .dx = 0xfa12,
        //                              .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xdb},
        //                             .init   = Registers{.bx = 0xface,},
        //                             .expect = Registers{.bx = 0xface,.ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xdc},
        //                             .init   = Registers{.bx = 0xface},
        //                             .expect = Registers{.ax = 0xce00, .bx = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xdd},
        //                             .init   = Registers{.bx = 0xce, .cx = 0x00ce,},
        //                             .expect = Registers{.bx = 0xce, .cx = 0xcece, .ip =
        //                             2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xde},
        //                             .init   = Registers{.bx = 0xface},
        //                             .expect = Registers{.bx = 0xface, .dx = 0xce00,.ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xdf},
        //                             .init   = Registers{.bx = 0xface,},
        //                             .expect = Registers{.bx = 0xcece, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xe0},
        //                             .init   = Registers{.ax = 0xface},
        //                             .expect = Registers{.ax = 0xfafa, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xe1},
        //                             .init   = Registers{.ax = 0xface,},
        //                             .expect = Registers{.ax = 0xface, .cx = 0xfa, .ip =
        //                             2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xe2},
        //                             .init   = Registers{.ax = 0xface},
        //                             .expect = Registers{.ax = 0xface, .dx = 0xfa,.ip =
        //                             2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xe3},
        //                             .init   = Registers{.ax = 0xface,},
        //                             .expect = Registers{.ax = 0xface, .bx = 0xfa, .ip =
        //                             2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xe4},
        //                             .init   = Registers{.ax = 0xface},
        //                             .expect = Registers{.ax = 0xface, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xe5},
        //                             .init   = Registers{.ax = 0xface,},
        //                             .expect = Registers{.ax = 0xface, .cx = 0xfa00, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xe6},
        //                             .init   = Registers{.ax = 0xface},
        //                             .expect = Registers{.ax = 0xface, .dx = 0xfa00,.ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xe7},
        //                             .init   = Registers{.ax = 0xface,},
        //                             .expect = Registers{.ax = 0xface, .bx = 0xfa00, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),

        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xe8},
        //                             .init   = Registers{.ax = 0xface, .cx = 0xab00},
        //                             .expect = Registers{.ax = 0xfaab, .cx = 0xab00, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xe9},
        //                             .init   = Registers{.cx = 0xface},
        //                             .expect = Registers{.cx = 0xfafa, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xea},
        //                             .init   = Registers{.cx = 0xface},
        //                             .expect = Registers{.cx = 0xface, .dx = 0xfa, .ip =
        //                             2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xeb},
        //                             .init   = Registers{.cx = 0xface,},
        //                             .expect = Registers{.bx = 0xfa, .cx = 0xface, .ip =
        //                             2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xec},
        //                             .init   = Registers{.cx = 0xface},
        //                             .expect = Registers{.ax = 0xfa00, .cx = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xed},
        //                             .init   = Registers{.cx = 0xface,},
        //                             .expect = Registers{.cx = 0xface, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xee},
        //                             .init   = Registers{.cx = 0xface},
        //                             .expect = Registers{.cx = 0xface, .dx = 0xfa00,.ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xef},
        //                             .init   = Registers{.cx = 0xface,},
        //                             .expect = Registers{.bx = 0xfa00, .cx = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xf0},
        //                             .init   = Registers{.ax = 0xface, .dx = 0xab00},
        //                             .expect = Registers{.ax = 0xfaab, .dx = 0xab00, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xf1},
        //                             .init   = Registers{.cx = 0xface, .dx = 0xab00},
        //                             .expect = Registers{.cx = 0xfaab, .dx = 0xab00, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xf2},
        //                             .init   = Registers{.dx = 0xface},
        //                             .expect = Registers{.dx = 0xfafa, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xf3},
        //                             .init   = Registers{.bx = 0x1200, .dx = 0xce00,},
        //                             .expect = Registers{.bx = 0x12ce, .dx = 0xce00, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xf4},
        //                             .init   = Registers{.dx = 0xface},
        //                             .expect = Registers{.ax = 0xfa00, .dx = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xf5},
        //                             .init   = Registers{.dx = 0xface,},
        //                             .expect = Registers{.cx = 0xfa00, .dx = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xf6},
        //                             .init   = Registers{.dx = 0xface},
        //                             .expect = Registers{.dx = 0xface, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xf7},
        //                             .init   = Registers{.dx = 0xface,},
        //                             .expect = Registers{.bx = 0xfa00, .dx = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),

        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xf8},
        //                             .init   = Registers{.ax = 0xface, .bx = 0xab00},
        //                             .expect = Registers{.ax = 0xfaab, .bx = 0xab00, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xf9},
        //                             .init   = Registers{.bx = 0x1200, .cx = 0xface},
        //                             .expect = Registers{.bx = 0x1200, .cx = 0xfa12, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xfa},
        //                             .init   = Registers{.bx = 0x1200, .dx = 0xface},
        //                             .expect = Registers{.bx = 0x1200, .dx = 0xfa12,
        //                              .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xfb},
        //                             .init   = Registers{.bx = 0xface,},
        //                             .expect = Registers{.bx = 0xfafa,.ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xfc},
        //                             .init   = Registers{.bx = 0xface},
        //                             .expect = Registers{.ax = 0xfa00, .bx = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xfd},
        //                             .init   = Registers{.bx = 0xc100, .cx = 0x00ce,},
        //                             .expect = Registers{.bx = 0xc100, .cx = 0xc1ce, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xfe},
        //                             .init   = Registers{.bx = 0xface},
        //                             .expect = Registers{.bx = 0xface, .dx = 0xfa00,.ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x88, 0xff},
        //                             .init   = Registers{.bx = 0xface,},
        //                             .expect = Registers{.bx = 0xface, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                 },
        //         },
        //         MovFromMemToRegParams{
        //             .name = "0x89",
        //             .data =
        //                 {
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x00},
        //                             .init = Registers{.ax = 0x3a10, .bx = 0x80, .si =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a10, .bx = 0x80, .si = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x01},
        //                             .init = Registers{.ax = 0x3a10, .bx = 0x80, .di =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a10, .bx = 0x80, .di = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x02},
        //                             .init = Registers{.ax = 0x3a20, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.ax = 0x3a20, .si = 0x04, .bp = 0x80,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x03},
        //                             .init = Registers{.ax = 0x3a20, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a20, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x04},
        //                             .init          = Registers{.ax = 0x3a20, .si =
        //                             0xff}, .expect        = Registers{.ax = 0x3a20, .si
        //                             = 0xff, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xff, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x05},
        //                             .init          = Registers{.ax = 0x3a20, .di =
        //                             0x84}, .expect        = Registers{.ax = 0x3a20, .di
        //                             = 0x84, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0x84, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x06, 0x11, 0x20},
        //                             .init          = Registers{.ax = 0x3a20},
        //                             .expect        = Registers{.ax = 0x3a20, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2011, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x07},
        //                             .init          = Registers{.ax = 0x3a20, .bx =
        //                             0xab}, .expect        = Registers{.ax = 0x3a20, .bx
        //                             = 0xab, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xab, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x08},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a20, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a20, .si = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x09},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a20, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a20, .di = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x0a},
        //                             .init = Registers{.cx = 0x3a20, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.cx = 0x3a20, .si = 0x04, .bp = 0x80,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x0b},
        //                             .init = Registers{.cx = 0x3a20, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.cx = 0x3a20, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x0c},
        //                             .init          = Registers{.cx = 0x3a20, .si =
        //                             0xff}, .expect        = Registers{.cx = 0x3a20, .si
        //                             = 0xff, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xff, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x0d},
        //                             .init          = Registers{.cx = 0x3a20, .di =
        //                             0x84}, .expect        = Registers{.cx = 0x3a20, .di
        //                             = 0x84, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0x84, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x0e, 0x11, 0x20},
        //                             .init          = Registers{.cx = 0x3a20},
        //                             .expect        = Registers{.cx = 0x3a20, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2011, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x0f},
        //                             .init          = Registers{.bx = 0xab, .cx =
        //                             0x3a20}, .expect        = Registers{.bx = 0xab, .cx
        //                             = 0x3a20, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xab, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x10},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a20, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a20, .si = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x11},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a20, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a20, .di = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x12},
        //                             .init = Registers{.dx = 0x3a20, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.dx = 0x3a20, .si = 0x04, .bp = 0x80,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x13},
        //                             .init = Registers{.dx = 0x3a20, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.dx = 0x3a20, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x14},
        //                             .init          = Registers{.dx = 0x3a20, .si =
        //                             0xff}, .expect        = Registers{.dx = 0x3a20, .si
        //                             = 0xff, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xff, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x15},
        //                             .init          = Registers{.dx = 0x3a20, .di =
        //                             0x84}, .expect        = Registers{.dx = 0x3a20, .di
        //                             = 0x84, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0x84, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x16, 0x11, 0x20},
        //                             .init          = Registers{.dx = 0x3a20},
        //                             .expect        = Registers{.dx = 0x3a20, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2011, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x17},
        //                             .init          = Registers{.bx = 0xab, .dx =
        //                             0x3a20}, .expect        = Registers{.bx = 0xab, .dx
        //                             = 0x3a20, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xab, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x18},
        //                             .init          = Registers{.bx = 0x0130, .si =
        //                             0x04}, .expect        = Registers{.bx = 0x0130, .si
        //                             = 0x04, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0x134, .data = {0x30, 0x1}},
        //                             .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x19},
        //                             .init          = Registers{.bx = 0x0120, .di =
        //                             0x04}, .expect        = Registers{.bx = 0x0120, .di
        //                             = 0x04, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0x124, .data = {0x20, 0x01}},
        //                             .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x1a},
        //                             .init = Registers{.bx = 0x3a20, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.bx = 0x3a20, .si = 0x04, .bp = 0x80,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x1b},
        //                             .init = Registers{.bx = 0x3a20, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x3a20, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x1c},
        //                             .init          = Registers{.bx = 0x3a20, .si =
        //                             0xff}, .expect        = Registers{.bx = 0x3a20, .si
        //                             = 0xff, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xff, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x1d},
        //                             .init          = Registers{.bx = 0x3a20, .di =
        //                             0x84}, .expect        = Registers{.bx = 0x3a20, .di
        //                             = 0x84, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0x84, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x1e, 0x11, 0x20},
        //                             .init          = Registers{.bx = 0x3a20},
        //                             .expect        = Registers{.bx = 0x3a20, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2011, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x1f},
        //                             .init          = Registers{.bx = 0x3a20},
        //                             .expect        = Registers{.bx = 0x3a20, .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x3a20, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x20},
        //                             .init = Registers{.bx = 0x80, .si = 0x04, .sp =
        //                             0x3a10}, .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .sp = 0x3a10,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x21},
        //                             .init = Registers{.bx = 0x80, .di = 0x04, .sp =
        //                             0x3a10}, .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .sp = 0x3a10,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x22},
        //                             .init = Registers{.si = 0x04, .bp = 0x80, .sp =
        //                             0x3a10}, .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .sp = 0x3a10,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x23},
        //                             .init = Registers{.di = 0x80, .bp = 0x04, .sp =
        //                             0x3a10}, .expect =
        //                                 Registers{.di = 0x80, .bp = 0x04, .sp = 0x3a10,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x24},
        //                             .init          = Registers{.si = 0xff, .sp =
        //                             0x3a10}, .expect        = Registers{.si = 0xff, .sp
        //                             = 0x3a10, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xff, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x25},
        //                             .init          = Registers{.di = 0x84, .sp =
        //                             0x3a10}, .expect        = Registers{.di = 0x84, .sp
        //                             = 0x3a10, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0x84, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x26, 0x11, 0x20},
        //                             .init          = Registers{.sp = 0x3a10},
        //                             .expect        = Registers{.sp = 0x3a10, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2011, .data
        //                             = {0x10, 0x3a}}, .cycles        = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x27},
        //                             .init          = Registers{.bx = 0xab, .sp =
        //                             0x3a10}, .expect        = Registers{.bx = 0xab, .sp
        //                             = 0x3a10, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xab, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x28},
        //                             .init = Registers{.bx = 0x80, .si = 0x04, .bp =
        //                             0x3a10}, .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .bp = 0x3a10,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x29},
        //                             .init = Registers{.bx = 0x80, .di = 0x04, .bp =
        //                             0x3a10}, .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .bp = 0x3a10,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x2a},
        //                             .init = Registers{.si = 0x04, .bp = 0x180},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x180, .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x184, .data =
        //                             {0x80, 0x01}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x2b},
        //                             .init = Registers{.di = 0x80, .bp = 0x100},
        //                             .expect =
        //                                 Registers{.di = 0x80, .bp = 0x100, .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x180, .data =
        //                             {0x00, 0x1}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x2c},
        //                             .init          = Registers{.si = 0xff, .bp =
        //                             0x3a10}, .expect        = Registers{.si = 0xff, .bp
        //                             = 0x3a10, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xff, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x2d},
        //                             .init          = Registers{.di = 0x84, .bp =
        //                             0x3a10}, .expect        = Registers{.di = 0x84, .bp
        //                             = 0x3a10, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0x84, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x2e, 0x11, 0x20},
        //                             .init          = Registers{.bp = 0x3a10},
        //                             .expect        = Registers{.bp = 0x3a10, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2011, .data
        //                             = {0x10, 0x3a}}, .cycles        = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x2f},
        //                             .init          = Registers{.bx = 0xab, .bp =
        //                             0x3a10}, .expect        = Registers{.bx = 0xab, .bp
        //                             = 0x3a10, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xab, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x30},
        //                             .init = Registers{.bx = 0x80, .si = 0x100},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x100, .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x180, .data =
        //                             {0x00, 0x01}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x31},
        //                             .init = Registers{.bx = 0x80, .si = 0x100, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .si = 0x100, .di = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x00, 0x01}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x32},
        //                             .init = Registers{.si = 0x100, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x100, .bp = 0x80, .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x180, .data =
        //                             {0x00, 0x01}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x33},
        //                             .init = Registers{.si = 0x3a3b, .di = 0x80, .bp =
        //                             0x04 }, .expect =
        //                                 Registers{.si = 0x3a3b, .di = 0x80, .bp = 0x04,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x3b, 0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x34},
        //                             .init          = Registers{.si = 0x1ff},
        //                             .expect        = Registers{.si = 0x1ff, .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x1ff, .data =
        //                             {0xff, 0x01}}, .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x35},
        //                             .init          = Registers{.si = 0x3a10, .di =
        //                             0x84}, .expect        = Registers{.si = 0x3a10, .di
        //                             = 0x84, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0x84, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x36, 0x11, 0x20},
        //                             .init          = Registers{.si = 0x3a10},
        //                             .expect        = Registers{.si = 0x3a10, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2011, .data
        //                             = {0x10, 0x3a}}, .cycles        = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x37},
        //                             .init          = Registers{.bx = 0xab, .si =
        //                             0x3a10}, .expect        = Registers{.bx = 0xab, .si
        //                             = 0x3a10, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xab, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x38},
        //                             .init = Registers{.bx = 0x80, .si = 0x04, .di =
        //                             0x3a10}, .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .di = 0x3a10,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x84, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x39},
        //                             .init = Registers{.bx = 0x80, .di = 0x104},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x104, .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x184, .data =
        //                             {0x04, 0x01}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x3a},
        //                             .init = Registers{.si = 0x04, .di = 0x1234, .bp =
        //                             0x10}, .expect =
        //                                 Registers{.si = 0x04, .di = 0x1234, .bp = 0x10,
        //                                 .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x14, .data =
        //                             {0x34, 0x12}}, .cycles        = 0x11,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x3b},
        //                             .init = Registers{.di = 0x180, .bp = 0x100},
        //                             .expect =
        //                                 Registers{.di = 0x180, .bp = 0x100, .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x280, .data =
        //                             {0x80, 0x1}}, .cycles        = 0x10,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x3c},
        //                             .init          = Registers{.si = 0xff, .di =
        //                             0x3a10}, .expect        = Registers{.si = 0xff, .di
        //                             = 0x3a10, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xff, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x3d},
        //                             .init          = Registers{.di = 0x184},
        //                             .expect        = Registers{.di = 0x184, .ip = 2},
        //                             .expect_memory = MemoryOp{.address = 0x184, .data =
        //                             {0x84, 0x1}}, .cycles        = 0x0e,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x3e, 0x11, 0x20},
        //                             .init          = Registers{.di = 0x3a10},
        //                             .expect        = Registers{.di = 0x3a10, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2011, .data
        //                             = {0x10, 0x3a}}, .cycles        = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x3f},
        //                             .init          = Registers{.bx = 0xab, .di =
        //                             0x3a10}, .expect        = Registers{.bx = 0xab, .di
        //                             = 0x3a10, .ip = 2}, .expect_memory =
        //                             MemoryOp{.address = 0xab, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x0e,
        //                         }),

        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x40, 0x10},
        //                             .init = Registers{.ax = 0x3a10, .bx = 0x80, .si =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a10, .bx = 0x80, .si = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x41, 0x10},
        //                             .init = Registers{.ax = 0x3a10, .bx = 0x80, .di =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a10, .bx = 0x80, .di = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x42, 0x10},
        //                             .init = Registers{.ax = 0x3a20, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.ax = 0x3a20, .si = 0x04, .bp = 0x80,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x43, 0x10},
        //                             .init = Registers{.ax = 0x3a20, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a20, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x44, 0x10},
        //                             .init          = Registers{.ax = 0x3a20, .si =
        //                             0xff}, .expect        = Registers{.ax = 0x3a20, .si
        //                             = 0xff, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x10f, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x45, 0x10},
        //                             .init          = Registers{.ax = 0x3a20, .di =
        //                             0x84}, .expect        = Registers{.ax = 0x3a20, .di
        //                             = 0x84, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x94, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x46, 0x10},
        //                             .init          = Registers{.ax = 0x3a20, .bp =
        //                             0x2001}, .expect        = Registers{.ax = 0x3a20,
        //                             .bp = 0x2001, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x2011, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x47, 0x10},
        //                             .init          = Registers{.ax = 0x3a20, .bx =
        //                             0xab}, .expect        = Registers{.ax = 0x3a20, .bx
        //                             = 0xab, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0xbb, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x48, 0x10},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a20, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a20, .si = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x49, 0x10},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a20, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a20, .di = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x4a, 0x10},
        //                             .init = Registers{.cx = 0x3a20, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.cx = 0x3a20, .si = 0x04, .bp = 0x80,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x4b, 0x10},
        //                             .init = Registers{.cx = 0x3a20, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.cx = 0x3a20, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x4c, 0x10},
        //                             .init          = Registers{.cx = 0x3a20, .si =
        //                             0xff}, .expect        = Registers{.cx = 0x3a20, .si
        //                             = 0xff, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x10f, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x4d, 0x10},
        //                             .init          = Registers{.cx = 0x3a20, .di =
        //                             0x84}, .expect        = Registers{.cx = 0x3a20, .di
        //                             = 0x84, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x94, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x4e, 0x11},
        //                             .init          = Registers{.cx = 0x3a20, .bp =
        //                             0x2000}, .expect        = Registers{.cx = 0x3a20,
        //                             .bp = 0x2000, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x2011, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x4f, 0x10},
        //                             .init          = Registers{.bx = 0xab, .cx =
        //                             0x3a20}, .expect        = Registers{.bx = 0xab, .cx
        //                             = 0x3a20, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0xbb, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x50, 0x10},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a20, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a20, .si = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x51, 0x10},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a20, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a20, .di = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x52, 0x10},
        //                             .init = Registers{.dx = 0x3a20, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.dx = 0x3a20, .si = 0x04, .bp = 0x80,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x53, 0x10},
        //                             .init = Registers{.dx = 0x3a20, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.dx = 0x3a20, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x54, 0x10},
        //                             .init          = Registers{.dx = 0x3a20, .si =
        //                             0xff}, .expect        = Registers{.dx = 0x3a20, .si
        //                             = 0xff, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x10f, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x55, 0x10},
        //                             .init          = Registers{.dx = 0x3a20, .di =
        //                             0x84}, .expect        = Registers{.dx = 0x3a20, .di
        //                             = 0x84, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x94, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x56, 0x11},
        //                             .init          = Registers{.dx = 0x3a20, .bp =
        //                             0x2000}, .expect        = Registers{.dx = 0x3a20,
        //                             .bp = 0x2000, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x2011, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x57, 0x10},
        //                             .init          = Registers{.bx = 0xab, .dx =
        //                             0x3a20}, .expect        = Registers{.bx = 0xab, .dx
        //                             = 0x3a20, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0xbb, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x58, 0x10},
        //                             .init          = Registers{.bx = 0x0130, .si =
        //                             0x04}, .expect        = Registers{.bx = 0x0130, .si
        //                             = 0x04, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x144, .data = {0x30, 0x1}},
        //                             .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x59, 0x10},
        //                             .init          = Registers{.bx = 0x0120, .di =
        //                             0x04}, .expect        = Registers{.bx = 0x0120, .di
        //                             = 0x04, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x134, .data = {0x20, 0x01}},
        //                             .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x5a, 0x10},
        //                             .init = Registers{.bx = 0x3a20, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.bx = 0x3a20, .si = 0x04, .bp = 0x80,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x5b, 0x10},
        //                             .init = Registers{.bx = 0x3a20, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x3a20, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x20, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x5c, 0x10},
        //                             .init          = Registers{.bx = 0x3a20, .si =
        //                             0xff}, .expect        = Registers{.bx = 0x3a20, .si
        //                             = 0xff, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x10f, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x5d, 0x10},
        //                             .init          = Registers{.bx = 0x3a20, .di =
        //                             0x84}, .expect        = Registers{.bx = 0x3a20, .di
        //                             = 0x84, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x94, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x5e, 0x11},
        //                             .init          = Registers{.bx = 0x3a20, .bp =
        //                             0x2000}, .expect        = Registers{.bx = 0x3a20,
        //                             .bp = 0x2000, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x2011, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x5f, 0x10},
        //                             .init          = Registers{.bx = 0x3a20},
        //                             .expect        = Registers{.bx = 0x3a20, .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x3a30, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x60, 0x10},
        //                             .init = Registers{.bx = 0x80, .si = 0x04, .sp =
        //                             0x3a10}, .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .sp = 0x3a10,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x61, 0x10},
        //                             .init = Registers{.bx = 0x80, .di = 0x04, .sp =
        //                             0x3a10}, .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .sp = 0x3a10,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x62, 0x10},
        //                             .init = Registers{.si = 0x04, .bp = 0x80, .sp =
        //                             0x3a10}, .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .sp = 0x3a10,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x63, 0x10},
        //                             .init = Registers{.di = 0x80, .bp = 0x04, .sp =
        //                             0x3a10}, .expect =
        //                                 Registers{.di = 0x80, .bp = 0x04, .sp = 0x3a10,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x64, 0x10},
        //                             .init          = Registers{.si = 0xff, .sp =
        //                             0x3a10}, .expect        = Registers{.si = 0xff, .sp
        //                             = 0x3a10, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x10f, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x65, 0x10},
        //                             .init          = Registers{.di = 0x84, .sp =
        //                             0x3a10}, .expect        = Registers{.di = 0x84, .sp
        //                             = 0x3a10, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x94, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x66, 0x11},
        //                             .init          = Registers{.bp = 0x2000, .sp =
        //                             0x3a10}, .expect        = Registers{.bp = 0x2000,
        //                             .sp = 0x3a10, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x2011, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x67, 0x10},
        //                             .init          = Registers{.bx = 0xab, .sp =
        //                             0x3a10}, .expect        = Registers{.bx = 0xab, .sp
        //                             = 0x3a10, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0xbb, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x68, 0x10},
        //                             .init = Registers{.bx = 0x80, .si = 0x04, .bp =
        //                             0x3a10}, .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .bp = 0x3a10,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x69, 0x10},
        //                             .init = Registers{.bx = 0x80, .di = 0x04, .bp =
        //                             0x3a10}, .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .bp = 0x3a10,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x6a, 0x10},
        //                             .init = Registers{.si = 0x04, .bp = 0x180},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x180, .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x194, .data =
        //                             {0x80, 0x01}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x6b, 0x10},
        //                             .init = Registers{.di = 0x80, .bp = 0x100},
        //                             .expect =
        //                                 Registers{.di = 0x80, .bp = 0x100, .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x190, .data =
        //                             {0x00, 0x1}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x6c, 0x10},
        //                             .init          = Registers{.si = 0xff, .bp =
        //                             0x3a10}, .expect        = Registers{.si = 0xff, .bp
        //                             = 0x3a10, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x10f, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x6d, 0x10},
        //                             .init          = Registers{.di = 0x84, .bp =
        //                             0x3a10}, .expect        = Registers{.di = 0x84, .bp
        //                             = 0x3a10, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x94, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x6e, 0x11},
        //                             .init          = Registers{.bp = 0x2001},
        //                             .expect        = Registers{.bp = 0x2001, .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x2012, .data
        //                             = {0x01, 0x20}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x6f, 0x10},
        //                             .init          = Registers{.bx = 0xab, .bp =
        //                             0x3a10}, .expect        = Registers{.bx = 0xab, .bp
        //                             = 0x3a10, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0xbb, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x70, 0x10},
        //                             .init = Registers{.bx = 0x80, .si = 0x100},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x100, .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x190, .data =
        //                             {0x00, 0x01}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x71, 0x10},
        //                             .init = Registers{.bx = 0x80, .si = 0x100, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .si = 0x100, .di = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x00, 0x01}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x72, 0x10},
        //                             .init = Registers{.si = 0x100, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x100, .bp = 0x80, .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x190, .data =
        //                             {0x00, 0x01}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x73, 0x10},
        //                             .init = Registers{.si = 0x3a3b, .di = 0x80, .bp =
        //                             0x04 }, .expect =
        //                                 Registers{.si = 0x3a3b, .di = 0x80, .bp = 0x04,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x3b, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x74, 0x10},
        //                             .init          = Registers{.si = 0x1ff},
        //                             .expect        = Registers{.si = 0x1ff, .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x20f, .data =
        //                             {0xff, 0x01}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x75, 0x10},
        //                             .init          = Registers{.si = 0x3a10, .di =
        //                             0x84}, .expect        = Registers{.si = 0x3a10, .di
        //                             = 0x84, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x94, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x76, 0x11},
        //                             .init          = Registers{.si = 0x3a10, .bp =
        //                             0x2000}, .expect        = Registers{.si = 0x3a10,
        //                             .bp = 0x2000, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x2011, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x77, 0x10},
        //                             .init          = Registers{.bx = 0xab, .si =
        //                             0x3a10}, .expect        = Registers{.bx = 0xab, .si
        //                             = 0x3a10, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0xbb, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x78, 0x10},
        //                             .init = Registers{.bx = 0x80, .si = 0x04, .di =
        //                             0x3a10}, .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .di = 0x3a10,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x94, .data =
        //                             {0x10, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x79, 0x10},
        //                             .init = Registers{.bx = 0x80, .di = 0x104},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x104, .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x194, .data =
        //                             {0x04, 0x01}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x7a, 0x10},
        //                             .init = Registers{.si = 0x04, .di = 0x1234, .bp =
        //                             0x10}, .expect =
        //                                 Registers{.si = 0x04, .di = 0x1234, .bp = 0x10,
        //                                 .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x24, .data =
        //                             {0x34, 0x12}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x7b, 0x10},
        //                             .init = Registers{.di = 0x180, .bp = 0x100},
        //                             .expect =
        //                                 Registers{.di = 0x180, .bp = 0x100, .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x290, .data =
        //                             {0x80, 0x1}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x7c, 0x10},
        //                             .init          = Registers{.si = 0xff, .di =
        //                             0x3a10}, .expect        = Registers{.si = 0xff, .di
        //                             = 0x3a10, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x10f, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x7d, 0x10},
        //                             .init          = Registers{.di = 0x184},
        //                             .expect        = Registers{.di = 0x184, .ip = 3},
        //                             .expect_memory = MemoryOp{.address = 0x194, .data =
        //                             {0x84, 0x1}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x7e, 0x10},
        //                             .init          = Registers{.di = 0x3a10, .bp =
        //                             0x2001}, .expect        = Registers{.di = 0x3a10,
        //                             .bp = 0x2001, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0x2011, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x7f, 0x10},
        //                             .init          = Registers{.bx = 0xab, .di =
        //                             0x3a10}, .expect        = Registers{.bx = 0xab, .di
        //                             = 0x3a10, .ip = 3}, .expect_memory =
        //                             MemoryOp{.address = 0xbb, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        // //////////////////////////////////////////////////////////////
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x80, 0x10, 0x20},
        //                             .init = Registers{.ax = 0x3a10, .bx = 0x80, .si =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a10, .bx = 0x80, .si = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2094, .data
        //                             = {0x10, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x81, 0x10, 0x20},
        //                             .init = Registers{.ax = 0x3a10, .bx = 0x80, .di =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a10, .bx = 0x80, .di = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2094, .data
        //                             = {0x10, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x82, 0x10, 0x20},
        //                             .init = Registers{.ax = 0x3a20, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.ax = 0x3a20, .si = 0x04, .bp = 0x80,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2094, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x83, 0x10, 0x20},
        //                             .init = Registers{.ax = 0x3a20, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.ax = 0x3a20, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2094, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x84, 0x10, 0x20},
        //                             .init          = Registers{.ax = 0x3a20, .si =
        //                             0xff}, .expect        = Registers{.ax = 0x3a20, .si
        //                             = 0xff, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x210f, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x85, 0x10, 0x20},
        //                             .init          = Registers{.ax = 0x3a20, .di =
        //                             0x84}, .expect        = Registers{.ax = 0x3a20, .di
        //                             = 0x84, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x2094, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x86, 0x10, 0x20},
        //                             .init          = Registers{.ax = 0x3a20, .bp =
        //                             0x1001}, .expect        = Registers{.ax = 0x3a20,
        //                             .bp = 0x1001, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x3011, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x87, 0x10, 0x20},
        //                             .init          = Registers{.ax = 0x3a20, .bx =
        //                             0xab}, .expect        = Registers{.ax = 0x3a20, .bx
        //                             = 0xab, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x20bb, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x88, 0x10, 0x20},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a20, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a20, .si = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2094, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x89, 0x10, 0x20},
        //                             .init = Registers{.bx = 0x80, .cx = 0x3a20, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .cx = 0x3a20, .di = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2094, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x8a, 0x10, 0x20},
        //                             .init = Registers{.cx = 0x3a20, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.cx = 0x3a20, .si = 0x04, .bp = 0x80,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2094, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x8b, 0x10, 0x20},
        //                             .init = Registers{.cx = 0x3a20, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.cx = 0x3a20, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x2094, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x8c, 0x10, 0x20},
        //                             .init          = Registers{.cx = 0x3a20, .si =
        //                             0xff}, .expect        = Registers{.cx = 0x3a20, .si
        //                             = 0xff, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x210f, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x8d, 0x10, 0x20},
        //                             .init          = Registers{.cx = 0x3a20, .di =
        //                             0x84}, .expect        = Registers{.cx = 0x3a20, .di
        //                             = 0x84, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x2094, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x8e, 0x11, 0x20},
        //                             .init          = Registers{.cx = 0x3a20, .bp =
        //                             0x1000}, .expect        = Registers{.cx = 0x3a20,
        //                             .bp = 0x1000, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x3011, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x8f, 0x10, 0x20},
        //                             .init          = Registers{.bx = 0xab, .cx =
        //                             0x3a20}, .expect        = Registers{.bx = 0xab, .cx
        //                             = 0x3a20, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x20bb, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x90, 0x10, 0x11},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a20, .si =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a20, .si = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1194, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x91, 0x10, 0x11},
        //                             .init = Registers{.bx = 0x80, .dx = 0x3a20, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .dx = 0x3a20, .di = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1194, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x92, 0x10, 0x11},
        //                             .init = Registers{.dx = 0x3a20, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.dx = 0x3a20, .si = 0x04, .bp = 0x80,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1194, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x93, 0x10, 0x11},
        //                             .init = Registers{.dx = 0x3a20, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.dx = 0x3a20, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1194, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x94, 0x10, 0x11},
        //                             .init          = Registers{.dx = 0x3a20, .si =
        //                             0xff}, .expect        = Registers{.dx = 0x3a20, .si
        //                             = 0xff, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x120f, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x95, 0x10, 0x11},
        //                             .init          = Registers{.dx = 0x3a20, .di =
        //                             0x84}, .expect        = Registers{.dx = 0x3a20, .di
        //                             = 0x84, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1194, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x96, 0x11, 0x11},
        //                             .init          = Registers{.dx = 0x3a20, .bp =
        //                             0x2000}, .expect        = Registers{.dx = 0x3a20,
        //                             .bp = 0x2000, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x3111, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x97, 0x10, 0x11},
        //                             .init          = Registers{.bx = 0xab, .dx =
        //                             0x3a20}, .expect        = Registers{.bx = 0xab, .dx
        //                             = 0x3a20, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x11bb, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x98, 0x10, 0x11},
        //                             .init          = Registers{.bx = 0x0130, .si =
        //                             0x04}, .expect        = Registers{.bx = 0x0130, .si
        //                             = 0x04, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1244, .data = {0x30, 0x1}},
        //                             .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x99, 0x10, 0x11},
        //                             .init          = Registers{.bx = 0x0120, .di =
        //                             0x04}, .expect        = Registers{.bx = 0x0120, .di
        //                             = 0x04, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1234, .data = {0x20, 0x01}},
        //                             .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x9a, 0x10, 0x11},
        //                             .init = Registers{.bx = 0x3a20, .si = 0x04, .bp =
        //                             0x80}, .expect =
        //                                 Registers{.bx = 0x3a20, .si = 0x04, .bp = 0x80,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1194, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0x9b, 0x10, 0x11},
        //                             .init = Registers{.bx = 0x3a20, .di = 0x80, .bp =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x3a20, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1194, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x9c, 0x10, 0x11},
        //                             .init          = Registers{.bx = 0x3a20, .si =
        //                             0xff}, .expect        = Registers{.bx = 0x3a20, .si
        //                             = 0xff, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x120f, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x9d, 0x10, 0x11},
        //                             .init          = Registers{.bx = 0x3a20, .di =
        //                             0x84}, .expect        = Registers{.bx = 0x3a20, .di
        //                             = 0x84, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1194, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x9e, 0x11, 0x11},
        //                             .init          = Registers{.bx = 0x3a20, .bp =
        //                             0x2000}, .expect        = Registers{.bx = 0x3a20,
        //                             .bp = 0x2000, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x3111, .data = {0x20, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0x9f, 0x10, 0x11},
        //                             .init          = Registers{.bx = 0x3a20},
        //                             .expect        = Registers{.bx = 0x3a20, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x4b30, .data
        //                             = {0x20, 0x3a}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0xa0, 0x10, 0x11},
        //                             .init = Registers{.bx = 0x80, .si = 0x04, .sp =
        //                             0x3a10}, .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .sp = 0x3a10,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1194, .data
        //                             = {0x10, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0xa1, 0x10, 0x11},
        //                             .init = Registers{.bx = 0x80, .di = 0x04, .sp =
        //                             0x3a10}, .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .sp = 0x3a10,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1194, .data
        //                             = {0x10, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0xa2, 0x10, 0x11},
        //                             .init = Registers{.si = 0x04, .bp = 0x80, .sp =
        //                             0x3a10}, .expect =
        //                                 Registers{.si = 0x04, .bp = 0x80, .sp = 0x3a10,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1194, .data
        //                             = {0x10, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0xa3, 0x10, 0x11},
        //                             .init = Registers{.di = 0x80, .bp = 0x04, .sp =
        //                             0x3a10}, .expect =
        //                                 Registers{.di = 0x80, .bp = 0x04, .sp = 0x3a10,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1194, .data
        //                             = {0x10, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0xa4, 0x10, 0x11},
        //                             .init          = Registers{.si = 0xff, .sp =
        //                             0x3a10}, .expect        = Registers{.si = 0xff, .sp
        //                             = 0x3a10, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x120f, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0xa5, 0x10, 0x11},
        //                             .init          = Registers{.di = 0x84, .sp =
        //                             0x3a10}, .expect        = Registers{.di = 0x84, .sp
        //                             = 0x3a10, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1194, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0xa6, 0x11, 0x11},
        //                             .init          = Registers{.bp = 0x2000, .sp =
        //                             0x3a10}, .expect        = Registers{.bp = 0x2000,
        //                             .sp = 0x3a10, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x3111, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0xa7, 0x10, 0x11},
        //                             .init          = Registers{.bx = 0xab, .sp =
        //                             0x3a10}, .expect        = Registers{.bx = 0xab, .sp
        //                             = 0x3a10, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x11bb, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0xa8, 0x10, 0x11},
        //                             .init = Registers{.bx = 0x80, .si = 0x04, .bp =
        //                             0x3a10}, .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .bp = 0x3a10,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1194, .data
        //                             = {0x10, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0xa9, 0x10, 0x11},
        //                             .init = Registers{.bx = 0x80, .di = 0x04, .bp =
        //                             0x3a10}, .expect =
        //                                 Registers{.bx = 0x80, .di = 0x04, .bp = 0x3a10,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1194, .data
        //                             = {0x10, 0x3a}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0xaa, 0x10, 0x11},
        //                             .init = Registers{.si = 0x04, .bp = 0x180},
        //                             .expect =
        //                                 Registers{.si = 0x04, .bp = 0x180, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1294, .data
        //                             = {0x80, 0x01}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0xab, 0x10, 0x11},
        //                             .init = Registers{.di = 0x80, .bp = 0x100},
        //                             .expect =
        //                                 Registers{.di = 0x80, .bp = 0x100, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1290, .data
        //                             = {0x00, 0x1}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0xac, 0x10, 0x11},
        //                             .init          = Registers{.si = 0xff, .bp =
        //                             0x3a10}, .expect        = Registers{.si = 0xff, .bp
        //                             = 0x3a10, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x120f, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0xad, 0x10, 0x11},
        //                             .init          = Registers{.di = 0x84, .bp =
        //                             0x3a10}, .expect        = Registers{.di = 0x84, .bp
        //                             = 0x3a10, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1194, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0xae, 0x11, 0x11},
        //                             .init          = Registers{.bp = 0x2001},
        //                             .expect        = Registers{.bp = 0x2001, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x3112, .data
        //                             = {0x01, 0x20}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0xaf, 0x10, 0x11},
        //                             .init          = Registers{.bx = 0xab, .bp =
        //                             0x3a10}, .expect        = Registers{.bx = 0xab, .bp
        //                             = 0x3a10, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x11bb, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0xb0, 0x10, 0x11},
        //                             .init = Registers{.bx = 0x80, .si = 0x100},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .si = 0x100, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1290, .data
        //                             = {0x00, 0x01}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0xb1, 0x10, 0x11},
        //                             .init = Registers{.bx = 0x80, .si = 0x100, .di =
        //                             0x04}, .expect =
        //                                 Registers{.bx = 0x80, .si = 0x100, .di = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1194, .data
        //                             = {0x00, 0x01}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0xb2, 0x10, 0x11},
        //                             .init = Registers{.si = 0x100, .bp = 0x80},
        //                             .expect =
        //                                 Registers{.si = 0x100, .bp = 0x80, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1290, .data
        //                             = {0x00, 0x01}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0xb3, 0x10, 0x11},
        //                             .init = Registers{.si = 0x3a3b, .di = 0x80, .bp =
        //                             0x04 }, .expect =
        //                                 Registers{.si = 0x3a3b, .di = 0x80, .bp = 0x04,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1194, .data
        //                             = {0x3b, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0xb4, 0x10, 0x11},
        //                             .init          = Registers{.si = 0x1ff},
        //                             .expect        = Registers{.si = 0x1ff, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x130f, .data
        //                             = {0xff, 0x01}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0xb5, 0x10, 0x11},
        //                             .init          = Registers{.si = 0x3a10, .di =
        //                             0x84}, .expect        = Registers{.si = 0x3a10, .di
        //                             = 0x84, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x1194, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0xb6, 0x11, 0x11},
        //                             .init          = Registers{.si = 0x3a10, .bp =
        //                             0x2000}, .expect        = Registers{.si = 0x3a10,
        //                             .bp = 0x2000, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x3111, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0xb7, 0x10, 0x11},
        //                             .init          = Registers{.bx = 0xab, .si =
        //                             0x3a10}, .expect        = Registers{.bx = 0xab, .si
        //                             = 0x3a10, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x11bb, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0xb8, 0x10, 0x11},
        //                             .init = Registers{.bx = 0x80, .si = 0x04, .di =
        //                             0x3a10}, .expect =
        //                                 Registers{.bx = 0x80, .si = 0x04, .di = 0x3a10,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1194, .data
        //                             = {0x10, 0x3a}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0xb9, 0x10, 0x11},
        //                             .init = Registers{.bx = 0x80, .di = 0x104},
        //                             .expect =
        //                                 Registers{.bx = 0x80, .di = 0x104, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1294, .data
        //                             = {0x04, 0x01}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0xba, 0x10, 0x11},
        //                             .init = Registers{.si = 0x04, .di = 0x1234, .bp =
        //                             0x10}, .expect =
        //                                 Registers{.si = 0x04, .di = 0x1234, .bp = 0x10,
        //                                 .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1124, .data
        //                             = {0x34, 0x12}}, .cycles        = 0x15,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd  = {0x89, 0xbb, 0x10, 0x11},
        //                             .init = Registers{.di = 0x180, .bp = 0x100},
        //                             .expect =
        //                                 Registers{.di = 0x180, .bp = 0x100, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1390, .data
        //                             = {0x80, 0x1}}, .cycles        = 0x14,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0xbc, 0x10, 0x11},
        //                             .init          = Registers{.si = 0xff, .di =
        //                             0x3a10}, .expect        = Registers{.si = 0xff, .di
        //                             = 0x3a10, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x120f, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0xbd, 0x10, 0x11},
        //                             .init          = Registers{.di = 0x184},
        //                             .expect        = Registers{.di = 0x184, .ip = 4},
        //                             .expect_memory = MemoryOp{.address = 0x1294, .data
        //                             = {0x84, 0x1}}, .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0xbe, 0x10, 0x11},
        //                             .init          = Registers{.di = 0x3a10, .bp =
        //                             0x2001}, .expect        = Registers{.di = 0x3a10,
        //                             .bp = 0x2001, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x3111, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd           = {0x89, 0xbf, 0x10, 0x11},
        //                             .init          = Registers{.bx = 0xab, .di =
        //                             0x3a10}, .expect        = Registers{.bx = 0xab, .di
        //                             = 0x3a10, .ip = 4}, .expect_memory =
        //                             MemoryOp{.address = 0x11bb, .data = {0x10, 0x3a}},
        //                             .cycles        = 0x12,
        //                         }),


        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xc0},
        //                             .init   = Registers{.ax = 0xface},
        //                             .expect = Registers{.ax = 0xface, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xc1},
        //                             .init   = Registers{.ax = 0xabcd},
        //                             .expect = Registers{.ax = 0xabcd, .cx = 0xabcd, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xc2},
        //                             .init   = Registers{.ax = 0xaabb},
        //                             .expect = Registers{.ax = 0xaabb, .dx = 0xaabb, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xc3},
        //                             .init   = Registers{.ax = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .bx = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xc4},
        //                             .init   = Registers{.ax = 0xabba},
        //                             .expect = Registers{.ax = 0xabba, .sp = 0xabba, .ip
        //                             = 2}, .cycles = 0x0f,

        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xc5},
        //                             .init   = Registers{.ax = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .bp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xc6},
        //                             .init   = Registers{.ax = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .si = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xc7},
        //                             .init   = Registers{.ax = 0xface},
        //                             .expect = Registers{.ax = 0xface, .di = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xc8},
        //                             .init   = Registers{.cx = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .cx = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xc9},
        //                             .init   = Registers{.cx = 0x12ab},
        //                             .expect = Registers{.cx = 0x12ab, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xca},
        //                             .init   = Registers{.cx = 0xaabb},
        //                             .expect = Registers{.cx = 0xaabb, .dx = 0xaabb, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xcb},
        //                             .init   = Registers{.cx = 0xface},
        //                             .expect = Registers{.bx = 0xface, .cx = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xcc},
        //                             .init   = Registers{.cx = 0xface},
        //                             .expect = Registers{.cx = 0xface, .sp = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,

        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xcd},
        //                             .init   = Registers{.cx = 0x1234},
        //                             .expect = Registers{.cx = 0x1234, .bp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xce},
        //                             .init   = Registers{.cx = 0xface,},
        //                             .expect = Registers{.cx = 0xface, .si = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xcf},
        //                             .init   = Registers{.cx = 0xfaab},
        //                             .expect = Registers{.cx = 0xfaab, .di = 0xfaab, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),

        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xd0},
        //                             .init   = Registers{.dx = 0xface},
        //                             .expect = Registers{.ax = 0xface, .dx = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xd1},
        //                             .init   = Registers{.dx = 0xabcd},
        //                             .expect = Registers{.cx = 0xabcd, .dx = 0xabcd, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xd2},
        //                             .init   = Registers{.dx = 0xaabb},
        //                             .expect = Registers{.dx = 0xaabb, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xd3},
        //                             .init   = Registers{.dx = 0x1234},
        //                             .expect = Registers{.bx = 0x1234, .dx = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xd4},
        //                             .init   = Registers{.dx = 0xabba},
        //                             .expect = Registers{.dx = 0xabba, .sp = 0xabba, .ip
        //                             = 2}, .cycles = 0x0f,

        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xd5},
        //                             .init   = Registers{.dx = 0x1234},
        //                             .expect = Registers{.dx = 0x1234, .bp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xd6},
        //                             .init   = Registers{.dx = 0x1234},
        //                             .expect = Registers{.dx = 0x1234, .si = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xd7},
        //                             .init   = Registers{.dx = 0xface},
        //                             .expect = Registers{.dx = 0xface, .di = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xd8},
        //                             .init   = Registers{.bx = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .bx = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xd9},
        //                             .init   = Registers{.bx = 0x12ab},
        //                             .expect = Registers{.bx = 0x12ab, .cx = 0x12ab, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xda},
        //                             .init   = Registers{.bx = 0xaabb},
        //                             .expect = Registers{.bx = 0xaabb, .dx = 0xaabb, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xdb},
        //                             .init   = Registers{.bx = 0xface},
        //                             .expect = Registers{.bx = 0xface, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xdc},
        //                             .init   = Registers{.bx = 0xface},
        //                             .expect = Registers{.bx = 0xface, .sp = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,

        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xdd},
        //                             .init   = Registers{.bx = 0x1234},
        //                             .expect = Registers{.bx = 0x1234, .bp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xde},
        //                             .init   = Registers{.bx = 0xface,},
        //                             .expect = Registers{.bx = 0xface, .si = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xdf},
        //                             .init   = Registers{.bx = 0xfaab},
        //                             .expect = Registers{.bx = 0xfaab, .di = 0xfaab, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xe0},
        //                             .init   = Registers{.sp = 0xface},
        //                             .expect = Registers{.ax = 0xface, .sp = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xe1},
        //                             .init   = Registers{.sp = 0xabcd},
        //                             .expect = Registers{.cx = 0xabcd, .sp = 0xabcd, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xe2},
        //                             .init   = Registers{.sp = 0xaabb},
        //                             .expect = Registers{.dx = 0xaabb, .sp = 0xaabb, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xe3},
        //                             .init   = Registers{.sp = 0x1234},
        //                             .expect = Registers{.bx = 0x1234, .sp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xe4},
        //                             .init   = Registers{.sp = 0xabba},
        //                             .expect = Registers{.sp = 0xabba, .ip = 2},
        //                             .cycles = 0x0f,

        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xe5},
        //                             .init   = Registers{.sp = 0x1234},
        //                             .expect = Registers{.bp = 0x1234, .sp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xe6},
        //                             .init   = Registers{.sp = 0x1234},
        //                             .expect = Registers{.si = 0x1234, .sp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xe7},
        //                             .init   = Registers{.sp = 0xface},
        //                             .expect = Registers{.di = 0xface, .sp = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xe8},
        //                             .init   = Registers{.bp = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .bp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xe9},
        //                             .init   = Registers{.bp = 0x12ab},
        //                             .expect = Registers{.cx = 0x12ab, .bp = 0x12ab, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xea},
        //                             .init   = Registers{.bp = 0xaabb},
        //                             .expect = Registers{.dx = 0xaabb, .bp = 0xaabb, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xeb},
        //                             .init   = Registers{.bp = 0xface},
        //                             .expect = Registers{.bx = 0xface, .bp = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xec},
        //                             .init   = Registers{.bp = 0xface},
        //                             .expect = Registers{.bp = 0xface, .sp = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,

        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xed},
        //                             .init   = Registers{.bp = 0x1234},
        //                             .expect = Registers{.bp = 0x1234, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xee},
        //                             .init   = Registers{.bp = 0xface},
        //                             .expect = Registers{.si = 0xface, .bp = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xef},
        //                             .init   = Registers{.bp = 0xfaab},
        //                             .expect = Registers{.di = 0xfaab, .bp = 0xfaab, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xf0},
        //                             .init   = Registers{.si = 0xface},
        //                             .expect = Registers{.ax = 0xface, .si = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xf1},
        //                             .init   = Registers{.si = 0xabcd},
        //                             .expect = Registers{.cx = 0xabcd, .si = 0xabcd, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xf2},
        //                             .init   = Registers{.si = 0xaabb},
        //                             .expect = Registers{.dx = 0xaabb, .si = 0xaabb, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xf3},
        //                             .init   = Registers{.si = 0x1234},
        //                             .expect = Registers{.bx = 0x1234, .si = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xf4},
        //                             .init   = Registers{.si = 0xabba},
        //                             .expect = Registers{.si = 0xabba, .sp = 0xabba, .ip
        //                             = 2}, .cycles = 0x0f,

        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xf5},
        //                             .init   = Registers{.si = 0x1234},
        //                             .expect = Registers{.si = 0x1234, .bp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xf6},
        //                             .init   = Registers{.si = 0x1234},
        //                             .expect = Registers{.si = 0x1234, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xf7},
        //                             .init   = Registers{.si = 0xface},
        //                             .expect = Registers{.si = 0xface, .di = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xf8},
        //                             .init   = Registers{.di = 0x1234},
        //                             .expect = Registers{.ax = 0x1234, .di = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xf9},
        //                             .init   = Registers{.di = 0x12ab},
        //                             .expect = Registers{.cx = 0x12ab, .di = 0x12ab, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xfa},
        //                             .init   = Registers{.di = 0xaabb},
        //                             .expect = Registers{.dx = 0xaabb, .di = 0xaabb, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xfb},
        //                             .init   = Registers{.di = 0xface},
        //                             .expect = Registers{.bx = 0xface, .di = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xfc},
        //                             .init   = Registers{.di = 0xface},
        //                             .expect = Registers{.di = 0xface, .sp = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,

        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xfd},
        //                             .init   = Registers{.di = 0x1234},
        //                             .expect = Registers{.di = 0x1234, .bp = 0x1234, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xfe},
        //                             .init   = Registers{.di = 0xface},
        //                             .expect = Registers{.si = 0xface, .di = 0xface, .ip
        //                             = 2}, .cycles = 0x0f,
        //                         }),
        //                     add(
        //                         {
        //                             .cmd    = {0x89, 0xff},
        //                             .init   = Registers{.di = 0xfaab},
        //                             .expect = Registers{.di = 0xfaab, .ip = 2},
        //                             .cycles = 0x0f,
        //                         }),
        //                 },
        //         },
        //         MovFromMemToRegParams{
        //             .name = "0xc6",
        //             .data =
        //             {
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x00, 0xab},
        //                         .init = Registers{.bx = 0x10, .si = 0x12},
        //                         .expect = Registers{.bx = 0x10, .si = 0x12, .ip = 3},
        //                         .expect_memory = MemoryOp {.address = 0x22, .data =
        //                         {0xab}}, .cycles = 0x11,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x01, 0xab},
        //                         .init = Registers{.bx = 0x10, .di = 0x12},
        //                         .expect = Registers{.bx = 0x10, .di = 0x12, .ip = 3},
        //                         .expect_memory = MemoryOp {.address = 0x22, .data =
        //                         {0xab}}, .cycles = 0x12,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x02, 0xab},
        //                         .init = Registers{.si = 0x2000, .bp = 0x10},
        //                         .expect = Registers{.si = 0x2000, .bp = 0x10,.ip = 3},
        //                         .expect_memory = MemoryOp {.address = 0x2010, .data =
        //                         {0xab}}, .cycles = 0x12,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x03, 0xab},
        //                         .init = Registers{.di = 0x12, .bp = 0x10},
        //                         .expect = Registers{.di = 0x12, .bp = 0x10, .ip = 3},
        //                         .expect_memory = MemoryOp {.address = 0x22, .data =
        //                         {0xab}}, .cycles = 0x11,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x04, 0xab},
        //                         .init = Registers{.si = 0x22},
        //                         .expect = Registers{.si = 0x22, .ip = 3},
        //                         .expect_memory = MemoryOp {.address = 0x22, .data =
        //                         {0xab}}, .cycles = 0x0f,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x05, 0xab},
        //                         .init = Registers{.di = 0x22},
        //                         .expect = Registers{.di = 0x22, .ip = 3},
        //                         .expect_memory = MemoryOp {.address = 0x22, .data =
        //                         {0xab}}, .cycles = 0x0f,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x06, 0x34, 0x12, 0xab},
        //                         .init = Registers{},
        //                         .expect = Registers{.ip = 5},
        //                         .expect_memory = MemoryOp {.address = 0x1234, .data =
        //                         {0xab}}, .cycles = 0x10,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x07, 0xab},
        //                         .init = Registers{.bx = 0x123},
        //                         .expect = Registers{.bx = 0x123, .ip = 3},
        //                         .expect_memory = MemoryOp {.address = 0x123, .data =
        //                         {0xab}}, .cycles = 0x0f,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x40, 0x10, 0xab},
        //                         .init = Registers{.bx = 0x10, .si = 0x12},
        //                         .expect = Registers{.bx = 0x10, .si = 0x12, .ip = 4},
        //                         .expect_memory = MemoryOp {.address = 0x32, .data =
        //                         {0xab}}, .cycles = 0x15,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x41, 0x12, 0xab},
        //                         .init = Registers{.bx = 0x10, .di = 0x12},
        //                         .expect = Registers{.bx = 0x10, .di = 0x12, .ip = 4},
        //                         .expect_memory = MemoryOp {.address = 0x34, .data =
        //                         {0xab}}, .cycles = 0x16,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x42, 0x02, 0xab},
        //                         .init = Registers{.si = 0x2000, .bp = 0x10},
        //                         .expect = Registers{.si = 0x2000, .bp = 0x10,.ip = 4},
        //                         .expect_memory = MemoryOp {.address = 0x2012, .data =
        //                         {0xab}}, .cycles = 0x16,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x43, 0x03, 0xab},
        //                         .init = Registers{.di = 0x12, .bp = 0x10},
        //                         .expect = Registers{.di = 0x12, .bp = 0x10, .ip = 4},
        //                         .expect_memory = MemoryOp {.address = 0x25, .data =
        //                         {0xab}}, .cycles = 0x15,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x44, 0x10, 0xab},
        //                         .init = Registers{.si = 0x22},
        //                         .expect = Registers{.si = 0x22, .ip = 4},
        //                         .expect_memory = MemoryOp {.address = 0x32, .data =
        //                         {0xab}}, .cycles = 0x13,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x45, 0x05, 0xab},
        //                         .init = Registers{.di = 0x22},
        //                         .expect = Registers{.di = 0x22, .ip = 4},
        //                         .expect_memory = MemoryOp {.address = 0x27, .data =
        //                         {0xab}}, .cycles = 0x13,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x46, 0x34, 0xab},
        //                         .init = Registers{.bp = 0x12},
        //                         .expect = Registers{.bp = 0x12, .ip = 4},
        //                         .expect_memory = MemoryOp {.address = 0x46, .data =
        //                         {0xab}}, .cycles = 0x13,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x47, 0x10, 0xab},
        //                         .init = Registers{.bx = 0x122},
        //                         .expect = Registers{.bx = 0x122,.ip = 4},
        //                         .expect_memory = MemoryOp {.address = 0x132, .data =
        //                         {0xab}}, .cycles = 0x13,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x80, 0x10, 0x20, 0xab},
        //                         .init = Registers{.bx = 0x10, .si = 0x12},
        //                         .expect = Registers{.bx = 0x10, .si = 0x12, .ip = 5},
        //                         .expect_memory = MemoryOp {.address = 0x2032, .data =
        //                         {0xab}}, .cycles = 0x15,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x81, 0x12, 0x10, 0xab},
        //                         .init = Registers{.bx = 0x10, .di = 0x12},
        //                         .expect = Registers{.bx = 0x10, .di = 0x12, .ip = 5},
        //                         .expect_memory = MemoryOp {.address = 0x1034, .data =
        //                         {0xab}}, .cycles = 0x16,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x82, 0x02, 0x10, 0xab},
        //                         .init = Registers{.si = 0x2000, .bp = 0x10},
        //                         .expect = Registers{.si = 0x2000, .bp = 0x10,.ip = 5},
        //                         .expect_memory = MemoryOp {.address = 0x3012, .data =
        //                         {0xab}}, .cycles = 0x16,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x83, 0x03, 0x10, 0xab},
        //                         .init = Registers{.di = 0x12, .bp = 0x10},
        //                         .expect = Registers{.di = 0x12, .bp = 0x10, .ip = 5},
        //                         .expect_memory = MemoryOp {.address = 0x1025, .data =
        //                         {0xab}}, .cycles = 0x15,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x84, 0x10, 0x05, 0xab},
        //                         .init = Registers{.si = 0x22},
        //                         .expect = Registers{.si = 0x22, .ip = 5},
        //                         .expect_memory = MemoryOp {.address = 0x532, .data =
        //                         {0xab}}, .cycles = 0x13,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x85, 0x05, 0x10, 0xab},
        //                         .init = Registers{.di = 0x22},
        //                         .expect = Registers{.di = 0x22, .ip = 5},
        //                         .expect_memory = MemoryOp {.address = 0x1027, .data =
        //                         {0xab}}, .cycles = 0x13,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x86, 0x34, 0x10, 0xab},
        //                         .init = Registers{.bp = 0x12},
        //                         .expect = Registers{.bp = 0x12, .ip = 5},
        //                         .expect_memory = MemoryOp {.address = 0x1046, .data =
        //                         {0xab}}, .cycles = 0x13,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0x87, 0x10, 0x05, 0xab},
        //                         .init = Registers{.bx = 0x12},
        //                         .expect = Registers{.bx = 0x12, .ip = 5},
        //                         .expect_memory = MemoryOp {.address = 0x522, .data =
        //                         {0xab}}, .cycles = 0x13,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0xc0, 0xab},
        //                         .init = Registers{},
        //                         .expect = Registers{.ax = 0xab, .ip=3},
        //                         .cycles = 0x04,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0xc1, 0xab},
        //                         .init = Registers{},
        //                         .expect = Registers{.cx = 0xab, .ip = 3},
        //                         .cycles = 0x04,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0xc2, 0xab},
        //                         .init = Registers{},
        //                         .expect = Registers{.dx = 0xab, .ip = 3},
        //                         .cycles = 0x04,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0xc3, 0xab},
        //                         .init = Registers{},
        //                         .expect = Registers{.bx = 0xab, .ip = 3},
        //                         .cycles = 0x04,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0xc4, 0xab},
        //                         .init = Registers{},
        //                         .expect = Registers{.ax = 0xab00, .ip = 3},
        //                         .cycles = 0x04,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0xc5, 0xab},
        //                         .init = Registers{},
        //                         .expect = Registers{.cx = 0xab00, .ip = 3},
        //                         .cycles = 0x04,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0xc6, 0xab},
        //                         .init = Registers{},
        //                         .expect = Registers{.dx = 0xab00, .ip = 3},
        //                         .cycles = 0x04,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc6, 0xc7, 0xab},
        //                         .init = Registers{},
        //                         .expect = Registers{.bx = 0xab00, .ip = 3},
        //                         .cycles = 0x04,
        //                     }),
        //                 },
        //         },
        //         MovFromMemToRegParams{
        //             .name = "0xc7",
        //             .data =
        //             {
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x00, 0xab, 0xcd},
        //                         .init = Registers{.bx = 0x10, .si = 0x12},
        //                         .expect = Registers{.bx = 0x10, .si = 0x12, .ip = 4},
        //                         .expect_memory = MemoryOp {.address = 0x22, .data =
        //                         {0xab, 0xcd}}, .cycles = 0x11,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x01, 0xab, 0x12},
        //                         .init = Registers{.bx = 0x10, .di = 0x12},
        //                         .expect = Registers{.bx = 0x10, .di = 0x12, .ip = 4},
        //                         .expect_memory = MemoryOp {.address = 0x22, .data =
        //                         {0xab, 0x12}}, .cycles = 0x12,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x02, 0xab, 0x10},
        //                         .init = Registers{.si = 0x2000, .bp = 0x10},
        //                         .expect = Registers{.si = 0x2000, .bp = 0x10,.ip = 4},
        //                         .expect_memory = MemoryOp {.address = 0x2010, .data =
        //                         {0xab, 0x10}}, .cycles = 0x12,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x03, 0xab, 0x2a},
        //                         .init = Registers{.di = 0x12, .bp = 0x10},
        //                         .expect = Registers{.di = 0x12, .bp = 0x10, .ip = 4},
        //                         .expect_memory = MemoryOp {.address = 0x22, .data =
        //                         {0xab, 0x2a}}, .cycles = 0x11,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x04, 0xab, 0x11},
        //                         .init = Registers{.si = 0x22},
        //                         .expect = Registers{.si = 0x22, .ip = 4},
        //                         .expect_memory = MemoryOp {.address = 0x22, .data =
        //                         {0xab, 0x11}}, .cycles = 0x0f,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x05, 0xab, 0x02},
        //                         .init = Registers{.di = 0x22},
        //                         .expect = Registers{.di = 0x22, .ip = 4},
        //                         .expect_memory = MemoryOp {.address = 0x22, .data =
        //                         {0xab, 0x02}}, .cycles = 0x0f,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x06, 0x34, 0x12, 0xab, 0x19},
        //                         .init = Registers{},
        //                         .expect = Registers{.ip = 6},
        //                         .expect_memory = MemoryOp {.address = 0x1234, .data =
        //                         {0xab, 0x19}}, .cycles = 0x10,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x07, 0xab, 0x29},
        //                         .init = Registers{.bx = 0x101},
        //                         .expect = Registers{.bx = 0x101, .ip = 4},
        //                         .expect_memory = MemoryOp {.address = 0x101, .data =
        //                         {0xab, 0x29}}, .cycles = 0x0f,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x40, 0x10, 0xab, 0x10},
        //                         .init = Registers{.bx = 0x10, .si = 0x12},
        //                         .expect = Registers{.bx = 0x10, .si = 0x12, .ip = 5},
        //                         .expect_memory = MemoryOp {.address = 0x32, .data =
        //                         {0xab, 0x10}}, .cycles = 0x15,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x41, 0x12, 0xab, 0x12},
        //                         .init = Registers{.bx = 0x10, .di = 0x12},
        //                         .expect = Registers{.bx = 0x10, .di = 0x12, .ip = 5},
        //                         .expect_memory = MemoryOp {.address = 0x34, .data =
        //                         {0xab, 0x12}}, .cycles = 0x16,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x42, 0x02, 0xab, 0x11},
        //                         .init = Registers{.si = 0x2000, .bp = 0x10},
        //                         .expect = Registers{.si = 0x2000, .bp = 0x10,.ip = 5},
        //                         .expect_memory = MemoryOp {.address = 0x2012, .data =
        //                         {0xab, 0x11}}, .cycles = 0x16,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x43, 0x03, 0xab, 0x10},
        //                         .init = Registers{.di = 0x12, .bp = 0x10},
        //                         .expect = Registers{.di = 0x12, .bp = 0x10, .ip = 5},
        //                         .expect_memory = MemoryOp {.address = 0x25, .data =
        //                         {0xab, 0x10}}, .cycles = 0x15,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x44, 0x10, 0xab, 0xa0},
        //                         .init = Registers{.si = 0x22},
        //                         .expect = Registers{.si = 0x22, .ip = 5},
        //                         .expect_memory = MemoryOp {.address = 0x32, .data =
        //                         {0xab, 0xa0}}, .cycles = 0x13,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x45, 0x05, 0xab, 0x05},
        //                         .init = Registers{.di = 0x22},
        //                         .expect = Registers{.di = 0x22, .ip = 5},
        //                         .expect_memory = MemoryOp {.address = 0x27, .data =
        //                         {0xab, 0x05}}, .cycles = 0x13,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x46, 0x34, 0xab, 0x10},
        //                         .init = Registers{.bp = 0x12},
        //                         .expect = Registers{.bp = 0x12, .ip = 5},
        //                         .expect_memory = MemoryOp {.address = 0x46, .data =
        //                         {0xab, 0x10}}, .cycles = 0x13,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x47, 0x10, 0xab, 0x2a},
        //                         .init = Registers{.bx = 0x110},
        //                         .expect = Registers{.bx = 0x110, .ip = 5},
        //                         .expect_memory = MemoryOp {.address = 0x120, .data =
        //                         {0xab, 0x2a}}, .cycles = 0x13,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x80, 0x10, 0x20, 0xab, 0xaa},
        //                         .init = Registers{.bx = 0x10, .si = 0x12},
        //                         .expect = Registers{.bx = 0x10, .si = 0x12, .ip = 6},
        //                         .expect_memory = MemoryOp {.address = 0x2032, .data =
        //                         {0xab, 0xaa}}, .cycles = 0x15,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x81, 0x12, 0x10, 0xab, 0x12},
        //                         .init = Registers{.bx = 0x10, .di = 0x12},
        //                         .expect = Registers{.bx = 0x10, .di = 0x12, .ip = 6},
        //                         .expect_memory = MemoryOp {.address = 0x1034, .data =
        //                         {0xab, 0x12}}, .cycles = 0x16,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x82, 0x02, 0x10, 0xab, 0x07},
        //                         .init = Registers{.si = 0x2000, .bp = 0x10},
        //                         .expect = Registers{.si = 0x2000, .bp = 0x10,.ip = 6},
        //                         .expect_memory = MemoryOp {.address = 0x3012, .data =
        //                         {0xab, 0x07}}, .cycles = 0x16,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x83, 0x03, 0x10, 0xab, 0xa1},
        //                         .init = Registers{.di = 0x12, .bp = 0x10},
        //                         .expect = Registers{.di = 0x12, .bp = 0x10, .ip = 6},
        //                         .expect_memory = MemoryOp {.address = 0x1025, .data =
        //                         {0xab, 0xa1}}, .cycles = 0x15,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x84, 0x10, 0x05, 0xab, 0x12},
        //                         .init = Registers{.si = 0x22},
        //                         .expect = Registers{.si = 0x22, .ip = 6},
        //                         .expect_memory = MemoryOp {.address = 0x532, .data =
        //                         {0xab, 0x12}}, .cycles = 0x13,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x85, 0x05, 0x10, 0xab, 0x01},
        //                         .init = Registers{.di = 0x22},
        //                         .expect = Registers{.di = 0x22, .ip = 6},
        //                         .expect_memory = MemoryOp {.address = 0x1027, .data =
        //                         {0xab, 0x01}}, .cycles = 0x13,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x86, 0x34, 0x10, 0xab, 0x12},
        //                         .init = Registers{.bp = 0x12},
        //                         .expect = Registers{.bp = 0x12, .ip = 6},
        //                         .expect_memory = MemoryOp {.address = 0x1046, .data =
        //                         {0xab, 0x12}}, .cycles = 0x13,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0x87, 0x10, 0x05, 0xab, 0x10},
        //                         .init = Registers{.bx = 0x12},
        //                         .expect = Registers{.bx = 0x12, .ip = 6},
        //                         .expect_memory = MemoryOp {.address = 0x522, .data =
        //                         {0xab, 0x10}}, .cycles = 0x13,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0xc0, 0xab, 0xa0},
        //                         .init = Registers{},
        //                         .expect = Registers{.ax = 0xa0ab, .ip=4},
        //                         .cycles = 0x04,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0xc1, 0xab, 0x12},
        //                         .init = Registers{},
        //                         .expect = Registers{.cx = 0x12ab, .ip = 4},
        //                         .cycles = 0x04,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0xc2, 0xab, 0x19},
        //                         .init = Registers{},
        //                         .expect = Registers{.dx = 0x19ab, .ip = 4},
        //                         .cycles = 0x04,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0xc3, 0xab, 0x10},
        //                         .init = Registers{},
        //                         .expect = Registers{.bx = 0x10ab, .ip = 4},
        //                         .cycles = 0x04,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0xc4, 0xab, 0x01},
        //                         .init = Registers{},
        //                         .expect = Registers{.sp = 0x01ab, .ip = 4},
        //                         .cycles = 0x04,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0xc5, 0xab, 0x10},
        //                         .init = Registers{},
        //                         .expect = Registers{.bp = 0x10ab, .ip = 4},
        //                         .cycles = 0x04,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0xc6, 0xab, 0x12},
        //                         .init = Registers{},
        //                         .expect = Registers{.si = 0x12ab, .ip = 4},
        //                         .cycles = 0x04,
        //                     }),
        //                 add(
        //                     {
        //                         .cmd    = {0xc7, 0xc7, 0xab, 0x10},
        //                         .init = Registers{},
        //                         .expect = Registers{.di = 0x10ab, .ip = 4},
        //                         .cycles = 0x04,
        //                     }),
        //                 },
        //         }

    );
}

INSTANTIATE_TEST_CASE_P(MovTests_1, MovFromMemToRegTests, get_mov_values(),
                        generate_test_case_name);

} // namespace msemu::cpu8086
