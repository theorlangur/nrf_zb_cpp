#ifndef ZB_STR_HPP
#define ZB_STR_HPP

#include "zb_types.hpp"
#include <cstring>
#include <string_view>
#include <span>

namespace zb
{
    struct ZigbeeStrView
    {
        char *pStr;

        operator void*() { return pStr; }
        uint8_t size() const { return pStr[0]; }
        std::string_view sv() const { return {pStr + 1, pStr[0]}; }
    };

    struct ZigbeeStrRef
    {
        char sz;

        operator void*() { return this; }
        uint8_t size() const { return sz; }
        std::string_view sv() const { return {&sz + 1, sz}; }
    };

    template<size_t N>
    struct ZigbeeStrBuf: public ZigbeeStrRef
    {
        char data[N];
    };

    struct ZigbeeOctetRef
    {
        uint8_t sz;

        operator void*() { return this; }
        uint8_t size() const { return sz; }
        std::span<const uint8_t> sv() const { return {&sz + 1, sz}; }
    };

    template<size_t N>
    struct ZigbeeOctetBuf: public ZigbeeOctetRef
    {
        uint8_t data[N];
    };

    template<size_t N>
    struct ZigbeeStr
    {
        char name[N];

        template<size_t M, size_t...idx>
        constexpr ZigbeeStr(std::index_sequence<idx...>, const char (&n)[M]):
            name{ M-1, n[idx]... }
        {
        }

        template<size_t M>
        constexpr ZigbeeStr(const char (&n)[M]):
            ZigbeeStr(std::make_index_sequence<M-1>(), n)
        {
        }

        constexpr ZigbeeStr():
            name{0}
        {
        }

        operator void*() { return name; }
        size_t size() const { return N - 1; }
        std::string_view sv() const { return {name + 1, N - 1}; }
        ZigbeeStrView zsv() const { return {name}; }
        ZigbeeStrRef& zsv_ref() { return *(ZigbeeStrRef*)name; }

        template<size_t M>
        ZigbeeStr<N>& operator=(const char (&n)[M])
        {
            static_assert(M <= N, "String literal is too big");
            name[0] = M - 1;
            std::memcpy(name + 1, n, M - 1);
            return *this;
        }

        static constexpr Type TypeId() { return Type::CharStr; }
    };

    template<size_t N>
    struct ZigbeeBin
    {
        uint8_t data[N];

        template<class T, size_t M, size_t...idx>
        constexpr ZigbeeBin(std::index_sequence<idx...>, std::array<T, M> const &n):
            data{ M-1, n[idx]... }
        {
        }

        template<class T, size_t M>
        constexpr ZigbeeBin(std::array<T, M> const& n):
            ZigbeeBin(std::make_index_sequence<M-1>(), n)
        {
        }

        constexpr ZigbeeBin():
            data{0}
        {
        }

        operator void*() { return data; }
        size_t size() const { return N - 1; }
        std::span<const uint8_t> sv() const { return {data + 1, N - 1}; }

        template<size_t M>
        ZigbeeBin<N>& operator=(std::array<uint8_t, M> const& n)
        {
            static_assert(M <= N, "String literal is too big");
            data[0] = M - 1;
            std::memcpy(data + 1, n, M - 1);
            return *this;
        }

        static constexpr Type TypeId() { return Type::OctetStr; }
    };

    template<class T, size_t N> requires (std::is_trivially_copyable_v<T>
            && std::is_trivially_constructible_v<T>)
    struct [[gnu::packed]] ZigbeeBinTyped
    {
        uint8_t len_bytes;
        T data[N];

        template<size_t M, size_t...idx>
        constexpr ZigbeeBinTyped(std::index_sequence<idx...>, std::array<T, M> const &n):
            len_bytes{sizeof(T) * M},
            data{ M-1, n[idx]... }
        {
            static_assert(sizeof(T) * M <= 255);
        }

        template<size_t M>
        constexpr ZigbeeBinTyped(std::array<T, M> const& n):
            ZigbeeBinTyped(std::make_index_sequence<M-1>(), n)
        {
        }

        constexpr ZigbeeBinTyped():
            len_bytes{0}
            , data{}
        {
        }

        operator void*() { return this; }
        bool valid() const { return (len_bytes % sizeof(T)) == 0; }
        size_t size() const { return len_bytes / sizeof(T); }
        size_t max_size() const { return N; }
        size_t size_bytes() const { return N * sizeof(T); }
        std::span<const T> sv() const { return {data, N}; }

        template<class Me>
        auto& operator[](this Me const& t, size_t i) { return t.data[i]; }

        static constexpr Type TypeId() { return Type::OctetStr; }
    };


    template<size_t N>
    constexpr ZigbeeStr<N> ZbStr(const char (&n)[N])
    {
        static_assert(N < 255, "String too long");
        return [&]<size_t...idx>(std::index_sequence<idx...>){
            return ZigbeeStr<N>{.name={N-1, n[idx]...}};
        }(std::make_index_sequence<N-1>());
    }
}
#endif
