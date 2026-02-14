#ifndef ZB_DESC_HELPER_TYPES_CMD_HANDLING_HPP_
#define ZB_DESC_HELPER_TYPES_CMD_HANDLING_HPP_

#include "zb_desc_helper_types.hpp"
#include <span>

namespace zb
{
    struct global_device;

    namespace internals{
        template<class T, uint8_t dummy> requires requires { sizeof(T); }
        decltype(auto) delay_tpl_call(T *) { 
            return T::get(); 
        }

        template<class T, uint8_t dummy>
        decltype(auto) delay_tpl_call(...) { 
            static_assert(sizeof(T*) == 0, "Define struct global_device with a static method get!");
            return nullptr; 
        }
    }

    struct CmdHandlingResult
    {
        zb_ret_t status = RET_OK;
        bool processed = true;
    };
    using cmd_field_raw_handler_t = CmdHandlingResult (*)(zb_zcl_parsed_hdr_t* pHdr, std::span<uint8_t> data, void *pField);

    struct RawHandlerResult
    {
        cmd_field_raw_handler_t h = nullptr;
        void *field = nullptr;
    };

    template<class StructTag, uint8_t ep>
    struct cluster_custom_handler_t
    {
        static zb_discover_cmd_list_t* get_cmd_list() 
        {
            auto &dev_ctx = internals::delay_tpl_call<global_device, ep>((global_device*)nullptr);
            return dev_ctx.template ep_obj<ep>().template attribute_list<StructTag>();
        }

        static CmdHandlingResult on_cmd(zb_zcl_parsed_hdr_t* pHdr, std::span<uint8_t> data)
        {
            auto &dev_ctx = internals::delay_tpl_call<global_device, ep>((global_device*)nullptr);
            RawHandlerResult raw_handler = dev_ctx.template ep_obj<ep>().template attribute_list<StructTag>().find_handler_for_cmd(pHdr->cmd_id);
            if (raw_handler.field)
                return raw_handler.h(pHdr, data, raw_handler.field);
            return {RET_OK, false};
        }
    };

    template<class DerivedHandler>
    struct cluster_custom_handler_base_t
    {
        static zb_discover_cmd_list_t* get_cmd_list() 
        { 
            auto &dev_ctx = DerivedHandler::get_device();
            return [&]<class StructTag, uint8_t ep>(cluster_custom_handler_t<StructTag, ep>*)->zb_discover_cmd_list_t*{
                return dev_ctx.template ep_obj<ep>().template attribute_list<StructTag>();
            }((DerivedHandler*)nullptr);
        }

        static CmdHandlingResult on_cmd(zb_zcl_parsed_hdr_t* pHdr, std::span<uint8_t> data)
        {
            auto &dev_ctx = DerivedHandler::get_device();
            return [&]<class StructTag, uint8_t ep>(cluster_custom_handler_t<StructTag, ep>*)->CmdHandlingResult{
                RawHandlerResult raw_handler = dev_ctx.template ep_obj<ep>().template attribute_list<StructTag>().find_handler_for_cmd(pHdr->cmd_id);
                if (raw_handler.field)
                    return raw_handler.h(pHdr, data, raw_handler.field);
                return {RET_OK, false};
            }((DerivedHandler*)nullptr);
        }
    };
}
#endif
