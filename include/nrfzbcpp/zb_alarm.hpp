#ifndef ZBH_ALARM_HPP_
#define ZBH_ALARM_HPP_

#include <optional>
#include <zephyr/sys/reboot.h>
#include <cstdint>
#include <lib_function.hpp>
#include <lib_misc_helpers.hpp>
#include <nrf_general/libz_thread_lock.hpp>
#include "zboss_api_core.h"

#ifndef ALARM_LIST_LOCK_TYPE
#define ALARM_LIST_LOCK_TYPE thread::SpinLock
#endif

namespace zb
{
    struct zb_alarm_t
    {
        static constexpr uint8_t kCounterOfDeathInactive = 0xff;
        static constexpr uint8_t kCounterOfDeathValue = 6;
        static constexpr uint8_t kLowOnHandlesThreshold = 2;

        static bool g_RunningOutOfHandles;
        static uint8_t g_CounterOfDeath;

        struct timer_list_t
        {
            static constexpr uint8_t kFull = 9;
            static constexpr uint8_t kMaxSize = kFull - 1;

            using handle_t = uint8_t;
            static constexpr handle_t kInvalidHandle = kFull;

            struct timer_entry_t
            {
                zb_alarm_t *pAlarm;
                void* cb;
                void *param;
            };

            struct handle_entry_t
            {
                timer_entry_t &entry;
                handle_t h;
            };

            std::optional<handle_entry_t> Allocate()
            {
                thread::LockGuard l(&g_AlarmLock);
                if (head != kFull)
                {
                    auto r = head;
                    head = entries[head].nextFree;
                    return handle_entry_t{entries[r].entry, r};
                }
                return std::nullopt;
            }

            void Free(handle_t h)
            {
                thread::LockGuard l(&g_AlarmLock);
                entries[h].nextFree = head;
                head = h;
            }

            auto& GetEntry(handle_t h) { return entries[h].entry; }

            static constexpr timer_list_t Create()
            {
                timer_list_t r;
                r.head = 0;
                for(size_t i = 0; i < kMaxSize; ++i)
                {
                    auto &e = r.entries[i];
                    e.nextFree = i + 1;
                }
                r.entries[kMaxSize - 1].nextFree = kFull;
                return r;
            }

        private:

            union list_entry_t
            {
                uint8_t nextFree;
                timer_entry_t entry;
            };
            handle_t head;
            list_entry_t entries[kMaxSize];

            static ALARM_LIST_LOCK_TYPE g_AlarmLock;
        };
        constinit static timer_list_t g_TimerList;

        using callback_t = void(*)(void*);

        static void on_alarm(timer_list_t::timer_entry_t const& e)
        {
            ((callback_t)e.cb)(e.param);
        }

        template<auto CB = zb_alarm_t::on_alarm>
        static void on_scheduled_alarm(uint8_t param)
        {
            auto e = g_TimerList.GetEntry(param);
            g_TimerList.Free(e.pAlarm->h);
            e.pAlarm->h = timer_list_t::kInvalidHandle;
            CB(e);
        }

        const char *pDescr = nullptr;
        timer_list_t::handle_t h = timer_list_t::kInvalidHandle;

        ~zb_alarm_t() { Cancel(); }

        bool IsRunning() const { return h != timer_list_t::kInvalidHandle; }
        void Cancel() { Cancel<on_alarm>(); }
        zb_ret_t Setup(callback_t cb, void *param, uint32_t time) { return Setup<callback_t, on_alarm>(cb, param, time); }

    protected:

        template<auto CB>
        void Cancel()
        {
            if (IsRunning())
            {
                zb_schedule_alarm_cancel(on_scheduled_alarm<CB>, h, nullptr);
                g_TimerList.Free(h);
                h = timer_list_t::kInvalidHandle;
            }
        }

        template<class callback_t, auto CB>
        zb_ret_t Setup(callback_t cb, void *param, uint32_t time)
        {
            Cancel<CB>();
            if (auto r = g_TimerList.Allocate())
            {
                auto &v = *r;
                h = v.h;
                static_assert(sizeof(void*) == sizeof(callback_t));
                v.entry.cb = (void*)cb;
                v.entry.param = param;
                v.entry.pAlarm = this;

                if (h >= kLowOnHandlesThreshold)
                {
                    FMT_PRINT("Got back handle {:x} >= threshold of {:x}. Prepare to die\n", h, kLowOnHandlesThreshold);
                    //soon we'll be out of handles - let's restart at a convenient moment
                    g_RunningOutOfHandles = true;
                }
                return zb_schedule_app_alarm(on_scheduled_alarm<CB>, h, ZB_MILLISECONDS_TO_BEACON_INTERVAL(time));
            }else
                return RET_NO_MEMORY;
        }

    public:
        static void deactivate_counter_of_death()
        {
            FMT_PRINT("Low on handles: Counter of death deactivated\n");
            g_CounterOfDeath = kCounterOfDeathInactive;
        }

        static void check_counter_of_death()
        {
            if (g_RunningOutOfHandles)
            {
                g_CounterOfDeath = kCounterOfDeathValue;
                FMT_PRINT("Low on handles: Counter of death activated: {} iterations left\n", g_CounterOfDeath);
            }
        }

        static void check_death_count()
        {
            if (g_RunningOutOfHandles && g_CounterOfDeath != kCounterOfDeathInactive)
            {
                if (!(--g_CounterOfDeath))
                {
                    //boom
                    FMT_PRINT("Low on handles: time to die and reborn\n");

                    sys_reboot(SYS_REBOOT_WARM);
                    return;
                }
                FMT_PRINT("Low on handles: tick-tock: {} iterations left\n", zb_alarm_t::g_CounterOfDeath);
            }
        }
    };

    inline bool zb_alarm_t::g_RunningOutOfHandles = false;
    inline uint8_t zb_alarm_t::g_CounterOfDeath = kCounterOfDeathInactive;
    inline ALARM_LIST_LOCK_TYPE zb_alarm_t::timer_list_t::g_AlarmLock;
    inline constinit zb_alarm_t::timer_list_t zb_alarm_t::g_TimerList = zb_alarm_t::timer_list_t::Create();

    struct zb_timer_t: zb_alarm_t
    {
        //return 'true' if timer should go on, false - if timer should stop
        using callback_t = bool (*)(void* param);

        uint32_t m_Interval = 0;

        ~zb_timer_t() { Cancel(); }

        static void on_timer(timer_list_t::timer_entry_t const& e)
        {
            if (callback_t(e.cb)(e.param))
            {
                zb_timer_t *pT = static_cast<zb_timer_t*>(e.pAlarm);
                //re-schedule
                auto res = pT->Setup(callback_t(e.cb), e.param, pT->m_Interval);
                if (res != RET_OK)
                {
                    FMT_PRINT("Could not re-register timer with error {:x}\n", res);
                }
            }
        }

        void Cancel() { zb_alarm_t::Cancel<on_timer>(); }

        zb_ret_t Setup(callback_t cb, void *param, uint32_t time)
        {
            m_Interval = time;
            return zb_alarm_t::Setup<callback_t, on_timer>(cb, param, time);
        }
    };

    template<size_t FuncSZ = 16>
    struct zb_timer_ext_t: zb_timer_t
    {
        using generic_callback_t = FixedFunction<FuncSZ, bool()>;
        generic_callback_t m_Callback;

        static bool on_timer_ext(void *param) { return (*(generic_callback_t*)param)(); }

        template<class callback_t>
        zb_ret_t Setup(callback_t &&cb, uint32_t time)
        {
            m_Callback = std::forward<callback_t>(cb);
            return zb_timer_t::Setup(on_timer_ext, &m_Callback, time);
        }
    };
    using zb_timer_ext_16_t = zb_timer_ext_t<16>;

    template<size_t FuncSZ = 16>
    struct zb_alarm_ext_t: zb_alarm_t
    {
        using generic_callback_t = FixedFunction<FuncSZ, void()>;
        generic_callback_t m_Callback;

        static void on_timer_ext(void *param) { (*(generic_callback_t*)param)(); }

        template<class callback_t>
        zb_ret_t Setup(callback_t &&cb, uint32_t time)
        {
            m_Callback = std::forward<callback_t>(cb);
            return zb_alarm_t::Setup(on_timer_ext, &m_Callback, time);
        }
    };
    using zb_alarm_ext_16_t = zb_alarm_ext_t<16>;

 
}
#endif
