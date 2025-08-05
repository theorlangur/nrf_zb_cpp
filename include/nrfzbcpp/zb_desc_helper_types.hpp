#ifndef ZB_DESC_HELPER_TYPES_HPP_
#define ZB_DESC_HELPER_TYPES_HPP_

#include "zb_types.hpp"

namespace zb
{
    template<class T>
    concept ZigbeeCluster = requires { T::clusters; T::server_cluster_count(); T::client_cluster_count(); };

    template<class T>
    concept ZigbeeEPSelfContained = requires { typename T::ClusterListType; requires  ZigbeeCluster<typename T::ClusterListType>; };

    //this template class is to be specialized for every cluster struct
    template<class zb_struct>
    struct zcl_description_t
    {
        static constexpr inline auto get() { static_assert(sizeof(zb_struct) == 0, "Cluster description not found for type"); return std::false_type{}; }
    };


    template<class zb_struct>
    concept ZigbeeClusterStruct = requires { requires !std::is_same_v<decltype(zb::zcl_description_t<zb_struct>::get()), std::false_type>; };

    template<class T>//TAttributeList<...>
    concept ZigbeeAttributeList = requires { typename T::Tag; requires ZigbeeClusterStruct<typename T::Tag>; };

    template<class ClusterTag, uint8_t ep>
    void generic_cluster_init();
    //base template + a macro to get proper init functions for server/client roles depending on the cluster
    template<zb_uint16_t id> constexpr zb_zcl_cluster_init_t get_cluster_init(Role r) { static_assert(id >= 0xfc00, "get_cluster_init not specialized for this cluster!"); return nullptr; }
#define DEFINE_GET_CLUSTER_INIT_FOR(cid) template<> constexpr zb_zcl_cluster_init_t get_cluster_init<cid>(Role r) { return r == Role::Server ? cid##_SERVER_ROLE_INIT : (r == Role::Client ? cid##_CLIENT_ROLE_INIT : NULL); }
#define DEFINE_NULL_CLUSTER_INIT_FOR(cid) template<> constexpr zb_zcl_cluster_init_t get_cluster_init<cid>(Role r) { return NULL; }


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
}
#endif
