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

#pragma once

#include <cstdint>

namespace msemu::cpu8086
{

#ifdef PICO
register uint32_t r5 asm("r5");
register uint32_t r6 asm("r6");
register uint32_t r7 asm("r7");
register uint32_t r8 asm("r8");
register uint32_t r9 asm("r9");
register uint16_t r10 asm("r10");
register uint32_t r11 asm("r11");


#else
uint32_t r5  = 0;
uint32_t r6  = 0;
uint32_t r7  = 0;
uint32_t r8  = 0;
uint32_t r9  = 0;
uint16_t r10 = 0;
uint32_t r11 = 0;
#endif

#define set_reg8(reg, val, mask, offset)                                                 \
    {                                                                                    \
        reg = reg & (~mask);                                                             \
        reg = reg | (static_cast<uint32_t>(val) << offset);                              \
    }

#define set_reg16(reg, val, mask, offset)                                                \
    {                                                                                    \
        reg = reg & (~mask);                                                             \
        reg = reg | (static_cast<uint32_t>(val) << offset);                              \
    }

#define get_reg8(reg, mask, offset) (static_cast<uint8_t>((reg >> offset) & 0xff))

#define get_reg16(reg, mask, offset) (static_cast<uint16_t>((reg >> offset) & 0xffff))

struct Register
{
private:
    constexpr static uint32_t ax_mask   = 0x0000ffff;
    constexpr static uint32_t ax_offset = 0;
    constexpr static uint32_t al_mask   = 0x000000ff;
    constexpr static uint32_t al_offset = 0;
    constexpr static uint32_t ah_mask   = 0x0000ff00;
    constexpr static uint32_t ah_offset = 8;


    constexpr static uint32_t bx_mask   = 0xffff0000;
    constexpr static uint32_t bx_offset = 16;
    constexpr static uint32_t bl_mask   = 0x00ff0000;
    constexpr static uint32_t bl_offset = 16;
    constexpr static uint32_t bh_mask   = 0xff000000;
    constexpr static uint32_t bh_offset = 24;

    constexpr static uint32_t cx_mask   = 0x0000ffff;
    constexpr static uint32_t cx_offset = 0;
    constexpr static uint32_t cl_mask   = 0x000000ff;
    constexpr static uint32_t cl_offset = 0;
    constexpr static uint32_t ch_mask   = 0x0000ff00;
    constexpr static uint32_t ch_offset = 8;


    constexpr static uint32_t dx_mask   = 0xffff0000;
    constexpr static uint32_t dx_offset = 16;
    constexpr static uint32_t dl_mask   = 0x00ff0000;
    constexpr static uint32_t dl_offset = 16;
    constexpr static uint32_t dh_mask   = 0xff000000;
    constexpr static uint32_t dh_offset = 24;

    constexpr static uint32_t sp_mask   = 0x0000ffff;
    constexpr static uint32_t sp_offset = 0;

    constexpr static uint32_t bp_mask   = 0xffff0000;
    constexpr static uint32_t bp_offset = 16;

    constexpr static uint32_t si_mask   = 0x0000ffff;
    constexpr static uint32_t si_offset = 0;

    constexpr static uint32_t di_mask   = 0xffff0000;
    constexpr static uint32_t di_offset = 16;

    constexpr static uint32_t cs_mask   = 0x0000ffff;
    constexpr static uint32_t cs_offset = 0;

    constexpr static uint32_t ds_mask   = 0xffff0000;
    constexpr static uint32_t ds_offset = 16;

    constexpr static uint32_t ss_mask   = 0x0000ffff;
    constexpr static uint32_t ss_offset = 0;

    constexpr static uint32_t es_mask   = 0xffff0000;
    constexpr static uint32_t es_offset = 16;


public:
    constexpr static uint32_t ax_id = 0;
    constexpr static uint32_t al_id = 1;
    constexpr static uint32_t ah_id = 2;
    constexpr static uint32_t bx_id = 3;
    constexpr static uint32_t bl_id = 4;
    constexpr static uint32_t bh_id = 5;
    constexpr static uint32_t cx_id = 6;
    constexpr static uint32_t cl_id = 7;
    constexpr static uint32_t ch_id = 8;
    constexpr static uint32_t dx_id = 9;
    constexpr static uint32_t dl_id = 10;
    constexpr static uint32_t dh_id = 11;

    constexpr static uint32_t sp_id = 12;
    constexpr static uint32_t bp_id = 13;
    constexpr static uint32_t si_id = 14;
    constexpr static uint32_t di_id = 15;

    constexpr static uint32_t cs_id = 16;
    constexpr static uint32_t ds_id = 17;
    constexpr static uint32_t ss_id = 18;
    constexpr static uint32_t es_id = 19;


    static inline void reset()
    {
        r5  = 0;
        r6  = 0;
        r7  = 0;
        r8  = 0;
        r9  = 0;
        r10 = 0;
        r11 = 0;
    }

    static inline uint16_t ax()
    {
        return get_reg16(r5, ax_mask, ax_offset);
    }

    static inline void ax(uint16_t v)
    {
        set_reg16(r5, v, ax_mask, ax_offset);
    }

    static inline void al(uint8_t v)
    {
        set_reg8(r5, v, al_mask, al_offset);
    }

    static inline uint8_t al()
    {
        return get_reg8(r5, al_mask, al_offset);
    }

    static inline void ah(uint8_t v)
    {
        set_reg8(r5, v, ah_mask, ah_offset);
    }

    static inline uint8_t ah()
    {
        return get_reg8(r5, ah_mask, ah_offset);
    }

    static inline uint16_t bx()
    {
        return get_reg16(r5, bx_mask, bx_offset);
    }

    static inline void bx(uint16_t v)
    {
        set_reg16(r5, v, bx_mask, bx_offset);
    }

    static inline void bl(uint8_t v)
    {
        set_reg8(r5, v, bl_mask, bl_offset);
    }

    static inline uint8_t bl()
    {
        return get_reg8(r5, bl_mask, bl_offset);
    }

    static inline void bh(uint8_t v)
    {
        set_reg8(r5, v, bh_mask, bh_offset);
    }

    static inline uint8_t bh()
    {
        return get_reg8(r5, bh_mask, bh_offset);
    }

    static inline uint16_t cx()
    {
        return get_reg16(r6, cx_mask, cx_offset);
    }

    static inline void cx(uint16_t v)
    {
        set_reg16(r6, v, cx_mask, cx_offset);
    }

    static inline void cl(uint8_t v)
    {
        set_reg8(r6, v, cl_mask, cl_offset);
    }

    static inline uint8_t cl()
    {
        return get_reg8(r6, cl_mask, cl_offset);
    }

    static inline void ch(uint8_t v)
    {
        set_reg8(r6, v, ch_mask, ch_offset);
    }

    static inline uint8_t ch()
    {
        return get_reg8(r6, ch_mask, ch_offset);
    }

    static inline uint16_t dx()
    {
        return get_reg16(r6, dx_mask, dx_offset);
    }

    static inline void dx(uint16_t v)
    {
        set_reg16(r6, v, dx_mask, dx_offset);
    }

    static inline void dl(uint8_t v)
    {
        set_reg8(r6, v, dl_mask, dl_offset);
    }

    static inline uint8_t dl()
    {
        return get_reg8(r6, dl_mask, dl_offset);
    }

    static inline void dh(uint8_t v)
    {
        set_reg8(r6, v, dh_mask, dh_offset);
    }

    static inline uint8_t dh()
    {
        return get_reg8(r6, dh_mask, dh_offset);
    }

    static inline uint16_t sp()
    {
        return get_reg16(r7, sp_mask, sp_offset);
    }

    static inline void sp(uint16_t v)
    {
        set_reg16(r7, v, sp_mask, sp_offset);
    }

    static inline uint16_t bp()
    {
        return get_reg16(r7, bp_mask, bp_offset);
    }

    static inline void bp(uint16_t v)
    {
        set_reg16(r7, v, bp_mask, bp_offset);
    }

    static inline uint16_t si()
    {
        return get_reg16(r8, si_mask, si_offset);
    }

    static inline void si(uint16_t v)
    {
        set_reg16(r8, v, si_mask, si_offset);
    }

    static inline uint16_t di()
    {
        return get_reg16(r8, di_mask, di_offset);
    }

    static inline void di(uint16_t v)
    {
        set_reg16(r8, v, di_mask, di_offset);
    }

    static inline uint16_t cs()
    {
        return get_reg16(r9, cs_mask, cs_offset);
    }

    static inline void cs(uint16_t v)
    {
        set_reg16(r9, v, cs_mask, cs_offset);
    }

    static inline uint16_t ds()
    {
        return get_reg16(r9, ds_mask, ds_offset);
    }

    static inline void ds(uint16_t v)
    {
        set_reg16(r9, v, ds_mask, ds_offset);
    }

    static inline uint16_t ss()
    {
        return get_reg16(r11, ss_mask, ss_offset);
    }

    static inline void ss(uint16_t v)
    {
        set_reg16(r11, v, ss_mask, ss_offset);
    }

    static inline uint16_t es()
    {
        return get_reg16(r11, es_mask, es_offset);
    }

    static inline void es(uint16_t v)
    {
        set_reg16(r11, v, es_mask, es_offset);
    }

    static inline uint16_t ip()
    {
        return r10;
    }


    static inline void ip(uint16_t v)
    {
        r10 = v;
    }

    static inline void increment_ip(uint16_t value = 1)
    {
        r10 = r10 + value;
    }
};

template <uint32_t reg>
inline void set_register_by_id(uint16_t value)
{
    switch (reg)
    {
        case Register::ax_id:
            Register::ax(value);
            break;
        case Register::al_id:
            Register::al(static_cast<uint8_t>(value));
            break;
        case Register::ah_id:
            Register::ah(static_cast<uint8_t>(value));
            break;
        case Register::bx_id:
            Register::bx(value);
            break;
        case Register::bl_id:
            Register::bl(static_cast<uint8_t>(value));
            break;
        case Register::bh_id:
            Register::bh(static_cast<uint8_t>(value));
            break;
        case Register::cx_id:
            Register::cx(value);
            break;
        case Register::cl_id:
            Register::cl(static_cast<uint8_t>(value));
            break;
        case Register::ch_id:
            Register::ch(static_cast<uint8_t>(value));
            break;
        case Register::dx_id:
            Register::dx(value);
            break;
        case Register::dl_id:
            Register::dl(static_cast<uint8_t>(value));
            break;
        case Register::dh_id:
            Register::dh(static_cast<uint8_t>(value));
            break;
        case Register::sp_id:
            Register::sp(value);
            break;
        case Register::bp_id:
            Register::bp(value);
            break;
        case Register::si_id:
            Register::si(value);
            break;
        case Register::di_id:
            Register::di(value);
            break;
        case Register::cs_id:
            Register::cs(value);
            break;
        case Register::ds_id:
            Register::ds(value);
            break;
        case Register::ss_id:
            Register::ss(value);
            break;
        case Register::es_id:
            Register::es(value);
            break;
    }
}

inline void set_register_by_id(uint32_t reg, uint16_t value)
{
    switch (reg)
    {
        case Register::ax_id:
            Register::ax(value);
            break;
        case Register::al_id:
            Register::al(static_cast<uint8_t>(value));
            break;
        case Register::ah_id:
            Register::ah(static_cast<uint8_t>(value));
            break;
        case Register::bx_id:
            Register::bx(value);
            break;
        case Register::bl_id:
            Register::bl(static_cast<uint8_t>(value));
            break;
        case Register::bh_id:
            Register::bh(static_cast<uint8_t>(value));
            break;
        case Register::cx_id:
            Register::cx(value);
            break;
        case Register::cl_id:
            Register::cl(static_cast<uint8_t>(value));
            break;
        case Register::ch_id:
            Register::ch(static_cast<uint8_t>(value));
            break;
        case Register::dx_id:
            Register::dx(value);
            break;
        case Register::dl_id:
            Register::dl(static_cast<uint8_t>(value));
            break;
        case Register::dh_id:
            Register::dh(static_cast<uint8_t>(value));
            break;
        case Register::sp_id:
            Register::sp(value);
            break;
        case Register::bp_id:
            Register::bp(value);
            break;
        case Register::si_id:
            Register::si(value);
            break;
        case Register::di_id:
            Register::di(value);
            break;
        case Register::cs_id:
            Register::cs(value);
            break;
        case Register::ds_id:
            Register::ds(value);
            break;
        case Register::ss_id:
            Register::ss(value);
            break;
        case Register::es_id:
            Register::es(value);
            break;
    }
}

template <uint32_t reg, typename T>
inline T get_register_by_id()
{
    switch (reg)
    {
        case Register::ax_id:
            return static_cast<T>(Register::ax());
        case Register::al_id:
            return static_cast<T>(Register::al());
        case Register::ah_id:
            return static_cast<T>(Register::ah());
        case Register::bx_id:
            return static_cast<T>(Register::bx());
        case Register::bl_id:
            return static_cast<T>(Register::bl());
        case Register::bh_id:
            return static_cast<T>(Register::bh());
        case Register::cx_id:
            return static_cast<T>(Register::cx());
        case Register::cl_id:
            return static_cast<T>(Register::cl());
        case Register::ch_id:
            return static_cast<T>(Register::ch());
        case Register::dx_id:
            return static_cast<T>(Register::dx());
        case Register::dl_id:
            return static_cast<T>(Register::dl());
        case Register::dh_id:
            return static_cast<T>(Register::dh());
        case Register::sp_id:
            return static_cast<T>(Register::sp());
        case Register::bp_id:
            return static_cast<T>(Register::bp());
        case Register::si_id:
            return static_cast<T>(Register::si());
        case Register::di_id:
            return static_cast<T>(Register::di());
        case Register::cs_id:
            return static_cast<T>(Register::cs());
        case Register::ds_id:
            return static_cast<T>(Register::ds());
        case Register::ss_id:
            return static_cast<T>(Register::ss());
        case Register::es_id:
            return static_cast<T>(Register::es());
    }
    return std::numeric_limits<T>::max();
}


template <typename T>
inline T get_register_by_id(uint32_t reg)
{
    switch (reg)
    {
        case Register::ax_id:
            return static_cast<T>(Register::ax());
        case Register::al_id:
            return static_cast<T>(Register::al());
        case Register::ah_id:
            return static_cast<T>(Register::ah());
        case Register::bx_id:
            return static_cast<T>(Register::bx());
        case Register::bl_id:
            return static_cast<T>(Register::bl());
        case Register::bh_id:
            return static_cast<T>(Register::bh());
        case Register::cx_id:
            return static_cast<T>(Register::cx());
        case Register::cl_id:
            return static_cast<T>(Register::cl());
        case Register::ch_id:
            return static_cast<T>(Register::ch());
        case Register::dx_id:
            return static_cast<T>(Register::dx());
        case Register::dl_id:
            return static_cast<T>(Register::dl());
        case Register::dh_id:
            return static_cast<T>(Register::dh());
        case Register::sp_id:
            return static_cast<T>(Register::sp());
        case Register::bp_id:
            return static_cast<T>(Register::bp());
        case Register::si_id:
            return static_cast<T>(Register::si());
        case Register::di_id:
            return static_cast<T>(Register::di());
        case Register::cs_id:
            return static_cast<T>(Register::cs());
        case Register::ds_id:
            return static_cast<T>(Register::ds());
        case Register::ss_id:
            return static_cast<T>(Register::ss());
        case Register::es_id:
            return static_cast<T>(Register::es());
    }
    return std::numeric_limits<T>::max();
}


struct Registers
{
    uint16_t ax;
    uint16_t bx;
    uint16_t cx;
    uint16_t dx;
    uint16_t si; // source index
    uint16_t di; // destination index
    uint16_t bp; // base pointer
    uint16_t sp; // stack pointer
    uint16_t ip; // instruction pointer
    uint16_t cs; // code segment
    uint16_t ds; // data segment
    uint16_t es; // extra segment
    uint16_t ss; // stack segment
    struct
    {
        uint16_t res_0 : 1;
        uint16_t res_1 : 1;
        uint16_t res_2 : 1;
        uint16_t res_3 : 1;
        uint16_t o : 1; // overflow
        uint16_t d : 1; // direction
        uint16_t i : 1; // interrupt
        uint16_t t : 1; // trap
        uint16_t s : 1; // sign
        uint16_t z : 1; // zero
        uint16_t res_4 : 1;
        uint16_t a : 1; // adjust
        uint16_t res_5 : 1;
        uint16_t p : 1; // parity
        uint16_t res_6 : 1;
        uint16_t c : 1; // carry
    } flags;
};

enum class RegisterPart
{
    low,
    high,
    whole
};

namespace _detail
{
template <RegisterPart where>
constexpr uint16_t register_mask()
{
    if constexpr (where == RegisterPart::low)
    {
        return 0xff;
    }
    if constexpr (where == RegisterPart::high)
    {
        return 0xff00;
    }
    return 0xffff;
}

template <RegisterPart where>
constexpr uint16_t register_offset()
{
    if constexpr (where == RegisterPart::low)
    {
        return 0;
    }
    if constexpr (where == RegisterPart::high)
    {
        return 8;
    }
    return 0;
}
} // namespace _detail

template <RegisterPart where>
constexpr inline void set_register(uint16_t& reg, uint16_t value)
{
    if constexpr (where == RegisterPart::low)
    {
        *reinterpret_cast<uint8_t*>(&reg) = static_cast<uint8_t>(value);
    }
    else if constexpr (where == RegisterPart::high)
    {
        reinterpret_cast<uint8_t*>(&reg)[1] = static_cast<uint8_t>(value);
    }
    else
    {
        reg = value;
    }
}

template <RegisterPart where, typename T = uint16_t>
constexpr inline T get_register(uint16_t reg)
{
    if constexpr (where == RegisterPart::low)
    {
        return reinterpret_cast<uint8_t*>(&reg)[0];
    }
    else if constexpr (where == RegisterPart::high)
    {
        return reinterpret_cast<uint8_t*>(&reg)[1];
    }
    else
    {
        return reg;
    }
}

template <RegisterPart p>
struct RegisterDataType
{
    using type = uint8_t;
};

template <>
struct RegisterDataType<RegisterPart::whole>
{
    using type = uint16_t;
};


} // namespace msemu::cpu8086
