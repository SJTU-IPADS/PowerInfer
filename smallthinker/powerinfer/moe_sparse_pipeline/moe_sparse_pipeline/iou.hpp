#pragma once

#include <string>
#include <vector>
#include <liburing.h>

namespace moe_sparse_pipeline {

struct IOUring {
    using CallbackFn = void(void *user_data);

    int fd = -1;
    size_t n_inflight = 0;

    explicit IOUring(const std::string &path, size_t queue_depth = 64);
    ~IOUring();

    // Enqueue one read request into io_uring
    void enqueue_read(void *buffer, size_t offset, size_t size, void *user_data, CallbackFn *callback);

    // Submit all IO requests with io_uring_submit, and wait for at least `wait_nr` IO requests to complete
    void submit_and_wait(size_t wait_nr);

    // Examine all completed IO requests and invoke corresponding callbacks
    size_t reap();

private:
    struct RequestData {
        size_t read_size = 0;
        void *user_data = nullptr;
        CallbackFn *callback = nullptr;
    };

    struct io_uring ring{};
    size_t queue_depth = 0;
    std::vector<RequestData> req_data_buf;
    size_t req_data_buf_pos = 0;
};

}
