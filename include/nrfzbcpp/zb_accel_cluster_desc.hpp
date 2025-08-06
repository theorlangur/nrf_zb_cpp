#ifndef ZB_ACCEL_CLUSTER_DESC_HPP_
#define ZB_ACCEL_CLUSTER_DESC_HPP_

#include "zb_main.hpp"

extern "C"
{
#include <zboss_api_addons.h>
#include <zb_mem_config_med.h>
#include <zb_nrf_platform.h>
}

namespace zb
{
    static constexpr uint16_t kZB_ZCL_CLUSTER_ID_ACCEL = 0xfc00;
    static constexpr uint8_t kZB_ZCL_ACCEL_CMD_ON_EVENT = 100;
    static constexpr uint8_t kZB_ZCL_ACCEL_CMD_ON_IN_EVENT = 101;
    struct EventArgs
    {
        uint32_t tap : 1;
        uint32_t dbl_tap : 1;
        uint32_t sixD_x_hi : 1;
        uint32_t sixD_x_lo : 1;
        uint32_t sixD_y_hi : 1;
        uint32_t sixD_y_lo : 1;
        uint32_t sixD_z_hi : 1;
        uint32_t sixD_z_lo : 1;
        uint32_t wake : 1;
        uint32_t sleep : 1;
        uint32_t flip_x : 1;
        uint32_t flip_y : 1;
        uint32_t flip_z : 1;
    };

    struct InEvent
    {
        uint8_t param1;
        uint16_t param2;
    }ZB_PACKED_STRUCT;

    struct accel_config_t
    {
        uint8_t on_event_pool_size = 0;
    };

    template<accel_config_t cfg = {}>
    struct zb_zcl_accel_basic_t
    {
        float x;//1.f == 1G
        float y;//1.f == 1G
        float z;//1.f == 1G
        cluster_in_cmd_desc_t<kZB_ZCL_ACCEL_CMD_ON_IN_EVENT, InEvent> on_in_event;
        [[no_unique_address]]cluster_std_cmd_desc_with_pool_size_t<kZB_ZCL_ACCEL_CMD_ON_EVENT, cfg.on_event_pool_size, EventArgs> on_event;
    };

    template<accel_config_t cfg> struct zcl_description_t<zb_zcl_accel_basic_t<cfg>> {
        static constexpr auto get()
        {
            using T = zb_zcl_accel_basic_t<cfg>;
            return cluster_struct_desc_t<
                cluster_info_t{.id = kZB_ZCL_CLUSTER_ID_ACCEL},
                cluster_attributes_desc_t<
                    cluster_mem_desc_t{.m = &T::x,.id = 0x0000, .a=Access::RP},
                    cluster_mem_desc_t{.m = &T::y,.id = 0x0001, .a=Access::RP},
                    cluster_mem_desc_t{.m = &T::z,.id = 0x0002, .a=Access::RP}
                >{}
                ,cluster_commands_desc_t<
                     &T::on_event
                     ,&T::on_in_event
                >{}
            >{};
        }
    };

DEFINE_NULL_CLUSTER_INIT_FOR(kZB_ZCL_CLUSTER_ID_ACCEL);
}
#endif
