#pragma once

#include <string>
#include <fstream>
#include <vector>

namespace moe_sparse_pipeline {

struct ExpertBundleBuilder {
    size_t current_offset = 0;

    explicit ExpertBundleBuilder(const std::string &out_path);

    // Assume data format is q4_0
    void append(const void *data, size_t ne0, size_t ne1,bool repack);
    void append_zero(size_t ne0, size_t ne1); // for non-moe layers, append zero data
    void flush();

private:
    std::ofstream file;
    std::vector<char> buf;
};

}
