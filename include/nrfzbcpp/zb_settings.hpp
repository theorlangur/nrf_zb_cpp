#ifndef ZB_SETTINGS_HPP_
#define ZB_SETTINGS_HPP_

#include "zb_handlers.hpp"
#include "zephyr/settings/settings.h"

namespace zb
{
    template<class T>
        struct settings_entry
        {
            const char *name;
            T &mem;
        };
    template<class T>
        settings_entry(const char *n, T &m) -> settings_entry<T>;

    template<auto entry, zb::set_attr_value_handler_t h = nullptr>
        void on_setting_changed(zb_zcl_set_attr_value_param_t *p, zb_zcl_device_callback_param_t *pDevCBParam)
        {
            if constexpr (h != nullptr)
                h(p, pDevCBParam);
            settings_save_one(entry.name, &entry.mem, sizeof(std::remove_cvref_t<decltype(entry.mem)>));
        }

    template<size_t off, auto... entries>
        struct persistent_settings_manager
        {
            static int zigbee_settings_set(const char *name, size_t len,
                    settings_read_cb read_cb, void *cb_arg)
            {
                int rc;
                bool found = false;
                auto process_entry = [&](auto e){
                    if (found) return;
                    const char *next;
                    if (settings_name_steq(name, e.name + off, &next) && !next) {
                        using T = std::remove_cvref_t<decltype(e.mem)>;
                        found = true;
                        if (len != sizeof(T)) {
                            rc = -EINVAL;
                            return;
                        }
                        rc = read_cb(cb_arg, &e.mem, sizeof(T));
                        //LOG_INF("Loaded PAN ID: 0x%04x", zb_config.pan_id);
                    }
                };

                (process_entry(entries),...);
                return found ? -ENOENT : rc;
            }

            template<zb::set_attr_value_handler_t h>
                static constexpr zb::set_attr_value_handler_t find_recursive(const char *name)
                {
                    return nullptr;
                }

            template<zb::set_attr_value_handler_t h, auto e, auto... rest>
                static constexpr zb::set_attr_value_handler_t find_recursive(const char *name)
                {
                    if (e.name == name)
                        return &on_setting_changed<e, h>;
                    else
                        return find_recursive<h, rest...>(name);
                }

            template<zb::set_attr_value_handler_t h>
                static constexpr zb::set_attr_value_handler_t make_on_changed(const char *name)
                {
                    return find_recursive<h, entries...>(name);
                }
        };
}
#endif
