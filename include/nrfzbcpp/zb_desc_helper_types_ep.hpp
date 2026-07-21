#ifndef ZB_DESC_HELPER_TYPES_EP_HPP_
#define ZB_DESC_HELPER_TYPES_EP_HPP_

#include "lib_ring_buffer.hpp"
#include "nrfzbcpp/zb_buf.hpp"
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
        uint8_t cmd_queue_depth = 2;//0 - auto
    };

    using cmd_id_t = uint8_t;
    using cmd_send_status_cb_t = void(*)(cmd_id_t, zb_zcl_command_send_status_t *);
    static const constexpr uint32_t kCmdTimeoutDefault = uint32_t(-1);
    struct send_cmd_config_t
    {
        cmd_send_status_cb_t cb;
        uint32_t timeout_ms = kCmdTimeoutDefault;
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
        static constexpr auto kCmdQueueSize = i.cmd_queue_depth;
        static constexpr auto kCmdMaxArgsSize = Clusters::max_command_arg_raw_size();
        static constexpr size_t kMaxAllowedArgumentSize = 100;
        static_assert(kCmdMaxArgsSize <= kMaxAllowedArgumentSize, "Too much data for command arguments");

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

        struct issued_cmd_t
        {
            cmd_send_status_cb_t cb = nullptr;
            zb_bufid_t buf = ZB_BUF_INVALID;
            cmd_id_t cmd_id;
        };

        inline static cmd_id_t g_cmd_num = 0;
        inline static pre_alloc_zb_bufs_out<kCmdQueueSize> g_PreAllocBufs;
        inline static std::array<issued_cmd_t, kCmdQueueSize> g_IssuedCmds;

        static issued_cmd_t* find_free_issued_cmd_entry()
        {
            for(auto &cmd : g_IssuedCmds)
            {
                if (cmd.buf == ZB_BUF_INVALID)
                    return &cmd;
            }
            return nullptr;
        }

        static void on_send_cmd_timeout2(zb_bufid_t buf)
        {
            for(auto &cmd : g_IssuedCmds)
            {
                if (cmd.buf == buf)
                {
                    g_PreAllocBufs.deallocate(cmd.buf);
                    cmd.buf = ZB_BUF_INVALID;
                    cmd.cb(cmd.cmd_id, nullptr);
                    return;
                }
            }
            ZB_ASSERT(false);
        }

        static void on_send_cmd_cb2(zb_uint8_t buf)
        {
            for(auto &cmd : g_IssuedCmds)
            {
                if (cmd.buf == buf)
                {
                    zb_schedule_alarm_cancel(on_send_cmd_cb2, buf, nullptr);
                    zb_zcl_command_send_status_t *cmd_send_status = buf ? ZB_BUF_GET_PARAM(buf, zb_zcl_command_send_status_t) : nullptr;
                    g_PreAllocBufs.deallocate(cmd.buf);//cmd_send_status memory is still valid
                    cmd.buf = ZB_BUF_INVALID;
                    cmd.cb(cmd.cmd_id, cmd_send_status);
                    return;
                }
            }
        }

        template<auto memPtr, send_cmd_config_t cfg={}, class... Args> requires (!is_zb_addr_type_c<Args> && ...)
        [[nodiscard]] std::optional<cmd_id_t> send_cmd_impl(zb_addr_u addr, addr_mode_t mode, uint8_t dst_ep, Args&&...args)
        {
            zb_bufid_t b = g_PreAllocBufs.allocate();
            if (b == ZB_BUF_INVALID)
                return std::nullopt;
            issued_cmd_t *pIssued = find_free_issued_cmd_entry();
            ZB_ASSERT(pIssued);//size of issued array and pre-allocated are the same
            //so it must be valid
            using cmd_desc_t = cmd_description_for_mem_ptr_t<memPtr>;
            using ClusterDescType = cluster_description_for_mem_ptr_t<memPtr>;
            constexpr auto kTimeout = cfg.timeout_ms == kCmdTimeoutDefault ? cmd_desc_t::timeout_ms() : cfg.timeout_ms;
            constexpr auto ci = ClusterDescType::info();
            constexpr uint16_t manu_code = ci.manuf_code != ZB_ZCL_MANUF_CODE_INVALID ? ci.manuf_code : cmd_desc_t::manufacturer();
            frame_ctl_t f{.f{
                .cluster_specific = true, 
                    .manufacture_specific = manu_code != ZB_ZCL_MANUF_CODE_INVALID
                        , .direction = ci.role == role_t::Client ? frame_direction_t::ToServer : frame_direction_t::ToClient
                        , .disable_default_response = false
            }};
            ZB_ZCL_GET_SEQ_NUM();
            uint8_t* ptr = (uint8_t*)zb_zcl_start_command_header(b, f.u8, manu_code, cmd_desc_t::kCmdId, nullptr);
            ptr = cmd_desc_t::cmd_prepare_t::store_to(ptr, kMaxAllowedArgumentSize, std::forward<Args>(args)...);
            zb_ret_t ret = zb_zcl_finish_and_send_packet(b, ptr, &addr, (uint8_t)mode/*addr mode*/, dst_ep, i.ep, ZB_AF_HA_PROFILE_ID, ci.id, on_send_cmd_cb2);
            if (RET_OK != ret)
            {
                g_PreAllocBufs.deallocate(b);
                return std::nullopt;
            }

            pIssued->buf = b;
            pIssued->cb = cfg.cb;
            pIssued->cmd_id = g_cmd_num;
            if (zb_schedule_app_alarm(on_send_cmd_timeout2, b, ZB_MILLISECONDS_TO_BEACON_INTERVAL(kTimeout)) != RET_OK)
            {
                pIssued->buf = ZB_BUF_INVALID;
                g_PreAllocBufs.deallocate(b);
                return std::nullopt;
            }

            return g_cmd_num++;
        }

    public:
        void init()
        {
            g_PreAllocBufs.init();
        }

        template<auto memPtr>
        auto attr() { return attr_raw<memPtr, false>(); }

        template<auto memPtr>
        auto attr_checked() { return attr_raw<memPtr, true>(); }

        void dump_info()
        {
            printk("g_cmd_num=%d;\r\n", g_cmd_num);
            for(auto &cmd : g_IssuedCmds)
                printk("cmd: id=%d; buf idx=%d\r\n", cmd.cmd_id, cmd.buf);
        }

        template<auto memPtr, send_cmd_config_t cfg={}, class... Args> requires (!is_zb_addr_type_c<Args> && ...)
        [[nodiscard]] std::optional<cmd_id_t> send_cmd(Args&&...args)
        {
            return send_cmd_impl<memPtr, cfg>(zb_addr_u{.addr_short = 0}, addr_mode_t::NoAddr_NoEP, 0, std::forward<Args>(args)...);
        }

        template<auto memPtr, send_cmd_config_t cfg={}, class... Args>
        [[nodiscard]] std::optional<cmd_id_t> send_cmd(short_addr_t addr, Args&&...args)
        {
            return send_cmd_impl<memPtr, cfg>(zb_addr_u{.addr_short = addr.short_addr}, addr_mode_t::Dst16EP, addr.ep, std::forward<Args>(args)...);
        }

        template<auto memPtr, send_cmd_config_t cfg={}, class... Args>
        [[nodiscard]] std::optional<cmd_id_t> send_cmd(long_addr_t a, Args&&...args)
        {
            zb_addr_u addr;
            std::memcpy(addr.addr_long, a.long_addr, sizeof(a.long_addr));
            return send_cmd_impl<memPtr, cfg>(addr, addr_mode_t::Dst64EP, a.ep, std::forward<Args>(args)...);
        }

        template<auto memPtr, send_cmd_config_t cfg={}, class... Args>
        [[nodiscard]] std::optional<cmd_id_t> send_cmd(group_addr_t a, Args&&...args)
        {
            return send_cmd_impl<memPtr, cfg>(zb_addr_u{.addr_short = a.group}, addr_mode_t::Group_NoEP, 0, std::forward<Args>(args)...);
        }

        template<auto memPtr, send_cmd_config_t cfg={}, class... Args>
        [[nodiscard]] std::optional<cmd_id_t> send_cmd(bind_id_addr_t a, Args&&...args)
        {
            return send_cmd_impl<memPtr, cfg>(zb_addr_u{.addr_short = 0}, addr_mode_t::EPAsBindTableId, a.bind_table_id, std::forward<Args>(args)...);
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
