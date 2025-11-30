#ifndef ZB_DESC_HELPER_TYPES_CLUSTER_HPP_
#define ZB_DESC_HELPER_TYPES_CLUSTER_HPP_

#include "lib_object_pool.hpp"
#include "zb_desc_helper_types_attr.hpp"
#include "zb_desc_helper_types_cmd_handling.hpp"
#include <algorithm>
#include <optional>
#include <span>

namespace zb
{
    enum class FrameDirection: uint8_t
    {
        ToServer = ZB_ZCL_FRAME_DIRECTION_TO_SRV,
        ToClient = ZB_ZCL_FRAME_DIRECTION_TO_CLI
    };

    enum class AddrMode: uint8_t
    {
        NoAddr_NoEP = ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT,
        Group_NoEP = ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT,
        Dst16EP = ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        Dst64EP = ZB_APS_ADDR_MODE_64_ENDP_PRESENT,
        EPAsBindTableId = ZB_APS_ADDR_MODE_BIND_TBL_ID
    };

    union frame_ctl_t
    {
        struct{
            uint8_t cluster_specific     : 1;
            uint8_t manufacture_specific : 1;
            FrameDirection direction     : 1;
            uint8_t disable_default_response : 1;
        } f;
        uint8_t u8;
    };


    template<class T, class MemType>
    using mem_attr_t = MemType T::*;

    template<class T, class MemType>
    struct cluster_mem_desc_t
    {
        using MemT = MemType;

        mem_attr_t<T, MemType> m;
        zb_uint16_t id;
        Access a = Access::Read;
        Type type = TypeToTypeId<MemType>();

        constexpr inline bool has_access(Access _a) const { return a & _a; } 
        constexpr inline bool is_cvc() const { 
            if (a & Access::Report)
            {
                switch(type)
                {
                    case Type::S8:
                    case Type::U8:
                    case Type::S16:
                    case Type::U16:
                    case Type::S24:
                    case Type::U24:
                    case Type::S32:
                    case Type::U32:
                    case Type::Float:
                    case Type::HalfFloat:
                    case Type::Double:
                        return true;
                    default:
                        break;
                }
            }
            return false;
        } 
    };
    template<class T, class MemType>
    using attribute_t = cluster_mem_desc_t<T, MemType>;

    //helper for attributes
    template<auto memPtr, auto...ClusterMemDescriptions>
    struct find_cluster_mem_desc_t;

    template<auto memPtr, auto MemDesc, auto...ClusterMemDescriptions> requires (MemDesc.m == memPtr)
    struct find_cluster_mem_desc_t<memPtr, MemDesc, ClusterMemDescriptions...>
    {
        static constexpr auto mem_desc() { return MemDesc; }
    };

    template<auto memPtr, auto MemDesc, auto...ClusterMemDescriptions>
    struct find_cluster_mem_desc_t<memPtr, MemDesc, ClusterMemDescriptions...>: find_cluster_mem_desc_t<memPtr, ClusterMemDescriptions...>
    {
    };

    template<auto memPtr>
    struct find_cluster_mem_desc_t<memPtr>
    {
        static constexpr auto cmd_desc() { static_assert(sizeof(memPtr) == 0, "Pointer to a member is not an attribte"); }
    };

    //helper for commands

    template<auto MemPtr>
    constexpr uint8_t CmdIdFromCmdMemPtr = mem_ptr_traits<decltype(MemPtr)>::MemberType::kCmdId;

    template<auto MemPtr, auto...CmdMemPtrs>
    struct find_cluster_cmd_desc_t;

    template<auto MemPtr, auto CmdMemPtr, auto...CmdMemPtrs> requires (MemPtr == CmdMemPtr)
    struct find_cluster_cmd_desc_t<MemPtr, CmdMemPtr, CmdMemPtrs...>
    {
        static constexpr auto cmd_desc() { return typename mem_ptr_traits<decltype(MemPtr)>::MemberType{}; }
    };

    template<auto MemPtr, auto CmdMemPtr, auto...CmdMemPtrs>
    struct find_cluster_cmd_desc_t<MemPtr, CmdMemPtr, CmdMemPtrs...>: find_cluster_cmd_desc_t<MemPtr, CmdMemPtrs...>
    {
    };

    template<auto MemPtr>
    struct find_cluster_cmd_desc_t<MemPtr>
    {
        static constexpr auto cmd_desc() { static_assert(sizeof(MemPtr) == 0, "Pointer to a member is not a command"); }
    };

    struct cluster_info_t
    {
        zb_uint16_t id;
        zb_uint16_t rev = 0;
        Role        role = Role::Server;
        zb_uint16_t manuf_code = ZB_ZCL_MANUF_CODE_INVALID;

        constexpr bool operator==(cluster_info_t const&) const = default;
    };

    struct request_runtime_args_base_t
    {
        zb_callback_t cb;
        zb_addr_u dst_addr;
        uint8_t dst_ep;
        AddrMode addr_mode;
        bool canceled;
    };

    template<size_t I>
    struct request_tag_t{};

    template<size_t I, class T>
    struct request_runtime_arg_t
    {
        T val;

        constexpr T& get(request_tag_t<I>) { return val; }
        void copy_to(request_tag_t<I>, uint8_t *&pPtr) const
        { 
            if constexpr (sizeof(T) == 1)
                *pPtr++ = *(const uint8_t*)&val;
            else if constexpr (sizeof(T) == 2)
            {
                *pPtr++ = ((const uint8_t*)&val)[0];
                *pPtr++ = ((const uint8_t*)&val)[1];
            }else
            {
                std::memcpy(pPtr, &val, sizeof(T));
                pPtr += sizeof(T);
            }
        }
    };

    template<class Seq, class... Args>
    struct request_runtime_args_var_t;

    template<size_t... I, class... Args>
    struct request_runtime_args_var_t<std::index_sequence<I...>, Args...>: request_runtime_args_base_t, request_runtime_arg_t<I, Args>...
    {
        using request_runtime_arg_t<I, Args>::get...;
        using request_runtime_arg_t<I, Args>::copy_to...;

        request_runtime_args_var_t(zb_callback_t cb, uint16_t short_a, uint8_t e, AddrMode _addr_mode, Args&&... args):
            request_runtime_args_base_t{.cb = cb, .dst_addr = {.addr_short = short_a} , .dst_ep = e, .addr_mode = _addr_mode, .canceled = false},
            request_runtime_arg_t<I, Args>{args}...
        {}

        request_runtime_args_var_t(zb_callback_t cb, zb_ieee_addr_t long_a, uint8_t e, AddrMode _addr_mode, Args&&... args):
            request_runtime_args_base_t{.cb = cb, .dst_ep = e, .addr_mode = _addr_mode, .canceled = false},
            request_runtime_arg_t<I, Args>{args}...
        {
            std::memcpy(dst_addr.addr_long, long_a, sizeof(zb_ieee_addr_t));
        }

        uint8_t* copy_to_buffer(uint8_t *pPtr) const
        {
            (copy_to(request_tag_t<I>{}, pPtr),...);
            return pPtr;
        }
    };

    struct request_args_t
    {
        uint8_t ep;
        uint16_t profile_id = ZB_AF_HA_PROFILE_ID;
    };

    struct cmd_cfg_t
    {
        uint8_t cmd_id;
        uint8_t pool_size = 1;
        bool    receive = false;
        uint16_t manuf_code = ZB_ZCL_MANUF_CODE_INVALID;
        uint32_t timeout_ms = 0;//no timeout
    };

    template<class T, bool exists>
    struct ConditionalVar;

    template<class T>
    struct ConditionalVar<T, true> { T value; };
    template<class T>
    struct ConditionalVar<T, false> {};
    //as NTTP to cluster description template type
    template<cmd_cfg_t cfg, class... Args>
    struct cluster_cmd_desc_t
    {
        using runtime_args_t = request_runtime_args_var_t<decltype(std::make_index_sequence<sizeof...(Args)>{}), Args...>;
        using PoolType = ObjectPool<runtime_args_t, cfg.pool_size>;
        using pool_idx_type_t = typename PoolType::size_type;
        using args_ret_t = std::optional<pool_idx_type_t>;
        static PoolType g_Pool;
        using RequestPtr = PoolType::template Ptr<g_Pool>;
        using typed_callback_t = void(*)(Args const&...);

        static constexpr auto pool_size() { return cfg.pool_size; }
        static constexpr bool is_generated() { return pool_size() >= 1; }
        static constexpr bool is_received() { return cfg.receive; }
        static constexpr auto timeout_ms() { return cfg.timeout_ms; }

        static constexpr uint8_t kCmdId = cfg.cmd_id;

        [[no_unique_address]]ConditionalVar<typed_callback_t, cfg.receive> m_Callback{};

        static zb_bool_t on_recv_cmd(zb_uint8_t param)
        {
            zb_bool_t processed = ZB_TRUE;
            zb_zcl_parsed_hdr_t cmd_info;
            zb_ret_t status = RET_OK;

            if ( ZB_ZCL_GENERAL_GET_CMD_LISTS_PARAM == param )
            {
                //ZCL_CTX().zb_zcl_cluster_cmd_list = &gs_poll_control_client_cmd_list;
                return ZB_TRUE;
            }
            return ZB_TRUE;
        }

        template<class... TArgs> requires (std::is_convertible_v<std::remove_cvref_t<TArgs>, std::remove_cvref_t<Args>> &&...)
        static args_ret_t prepare_args(zb_callback_t cb, TArgs&&... args) { 
            auto r = g_Pool.PtrToIdx(g_Pool.Acquire(cb, uint16_t(0), uint8_t(0), AddrMode::NoAddr_NoEP, std::forward<Args>(args)...)); 
            return r == PoolType::kInvalid ? std::nullopt : args_ret_t(r);
        }

        template<class... TArgs> requires (std::is_convertible_v<std::remove_cvref_t<TArgs>, std::remove_cvref_t<Args>> &&...)
        static args_ret_t prepare_args(zb_callback_t cb, uint16_t short_addr, uint8_t ep, TArgs&&... args) 
        { 
            auto r = g_Pool.PtrToIdx(g_Pool.Acquire(cb, short_addr, ep, AddrMode::Dst16EP, std::forward<Args>(args)...)); 
            return r == PoolType::kInvalid ? std::nullopt : args_ret_t(r);
        }

        template<class... TArgs> requires (std::is_convertible_v<std::remove_cvref_t<TArgs>, std::remove_cvref_t<Args>> &&...)
        static void prepare_args(zb_callback_t cb, zb_ieee_addr_t ieee_addr, uint8_t ep, TArgs&&... args)
        {
            auto r = g_Pool.PtrToIdx(g_Pool.Acquire(cb, ieee_addr, ep, AddrMode::Dst64EP, std::forward<Args>(args)...)); 
            return r == PoolType::kInvalid ? std::nullopt : args_ret_t(r);
        }

        template<class... TArgs> requires (std::is_convertible_v<std::remove_cvref_t<TArgs>, std::remove_cvref_t<Args>> &&...)
        static args_ret_t prepare_args(zb_callback_t cb, uint16_t group_addr, TArgs&&... args) 
        { 
            auto r = g_Pool.PtrToIdx(g_Pool.Acquire(cb, group_addr, uint8_t(0), AddrMode::Group_NoEP, std::forward<Args>(args)...)); 
            return r == PoolType::kInvalid ? std::nullopt : args_ret_t(r);
        }

        template<class... TArgs> requires (std::is_convertible_v<std::remove_cvref_t<TArgs>, std::remove_cvref_t<Args>> &&...)
        static args_ret_t prepare_args(zb_callback_t cb, uint8_t bind_table_id, TArgs&&... args) 
        { 
            auto r = g_Pool.PtrToIdx(g_Pool.Acquire(cb, uint16_t(0), bind_table_id, AddrMode::EPAsBindTableId, std::forward<Args>(args)...)); 
            return r == PoolType::kInvalid ? std::nullopt : args_ret_t(r);
        }
            
        template<cluster_info_t i, request_args_t r>
        static bool request(uint16_t argsPoolIdx)
        {
            static_assert(r.profile_id && cfg.pool_size >= 1, "This command cannot be sent");
            return zigbee_get_out_buf_delayed_ext( &on_out_buf_ready<i, r>, argsPoolIdx, 0) == RET_OK;
        }

        static void cancel(uint16_t argsPoolIdx)
        {
            auto *pArgs = g_Pool.IdxToPtr(argsPoolIdx);
            if (g_Pool.IsValid(pArgs))
                pArgs->canceled = true;
        }

        template<cluster_info_t i, request_args_t r> requires (cfg.pool_size >= 1)
        static void on_out_buf_ready(zb_bufid_t bufid, uint16_t poolIdx)
        {
            auto *pArgs = g_Pool.IdxToPtr(poolIdx);
            if (!g_Pool.IsValid(pArgs))
            {
                //weird
                return;
            }

            if (pArgs->canceled)
                return;
            RequestPtr raii(pArgs);

            if (bufid == ZB_BUF_INVALID)
            {
                //out of mem?
                if (pArgs->cb) pArgs->cb(0);
                return;
            }

            constexpr uint16_t manu_code = i.manuf_code != ZB_ZCL_MANUF_CODE_INVALID ? i.manuf_code : cfg.manuf_code;

            static_assert(i.role == Role::Client || i.role == Role::Server);
            frame_ctl_t f{.f{
                .cluster_specific = true, 
                .manufacture_specific = manu_code != ZB_ZCL_MANUF_CODE_INVALID
                , .direction = i.role == Role::Client ? FrameDirection::ToServer : FrameDirection::ToClient
                , .disable_default_response = false
            }};
            ZB_ZCL_GET_SEQ_NUM();
            uint8_t* ptr = (uint8_t*)zb_zcl_start_command_header(bufid, f.u8, manu_code, cfg.cmd_id, nullptr);
            ptr = pArgs->copy_to_buffer(ptr);
            zb_ret_t ret = zb_zcl_finish_and_send_packet(bufid, ptr, &pArgs->dst_addr, (uint8_t)pArgs->addr_mode, pArgs->dst_ep, r.ep, r.profile_id, i.id, pArgs->cb);
            if (RET_OK != ret && pArgs->cb)
                pArgs->cb(0);
            if (RET_OK != ret)
                zb_buf_free(bufid);
        }
    };
    template<cmd_cfg_t cfg, class... Args>
    constinit inline cluster_cmd_desc_t<cfg, Args...>::PoolType cluster_cmd_desc_t<cfg, Args...>::g_Pool{};

    template<zb_uint8_t cmd_id, class... Args>
    struct cluster_std_cmd_desc_t: cluster_cmd_desc_t<{.cmd_id = cmd_id}, Args...> {};

    template<zb_uint8_t cmd_id, uint8_t pool_size, class... Args>
    struct cluster_std_cmd_desc_with_pool_size_t: cluster_cmd_desc_t<{.cmd_id = cmd_id, .pool_size = pool_size ? pool_size : cmd_cfg_t{}.pool_size}, Args...> {};

    template<zb_uint8_t cmd_id, class... Args>
    struct cluster_in_cmd_desc_t: cluster_cmd_desc_t<{.cmd_id = cmd_id, .pool_size = 0, .receive = true}, Args...> {
        using this_type = cluster_in_cmd_desc_t<cmd_id, Args...>;
        using callback_t = CmdHandlingResult(*)(Args const&...);
        static constexpr size_t kArgRawSize = (sizeof(Args) + ... + 0);
        callback_t cb = nullptr;

        static CmdHandlingResult raw_handler(zb_zcl_parsed_hdr_t* pHdr, std::span<uint8_t> data, void *pField)
        {
            this_type *pThis = (this_type *)pField;
            if (!pThis->cb) return {RET_OK, false};
            if (data.size() < kArgRawSize) return {RET_ILLEGAL_REQUEST, true};

            const uint8_t *pData = data.data();
            auto to_arg = [&]<class A>(A *pDummy)->A const&
            {
                const A *p = (const A*)pData;
                pData += sizeof(A);
                return *p;
            };
            return pThis->cb(to_arg((Args*)nullptr)...);
        }
    };

    template<zb_uint8_t cmd_id, class... Args>
    using cmd_in_t = cluster_in_cmd_desc_t<cmd_id, Args...>;

    template<zb_uint8_t cmd_id, uint8_t pool_size, class... Args>
    using cmd_pool_t = cluster_std_cmd_desc_with_pool_size_t<cmd_id, pool_size, Args...>;

    template<cmd_cfg_t cfg, class... Args>
    using cmd_generic_t = cluster_cmd_desc_t<cfg, Args...>;

    template<auto... attributeMemberDesc>
    struct cluster_attributes_desc_t
    {
        static constexpr inline size_t count_members_with_access(Access a) { return ((size_t)attributeMemberDesc.has_access(a) + ... + 0); }
        static constexpr inline size_t count_cvc_members() { return ((size_t)attributeMemberDesc.is_cvc() + ... + 0); }
        template<auto memPtr>
        static constexpr inline auto get_member_description() { return find_cluster_mem_desc_t<memPtr, attributeMemberDesc...>::mem_desc(); }

        template<auto... attributeMemberDesc2>
        friend constexpr auto operator+(cluster_attributes_desc_t<attributeMemberDesc...> lhs, cluster_attributes_desc_t<attributeMemberDesc2...> rhs)
        {
            return cluster_attributes_desc_t< attributeMemberDesc..., attributeMemberDesc2... >{};
        }
    };

    template<auto... attributeMemberDesc>
    using attributes_t = cluster_attributes_desc_t<attributeMemberDesc...>;

    template<auto... cmdMemberDesc>
    struct cluster_commands_desc_t
    {
        static constexpr size_t kCmdCount = sizeof...(cmdMemberDesc);
        template<auto memPtr>
        static constexpr inline auto get_cmd_description() { return find_cluster_cmd_desc_t<memPtr, cmdMemberDesc...>::cmd_desc(); }
        static constexpr inline size_t count_generated() { return ((size_t)mem_ptr_traits<decltype(cmdMemberDesc)>::MemberType::is_generated() + ... + 0); }
        static constexpr inline size_t count_received() { return ((size_t)mem_ptr_traits<decltype(cmdMemberDesc)>::MemberType::is_received() + ... + 0); }

        static constexpr RawHandlerResult find_cmd_handler(uint8_t id, auto *pStruct)
        {
            RawHandlerResult res;
            bool found = false;
            auto check = [&]<class CmdType>(CmdType *pF){
                if constexpr (CmdType::is_received())
                {
                    if (!found && pF->kCmdId == id)
                    {
                        res.field = pF;
                        res.h = &pF->raw_handler;
                        found = true;
                    }
                }
            };
            (check(&(pStruct->*cmdMemberDesc)),...);
            return res;
        }

        static constexpr inline auto get_generated_commands()
        {
            cmd_id_list_t<count_generated()> res;
            int i = 0;
            auto add = [&](bool is_gen, uint8_t id){
                if (is_gen)
                    res.cmds[i++] = id;
            };
            (add(mem_ptr_traits<decltype(cmdMemberDesc)>::MemberType::is_generated(), mem_ptr_traits<decltype(cmdMemberDesc)>::MemberType::kCmdId),...);
            return res;
        }

        static constexpr inline auto get_received_commands()
        {
            cmd_id_list_t<count_received()> res;
            int i = 0;
            auto add = [&](bool is_recv, uint8_t id){
                if (is_recv)
                    res.cmds[i++] = id;
            };
            (add(mem_ptr_traits<decltype(cmdMemberDesc)>::MemberType::is_received(), mem_ptr_traits<decltype(cmdMemberDesc)>::MemberType::kCmdId),...);
            return res;
        }

        static constexpr inline auto max_command_pool_size() 
        { 
            if constexpr (kCmdCount >= 2)
                return std::max(std::initializer_list<uint8_t>{mem_ptr_traits<decltype(cmdMemberDesc)>::MemberType::pool_size()...}); 
            else if constexpr (kCmdCount == 1)
                return (mem_ptr_traits<decltype(cmdMemberDesc)>::MemberType::pool_size(),...);
            else
                return 0;
        }

        template<auto... cmdMemberDesc2>
        friend constexpr auto operator+(cluster_commands_desc_t<cmdMemberDesc...> lhs, cluster_commands_desc_t<cmdMemberDesc2...> rhs)
        {
            return cluster_commands_desc_t< cmdMemberDesc..., cmdMemberDesc2... >{};
        }
    };
    template<auto... cmdMemberDesc>
    using commands_t = cluster_commands_desc_t<cmdMemberDesc...>;

    template<cluster_info_t ci, auto attributes = cluster_attributes_desc_t<>{}, auto cmds = cluster_commands_desc_t<>{}>//auto = cluster_attributes_desc_t
    struct cluster_struct_desc_t
    {
        static constexpr inline zb_uint16_t rev() { return ci.rev; }
        static constexpr inline auto info() { return ci; }
        static constexpr inline size_t count_members_with_access(Access a) { return attributes.count_members_with_access(a); }
        static constexpr inline size_t count_cvc_members() { return attributes.count_cvc_members(); }
        static constexpr inline auto max_command_pool_size() { return cmds.max_command_pool_size(); }
        static constexpr inline size_t count_generated() { return cmds.count_generated(); }
        static constexpr inline size_t count_received() { return cmds.count_received(); }
        static constexpr inline auto get_generated_commands() { return cmds.get_generated_commands(); }
        static constexpr inline auto get_received_commands() { return cmds.get_received_commands(); }
        static constexpr RawHandlerResult find_cmd_handler(uint8_t id, auto *pStruct) { return cmds.find_cmd_handler(id, pStruct); }

        template<auto memPtr>
        static constexpr inline auto get_member_description() { return attributes.template get_member_description<memPtr>(); }

        template<auto memPtr>
        static constexpr inline auto get_cmd_description() { return cmds.template get_cmd_description<memPtr>(); }

        template<cluster_info_t ci2, cluster_attributes_desc_t attributes2, cluster_commands_desc_t cmds2>
        friend constexpr auto operator+(cluster_struct_desc_t<ci, attributes, cmds> lhs, cluster_struct_desc_t<ci2, attributes2, cmds2> rhs)
        {
            static_assert(ci == ci2, "Must be the same revision");
            return cluster_struct_desc_t< ci, attributes + attributes2, cmds + cmds2>{};
        }
    };
    template<cluster_info_t ci, auto attributes = cluster_attributes_desc_t<>{}, auto cmds = cluster_commands_desc_t<>{}>//auto = cluster_attributes_desc_t
    using cluster_t = cluster_struct_desc_t<ci, attributes, cmds>;

    template<class ClusterType>
    constexpr zb_uint16_t cluster_id_v = zb::zcl_description_t<ClusterType>::get().info().id;

    template<class T, class DestT, class MemType> requires std::is_base_of_v<DestT, T>
    constexpr ADesc<MemType> cluster_mem_to_attr_desc(T& s, cluster_mem_desc_t<DestT,MemType> d) { return {.id = d.id, .a = d.a, .pData = &(s.*d.m), .type = d.type}; }

    template<class T,cluster_info_t ci, auto... ClusterMemDescriptions, cluster_attributes_desc_t<ClusterMemDescriptions...> attributes, cluster_commands_desc_t cmds>
    constexpr auto cluster_struct_to_attr_list(T &s, cluster_struct_desc_t<ci, attributes, cmds>)
    {
        return MakeAttributeList(&s, cluster_mem_to_attr_desc(s, ClusterMemDescriptions)...);
    }

    template<uint8_t ep, class... T>
    struct TClusterList
    {
        static constexpr size_t N = sizeof...(T);

        TClusterList(TClusterList const&) = delete;
        TClusterList(TClusterList &&) = delete;
        void operator=(TClusterList const&) = delete;
        void operator=(TClusterList &&) = delete;

        static constexpr auto max_command_pool_size() 
        { 
            if constexpr (N >= 2)
                return std::max(std::initializer_list<uint8_t>{T::max_command_pool_size()...}); 
            else if constexpr(N == 1)
                return (T::max_command_pool_size(),...); 
            else
                return 0;
        }
        static constexpr size_t reporting_attributes_count() { return (T::attributes_with_access(Access::Report) + ... + 0); }
        static constexpr size_t cvc_attributes_count() { return (T::cvc_attributes() + ... + 0); }

        static constexpr size_t server_cluster_count() { return (T::is_role(Role::Server) + ... + 0); }
        static constexpr size_t client_cluster_count() { return (T::is_role(Role::Client) + ... + 0); }
        static constexpr bool has_info(cluster_info_t ci) { return ((T::info() == ci) || ...); }

        constexpr TClusterList(T&... d):
            clusters{ d.template desc<ep>()... }
        {
        }

        alignas(4) zb_zcl_cluster_desc_t clusters[N];
    };

    template<class StructTag, uint8_t ep>
    inline zb_bool_t on_cluster_cmd_handling(zb_uint8_t param)
    {
        if ( ZB_ZCL_GENERAL_GET_CMD_LISTS_PARAM == param )
        {
            ZCL_CTX().zb_zcl_cluster_cmd_list = cluster_custom_handler_t<StructTag, ep>::get_cmd_list();
            return ZB_TRUE;
        }

        //zb_bool_t processed = ZB_TRUE;
        //zb_ret_t status = RET_OK;
        zb_zcl_parsed_hdr_t *cmd_info = ZB_BUF_GET_PARAM(param, zb_zcl_parsed_hdr_t);

        auto [status, processed] = cluster_custom_handler_t<StructTag, ep>::on_cmd(cmd_info, std::span<uint8_t>{(uint8_t*)zb_buf_begin(param), zb_buf_len(param)});

        if( processed )
        {
            if( cmd_info->disable_default_response && status == RET_OK)
            {
                zb_buf_free(param);
            }
            else if (status == RET_NOT_IMPLEMENTED)
            {
                ZB_ZCL_PROCESS_COMMAND_FINISH(param, cmd_info, ZB_ZCL_STATUS_UNSUP_CMD);
            }
            else if (status != RET_BUSY)
            {
                ZB_ZCL_PROCESS_COMMAND_FINISH(param, cmd_info, status==RET_OK ? ZB_ZCL_STATUS_SUCCESS : ZB_ZCL_STATUS_INVALID_FIELD);
            }
        }

        return processed;
    }

    template<class StructTag, uint8_t ep>
    void generic_cluster_init()
    {
        using zcl_desc_t = zcl_description_t<StructTag>;
        constexpr auto d = zcl_description_t<StructTag>::get();
        if constexpr (requires { zcl_desc_t::zboss_init_func(d.info().role); })
        {
            //there's a default ZBOSS init func -> try and call it
            auto zboss_init_f = zcl_desc_t::zboss_init_func(d.info().role);
            //Note: this will work poorly when same cluster is used for different end points
            if (zboss_init_f) zboss_init_f();
        }
        zb_zcl_cluster_check_value_t check_val = nullptr;
        zb_zcl_cluster_write_attr_hook_t write_hook = nullptr;
        zb_zcl_cluster_handler_t cmd_handler = nullptr;
        if constexpr (d.count_received() > 0)
            cmd_handler = &on_cluster_cmd_handling<StructTag, ep>;

        if (check_val || write_hook || cmd_handler)
        {
            constexpr auto i = d.info();
            zb_zcl_add_cluster_handlers(i.id, (uint8_t)i.role
                    , check_val /*cluster_check_value*/
                    , write_hook /*cluster_write_attr_hook*/
                    , cmd_handler /*cluster_handler*/
                    );
        }
    }

    /**********************************************************************/
    /* Template logic to check for duplicate cluster ids                  */
    /**********************************************************************/
    namespace cluster_tools
    {
        template<class Cluster>
        constexpr bool IsAllUniqueHelper(){ return true; }

        template<class ClusterPrime, class ClusterSecondary>
        constexpr bool IsAllUniqueHelper(){ 
            static_assert(cluster_id_v<ClusterPrime> != cluster_id_v<ClusterSecondary>);
            return cluster_id_v<ClusterPrime> != cluster_id_v<ClusterSecondary>; }

        template<class ClusterPrime, class ClusterSecondary, class... Clusters> requires (sizeof...(Clusters) > 0)
        constexpr bool IsAllUniqueHelper(){ return IsAllUniqueHelper<ClusterPrime,ClusterSecondary>() && IsAllUniqueHelper<ClusterPrime, Clusters...>(); }

        template<class ClusterPrime>
        constexpr bool IsAllUniquePrimaryHelper(){ return true; }

        template<class ClusterPrime, class ClusterSecondary>
        constexpr bool IsAllUniquePrimaryHelper(){ return IsAllUniqueHelper<ClusterPrime, ClusterSecondary>(); }

        template<class ClusterPrime, class... Clusters> requires (sizeof...(Clusters) > 1)
        constexpr bool IsAllUniquePrimaryHelper()
        { 
            return IsAllUniqueHelper<ClusterPrime, Clusters...>() && IsAllUniquePrimaryHelper<Clusters...>(); 
        }

        template<class... Clusters>
        constexpr bool kAllUniqueIds = IsAllUniquePrimaryHelper<Clusters...>();
    };
}
#endif
