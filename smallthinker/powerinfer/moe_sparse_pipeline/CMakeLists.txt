add_library(powerinfer-moe-sparse-pipeline)

target_sources(powerinfer-moe-sparse-pipeline PRIVATE
    "expert_bundle.cpp"
    "expert_cache.cpp"
    "iou.cpp"
    "packed_kernel.cpp"
    "pipeline.cpp"
    "task.cpp"
)
target_include_directories(powerinfer-moe-sparse-pipeline PUBLIC .)
target_link_libraries(powerinfer-moe-sparse-pipeline PUBLIC az)
target_link_libraries(powerinfer-moe-sparse-pipeline PRIVATE powerinfer-cpu)
