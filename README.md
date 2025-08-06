# NRF ZBoss C++ wrappers

## Goal
Replace a macro madness with a C++ template madness )) (hopefully C++26's static reflection will simplify it though).

## Examples
How to declare a zigbee device with an endpoint and several clusters:
```cpp
#include <nrfzbcpp/zb_main.hpp>
#include <nrfzbcpp/zb_power_config_cluster_desc.hpp>
#include <nrfzbcpp/zb_poll_ctrl_cluster_desc.hpp>
#include <nrfzbcpp/zb_temp_cluster_desc.hpp>//not in a set of standard clusters

/* Manufacturer name (32 bytes). */
#define INIT_BASIC_MANUF_NAME      "CutsomManufaturer"
/* Model number assigned by manufacturer (32-bytes long string). */
#define INIT_BASIC_MODEL_ID        "SomeDeviceModel"

constexpr uint8_t kDEV_EP = 1;
constexpr uint16_t kDEV_ID = 0x1234;

/* Main application customizable context type.
 * Stores all settings and static values.
 */
struct device_ctx_t{
    zb::zb_zcl_basic_names_t basic_attr;
    zb::zb_zcl_power_cfg_battery_info_t battery_attr;
    zb::zb_zcl_poll_ctrl_basic_t poll_ctrl;
    zb::zb_zcl_temp_basic_t temperature;
};
constexpr auto kTemp = &zb::zb_zcl_temp_basic_t::measured_value;//for easier access later

using namespace zb::literals;
/* Zigbee device application context storage. */
//Here lives actual data of the clusters
static constinit device_ctx_t dev_ctx{
    .basic_attr = {
	{
	    .zcl_version = ZB_ZCL_VERSION,
	    .power_source = zb::zb_zcl_basic_min_t::PowerSource::Battery
	},
	/*.manufacturer =*/ INIT_BASIC_MANUF_NAME,
	/*.model =*/ INIT_BASIC_MODEL_ID,
    },
    .poll_ctrl = {
	.check_in_interval = 60_min_to_qs,
	.long_poll_interval = 7_sec_to_qs,
	//.short_poll_interval = 1_sec_to_qs,
    },
    .temperature = {
	.measured_value = 0,
    }
};

//Here we're making the actual Zigbee device with properly initialized structures
//All necessary cluster/attribute information is auto-deduced from the types of cluster member variables of dev_ctx
constinit static auto zb_ctx = zb::make_device(
	zb::make_ep_args<{.ep=kEP, .dev_id=kDEV_ID, .dev_ver=1}>(
	    dev_ctx.basic_attr
	    , dev_ctx.battery_attr
	    , dev_ctx.poll_ctrl
	    , dev_ctx.temperature
	    )
	);

//for convenient access to some typical operations later
constinit static auto &zb_ep = zb_ctx.ep<kEP>();

//some temperature sensor somewhere
static const struct device *const temp_dev = DEVICE_DT_GET(DT_NODELABEL(temp_sensor));

//assuming this is called somewhere in a ZBoss thread
void on_temperature_measured()
{
    sensor_sample_fetch(temp_dev);
    sensor_value t;
    sensor_channel_get(temp_dev, SENSOR_CHAN_AMBIENT_TEMP, &t);
    //that's it, this assignment internally will do zb_zcl_set_attr_val
    //with all proper arguments
    zb_ep.attr<kTemp>() = int16_t(t.val1*100 + t.val2 / 10000.f);
}

void on_zigbee_start()
{
    //...
}

//...
void zboss_signal_handler(zb_bufid_t bufid)
{
    zb_zdo_app_signal_hdr_t *pHdr;
    auto signalId = zb_get_app_signal(bufid, &pHdr);
    zb_ret_t status = zb_buf_get_status(bufid);
	auto ret = zb::tpl_signal_handler<zb::sig_handlers_t{
	    .on_dev_reboot = on_zigbee_start,
	    .on_steering = on_zigbee_start,
	    .on_can_sleep = &zb_sleep_now,
	   }>(bufid);
    const uint32_t LOCAL_ERR_CODE = (uint32_t) (-ret);	
    if (LOCAL_ERR_CODE != RET_OK) {				
	zb_osif_abort();				
    }							
}

int main()
{
    ZB_AF_REGISTER_DEVICE_CTX(zb_ctx);
    zigbee_enable();
    k_sleep(K_FOREVER);
}
```

### How to define a new cluster

```cpp
#include <nrfzbcpp/zb_main.hpp>

namespace zb
{
    static constexpr uint16_t kZB_ZCL_MY_CLUSTER_ID = 0xfc01;
    static constexpr uint16_t kZB_MY_ATTR1_ID = 0x0000;
    static constexpr uint16_t kZB_MY_ATTR2_ID = 0x0001;

    struct zb_zcl_my_cluster_t
    {
	uint8_t attr1;
	float attr2;
    };

    //This is important as it actually makes 'zb_zcl_my_cluster_t' type
    //known to the rest of the system
    template<>
    struct zcl_description_t<zb_zcl_my_cluster_t>{
        static constexpr auto get()
        {
            using T = zb_zcl_my_cluster_t;
            return cluster_struct_desc_t<
                cluster_info_t{.id = kZB_ZCL_MY_CLUSTER_ID/*, .role=Role::Server <- default*/},
                cluster_attributes_desc_t<
                    cluster_mem_desc_t{.m = &T::attr1,.id = kZB_MY_ATTR1_ID, .a=Access::RW/*, .type=Type::U8 <- autodeduced, but can be explicit, see zb::Type in zb_types.hpp*/}
                    ,cluster_mem_desc_t{.m = &T::attr2,.id = kZB_MY_ATTR2_ID, .a=Access::RP}
                >{}
            >{};
        }
    };
}
```
Created in this way cluster may be used then in a straightforward way:
```cpp
struct device_ctx_t{
    zb::zb_zcl_basic_names_t basic_attr;
    zb::zb_zcl_my_cluster_t my_cluster;
};
constexpr auto kA1 = &zb::zb_zcl_my_cluster_t::attr1;
constexpr auto kA2 = &zb::zb_zcl_my_cluster_t::attr2;

/* Zigbee device application context storage. */
//Here lives actual data of the clusters
static constinit device_ctx_t dev_ctx{
    .basic_attr = {
	{
	    .zcl_version = ZB_ZCL_VERSION,
	    .power_source = zb::zb_zcl_basic_min_t::PowerSource::Battery
	},
	/*.manufacturer =*/ "Manufacture",
	/*.model =*/ "ModelID",
    },
    .my_cluster = {
	.attr1 = 2,//some initial value
	.attr2 = 1,//some initial value
    }
};

constinit static auto zb_ctx = zb::make_device(
	zb::make_ep_args<{.ep=1, .dev_id=1234, .dev_ver=1}>(
	    dev_ctx.basic_attr
	    , dev_ctx.my_cluster
	    )
	);

//for convenient access to some typical operations later
constinit static auto &zb_ep = zb_ctx.ep<1>();
//...and usage
zb_ep.attr<kA1>() = 16;
zb_ep.attr<kA2>() = 1024.56f;
```

## Known issues with compilers
LLVM vs GCC (constinit issue)
