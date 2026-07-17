#ifndef ZB_STD_CLUSTER_DESC_HPP_
#define ZB_STD_CLUSTER_DESC_HPP_

#include "zb_main.hpp"
#include "zb_str.hpp"

extern "C"
{
#include <zboss_api_addons.h>
#include <zb_nrf_platform.h>
}

namespace zb
{

struct zb_zcl_basic_min_t
{
    enum PowerSource: zb_uint8_t
    {
        Unknown               = 0x00,
        Mains1Phase           = 0x01,
        Mains3Phase           = 0x02,
        Battery               = 0x03,
        DC                    = 0x04,
        EmergencyConst        = 0x05,
        EmergencySwitch       = 0x06,
        BackupUnknown         = 0x80,
        BackupMains1Phase     = 0x81,
        BackupMains3Phase     = 0x82,
        BackupBattery         = 0x83,
        BackupDC              = 0x84,
        BackupEmergencyConst  = 0x85,
        BackupEmergencySwitch = 0x86,
    };
    zb_uint8_t zcl_version;
    PowerSource power_source;
} ;

struct zb_zcl_on_off_attrs_client_t
{
    [[no_unique_address]]cmd_pool_t<ZB_ZCL_CMD_ON_OFF_OFF_ID, 3> off;
    [[no_unique_address]]cmd_pool_t<ZB_ZCL_CMD_ON_OFF_ON_ID, 3> on;
    [[no_unique_address]]cmd_pool_t<ZB_ZCL_CMD_ON_OFF_ON_WITH_TIMED_OFF_ID, 3, uint8_t, uint16_t, uint16_t> on_with_timed_off;
    [[no_unique_address]]cmd_pool_t<ZB_ZCL_CMD_ON_OFF_TOGGLE_ID, 3> toggle;
};

template<> struct zcl_description_t<zb_zcl_basic_min_t> {
    static constexpr auto get()
    {
	using T = zb_zcl_basic_min_t;
	return cluster_t<
	{.id = ZB_ZCL_CLUSTER_ID_BASIC},
	    attributes_t<
		attribute_t{.m = &T::zcl_version , .id = ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID },
	    attribute_t{.m = &T::power_source, .id = ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID}
	>{}
	>{};
    }
    DEFINE_ZBOSS_INIT_GETTER_FOR(ZB_ZCL_CLUSTER_ID_BASIC);
};

struct zb_zcl_basic_names_t: zb_zcl_basic_min_t
{
    zigbee_str_t<33> manufacturer;
    zigbee_str_t<33> model;
};

template<> struct zcl_description_t<zb_zcl_basic_names_t> {
    static constexpr auto get()
    {
	using T = zb_zcl_basic_names_t;
	return zcl_description_t<zb_zcl_basic_min_t>::get() + 
	    cluster_t<
	    {.id = ZB_ZCL_CLUSTER_ID_BASIC},
	    attributes_t<
		attribute_t{.m = &T::manufacturer, .id = ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID},
	        attribute_t{.m = &T::model       , .id = ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID }
	>{}
	>{};
    }
    DEFINE_ZBOSS_INIT_GETTER_FOR(ZB_ZCL_CLUSTER_ID_BASIC);
};

template<> struct zcl_description_t<zb_zcl_basic_attrs_ext_t> {
    static constexpr auto get()
    {
	using T = zb_zcl_basic_attrs_ext_t;
	return cluster_t<
	{.id = ZB_ZCL_CLUSTER_ID_BASIC},
	    attributes_t<
		attribute_t{.m = &T::zcl_version,   .id = ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID},
	        attribute_t{.m = &T::app_version,   .id = ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID},
	        attribute_t{.m = &T::stack_version, .id = ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID},
	        attribute_t{.m = &T::hw_version,    .id = ZB_ZCL_ATTR_BASIC_HW_VERSION_ID},
	        attribute_t{.m = &T::mf_name,       .id = ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,    .a = access_t::Read, .type=type_t::CharStr},
	        attribute_t{.m = &T::model_id,      .id = ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,     .a = access_t::Read, .type=type_t::CharStr},
	        attribute_t{.m = &T::date_code,     .id = ZB_ZCL_ATTR_BASIC_DATE_CODE_ID,            .a = access_t::Read, .type=type_t::CharStr},
	        attribute_t{.m = &T::power_source,  .id = ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID,         .a = access_t::Read, .type=type_t::E8},
	        attribute_t{.m = &T::location_id,   .id = ZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID, .a = access_t::RW,   .type=type_t::CharStr},
	        attribute_t{.m = &T::ph_env,        .id = ZB_ZCL_ATTR_BASIC_PHYSICAL_ENVIRONMENT_ID, .a = access_t::RW,   .type=type_t::E8},
	        attribute_t{.m = &T::sw_ver,        .id = ZB_ZCL_ATTR_BASIC_SW_BUILD_ID,             .a = access_t::Read, .type=type_t::CharStr}
	>{}
	>{};
    }
    DEFINE_ZBOSS_INIT_GETTER_FOR(ZB_ZCL_CLUSTER_ID_BASIC);
};

struct zb_zcl_identify_attrs_client_t
{
    enum effect_id: uint8_t
    {
	Blink = 0x00, Breathe = 0x01, Okay=0x02, ChannelChange=0x0b, FinishEffect=0xfe, StopEffect=0xff
    };
    enum effect_variant: uint8_t
    {
	Default = 0x00
    };
    [[no_unique_address]]cmd_pool_t<ZB_ZCL_CMD_IDENTIFY_IDENTIFY_ID, 1, uint16_t> identify;
    [[no_unique_address]]cmd_pool_t<ZB_ZCL_CMD_IDENTIFY_IDENTIFY_QUERY_ID, 1> indentify_query;
    [[no_unique_address]]cmd_pool_t<ZB_ZCL_CMD_IDENTIFY_TRIGGER_EFFECT_ID, 1, effect_id, effect_variant> trigger_effect;
};

template<> struct zcl_description_t<zb_zcl_identify_attrs_t> {
    static constexpr auto get()
    {
	using T = zb_zcl_identify_attrs_t;
	return cluster_t<
	{.id = ZB_ZCL_CLUSTER_ID_IDENTIFY},
	    attributes_t<
		attribute_t{.m = &T::identify_time, .id = ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID, .a = access_t::RW}
	>{}
	>{};
    }
    DEFINE_ZBOSS_INIT_GETTER_FOR(ZB_ZCL_CLUSTER_ID_IDENTIFY);
};

template<> struct zcl_description_t<zb_zcl_identify_attrs_client_t> {
    static constexpr auto get()
    {
	using T = zb_zcl_identify_attrs_client_t;
	return cluster_t<
	{.id = ZB_ZCL_CLUSTER_ID_IDENTIFY, .role = role_t::Client},
	    attributes_t<>{},
	    commands_t<
		&T::identify
		,&T::indentify_query
		,&T::trigger_effect
		>{}
	>{};
    }
    DEFINE_ZBOSS_INIT_GETTER_FOR(ZB_ZCL_CLUSTER_ID_IDENTIFY);
};

template<> struct zcl_description_t<zb_zcl_on_off_attrs_t> {
    static constexpr auto get()
    {
	using T = zb_zcl_on_off_attrs_t;
	return cluster_t<
	{.id = ZB_ZCL_CLUSTER_ID_ON_OFF},
	    attributes_t<
		attribute_t{.m = &T::on_off, .id = ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, .a = access_t::RPS, .type=type_t::Bool}
	>{}
	>{};
    }
    DEFINE_ZBOSS_INIT_GETTER_FOR(ZB_ZCL_CLUSTER_ID_ON_OFF);
};

template<> struct zcl_description_t<zb_zcl_on_off_attrs_client_t> {
    static constexpr auto get()
    {
	return cluster_t<
	{.id = ZB_ZCL_CLUSTER_ID_ON_OFF, .role = role_t::Client},
	    attributes_t<>{},
	    commands_t<
		&zb_zcl_on_off_attrs_client_t::on
		,&zb_zcl_on_off_attrs_client_t::off
		,&zb_zcl_on_off_attrs_client_t::on_with_timed_off
		,&zb_zcl_on_off_attrs_client_t::toggle
		>{}
	>{};
    }
    DEFINE_ZBOSS_INIT_GETTER_FOR(ZB_ZCL_CLUSTER_ID_ON_OFF);
};

template<> struct zcl_description_t<zb_zcl_groups_attrs_t> {
    static constexpr auto get()
    {
	using T = zb_zcl_groups_attrs_t;
	return cluster_t<
	{.id = ZB_ZCL_CLUSTER_ID_GROUPS},
	    attributes_t<
		attribute_t{.m = &T::name_support, .id = ZB_ZCL_ATTR_GROUPS_NAME_SUPPORT_ID, .a = access_t::Read, .type=type_t::Map8}
	>{}
	>{};
    }
    DEFINE_ZBOSS_INIT_GETTER_FOR(ZB_ZCL_CLUSTER_ID_GROUPS);
};

template<> struct zcl_description_t<zb_zcl_scenes_attrs_t> {
    static constexpr auto get()
    {
	using T = zb_zcl_scenes_attrs_t;
	return cluster_t<
	{.id = ZB_ZCL_CLUSTER_ID_SCENES},
	    attributes_t<
		attribute_t{.m = &T::scene_count,   .id = ZB_ZCL_ATTR_SCENES_SCENE_COUNT_ID,   .a = access_t::Read}
	      , attribute_t{.m = &T::current_scene, .id = ZB_ZCL_ATTR_SCENES_CURRENT_SCENE_ID, .a = access_t::Read}
	      , attribute_t{.m = &T::scene_valid,   .id = ZB_ZCL_ATTR_SCENES_SCENE_VALID_ID,   .a = access_t::Read, .type=type_t::Bool}
	      , attribute_t{.m = &T::name_support,  .id = ZB_ZCL_ATTR_SCENES_NAME_SUPPORT_ID,  .a = access_t::Read, .type=type_t::Map8}
	      , attribute_t{.m = &T::current_group, .id = ZB_ZCL_ATTR_SCENES_CURRENT_GROUP_ID, .a = access_t::Read}
	>{}
	>{};
    }
    DEFINE_ZBOSS_INIT_GETTER_FOR(ZB_ZCL_CLUSTER_ID_SCENES);
};


    template<> struct zcl_description_t<zb_zcl_level_control_attrs_t> {
    static constexpr auto get()
    {
	using T = zb_zcl_level_control_attrs_t;
	return cluster_t<
	{.id = ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL},
	    attributes_t<
		attribute_t{.m = &T::current_level, .id = ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID, .a = access_t::RPS}
	      , attribute_t{.m = &T::remaining_time, .id = ZB_ZCL_ATTR_LEVEL_CONTROL_REMAINING_TIME_ID, .a = access_t::Read}
	>{}
	>{};
    }
    DEFINE_ZBOSS_INIT_GETTER_FOR(ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL);
};

struct zb_zcl_level_control_attrs_client_t
{
    enum fade_mode_e: uint8_t{ Up = 0, Down = 1};
    [[no_unique_address]]cmd_pool_t<ZB_ZCL_CMD_LEVEL_CONTROL_MOVE_TO_LEVEL, 1, uint8_t, uint16_t, uint8_t, uint8_t> move_to_level;
    [[no_unique_address]]cmd_pool_t<ZB_ZCL_CMD_LEVEL_CONTROL_MOVE, 1, fade_mode_e, uint8_t, uint8_t, uint8_t> move;
    [[no_unique_address]]cmd_pool_t<ZB_ZCL_CMD_LEVEL_CONTROL_STEP, 1, fade_mode_e, uint8_t, uint16_t, uint8_t, uint8_t> step;
    [[no_unique_address]]cmd_pool_t<ZB_ZCL_CMD_LEVEL_CONTROL_STOP, 1, uint8_t, uint8_t> stop;
    [[no_unique_address]]cmd_pool_t<ZB_ZCL_CMD_LEVEL_CONTROL_MOVE_TO_LEVEL_WITH_ON_OFF, 1, uint8_t, uint16_t, uint8_t, uint8_t> move_to_level_with_on_off;
    [[no_unique_address]]cmd_pool_t<ZB_ZCL_CMD_LEVEL_CONTROL_MOVE_WITH_ON_OFF, 1, fade_mode_e, uint8_t, uint8_t, uint8_t> move_with_on_off;
    [[no_unique_address]]cmd_pool_t<ZB_ZCL_CMD_LEVEL_CONTROL_STEP_WITH_ON_OFF, 1, fade_mode_e, uint8_t, uint16_t, uint8_t, uint8_t> step_with_on_off;
    [[no_unique_address]]cmd_pool_t<ZB_ZCL_CMD_LEVEL_CONTROL_STOP_WITH_ON_OFF, 1, uint8_t, uint8_t> stop_with_on_off;
    [[no_unique_address]]cmd_pool_t<ZB_ZCL_CMD_LEVEL_CONTROL_MOVE_TO_CLOSEST_FREQUENCY, 1, uint16_t> move_to_closest_freq;
};

    template<> struct zcl_description_t<zb_zcl_level_control_attrs_client_t> {
    static constexpr auto get()
    {
	using T = zb_zcl_level_control_attrs_client_t;
	return cluster_t<
	{.id = ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL, .role = role_t::Client},
	    attributes_t<>{},
	    commands_t<
		 &T::move_to_level
		,&T::move
		,&T::step
		,&T::stop
		,&T::move_to_level_with_on_off
		,&T::move_with_on_off
		,&T::step_with_on_off
		,&T::stop_with_on_off
		,&T::move_to_closest_freq
		>{}
	>{};
    }
    DEFINE_ZBOSS_INIT_GETTER_FOR(ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL);
};


}

#endif
