#include "powerinfer-perf.h"
#include "powerinfer-perf.hpp"

void powerinfer_perf_begin(const char *name) {
    powerinfer_begin_event(name);
}

void powerinfer_perf_end() {
    powerinfer_end_event();
}
