#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <thread>
#include <tuple>

namespace powerinfer {

    struct DiskBuffer {
    public:
        static constexpr int NUM_BUFFER         = 2;
        static constexpr int INIT_BUFFER_SIZE   = 32 * 1024 * 1024;

    public:
        size_t              cur_buffer_size[NUM_BUFFER] { 0 };
        size_t              buffer_padding[NUM_BUFFER] { 0 };
        uint8_t *           buffer_ptr[NUM_BUFFER] {0};
        std::atomic_bool    event_flag[NUM_BUFFER];
        size_t              prev_tensor[NUM_BUFFER] { 0 };

    public:
        DiskBuffer() {
            for (int i = 0; i < NUM_BUFFER; ++i)  {
                cur_buffer_size[i] = INIT_BUFFER_SIZE;
                buffer_padding[i]  = 0;
                buffer_ptr[i]      = aligned_allocate(INIT_BUFFER_SIZE, 4096);
                event_flag[i].store(true);
                prev_tensor[i]     = std::numeric_limits<size_t>::max();
            }
        }

        ~DiskBuffer() {
            for (int i = 0; i < NUM_BUFFER; ++i) {
                wait_event(i);
                if (buffer_ptr[i] != nullptr) {
                    aligned_deallocate(buffer_ptr[i]);
                }
            }
        }

    public:
        std::tuple<uint8_t *, size_t, size_t> get_buffer(const int idx, const size_t expect_offset, const size_t expect_size) {
            const size_t offset_beg = expect_offset / 4096 * 4096;
            const size_t offset_end = (expect_offset + expect_size + 4095) / 4096 * 4096;
            const size_t read_size  = offset_end - offset_beg;

            if (cur_buffer_size[idx] < read_size) {
                cur_buffer_size[idx] = read_size;

                aligned_deallocate(buffer_ptr[idx]);
                buffer_ptr[idx] = aligned_allocate(read_size, 4096);
            }

            buffer_padding[idx] = expect_offset - offset_beg;
            return { buffer_ptr[idx], offset_beg, read_size };
        }

        const uint8_t *read_buffer(const int idx) const {
            return buffer_ptr[idx] + buffer_padding[idx];
        }

        void set_event(const int idx) {
            event_flag[idx].store(false);
        }

        void finish_event(const int idx) {
            event_flag[idx].store(true);
        }

        void wait_event(const int idx) const {
            while (!event_flag[idx].load()) { std::this_thread::yield(); }
        }

    public:
        inline static uint8_t *aligned_allocate(const size_t size, const size_t alignment) {
        #ifdef _WIN32
            return static_cast<uint8_t *>(_aligned_malloc(size, alignment));
        #else
            return new(static_cast<std::align_val_t>(alignment)) uint8_t[size];
        #endif
        }

        inline static void aligned_deallocate(uint8_t *ptr) {
        #ifdef _WIN32
            return _aligned_free(ptr);
        #else
            delete[] ptr;
        #endif
        }
    };

} // namespace powerinfer