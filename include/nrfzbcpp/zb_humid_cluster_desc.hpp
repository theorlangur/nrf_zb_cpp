#ifndef ZB_HUMID_CLUSTER_DESC_HPP_
#define ZB_HUMID_CLUSTER_DESC_HPP_

#include "zb_main.hpp"

extern "C"
{
#include <zboss_api_addons.h>
#include <zb_mem_config_med.h>
#include <zb_nrf_platform.h>
}

namespace zb
{
    static constexpr uint16_t kZB_ZCL_CLUSTER_ID_REL_HUMIDITY = 0x0405;
    struct zb_zcl_rel_humid_basic_t
    {
        uint16_t measured_value;

        static uint16_t FromRelH(float v) { return uint16_t(v * 100); }
        static float ToRelH(uint16_t v) { return float(v) / 100.f; }
    };

    template<>
    struct zcl_description_t<zb_zcl_rel_humid_basic_t> {
        static constexpr auto get()
        {
            using T = zb_zcl_rel_humid_basic_t;
            return cluster_t<
                {.id = kZB_ZCL_CLUSTER_ID_REL_HUMIDITY},
                attributes_t<
                    attribute_t{.m = &T::measured_value,.id = 0x0000, .a=Access::RP}
                >{}
            >{};
        }
    };

    struct zb_zcl_rel_humid_t: zb_zcl_rel_humid_basic_t
    {
        uint16_t min_measured_value;
        uint16_t max_measured_value;
    };

    template<>
    struct zcl_description_t<zb_zcl_rel_humid_t> {
        static constexpr auto get()
        {
            using T = zb_zcl_rel_humid_t;
            return zcl_description_t<zb_zcl_rel_humid_basic_t>::get() 
                + cluster_t<
                {.id = kZB_ZCL_CLUSTER_ID_REL_HUMIDITY},
                attributes_t<
                    attribute_t{.m = &T::min_measured_value,.id = 0x0001},
                    attribute_t{.m = &T::max_measured_value,.id = 0x0002}
                >{}
            >{};
        }
    };

    struct zb_zcl_rel_humid_ext_t: zb_zcl_rel_humid_t
    {
        uint16_t tolerance;
    };

    template<>
    struct zcl_description_t<zb_zcl_rel_humid_ext_t> {
    static constexpr auto get_cluster_description()
    {
        using T = zb_zcl_rel_humid_ext_t;
        return zcl_description_t<zb_zcl_rel_humid_t>::get() 
            + cluster_t<
            {.id = kZB_ZCL_CLUSTER_ID_REL_HUMIDITY},
            attributes_t<
                attribute_t{.m = &T::tolerance,.id = 0x0003}
            >{}
        >{};
    }
    };
}
#endif
