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

#include <gtest/gtest.h>

namespace msemu::cpu8086
{

class AaaTests : public TestBase
{
};

TEST_F(AaaTests, ProcessCmd_0x37)
{
    sut_.set_registers(Registers{.al = 0x00});
    std::vector<uint8_t> cmd = {0x37, 0x37, 0x37, 0x37};
    bus_.write(0, cmd);
    sut_.step();
    EXPECT_EQ(sut_.get_registers().al, 0);
    EXPECT_FALSE(sut_.get_registers().flags.a);
    EXPECT_FALSE(sut_.get_registers().flags.c);
    EXPECT_EQ(sut_.get_registers().ah, 0);
    EXPECT_EQ(sut_.last_instruction_cost(), 8);

    sut_.set_registers(Registers{.al = 0x05});
    sut_.step();
    EXPECT_EQ(sut_.get_registers().al, 5);
    EXPECT_FALSE(sut_.get_registers().flags.a);
    EXPECT_FALSE(sut_.get_registers().flags.c);
    EXPECT_EQ(sut_.get_registers().ah, 0);
    EXPECT_EQ(sut_.last_instruction_cost(), 8);


    sut_.set_registers(Registers{.al = 0x05, .flags = {.c = 1}});
    sut_.step();
    EXPECT_EQ(sut_.get_registers().al, 5);
    EXPECT_FALSE(sut_.get_registers().flags.a);
    EXPECT_FALSE(sut_.get_registers().flags.c);
    EXPECT_EQ(sut_.get_registers().ah, 0);
    EXPECT_EQ(sut_.last_instruction_cost(), 8);


    sut_.set_registers(Registers{.al = 0x01, .flags{.a = 1}});
    sut_.step();
    EXPECT_EQ(sut_.get_registers().al, 0x07);
    EXPECT_TRUE(sut_.get_registers().flags.a);
    EXPECT_TRUE(sut_.get_registers().flags.c);
    EXPECT_EQ(sut_.get_registers().ah, 1);
    EXPECT_EQ(sut_.last_instruction_cost(), 8);


    sut_.set_registers(Registers{.al = 0x0a});
    sut_.step();
    EXPECT_EQ(sut_.get_registers().al, 0);
    EXPECT_TRUE(sut_.get_registers().flags.a);
    EXPECT_TRUE(sut_.get_registers().flags.c);
    EXPECT_EQ(sut_.get_registers().ah, 1);
    EXPECT_EQ(sut_.last_instruction_cost(), 8);


    sut_.set_registers(Registers{.al = 0x0b});
    sut_.step();
    EXPECT_EQ(sut_.get_registers().al, 1);
    EXPECT_TRUE(sut_.get_registers().flags.a);
    EXPECT_TRUE(sut_.get_registers().flags.c);
    EXPECT_EQ(sut_.get_registers().ah, 1);
    EXPECT_EQ(sut_.last_instruction_cost(), 8);
}


} // namespace msemu::cpu8086
