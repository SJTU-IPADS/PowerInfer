#include "az/core/perfetto_trace.hpp"
#include "az/core/perfetto_trace.h"
#include "az/assert.hpp"

#if defined(AZ_ENABLE_PERFETTO)

#include "perfetto.h"

constexpr const char *event_category = "event";
constexpr const char *counter_category = "counter";

// clang-format off
PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category(event_category),
    perfetto::Category(counter_category),
);
PERFETTO_TRACK_EVENT_STATIC_STORAGE();
// clang-format on

// We do not want to include perfetto.h in perfetto_trace.hpp
// Therefore tracing object is defined in C++ source
static std::unique_ptr<perfetto::TracingSession> tracing;

#endif

namespace az {

PerfettoTrace::~PerfettoTrace() {
    stop_tracing();
}

void PerfettoTrace::start_tracing(const Path &trace_path, size_t buffer_size_kb) {
#if defined(AZ_ENABLE_PERFETTO)
    AZ_ASSERT(!tracing);

    this->trace_path = trace_path;

    perfetto::TracingInitArgs args;
    args.backends = perfetto::kInProcessBackend;
    args.use_monotonic_clock = true;
    perfetto::Tracing::Initialize(args);
    AZ_ASSERT(perfetto::TrackEvent::Register());

    perfetto::TraceConfig cfg;
    cfg.add_buffers()->set_size_kb(buffer_size_kb);
    auto *ds_cfg = cfg.add_data_sources()->mutable_config();
    ds_cfg->set_name("track_event");

    tracing = perfetto::Tracing::NewTrace();
    tracing->Setup(cfg);
    tracing->StartBlocking();

    enabled = true;
#else
    AZ_UNUSED(trace_path);
    AZ_UNUSED(buffer_size_kb);
#endif
}

void PerfettoTrace::stop_tracing() {
#if defined(AZ_ENABLE_PERFETTO)
    AZ_ASSERT(tracing);

    fmt::println("Saving Perfetto trace data to {:?}...", trace_path);

    perfetto::TrackEvent::Flush();
    tracing->StopBlocking();
    std::vector<char> trace_data(tracing->ReadTraceBlocking());

    std::ofstream trace_file;
    trace_file.open(trace_path, std::ios::out | std::ios::binary);

    if (!trace_file) {
        fmt::println(stderr, "Cannot open {:?}.", trace_path);
        abort();
    }

    trace_file.write(trace_data.data(), trace_data.size());
    trace_file.close();

    tracing.reset();
    perfetto::Tracing::Shutdown();
    enabled = false;
#endif
}

void PerfettoTrace::enable() {
    enabled = true;
}

void PerfettoTrace::disable() {
    enabled = false;
}

#if defined(AZ_ENABLE_PERFETTO)

TraceEvent::TraceEvent(const char *name) {
    TRACE_EVENT_BEGIN(event_category, perfetto::StaticString{name});
}

TraceEvent::TraceEvent(const std::string &name) {
    TRACE_EVENT_BEGIN(event_category, perfetto::DynamicString{name});
}

void TraceEvent::end() {
    if (!ended) {
        TRACE_EVENT_END(event_category);
        ended = true;
    }
}
#ifdef __cplusplus
extern "C" {
#endif

void TraceEventStart(const char *name)
{
    TRACE_EVENT_BEGIN(event_category, perfetto::StaticString{name});
}

void TraceEventEnd(const char *name)
{
    AZ_UNUSED(name);
    TRACE_EVENT_END(event_category);
}

#ifdef __cplusplus
}
#endif
#endif

}  // namespace az
