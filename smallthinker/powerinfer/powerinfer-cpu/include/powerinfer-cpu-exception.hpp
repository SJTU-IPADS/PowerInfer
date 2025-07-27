#pragma once

#include <exception>
#include <string>

namespace powerinfer {

    class PowerInferHostException: std::exception {
    public:
        inline static std::string g_error_string_;

    public:
        PowerInferHostException() { g_error_string_ = "Unknown Host error"; print_error(); }

        PowerInferHostException(const std::string &err) { g_error_string_ = err; print_error(); }

        ~PowerInferHostException() noexcept override = default;

    public:
        const char * what() const noexcept override { return g_error_string_.c_str(); }

        static void print_error() {
            fprintf(stderr, "[PowerInfer][Host] ERROR: %s\n", g_error_string_.c_str());
        }
    };

} // namepsace powerinfer
