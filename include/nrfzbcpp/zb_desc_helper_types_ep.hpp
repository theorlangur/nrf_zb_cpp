#ifndef ZB_DESC_HELPER_TYPES_EP_HPP_
#define ZB_DESC_HELPER_TYPES_EP_HPP_

#include "lib_ring_buffer.hpp"
#include "zb_desc_helper_types_cluster.hpp"
#include "nrfzbcpp/zb_alarm.hpp"

namespace zb
{
    template<size_t ServerCount, size_t ClientCount>
    struct ZB_PACKED_PRE simple_desc_t: zb_af_simple_desc_1_1_t
    {
        zb_uint16_t app_cluster_list_ext[(ServerCount + ClientCount) - 2];
    } ZB_PACKED_STRUCT;

    template<size_t ServerCount, size_t ClientCount> requires ((ServerCount + ClientCount) < 2)
    struct ZB_PACKED_PRE simple_desc_t<ServerCount, ClientCount>: zb_af_simple_desc_1_1_t
    {
    } ZB_PACKED_STRUCT;
    
    template<cluster_info_t ci, auto mem_desc, bool withCheck> requires requires { typename decltype(mem_desc)::MemT; }
    struct attribute_access_t
    {
        using MemT = decltype(mem_desc)::MemT;

        auto operator=(MemT const& v)
        {
            return zb_zcl_set_attr_val(ep.ep_id, ci.id, (zb_uint8_t)ci.role, mem_desc.id, (zb_uint8_t*)&v, withCheck);
        }

        zb_af_endpoint_desc_t &ep;
    };

    struct ep_cluster_attribute_desc_t
    {
        static constexpr uint16_t kANY_CLUSTER = 0xffff;
        static constexpr uint16_t kANY_ATTRIBUTE = 0xffff;
        static constexpr uint8_t  kANY_EP = 0xff;

        zb_uint8_t ep = kANY_EP;
        uint16_t cluster = kANY_CLUSTER;
        uint16_t attribute = kANY_ATTRIBUTE;

        constexpr bool fits(zb_uint8_t _ep, uint16_t _cluster, uint16_t _attr) const
        {
            return ((_ep == ep) || (ep == kANY_EP))
                && ((_cluster == cluster) || (cluster == kANY_CLUSTER))
                && ((_attr == attribute) || (attribute == kANY_ATTRIBUTE));
        }
    };

    struct ep_base_info_t
    {
        zb_uint8_t ep;
        zb_uint16_t dev_id;
        zb_uint8_t dev_ver;
        uint8_t cmd_queue_depth = 0;//0 - auto
    };

    using cmd_id_t = uint8_t;
    using cmd_send_status_cb_t = void(*)(cmd_id_t, zb_zcl_command_send_status_t *);
    static const constexpr uint32_t kCmdTimeoutDefault = uint32_t(-1);
    struct send_cmd_config_t
    {
        cmd_send_status_cb_t cb;
        uint32_t timeout_ms = kCmdTimeoutDefault;
    };

    using send_request_func_t = bool (*)(uint16_t argsPoolIdx);
    using cancel_func_t = void (*)(uint16_t argsPoolIdx);
    struct cmd_request_t
    {
        cmd_id_t id;
        uint8_t args_idx;
        send_request_func_t send_req;
        cmd_send_status_cb_t cb;
        uint32_t timeout_ms;
    };

    template<auto memPtr>
    using cluster_description_for_mem_ptr_t = decltype(zcl_description_t<typename mem_ptr_traits<decltype(memPtr)>::ClassType>::get());

    template<auto memPtr>
    using cmd_description_for_mem_ptr_t = decltype(cluster_description_for_mem_ptr_t<memPtr>::template get_cmd_description<memPtr>());

    struct short_addr_t
    {
        using addr_tag = void;
        uint16_t short_addr;
        uint8_t ep;
    };
    struct long_addr_t
    {
        using addr_tag = void;
        long_addr_t(zb_ieee_addr_t a, uint8_t e):
            ep(e)
        {
            memcpy(long_addr, a, sizeof(zb_ieee_addr_t));
        }
        zb_ieee_addr_t long_addr;
        uint8_t ep;
    };
    struct group_addr_t
    {
        using addr_tag = void;
        uint16_t group;
    };
    struct bind_id_addr_t
    {
        using addr_tag = void;
        uint8_t bind_table_id;
    };

    inline short_addr_t to_short(uint16_t _short, uint8_t ep) { return {_short, ep}; }
    inline long_addr_t to_long(zb_ieee_addr_t addr, uint8_t ep) { return {addr, ep}; }
    inline group_addr_t to_group(uint16_t group) { return {group}; }
    inline bind_id_addr_t to_bind_id(uint8_t id) { return {id}; }

    template<class C>
    concept is_zb_addr_type_c = requires{ typename C::addr_tag; };

    template<ep_base_info_t i, class Clusters>
    struct ep_desc_t
    {
        using SimpleDesc = simple_desc_t<Clusters::server_cluster_count(), Clusters::client_cluster_count()>;
        static constexpr auto kCmdQueueSize = i.cmd_queue_depth ? i.cmd_queue_depth : Clusters::max_command_pool_size();
        static constexpr auto kCmdMaxArgsSize = Clusters::max_command_arg_raw_size();
        using PoolType = ObjectPool<request_runtime_args_raw_t<kCmdMaxArgsSize>, kCmdQueueSize>;
        static_assert(kCmdQueueSize >= Clusters::max_command_pool_size(), "It's not allowed to set the command queue size lower than max pool size among all commands");

        template<class T1, class T2, class... T> requires std::is_same_v<cluster_list_t<i.ep, T1, T2, T...>, Clusters>
        constexpr ep_desc_t(cluster_list_t<i.ep, T1, T2, T...> &clusters):
            simple_desc{ 
                {
                    .endpoint = i.ep, 
                    .app_profile_id = ZB_AF_HA_PROFILE_ID, 
                    .app_device_id = i.dev_id,
                    .app_device_version = i.dev_ver,
                    .reserved = 0,
                    .app_input_cluster_count = Clusters::server_cluster_count(),
                    .app_output_cluster_count = Clusters::client_cluster_count(),
                    .app_cluster_list = {T1::info().id, T2::info().id}
                },
                { T::info().id... }//rest
            },
            rep_ctx{},
            cvc_alarm_ctx{},
            ep{
                .ep_id = i.ep,
                .profile_id = ZB_AF_HA_PROFILE_ID,
                .device_handler = nullptr,
                .identify_handler = nullptr,
                .reserved_size = 0,
                .reserved_ptr = nullptr,
                .cluster_count = sizeof...(T) + 2,
                .cluster_desc_list = clusters.clusters,
                .simple_desc = &simple_desc,
                .rep_info_count = Clusters::reporting_attributes_count(),
                .reporting_info = rep_ctx,
                .cvc_alarm_count = Clusters::cvc_attributes_count(),
                .cvc_alarm_info = cvc_alarm_ctx
            }
        {
        }

        template<class T1, class... T> requires (std::is_same_v<cluster_list_t<i.ep, T1, T...>, Clusters> && (Clusters::server_cluster_count() + Clusters::client_cluster_count() < 2))
        constexpr ep_desc_t(cluster_list_t<i.ep, T1, T...> &clusters):
            simple_desc{ 
                {
                    .endpoint = i.ep, 
                    .app_profile_id = ZB_AF_HA_PROFILE_ID, 
                    .app_device_id = i.dev_id,
                    .app_device_version = i.dev_ver,
                    .reserved = 0,
                    .app_input_cluster_count = Clusters::server_cluster_count(),
                    .app_output_cluster_count = Clusters::client_cluster_count(),
                    .app_cluster_list = {T1::info().id}
                }
            },
            rep_ctx{},
            cvc_alarm_ctx{},
            ep{
                .ep_id = i.ep,
                .profile_id = ZB_AF_HA_PROFILE_ID,
                .device_handler = nullptr,
                .identify_handler = nullptr,
                .reserved_size = 0,
                .reserved_ptr = nullptr,
                .cluster_count = sizeof...(T) + 1,
                .cluster_desc_list = clusters.clusters,
                .simple_desc = &simple_desc,
                .rep_info_count = Clusters::reporting_attributes_count(),
                .reporting_info = rep_ctx,
                .cvc_alarm_count = Clusters::cvc_attributes_count(),
                .cvc_alarm_info = cvc_alarm_ctx
            }
        {
        }

    private:
        template<class _MemType, class _StructType, class _ClusterType>
        struct cluster_type_info_t
        {
            using MemType = _MemType;
            using StructType = _StructType;
            using ClusterType = _ClusterType;
        };
        template<auto memPtr>
        static consteval auto validate_mem_ptr()
        {
            using MemPtrType = decltype(memPtr);
            static_assert(mem_ptr_traits<MemPtrType>::is_mem_ptr, "Only member pointer is allowed");

            using ClassType = mem_ptr_traits<MemPtrType>::ClassType;
            //using MemType = mem_ptr_traits<MemPtrType>::MemberType;
            using ClusterDescType = decltype(zcl_description_t<ClassType>::get());
            static_assert(Clusters::has_info(ClusterDescType::info()), "Requested cluster is not part of the EP");
            return cluster_type_info_t<MemPtrType, ClassType, ClusterDescType>{};
        }

        template<auto memPtr, bool checked>
        auto attr_raw()
        {
            constexpr auto types = validate_mem_ptr<memPtr>();
            using ClusterDescType = decltype(types)::ClusterType;
            return attribute_access_t<ClusterDescType::info(), ClusterDescType::template get_member_description<memPtr>(), checked>{ep};
        }

        inline static cmd_id_t g_cmd_num = 0;
        inline static RingBuffer<cmd_request_t, kCmdQueueSize> g_CmdQueue;
        inline static ObjectPool<request_runtime_args_raw_t<kCmdMaxArgsSize>, kCmdQueueSize> g_CmdArgs;
        inline static zb::zb_alarm_ext_16_t g_CmdTimeoutTracker;
        using RequestPtr = PoolType::template Ptr<g_CmdArgs>;

        static void on_send_cmd_timeout(cmd_request_t *pCmdReq)
        {
            auto *pArgs = g_CmdArgs.IdxToPtr(pCmdReq->args_idx);
            if (g_CmdArgs.IsValid(pArgs))
                pArgs->canceled = true;
            auto *pCurrent = g_CmdQueue.peek();
            if (pCurrent == pCmdReq)
                on_send_cmd_cb(0);
            else
            {
                //how is this possible?
            }
        }

        static bool send_next_cmd(bool with_cb = true)
        {
            if (auto *pNextCmd = g_CmdQueue.peek())
            {
                //try and send next request
                if (!pNextCmd->send_req(pNextCmd->args_idx))
                {
                    //couldn't send
                    auto cb = pNextCmd->cb;
                    auto cmd_id = pNextCmd->id;
                    g_CmdQueue.drop();
                    if (cb && with_cb)
                        cb(cmd_id, nullptr);
#if defined(DBG_CMD)
                    else
                        printk("failed to initiate send command request for %d (cb=%p)\r\n", cmd_id, cb);
#endif
                    return false;
                }else if (pNextCmd->timeout_ms)
                    g_CmdTimeoutTracker.Setup([pNextCmd]{on_send_cmd_timeout(pNextCmd);}, pNextCmd->timeout_ms);
                return true;
            }
            return false;
        }

        static void on_send_cmd_cb(zb_uint8_t param)
        {
            g_CmdTimeoutTracker.Cancel();
            auto *pCurrent = g_CmdQueue.peek();
            if (!pCurrent)
            {
#if defined(DBG_CMD)
                printk("on_send_cmd_cb: param=%d; pCurrent=null; queue empty. unexpected\r\n", param);
#endif
                return;
            }
            ZB_ASSERT(pCurrent);
            auto cmd_id = pCurrent->id;
            auto cb = pCurrent->cb;
#if defined(DBG_CMD)
            printk("on_send_cmd_cb: param=%d; id=%d; cb=%p; pCurrent=%p\r\n", param, cmd_id, (void*)cb, pCurrent);
#endif
            g_CmdQueue.drop();
            if (cb)
            {
                zb_zcl_command_send_status_t *cmd_send_status = param ? ZB_BUF_GET_PARAM(param, zb_zcl_command_send_status_t) : nullptr;
                cb(cmd_id, cmd_send_status);
            }

            if (param)
                zb_buf_free(param);
            //if we cannot send commands we'll just drain the queue
            //sad but there's nothing much else we can do
            while(g_CmdQueue.peek() && !send_next_cmd());
        }

        template<auto memPtr, send_cmd_config_t cfg>
        std::optional<cmd_id_t> send_cmd_impl(auto args_pool_idx)
        {
            using ClusterDescType = cluster_description_for_mem_ptr_t<memPtr>;
            using cmd_desc_t = cmd_description_for_mem_ptr_t<memPtr>;

            RequestPtr raii(g_CmdArgs.IdxToPtr(*args_pool_idx));
            constexpr auto kTimeout = cfg.timeout_ms == kCmdTimeoutDefault ? cmd_desc_t::timeout_ms() : cfg.timeout_ms;
            auto r = g_CmdQueue.push(
                    /*struct cmd_request*/
                    /*id*/        g_cmd_num,
                    /*args_idx*/  *args_pool_idx,
                    /*send_req*/  &cmd_desc_t::template request<g_CmdArgs, ClusterDescType::info(), {.ep = i.ep}>,
                    /*cb*/        cfg.cb,
                    /*timeout_ms*/kTimeout
            );
            if (!r) return std::nullopt;
            //if the size of the Queue is 1 it means this command is the only there
            //we need to send it right away
#if defined(DBG_CMD)
            printk("send_cmd1: id=%d; cb=%p\r\n", g_cmd_num, (void*)cfg.cb);
#endif
            if (*r == 1) 
                if (!send_next_cmd(false))
                    return std::nullopt;
            raii.release();
            return g_cmd_num++;
        }

    public:
        template<auto memPtr>
        auto attr() { return attr_raw<memPtr, false>(); }

        template<auto memPtr>
        auto attr_checked() { return attr_raw<memPtr, true>(); }

        template<auto memPtr>
        void dump_mem_info()
        {
            constexpr auto types = validate_mem_ptr<memPtr>();
            using ClusterDescType = decltype(types)::ClusterType;
            constexpr auto cmd_desc = ClusterDescType::template get_cmd_description<memPtr>();
            using cmd_desc_t = decltype(cmd_desc);
            const auto &allocated = cmd_desc_t::g_Pool.AllocatedBitSet();
            printk("Mem info pool idx allocated:");
            for(size_t b = 0; b < allocated.BitCount; ++b)
            {
                if (allocated.test(b))
                    printk(" %d;", b);
            }
            printk("\r\n");
        }

        template<auto... memPtr>
        void dump_info()
        {
            printk("g_cmd_num=%d; queue size=%d\r\n", g_cmd_num, g_CmdQueue.size());
            for(cmd_request_t *pCmdReq : g_CmdQueue)
                printk("cmd: id=%d; arg idx=%d\r\n", pCmdReq->id, pCmdReq->args_idx);

            if constexpr(sizeof...(memPtr) > 0)
                (dump_mem_info<memPtr>(),...);
        }

        template<auto memPtr, send_cmd_config_t cfg={}, class... Args> requires (!is_zb_addr_type_c<Args> && ...)
        [[nodiscard]] std::optional<cmd_id_t> send_cmd(Args&&...args)
        {
            using cmd_desc_t = cmd_description_for_mem_ptr_t<memPtr>;
            return send_cmd_impl<memPtr, cfg>(cmd_desc_t::cmd_prepare_t::template prepare_t<g_CmdArgs>::prepare_args(&on_send_cmd_cb, std::forward<Args>(args)...));
        }

        template<auto memPtr, send_cmd_config_t cfg={}, class... Args>
        [[nodiscard]] std::optional<cmd_id_t> send_cmd(short_addr_t addr, Args&&...args)
        {
            using cmd_desc_t = cmd_description_for_mem_ptr_t<memPtr>;
            return send_cmd_impl<memPtr, cfg>(cmd_desc_t::cmd_prepare_t::template prepare_t<g_CmdArgs>::prepare_args(&on_send_cmd_cb, addr.short_addr, addr.ep, std::forward<Args>(args)...));
        }

        template<auto memPtr, send_cmd_config_t cfg={}, class... Args>
        [[nodiscard]] std::optional<cmd_id_t> send_cmd(long_addr_t a, Args&&...args)
        {
            using cmd_desc_t = cmd_description_for_mem_ptr_t<memPtr>;
            return send_cmd_impl<memPtr, cfg>(cmd_desc_t::cmd_prepare_t::template prepare_t<g_CmdArgs>::prepare_args(&on_send_cmd_cb, a.long_addr, a.ep, std::forward<Args>(args)...));
        }

        template<auto memPtr, send_cmd_config_t cfg={}, class... Args>
        [[nodiscard]] std::optional<cmd_id_t> send_cmd(group_addr_t a, Args&&...args)
        {
            using cmd_desc_t = cmd_description_for_mem_ptr_t<memPtr>;
            return send_cmd_impl<memPtr, cfg>(cmd_desc_t::cmd_prepare_t::template prepare_t<g_CmdArgs>::prepare_args(&on_send_cmd_cb, a.group, std::forward<Args>(args)...));
        }

        template<auto memPtr, send_cmd_config_t cfg={}, class... Args>
        [[nodiscard]] std::optional<cmd_id_t> send_cmd(bind_id_addr_t a, Args&&...args)
        {
            using cmd_desc_t = cmd_description_for_mem_ptr_t<memPtr>;
            return send_cmd_impl<memPtr, cfg>(cmd_desc_t::cmd_prepare_t::template prepare_t<g_CmdArgs>::prepare_args(&on_send_cmd_cb, a.bind_table_id, std::forward<Args>(args)...));
        }

        template<auto memPtr>
        constexpr ep_cluster_attribute_desc_t attribute_desc()
        {
            using MemPtrType = decltype(memPtr);
            static_assert(mem_ptr_traits<MemPtrType>::is_mem_ptr, "Only member pointer is allowed");

            using ClassType = mem_ptr_traits<MemPtrType>::ClassType;
            //using MemType = mem_ptr_traits<MemPtrType>::MemberType;
            using ClusterDescType = decltype(zcl_description_t<ClassType>::get());
            static_assert(Clusters::has_info(ClusterDescType::info()), "Requested cluster is not part of the EP");
            return {.ep = i.ep, .cluster = ClusterDescType::info().id, .attribute = ClusterDescType::template get_member_description<memPtr>().id};
        }

        template<class Cluster>
        constexpr ep_cluster_attribute_desc_t cluster_desc()
        {
            using ClusterDescType = decltype(zcl_description_t<Cluster>::get());
            static_assert(Clusters::has_info(ClusterDescType::info()), "Requested cluster is not part of the EP");
            return {.ep = i.ep, .cluster = ClusterDescType::info().id};
        }

        alignas(4) SimpleDesc simple_desc;
        alignas(4) zb_zcl_reporting_info_t rep_ctx[Clusters::reporting_attributes_count()];
        alignas(4) zb_zcl_cvc_alarm_variables_t cvc_alarm_ctx[Clusters::cvc_attributes_count()];
        alignas(4) zb_af_endpoint_desc_t ep;
    };
}
#endif
