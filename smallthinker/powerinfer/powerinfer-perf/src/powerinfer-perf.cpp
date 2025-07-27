#include "powerinfer-perf.hpp"

#if defined(POWERINFER_WITH_TRACING)
#include <atomic>
#include <memory>
#include <fstream>

#include "perfetto.h"

#define TRACE_CATEGORY "powerinfer"

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category(TRACE_CATEGORY).SetDescription("PowerInfer DLL"),
);

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

static struct EnablePerfTracer {
    const char *trace_path = nullptr;
    std::unique_ptr<perfetto::TracingSession> session;
    std::atomic<bool> enabled{false};

    EnablePerfTracer() {
        trace_path = getenv("PERF_TRACE_PATH");
        if (trace_path) {
            llama_start_tracing();
            powerinfer_enable_tracing();
        }
    }

    ~EnablePerfTracer() {
        if (trace_path) {
            powerinfer_disable_tracing();
            llama_stop_tracing(trace_path);
        }
    }
} trace_state;

void powerinfer_enable_tracing() {
    trace_state.enabled.store(true);
}

void powerinfer_disable_tracing() {
    trace_state.enabled.store(false);
}

void powerinfer_begin_event(const char *name) {
    if (trace_state.enabled.load(std::memory_order_relaxed)) {
        TRACE_EVENT_BEGIN(TRACE_CATEGORY, perfetto::DynamicString{name});
    }
}

void powerinfer_end_event() {
    if (trace_state.enabled.load(std::memory_order_relaxed)) {
        TRACE_EVENT_END(TRACE_CATEGORY);
    }
}

void powerinfer_begin_event_at_track(const char *name, uint64_t track_id) {
    if (trace_state.enabled.load(std::memory_order_relaxed)) {
        TRACE_EVENT_BEGIN(TRACE_CATEGORY, perfetto::DynamicString{name}, perfetto::Track(track_id));
    }
}

void powerinfer_end_event_at_track(uint64_t track_id) {
    if (trace_state.enabled.load(std::memory_order_relaxed)) {
        TRACE_EVENT_END(TRACE_CATEGORY, perfetto::Track(track_id));
    }
}

void llama_start_tracing() {
    perfetto::TracingInitArgs args;
    args.backends = perfetto::kInProcessBackend;
    args.use_monotonic_clock = true;
    perfetto::Tracing::Initialize(args);
    perfetto::TrackEvent::Register();

    perfetto::TraceConfig cfg;
    cfg.add_buffers()->set_size_kb(512 * 1024);  // Record up to 10 MiB.
    auto *ds_cfg = cfg.add_data_sources()->mutable_config();
    ds_cfg->set_name("track_event");
    perfetto::protos::gen::TrackEventConfig track_event_cfg;
    track_event_cfg.add_enabled_categories("*");
    track_event_cfg.add_enabled_categories(TRACE_CATEGORY);
    ds_cfg->set_track_event_config_raw(track_event_cfg.SerializeAsString());

    trace_state.session = perfetto::Tracing::NewTrace();
    trace_state.session->Setup(cfg);
    trace_state.session->StartBlocking();
}

void llama_stop_tracing(const char *save_path) {
    puts("Saving trace data...");

    perfetto::TrackEvent::Flush();
    trace_state.session->StopBlocking();
    std::vector<char> trace_data(trace_state.session->ReadTraceBlocking());

    std::ofstream output;
    output.open(save_path, std::ios::out | std::ios::binary);
    if (!output.is_open()) {
        fprintf(stderr, "Failed to open file %s for writing\n", save_path);
        return;
    }
    output.write(trace_data.data(), trace_data.size());
    if (output.fail()) {
        fprintf(stderr, "Failed to write data to file %s\n", save_path);
        output.close();
        return;
    }

    output.close();

    printf(
        "%s: Saved %.3lf MiB trace data to \"%s\"\n",
        __func__,
        trace_data.size() / 1024.0 / 1024,
        save_path
    );

    trace_state.session.reset();
    perfetto::Tracing::Shutdown();
}

#else

void powerinfer_enable_tracing() { }

void powerinfer_disable_tracing() { }

void powerinfer_begin_event(const char *name) { (void)(name); }

void powerinfer_end_event() { }

void powerinfer_begin_event_at_track(const char *name, uint64_t track_id) { (void)(name); (void)(track_id); }

void powerinfer_end_event_at_track(uint64_t track_id) { (void)(track_id); }

void llama_start_tracing() { }

void llama_stop_tracing(const char *save_path) { (void)(save_path); }

#endif
