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

#include "core_dump.hpp"

#include <cstdio>
#include <string_view>

#include "16_bit_modrm.hpp"

namespace msemu::cpu8086
{

namespace
{
constexpr uint8_t get_mod(uint8_t byte)
{
    return (byte >> 6) & 0x03;
}

constexpr uint8_t get_rm(uint8_t byte)
{
    return byte & 0x07;
}

constexpr uint8_t get_reg_op(uint8_t byte)
{
    return (byte >> 3) & 0x07;
}


} // namespace

void puts_many(const char* str, std::size_t times, bool newline)
{
    for (std::size_t i = 0; i < times; ++i)
    {
        fputs(str, stdout);
    }
    if (newline)
    {
        puts("");
    }
}

void print_table_top(std::size_t columns, std::size_t size, bool newline)
{
    puts_many(left_top, 1, false);
    for (std::size_t column = 0; column < columns - 1; ++column)
    {
        puts_many(horizontal, size, false);
        puts_many(cross_top, 1, false);
    }
    puts_many(horizontal, size, false);
    puts_many(right_top, 1, newline);
}

void print_table_bottom(std::size_t columns, std::size_t size)
{
    if (columns == 0)
    {
        columns = 1;
    }

    puts_many(left_bottom, 1, false);
    for (std::size_t column = 0; column < columns - 1; ++column)
    {
        puts_many(horizontal, size, false);
        puts_many(cross_bottom, 1, false);
    }
    puts_many(horizontal, size, false);
    puts_many(right_bottom, 1);
}

uint8_t getmodrm_command_size(const ModRM mod)
{
    switch (mod.mod)
    {
        case 0:
            return 0;
        case 1:
            return 1;
        case 2:
            return 2;
        case 3:
            return 0;
    }
    return 0;
}

namespace
{
char mod0_mapping[8][9]  = {"bx+si", "bx+di", "bp+si", "bp+di", "si", "di", "", "bx"};
char mod1_mapping[8][20] = {"bx+si", "bx+di", "bp+si", "bp+di", "si", "di", "bp", "bx"};
char mod2_mapping[8][30] = {"bx+si", "bx+di", "bp+si", "bp+di", "si", "di", "bp", "bx"};

char reg8_mapping[8][3]  = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
char reg16_mapping[8][3] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
char sreg_mapping[4][3]  = {"es", "cs", "ss", "ds"};

std::pair<const char*, const char*> get_modrm_8_mapping(const ModRM mod)
{
    switch (mod.mod)
    {
        case 0:
        {
            return std::make_pair(mod0_mapping[mod.rm], reg8_mapping[mod.reg]);
        }
        case 1:
        {
            return std::make_pair(mod1_mapping[mod.rm], reg8_mapping[mod.reg]);
        }
        case 2:
        {
            return std::make_pair(mod2_mapping[mod.rm], reg8_mapping[mod.reg]);
        }
        case 3:
        {
            return std::make_pair(reg8_mapping[mod.rm], reg8_mapping[mod.reg]);
        }
    }
    return std::make_pair("unk", "unk");
}

std::pair<const char*, const char*> get_modrm_16_mapping(const ModRM mod)
{
    switch (mod.mod)
    {
        case 0:
        {
            return std::make_pair(mod0_mapping[mod.rm], reg16_mapping[mod.reg]);
        }
        case 1:
        {
            return std::make_pair(mod1_mapping[mod.rm], reg16_mapping[mod.reg]);
        }
        case 2:
        {
            return std::make_pair(mod2_mapping[mod.rm], reg16_mapping[mod.reg]);
        }
        case 3:
        {
            return std::make_pair(reg8_mapping[mod.rm], reg16_mapping[mod.reg]);
        }
    }
    return std::make_pair("unk", "unk");
}

enum class SectionMod
{
    ES   = 0x00,
    CS   = 0x01,
    SS   = 0x02,
    DS   = 0x03,
    None = 0x04,
};

static SectionMod mod = SectionMod::None;

void get_address_string(char* str, const std::size_t size, const char* mod_name, uint8_t data[6],
                        const ModRM modrm, const char* name)
{
    if (!mod_name)
    {
        if (modrm.mod == 0 && modrm.rm == 6)
        {
            snprintf(str, size, "[0x%02x%02x]", data[2], data[1]);
        }
        else if (modrm.mod == 0)
        {
            snprintf(str, size, "[%s]", name);
        }
        else if (modrm.mod == 1)
        {
            snprintf(str, size, "[%s+0x%02x]", name, data[1]);
        }
        else if (modrm.mod == 2)
        {
            snprintf(str, size, "[%s+0x%02x%02x]", name, data[2], data[1]);
        }
        else if (modrm.mod == 3)
        {
            snprintf(str, size, "%s", name);
        }
    }
    else
    {
        if (modrm.mod == 0 && modrm.rm == 6)
        {
            snprintf(str, size, "[%s:0x%02x%02x]", mod_name, data[2], data[1]);
        }
        else if (modrm.mod == 0)
        {
            snprintf(str, size, "[%s:%s]", mod_name, name);
        }
        else if (modrm.mod == 1)
        {
            snprintf(str, size, "[%s:%s+0x%02x]", mod_name, name, data[1]);
        }
        else if (modrm.mod == 2)
        {
            snprintf(str, size, "[%s:%s+0x%02x%02x]", mod_name, name, data[2], data[1]);
        }
        else if (modrm.mod == 3)
        {
            snprintf(str, size, "%s:%s", mod_name, name);
        }
    }
}

uint8_t get_modrm_size(const ModRM modrm)
{
    if (modrm.mod == 0 && modrm.rm == 6)
    {
        return 2;
    }
    return modrm.mod < 3 ? modrm.mod + 1 : 1;
}

uint8_t print_modrm8_from_reg(char* line, std::size_t max_size, std::string_view command, uint8_t data[6],
                              const char* mod_name)
{
    const ModRM modrm  = data[0];
    const auto names   = get_modrm_8_mapping(modrm);
    const uint8_t size = get_modrm_size(modrm);

    char address_name[100];
    const char* reg_name = reg8_mapping[modrm.reg];
    get_address_string(address_name, sizeof(address_name), mod_name, data, modrm, names.first);
    snprintf(line, max_size, "%s %s,%s", command.data(), address_name, reg_name);
    return size;
}

uint8_t print_modrm16_from_reg(char* line, std::size_t max_size, std::string_view command, uint8_t data[6],
                               const char* mod_name)
{
    const ModRM modrm = data[0];
    const auto names  = get_modrm_16_mapping(modrm);
    uint8_t size      = get_modrm_size(modrm);

    char address_name[100];
    const char* reg_name = reg16_mapping[modrm.reg];
    get_address_string(address_name, sizeof(address_name), mod_name, data, modrm, names.first);
    snprintf(line, max_size, "%s %s,%s", command.data(), address_name, reg_name);
    return size;
}

uint8_t print_reg_from_modrm8(char* line, std::size_t max_size, std::string_view command, uint8_t data[6],
                              const char* mod_name)
{
    const ModRM modrm  = data[0];
    const auto names   = get_modrm_8_mapping(modrm);
    const uint8_t size = get_modrm_size(modrm);

    char address_name[100];
    const char* reg_name = reg8_mapping[modrm.reg];
    get_address_string(address_name, sizeof(address_name), mod_name, data, modrm, names.second);
    snprintf(line, max_size, "%s %s,%s", command.data(), reg_name, address_name);
    return size;
}

uint8_t print_reg_from_modrm16(char* line, std::size_t max_size, std::string_view command, uint8_t data[6],
                               const char* mod_name)
{
    const ModRM modrm = data[0];
    const auto names  = get_modrm_16_mapping(modrm);
    uint8_t size      = get_modrm_size(modrm);

    char address_name[100];
    const char* reg_name = reg16_mapping[modrm.reg];
    get_address_string(address_name, sizeof(address_name), mod_name, data, modrm, names.second);
    snprintf(line, max_size, "%s %s,%s", command.data(), reg_name, address_name);
    return size;
}


uint8_t print_imm8(char* line, std::size_t max_size, std::string_view command, std::string_view dest,
                   uint8_t data[6])
{
    snprintf(line, max_size, "%s %s,0x%02x", command.data(), dest.data(), data[0]);
    return 2;
}

uint8_t print_imm16(char* line, std::size_t max_size, std::string_view command, std::string_view dest,
                    uint8_t data[6])
{
    snprintf(line, max_size, "%s %s,0x%02x%02x", command.data(), dest.data(), data[1], data[0]);
    return 2;
}


} // namespace

uint8_t opcode_to_command(char* line, std::size_t max_size, std::size_t opcode, uint8_t data[6],
                          std::size_t ip)
{
    const char* mod_name = nullptr;
    if (mod != SectionMod::None)
    {
        mod_name = sreg_mapping[static_cast<uint32_t>(mod)];
        mod      = SectionMod::None;
    }


    switch (opcode)
    {
        case 0x37:
        {
            snprintf(line, max_size, "aaa");
            return 1;
        }
        case 0xd5:
        {
            snprintf(line, max_size, "aad");
            return 2;
        }
        case 0xd4:
        {
            snprintf(line, max_size, "aam");
            return 2;
        }
        case 0x3f:
        {
            snprintf(line, max_size, "aas");
            return 1;
        }
        case 0x14:
        {
            return print_imm8(line, max_size, "adc", "al", data);
        }
        case 0x15:
        {
            return print_imm16(line, max_size, "adc", "ax", data);
        }
        case 0x12:
        {
            return print_reg_from_modrm8(line, max_size, "adc", data, mod_name);
        }
        case 0x13:
        {
            return print_reg_from_modrm16(line, max_size, "adc", data, mod_name);
        }

        case 0x00:
        {
            return print_modrm8_from_reg(line, max_size, "add", data, mod_name);
        }
        case 0x26:
        {
            mod = SectionMod::ES;
            return 0;
        }
        case 0x36:
        {
            mod = SectionMod::SS;
            return 0;
        }
        case 0x2e:
        {
            mod = SectionMod::CS;
            return 0;
        }
        case 0x3e:
        {
            mod = SectionMod::DS;
            return 0;
        }
        case 0x31:
        {
            auto names = get_modrm_8_mapping(data[0]);
            snprintf(line, max_size, "xor %s,%s", names.first, names.second);
            return 2 + getmodrm_command_size(data[0]);
        }
        case 0xeb:
        {
            const uint8_t address = static_cast<uint8_t>(ip + 2u + data[0]);
            snprintf(line, max_size, "jmp 0x%02x", address);
            return 2;
        }
        break;
        case 0x48:
        case 0x49:
        case 0x4a:
        case 0x4b:
        case 0x4c:
        case 0x4d:
        case 0x4e:
        case 0x4f:
        {
            snprintf(line, max_size, "dec %s", reg16_mapping[(opcode & 0xf) - 8]);
        }
        break;
        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x54:
        case 0x55:
        case 0x56:
        case 0x57:
        {
            snprintf(line, max_size, "push %s", reg16_mapping[(opcode & 0x7)]);
            return 1;
        }
        case 0x58:
        case 0x59:
        case 0x5a:
        case 0x5b:
        case 0x5c:
        case 0x5d:
        case 0x5e:
        case 0x5f:
        {
            snprintf(line, max_size, "pop %s", reg16_mapping[(opcode & 0x07)]);
            return 1;
        }
        case 0x07:
        {
            snprintf(line, max_size, "pop es");
            return 1;
        }
        case 0x17:
        {
            snprintf(line, max_size, "pop ss");
            return 1;
        }
        case 0x1f:
        {
            snprintf(line, max_size, "pop ds");
            return 1;
        }
        case 0x06:
        {
            snprintf(line, max_size, "push es");
            return 1;
        }
        case 0x0e:
        {
            snprintf(line, max_size, "push cs");
            return 1;
        }
        case 0x16:
        {
            snprintf(line, max_size, "push ss");
            return 1;
        }
        case 0x1e:
        {
            snprintf(line, max_size, "push ds");
            return 1;
        }
        case 0x88:
        {
            return print_modrm8_from_reg(line, max_size, "mov", data, mod_name) + 2;
        }
        case 0x89:
        {
            return print_modrm16_from_reg(line, max_size, "mov", data, mod_name) + 2;
        }
        break;
        case 0x8e:
        {
            snprintf(line, max_size, "mov %s,%s", sreg_mapping[get_reg_op(data[0]) & 0x3],
                     reg16_mapping[get_rm(data[0])]);
            return 2;
        }
        break;
        case 0xaa:
        {
            snprintf(line, max_size, "stosb");
            return 1;
        }
        case 0xab:
        {
            snprintf(line, max_size, "stosw");
            return 1;
        }
        case 0xb0:
        case 0xb1:
        case 0xb2:
        case 0xb3:
        case 0xb4:
        case 0xb5:
        case 0xb6:
        case 0xb7:
        {
            snprintf(line, max_size, "mov %s,0x%02x", reg8_mapping[opcode & 0xf], data[0]);
        }
        break;
        case 0xb8:
        case 0xb9:
        case 0xba:
        case 0xbb:
        case 0xbc:
        case 0xbd:
        case 0xbe:
        case 0xbf:
        {
            snprintf(line, max_size, "mov %s,0x%02x%02x", reg16_mapping[(opcode & 0xf) - 8], data[1],
                     data[0]);
            return 3;
        }
        break;
        case 0xc3:
        {
            snprintf(line, max_size, "ret");
        }
        break;
        case 0xcc:
        case 0xcd:
        {
            snprintf(line, max_size, "int %02x", data[0]);
            return 1;
        }
        case 0xfc:
        {
            snprintf(line, max_size, "cld");
            return 1;
        };
        default:
        {
            snprintf(line, max_size, "- - -");
        }
    }
    return 1;
} // namespace

} // namespace msemu::cpu8086