add_library(az-pipeline)

target_sources(az-pipeline PRIVATE
    "pipeline.cpp"
    "task.cpp"
)
target_link_libraries(az-pipeline PUBLIC az-core)
