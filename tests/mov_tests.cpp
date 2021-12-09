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

#include "test_base.hpp"

namespace msemu::cpu8086
{

struct MemoryOp
{
    uint32_t address          = 0;
    std::vector<uint8_t> data = {};
};

struct TestData
{
    std::vector<uint8_t> cmd = {};
    MemoryOp memop{};

    std::optional<Registers> init;
    std::optional<Registers> expect;
    std::optional<MemoryOp> expect_memory;
    uint8_t cycles = 0;
    std::source_location location;
    std::source_location init_sut_location;
    std::source_location expect_location;
    std::optional<ModRM> mod;
};

struct MovTestsParams
{
public:
    std::string name           = "default";
    std::vector<TestData> data = {};
};

class MovTests : public ParametrizedTestBase<MovTestsParams>
{
};

void stringify_array(const auto& data, auto& str)
{
    for (std::size_t i = 0; i < data.size(); ++i)
    {
        str << "0x" << std::hex << std::setfill('0') << std::setw(2) << static_cast<uint32_t>(data[i]);
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
    str << "Location: " << data.init_sut_location.file_name() << ":" << data.init_sut_location.line()
        << std::endl;
    return str.str();
}

std::string print_test_case_info(const TestData& data, std::string error, int number)
{
    std::stringstream str;

    str << "TC number: " << number << std::endl;
    str << "TC location: " << data.location.file_name() << ":" << data.location.line() << std::endl;
    str << "Expect location: " << data.expect_location.file_name() << ":" << data.expect_location.line()
        << std::endl;
    str << "Init location: " << data.init_sut_location.file_name() << ":" << data.init_sut_location.line()
        << std::endl;
    str << "error msg: " << error << std::endl;
    str << "cmd: {";
    stringify_array(data.cmd, str);
    str << "}" << std::endl;
    return str.str();
}

TEST_P(MovTests, ProcessCmd)
{
    const MovTestsParams& data = GetParam();

    int i = 0;

    sut_.set_registers(Registers{});
    for (const auto& test_data : data.data)
    {
        bus_.clear();
        if (test_data.init)
        {
            sut_.set_registers(*test_data.init);
        }

        if (test_data.memop.data.size())
        {
            if (test_data.mod)
            {
                auto m = test_data.mod->rm;
                if (m == 0 || m == 1 || m == 4 || m == 5 || m == 7)
                {
                    const uint32_t address = (sut_.get_registers().ds << 4) + test_data.memop.address;
                    bus_.write(address, test_data.memop.data);
                }
                else if (m == 2 || m == 3 || m == 6)
                {
                    const uint32_t address = (sut_.get_registers().ss << 4) + test_data.memop.address;

                    bus_.write(address, test_data.memop.data);
                }
            }
            else
            {
                bus_.write(test_data.memop.address, test_data.memop.data);
            }
        }

        auto& opcode = test_data.cmd;

        const uint32_t address =
            (static_cast<uint32_t>(sut_.get_registers().cs) << 4) + sut_.get_registers().ip;
        bus_.write(address, opcode);

        sut_.step();

        if (test_data.expect)
        {
            EXPECT_EQ(test_data.expect, sut_.get_registers())
                << print_test_case_info(test_data, sut_.get_error(), i);
        }
        if (test_data.expect_memory)
        {
            std::vector<uint8_t> from_memory(test_data.expect_memory->data.size());
            bus_.read(test_data.expect_memory->address, from_memory);
            EXPECT_THAT(from_memory, ::testing::ElementsAreArray(test_data.expect_memory->data))
                << print_test_case_info(test_data, sut_.get_error(), i);
        }

        EXPECT_EQ(sut_.last_instruction_cost(), test_data.cycles) << print_cycles_info(test_data);

        ++i;
    }
}

inline void PrintTo(const MovTestsParams& reg, ::std::ostream* os)
{
    *os << "name: " << reg.name << std::endl;
}

inline std::string get_segment_register_name(uint16_t Registers::*reg)
{
    if (reg == &Registers::es)
    {
        return "es";
    }
    else if (reg == &Registers::cs)
    {
        return "cs";
    }
    else if (reg == &Registers::ds)
    {
        return "ds";
    }
    else if (reg == &Registers::ss)
    {
        return "ss";
    }
    return "unk";
}

inline uint8_t get_segment_modifier_byte(uint16_t Registers::*reg)
{
    if (reg == &Registers::es)
    {
        return 0x26;
    }
    else if (reg == &Registers::cs)
    {
        return 0x2e;
    }
    else if (reg == &Registers::ds)
    {
        return 0x3e;
    }
    else if (reg == &Registers::ss)
    {
        return 0x36;
    }
    return 0x00;
}

MovTestsParams
    modrm_mem_to_reg8_with_section_offset(uint8_t command, uint16_t Registers::*reg,
                                          const std::source_location loc = std::source_location::current())
{
    Registers init{.di = 0x200};
    init.*reg = 0x100;

    Registers expect{.bx = 0xab00, .di = 0x200, .ip = 0x3};
    expect.*reg = 0x100;
    return MovTestsParams{get_name(command) + "_sec_" + get_segment_register_name(reg),
                          {TestData{
                              .cmd      = {get_segment_modifier_byte(reg), command, 0x3d},
                              .memop    = MemoryOp{.address = 0x1200, .data = {0xab}},
                              .init     = init,
                              .expect   = expect,
                              .cycles   = 17,
                              .location = loc,
                          }}};
}

MovTestsParams mem_to_reg_data(uint8_t command, uint16_t Registers::*reg, uint16_t expect,
                               const std::vector<uint8_t>& init_memory,
                               const std::source_location loc = std::source_location::current())
{
    Registers expect_1 = {.ip = 0x03};
    Registers expect_2 = {.ip = 0x06};
    expect_1.*reg      = expect;
    expect_2.*reg      = expect;

    return MovTestsParams{get_name(command),
                          {
                              TestData{.cmd      = {command, 0x01, 0x20},
                                       .memop    = {.address = 0x2001, .data = init_memory},
                                       .expect   = expect_1,
                                       .cycles   = 14,
                                       .location = loc},
                              TestData{.cmd      = {command, 0x10, 0x20},
                                       .memop    = {.address = 0x2010, .data = init_memory},
                                       .expect   = expect_2,
                                       .cycles   = 14,
                                       .location = loc},
                          }};
}

MovTestsParams reg_to_mem_data(uint8_t command, uint16_t Registers::*reg, uint16_t init_reg,
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

    return MovTestsParams{get_name(command),
                          {
                              TestData{.cmd           = {command, 0x01, 0x20},
                                       .init          = init_1,
                                       .expect        = expect_1,
                                       .expect_memory = MemoryOp{.address = 0x2001, .data = expected_memory},
                                       .cycles        = 14,
                                       .location      = loc},
                              TestData{.cmd           = {command, 0x10, 0x20},
                                       .init          = init_2,
                                       .expect        = expect_2,
                                       .expect_memory = MemoryOp{.address = 0x2010, .data = expected_memory},
                                       .cycles        = 14,
                                       .location      = loc},
                          }};
}

MovTestsParams imm8_to_reg_lo(uint8_t command, uint16_t Registers::*reg,
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

    return MovTestsParams{
        get_name(command),
        {
            {.cmd = {command, data1}, .init = init_1, .expect = expect_1, .cycles = 4, .location = loc},
            {.cmd = {command, data2}, .init = init_2, .expect = expect_2, .cycles = 4, .location = loc},
        },
    };
}

MovTestsParams imm8_to_reg_hi(uint8_t command, uint16_t Registers::*reg,
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


    return MovTestsParams{
        get_name(command),
        {
            {.cmd = {command, data1}, .init = init_1, .expect = expect_1, .cycles = 4, .location = loc},
            {.cmd = {command, data2}, .init = init_2, .expect = expect_2, .cycles = 4, .location = loc},
        },

    };
}

MovTestsParams imm8_to_reg(uint8_t command, uint16_t Registers::*reg,
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


    return MovTestsParams{
        get_name(command),
        {
            {.cmd      = {command, data1_h, data1_l},
             .init     = init_1,
             .expect   = expect_1,
             .cycles   = 4,
             .location = loc},
            {.cmd      = {command, data2_h, data2_l},
             .init     = init_2,
             .expect   = expect_2,
             .cycles   = 4,
             .location = loc},
        },
    };
}

struct InitSut
{
    InitSut(const Registers& registers, const std::vector<uint8_t>& data, uint8_t cycles,
            std::source_location current_location = std::source_location::current())
        : init_registers(registers)
        , append_data(data)
        , expected_cycles(cycles)
        , location(current_location)
    {
    }

    Registers init_registers;
    std::vector<uint8_t> append_data;
    uint8_t expected_cycles;
    const std::source_location location;
};


struct ModRMInitData
{
    ModRMInitData(const MemoryOp& memory, uint16_t init, uint16_t expect,
                  const std::source_location loc = std::source_location::current())
        : init_memory(memory)
        , init_value(init)
        , expect_value(expect)
        , location(loc)
    {
    }

    MemoryOp init_memory;
    uint16_t init_value;
    uint16_t expect_value;
    const std::source_location location;
};

const std::array<InitSut, 8> op_mod_sreg_0{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {}, 19},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {}, 20},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {}, 20},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {}, 19},
    InitSut{Registers{.si = 0x2030}, {}, 17},
    InitSut{Registers{.di = 0x2030}, {}, 17},
    InitSut{Registers{}, {0x30, 0x20}, 18},
    InitSut{Registers{.bx = 0x2030}, {}, 17},
};

const std::array<InitSut, 8> op_mod_sreg_1{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {0x15}, 23},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {0x15}, 24},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {0x15}, 24},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {0x15}, 23},
    InitSut{Registers{.si = 0x2030}, {0x15}, 21},
    InitSut{Registers{.di = 0x2030}, {0x15}, 21},
    InitSut{Registers{.bp = 0x2030}, {0x15}, 21},
    InitSut{Registers{.bx = 0x2030}, {0x15}, 21},
};

const std::array<InitSut, 8> op_mod_sreg_2{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {0x15, 0x10}, 23},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {0x15, 0x10}, 24},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {0x15, 0x10}, 24},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {0x15, 0x10}, 23},
    InitSut{Registers{.si = 0x2030}, {0x15, 0x10}, 21},
    InitSut{Registers{.di = 0x2030}, {0x15, 0x10}, 21},
    InitSut{Registers{.bp = 0x2030}, {0x15, 0x10}, 21},
    InitSut{Registers{.bx = 0x2030}, {0x15, 0x10}, 21},
};

const std::array<InitSut, 8> op_mod_sreg_3{
    InitSut{Registers{.ax = 0x1234}, {}, 2}, InitSut{Registers{.cx = 0x1234}, {}, 2},
    InitSut{Registers{.dx = 0x1234}, {}, 2}, InitSut{Registers{.bx = 0x1234}, {}, 2},
    InitSut{Registers{.sp = 0x1234}, {}, 2}, InitSut{Registers{.bp = 0x1234}, {}, 2},
    InitSut{Registers{.si = 0x1234}, {}, 2}, InitSut{Registers{.di = 0x1234}, {}, 2},
};

const std::array<InitSut, 8> op_to_mod_sreg_0{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {}, 20},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {}, 21},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {}, 21},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {}, 20},
    InitSut{Registers{.si = 0x2030}, {}, 18},
    InitSut{Registers{.di = 0x2030}, {}, 18},
    InitSut{Registers{}, {0x30, 0x20}, 19},
    InitSut{Registers{.bx = 0x2030}, {}, 18},
};

const std::array<InitSut, 8> op_to_mod_sreg_1{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {0x15}, 24},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {0x15}, 25},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {0x15}, 25},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {0x15}, 24},
    InitSut{Registers{.si = 0x2030}, {0x15}, 22},
    InitSut{Registers{.di = 0x2030}, {0x15}, 22},
    InitSut{Registers{.bp = 0x2030}, {0x15}, 22},
    InitSut{Registers{.bx = 0x2030}, {0x15}, 22},
};

const std::array<InitSut, 8> op_to_mod_sreg_2{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {0x15, 0x10}, 24},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {0x15, 0x10}, 25},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {0x15, 0x10}, 25},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {0x15, 0x10}, 24},
    InitSut{Registers{.si = 0x2030}, {0x15, 0x10}, 22},
    InitSut{Registers{.di = 0x2030}, {0x15, 0x10}, 22},
    InitSut{Registers{.bp = 0x2030}, {0x15, 0x10}, 22},
    InitSut{Registers{.bx = 0x2030}, {0x15, 0x10}, 22},
};

const std::array<InitSut, 8> op_to_mod_sreg_3{
    InitSut{Registers{.ax = 0x1234}, {}, 2}, InitSut{Registers{.cx = 0x1234}, {}, 2},
    InitSut{Registers{.dx = 0x1234}, {}, 2}, InitSut{Registers{.bx = 0x1234}, {}, 2},
    InitSut{Registers{.sp = 0x1234}, {}, 2}, InitSut{Registers{.bp = 0x1234}, {}, 2},
    InitSut{Registers{.si = 0x1234}, {}, 2}, InitSut{Registers{.di = 0x1234}, {}, 2},
};


const std::array<InitSut, 8> op_mod_reg8_0{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {}, 19},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {}, 20},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {}, 20},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {}, 19},
    InitSut{Registers{.si = 0x2030}, {}, 17},
    InitSut{Registers{.di = 0x2030}, {}, 17},
    InitSut{Registers{}, {0x30, 0x20}, 18},
    InitSut{Registers{.bx = 0x2030}, {}, 17},
};

const std::array<InitSut, 8> op_mod_reg8_1{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {0x15}, 23},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {0x15}, 24},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {0x15}, 24},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {0x15}, 23},
    InitSut{Registers{.si = 0x2030}, {0x15}, 21},
    InitSut{Registers{.di = 0x2030}, {0x15}, 21},
    InitSut{Registers{.bp = 0x2030}, {0x15}, 21},
    InitSut{Registers{.bx = 0x2030}, {0x15}, 21},
};

const std::array<InitSut, 8> op_mod_reg8_2{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {0x15, 0x10}, 23},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {0x15, 0x10}, 24},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {0x15, 0x10}, 24},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {0x15, 0x10}, 23},
    InitSut{Registers{.si = 0x2030}, {0x15, 0x10}, 21},
    InitSut{Registers{.di = 0x2030}, {0x15, 0x10}, 21},
    InitSut{Registers{.bp = 0x2030}, {0x15, 0x10}, 21},
    InitSut{Registers{.bx = 0x2030}, {0x15, 0x10}, 21},
};

const std::array<InitSut, 8> op_mod_reg8_3{
    InitSut{Registers{.ax = 0x3412}, {}, 2}, InitSut{Registers{.cx = 0x3412}, {}, 2},
    InitSut{Registers{.dx = 0x3412}, {}, 2}, InitSut{Registers{.bx = 0x3412}, {}, 2},
    InitSut{Registers{.ax = 0x1234}, {}, 2}, InitSut{Registers{.cx = 0x1234}, {}, 2},
    InitSut{Registers{.dx = 0x1234}, {}, 2}, InitSut{Registers{.bx = 0x1234}, {}, 2},
};

const std::array<InitSut, 8> op_mod_from_reg8_0{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {}, 20},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {}, 21},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {}, 21},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {}, 20},
    InitSut{Registers{.si = 0x2030}, {}, 18},
    InitSut{Registers{.di = 0x2030}, {}, 18},
    InitSut{Registers{}, {0x30, 0x20}, 19},
    InitSut{Registers{.bx = 0x2030}, {}, 18},
};

const std::array<InitSut, 8> op_mod_from_reg8_1{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {0x15}, 24},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {0x15}, 25},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {0x15}, 25},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {0x15}, 24},
    InitSut{Registers{.si = 0x2030}, {0x15}, 22},
    InitSut{Registers{.di = 0x2030}, {0x15}, 22},
    InitSut{Registers{.bp = 0x2030}, {0x15}, 22},
    InitSut{Registers{.bx = 0x2030}, {0x15}, 22},
};

const std::array<InitSut, 8> op_mod_from_reg8_2{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {0x15, 0x10}, 24},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {0x15, 0x10}, 25},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {0x15, 0x10}, 25},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {0x15, 0x10}, 24},
    InitSut{Registers{.si = 0x2030}, {0x15, 0x10}, 22},
    InitSut{Registers{.di = 0x2030}, {0x15, 0x10}, 22},
    InitSut{Registers{.bp = 0x2030}, {0x15, 0x10}, 22},
    InitSut{Registers{.bx = 0x2030}, {0x15, 0x10}, 22},
};

const std::array<InitSut, 8> op_mod_from_reg8_3{
    InitSut{Registers{.ax = 0x003a}, {}, 2}, InitSut{Registers{.cx = 0x003a}, {}, 2},
    InitSut{Registers{.dx = 0x003a}, {}, 2}, InitSut{Registers{.bx = 0x003a}, {}, 2},
    InitSut{Registers{.ax = 0x3a00}, {}, 2}, InitSut{Registers{.cx = 0x3a00}, {}, 2},
    InitSut{Registers{.dx = 0x3a00}, {}, 2}, InitSut{Registers{.bx = 0x3a00}, {}, 2},
};


const std::array<InitSut, 8> op_mod_reg16_3{
    InitSut{Registers{.ax = 0x1234}, {}, 2}, InitSut{Registers{.cx = 0x1234}, {}, 2},
    InitSut{Registers{.dx = 0x1234}, {}, 2}, InitSut{Registers{.bx = 0x1234}, {}, 2},
    InitSut{Registers{.sp = 0x1234}, {}, 2}, InitSut{Registers{.bp = 0x1234}, {}, 2},
    InitSut{Registers{.si = 0x1234}, {}, 2}, InitSut{Registers{.di = 0x1234}, {}, 2},
};

const std::array<InitSut, 8> op_mod_from_reg16_3{
    InitSut{Registers{.ax = 0x1234}, {}, 2}, InitSut{Registers{.cx = 0x1234}, {}, 2},
    InitSut{Registers{.dx = 0x1234}, {}, 2}, InitSut{Registers{.bx = 0x1234}, {}, 2},
    InitSut{Registers{.sp = 0x1234}, {}, 2}, InitSut{Registers{.bp = 0x1234}, {}, 2},
    InitSut{Registers{.si = 0x1234}, {}, 2}, InitSut{Registers{.di = 0x1234}, {}, 2},
};

const std::array<InitSut, 8> op_mod_from_imm8_0{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {0x3a}, 21},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {0x3a}, 22},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {0x3a}, 22},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {0x3a}, 21},
    InitSut{Registers{.si = 0x2030}, {0x3a}, 19},
    InitSut{Registers{.di = 0x2030}, {0x3a}, 19},
    InitSut{Registers{}, {0x30, 0x20, 0x3a}, 20},
    InitSut{Registers{.bx = 0x2030}, {0x3a}, 19},
};

const std::array<InitSut, 8> op_mod_from_imm8_1{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {0x15, 0x3a}, 25},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {0x15, 0x3a}, 26},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {0x15, 0x3a}, 26},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {0x15, 0x3a}, 25},
    InitSut{Registers{.si = 0x2030}, {0x15, 0x3a}, 23},
    InitSut{Registers{.di = 0x2030}, {0x15, 0x3a}, 23},
    InitSut{Registers{.bp = 0x2030}, {0x15, 0x3a}, 23},
    InitSut{Registers{.bx = 0x2030}, {0x15, 0x3a}, 23},
};

const std::array<InitSut, 8> op_mod_from_imm8_2{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {0x15, 0x10, 0x3a}, 25},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {0x15, 0x10, 0x3a}, 26},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {0x15, 0x10, 0x3a}, 26},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {0x15, 0x10, 0x3a}, 25},
    InitSut{Registers{.si = 0x2030}, {0x15, 0x10, 0x3a}, 23},
    InitSut{Registers{.di = 0x2030}, {0x15, 0x10, 0x3a}, 23},
    InitSut{Registers{.bp = 0x2030}, {0x15, 0x10, 0x3a}, 23},
    InitSut{Registers{.bx = 0x2030}, {0x15, 0x10, 0x3a}, 23},
};

const std::array<InitSut, 8> op_mod_from_imm8_3{
    InitSut{Registers{.ax = 0x003a}, {0x3a}, 4}, InitSut{Registers{.cx = 0x003a}, {0x3a}, 4},
    InitSut{Registers{.dx = 0x003a}, {0x3a}, 4}, InitSut{Registers{.bx = 0x003a}, {0x3a}, 4},
    InitSut{Registers{.ax = 0x3a00}, {0x3a}, 4}, InitSut{Registers{.cx = 0x3a00}, {0x3a}, 4},
    InitSut{Registers{.dx = 0x3a00}, {0x3a}, 4}, InitSut{Registers{.bx = 0x3a00}, {0x3a}, 4},
};


const std::array<InitSut, 8> op_mod_from_imm16_0{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {0x3a, 0xbc}, 21},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {0x3a, 0xbc}, 22},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {0x3a, 0xbc}, 22},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {0x3a, 0xbc}, 21},
    InitSut{Registers{.si = 0x2030}, {0x3a, 0xbc}, 19},
    InitSut{Registers{.di = 0x2030}, {0x3a, 0xbc}, 19},
    InitSut{Registers{}, {0x30, 0x20, 0x3a, 0xbc}, 20},
    InitSut{Registers{.bx = 0x2030}, {0x3a, 0xbc}, 19},
};

const std::array<InitSut, 8> op_mod_from_imm16_1{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {0x15, 0x3a, 0xbc}, 25},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {0x15, 0x3a, 0xbc}, 26},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {0x15, 0x3a, 0xbc}, 26},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {0x15, 0x3a, 0xbc}, 25},
    InitSut{Registers{.si = 0x2030}, {0x15, 0x3a, 0xbc}, 23},
    InitSut{Registers{.di = 0x2030}, {0x15, 0x3a, 0xbc}, 23},
    InitSut{Registers{.bp = 0x2030}, {0x15, 0x3a, 0xbc}, 23},
    InitSut{Registers{.bx = 0x2030}, {0x15, 0x3a, 0xbc}, 23},
};

const std::array<InitSut, 8> op_mod_from_imm16_2{
    InitSut{Registers{.bx = 0x1010, .si = 0x1020}, {0x15, 0x10, 0x3a, 0xbc}, 25},
    InitSut{Registers{.bx = 0x1010, .di = 0x1020}, {0x15, 0x10, 0x3a, 0xbc}, 26},
    InitSut{Registers{.si = 0x1020, .bp = 0x1010}, {0x15, 0x10, 0x3a, 0xbc}, 26},
    InitSut{Registers{.di = 0x1020, .bp = 0x1010}, {0x15, 0x10, 0x3a, 0xbc}, 25},
    InitSut{Registers{.si = 0x2030}, {0x15, 0x10, 0x3a, 0xbc}, 23},
    InitSut{Registers{.di = 0x2030}, {0x15, 0x10, 0x3a, 0xbc}, 23},
    InitSut{Registers{.bp = 0x2030}, {0x15, 0x10, 0x3a, 0xbc}, 23},
    InitSut{Registers{.bx = 0x2030}, {0x15, 0x10, 0x3a, 0xbc}, 23},
};

const std::array<InitSut, 8> op_mod_from_imm16_3{
    InitSut{Registers{.ax = 0xbc3a}, {0x3a, 0xbc}, 4}, InitSut{Registers{.cx = 0xbc3a}, {0x3a, 0xbc}, 4},
    InitSut{Registers{.dx = 0xbc3a}, {0x3a, 0xbc}, 4}, InitSut{Registers{.bx = 0xbc3a}, {0x3a, 0xbc}, 4},
    InitSut{Registers{.sp = 0xbc3a}, {0x3a, 0xbc}, 4}, InitSut{Registers{.bp = 0xbc3a}, {0x3a, 0xbc}, 4},
    InitSut{Registers{.si = 0xbc3a}, {0x3a, 0xbc}, 4}, InitSut{Registers{.di = 0xbc3a}, {0x3a, 0xbc}, 4},
};


struct MovDataInit
{
    std::array<InitSut, 8> sut_init;
    std::array<ModRMInitData, 8> test_init;
};

template <typename T>
MovTestsParams modmr_generate_data(uint8_t command, const std::vector<MovDataInit>& modmr_test_inits,
                                   const std::vector<T Registers::*>& regs, ModRM limits,
                                   const std::source_location& loc)
{
    ++limits.mod;
    ++limits.rm;
    ++limits.reg;
    MovTestsParams params;
    params.name = get_name(command);

    ModRM modrm = 0;

    do
    {
        modrm.reg = 0;
        do
        {
            modrm.rm = 0;
            do
            {
                auto& init      = modmr_test_inits[modrm.mod];
                auto& d         = init.sut_init[modrm.rm];
                auto& init_data = init.test_init[modrm.rm];
                const auto& r   = regs[modrm.reg];


                TestData test_data{
                    .cmd               = {command, modrm},
                    .memop             = init_data.init_memory,
                    .init              = d.init_registers,
                    .expect            = d.init_registers,
                    .cycles            = d.expected_cycles,
                    .location          = loc,
                    .init_sut_location = d.location,
                    .expect_location   = init_data.location,
                    .mod               = modrm,
                };

                params.data.push_back(test_data);

                std::copy(d.append_data.begin(), d.append_data.end(),
                          std::back_inserter(params.data.back().cmd));

                if ((*params.data.back().init).*r == 0)
                {
                    (*params.data.back().init).*r = static_cast<T>(init_data.init_value);
                }

                (*params.data.back().expect).*r = static_cast<T>(init_data.expect_value);

                (*params.data.back().expect).ip = static_cast<uint16_t>(params.data.back().cmd.size());

                ++modrm.rm;
            } while (modrm.rm != limits.rm);
            ++modrm.reg;
        } while (modrm.reg != limits.reg);
        ++modrm.mod;
    } while (modrm.mod != limits.mod);

    return params;
}

template <typename T>
MovTestsParams reg_to_modmr_generate_data(uint8_t command, const std::vector<MovDataInit>& modmr_test_inits,
                                          const std::vector<T Registers::*>& regs, ModRM limits,
                                          const std::source_location& loc)
{
    ++limits.mod;
    ++limits.rm;
    ++limits.reg;
    MovTestsParams params;
    params.name = get_name(command);

    ModRM modrm = 0;

    do
    {
        modrm.reg = 0;
        do
        {
            modrm.rm = 0;
            do
            {
                auto& init      = modmr_test_inits[modrm.mod];
                auto& d         = init.sut_init[modrm.rm];
                auto& init_data = init.test_init[modrm.rm];
                const auto& r   = regs[modrm.reg];


                TestData test_data{
                    .cmd               = {command, modrm},
                    .init              = d.init_registers,
                    .expect            = d.init_registers,
                    .expect_memory     = init_data.init_memory,
                    .cycles            = d.expected_cycles,
                    .location          = loc,
                    .init_sut_location = d.location,
                    .expect_location   = init_data.location,
                };

                params.data.push_back(test_data);

                std::copy(d.append_data.begin(), d.append_data.end(),
                          std::back_inserter(params.data.back().cmd));

                if ((*params.data.back().init).*r == 0)
                {
                    (*params.data.back().init).*r   = static_cast<T>(init_data.init_value);
                    (*params.data.back().expect).*r = static_cast<T>(init_data.expect_value);
                }
                else
                {
                    if ((*params.data.back().expect_memory).data.size())
                    {
                        if constexpr (std::is_same_v<T, uint8_t>)
                        {
                            auto value                                  = (*params.data.back().init).*r;
                            (*params.data.back().expect_memory).data[0] = value;
                        }
                        else
                        {
                            auto value                                  = (*params.data.back().init).*r;
                            (*params.data.back().expect_memory).data[0] = static_cast<uint8_t>(value);
                            (*params.data.back().expect_memory).data[1] = static_cast<uint8_t>(value >> 8);
                        }
                    }
                    else
                    {
                        (*params.data.back().init).*r   = static_cast<T>(init_data.init_value);
                        (*params.data.back().expect).*r = static_cast<T>(init_data.expect_value);
                    }
                }
                (*params.data.back().expect).ip = static_cast<uint16_t>(params.data.back().cmd.size());

                ++modrm.rm;
            } while (modrm.rm != limits.rm);
            ++modrm.reg;
        } while (modrm.reg != limits.reg);
        ++modrm.mod;
    } while (modrm.mod != limits.mod);

    return params;
}


MovTestsParams modmr_to_reg8(uint8_t command,
                             const std::source_location loc = std::source_location::current())
{
    const std::vector<MovDataInit> modmr_test_inits{
        {.sut_init  = op_mod_reg8_0,
         .test_init = {ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x1010, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x1010, 0x3a}}},
        {.sut_init  = op_mod_reg8_1,
         .test_init = {ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x1010, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x1010, 0x3a}}},
        {.sut_init  = op_mod_reg8_2,
         .test_init = {ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x1010, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x1234, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x1010, 0x3a}}},
        {.sut_init  = op_mod_reg8_3,
         .test_init = {
             ModRMInitData{{}, 0x00, 0x12},
             ModRMInitData{{}, 0x00, 0x12},
             ModRMInitData{{}, 0x00, 0x12},
             ModRMInitData{{}, 0x00, 0x12},
             ModRMInitData{{}, 0x00, 0x12},
             ModRMInitData{{}, 0x00, 0x12},
             ModRMInitData{{}, 0x00, 0x12},
             ModRMInitData{{}, 0x00, 0x12},
         }}};

    const std::vector<uint8_t Registers::*> regs{&Registers::al, &Registers::cl, &Registers::dl,
                                                 &Registers::bl, &Registers::ah, &Registers::ch,
                                                 &Registers::dh, &Registers::bh};

    const ModRM modmr_limits(3, 7, 7);

    return modmr_generate_data<uint8_t>(command, modmr_test_inits, regs, modmr_limits, loc);
}


MovTestsParams reg8_to_modmr(uint8_t command,
                             const std::source_location loc = std::source_location::current())
{
    const std::vector<MovDataInit> modmr_test_inits{
        {.sut_init  = op_mod_from_reg8_0,
         .test_init = {ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x3a, 0x3a}}},
        {.sut_init  = op_mod_from_reg8_1,
         .test_init = {ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x3a, 0x3a}}},
        {.sut_init  = op_mod_from_reg8_2,
         .test_init = {ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x3a, 0x3a},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x3a, 0x3a}}},
        {.sut_init  = op_mod_from_reg8_3,
         .test_init = {
             ModRMInitData{{}, 0x3a, 0x3a},
             ModRMInitData{{}, 0x3a, 0x3a},
             ModRMInitData{{}, 0x3a, 0x3a},
             ModRMInitData{{}, 0x3a, 0x3a},
             ModRMInitData{{}, 0x3a, 0x3a},
             ModRMInitData{{}, 0x3a, 0x3a},
             ModRMInitData{{}, 0x3a, 0x3a},
             ModRMInitData{{}, 0x3a, 0x3a},
         }}};

    const std::vector<uint8_t Registers::*> regs{&Registers::al, &Registers::cl, &Registers::dl,
                                                 &Registers::bl, &Registers::ah, &Registers::ch,
                                                 &Registers::dh, &Registers::bh};

    const ModRM modmr_limits(3, 7, 7);

    return reg_to_modmr_generate_data<uint8_t>(command, modmr_test_inits, regs, modmr_limits, loc);
}


MovTestsParams modmr_to_reg16(uint8_t command,
                              const std::source_location loc = std::source_location::current())
{
    const std::vector<MovDataInit> modmr_test_inits{
        {.sut_init = op_mod_reg8_0,
         .test_init =
             {
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0x1010, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0x1010, 0xbc3a},
             }},
        {.sut_init = op_mod_reg8_1,
         .test_init =
             {
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0x1010, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0x1010, 0xbc3a},
             }},
        {.sut_init = op_mod_reg8_2,
         .test_init =
             {
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0x1010, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0x1010, 0xbc3a},
             }},
        {.sut_init  = op_mod_reg16_3,
         .test_init = {
             ModRMInitData{{}, 0x0, 0x1234},
             ModRMInitData{{}, 0x0, 0x1234},
             ModRMInitData{{}, 0x0, 0x1234},
             ModRMInitData{{}, 0x0, 0x1234},
             ModRMInitData{{}, 0x0, 0x1234},
             ModRMInitData{{}, 0x0, 0x1234},
             ModRMInitData{{}, 0x0, 0x1234},
             ModRMInitData{{}, 0x0, 0x1234},
         }}};


    const std::vector<uint16_t Registers::*> regs{&Registers::ax, &Registers::cx, &Registers::dx,
                                                  &Registers::bx, &Registers::sp, &Registers::bp,
                                                  &Registers::si, &Registers::di};
    const ModRM modmr_limits(3, 7, 7);

    return modmr_generate_data(command, modmr_test_inits, regs, modmr_limits, loc);
}


MovTestsParams reg16_to_modmr(uint8_t command,
                              const std::source_location loc = std::source_location::current())
{
    const std::vector<MovDataInit> modmr_test_inits{
        {.sut_init = op_mod_from_reg8_0,
         .test_init =
             {
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
             }},
        {.sut_init = op_mod_from_reg8_1,
         .test_init =
             {
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
             }},
        {.sut_init = op_mod_from_reg8_2,
         .test_init =
             {
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
             }},
        {.sut_init  = op_mod_from_reg16_3,
         .test_init = {
             ModRMInitData{{}, 0x1234, 0x1234},
             ModRMInitData{{}, 0x1234, 0x1234},
             ModRMInitData{{}, 0x1234, 0x1234},
             ModRMInitData{{}, 0x1234, 0x1234},
             ModRMInitData{{}, 0x1234, 0x1234},
             ModRMInitData{{}, 0x1234, 0x1234},
             ModRMInitData{{}, 0x1234, 0x1234},
             ModRMInitData{{}, 0x1234, 0x1234},
         }}};


    const std::vector<uint16_t Registers::*> regs{&Registers::ax, &Registers::cx, &Registers::dx,
                                                  &Registers::bx, &Registers::sp, &Registers::bp,
                                                  &Registers::si, &Registers::di};
    const ModRM modmr_limits(3, 7, 7);

    return reg_to_modmr_generate_data(command, modmr_test_inits, regs, modmr_limits, loc);
}


MovTestsParams modmr_to_sreg(uint8_t command,
                             const std::source_location loc = std::source_location::current())
{
    const std::vector<MovDataInit> modmr_test_inits{
        {
            MovDataInit{
                .sut_init = op_mod_sreg_0,
                .test_init =
                    {

                        ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0x0004, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0x0004, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0x1010, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0x0004, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0x0004, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0x1010, 0xbc3a},
                    },
            },
            MovDataInit{
                .sut_init = op_mod_sreg_1,
                .test_init =
                    {

                        ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0x0004, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0x0004, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0x1010, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0x1010, 0xbc3a},
                    },
            },
            MovDataInit{
                .sut_init = op_mod_sreg_2,
                .test_init =
                    {

                        ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0x0004, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0x0004, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0x1010, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0x0004, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0x0004, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0x1234, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0x1010, 0xbc3a},
                    },
            },
            MovDataInit{
                .sut_init = op_mod_sreg_3,
                .test_init =
                    {

                        ModRMInitData{{}, 0x0, 0x1234},
                        ModRMInitData{{}, 0x0, 0x1234},
                        ModRMInitData{{}, 0x0, 0x1234},
                        ModRMInitData{{}, 0x0, 0x1234},
                        ModRMInitData{{}, 0x0, 0x1234},
                        ModRMInitData{{}, 0x0, 0x1234},
                        ModRMInitData{{}, 0x0, 0x1234},
                        ModRMInitData{{}, 0x0, 0x1234},
                    },
            },

        },
    };

    const std::vector<uint16_t Registers::*> regs{&Registers::es, &Registers::cs, &Registers::ss,
                                                  &Registers::ds};

    const ModRM modmr_limits(3, 3, 7);

    return modmr_generate_data(command, modmr_test_inits, regs, modmr_limits, loc);
}


MovTestsParams sreg_to_modmr(uint8_t command,
                             const std::source_location loc = std::source_location::current())
{
    const std::vector<MovDataInit> modmr_test_inits{
        {
            MovDataInit{
                .sut_init = op_to_mod_sreg_0,
                .test_init =
                    {

                        ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                    },
            },
            MovDataInit{
                .sut_init = op_to_mod_sreg_1,
                .test_init =
                    {

                        ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                    },
            },
            MovDataInit{
                .sut_init = op_to_mod_sreg_2,
                .test_init =
                    {

                        ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                        ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0xbc3a, 0xbc3a},
                    },
            },
            MovDataInit{
                .sut_init = op_to_mod_sreg_3,
                .test_init =
                    {

                        ModRMInitData{{}, 0x1234, 0x1234},
                        ModRMInitData{{}, 0x1234, 0x1234},
                        ModRMInitData{{}, 0x1234, 0x1234},
                        ModRMInitData{{}, 0x1234, 0x1234},
                        ModRMInitData{{}, 0x1234, 0x1234},
                        ModRMInitData{{}, 0x1234, 0x1234},
                        ModRMInitData{{}, 0x1234, 0x1234},
                        ModRMInitData{{}, 0x1234, 0x1234},
                    },
            },

        },
    };

    const std::vector<uint16_t Registers::*> regs{&Registers::es, &Registers::cs, &Registers::ss,
                                                  &Registers::ds};

    const ModRM modmr_limits(3, 0, 7);

    return reg_to_modmr_generate_data(command, modmr_test_inits, regs, modmr_limits, loc);
}


MovTestsParams imm8_to_modmr(uint8_t command,
                             const std::source_location loc = std::source_location::current())
{
    const std::vector<MovDataInit> modmr_test_inits{
        {.sut_init  = op_mod_from_imm8_0,
         .test_init = {ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a}}, 0x00, 0x00}}},
        {.sut_init  = op_mod_from_imm8_1,
         .test_init = {ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a}}, 0x00, 0x00}}},
        {.sut_init  = op_mod_from_imm8_2,
         .test_init = {ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x00, 0x00},
                       ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a}}, 0x00, 0x00}}},
        {.sut_init  = op_mod_from_imm8_3,
         .test_init = {
             ModRMInitData{{}, 0x3a, 0x3a},
             ModRMInitData{{}, 0x3a, 0x3a},
             ModRMInitData{{}, 0x3a, 0x3a},
             ModRMInitData{{}, 0x3a, 0x3a},
             ModRMInitData{{}, 0x3a, 0x3a},
             ModRMInitData{{}, 0x3a, 0x3a},
             ModRMInitData{{}, 0x3a, 0x3a},
             ModRMInitData{{}, 0x3a, 0x3a},
         }}};

    const std::vector<uint8_t Registers::*> regs{&Registers::al, &Registers::cl, &Registers::dl,
                                                 &Registers::bl, &Registers::ah, &Registers::ch,
                                                 &Registers::dh, &Registers::bh};

    const ModRM modmr_limits(3, 0, 7);

    return reg_to_modmr_generate_data<uint8_t>(command, modmr_test_inits, regs, modmr_limits, loc);
}


MovTestsParams imm16_to_modmr(uint8_t command,
                              const std::source_location loc = std::source_location::current())
{
    const std::vector<MovDataInit> modmr_test_inits{
        {.sut_init = op_mod_from_imm16_0,
         .test_init =
             {
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x2030, .data = {0x3a, 0xbc}}, 0, 0},
             }},
        {.sut_init = op_mod_from_imm16_1,
         .test_init =
             {
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x2045, .data = {0x3a, 0xbc}}, 0, 0},
             }},
        {.sut_init = op_mod_from_imm16_2,
         .test_init =
             {
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0, 0},
                 ModRMInitData{MemoryOp{.address = 0x3045, .data = {0x3a, 0xbc}}, 0, 0},
             }},
        {.sut_init  = op_mod_from_imm16_3,
         .test_init = {
             ModRMInitData{{}, 0xbc3a, 0xbc3a},
             ModRMInitData{{}, 0xbc3a, 0xbc3a},
             ModRMInitData{{}, 0xbc3a, 0xbc3a},
             ModRMInitData{{}, 0xbc3a, 0xbc3a},
             ModRMInitData{{}, 0xbc3a, 0xbc3a},
             ModRMInitData{{}, 0xbc3a, 0xbc3a},
             ModRMInitData{{}, 0xbc3a, 0xbc3a},
             ModRMInitData{{}, 0xbc3a, 0xbc3a},
         }}};


    const std::vector<uint16_t Registers::*> regs{&Registers::ax, &Registers::cx, &Registers::dx,
                                                  &Registers::bx, &Registers::sp, &Registers::bp,
                                                  &Registers::si, &Registers::di};
    const ModRM modmr_limits(3, 0, 7);

    return reg_to_modmr_generate_data<uint16_t>(command, modmr_test_inits, regs, modmr_limits, loc);
}


auto get_mov_test_parameters()
{
    return ::testing::Values(
        mem_to_reg_data(0xa0, &Registers::ax, 0x00ab, {0xab, 0xff}),
        mem_to_reg_data(0xa1, &Registers::ax, 0xface, {0xce, 0xfa}),
        reg_to_mem_data(0xa2, &Registers::ax, 0x12ab, {0xab, 0x00}),
        reg_to_mem_data(0xa3, &Registers::ax, 0xabcd, {0xcd, 0xab}), imm8_to_reg_lo(0xb0, &Registers::ax),
        imm8_to_reg_lo(0xb1, &Registers::cx), imm8_to_reg_lo(0xb2, &Registers::dx),
        imm8_to_reg_lo(0xb3, &Registers::bx), imm8_to_reg_hi(0xb4, &Registers::ax),
        imm8_to_reg_hi(0xb5, &Registers::cx), imm8_to_reg_hi(0xb6, &Registers::dx),
        imm8_to_reg_hi(0xb7, &Registers::bx), imm8_to_reg(0xb8, &Registers::ax),
        imm8_to_reg(0xb9, &Registers::cx), imm8_to_reg(0xba, &Registers::dx),
        imm8_to_reg(0xbb, &Registers::bx), imm8_to_reg(0xbc, &Registers::sp),
        imm8_to_reg(0xbd, &Registers::bp), imm8_to_reg(0xbe, &Registers::si),
        imm8_to_reg(0xbf, &Registers::di), modmr_to_reg8(0x8a), reg8_to_modmr(0x88), modmr_to_reg16(0x8b),
        reg16_to_modmr(0x89), modmr_to_sreg(0x8e), sreg_to_modmr(0x8c), imm8_to_modmr(0xc6),
        imm16_to_modmr(0xc7), modrm_mem_to_reg8_with_section_offset(0x8a, &Registers::es),
        modrm_mem_to_reg8_with_section_offset(0x8a, &Registers::cs),
        modrm_mem_to_reg8_with_section_offset(0x8a, &Registers::ss),
        modrm_mem_to_reg8_with_section_offset(0x8a, &Registers::ds));
}

INSTANTIATE_TEST_CASE_P(OpcodesMov, MovTests, get_mov_test_parameters(),
                        generate_test_case_name<MovTestsParams>);


} // namespace msemu::cpu8086
