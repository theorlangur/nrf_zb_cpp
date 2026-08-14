#ifndef ZB_TOOLS_HPP_
#define ZB_TOOLS_HPP_

//workaround
typedef enum zb_phy_status_e
{
  PHY_BUSY                  = 0x00,
  PHY_BUSY_RX               = 0x01,
  PHY_BUSY_TX               = 0x02,
  PHY_FORCE_TRX_OFF         = 0x03,
  PHY_IDLE                  = 0x04,
  PHY_INVALID_PARAMETER     = 0x05,
  PHY_RX_ON                 = 0x06,
  PHY_SUCCESS               = 0x07,
  PHY_TRX_OFF               = 0x08,
  PHY_TX_ON                 = 0x09,
  PHY_UNSUPPORTED_ATTRIBUTE = 0x0a,
  PHY_READ_ONLY             = 0x0b
} zb_phy_status_t;

extern "C" {
#include <zboss_api.h>
#include <osif/mac_platform.h>
}

namespace zb
{
    struct tx_power{
        using tx_power_cb = void(*)(zb_ret_t res);

        static zb_ret_t set_tx_power(zb_int8_t power, uint32_t channel_mask, tx_power_cb on_complete = {})
        {
            g_cb = on_complete;
            g_channels_to_set = channel_mask;
            g_tx_power = power;
            g_error = RET_OK;
            g_last_channel = ZB_TRANSCEIVER_START_CHANNEL_NUMBER;
            for (; g_last_channel <= ZB_TRANSCEIVER_MAX_CHANNEL_NUMBER; g_last_channel++) {
                if (g_channels_to_set & (1 << g_last_channel)) {
                    return zb_buf_get_out_delayed_ext(set_tx_power_for_channel, g_last_channel, 0);
                }
            }
            return RET_ERROR;
        }

    private:
        static void set_tx_power_for_channel(uint8_t buf, uint16_t channel)
        {
            zb_tx_power_params_t *power_params;

            power_params = (zb_tx_power_params_t *)zb_buf_initial_alloc(buf, sizeof(zb_tx_power_params_t));

            power_params->page = ZB_CHANNEL_PAGE0_2_4_GHZ;
            power_params->channel = channel;
            power_params->tx_power = g_tx_power;
            power_params->cb = on_tx_power_set;

            zigbee_schedule_callback(zb_set_tx_power_async, buf);
        }

        static void on_tx_power_set(uint8_t param)
        {
            //char *status = NULL;
            zb_tx_power_params_t *power_params = (zb_tx_power_params_t *)zb_buf_begin(param);

            if (g_error < power_params->status)
                g_error = power_params->status;
            ++g_last_channel;
            for (; g_last_channel <= ZB_TRANSCEIVER_MAX_CHANNEL_NUMBER; g_last_channel++) {
                if (g_channels_to_set & (1 << g_last_channel)) {
                    set_tx_power_for_channel(param, g_last_channel);
                    return;
                }
            }

            zb_buf_free(param);
            if (g_cb)
                g_cb(g_error);
        }

        inline static tx_power_cb g_cb;
        inline static uint32_t g_channels_to_set;
        inline static uint8_t g_last_channel;
        inline static int8_t g_tx_power;
        inline static zb_ret_t g_error;
    };
}
#endif
