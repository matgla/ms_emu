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

class AamTests : public TestBase
{
};

TEST_F(AamTests, ProcessCmd_0xd4)
{
    std::vector<uint8_t> cmd = {0xd4, 0x0a, 0xd4, 0x0a};
    bus_.write(0, cmd);


    sut_.set_registers(Registers{.ax = 0xffff});
    sut_.step();
    EXPECT_EQ(sut_.get_registers().al, 0x05);
    EXPECT_FALSE(sut_.get_registers().flags.s);
    EXPECT_FALSE(sut_.get_registers().flags.z);
    EXPECT_TRUE(sut_.get_registers().flags.p);

    EXPECT_EQ(sut_.get_registers().ah, 0x19);
    EXPECT_EQ(sut_.last_instruction_cost(), 83);

    sut_.set_registers(Registers{.ax = 0x0000});
    sut_.step();
    EXPECT_EQ(sut_.get_registers().al, 0x00);
    EXPECT_FALSE(sut_.get_registers().flags.s);
    EXPECT_TRUE(sut_.get_registers().flags.z);
    EXPECT_TRUE(sut_.get_registers().flags.p);

    EXPECT_EQ(sut_.get_registers().ah, 0x00);
    EXPECT_EQ(sut_.last_instruction_cost(), 83);

    sut_.set_registers(Registers{.ax = 0x1020});
    sut_.step();
    EXPECT_EQ(sut_.get_registers().al, 0x02);
    EXPECT_FALSE(sut_.get_registers().flags.s);
    EXPECT_FALSE(sut_.get_registers().flags.z);
    EXPECT_FALSE(sut_.get_registers().flags.p);

    EXPECT_EQ(sut_.get_registers().ah, 0x03);
    EXPECT_EQ(sut_.last_instruction_cost(), 83);
}


} // namespace msemu::cpu8086
