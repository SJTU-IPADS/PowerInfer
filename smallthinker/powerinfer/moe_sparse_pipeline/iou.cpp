#include <fcntl.h>

#include "powerinfer-log.hpp"
#include "moe_sparse_pipeline/config.hpp"
#include "moe_sparse_pipeline/iou.hpp"
#include <unistd.h>
namespace moe_sparse_pipeline {

IOUring::IOUring(const std::string &path, size_t queue_depth) : queue_depth(queue_depth), req_data_buf(queue_depth) {
    fd = open(path.c_str(), O_RDONLY | O_DIRECT);
    POWERINFER_ASSERT(fd >= 0);

    io_uring_params params{};

    if (iou_enable_sq_poll) {
        params.flags |= IORING_SETUP_SQPOLL;

        if (iou_sq_poll_cpu_affinity != -1) {
            params.flags |= IORING_SETUP_SQ_AFF;
            params.sq_thread_idle = 1000;
            params.sq_thread_cpu = iou_sq_poll_cpu_affinity;
        }
    }

    int ret = io_uring_queue_init_params(queue_depth, &ring, &params);
    POWERINFER_ASSERT(ret >= 0);
}

IOUring::~IOUring() {
    if (fd != -1) {
        io_uring_queue_exit(&ring);
        close(fd);
        fd = -1;
    }
}

void IOUring::enqueue_read(void *buffer, size_t offset, size_t size, void *user_data, CallbackFn *callback) {
    POWERINFER_ASSERT(n_inflight < queue_depth);
    n_inflight++;

    io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    POWERINFER_ASSERT(sqe != nullptr);

    auto &req_data = req_data_buf[(req_data_buf_pos++) % queue_depth];
    req_data.read_size = size;
    req_data.user_data = user_data;
    req_data.callback = callback;

    io_uring_prep_read(sqe, fd, buffer, size, offset);
    sqe->flags |= IOSQE_ASYNC;
    sqe->user_data = reinterpret_cast<uint64_t>(&req_data);
}

void IOUring::submit_and_wait(size_t wait_nr) {
    int ret = io_uring_submit_and_wait(&ring, wait_nr);
    POWERINFER_ASSERT(ret >= 0 || ret == -EINTR);
}

size_t IOUring::reap() {
    io_uring_cqe* cqe;
    unsigned head;
    unsigned count = 0;

    io_uring_for_each_cqe(&ring, head, cqe) {
        ++count;
        auto &req_data = *reinterpret_cast<RequestData *>(cqe->user_data);
        if (cqe->res != req_data.read_size) {
            fprintf(stderr, "io_uring read error: expect %zu, got %d\n", req_data.read_size, cqe->res);
            abort();
        }

        req_data.callback(req_data.user_data);
    }
    io_uring_cq_advance(&ring, count);
    n_inflight -= count;

    return count;
}

}
