#ifndef ZB_STR_HPP
#define ZB_STR_HPP

#include "zb_types.hpp"
#include <cstring>
#include <string_view>
#include <span>

namespace zb
{
    struct zigbee_str_view_t
    {
        char *pStr;

        operator void*() { return pStr; }
        uint8_t size() const { return pStr[0]; }
        std::string_view sv() const { return {pStr + 1, pStr[0]}; }


        std::optional<const uint8_t*> serialize_from(const uint8_t *pSrc, size_t limit)
        {
            return std::nullopt;
        }

        std::optional<uint8_t*> serialize_to(uint8_t *pDst, size_t limit) const
        {
            if (size() >= limit)
                return std::nullopt;

            *pDst = size();
            std::memcpy(pDst + 1, pStr + 1, *pDst);
            return pDst + *pDst + 1;
        }
    };

    struct zigbee_str_ref_t
    {
        using __1byte_var_len = void;
        char sz;

        operator void*() { return this; }
        uint8_t size() const { return sz; }
        std::string_view sv() const { return {&sz + 1, sz}; }
    };

    template<size_t N>
    struct zigbee_str_buf_t: public zigbee_str_ref_t
    {
        char data[N];
    };

    struct zigbee_octet_ref_t
    {
        using __1byte_var_len = void;
        uint8_t sz;

        operator void*() { return this; }
        uint8_t size() const { return sz; }
        std::span<const uint8_t> sv() const { return {&sz + 1, sz}; }
    };

    template<size_t N>
    struct zigbee_octet_buf_t: public zigbee_octet_ref_t
    {
        uint8_t data[N];
    };

    template<size_t N>
    struct zigbee_str_t
    {
        using __1byte_var_len = void;
        char name[N];

        template<size_t M, size_t...idx>
        constexpr zigbee_str_t(std::index_sequence<idx...>, const char (&n)[M]):
            name{ M-1, n[idx]... }
        {
        }

        template<size_t M>
        constexpr zigbee_str_t(const char (&n)[M]):
            zigbee_str_t(std::make_index_sequence<M-1>(), n)
        {
        }

        constexpr zigbee_str_t():
            name{0}
        {
        }

        operator void*() { return name; }
        size_t capacity() const { return N - 1; }
        size_t size() const { return *name; }
        std::string_view sv() const { return {name + 1, size()}; }
        zigbee_str_view_t zsv() const { return {name}; }
        zigbee_str_ref_t& zsv_ref() { return *(zigbee_str_ref_t*)name; }

        template<size_t M>
        zigbee_str_t<N>& operator=(const char (&n)[M])
        {
            static_assert(M <= N, "String literal is too big");
            name[0] = M - 1;
            std::memcpy(name + 1, n, M - 1);
            return *this;
        }

        static constexpr type_t TypeId() { return type_t::CharStr; }
        static bool TypeValidator(uint8_t *value) { return *value < N; }

        std::optional<const uint8_t*> serialize_from(const uint8_t *pSrc, size_t limit)
        {
            if (*pSrc >= N) return std::nullopt;
            if (*pSrc >= limit) return std::nullopt;

            std::memcpy(name, pSrc, *pSrc);
            return pSrc + *pSrc + 1;
        }

        std::optional<uint8_t*> serialize_to(uint8_t *pDst, size_t limit) const
        {
            if (size() >= limit)
                return std::nullopt;

            *pDst = size();
            std::memcpy(pDst + 1, name + 1, *pDst);
            return pDst + *pDst + 1;
        }

        static constexpr size_t serialize_limit()
        {
            return N;
        }
    };

    template<size_t N>
    struct zigbee_bin_t
    {
        using __1byte_var_len = void;
        uint8_t data[N];

        template<class T, size_t M, size_t...idx>
        constexpr zigbee_bin_t(std::index_sequence<idx...>, std::array<T, M> const &n):
            data{ M-1, n[idx]... }
        {
        }

        template<class T, size_t M>
        constexpr zigbee_bin_t(std::array<T, M> const& n):
            zigbee_bin_t(std::make_index_sequence<M-1>(), n)
        {
        }

        constexpr zigbee_bin_t():
            data{0}
        {
        }

        operator void*() { return data; }
        size_t size() const { return N - 1; }
       std::span<const uint8_t> sv() const { return {data + 1, N - 1}; }

        template<size_t M>
        zigbee_bin_t<N>& operator=(std::array<uint8_t, M> const& n)
        {
            static_assert(M <= N, "String literal is too big");
            data[0] = M - 1;
            std::memcpy(data + 1, &n, M - 1);
            return *this;
        }

        static constexpr type_t TypeId() { return type_t::OctetStr; }
        static bool TypeValidator(uint8_t *value) { return *value < N; }

        std::optional<const uint8_t*> serialize_from(const uint8_t *pSrc, size_t limit)
        {
            if (*pSrc >= N) return std::nullopt;
            if (*pSrc >= limit) return std::nullopt;

            std::memcpy(data, pSrc, *pSrc);
            return pSrc + *pSrc + 1;
        }

        std::optional<uint8_t*> serialize_to(uint8_t *pDst, size_t limit) const
        {
            if (size() >= limit)
                return std::nullopt;

            *pDst = size();
            std::memcpy(pDst + 1, data + 1, *pDst);
            return pDst + *pDst + 1;
        }

        static constexpr size_t serialize_limit()
        {
            return N;
        }
    };

    template<class T, size_t N> requires (std::is_trivially_copyable_v<T>
            && std::is_trivially_constructible_v<T>)
    struct [[gnu::packed]] zigbee_bin_typed_array_t
    {
        using __1byte_var_len = void;
        uint8_t len_bytes;
        T data[N];

        template<size_t M, size_t...idx>
        constexpr zigbee_bin_typed_array_t(std::index_sequence<idx...>, std::array<T, M> const &n):
            len_bytes{sizeof(T) * M},
            data{ n[idx]... }
        {
            static_assert(sizeof(T) * M <= 255);
        }

        template<size_t M>
        constexpr zigbee_bin_typed_array_t(std::array<T, M> const& n):
            zigbee_bin_typed_array_t(std::make_index_sequence<M>(), n)
        {
        }

        constexpr zigbee_bin_typed_array_t():
            len_bytes{0}
            , data{}
        {
        }

        operator void*() { return this; }
        bool valid() const { return (len_bytes % sizeof(T)) == 0; }
        size_t size() const { return len_bytes / sizeof(T); }
        static constexpr size_t max_size() { return N; }
        static constexpr size_t size_bytes() { return N * sizeof(T); }
        std::span<const T> sv() const { return {data, N}; }

        template<class Me>
        auto& operator[](this Me const& t, size_t i) { return t.data[i]; }

        static constexpr type_t TypeId() { return type_t::OctetStr; }
        static bool TypeValidator(uint8_t *value) 
        {
            return (*value <= size_bytes()) && (*value % sizeof(T) == 0); 
        }

        std::optional<const uint8_t*> serialize_from(const uint8_t *pSrc, size_t limit)
        {
            if (*pSrc > size_bytes()) return std::nullopt;
            if (*pSrc >= limit) return std::nullopt;

            len_bytes = *pSrc;

            std::memcpy(data, pSrc + 1, *pSrc);
            return pSrc + *pSrc + 1;
        }

        std::optional<uint8_t*> serialize_to(uint8_t *pDst, size_t limit) const
        {
            if (size() >= limit)
                return std::nullopt;

            *pDst = len_bytes;
            std::memcpy(pDst + 1, data, *pDst);
            return pDst + *pDst + 1;
        }

        static constexpr size_t serialize_limit()
        {
            return size_bytes() + 1;
        }
    };

    template<class T> requires (std::is_trivially_copyable_v<T>
            && sizeof(T) <= 255)
    struct [[gnu::packed]] zigbee_bin_typed_t
    {
        using __1byte_var_len = void;
        uint8_t len_bytes;
        T data;

        constexpr zigbee_bin_typed_t(T d = {}):
            len_bytes{sizeof(T)}
            , data{d}
        {
        }

        operator void*() { return this; }

        T* operator->() { return &data; }
        operator T&() { return data; }
        operator const T&() const { return data; }

        static constexpr type_t TypeId() { return type_t::OctetStr; }
        static bool TypeValidator(uint8_t *value) 
        {
            return (*value == sizeof(T)) && ValidateCustomType((const T*)(value + 1));
        }

        std::optional<const uint8_t*> serialize_from(const uint8_t *pSrc, size_t limit)
        {
            if (*pSrc > sizeof(T)) return std::nullopt;
            if (*pSrc >= limit) return std::nullopt;
            len_bytes = *pSrc;

            std::memcpy(data, pSrc + 1, *pSrc);
            return pSrc + *pSrc + 1;
        }

        std::optional<uint8_t*> serialize_to(uint8_t *pDst, size_t limit) const
        {
            if (len_bytes >= limit)
                return std::nullopt;

            *pDst = len_bytes;
            std::memcpy(pDst + 1, data + 1, *pDst);
            return pDst + *pDst + 1;
        }

        static constexpr size_t serialize_limit()
        {
            return sizeof(T) + 1;
        }
    };


    template<size_t N>
    constexpr zigbee_str_t<N> ZbStr(const char (&n)[N])
    {
        static_assert(N < 255, "String too long");
        return [&]<size_t...idx>(std::index_sequence<idx...>){
            return zigbee_str_t<N>{.name={N-1, n[idx]...}};
        }(std::make_index_sequence<N-1>());
    }


}
#endif
