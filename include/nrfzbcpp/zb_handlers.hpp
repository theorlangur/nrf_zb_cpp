#ifndef ZB_HANDLERS_HPP_
#define ZB_HANDLERS_HPP_

#include "zb_buf.hpp"
#include "zb_desc_helper_types_ep.hpp"
#include "lib_notification_node.hpp"
#include "zb_str.hpp"

namespace zb
{
    /**********************************************************************/
    /* Set Attribute value callback                                       */
    /**********************************************************************/
    using dev_callback_handler_t = void(*)(zb_zcl_device_callback_param_t *);
    using err_callback_handler_t = void(*)(int err);
    using set_attr_value_handler_t = void (*)(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam);
    struct update_settings_tag_t{};
    using update_settings_handler_t = void (*)(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam, update_settings_tag_t t);
    struct set_attr_val_gen_desc_t: EPClusterAttributeDesc_t
    {
        set_attr_value_handler_t handler;
        update_settings_handler_t settings;
    };

    template<auto F>
    struct typed_set_attr_value_handler
    {
        static constexpr bool value = false;
        static_assert(sizeof(F) == 0, "Not supported signature");
        void handle(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam)
        {
        }
    };

    template<class T> using TypedSetHandlerV1 = void(*)(const T&);
    template<class T> using TypedSetHandlerV2 = void(*)(const T&, zb_zcl_device_callback_param_t *pDevCBParam);
    template<class T> using TypedSetHandlerV3 = void(*)(const T&, zb_zcl_device_callback_param_t *pDevCBParam, zb_zcl_set_attr_value_param_t *p);
    using TypedSetHandlerV4 = void(*)();
    template<class T> using TypedSetHandlerV1_Copy = void(*)(T);
    template<class T> using TypedSetHandlerV2_Copy = void(*)(T, zb_zcl_device_callback_param_t *pDevCBParam);
    template<class T> using TypedSetHandlerV3_Copy = void(*)(T, zb_zcl_device_callback_param_t *pDevCBParam, zb_zcl_set_attr_value_param_t *p);

    template<class T>
    T const& get_typed_data(zb_zcl_set_attr_value_param_t *p)
    {
        //account for zigbee str
        if constexpr (std::is_same_v<T, ZigbeeStrRef>)
            return *(T*)p->values.data_variable;
        else if constexpr (sizeof(T) == 1)
            return *(T*)&p->values.data8;
        else if constexpr (sizeof(T) == 2)
            return *(T*)&p->values.data16;
        else if constexpr (sizeof(T) == 3)
            return *(T*)&p->values.data24;
        else if constexpr (sizeof(T) == 4)
            return *(T*)&p->values.data32;
        else if constexpr (sizeof(T) == 6)
            return *(T*)&p->values.data48;
        else if constexpr (sizeof(T) == 8)
            return *(T*)&p->values.data_ieee;
    }

    template<class T, TypedSetHandlerV1<T> F>
    struct typed_set_attr_value_handler<F>
    {
        static constexpr bool value = true;
        static void handle(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam) { F(get_typed_data<T>(p)); }
    };

    template<class T, TypedSetHandlerV1_Copy<T> F>
    struct typed_set_attr_value_handler<F>
    {
        static constexpr bool value = true;
        static void handle(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam) { F(get_typed_data<T>(p)); }
    };

    template<class T, TypedSetHandlerV2<T> F>
    struct typed_set_attr_value_handler<F>
    {
        static constexpr bool value = true;
        static void handle(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam) { F(get_typed_data<T>(p), pDevCBParam); }
    };

    template<class T, TypedSetHandlerV2_Copy<T> F>
    struct typed_set_attr_value_handler<F>
    {
        static constexpr bool value = true;
        static void handle(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam) { F(get_typed_data<T>(p), pDevCBParam); }
    };

    template<class T, TypedSetHandlerV3<T> F>
    struct typed_set_attr_value_handler<F>
    {
        static constexpr bool value = true;
        static void handle(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam) { F(get_typed_data<T>(p), pDevCBParam, p); }
    };

    template<class T, TypedSetHandlerV3_Copy<T> F>
    struct typed_set_attr_value_handler<F>
    {
        static constexpr bool value = true;
        static void handle(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam) { F(get_typed_data<T>(p), pDevCBParam, p); }
    };

    template<TypedSetHandlerV4 F>
    struct typed_set_attr_value_handler<F>
    {
        static constexpr bool value = true;
        static void handle(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam) { F(); }
    };

    template<auto F> 
    struct to_handler_t;

    template<auto F> requires typed_set_attr_value_handler<F>::value
    struct to_handler_t<F>
    {
        static constexpr set_attr_value_handler_t value = typed_set_attr_value_handler<F>::handle;
    };

    template<auto F>
    constexpr static auto to_handler_v = to_handler_t<F>::value;

    template<const char *settings_entry_name, const auto &Mem>
    struct persistent_settings_t
    {
        static void handle(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam, update_settings_tag_t t)
        {
            settings_save_one(settings_entry_name, &Mem, sizeof(std::remove_cvref_t<decltype(Mem)>));
        }
    };

    template<const char *settings_entry_name, const auto &Mem>
    constexpr static update_settings_handler_t settings_v = &persistent_settings_t<settings_entry_name, Mem>::handle;

    struct SetAttrValHandlingNode: GenericNotificationNode<SetAttrValHandlingNode>
    {
        set_attr_val_gen_desc_t h;
    };

    struct dev_cb_handlers_desc
    {
        dev_callback_handler_t default_handler = nullptr;
        err_callback_handler_t error_handler = nullptr;
    };

    struct GenericDeviceCBHandlingNode: GenericNotificationNode<GenericDeviceCBHandlingNode>
    {
        dev_callback_handler_t h;

        void DoNotify(zb_zcl_device_callback_param_t *pDevParam) { h(pDevParam); }
    };

    template<dev_cb_handlers_desc generic={}, set_attr_val_gen_desc_t... handlers>
    void tpl_device_cb(zb_bufid_t bufid)
    {
        zb::BufViewPtr bv{bufid};
        auto *pDevParam = bv.param<zb_zcl_device_callback_param_t>();
        if (!pDevParam)
        {
            if (generic.error_handler)
                generic.error_handler(-1);
            return;
        }
        pDevParam->status = RET_OK;
        switch(pDevParam->device_cb_id)
        {
            case ZB_ZCL_SET_ATTR_VALUE_CB_ID:
                {
                    auto *pSetVal = &pDevParam->cb_param.set_attr_value_param;
                    if constexpr (sizeof...(handlers))
                    {
                        static_assert(((handlers.handler != nullptr) && ...), "Invalid handler detected");
                        auto f = [&](const zb::set_attr_val_gen_desc_t &h)
                        {
                            if (h.fits(pDevParam->endpoint, pSetVal->cluster_id, pSetVal->attr_id))
                            {
                                h.handler(pSetVal, pDevParam);
                                if (h.settings)
                                    h.settings(pSetVal, pDevParam, update_settings_tag_t{});
                            }
                        };
                        (f(handlers),...);
                        //[[maybe_unused]]zb_ret_t r = 
                        //    ((handlers.fits(pDevParam->endpoint, pSetVal->cluster_id, pSetVal->attr_id) ? handlers.handler(pSetVal, pDevParam), RET_OK : RET_OK), ...);
                    }

                    for(auto *pN : SetAttrValHandlingNode::g_List)
                    {
                        if (!pN)
                        {
                            if (generic.error_handler)
                                generic.error_handler(-2);
                            break;
                        }
                        if (pN->h.fits(pDevParam->endpoint, pSetVal->cluster_id, pSetVal->attr_id))
                            pN->h.handler(pSetVal, pDevParam);
                    }
                }
                break;
            default:
                break;
        }

        if constexpr (generic.default_handler)
            generic.default_handler(pDevParam);

        for(auto *pN : GenericDeviceCBHandlingNode::g_List)
        {
            if (!pN)
            {
                if (generic.error_handler)
                    generic.error_handler(-3);
                break;
            }
            pN->h(pDevParam);
        }
    }

    /**********************************************************************/
    /* NoReport callback                                                  */
    /**********************************************************************/
    struct no_report_attr_handler_desc_t: EPClusterAttributeDesc_t
    {
        zb_zcl_no_reporting_cb_t handler;
    };

    struct NoReportHandlingNode: GenericNotificationNode<NoReportHandlingNode>
    {
        no_report_attr_handler_desc_t h;
    };

    template<no_report_attr_handler_desc_t... handlers>
    void tpl_no_report_cb(zb_uint8_t ep, zb_uint16_t cluster_id, zb_uint16_t attr_id)
    {
        if constexpr (sizeof...(handlers))
        {
            static_assert(((handlers.handler != nullptr) && ...), "Invalid handler detected");
            [[maybe_unused]]bool r = 
                ((handlers.fits(ep, cluster_id, attr_id) ? handlers.handler(ep, cluster_id, attr_id), true : true), ...);

            for(auto *pN : NoReportHandlingNode::g_List)
            {
                if (pN->h.fits(ep, cluster_id, attr_id))
                    pN->h.handler(ep, cluster_id, attr_id);
            }
        }
    }

    /**********************************************************************/
    /* Attribute report callback                                          */
    /**********************************************************************/
    struct report_attr_handler_desc_t: EPClusterAttributeDesc_t
    {
        zb_zcl_report_attr_cb_t handler;
    };

    struct ReportHandlingNode: GenericNotificationNode<ReportHandlingNode>
    {
        report_attr_handler_desc_t h;
    };

    template<class T> requires (std::is_integral_v<T> && std::is_signed_v<T>)
    const T& get_typed_data(zb_uint8_t attr_type, zb_uint8_t *value)
    {
        if constexpr(sizeof(T) == 1)
        {
            switch(zb::Type(attr_type))
            {
                case zb::Type::S8:
                case zb::Type::E8:
                    return *(T*)value;
                default:
                    ZB_ASSERT(false);
                    zb_osif_abort();
                    break;
            }
        }
        else if constexpr(sizeof(T) == 2)
        {
            switch(zb::Type(attr_type))
            {
                case zb::Type::S16:
                case zb::Type::E16:
                    return *(T*)value;
                default:
                    ZB_ASSERT(false);
                    zb_osif_abort();
                    break;
            }
        }
        else if constexpr(sizeof(T) == 4)
        {
            switch(zb::Type(attr_type))
            {
                case zb::Type::S32: return *(T*)value;
                default:
                    ZB_ASSERT(false);
                    zb_osif_abort();
                    break;
            }
        }
        else if constexpr(sizeof(T) == 8)
        {
            switch(zb::Type(attr_type))
            {
                case zb::Type::S64: return *(T*)value;
                default:
                    ZB_ASSERT(false);
                    zb_osif_abort();
                    break;
            }
        }else
        {
            ZB_ASSERT(false);
            zb_osif_abort();
        }
    }

    template<class T> requires (std::is_integral_v<T> && std::is_unsigned_v<T>)
    const T& get_typed_data(zb_uint8_t attr_type, zb_uint8_t *value)
    {
        if constexpr(sizeof(T) == 1)
        {
            switch(zb::Type(attr_type))
            {
                case zb::Type::U8:
                case zb::Type::E8:
                    return *(T*)value;
                default:
                    ZB_ASSERT(false);
                    zb_osif_abort();
                    break;
            }
        }
        else if constexpr(sizeof(T) == 2)
        {
            switch(zb::Type(attr_type))
            {
                case zb::Type::U16:
                case zb::Type::E16:
                    return *(T*)value;
                default:
                    ZB_ASSERT(false);
                    zb_osif_abort();
                    break;
            }
        }
        else if constexpr(sizeof(T) == 4)
        {
            switch(zb::Type(attr_type))
            {
                case zb::Type::U32: return *(T*)value;
                default:
                    ZB_ASSERT(false);
                    zb_osif_abort();
                    break;
            }
        }
        else if constexpr(sizeof(T) == 8)
        {
            switch(zb::Type(attr_type))
            {
                case zb::Type::U64: return *(T*)value;
                default:
                    ZB_ASSERT(false);
                    zb_osif_abort();
                    break;
            }
        }else
        {
            ZB_ASSERT(false);
            zb_osif_abort();
        }
    }

    template<report_attr_handler_desc_t... handlers>
    void tpl_report_cb(zb_zcl_addr_t *addr, zb_uint8_t ep, zb_uint16_t cluster_id, zb_uint16_t attr_id, zb_uint8_t attr_type, zb_uint8_t *value)
    {
        if constexpr (sizeof...(handlers))
        {
            static_assert(((handlers.handler != nullptr) && ...), "Invalid handler detected");
            [[maybe_unused]]bool r = 
                ((handlers.fits(ep, cluster_id, attr_id) ? handlers.handler(addr, ep, cluster_id, attr_id, attr_type, value), true : true), ...);

            for(auto *pN : ReportHandlingNode::g_List)
            {
                if (pN->h.fits(ep, cluster_id, attr_id))
                    pN->h.handler(addr, ep, cluster_id, attr_id, attr_type, value);
            }
        }
    }
}
#endif
