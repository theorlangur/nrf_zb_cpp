#ifndef ZB_CO2_CLUSTER_DESC_HPP_
#define ZB_CO2_CLUSTER_DESC_HPP_

#include "zb_main.hpp"

extern "C"
{
#include <zboss_api_addons.h>
#include <zb_nrf_platform.h>
}

namespace zb
{
    static constexpr uint16_t kZB_ZCL_CLUSTER_ID_CO2 = 0x040D;
    struct zb_zcl_co2_basic_t
    {
        float measured_value;
    };

    template<>
    struct zcl_description_t<zb_zcl_co2_basic_t> {
        static constexpr auto get()
        {
            using T = zb_zcl_co2_basic_t;
            return cluster_t<
            {.id = kZB_ZCL_CLUSTER_ID_CO2},
                attributes_t<
                    attribute_t{.m = &T::measured_value,.id = 0x0000, .a=Access::RP}
            >{}
            >{};
        }
    };

    struct zb_zcl_co2_t: zb_zcl_co2_basic_t
    {
        float min_measured_value;
        float max_measured_value;
    };

    template<>
    struct zcl_description_t<zb_zcl_co2_t> {
        static constexpr auto get()
        {
            using T = zb_zcl_co2_t;
            return zcl_description_t<zb_zcl_co2_basic_t>::get() 
                + cluster_t<
                {.id = kZB_ZCL_CLUSTER_ID_CO2},
                attributes_t<
                    attribute_t{.m = &T::min_measured_value,.id = 0x0001},
                    attribute_t{.m = &T::max_measured_value,.id = 0x0002}
                >{}
            >{};
        }
    };

    struct zb_zcl_co2_ext_t: zb_zcl_co2_t
    {
        float tolerance;
    };

    template<>
    struct zcl_description_t<zb_zcl_co2_ext_t> {
        static constexpr auto get()
        {
            using T = zb_zcl_co2_ext_t;
            return zcl_description_t<zb_zcl_co2_t>::get() 
                + cluster_t<
                {.id = kZB_ZCL_CLUSTER_ID_CO2},
                attributes_t<
                    attribute_t{.m = &T::tolerance,.id = 0x0003}
                >{}
            >{};
        }
    };
}
#endif
