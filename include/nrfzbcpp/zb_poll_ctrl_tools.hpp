#ifndef ZB_POLL_CTRL_TOOLS_HPP_
#define ZB_POLL_CTRL_TOOLS_HPP_

#include "zb_poll_ctrl_cluster_desc.hpp"

namespace zb
{
    struct poll_ctrl_cfg
    {
        uint8_t ep;
        zb_callback_t callback_on_check_in;
        bool sleepy_end_device;
        zb_time_t long_poll_at_start = 2 * 1000;//2s
        zb_time_t start_awake_time = 30 * 1000;//30s to configure/communicate
    };

    static inline zb::ZbAlarmExt16 g_EnterLowPowerLongPollMode;
    template<poll_ctrl_cfg cfg>
    void configure_poll_control(zb::zb_zcl_poll_ctrl_basic_t &poll_ctrl_cluster)
    {
        zb_zcl_poll_control_start(0, cfg.ep);
        zb_zcl_poll_controll_register_cb(cfg.callback_on_check_in);

        if constexpr (cfg.sleepy_end_device)
        {
            if constexpr (cfg.start_awake_time)
            {
                //we start with 2-sec long poll for the first 30 seconds
                zb_zdo_pim_set_long_poll_interval(cfg.long_poll_at_start);
                g_EnterLowPowerLongPollMode.Setup([&poll_ctrl_cluster]{
                        if (poll_ctrl_cluster.long_poll_interval != 0xffffffff)
                        {
                        //printk("on_zigbee_start: long poll set to power save %d ms\r\n", (poll_ctrl_cluster.long_poll_interval * 1000 / 4));
                        zb_zdo_pim_set_long_poll_interval(poll_ctrl_cluster.long_poll_interval * 1000 / 4);
                        }
                        }, cfg.start_awake_time);
            }else
            {
                if (poll_ctrl_cluster.long_poll_interval != 0xffffffff)
                {
                    //printk("on_zigbee_start: long poll set to power save %d ms\r\n", (poll_ctrl_cluster.long_poll_interval * 1000 / 4));
                    zb_zdo_pim_set_long_poll_interval(poll_ctrl_cluster.long_poll_interval * 1000 / 4);
                }
            }
        }
        else
        {
            printk("on_zigbee_start: long poll set to non-power save\r\n");
            zb_zdo_pim_set_long_poll_interval(cfg.long_poll_at_start);
        }
    }
}
#endif
