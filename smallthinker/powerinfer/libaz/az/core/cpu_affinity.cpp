#include "az/core/cpu_affinity.hpp"

#include "az/assert.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace az {

auto get_sorted_cpu_info_list() -> std::vector<CPUInfo> {
    namespace fs = std::filesystem;

    std::vector<CPUInfo> cpus;
    const fs::path sys_cpu_dir = "/sys/devices/system/cpu";

    // 遍历系统CPU目录
    for (const auto &entry : fs::directory_iterator(sys_cpu_dir)) {
        if (!entry.is_directory())
            continue;

        const std::string dir_name = entry.path().filename().string();

        // 检查目录名称格式是否为cpuXX
        if (dir_name.compare(0, 3, "cpu") != 0)
            continue;
        const std::string id_str = dir_name.substr(3);

        // 验证数字部分
        if (id_str.empty() || !std::all_of(id_str.begin(), id_str.end(), ::isdigit)) {
            continue;
        }

        // 构造频率文件路径
        const fs::path freq_file = entry.path() / "cpufreq/cpuinfo_max_freq";
        if (!fs::exists(freq_file)) {
            continue;
        }

        // 读取频率值
        std::ifstream file(freq_file);
        if (!file)
            continue;

        std::string freq_str;
        if (!std::getline(file, freq_str))
            continue;

        try {
            const size_t cpu_id = std::stoul(id_str);
            const size_t freq = std::stoul(freq_str);
            cpus.emplace_back(CPUInfo{cpu_id, freq});
        } catch (const std::exception &) {
            continue;
        }
    }

    // 按频率降序排序
    std::sort(cpus.begin(), cpus.end());

    return cpus;
}

void auto_set_cpu_affinity(size_t thread_id) {
    static std::vector<CPUInfo> cpus;
    static RunOnce init_cpus([&] { cpus = get_sorted_cpu_info_list(); });

    AZ_ASSERT(thread_id < cpus.size());

    thread_local bool is_set = false;

    if (is_set) {
        return;
    }

    size_t cpu_id = cpus[thread_id].id;

#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    int ret = sched_setaffinity(0, sizeof(cpuset), &cpuset);
    AZ_ASSERT(ret == 0);
#else
    static RunOnce warn_not_implemented([] { fmt::println("WARN: {}: Not implemented", __func__); });
#endif

    is_set = true;
}

}  // namespace az
