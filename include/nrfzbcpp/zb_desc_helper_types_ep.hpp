#ifndef ZB_DESC_HELPER_TYPES_EP_HPP_
#define ZB_DESC_HELPER_TYPES_EP_HPP_

#include "lib_ring_buffer.hpp"
#include "zb_desc_helper_types_cluster.hpp"

namespace zb
{
    template<size_t ServerCount, size_t ClientCount>
    struct ZB_PACKED_PRE TSimpleDesc: zb_af_simple_desc_1_1_t
    {
        zb_uint16_t app_cluster_list_ext[(ServerCount + ClientCount) - 2];
    } ZB_PACKED_STRUCT;
    
    template<cluster_info_t ci, auto mem_desc, bool withCheck> requires requires { typename decltype(mem_desc)::MemT; }
    struct AttributeAccess
    {
        using MemT = decltype(mem_desc)::MemT;

        auto operator=(MemT const& v)
        {
            return zb_zcl_set_attr_val(ep.ep_id, ci.id, (zb_uint8_t)ci.role, mem_desc.id, (zb_uint8_t*)&v, withCheck);
        }

        zb_af_endpoint_desc_t &ep;
    };

    struct EPClusterAttributeDesc_t
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

    struct EPBaseInfo
    {
        zb_uint8_t ep;
        zb_uint16_t dev_id;
        zb_uint8_t dev_ver;
        uint8_t cmd_queue_depth = 0;//0 - auto
    };

    using cmd_id_t = uint8_t;
    using cmd_send_status_cb_t = void(*)(cmd_id_t, zb_zcl_command_send_status_t *);
    struct send_cmd_config_t
    {
        cmd_send_status_cb_t cb;
    };

    using send_request_func_t = void (*)(uint16_t argsPoolIdx);
    struct CmdRequest
    {
        cmd_id_t id;
        uint8_t args_idx;
        send_request_func_t send_req;
        cmd_send_status_cb_t cb;
    };

    template<EPBaseInfo i, class Clusters>
    struct EPDesc
    {
        using SimpleDesc = TSimpleDesc<Clusters::server_cluster_count(), Clusters::client_cluster_count()>;
        static constexpr auto kCmdQueueSize = i.cmd_queue_depth ? i.cmd_queue_depth : Clusters::max_command_pool_size();
        static_assert(kCmdQueueSize >= Clusters::max_command_pool_size(), "It's not allowed to set the command queue size lower than max pool size among all commands");

        template<class T1, class T2, class... T> requires std::is_same_v<TClusterList<T1, T2, T...>, Clusters>
        constexpr EPDesc(TClusterList<T1, T2, T...> &clusters):
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

    private:
        template<class _MemType, class _StructType, class _ClusterType>
        struct ClusterTypeInfo
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
            return ClusterTypeInfo<MemPtrType, ClassType, ClusterDescType>{};
        }

        template<auto memPtr, bool checked>
        auto attr_raw()
        {
            constexpr auto types = validate_mem_ptr<memPtr>();
            using ClusterDescType = decltype(types)::ClusterType;
            return AttributeAccess<ClusterDescType::info(), ClusterDescType::template get_member_description<memPtr>(), checked>{ep};
        }

        inline static cmd_id_t g_cmd_num = 0;
        using send_request_func_t = void (*)(uint16_t argsPoolIdx);
        struct CmdRequest
        {
            cmd_id_t id;
            uint8_t args_idx;
            send_request_func_t send_req;
            cmd_send_status_cb_t cb;
        };
        inline static RingBuffer<CmdRequest, kCmdQueueSize> g_CmdQueue;

        static bool send_next_cmd()
        {
            if (auto *pNextCmd = g_CmdQueue.peek())
            {
                pNextCmd->send_req(pNextCmd->args_idx);
                //send next request
                return true;
            }
            return false;
        }

        static void on_send_cmd_cb(zb_uint8_t param)
        {
            auto *pCurrent = g_CmdQueue.peek();
            ZB_ASSERT(pCurrent);
            auto cmd_id = pCurrent->id;
            auto cb = pCurrent->cb;
            g_CmdQueue.drop();
            if (cb)
            {
                zb_zcl_command_send_status_t *cmd_send_status = ZB_BUF_GET_PARAM(param, zb_zcl_command_send_status_t);
                cb(cmd_id, cmd_send_status);
            }
            send_next_cmd();
        }

    public:
        template<auto memPtr>
        auto attr() { return attr_raw<memPtr, false>(); }

        template<auto memPtr>
        auto attr_checked() { return attr_raw<memPtr, true>(); }

        template<auto memPtr, send_cmd_config_t cfg={}, class... Args>
        std::optional<cmd_id_t> send_cmd(Args&&...args)
        {
            constexpr auto types = validate_mem_ptr<memPtr>();
            using ClusterDescType = decltype(types)::ClusterType;
            constexpr auto cmd_desc = ClusterDescType::template get_cmd_description<memPtr>();
            using cmd_desc_t = decltype(cmd_desc);
            auto args_pool_idx = cmd_desc_t::prepare_args(&on_send_cmd_cb, std::forward<Args>(args)...);
            if (!args_pool_idx) return std::nullopt;
            auto r = g_CmdQueue.push(
                    g_cmd_num,
                    *args_pool_idx,
                    &cmd_desc_t::template request<ClusterDescType::info(), {.ep = i.ep}>,
                    cfg.cb
            );
            if (!r) return std::nullopt;
            if (*r == 1) send_next_cmd();
            return g_cmd_num++;
        }

        template<auto memPtr, send_cmd_config_t cfg={}, class... Args>
        auto send_cmd_to(uint16_t short_addr, uint8_t ep, Args&&...args)
        {
            constexpr auto types = validate_mem_ptr<memPtr>();
            using ClusterDescType = decltype(types)::ClusterType;
            constexpr auto cmd_desc = ClusterDescType::template get_cmd_description<memPtr>();
            using cmd_desc_t = decltype(cmd_desc);
            auto args_pool_idx = cmd_desc_t::prepare_args(&on_send_cmd_cb, short_addr, ep, std::forward<Args>(args)...);
            if (!args_pool_idx) return std::nullopt;
            auto r = g_CmdQueue.push(
                    g_cmd_num,
                    *args_pool_idx,
                    &cmd_desc_t::template request<ClusterDescType::info(), {.ep = i.ep}>,
                    cfg.cb
            );
            if (!r) return std::nullopt;
            if (*r == 1) send_next_cmd();
            return g_cmd_num++;
        }

        template<auto memPtr, send_cmd_config_t cfg={}, class... Args>
        auto send_cmd_to(zb_ieee_addr_t long_addr, uint8_t ep, Args&&...args)
        {
            constexpr auto types = validate_mem_ptr<memPtr>();
            using ClusterDescType = decltype(types)::ClusterType;
            constexpr auto cmd_desc = ClusterDescType::template get_cmd_description<memPtr>();
            using cmd_desc_t = decltype(cmd_desc);
            auto args_pool_idx = cmd_desc_t::prepare_args(&on_send_cmd_cb, long_addr, ep, std::forward<Args>(args)...);
            if (!args_pool_idx) return std::nullopt;
            auto r = g_CmdQueue.push(
                    g_cmd_num,
                    *args_pool_idx,
                    &cmd_desc_t::template request<ClusterDescType::info(), {.ep = i.ep}>,
                    cfg.cb
            );
            if (!r) return std::nullopt;
            if (*r == 1) send_next_cmd();
            return g_cmd_num++;
        }

        template<auto memPtr, send_cmd_config_t cfg={}, class... Args>
        auto send_cmd_to_group(uint16_t group, Args&&...args)
        {
            constexpr auto types = validate_mem_ptr<memPtr>();
            using ClusterDescType = decltype(types)::ClusterType;
            constexpr auto cmd_desc = ClusterDescType::template get_cmd_description<memPtr>();
            using cmd_desc_t = decltype(cmd_desc);
            auto args_pool_idx = cmd_desc_t::prepare_args(&on_send_cmd_cb, group, std::forward<Args>(args)...);
            if (!args_pool_idx) return std::nullopt;
            auto r = g_CmdQueue.push(
                    g_cmd_num,
                    *args_pool_idx,
                    &cmd_desc_t::template request<ClusterDescType::info(), {.ep = i.ep}>,
                    cfg.cb
            );
            if (!r) return std::nullopt;
            if (*r == 1) send_next_cmd();
            return g_cmd_num++;
        }

        template<auto memPtr, send_cmd_config_t cfg={}, class... Args>
        auto send_cmd_to_binded(uint8_t bind_table_id, Args&&...args)
        {
            constexpr auto types = validate_mem_ptr<memPtr>();
            using ClusterDescType = decltype(types)::ClusterType;
            constexpr auto cmd_desc = ClusterDescType::template get_cmd_description<memPtr>();
            using cmd_desc_t = decltype(cmd_desc);
            auto args_pool_idx = cmd_desc_t::prepare_args(&on_send_cmd_cb, bind_table_id, std::forward<Args>(args)...);
            if (!args_pool_idx) return std::nullopt;
            auto r = g_CmdQueue.push(
                    g_cmd_num,
                    *args_pool_idx,
                    &cmd_desc_t::template request<ClusterDescType::info(), {.ep = i.ep}>,
                    cfg.cb
            );
            if (!r) return std::nullopt;
            if (*r == 1) send_next_cmd();
            return g_cmd_num++;
        }

        template<auto memPtr>
        constexpr EPClusterAttributeDesc_t handler_filter_for_attribute()
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
        constexpr EPClusterAttributeDesc_t handler_filter_for_cluster()
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
