/*
bitops_traits.h - helper for using bitops with enum types
Copyright (C) 2025 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#pragma once
#include <type_traits>

template <typename T>
struct TEnableBitOps {
    static constexpr bool enable = false;
};

template <typename T>
typename std::enable_if_t<TEnableBitOps<T>::enable, T> operator|(T lhs, T rhs) {
    return static_cast<T>(static_cast<std::underlying_type_t<T>>(lhs) |
                          static_cast<std::underlying_type_t<T>>(rhs));
}

template <typename T>
typename std::enable_if_t<TEnableBitOps<T>::enable, T> operator&(T lhs, T rhs) {
    return static_cast<T>(static_cast<std::underlying_type_t<T>>(lhs) &
                          static_cast<std::underlying_type_t<T>>(rhs));
}

template <typename T>
typename std::enable_if_t<TEnableBitOps<T>::enable, T> operator^(T lhs, T rhs) {
    return static_cast<T>(static_cast<std::underlying_type_t<T>>(lhs) ^
                          static_cast<std::underlying_type_t<T>>(rhs));
}

template <typename T>
typename std::enable_if_t<TEnableBitOps<T>::enable, T> operator~(T value) {
    return static_cast<T>(~static_cast<std::underlying_type_t<T>>(value));
}

#define DEFINE_ENUM_BITOPS( T ) \
    template <> \
    struct TEnableBitOps<T> { \
        static constexpr bool enable = true; \
    };
