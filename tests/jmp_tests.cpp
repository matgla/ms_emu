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

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "test_base.hpp"

// namespace msemu::cpu8086
// {

// struct JmpTestsParams
// {
//     std::string name;
// };

// class JmpTests : public test::TestBase<JmpTestsParams>
// {
// };

// TEST_P(JmpTests, ProcessCmd)
// {
//     EXPECT_EQ(true, true);
// }

// auto get_jmp_test_parameters()
// {
//     return ::testing::Values(JmpTestsParams{.name = "dummy"});
// }

// INSTANTIATE_TEST_CASE_P(OpcodesJmp, JmpTests, get_jmp_test_parameters(), test::generate_test_case_name);

// } // namespace msemu::cpu8086
