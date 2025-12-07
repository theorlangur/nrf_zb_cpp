#ifndef ZB_POWER_CONFIG_TOOLS_HPP_
#define ZB_POWER_CONFIG_TOOLS_HPP_

#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/regulator.h>
#include "zb_power_config_cluster_desc.hpp"

namespace zb
{
    struct RegRAII
    {
        RegRAII(const struct device *pD):pDev(pD){ if (pDev) regulator_enable(pDev); }
        ~RegRAII() { if (pDev) regulator_disable(pDev); }
    private:
        const struct device *pDev;
    };

    struct battery_cfg
    {
        int battery_adc_channel = 0;
        int32_t maxBatteryVoltage = 1600;//mV
        int32_t minBatteryVoltage = 900;//mV
    };

    template<battery_cfg cfg, class EP>
    struct BatteryMeasurementsBlock
    {
        static const constexpr auto kAttrBattVoltage = &zb::zb_zcl_power_cfg_battery_info_t::batt_voltage;
        static const constexpr auto kAttrBattPercentage = &zb::zb_zcl_power_cfg_battery_info_t::batt_percentage_remaining;
        static const constexpr int32_t g_BatteryVoltageRange = cfg.maxBatteryVoltage - cfg.minBatteryVoltage;//mV
        static BatteryMeasurementsBlock *g_Battery;

        EP &zb_ep;
        const struct adc_dt_spec *adc_channels;
        const struct device *regulator;

        void update()
        {
            RegRAII batteryRegulator(regulator);
            uint16_t buf;
            struct adc_sequence sequence = {
                .buffer = &buf,
                /* buffer size in bytes, not number of samples */
                .buffer_size = sizeof(buf),
            };
            (void)adc_sequence_init_dt(&adc_channels[cfg.battery_adc_channel], &sequence);

            int err = adc_read_dt(&adc_channels[cfg.battery_adc_channel], &sequence);
            if (err == 0)
            {
                int32_t batteryVoltage = (int32_t)buf;
                adc_raw_to_millivolts_dt(&adc_channels[cfg.battery_adc_channel],
                        &batteryVoltage);

                printk("update_battery_state_zb: volt %d\r\n", batteryVoltage);
                zb_ep.template attr<kAttrBattVoltage>() = uint8_t(batteryVoltage / 100);
                zb_ep.template attr<kAttrBattPercentage>() = uint8_t((batteryVoltage - cfg.minBatteryVoltage) * 200 / g_BatteryVoltageRange);
            }
        }

        static void update_zb_callback(uint8_t dummy)
        {
            if (g_Battery)
                g_Battery->update();
        }

        int setup()
        {
            if (g_Battery)//double init not allowed
                return -1;
            g_Battery = this;

            if (!adc_is_ready_dt(&adc_channels[cfg.battery_adc_channel])) {
                printk("ADC controller device %s not ready\n", adc_channels[cfg.battery_adc_channel].dev->name);
                return -1;
            }

            int err = adc_channel_setup_dt(&adc_channels[cfg.battery_adc_channel]);
            if (err < 0) {
                printk("Could not setup channel #%d (%d)\n", 0, err);
                return err;
            }
            return 0;
        }
    };

    template<battery_cfg cfg, class EP>
    constinit BatteryMeasurementsBlock<cfg, EP> *BatteryMeasurementsBlock<cfg, EP>::g_Battery = nullptr;

    template<battery_cfg cfg, class EP>
    constexpr auto make_battery_measurements_block(EP &e, const struct adc_dt_spec *adc_channels, const struct device *reg = nullptr)
    {
        return BatteryMeasurementsBlock<cfg, EP>{e, adc_channels, reg};
    }
}
#endif
