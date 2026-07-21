# NRF ZBOSS C++ wrappers

<!-- mtoc-start -->

* [Goal](#goal)
* [Examples](#examples)
  * [How to define a new cluster](#how-to-define-a-new-cluster)
  * [Defining commands](#defining-commands)
    * [Command pools and queues](#command-pools-and-queues)
    * [Receiving commands](#receiving-commands)
  * [Typical signal handling](#typical-signal-handling)
  * [Handling attribute writes](#handling-attribute-writes)
    * [Recommended type-safe approach with `zb::handle_set_for`](#recommended-type-safe-approach-with-zbhandle_set_for)
    * [Low-level raw approach](#low-level-raw-approach)
    * [Higher level typed approach (not safe though)](#higher-level-typed-approach-not-safe-though)
* [Internals](#internals)
  * [Attributes](#attributes)
    * [Attributes for custom structs](#attributes-for-custom-structs)
    * [Attribute validation](#attribute-validation)
  * [Clusters](#clusters)
  * [End Points](#end-points)
  * [Commands subsystem](#commands-subsystem)
* [Known issues with compilers](#known-issues-with-compilers)

<!-- mtoc-end -->

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
	zb::make_ep_args<{.ep=kDEV_EP, .dev_id=kDEV_ID, .dev_ver=1}>(
	    dev_ctx.basic_attr
	    , dev_ctx.battery_attr
	    , dev_ctx.poll_ctrl
	    , dev_ctx.temperature
	    )
	);

//for convenient access to some typical operations later
constinit static auto &zb_ep = zb_ctx.ep<kDEV_EP>();

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
    static constexpr uint8_t kZB_MY_INBOUND_CMD_ID = 0x01;
    static constexpr uint8_t kZB_MY_OUTBOUND_CMD_ID = 0x02;

    struct zb_zcl_my_cluster_t
    {
	uint8_t attr1;
	float attr2;

	cmd_in_t<kZB_MY_INBOUND_CMD_ID, float/*arg1*/, uint8_t/*arg2*/> my_inbound_cmd1;
	cmd_out_t<kZB_MY_OUTBOUND_CMD_ID, uint8_t/*arg1*/> my_outbound_cmd2;
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
                attributes_t<
                    attribute_t{.m = &T::attr1,.id = kZB_MY_ATTR1_ID, .a=Access::RW/*, .type=Type::U8 <- autodeduced, but can be explicit, see zb::Type in zb_types.hpp*/}
                    ,attribute_t{.m = &T::attr2,.id = kZB_MY_ATTR2_ID, .a=Access::RP}
                >{},
		commands_t<
		    &T::my_inbound_cmd1,
		    &T::my_outbound_cmd2
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
constexpr auto kCmdOut = &zb::zb_zcl_my_cluster_t::my_outbound_cmd2;

//forward declaration of the inbound command handler
CmdHandlingResult my_inbound_cmd1_handler(float a1, uint8_t a2);

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
	.my_inbound_cmd1 = {.cb = my_inbound_cmd1_handler}
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
zb_ep.send_cmd<kCmdOut>(42/*Arg1, uint8_t*/);
...
CmdHandlingResult my_inbound_cmd1_handler(float a1, uint8_t a2)
{
    //handling incoming zb::kZB_MY_INBOUND_CMD_ID
    return {};
}
```

### Defining commands
In order to define commands that may be sent by a cluster (doesn't matter, server or client),
the following types are to be used:
```cpp
//simplified version
cmd_out_t<COMMAND_ID, Arguments...> my_command;
//generic, configurable version
cmd_generic_t</*cmd_cfg_t*/{.cmd_id=...,...},Arguments...> my_command2;
```
Example:
```cpp
namespace zb
{
    static constexpr uint16_t kZB_ZCL_MY_CLUSTER_ID = 0xfc01;
    static constexpr uint16_t kZB_MY_ATTR1_ID = 0x0000;
    static constexpr uint16_t kZB_MY_ATTR2_ID = 0x0001;
    static constexpr uint16_t kZB_MY_CMD1 = 0x0001;
    static constexpr uint16_t kZB_MY_CMD2 = 0x0002;

    struct cmd2_args
    {
	int8_t a1;
	float b2;
    }ZB_PACKED_STRUCT;

    struct zb_zcl_my_cluster_t
    {
	uint8_t attr1;
	float attr2;
	[[no_unique_address]]cmd_generic_t<{.cmd_id=kZB_MY_CMD1}, int16_t> my_command1;
	[[no_unique_address]]cmd_generic_t<{.cmd_id=kZB_MY_CMD2}, cmd2_args> my_command2;
    };

    template<>
    struct zcl_description_t<zb_zcl_my_cluster_t>{
        static constexpr auto get()
        {
            using T = zb_zcl_my_cluster_t;
            return cluster_struct_desc_t<
                cluster_info_t{.id = kZB_ZCL_MY_CLUSTER_ID},
                attributes_t<
                    attribute_t{.m = &T::attr1,.id = kZB_MY_ATTR1_ID, .a=Access::RW}
                    ,attribute_t{.m = &T::attr2,.id = kZB_MY_ATTR2_ID, .a=Access::RP}
                >{}
		//this bit here declares commands as part of the cluster
                ,commands_t<
                     &T::my_command1
                     ,&T::my_command2
                >{}
            >{};
        }
    };
}
```
Note: `[[no_unique_address]]` will tell the compiler it can optimize a layout as these commands
don't actually carry any data.
Sending such commands can be done by using methods of EP similar to working with attributes: 
```cpp
constexpr auto kCmd1 = &zb::zb_zcl_my_cluster_t::my_command1;
constexpr auto kCmd2 = &zb::zb_zcl_my_cluster_t::my_command2;
//...
zb_ep.send_cmd<kCmd1>(-32768);
zb_ep.send_cmd<kCmd2>(zb::cmd2_args{.a1 = 3, .b2 = 0.5f});
```
These are however shoot-and-forget. 
A callback if needed can be provided (at the moment required to be known at compile time) like this:
```cpp
void on_cmd_sent(zb::cmd_id_t cmd_id, zb_zcl_command_send_status_t *status)
{
    printk("zb: on_cmd_sent id:%d; status: %d\r\n", cmd_id, status->status);
}

auto cmd_id1 = zb_ep.send_cmd<kCmd1, {.cb = on_cmd_sent}>(-32768);
auto cmd_id2 = zb_ep.send_cmd<kCmd2, {.cb = on_cmd_sent}>(zb::cmd2_args{.a1 = 3, .b2 = 0.5f});
```

#### Command pools and queues
Note: outdated
Apparently ZBOSS doesn't really like attempts to send several commands for the same cluster (endpoint?) in parallel at the same time (or while
the other one is still being sent). To make sure that doesn't happen, there's a concept of the 'command queue' that exists on the
endpoint level.

The relevant class is `EPDesc<>`, which defines a `kCmdQueueSize` constant. The value for it is either taken from the
`EPBaseInfo::cmd_queue_depth` (available in the 1st template parameter to `EPDesc`) or inferred from all the commands across all the clusters
based on the maximum pool size of those commands.

The amount of each kind of command that the sending process can be initiated for is defined by the size of the pool (see `cluster_cmd_desc_t<...>::g_Pool`). 
The pools size for each command can be configured with `cmd_cfg_t::pool_size` (default 1).
There's also a helping class `cmd_out_t` that allows specifying a pool size.
Example:
```cpp
cmd_out_t<CMD_ID> cmd1;
cmd_generic_t<{.cmd_id=CMD_ID2}> cmd2;
```

Each send method (e.g. `send_cmd`, `send_cmd_to`) returns a `std::optional<cmd_id_t>`, where `cmd_id_t` is a a command index generated
by the logic of the `EPDesc<>` or `std::nullopt` optional if the command could not be sent (if either arguments could not be allocated and stored
in command's pool  `cluster_cmd_desc_t<...>::g_Pool` or the command could not be put into a command queue `EPDesc::g_CmdQueue`).

#### Receiving commands
Type to use in a cluster:
```cpp
cmd_in_t<kID, Args...> cmd_to_receive;
```
Commands that can be received carry a callback that can be set. Callback is of type 
`zb::CmdHandlingResult (*)(const Args &...)`.<br>
A `zb::CmdHandlingResult` is defined like:
```cpp
struct CmdHandlingResult
{
    zb_ret_t status = RET_OK;
    bool processed = true;
};
```
If `processed` is set to true, no further handling will be done. Otherwise a default handling will happen.
Unless command came with a `disable_default_response` flag set or `status` is returned as `RET_BUSY`, 
a command response will be generated.<br>
Example of usage:
```cpp
struct some_cluster_t
{
    cmd_in_t<kID, Args...> cmd_to_receive;
};

struct dev_ctx_t
{
    some_cluster_t c;
};

//forward declaration
zb::CmdHandlingResult my_cmd_handler(const Args &...);

static constinit dev_ctx_t dev_ctx{
    .c = {
	.cmd_to_receive = { .cb = my_cmd_handler }
    }
};

zb::CmdHandlingResult my_cmd_handler(const Args &...)
{
    printk("Received a command\r\n");
    return {};//processed, RET_OK
}
```

ZBOSS provides a way initialize a custom cluster or additionaly customize a behavior of the standard one.
The field in question is `zb_zcl_cluster_desc_t::cluster_init`.
During that init phase its typical to add some custom handlers on such events as:
 * checking validity of the value before it's written into an attribute
 * writing an attribute hook
 * handler for an incoming command

The way to set those is `zb_zcl_add_cluster_handlers`.
The library does assigns/generate a default initializer `generic_cluster_init` to the cluster (defined in `nrfzbcpp/zb_desc_helper_types_cluster.hpp`).
It also calls the original zboss's init function if it' available in `zcl_description_t` as a static `zboss_init_func` function.
If a cluster description obtained via `zcl_description_t<>::get` reports some amount of commands that may be received, `zb_zcl_add_cluster_handlers`
is invoked with a `on_cluster_cmd_handling` as a default handler. It takes care of a certain boilerplate logic (like reacting to `ZB_ZCL_GENERAL_GET_CMD_LISTS_PARAM`
and returning an array of existing commands) but the actual command handling is forwarded to a specialization of a `zb::cluster_custom_handler_t<...>` class.
Static function `on_cmd` is invoked. All of that boilerplate logic is already implemented by a `zb::cluster_custom_handler_t`. The only thing needed is to define
a `zb::global_device` struct with a static method `get` that returns a reference to device zigbee context:
```cpp
constinit static auto zb_ctx = zb::make_device(
	zb::make_ep_args<{.ep=1, .dev_id=1234, .dev_ver=1}>(
	    dev_ctx.basic_attr
	    , dev_ctx.my_cluster
	    )
	);
struct zb::global_device
{
    static auto& get() { return zb_ctx; }
};
```
The `get()` static function returns a reference to a zigbee device object (result of the `zb::make_device` call). 
`cluster_custom_handler_t` requires it search in a correct end point for a correct cluster when performing 
`get_cmd_list` or `on_cmd`.

This is typically the only thing needed to be implemented on user's side to be able to handle incomming commands (aside from the actual command handling
callback of course).

### Typical signal handling
In general for a signal handling a `zboss_signal_handler` must be defined. It accepts `zb_bufid_t bufid` as an argument. And with a code snippet like:

```cpp
zb_zdo_app_signal_hdr_t *pHdr;
auto signalId = zb_get_app_signal(bufid, &pHdr);
```

one can get the basic information no the signal.
The alternative `zb::tpl_signal_handler` provided by this library is arguably a little bit type-safer.

```cpp
auto ret = zb::tpl_signal_handler<zb::sig_handlers_t{
    .on_leave = ...,
    .on_error = ...,
    .on_dev_reboot = ...,
    ...
    .on_steering = ...,
   }>(bufid);
```
User can define handlers for the signals that pose the interest. Handlers are defined by initializing
corresponding fields in `zb::sig_handlers_t` and passing the `zb::sig_handlers_t` as a template argument to 
`zb::tpl_signal_handler`. In other words handling must be completely defined at a compile time. If that's
not possible you should resort to a default signal handling.
The complete list of signal handlers can be found at `include/nrfzbcpp/zb_signals.hpp` (`struct sig_handlers_t`).

### Handling attribute writes
Configuration of attribute writes handling is done at a compile time as template arguments to a base generic template device callback function
`zb::tpl_device_cb`. The first template argument of type `zb::dev_cb_handlers_desc` allows to specify an error handler for cases when an unexpected situation
has happened (not valid `zb_zcl_device_callback_param_t`, unexpected state of the runtime attribute handling nodes) and also a custom handling function
for cases defined in `zb_zcl_device_callback_id_e` enum. After `zb::dev_cb_handlers_desc` 0 or more arguments of type `zb::set_attr_val_gen_desc_t` may follow.
Each of the objects of that type define the `endpoint`, `cluster` and an `attribute` and a handling function.

#### Recommended type-safe approach with `zb::handle_set_for`
It requires a reference to the attribute member variable and a function-handler. At compile time a check will be performed
making sure the attribute member type can be converted to the 1st argument of the handler function (if it accepts at least 1).
`zb::handle_set_for` accepts 2 template parameters: 
1) a reference to the attribute member variable 
2) a pointer to the set-handler function

it also accepts 1 function parameter which is a reference to the endpoint subobject.
Example:

```cpp
void on_set_a1(float v)
{
    //do something with V (it's already set in my_cluster.attr1 at this point
}
void on_set_a2(uint8_t v)
{
    //do something with V (it's already set in my_cluster.attr2 at this point
}
...
auto dev_cb = zb::tpl_device_cb<
    zb::dev_cb_handlers_desc{ .error_handler = ... }
    , zb::handle_set_for<kAttr1, on_set_a1>(zb_ep)
    , zb::handle_set_for<kAttr2, on_set_a2>(zb_ep)
>;
```
Types of `kAttr1` and `on_set_a1` 1st argument don't have to be the same but it should be possible to implicitly converta kAttr1 into 1st argument
of `on_set_a1`.

#### Low-level raw approach
In the most low-level way it may look like:
```cpp
void my_raw_attribute_handler(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam);
...
zb::set_attr_val_gen_desc_t{ {.ep=EP_ID, .cluster=CLUSTER_ID, .attribute=ATTRIBUTE_ID} ,my_raw_attribute_handler }
```
There are also alternative and easier ways to define such handling. Endpoint/Cluster/Attribute part may be obtained by calling an endpoint constexpr method
`attribute_desc` with an attribute member pointer as a template argument:

```cpp
//given the following device context with clusters
struct device_ctx_t{
    zb::zb_zcl_basic_names_t basic_attr;
    zb::zb_zcl_my_cluster_t my_cluster;
};
//defining shortcut aliases for convenience
constexpr auto kA1 = &zb::zb_zcl_my_cluster_t::attr1;
constexpr auto kA2 = &zb::zb_zcl_my_cluster_t::attr2;
//making an actual ZBoss-conformant device
constinit static auto zb_ctx = zb::make_device(...);
//obtaining a reference to endpoint subobject
constinit static auto &zb_ep = zb_ctx.ep<1>();
...
zb::set_attr_val_gen_desc_t{ zb_ep.attribute_desc<kA1>()               , ... }
zb::set_attr_val_gen_desc_t{ zb_ep.attribute_desc<kA2>()               , ... }
```

#### Higher level typed approach (not safe though)
Handling of the attribute writes may be done in a type-friendly way.
In order to achieve it a helping constexpr template variable may be used: `zb::to_handler_v`.
It may accept 7 different signatures of the 'attribute set' function:

```cpp
void func1(const T&);
void func2(const T&, zb_zcl_device_callback_param_t *pDevCBParam);
void func3(const T&, zb_zcl_device_callback_param_t *pDevCBParam, zb_zcl_set_attr_value_param_t *p);
void func4();
void func5(T);
void func6(T, zb_zcl_device_callback_param_t *pDevCBParam);
void func7(T, zb_zcl_device_callback_param_t *pDevCBParam, zb_zcl_set_attr_value_param_t *p);
```

`func1-3` are the same as `func5-7`; the only difference is reference vs copy of T.
`func4` doesn't accept any parameters and is for pure notificational purposes.

Example:

```cpp
//given the following device context with clusters
struct device_ctx_t{
    zb::zb_zcl_basic_names_t basic_attr;
    zb::zb_zcl_my_cluster_t my_cluster;
};
//defining shortcut aliases for convenience
constexpr auto kA1 = &zb::zb_zcl_my_cluster_t::attr1;
constexpr auto kA2 = &zb::zb_zcl_my_cluster_t::attr2;
//making an actual ZBoss-conformant device
constinit static auto zb_ctx = zb::make_device(...);
//obtaining a reference to endpoint subobject
constinit static auto &zb_ep = zb_ctx.ep<1>();
...
void on_set_a1(float v)
{
    //do something with V (it's already set in my_cluster.attr1 at this point
}
void on_set_a2(uint8_t v)
{
    //do something with V (it's already set in my_cluster.attr2 at this point
}
...
auto dev_cb = zb::tpl_device_cb<
	zb::dev_cb_handlers_desc{ .error_handler = ... },
	zb::set_attr_val_gen_desc_t{ zb_ep.attribute_desc<kA1>() , to_handler_v<on_set_a1> },
	zb::set_attr_val_gen_desc_t{ zb_ep.attribute_desc<kA2>() , to_handler_v<on_set_a2> }
    >;
ZB_ZCL_REGISTER_DEVICE_CB(dev_cb);
```

## Internals
Many/all features of this library is built around templates and NTTP's.

### Attributes
 * Main description structure for attributes is `zb::attribute_mem_desc_t` (aliased as `zb::attribute_t`). It's a constexpr-capable
    struct with the following members:
     - `m` - a reference to a member variable (that'll hold the actual data for the attribute)
     - `id` - as per Zigbee spec a uint16_t ID of the attribute
     - `a` - access type for the attribute (see `zb::Access` enum)
     - `type` - a Zigbee type of the attribute. See `zb::Type` enum for allowed values. It can be 
	auto-inferred with the help of `zb::TypeToTypeId` or defined explicitly at the point of declaration of the cluster.
     - `has_access`: a constexpr method given a `zb::Access` as an argument returns `true` if bit-and operation with own `a` is non-0
     - `is_cvc`: a constexpr method that returns `true` if the `type` of the attribute belongs to the types that are `reportable` (I'm
       not sure about the precise definition, but ZBOSS seems to require a proper count of such attributes in order to properly consifugre reporting
       for them)
 * `zb::find_attribute_by_mem_ptr_t` is a template for searching an attribute description (`zb::attribute_mem_desc_t`) by a pointer to a member
 * `zb::cluster_attributes_desc_t` is a class template that exists only to store attribute descriptions as its variadic NTTPs. 
    - operator `+`: for convenience purposes so that you could add another such `zb::cluster_attributes_desc_t` and get a new `zb::cluster_attributes_desc_t`
	combining attribute descriptions of 2 operands.
    - `count_members_with_access`: a constexpr static method allowing to count amount attribute descriptions with a given `zb::Access` type
      by calling `zb::attribute_mem_desc_t::has_access` for all attribute descriptions and counting the amount of `true` returned.
    - `count_cvc_members`: a constexpr static method allowing to count amount attribute descriptions with reportable types
      by calling `zb::attribute_mem_desc_t::is_cvc` for all attribute descriptions and counting the amount of `true` returned.
 * `TAttributeList<StructTag, N>`: a key template class for defining runtime (const-initialized) data needed for ZBOSS.
    - `StructTag`: is a C++ struct type where attribute's actual member belongs to (e.g. `zb::zb_zcl_basic_names_t`).
    - `N`: total amount of attributes in the cluster. `attributes` array member is defined based on the `N` (+2, for the first mandatory
      `ZB_ZCL_ATTR_GLOBAL_CLUSTER_REVISION_ID` and the last `ZB_ZCL_NULL_ID`.
    - `cluster_struct`: it's an actual pointer to the `StructTag` variable so that at runtime correct pointers based on the pointers to members
      could be inferred and used to set/get the attribute data.
    - `rev`: a cluster revision (used as a data storage for `ZB_ZCL_ATTR_GLOBAL_CLUSTER_REVISION_ID` mandatory first attribute)
    - `received_commands`: list of command ids that may be received for this cluster
    - `generated_commands`: list of command ids that may be generated (sent) by this cluster
    - `cmd_list`: a ZBOSS definition of the command lists with the amount of commands of each type (receive/generate)
    - `constructor` accepts the pointer to the actual `StructTag` (will be written to `cluster_struct`) and a variadic amount of `zb::ADesc` types.
 * `zb::ADesc`  is the same as `zb::attribute_mem_desc_t` but contains an actual pointer to the data and not a member pointer.
 * `AttrDesc` is a function to convert a `zb::ADesc` to a `zb_zcl_attr_t` (what ZBOSS wants)
 * `attribute_declaration_to_real_attribute_description`: given a reference to a cluster type `T` (actual C++ struct type to store attribute data in members)
   and an attribute declaration `zb::attribute_mem_desc_t` it returns a correspoinding `ADesc` with an actual pointer to attribute data.
#### Attributes for custom structs
The library provides wrapper types to use non-standard C++ types as Zigbee attributes. These serialize/deserialize as Zigbee Octet Strings (variable-length binary blobs).

`zb::ZigbeeBinTyped<T>` — Wraps an arbitrary trivially-copyable type `T` as a binary attribute (`sizeof(T) <= 255`). Wire layout is `[length byte][T as raw bytes]`. Implicit conversions to `T&` and `const T&` allow transparent usage:

```cpp
struct sensor_cfg_t {
    float range_min = 0.2f;
    float range_max = 10.0f;
};

struct my_cluster_t {
    ZigbeeBinTyped<sensor_cfg_t> config;
};

// Usage in cluster description:
attribute_t{.m = &T::config, .id = 0x0001, .a = Access::RW}
```

`zb::ZigbeeStr<N>` — Fixed-length string with a length byte prefix (`TypeId()` = `Type::CharStr`, use `zb::ZbStr()` factory). `zb::ZigbeeBinTypedArray<T, N>` — Array of `T` with length prefix. `zb::ZigbeeOctetBuf<N>` — raw N-byte octet string (`TypeId()` = `Type::OctetStr`). `zb::ZigbeeBin<N>` — alternative binary wrapper with data-as-payload layout.

All custom-type wrappers are documented in the [Custom struct section](#attributes-for-custom-structs) above, and define `static Type TypeId()` and `static bool TypeValidator(uint8_t*)` methods that are automatically wired to the `validator` field of attributes whose member type is one of them.

#### Attribute validation
The `zb::attribute_mem_desc_t` struct has a `validator` field (`attr_validator_t`, a `bool(*)(uint8_t *value)` function pointer). If non-null, this function is called by ZBOSS before writing the attribute value. The validator receives a raw byte pointer to the incoming wire-format data and returns `true` if the value is accepted.

The validator for each attribute is populated automatically via C++20 `requires`:
```cpp
attr_validator_t validator = ValidatorForType<MemType>();
```
If `MemType` has a static `TypeValidator(uint8_t*)` method, that becomes the validator; otherwise it defaults to `nullptr`.

For custom types wrapped in `ZigbeeBinTyped`, validation chains to a user-defined `T::Validate(const T*)` static method:
1. `ZigbeeBinTyped::TypeValidator(value)` first checks that `*value == sizeof(T)`.
2. It then calls `ValidateCustomType((const T*)(value + 1))`.
3. `ValidateCustomType` uses C++20 detection: if `T::Validate(const T*)` exists, it calls it; otherwise returns `true`.

**Usage pattern:**
```cpp
struct base_cfg_t {
    float range_min = 0.2f;
    float range_max = 10.0f;

    static bool Validate(const base_cfg_t *pCfg) {
        return pCfg && pCfg->range_min < pCfg->range_max;
    }
};

struct my_cluster_t {
    ZigbeeBinTyped<base_cfg_t> config;
};

// Validator is auto-wired. No manual setup needed.
attribute_t{.m = &T::config, .id = 0x0001, .a = Access::RW}

// Override can still be set explicitly:
attribute_t{.m = &T::foo, .id = 0x0002, .a = Access::RW, .validator = &T::custom_validator_fn}
```

### Clusters
* `zb::cluster_info_t` is a structure for a basic cluster description, consists of following members:
  - `zb_uint16_t id` (**`mandatory`**) - cluster ID according to Zigbee spec
  - `zb_uint16_t rev` - cluster revision (default `0`)
  - `Role role` - cluster role (typically `Role::Server` or `Role::Client`), default: `Role::Server`
  - `zb_uint16_t manuf_code` - manufacturer code (default `ZB_ZCL_MANUF_CODE_INVALID`)
* `cluster_struct_desc_t` is a main description structure type for a Zigbee cluster. It's also aliased as `cluster_t`.
  It's template parameters are:
    - `zb::cluster_info_t ci`
    - `zb::cluster_attributes_desc_t attributes` (default `zb::cluster_attributes_desc_t</*empty attribute list*/>{}`)
    - `zb::cluster_commands_desc_t cmds` (default `zb::cluster_commands_desc_t</*empty command list*/>{}`)
  It has no own data members and it's purpose is to provide a compile-time template-friendly description of a cluster.
  Following `static` `constexpr` member functions are available:
    - `info` - a method to access `zb::cluster_info_t` template parameter
    - `count_members_with_access` - call to `zb::cluster_attributes_desc_t::count_members_with_access()` 
       returns the amount of attributes with a given access
    - `count_cvc_members` - call to  `zb::cluster_attributes_desc_t::count_cvc_members()`
       returns the amount of attributes potentially available for reporting tracking
    - `max_command_pool_size` - call to `zb::cluster_commands_desc_t::max_command_pool_size()`
       returns the maximum amount of commands may be 'in flight' at the same time
    - `count_generated` - call to  `zb::cluster_commands_desc_t::count_generated()`
       returns the amount of commands may be sent (generated) by the device
    - `count_received` - call to  `zb::cluster_commands_desc_t::count_received()`
       returns the amount of commands may be received by the device
    - `get_generated_commands` - call to  `zb::cluster_commands_desc_t::get_generated_commands()`
       returns the array with command IDs that may be sent (generated) by the device
    - `get_received_commands` - call to  `zb::cluster_commands_desc_t::get_received_commands()`
       returns the array with command IDs that may be received by the device
    - `find_cmd_handler(cmdId, auto *pClusterStruct)` - call to `zb::cluster_commands_desc_t::find_cmd_handler()`
       given a command ID to be received and a pointer to the actual `struct` representing the data of the cluster
       it tries to find and return a pointer to a `cluster_in_cmd_desc_t` member variable of `pClusterStruct` and it's `raw_handler`
    - `get_member_description<memPtr>` - return a `zb::attribute_mem_desc_t` given a pointer to a member variable (see [Attributes](#attributes))
    - `get_cmd_description<memPtr>` - return a `zb::cluster_cmd_desc_t` given a pointer to a member variable (see [Commands subsystem](#commands-subsystem))
     - `operator+` - a convenience way of adding attributes and commands from one cluster to another 
        (e.g. cluster defining a base functionality may be extended by another cluster with the same ID defining more optional attributes.
        see `zb_zcl_basic_names_t` vs `zb_zcl_basic_min_t`)
* `zcl_description_t<zb_struct>` — A template struct specialized for every cluster data type. Its static `get()` method returns a compile-time descriptor (`cluster_struct_desc_t`). Patterns: base specialization, extension via `operator+`, client-only (no attributes), and variadic template specialization with fold expressions `(zcl_description_t<Cfg>::get() + ...)`.
* `cluster_struct_to_attr_list(s, desc)` — Converts a cluster struct instance and compile-time descriptor into runtime ZBOSS attribute descriptors (`TAttributeList<StructTag, N>`). Internally uses `attribute_declaration_to_real_attribute_description()` to turn member pointers into real data pointers.
* `TClusterList<ep, ClusterTypes...>` — Variadic bundle of multiple cluster attribute lists into a single endpoint's cluster descriptor array. Requires client clusters at end (ZBOSS layout rule). Builds `alignas(4) zb_zcl_cluster_desc_t[]`, provides constexpr helpers: `cvc_attributes_count()`, `reporting_attributes_count()`, `server/client_cluster_count()`, `max_command_pool_size()`.

### End Points
Endpoints are the core organizational unit in Zigbee, represented by `EPDesc` and `EPDescSelfContained`.

`EPBaseInfo` — Metadata for an endpoint:
- `ep` — endpoint number (1-255)
- `dev_id` — device ID for simple descriptor
- `dev_ver` — device version
- `cmd_queue_depth` — 0 = auto-infer from cluster command pool sizes

`EPDesc<EPBaseInfo i, Clusters>` — The primary endpoint description class templated on base info and a `TClusterList`. Key features:
- Computes `kCmdQueueSize = max_command_pool_size()` or uses `cmd_queue_depth` if explicitly set.
- Stores runtime ZBOSS structures: `zb_af_endpoint_desc_t ep`, simple descriptor, reporting arrays (`rep_ctx[]`), CVC alarm context (`cvc_alarm_ctx[]`).
- Attribute access: `attr<memPtr>()` and `attr_checked<memPtr>()` perform type-safe assignment to ZBOSS attributes.
- Attribute descriptors: `attribute_desc<memPtr>()` returns a `EPClusterAttributeDesc_t` for use in device callbacks (set_attr handling).
- Command sending: multiple overloads of `send_cmd()` — direct, to short address, long address, group, or bind-table target. Returns `std::optional<cmd_id_t>` (command pool index or `nullopt` if queue full).
- Static members: `g_CmdQueue`, `g_CmdTimeoutTracker`, `g_cmd_num` for endpoint-level command pooling.

`EPDescSelfContained<EPBaseInfo i, ClusterTypes...>` — Alternative variant that also stores the actual cluster data structs alongside attribute descriptors. Provides `.attribute_list<StructTag>()` to retrieve a specific cluster's attribute list at runtime.

Address mode types: `ShortAddr`, `LongAddr`, `GroupAddr`, `BindIdAddr` helper structs for specifying command destinations. Factory functions: `to_short()`, `to_long()`, `to_group()`, `to_bind_id()`.

### Commands subsystem
Commands are managed through a pool-based dispatch system on the endpoint level.

`cluster_cmd_desc_t<cfg, Args...>` — The core command type template (in `zb_desc_helper_types_cluster.hpp`). Key properties:
- Template parameter `cfg` (`cmd_cfg_t`) specifies `cmd_id`, `pool_size`, and `receive` flag.
- Holds a static `ObjectPool<runtime_args_t, pool_size>` (`g_Pool`) to buffer command arguments, preventing concurrent-send conflicts in ZBOSS.
- **Sent commands**: use `prepare_args(cb, args...)` to pack arguments for sending. Returns an argument pool index (`uint16_t`). Then call `request(i, payload)` or `cancel(i)`. Static callbacks fire on completion/timeout/cancel. The sender type aliases:
  - `cmd_generic_t<{.cmd_id=..., .pool_size=...}, Args...>` — fully configurable command with optional callback.
  - `cmd_out_t<id, Args...>` — convenience alias for sent commands 

`cluster_in_cmd_desc_t` / `cmd_in_t<id, Args...>` — Inbound (received) command type. Parameters: `pool_size=0, receive=true`. Carries a user-settable callback of type `CmdHandlingResult(*)(Args const&...)` stored as `.cb`. Wire bytes are unpacked via `cmd_to_arg` (handles alignment). The `raw_handler` static method dispatches to the registered callback when invoked by ZBOSS.

`CmdHandlingResult` — Return type for inbound command handlers:
- `status`: `RET_OK` sends a success response; `RET_BUSY` suppresses default handling.
- `processed`: `true` skips further/default response processing; `false` lets ZBOSS continue.

Inbound dispatch chain: When ZBOSS receives a command, it invokes `cluster_custom_handler_t<StructTag>::on_cmd()`, which calls `attribute_list.find_handler_for_cmd(cmdId)` to find the `raw_handler` and field pointer, then forwards the call with parsed wire data. The handler (if `.cb != nullptr`) is invoked with unpacked typed arguments.

`cluster_commands_desc_t` — Compile-time registry of command member pointers in a cluster. Exposes: `count_generated()`, `count_received()`, `find_cmd_handler(id, pStruct)`, `get_generated/received_commands()`. Wired into clusters via `commands_t` NTTP in `cluster_struct_desc_t`.

## Known issues with compilers
The code compiles fine with `clang++-19`, `clang++-20` with `-std=c++23` option enabled.
GCC 12.2 (which comes with NCS SDK from Nordic) struggles with some constexpr's, declaring
it impossible to do `constinit` on a `zb_ctx` (the result of the `zb::make_device` call).
Removing `constinit` helps to overcome the compiler issue but I'm not sure if it has
a runtime overhead as a consequence.
