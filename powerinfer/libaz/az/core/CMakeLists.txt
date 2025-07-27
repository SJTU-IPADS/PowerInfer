add_library(az-core)

target_sources(az-core PRIVATE
    "aligned_alloc.cpp"
    "buf.cpp"
    "cpu_affinity.cpp"
    "cpu_yield.cpp"
    "fp16.c"
    "handle.cpp"
    "lru.cpp"
    "perfetto_trace.cpp"
    "spin_barrier.cpp"
    "utils.cpp"
)
target_link_libraries(az-core PUBLIC fmt::fmt)

if (AZ_ENABLE_PERFETTO)
    target_link_libraries(az-core PRIVATE perfetto)
endif()
