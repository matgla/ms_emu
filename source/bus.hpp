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

#pragma once

#include <cassert>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>

#include "memory.hpp"

namespace msemu
{

template <typename... T>
class Bus
{
public:
    Bus(T... t)
        : devices_(std::forward<T>(t)...)
    {
    }

    void print() const
    {
        print_impl();
    }

    const ConstMemoryView get(const char* name) const
    {
        return get_by_name_impl(name);
    }

    MemoryView get(const char* name)
    {
        return get_by_name_impl(name);
    }

    void clear()
    {
        clear_impl();
    }

    void write(const uint32_t address, const std::span<const uint8_t> data)
    {
        get_by_address_impl(address).write(address, data);
    }

    template <typename DataType>
    DataType read(const uint32_t address)
    {
        return get_by_address_impl(address).template read<DataType>(address);
    }

    void read(const uint32_t address, std::span<uint8_t> data)
    {
        get_by_address_impl(address).read(address, data);
    }

    void write(const uint32_t address, const auto data)
    {
        get_by_address_impl(address).write(address, data);
    }

private:
    using Devices = std::tuple<T...>;

    template <std::size_t I = 0>
    inline void print_impl() const
    {
        if constexpr (I == sizeof...(T))
        {
            return;
        }
        else
        {
            std::get<I>(devices_).print();
            print_impl<I + 1>();
        }
    }

    template <std::size_t I = 0>
    inline ConstMemoryView get_by_name_impl(std::string_view name) const
    {
        if constexpr (I == sizeof...(T))
        {
            return ConstMemoryView(std::span<const uint8_t>(), 0);
        }
        else
        {
            if (std::get<I>(devices_).name() == name)
            {
                return ConstMemoryView(std::get<I>(devices_).span(), std::get<I>(devices_).start_address);
            }
            return get_by_name_impl<I + 1>(name);
        }
    }

    template <std::size_t I = 0>
    inline MemoryView get_by_name_impl(std::string_view name)
    {
        if constexpr (I == sizeof...(T))
        {
            return MemoryView(std::span<uint8_t>(), 0);
        }
        else
        {
            if (std::get<I>(devices_).name() == name)
            {
                return MemoryView(std::get<I>(devices_).span(), std::get<I>(devices_).start_address);
            }
            return get_by_name_impl<I + 1>(name);
        }
    }


    template <std::size_t I = 0>
    inline MemoryView get_by_address_impl(const uint32_t address)
    {
        if constexpr (I == sizeof...(T))
        {
            return MemoryView(std::span<uint8_t>(), 0);
        }
        else
        {
            const auto& device = std::get<I>(devices_);
            if (address >= device.start_address && address < device.end_address)
            {
                return MemoryView(std::get<I>(devices_).span(), std::get<I>(devices_).start_address);
            }
            return get_by_address_impl<I + 1>(address);
        }
    }

    template <std::size_t I = 0>
    inline ConstMemoryView get_by_address_impl(const uint32_t address) const
    {
        if constexpr (I == sizeof...(T))
        {
            return ConstMemoryView(std::span<const uint8_t>(), 0);
        }
        else
        {
            const auto& device = std::get<I>(devices_);
            if (address >= device.start_address && address < device.end_address)
            {
                return ConstMemoryView(std::get<I>(devices_).span(), std::get<I>(devices_).start_address);
            }
            return get_by_address_impl<I + 1>(address);
        }
    }

    template <std::size_t I = 0>
    inline void clear_impl()
    {
        if constexpr (I == sizeof...(T))
        {
            return;
        }
        else
        {
            std::get<I>(devices_).clear();
            return clear_impl<I + 1>();
        }
    }

    std::tuple<T...> devices_;
};

template <typename... T>
Bus(T&&...) -> Bus<std::decay_t<T>...>;

} // namespace msemu
