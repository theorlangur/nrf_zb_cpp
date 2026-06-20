#ifndef ZB_MAIN_HPP_
#define ZB_MAIN_HPP_

#include "zb_desc_helper_types_device.hpp"
#include "zb_desc_helper_types_self_contained.hpp"
#include "zb_signals.hpp"
#include "zb_handlers.hpp"

#include <zigbee/zigbee_error_handler.h>

namespace zb
{
    //returns attribute_list_t with properly initialized array of zb_zcl_attr_t 
    //based on the information obtained via get_cluster_description for the passed structure type of ZbS
    //returns attribute_list_t<...>
    template<class ZbS> requires zigbee_cluster_struct_c<ZbS>
    constexpr auto to_attributes(ZbS &s) { return cluster_struct_to_attr_list(s, zcl_description_t<ZbS>::get()); }

    //given multiples of attribute_list_t<> objects this constructs and returns a TClusterList<..>
    //with a properly initialized array of zb_zcl_cluster_desc_t
    template<uint8_t ep, class... ClusterAttributesDesc> requires (zigbee_attribute_list_c<ClusterAttributesDesc> && ...)
    constexpr auto to_clusters(ClusterAttributesDesc&... c) -> TClusterList<ep, ClusterAttributesDesc...> { return {c...}; }

    //given multiples clusters in form of TClusterList<> this constructs and returns a ep_desc_t<..>
    //with a proper zb_af_simple_desc_1_1_t-derived simple description and a properyl initialized zb_af_endpoint_desc_t
    //also with the automatically calculated and reserved space for reporting attributes and level control alarms
    template<ep_base_info_t i, class... T>
    constexpr auto configure_ep(TClusterList<i.ep, T...> &clusters) -> ep_desc_t<i, TClusterList<i.ep, T...>>
    {
        return {clusters};
    }

    //packs existing Endpoints together into a Device, with properly initialized zb_af_device_ctx_t
    template<ep_base_info_t... i, class... Clusters>
    constexpr auto configure_device(ep_desc_t<i, Clusters>&...eps) { return Device{eps...}; }

    template<class... SelfContainedEP> requires (is_ep_desc_self_contained_c<SelfContainedEP> && ...)
    constexpr auto configure_device(SelfContainedEP&...eps) { return Device{eps.ep...}; }

    template<zb::ep_base_info_t i, class... Structs> requires (zigbee_cluster_struct_c<Structs> && ...)
    constexpr auto make_endpoint(Structs&... s) { return ep_desc_self_contained_t<i, Structs...>{s...}; }

    //Args==ep_args_t<...> -> ep_desc_type_from_arg_t_type<ep_args_t<...>> -> ep_desc_self_contained_t<...>
    template<class... Args>
    constexpr auto make_device(Args... a) { return device_full_t{a...}; }
}

#endif
