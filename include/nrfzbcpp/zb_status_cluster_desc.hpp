#ifndef ZB_STATUS_CLUSTER_DESC_HPP_
#define ZB_STATUS_CLUSTER_DESC_HPP_

#include "zb_main.hpp"

extern "C"
{
#include <zboss_api_addons.h>
#include <zb_nrf_platform.h>
}

namespace zb
{
    static constexpr uint16_t kZB_ZCL_CLUSTER_ID_STATUS = 0xfc80;
    struct zb_zcl_status_t
    {
        static constexpr uint8_t kCMD1 = 1;
        static constexpr uint8_t kCMD2 = 2;
        static constexpr uint8_t kCMD3 = 3;

        int16_t status1 = 0;
        int16_t status2 = 0;
        int16_t status3 = 0;

        /**********************************************************************/
        /* Commands                                                           */
        /**********************************************************************/
        cmd_in_t<kCMD1> cmd1;
        cmd_in_t<kCMD2> cmd2;
        cmd_in_t<kCMD3> cmd3;
    };

    template<> struct zcl_description_t<zb_zcl_status_t> {
        static constexpr auto get()
        {
            using T = zb_zcl_status_t;
            return cluster_t<
                cluster_info_t{.id = kZB_ZCL_CLUSTER_ID_STATUS},
                attributes_t<
                    attribute_t{.m = &T::status1,.id = 0x0000, .a=access_t::RP},
                    attribute_t{.m = &T::status2,.id = 0x0001, .a=access_t::RP},
                    attribute_t{.m = &T::status3,.id = 0x0002, .a=access_t::RP}
                >{},
                commands_t<&T::cmd1, &T::cmd2, &T::cmd3>{}
            >{};
        }
    };
}
#endif
