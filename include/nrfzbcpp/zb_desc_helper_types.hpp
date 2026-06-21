#ifndef ZB_DESC_HELPER_TYPES_HPP_
#define ZB_DESC_HELPER_TYPES_HPP_

#include "zb_types.hpp"

namespace zb
{
    template<class T>
    concept zigbee_cluster_c = requires { T::clusters; T::server_cluster_count(); T::client_cluster_count(); };

    template<class T>
    concept zigbee_ep_self_contained_c = requires { typename T::ClusterListType; requires  zigbee_cluster_c<typename T::ClusterListType>; };

    //this template class is to be specialized for every cluster struct
    template<class zb_struct>
    struct zcl_description_t
    {
        static constexpr inline auto get() { static_assert(sizeof(zb_struct) == 0, "Cluster description not found for type"); return std::false_type{}; }
    };


    template<class zb_struct>
    concept zigbee_cluster_struct_c = requires { requires !std::is_same_v<decltype(zb::zcl_description_t<zb_struct>::get()), std::false_type>; };

    template<class T>//attribute_list_t<...>
    concept zigbee_attribute_list_c = requires { typename T::Tag; requires zigbee_cluster_struct_c<typename T::Tag>; };

    template<class ClusterTag, uint8_t ep>
    void generic_cluster_init();

    using attr_validator_t = bool(*)(uint8_t *value);

#define DEFINE_ZBOSS_INIT_GETTER_FOR(ZCL_ID) static constexpr auto zboss_init_func(role_t r) { return r == role_t::Server ? ZCL_ID##_SERVER_ROLE_INIT : (r == role_t::Client ? ZCL_ID##_CLIENT_ROLE_INIT : NULL); }


    template<class MemPtr>
    struct mem_ptr_traits
    {
        static constexpr bool is_mem_ptr = false;
    };

    template<class T, class MemT>
    struct mem_ptr_traits<MemT T::*>
    {
        static constexpr bool is_mem_ptr = true;
        using ClassType = T;
        using MemberType = MemT;
    };

    /**********************************************************************/
    /* Template logic to check for duplicate cmd ids                      */
    /**********************************************************************/
    namespace tpl_tools
    {
        /**********************************************************************/
        /* Value-based unique ID checker                                      */
        /**********************************************************************/
        template<template<auto V> class IdGetter, auto I>
        concept valid_id_getter_c = requires { IdGetter<I>::id(); };

        template<template<auto V> class IdGetter, auto I> requires valid_id_getter_c<IdGetter, I>
        constexpr bool IsAllUniqueHelper(){ return true; }

        template<template<auto V> class IdGetter, auto Prime, auto Secondary> requires (valid_id_getter_c<IdGetter, Prime> && valid_id_getter_c<IdGetter, Secondary>)
        constexpr bool IsAllUniqueHelper(){ 
            static_assert(IdGetter<Prime>::id() != IdGetter<Secondary>::id());
            return IdGetter<Prime>::id() != IdGetter<Secondary>::id(); 
        }

        template<template<auto V> class IdGetter, auto Prime, auto Secondary, auto... Is> requires (sizeof...(Is) > 0)
        constexpr bool IsAllUniqueHelper(){ return IsAllUniqueHelper<IdGetter, Prime,Secondary>() && IsAllUniqueHelper<IdGetter, Prime, Is...>(); }

        template<template<auto V> class IdGetter, auto... Is>requires (sizeof...(Is) <= 1)
        constexpr bool IsAllUniquePrimaryHelper(){ return true; }

        template<template<auto V> class IdGetter, auto Prime, auto Secondary>
        constexpr bool IsAllUniquePrimaryHelper(){ return IsAllUniqueHelper<IdGetter, Prime, Secondary>(); }

        template<template<auto V> class IdGetter, auto Prime, auto... Is> requires (sizeof...(Is) > 1)
        constexpr bool IsAllUniquePrimaryHelper()
        { 
            return IsAllUniqueHelper<IdGetter, Prime, Is...>() && IsAllUniquePrimaryHelper<IdGetter, Is...>(); 
        }

        template<template<auto V> class IdGetter, auto... Is>
        constexpr bool kAllUniqueIds = IsAllUniquePrimaryHelper<IdGetter, Is...>();

        /**********************************************************************/
        /* Class-based unique ID checker                                      */
        /**********************************************************************/
        template<template<class> class IdGetterT, class I>
        concept valid_id_type_getter_c = requires { IdGetterT<I>::id(); };

        template<template<class> class IdGetterT, class I> requires valid_id_type_getter_c<IdGetterT, I>
        constexpr bool IsAllUniqueHelperT(){ return true; }

        template<template<class> class IdGetterT, class Prime, class Secondary> requires (valid_id_type_getter_c<IdGetterT, Prime> && valid_id_type_getter_c<IdGetterT, Secondary>)
        constexpr bool IsAllUniqueHelperT(){ 
            static_assert(IdGetterT<Prime>::id() != IdGetterT<Secondary>::id());
            return IdGetterT<Prime>::id() != IdGetterT<Secondary>::id(); 
        }

        template<template<class> class IdGetterT, class Prime, class Secondary, class... Is> requires (sizeof...(Is) > 0)
        constexpr bool IsAllUniqueHelperT(){ return IsAllUniqueHelperT<IdGetterT, Prime,Secondary>() && IsAllUniqueHelperT<IdGetterT, Prime, Is...>(); }

        template<template<class> class IdGetterT, class... Is>requires (sizeof...(Is) <= 1)
        constexpr bool IsAllUniquePrimaryHelperT(){ return true; }

        template<template<class> class IdGetterT, class Prime, class Secondary>
        constexpr bool IsAllUniquePrimaryHelperT(){ return IsAllUniqueHelperT<IdGetterT, Prime, Secondary>(); }

        template<template<class> class IdGetterT, class Prime, class... Is> requires (sizeof...(Is) > 1)
        constexpr bool IsAllUniquePrimaryHelperT()
        { 
            return IsAllUniqueHelperT<IdGetterT, Prime, Is...>() && IsAllUniquePrimaryHelperT<IdGetterT, Is...>(); 
        }

        template<template<class> class IdGetterT, class... Is>
        constexpr bool kAllUniqueIdsT = IsAllUniquePrimaryHelperT<IdGetterT, Is...>();
    };
}
#endif
