#pragma once

#include "az/common.hpp"

namespace az {

struct PerfettoTrace final : Noncopyable {
    static auto instance() -> PerfettoTrace & {
        static PerfettoTrace instance;
        return instance;
    }

    ~PerfettoTrace();

    /**
     * Default buffer size is 128 MiB.
     */
    void start_tracing(const Path &trace_path, size_t buffer_size_kb = 128 * 1024);
    void stop_tracing();

    void enable();
    void disable();

private:
    Path trace_path;
    std::atomic<bool> enabled = false;
};

#if defined(AZ_ENABLE_PERFETTO)

struct TraceEvent : Noncopyable {
    /**
     * NOTE: `name` must be a static string.
     */
    TraceEvent(const char *name);

    TraceEvent(const std::string &name);

    ~TraceEvent() {
        end();
    }

    void end();

private:
    bool ended = false;
};

#else

struct TraceEvent : Noncopyable {
    TraceEvent(const char *name) {
        AZ_UNUSED(name);
    }

    TraceEvent(const std::string &name) {
        AZ_UNUSED(name);
    }

    void end() {}
};

#endif

}  // namespace az
