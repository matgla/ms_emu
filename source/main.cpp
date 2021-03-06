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

#include <cstdio>
#include <cstdlib>

#include <locale.h>
#include <termios.h>
#include <unistd.h>

#include "bus.hpp"
#include "device.hpp"

#include "8086_cpu.hpp"

static struct termios term_orig;
void disable_buffered_io()
{
    struct termios term;
    int ec;
    if ((ec = tcgetattr(0, &term_orig)))
    {
        printf("ERR: tcgetattr failed with code: %d\n", ec);
        exit(-1);
    }

    term = term_orig;
    term.c_lflag &= static_cast<tcflag_t>(~ICANON);
    term.c_lflag |= ECHO;
    term.c_cc[VMIN]  = 0;
    term.c_cc[VTIME] = 0;

    if ((ec = tcsetattr(0, TCSANOW, &term)))
    {
        printf("tcsetattr failed with code: %d\n", ec);
        exit(-1);
    }
}

void restore_terminal_settings()
{
    tcsetattr(0, TCSANOW, &term_orig);
}

int main(int argc, const char* argv[])
{
    printf("8086 emulator starting\n");
    msemu::Memory<1024 * 128> memory;
    using FlashType = msemu::Device<msemu::Memory<1024 * 128>, 0x00000000>;
    using BiosType  = msemu::Device<msemu::Memory<1024 * 64>, 0x000f0100>;
    // msemu::Device<msemu::Memory<1024 * 128>, 0x> flash("FLASH");
    msemu::Bus bus(FlashType("flash"), BiosType("bios/rom"));
    bus.print();

    if (argc < 2)
    {
        printf("Please provide binary file\n");
        return 0;
    }

    msemu::MemoryView bios_memory = bus.get("bios/rom");
    printf("BIOS size: %x\n", bios_memory.size());
    bios_memory.load_from_file(argv[1]);

    msemu::cpu8086::Cpu cpu(bus);
    cpu.jump_to_bios();

    disable_buffered_io();
    setlocale(LC_CTYPE, "");
    //
    printf("ROM loaded\n");
    char c = '\n';
    while (c != 27)
    {
        if (read(STDIN_FILENO, &c, sizeof(c)) > 0)
        {
            if (c == 's')
            {
                cpu.step();
            }
        }
    }
    restore_terminal_settings();
}
