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

#include "test_base.hpp"

#include <source_location>
#include <sstream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace msemu::cpu8086
{

namespace
{

struct TestData
{
public:
    struct Values
    {
        Values(uint16_t o1, uint16_t o2, uint16_t result, Registers::Flags flags,
               const std::source_location &loc = std::source_location::current())
            : op0(o1)
            , op1(o2)
            , result(result)
            , flags(flags)
            , loc(loc)
        {
        }

        uint16_t op0;
        uint16_t op1;
        uint16_t result;
        Registers::Flags flags;
        std::source_location loc;
    };

    using Setter = std::function<void(const TestData::Values &, Registers &, Registers &, BusType &,
                                      std::vector<uint8_t> &)>;

private:
    TestData(const std::vector<Values> &values, bool is_byte, const Setter &setter)
        : values(values)
        , is_byte(is_byte)
        , init_reg(setter)
    {
    }

public:
    static TestData for_byte(const Setter &setter)
    {
        return TestData(
            {
                Values{0x50, 0x30, 0x80, Registers::Flags{.o = true, .s = true}},
                Values{0xff, 0x01, 0x00,
                       Registers::Flags{.o = true, .z = true, .a = true, .p = true, .c = true}},
                Values{0x00, 0x01, 0x02, Registers::Flags{}},
                Values{0xf0, 0x04, 0xf4, Registers::Flags{.s = true}},
            },
            true, setter);
    }

    static TestData for_word(const Setter &setter)
    {
        return TestData(
            {
                Values{0x5061, 0x3060, 0x80c1, Registers::Flags{.o = true, .s = true}},
                Values{0xffff, 0x0001, 0x0000,
                       Registers::Flags{.o = true, .z = true, .a = true, .p = true, .c = true}},
                Values{0x0000, 0x0001, 0x0002, Registers::Flags{}},
                Values{0xf124, 0x0010, 0xf134, Registers::Flags{.s = true}},
            },
            false, setter);
    }

    std::vector<Values> values;
    bool is_byte;
    Setter init_reg;
};


struct AdcTestsParams
{
    AdcTestsParams(uint8_t cmd, const std::vector<TestData> &test, std::string name,
                   std::source_location loc = std::source_location::current())
        : cmd(cmd)
        , test(test)
        , name(name)
        , loc(loc)
    {
    }

    uint8_t cmd;
    std::vector<TestData> test;
    std::string name;
    std::source_location loc;
};


std::vector<uint8_t Registers::*> reg8_mapping = {&Registers::al, &Registers::cl, &Registers::dl,
                                                  &Registers::bl, &Registers::ah, &Registers::ch,
                                                  &Registers::dh, &Registers::bh};

std::vector<uint16_t Registers::*> reg16_mapping = {&Registers::ax, &Registers::cx, &Registers::dx,
                                                    &Registers::bx, &Registers::sp, &Registers::bp,
                                                    &Registers::si, &Registers::di};

using RegisterInitializers = std::vector<std::vector<uint16_t Registers::*>>;

RegisterInitializers mod8_to_reg_0 = {
    {&Registers::bx, &Registers::si},
    {&Registers::bx, &Registers::di},
    {&Registers::bp, &Registers::si},
    {&Registers::bp, &Registers::di},
    {&Registers::si},
    {&Registers::di},
    {},
    {&Registers::bx},
};

RegisterInitializers mod8_to_reg_1 = {
    {&Registers::bx, &Registers::si},
    {&Registers::bx, &Registers::di},
    {&Registers::bp, &Registers::si},
    {&Registers::bp, &Registers::di},
    {&Registers::si},
    {&Registers::di},
    {&Registers::bp},
    {&Registers::bx},
};


std::vector<RegisterInitializers> mod8_to_reg_inits = {
    mod8_to_reg_0, mod8_to_reg_1, mod8_to_reg_1,
    // mod8_to_reg_3,
};


AdcTestsParams generate_modrm_to_reg8()
{
    ModRM mod = 0;

    std::vector<TestData> tests = {};
    do
    {
        mod.reg = 0;
        do
        {
            mod.rm = 0;
            do
            {
                auto test = TestData::for_byte(
                    [mod](const TestData::Values &data, Registers &regs_init, Registers &regs_expect,
                          BusType &bus, std::vector<uint8_t> &command)
                    {
                        regs_init.*reg8_mapping[mod.reg] = static_cast<uint8_t>(data.op0);
                        const auto &reg_to_inits         = mod8_to_reg_inits[mod.mod][mod.rm];
                        uint32_t address                 = 0x1020;
                        if (reg_to_inits.size() == 1)
                        {
                            if ((reg_to_inits[0] == &Registers::bx) && (mod.reg == 3 || mod.reg == 7))
                            {
                                address = regs_init.bx;
                            }
                            else
                            {
                                (regs_init.*reg_to_inits[0]) = 0x1020;
                            }
                        }
                        else if (reg_to_inits.size() == 2)
                        {
                            if ((reg_to_inits[0] == &Registers::bx || reg_to_inits[1] == &Registers::bx) &&
                                (mod.reg == 3 || mod.reg == 7))
                            {
                                if (reg_to_inits[0] != &Registers::bx)
                                {
                                    (regs_init.*reg_to_inits[0]) = 0x0100;
                                }
                                if (reg_to_inits[1] != &Registers::bx)
                                {
                                    (regs_init.*reg_to_inits[1]) = 0x0100;
                                }


                                address = 0x0100 + regs_init.bx;
                            }
                            else
                            {
                                (regs_init.*reg_to_inits[0]) = 0x0610;
                                (regs_init.*reg_to_inits[1]) = 0x0a10;
                            }
                        }
                        regs_expect = regs_init;

                        command.push_back(mod);

                        if (mod.mod == 0 && mod.rm == 6)
                        {
                            command.push_back(0x20);
                            command.push_back(0x10);
                            regs_expect.ip += 4;
                        }
                        else if (mod.mod == 1)
                        {
                            address += 0x20;
                            command.push_back(0x20);
                            regs_expect.ip += 3;
                        }
                        else if (mod.mod == 2)
                        {
                            command.push_back(0x20);
                            command.push_back(0x10);
                            regs_expect.ip += 4;
                            address += 0x1020;
                        }
                        else
                        {
                            regs_expect.ip += 2;
                        }

                        bus.write(address, data.op1);
                        regs_expect.*reg8_mapping[mod.reg] = static_cast<uint8_t>(data.result);
                        regs_expect.flags                  = data.flags;
                    });
                tests.push_back(test);
                ++mod.rm;
            } while (mod.rm != 0);
            ++mod.reg;
        } while (mod.reg != 0);
        ++mod.mod;
    } while (mod.mod != 3);

    mod.mod = 3;
    do
    {
        mod.reg = 0;
        do
        {
            mod.rm = 0;
            do
            {
                auto test = TestData::for_byte(
                    [mod](const TestData::Values &data, Registers &regs_init, Registers &regs_expect,
                          BusType &bus, std::vector<uint8_t> &command)
                    {
                        static_cast<void>(bus);
                        regs_init.*reg8_mapping[mod.reg] = static_cast<uint8_t>(data.op0);
                        regs_init.*reg8_mapping[mod.rm]  = static_cast<uint8_t>(data.op1);
                        regs_expect                      = regs_init;
                        regs_expect.flags                = data.flags;
                        if (mod.reg == mod.rm)
                        {
                            uint8_t part = static_cast<uint8_t>((data.op0 + data.op1) / 2);
                            regs_init.*reg8_mapping[mod.reg]  = part;
                            regs_expect.*reg8_mapping[mod.rm] = static_cast<uint8_t>(part * 2);
                            if (regs_init.flags.c)
                            {
                                regs_expect.*reg8_mapping[mod.rm] += 1;
                            }
                            if (part != 0x7a)
                            {
                                regs_expect.flags.a = false;
                            }
                            else
                            {
                                regs_expect.flags.o = true;
                                regs_expect.flags.a = true;
                            }
                        }
                        else
                        {
                            regs_expect.*reg8_mapping[mod.reg] = static_cast<uint8_t>(data.result);
                        }
                        regs_expect.ip += 2;
                        command.push_back(mod);
                    });
                tests.push_back(test);
                ++mod.rm;
            } while (mod.rm != 0);
            ++mod.reg;
        } while (mod.reg != 0);
        ++mod.mod;
    } while (mod.mod != 0);


    AdcTestsParams param(0x12, tests, "0x12");
    return param;
}


AdcTestsParams generate_modrm_to_reg16()
{
    ModRM mod = 0;

    std::vector<TestData> tests = {};
    do
    {
        mod.reg = 0;
        do
        {
            mod.rm = 0;
            do
            {
                auto test = TestData::for_word(
                    [mod](const TestData::Values &data, Registers &regs_init, Registers &regs_expect,
                          BusType &bus, std::vector<uint8_t> &command)
                    {
                        regs_init.*reg16_mapping[mod.reg] = data.op0;
                        const auto &reg_to_inits          = mod8_to_reg_inits[mod.mod][mod.rm];
                        uint32_t address                  = 0x1020;
                        if (reg_to_inits.size() == 1)
                        {
                            if ((reg_to_inits[0] == reg16_mapping[mod.reg]))
                            {
                                address = regs_init.*reg16_mapping[mod.reg];
                            }
                            else
                            {
                                (regs_init.*reg_to_inits[0]) = 0x1020;
                            }
                        }
                        else if (reg_to_inits.size() == 2)
                        {
                            if (reg_to_inits[0] == reg16_mapping[mod.reg] ||
                                reg_to_inits[1] == reg16_mapping[mod.reg])
                            {
                                if (reg_to_inits[0] != reg16_mapping[mod.reg])
                                {
                                    (regs_init.*reg_to_inits[0]) = 0x0100;
                                }
                                if (reg_to_inits[1] != reg16_mapping[mod.reg])
                                {
                                    (regs_init.*reg_to_inits[1]) = 0x0100;
                                }


                                address = (regs_init.*reg_to_inits[0]) + (regs_init.*reg_to_inits[1]);
                            }
                            else
                            {
                                (regs_init.*reg_to_inits[0]) = 0x0610;
                                (regs_init.*reg_to_inits[1]) = 0x0a10;
                            }
                        }
                        regs_expect = regs_init;

                        command.push_back(mod);

                        if (mod.mod == 0 && mod.rm == 6)
                        {
                            command.push_back(0x20);
                            command.push_back(0x10);
                            regs_expect.ip += 4;
                        }
                        else if (mod.mod == 1)
                        {
                            address += 0x20;
                            command.push_back(0x20);
                            regs_expect.ip += 3;
                        }
                        else if (mod.mod == 2)
                        {
                            command.push_back(0x20);
                            command.push_back(0x10);
                            regs_expect.ip += 4;
                            address += 0x1020;
                        }
                        else
                        {
                            regs_expect.ip += 2;
                        }

                        bus.write(address, data.op1);
                        regs_expect.*reg16_mapping[mod.reg] = data.result;
                        regs_expect.flags                   = data.flags;
                    });
                tests.push_back(test);
                ++mod.rm;
            } while (mod.rm != 0);
            ++mod.reg;
        } while (mod.reg != 0);
        ++mod.mod;
    } while (mod.mod != 3);

    mod.mod = 3;
    do
    {
        mod.reg = 0;
        do
        {
            mod.rm = 0;
            do
            {
                auto test = TestData::for_word(
                    [mod](const TestData::Values &data, Registers &regs_init, Registers &regs_expect,
                          BusType &bus, std::vector<uint8_t> &command)
                    {
                        static_cast<void>(bus);
                        regs_init.*reg16_mapping[mod.reg] = data.op0;
                        regs_init.*reg16_mapping[mod.rm]  = data.op1;
                        regs_expect                       = regs_init;
                        regs_expect.flags                 = data.flags;
                        if (mod.reg == mod.rm)
                        {
                            // Danger, this may broke parity flag, so we need to fix it
                            uint16_t part = static_cast<uint16_t>((data.op0 + data.op1) / 2);
                            if (std::bitset<8>(static_cast<uint8_t>(part + part)).count() % 2 == 0)
                            {
                                regs_expect.flags.p = true;
                            }
                            else
                            {
                                regs_expect.flags.p = false;
                            }
                            regs_init.*reg16_mapping[mod.reg]  = part;
                            regs_expect.*reg16_mapping[mod.rm] = static_cast<uint16_t>(part * 2);
                            if (regs_init.flags.c)
                            {
                                regs_expect.*reg16_mapping[mod.rm] += 1;
                            }
                            if (part != 0x7a)
                            {
                                regs_expect.flags.a = false;
                            }
                            else
                            {
                                regs_expect.flags.o = true;
                                regs_expect.flags.a = true;
                            }
                        }
                        else
                        {
                            regs_expect.*reg16_mapping[mod.reg] = static_cast<uint16_t>(data.result);
                        }
                        regs_expect.ip += 2;
                        command.push_back(mod);
                    });
                tests.push_back(test);
                ++mod.rm;
            } while (mod.rm != 0);
            ++mod.reg;
        } while (mod.reg != 0);
        ++mod.mod;
    } while (mod.mod != 0);


    AdcTestsParams param(0x13, tests, "0x13");
    return param;
}


auto get_adc_test_parameters()
{
    return ::testing::Values(
        AdcTestsParams(0x14,
                       {TestData::for_byte(
                           [](const TestData::Values &data, Registers &regs_init, Registers &regs_expect,
                              BusType &, std::vector<uint8_t> &command)
                           {
                               regs_init.ax   = data.op0;
                               regs_expect    = regs_init;
                               regs_expect.ax = data.result;
                               regs_expect.ip += 2;
                               regs_expect.flags = data.flags;
                               command.push_back(static_cast<uint8_t>(data.op1));
                           })},
                       "0x14"),
        AdcTestsParams(0x15,
                       {TestData::for_word(
                           [](const TestData::Values &data, Registers &regs_init, Registers &regs_expect,
                              BusType &, std::vector<uint8_t> &command)

                           {
                               regs_init.ax   = data.op0;
                               regs_expect    = regs_init;
                               regs_expect.ax = data.result;
                               regs_expect.ip += 2;
                               regs_expect.flags = data.flags;
                               command.push_back(static_cast<uint8_t>(data.op1));
                               command.push_back(static_cast<uint8_t>(data.op1 >> 8));
                           })},
                       "0x15"),
        generate_modrm_to_reg8(), generate_modrm_to_reg16());
}

void stringify_array(const auto &data, auto &str)
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

inline std::string print_test_case_info(const std::string &err, const TestData::Values &test,
                                        const AdcTestsParams &param, const std::vector<uint8_t> &data)
{
    std::stringstream str;
    str << "error msg : " << err << std::endl;
    str << "Test  loc : " << test.loc.file_name() << ":" << test.loc.line() << std::endl;
    str << "Paran loc : " << param.loc.file_name() << ":" << param.loc.line() << std::endl;
    str << "cmd: {";
    stringify_array(data, str);
    str << "}" << std::endl;
    return str.str();
}

} // namespace


class AdcTests : public ParametrizedTestBase<AdcTestsParams>
{
};

TEST_P(AdcTests, ProcessCmd)
{
    const AdcTestsParams &param = GetParam();
    for (const auto &test : param.test)
    {
        Registers regs_init{};
        Registers expected{};
        std::vector<uint8_t> command{};
        for (const auto &data : test.values)
        {
            command = {param.cmd};

            test.init_reg(data, regs_init, expected, bus_, command);
            sut_.set_registers(regs_init);
            const uint32_t address =
                (static_cast<uint32_t>(sut_.get_registers().cs) << 4) + sut_.get_registers().ip;

            bus_.write(address, command);
            sut_.step();
            EXPECT_EQ(sut_.get_registers(), expected)
                << print_test_case_info(sut_.get_error(), data, param, command);
            regs_init = sut_.get_registers();
        }
    }
}


inline void PrintTo(const AdcTestsParams &param, ::std::ostream *os)
{
    *os << "name: " << get_name(param.cmd) << std::endl;
}

INSTANTIATE_TEST_CASE_P(OpcodesAdc, AdcTests, get_adc_test_parameters(),
                        generate_test_case_name<AdcTestsParams>);

} // namespace msemu::cpu8086
