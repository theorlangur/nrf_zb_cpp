#ifndef ZB_DESC_HELPER_TYPES_SELF_CONTAINED_HPP_
#define ZB_DESC_HELPER_TYPES_SELF_CONTAINED_HPP_

#include "zb_desc_helper_types_device.hpp"

namespace zb
{
    template<class T>
    using to_attribute_list_type_t = decltype(zb::cluster_struct_to_attr_list(std::declval<T&>(), zb::zcl_description_t<T>::get()));

    template<class T> struct mem_tag_t{};

    template<class T>
    struct attribute_list_container_mem_t
    {
        using AttrListType = to_attribute_list_type_t<T>;
        AttrListType m;

        constexpr AttrListType& get(mem_tag_t<T>) { return m; }
    };

    template<class... Bases>
    struct attribute_list_container_t: attribute_list_container_mem_t<Bases>... { using attribute_list_container_mem_t<Bases>::get...; };

    template<class T>
    struct ep_args_list_container_mem_t
    {
        T &m;
        constexpr T& get(mem_tag_t<T>) { return m; }
    };

    template<ep_base_info_t i, class... Bases>
    struct ep_args_t: ep_args_list_container_mem_t<Bases>...
    {
        using ep_args_list_container_mem_t<Bases>::get...;
    };

    /**********************************************************************/
    /* Template logic to check for duplicate ep ids                       */
    /**********************************************************************/
    namespace ep_tools
    {
        template<class X>
        struct EPIdGetter { static constexpr auto id() { return X::ep_id(); } };

        template<class... EPs>
        constexpr bool kAllUniqueIds = tpl_tools::kAllUniqueIdsT<EPIdGetter, EPs...>;
    };


    template<ep_base_info_t i, class... Bases>
    constexpr auto make_ep_args(Bases&...b) { 
        static_assert(cluster_tools::kAllUniqueIds<Bases...>, "No duplicated cluster Id is allowed!");
        return ep_args_t<i, Bases...>{b...}; 
    }

    template<ep_base_info_t i, zigbee_cluster_struct_c... ClusterTypes>
    struct ep_desc_self_contained_t
    {
        using ClusterListType = zb::TClusterList<i.ep, to_attribute_list_type_t<ClusterTypes>...>;

        static constexpr zb_uint8_t ep_id() { return i.ep; }

        constexpr ep_desc_self_contained_t(ClusterTypes&...s):
            attributes{ zb::cluster_struct_to_attr_list(s, zb::zcl_description_t<ClusterTypes>::get())... },
            clusters{attributes.get(mem_tag_t<ClusterTypes>{})...},
            ep{clusters}
        {
        }

        constexpr ep_desc_self_contained_t(ep_args_t<i, ClusterTypes...> arg):
            attributes{ zb::cluster_struct_to_attr_list(arg.get(mem_tag_t<ClusterTypes>{}), zb::zcl_description_t<ClusterTypes>::get())... },
            clusters{attributes.get(mem_tag_t<ClusterTypes>{})...},
            ep{clusters}
        {
        }

        template<class StructTag>
        constexpr auto& attribute_list() { return attributes.get(mem_tag_t<StructTag>{}); }

        attribute_list_container_t<ClusterTypes...> attributes;
        ClusterListType clusters;
        zb::ep_desc_t<i, ClusterListType> ep;
    };
    //template<class... Clusters>
    //ep_desc_self_contained_t(ep_args_t<Clusters...>) -> ep_desc_self_contained_t<Clusters...>;

    template<class T>
    concept is_ep_desc_self_contained_c = requires(T t) {
        typename T::ClusterListType;
        t.ep;
        t.clusters;
        t.attributes;
    };

    template<zb_uint8_t ep>
    struct ep_tag_t{};

    template<class EP>
    struct ep_container_mem_t
    {
        EP m;

        constexpr EP& get(mem_tag_t<EP>) { return m; }
        constexpr EP& get(ep_tag_t<EP::ep_id()>) { return m; }
    };

    template<class... EPs>
    struct ep_list_container_t: ep_container_mem_t<EPs>...
    {
        using ep_container_mem_t<EPs>::get...;

        template<zb_uint8_t dummy>
        constexpr auto get(ep_tag_t<dummy>) { static_assert(sizeof(ep_tag_t<dummy>) == 0, "EP not found"); }
    };

    template<class T>
    struct ep_desc_type_from_arg_t;

    template<ep_base_info_t i, class... Clusters>
    struct ep_desc_type_from_arg_t<ep_args_t<i, Clusters...>>
    {
        using type = ep_desc_self_contained_t<i, Clusters...>;
    };

    template<class T>
    using ep_desc_type_from_arg_t_type = ep_desc_type_from_arg_t<T>::type;

    template<class... EPSelfContainedTypes>//see ep_desc_self_contained_t<...>, ep_desc_t<...>
    struct device_full_t
    {
        static_assert(ep_tools::kAllUniqueIds<EPSelfContainedTypes...>, "All EP ids must be unique!");
        static constexpr size_t N = sizeof...(EPSelfContainedTypes);

        template<class... EPArgs>
        constexpr device_full_t(EPArgs..._eps):
            eps{_eps...},
            endpoints{&eps.get(mem_tag_t<EPSelfContainedTypes>{}).ep.ep...},
            ctx{.ep_count = N, .ep_desc_list = endpoints}
        {
        }

        template<zb_uint8_t _ep>
        constexpr auto& ep() { return eps.get(ep_tag_t<_ep>{}).ep; }

        template<zb_uint8_t _ep>
        constexpr auto& ep_obj() { return eps.get(ep_tag_t<_ep>{}); }

        operator zb_af_device_ctx_t*() { return &ctx; }

        ep_list_container_t<EPSelfContainedTypes...> eps;
        zb_af_endpoint_desc_t *endpoints[N];
        zb_af_device_ctx_t ctx;
    };

    template<class... EPArgs>
    device_full_t(EPArgs...) -> device_full_t<ep_desc_type_from_arg_t_type<EPArgs>...>;
}
#endif
