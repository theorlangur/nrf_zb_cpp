#ifndef ZB_OCCUPANCY_SENSING_CLUSTER_DESC_HPP_
#define ZB_OCCUPANCY_SENSING_CLUSTER_DESC_HPP_

#include "zb_main.hpp"

extern "C"
{
#include <zboss_api_addons.h>
#include <zb_mem_config_med.h>
#include <zb_nrf_platform.h>
}

namespace zb
{
    static constexpr uint16_t kZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING = 0x0406;


    /**********************************************************************/
    /* Basic attributes                                                   */
    /**********************************************************************/
    struct zb_zcl_occupancy_t
    {
        enum Type: uint8_t { PIR = 0, Ultrasonic = 1, PIRandUltrasonic = 2, PhysicalContact = 3 };

        uint8_t occupancy = 0;
        Type    sensor_type = {};
        union
        {
            uint8_t type_bitmap_raw = 0;
            struct{
                uint8_t PIR             : 1;
                uint8_t Ultrasonic      : 1;
                uint8_t PhysicalContact : 1;
                uint8_t Unused          : 5;
            }type_bitmap;
        };
    };

    /**********************************************************************/
    /* Configuration blocks                                               */
    /**********************************************************************/
    struct zb_zcl_cfg_pir_t
    {
        uint16_t PIROccupiedToUnoccupiedDelay = 0;
        uint16_t PIRUnoccupiedToOccupiedDelay = 0;
        uint8_t PIRUnoccupiedToOccupiedThreshold = 1;
    };

    struct zb_zcl_cfg_ultrasonic_t
    {
        uint16_t UltrasonicOccupiedToUnoccupiedDelay = 0;
        uint16_t UltrasonicUnoccupiedToOccupiedDelay = 0;
        uint8_t  UltrasonicUnoccupiedToOccupiedThreshold = 1;
    };

    struct zb_zcl_cfg_physical_contact_t
    {
        uint16_t PhysicalContactOccupiedToUnoccupiedDelay = 0;
        uint16_t PhysicalContactUnoccupiedToOccupiedDelay = 0;
        uint8_t  PhysicalContactUnoccupiedToOccupiedThreshold = 1;
    };

    namespace occupancy_details{
        template<class T>
        concept ValidOccupancyCfg = std::is_same_v<T, zb_zcl_cfg_pir_t> 
                                 || std::is_same_v<T, zb_zcl_cfg_ultrasonic_t> 
                                 || std::is_same_v<T, zb_zcl_cfg_physical_contact_t>;

        template<class CfgPrime>
        constexpr bool IsAllUniqueHelper(){ return true; }

        template<class CfgPrime, class CfgSecondary>
        constexpr bool IsAllUniqueHelper(){ return !std::is_same_v<CfgPrime, CfgSecondary>; }

        template<class CfgPrime, class CfgSecondary, class... Cfg>
        constexpr bool IsAllUniqueHelper(){ return !std::is_same_v<CfgPrime, CfgSecondary> && IsAllUniqueHelper<CfgPrime, Cfg...>(); }

        template<class CfgPrime, class Cfg>
        constexpr bool IsAllUniquePrimaryHelper(){ return IsAllUniqueHelper<CfgPrime, Cfg>(); }

        template<class CfgPrime, class... Cfg>
        constexpr bool IsAllUniquePrimaryHelper()
        { 
            if constexpr(sizeof...(Cfg) > 1)
                return IsAllUniqueHelper<CfgPrime, Cfg...>() && IsAllUniquePrimaryHelper<Cfg...>(); 
            else
                return IsAllUniqueHelper<CfgPrime, Cfg...>();
        }

        template<class... Cfg>
        constexpr bool kAllUnique = IsAllUniquePrimaryHelper<Cfg...>();

        template<ValidOccupancyCfg Cfg>
        constexpr uint8_t get_occupancy_type_bitmask()
        {
            if constexpr (std::is_same_v<Cfg, zb_zcl_cfg_pir_t>)
                return 0b001;
            else if constexpr (std::is_same_v<Cfg, zb_zcl_cfg_ultrasonic_t>)
                return 0b010;
            else if constexpr (std::is_same_v<Cfg, zb_zcl_cfg_physical_contact_t>)
                return 0b100;
        }

        constexpr zb_zcl_occupancy_t::Type get_occupancy_bitmask_to_type(uint8_t bmp)
        {
            switch(bmp)
            {
                case 0b001://PIR
                    return zb_zcl_occupancy_t::Type::PIR;
                case 0b010://Ultrasonic
                    return zb_zcl_occupancy_t::Type::Ultrasonic;
                case 0b011://PIR + Ultrasonic
                    return zb_zcl_occupancy_t::Type::PIRandUltrasonic;
                case 0b100://PhysicalContact
                    return zb_zcl_occupancy_t::Type::PhysicalContact;
                case 0b101://PIR + PhysicalContact
                    return zb_zcl_occupancy_t::Type::PIR;
                case 0b110://PIR + Ultrasonic
                    return zb_zcl_occupancy_t::Type::Ultrasonic;
                case 0b111://PIR + Ultrasonic + PhysicalContact
                    return zb_zcl_occupancy_t::Type::PIRandUltrasonic;
                default:
                    return zb_zcl_occupancy_t::Type::PIR;
            }
        }
    };

    /**********************************************************************/
    /* Generic template to configure occupancy attributes                 */
    /**********************************************************************/
    template<class... Cfg> requires (
            (sizeof...(Cfg) <= 3)                                  //max 3 types of sensors types supported
            && (sizeof...(Cfg) >= 1)                               //makes sense to use this template only if you need at least 1 configuration
            && occupancy_details::kAllUnique<Cfg...>               //all configuration should be unique
            && (occupancy_details::ValidOccupancyCfg<Cfg> && ...)) //may be only of 3 types: PIR, Ultrasonic, PhysicalContact
    struct zb_zcl_occupancy_tpl_t: zb_zcl_occupancy_t, Cfg...
    {
        static constexpr uint8_t kTypeBitMask = (occupancy_details::get_occupancy_type_bitmask<Cfg>() | ...);
        constexpr zb_zcl_occupancy_tpl_t():
            //auto-initializing the correct type of the sensor by default based on the configuration combination used
            zb_zcl_occupancy_t{.sensor_type = occupancy_details::get_occupancy_bitmask_to_type(kTypeBitMask), .type_bitmap_raw = kTypeBitMask}
        {}
    };

    /**********************************************************************/
    /* Several predefined types                                           */
    /**********************************************************************/
    using zb_zcl_occupancy_pir_t = zb_zcl_occupancy_tpl_t<zb_zcl_cfg_pir_t>;
    using zb_zcl_occupancy_ultrasonic_t = zb_zcl_occupancy_tpl_t<zb_zcl_cfg_ultrasonic_t>;
    using zb_zcl_occupancy_physical_contact_t = zb_zcl_occupancy_tpl_t<zb_zcl_cfg_physical_contact_t>;

    /**********************************************************************/
    /* Template magic pieces                                              */
    /**********************************************************************/
    template<> struct zcl_description_t<zb_zcl_occupancy_t> {
        static constexpr auto get()
        {
            using T = zb_zcl_occupancy_t;
            return cluster_t<
                cluster_info_t{.id = kZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING},
                attributes_t<
                    attribute_t{.m = &T::occupancy,        .id = 0x0000, .a=Access::RP, .type=Type::Map8},
                    attribute_t{.m = &T::sensor_type,      .id = 0x0001, .a=Access::Read},
                    attribute_t{.m = &T::type_bitmap_raw,  .id = 0x0002, .a=Access::Read, .type=Type::Map8}
                >{}
            >{};
        }
    };

    template<> struct zcl_description_t<zb_zcl_cfg_pir_t> {
        static constexpr auto get()
        {
            using T = zb_zcl_cfg_pir_t;
            return cluster_t<
                cluster_info_t{.id = kZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING},
                attributes_t<
                    attribute_t{.m = &T::PIROccupiedToUnoccupiedDelay,      .id = 0x0010, .a=Access::RW},
                    attribute_t{.m = &T::PIRUnoccupiedToOccupiedDelay,      .id = 0x0011, .a=Access::RW},
                    attribute_t{.m = &T::PIRUnoccupiedToOccupiedThreshold,  .id = 0x0012, .a=Access::RW}
                >{}
            >{};
        }
    };

    template<> struct zcl_description_t<zb_zcl_cfg_ultrasonic_t> {
        static constexpr auto get()
        {
            using T = zb_zcl_cfg_ultrasonic_t;
            return cluster_t<
                cluster_info_t{.id = kZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING},
                attributes_t<
                    attribute_t{.m = &T::UltrasonicOccupiedToUnoccupiedDelay,      .id = 0x0020, .a=Access::RW},
                    attribute_t{.m = &T::UltrasonicUnoccupiedToOccupiedDelay,      .id = 0x0021, .a=Access::RW},
                    attribute_t{.m = &T::UltrasonicUnoccupiedToOccupiedThreshold,  .id = 0x0022, .a=Access::RW}
                >{}
            >{};
        }
    };

    template<> struct zcl_description_t<zb_zcl_cfg_physical_contact_t> {
        static constexpr auto get()
        {
            using T = zb_zcl_cfg_physical_contact_t;
            return cluster_t<
                cluster_info_t{.id = kZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING},
                attributes_t<
                    attribute_t{.m = &T::PhysicalContactOccupiedToUnoccupiedDelay,      .id = 0x0030, .a=Access::RW},
                    attribute_t{.m = &T::PhysicalContactUnoccupiedToOccupiedDelay,      .id = 0x0031, .a=Access::RW},
                    attribute_t{.m = &T::PhysicalContactUnoccupiedToOccupiedThreshold,  .id = 0x0032, .a=Access::RW}
                >{}
            >{};
        }
    };

    template<class... Cfg> struct zcl_description_t<zb_zcl_occupancy_tpl_t<Cfg...>> {
        static constexpr auto get()
        {
            return zcl_description_t<zb_zcl_occupancy_t>::get() + (zcl_description_t<Cfg>::get() + ...);
        }
    };
}
#endif
