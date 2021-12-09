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

    using Setter = std::function<void()>;

private:
    TestData(const std::vector<Values> &values, bool is_byte, const Setter &setter)
        : values(values)
        , is_byte(is_byte)
        , set_from(setter)
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
    std::function<void()> set_from;
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

// AdcTestsParams generate_modrm_to_reg8()
// {
//     ModRM mod = 0;

//     std::vector<TestData> tests = {};
//     do
//     {
//         do
//         {
//             do
//             {
//                 auto test = TestData::for_byte([]() {});
//                 tests.push_back(test);
//                 ++mod.rm;
//             } while (mod.rm != 1);
//             ++mod.reg;
//         } while (mod.reg != 1);
//         ++mod.mod;
//     } while (mod.mod != 1);

//     AdcTestsParams param(0x12, tests, "0x12");
//     return param;
// }

auto get_adc_test_parameters()
{
    return ::testing::Values(AdcTestsParams(0x14, {TestData::for_byte([]() {})}, "0x14"),
                             AdcTestsParams(0x15, {TestData::for_word([]() {})}, "0x15"));
    //  generate_modrm_to_reg8());
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
        Registers::Flags expected_flags{};
        std::vector<uint8_t> command{};
        for (const auto &data : test.values)
        {
            regs_init       = Registers{.ax = data.op0};
            regs_init.flags = expected_flags;
            sut_.set_registers(regs_init);
            command = {param.cmd};
            if (test.is_byte)
            {
                command.push_back(static_cast<uint8_t>(data.op1));
            }
            else
            {
                command.push_back(static_cast<uint8_t>(data.op1));
                command.push_back(static_cast<uint8_t>(data.op1 >> 8));
            }

            uint32_t address = sut_.get_registers().ip;
            bus_.write(address, command);
            sut_.step();
            expected_flags   = data.flags;
            Registers expect = {.ax = data.result, .ip = 2, .flags = expected_flags};
            EXPECT_EQ(sut_.get_registers(), expect)
                << print_test_case_info(sut_.get_error(), data, param, command);
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
