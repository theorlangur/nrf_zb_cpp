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
    struct set_attr_val_gen_desc_t: ep_cluster_attribute_desc_t
    {
        set_attr_value_handler_t handler;
    };

    template<auto F>
    struct typed_set_attr_value_handler_t
    {
        static constexpr bool value = false;
        static_assert(sizeof(F) == 0, "Not supported signature");
        void handle(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam)
        {
        }
    };

    template<class T> using typed_set_handler_v1_t = void(*)(const T&);
    template<class T> using typed_set_handler_v2_t = void(*)(const T&, zb_zcl_device_callback_param_t *pDevCBParam);
    template<class T> using typed_set_handler_v3_t = void(*)(const T&, zb_zcl_device_callback_param_t *pDevCBParam, zb_zcl_set_attr_value_param_t *p);
    using typed_set_handler_v4_t = void(*)();
    template<class T> using typed_set_handler_v1_t_copy = void(*)(T);
    template<class T> using typed_set_handler_v2_t_copy = void(*)(T, zb_zcl_device_callback_param_t *pDevCBParam);
    template<class T> using typed_set_handler_v3_t_copy = void(*)(T, zb_zcl_device_callback_param_t *pDevCBParam, zb_zcl_set_attr_value_param_t *p);

    template<class T>
    T const& get_typed_data(zb_zcl_set_attr_value_param_t *p)
    {

        //account for zigbee str
        if constexpr (requires { typename T::__1byte_var_len; })
            return *(T*)(p->values.data_variable.p_data);
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
        else
            static_assert(sizeof(T) == 0, "Unsupported type");
    }

    namespace internals
    {
        template<class T, auto F>
        struct set_attr_v1
        {
            using Arg = T;
            static constexpr bool value = true;
            template<std::convertible_to<T> AttrType = T>
            static void handle(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam) 
            { 
                F(get_typed_data<AttrType>(p)); 
            }
        };

        template<class T, auto F>
        struct set_attr_v2
        {
            using Arg = T;
            static constexpr bool value = true;
            template<std::convertible_to<T> AttrType = T>
            static void handle(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam) 
            { 
                F(get_typed_data<AttrType>(p), pDevCBParam); 
            }
        };

        template<class T, auto F>
        struct set_attr_v3
        {
            using Arg = T;
            static constexpr bool value = true;
            template<std::convertible_to<T> AttrType = T>
            static void handle(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam) 
            { 
                F(get_typed_data<AttrType>(p), pDevCBParam, p); 
            }
        };
    }


    template<class T, typed_set_handler_v1_t<T> F>
    struct typed_set_attr_value_handler_t<F>: internals::set_attr_v1<T, F> {};

    template<class T, typed_set_handler_v1_t_copy<T> F>
    struct typed_set_attr_value_handler_t<F>: internals::set_attr_v1<T, F> {};

    template<class T, typed_set_handler_v2_t<T> F>
    struct typed_set_attr_value_handler_t<F>: internals::set_attr_v2<T, F> {};

    template<class T, typed_set_handler_v2_t_copy<T> F>
    struct typed_set_attr_value_handler_t<F>: internals::set_attr_v2<T, F> {};

    template<class T, typed_set_handler_v3_t<T> F>
    struct typed_set_attr_value_handler_t<F>: internals::set_attr_v3<T, F> {};

    template<class T, typed_set_handler_v3_t_copy<T> F>
    struct typed_set_attr_value_handler_t<F>: internals::set_attr_v3<T, F> {};

    template<typed_set_handler_v4_t F>
    struct typed_set_attr_value_handler_t<F>
    {
        using Arg = void;
        static constexpr bool value = true;
        template<class Dummy>
        static void handle(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam) { F(); }
    };

    template<auto F> 
    struct to_handler_t;

    template<auto F> requires typed_set_attr_value_handler_t<F>::value
    struct to_handler_t<F>
    {
        static constexpr set_attr_value_handler_t value = typed_set_attr_value_handler_t<F>::handle;
    };

    template<auto F>
    constexpr static auto to_handler_v = to_handler_t<F>::value;

    template<auto kAttr, auto f>
    constexpr set_attr_val_gen_desc_t handle_set_for(auto &ep)
    {
        using fArg = std::remove_cvref_t<typename typed_set_attr_value_handler_t<f>::Arg>;
        using MemType = std::remove_cvref_t<typename mem_ptr_traits<decltype(kAttr)>::MemberType>;
        if constexpr (std::is_same_v<fArg, MemType>)//same types - simple
            return zb::set_attr_val_gen_desc_t{ep.template attribute_desc<kAttr>(), zb::to_handler_v<f>};
        else
        {
            static_assert(std::is_convertible_v<MemType, fArg>, "Cannot convert attribute member type to set function 1st argument type");
            return zb::set_attr_val_gen_desc_t{ep.template attribute_desc<kAttr>(), typed_set_attr_value_handler_t<f>::template handle<MemType>};
        }
    }

    struct set_attr_val_handling_node_t: GenericNotificationNode<set_attr_val_handling_node_t>
    {
        set_attr_val_gen_desc_t h;
    };

    struct dev_cb_handlers_desc_t
    {
        dev_callback_handler_t default_handler = nullptr;
        err_callback_handler_t error_handler = nullptr;
    };

    struct generic_device_cb_handling_node_t: GenericNotificationNode<generic_device_cb_handling_node_t>
    {
        dev_callback_handler_t h;

        void DoNotify(zb_zcl_device_callback_param_t *pDevParam) { h(pDevParam); }
    };

    template<dev_cb_handlers_desc_t generic={}, set_attr_val_gen_desc_t... handlers>
    void tpl_device_cb(zb_bufid_t bufid)
    {
        zb::buf_view_ptr_t bv{bufid};
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
                                h.handler(pSetVal, pDevParam);
                        };
                        (f(handlers),...);
                        //[[maybe_unused]]zb_ret_t r = 
                        //    ((handlers.fits(pDevParam->endpoint, pSetVal->cluster_id, pSetVal->attr_id) ? handlers.handler(pSetVal, pDevParam), RET_OK : RET_OK), ...);
                    }

                    for(auto *pN : set_attr_val_handling_node_t::g_List)
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

        for(auto *pN : generic_device_cb_handling_node_t::g_List)
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
    struct no_report_attr_handler_desc_t: ep_cluster_attribute_desc_t
    {
        zb_zcl_no_reporting_cb_t handler;
    };

    struct no_report_handling_node_t: GenericNotificationNode<no_report_handling_node_t>
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
        }

        for(auto *pN : no_report_handling_node_t::g_List)
        {
            if (pN->h.fits(ep, cluster_id, attr_id))
                pN->h.handler(ep, cluster_id, attr_id);
        }
    }

    /**********************************************************************/
    /* Attribute report callback                                          */
    /**********************************************************************/
    struct report_attr_handler_desc_t: ep_cluster_attribute_desc_t
    {
        zb_zcl_report_attr_cb_t handler;
    };

    struct report_handling_node_t: GenericNotificationNode<report_handling_node_t>
    {
        report_attr_handler_desc_t h;
    };

    template<class T> requires (std::is_integral_v<T> && std::is_signed_v<T>)
    const T& get_typed_data(zb_uint8_t attr_type, zb_uint8_t *value)
    {
        if constexpr(sizeof(T) == 1)
        {
            switch(zb::type_t(attr_type))
            {
                case zb::type_t::S8:
                case zb::type_t::E8:
                    return *(T*)value;
                default:
                    ZB_ASSERT(false);
                    zb_osif_abort();
                    break;
            }
        }
        else if constexpr(sizeof(T) == 2)
        {
            switch(zb::type_t(attr_type))
            {
                case zb::type_t::S16:
                case zb::type_t::E16:
                    return *(T*)value;
                default:
                    ZB_ASSERT(false);
                    zb_osif_abort();
                    break;
            }
        }
        else if constexpr(sizeof(T) == 4)
        {
            switch(zb::type_t(attr_type))
            {
                case zb::type_t::S32: return *(T*)value;
                default:
                    ZB_ASSERT(false);
                    zb_osif_abort();
                    break;
            }
        }
        else if constexpr(sizeof(T) == 8)
        {
            switch(zb::type_t(attr_type))
            {
                case zb::type_t::S64: return *(T*)value;
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
            switch(zb::type_t(attr_type))
            {
                case zb::type_t::U8:
                case zb::type_t::E8:
                    return *(T*)value;
                default:
                    ZB_ASSERT(false);
                    zb_osif_abort();
                    break;
            }
        }
        else if constexpr(sizeof(T) == 2)
        {
            switch(zb::type_t(attr_type))
            {
                case zb::type_t::U16:
                case zb::type_t::E16:
                    return *(T*)value;
                default:
                    ZB_ASSERT(false);
                    zb_osif_abort();
                    break;
            }
        }
        else if constexpr(sizeof(T) == 4)
        {
            switch(zb::type_t(attr_type))
            {
                case zb::type_t::U32: return *(T*)value;
                default:
                    ZB_ASSERT(false);
                    zb_osif_abort();
                    break;
            }
        }
        else if constexpr(sizeof(T) == 8)
        {
            switch(zb::type_t(attr_type))
            {
                case zb::type_t::U64: return *(T*)value;
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
        }

        for(auto *pN : report_handling_node_t::g_List)
        {
            if (pN->h.fits(ep, cluster_id, attr_id))
                pN->h.handler(addr, ep, cluster_id, attr_id, attr_type, value);
        }
    }
}
#endif
