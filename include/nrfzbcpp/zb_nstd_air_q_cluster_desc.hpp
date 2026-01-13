#ifndef ZB_NSTD_AIR_Q_CLUSTER_DESC_HPP_
#define ZB_NSTD_AIR_Q_CLUSTER_DESC_HPP_

#include "zb_main.hpp"

extern "C"
{
#include <zboss_api_addons.h>
#include <zb_mem_config_med.h>
#include <zb_nrf_platform.h>
}

namespace zb
{
    static constexpr uint16_t KZB_ZCL_CLUSTER_ID_AIR_Q = 0xfc08;
    struct zb_zcl_air_q_t
    {
        enum class AQI: uint8_t
        {
            Excellent = 1,
            Good,
            Moderate,
            Poor,
            Unhealthy
        };
        float tvoc = 0;
        AQI aqi = AQI::Excellent;
    };

    template<>
    struct zcl_description_t<zb_zcl_air_q_t> {
        static constexpr auto get()
        {
            using T = zb_zcl_air_q_t;
            return cluster_t<
            {.id = KZB_ZCL_CLUSTER_ID_AIR_Q},
                attributes_t<
                    attribute_t{.m = &T::tvoc,.id = 0x0000, .a=Access::RP},
                    attribute_t{.m = &T::aqi, .id = 0x0001, .a=Access::RP}
            >{}
            >{};
        }
    };
}
#endif
