add_library(powerinfer-fused-sparse-moe)

target_sources(powerinfer-fused-sparse-moe PRIVATE
    "fused_sparse_moe.cpp"
)
target_include_directories(powerinfer-fused-sparse-moe PUBLIC .)
target_link_libraries(powerinfer-fused-sparse-moe PUBLIC az)
target_link_libraries(powerinfer-fused-sparse-moe PRIVATE powerinfer-cpu)
