#pragma once

#include <cstdint>
#include <string>

void powerinfer_enable_tracing();
void powerinfer_disable_tracing();
void powerinfer_begin_event(const char *name);
void powerinfer_end_event();
void powerinfer_begin_event_at_track(const char *name, uint64_t track_id);
void powerinfer_end_event_at_track(uint64_t track_id);

void llama_start_tracing();
void llama_stop_tracing(const char *save_path);

struct PerfEvent {
    PerfEvent(const char *name) { powerinfer_begin_event(name); }
    ~PerfEvent() noexcept { powerinfer_end_event(); }
};

struct PerfTracer {
    inline static const char *PERF_FILENAME = "PowerInfer.perf";

    PerfTracer() {
        llama_start_tracing();
        powerinfer_enable_tracing();
    }
    ~PerfTracer() noexcept {
        powerinfer_disable_tracing();
        llama_stop_tracing(PERF_FILENAME);
    }
};
