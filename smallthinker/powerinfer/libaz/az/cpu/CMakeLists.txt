add_library(az-cpu)

target_sources(az-cpu PRIVATE
    "axpy.cpp"
    "exp_lut.cpp"
    "quant_types.cpp"
    "silu_lut.cpp"
    "softmax.cpp"
    "vec_dot.cpp"
    "aarch64/gemv.cpp"
)
target_link_libraries(az-cpu PUBLIC az-core)
