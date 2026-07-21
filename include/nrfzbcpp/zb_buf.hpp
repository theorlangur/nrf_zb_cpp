#ifndef ZB_BUF_HPP_
#define ZB_BUF_HPP_

#include "zb_types.hpp"
#include <utility>

namespace zb
{
    struct buf_ptr_base_t
    {
        zb_bufid_t release()
        {
            zb_bufid_t r = bufid; 
            bufid = 0;
            return r;
        }

        zb_uint_t size() const { return bufid ? zb_buf_len(bufid) : 0; }

        void* raw() const { return bufid ? zb_buf_begin(bufid) : nullptr; }

        template<class T>
        T* as() const { return reinterpret_cast<T*>(raw()); }

        template<class T>
        T* param() const { return bufid ? reinterpret_cast<T*>(zb_buf_get_tail_func(bufid, sizeof(T))) : nullptr; }

        zb_bufid_t bufid = 0;
    };

    struct buf_ptr_t: buf_ptr_base_t
    {
        ~buf_ptr_t() { reset(); }

        void reset(zb_bufid_t b = 0)
        {
            if (bufid) zb_buf_free(bufid);
            bufid = b;
        }
    };

    struct buf_view_ptr_t: buf_ptr_base_t
    {
    };


    //this is expected to be in small numbers
    template<size_t N>
    struct pre_alloc_zb_bufs_out
    {
        //returns amount of allocation failures
        int init()
        {
            for(size_t i = 0; i < N; ++i)
            {
                if ((bufs[i] = zb_buf_get_out()) == ZB_BUF_INVALID)
                    return int(N - i);
            }
            return 0;
        }

        zb_bufid_t allocate()
        {
            for(size_t i = 0; i < N; ++i)
            {
                if (bufs[i] != ZB_BUF_INVALID)
                    return std::exchange(bufs[i], ZB_BUF_INVALID);
            }
            return ZB_BUF_INVALID;
        }

        void deallocate(zb_bufid_t buf)
        {
            for(size_t i = 0; i < N; ++i)
            {
                if (bufs[i] == ZB_BUF_INVALID)
                {
                    bufs[i] = buf;
                    zb_buf_reuse(buf);
                    return;
                }
            }
            //we're dead
            ZB_ASSERT(false);
        }

        void free()
        {
            for(size_t i = 0; i < N; ++i)
            {
                if (bufs[i] != ZB_BUF_INVALID)
                {
                    zb_buf_free(bufs[i]);
                    bufs[i] = ZB_BUF_INVALID;
                }
            }
        }
    private:
        zb_bufid_t bufs[N]{};
    };
}
#endif

