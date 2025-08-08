#pragma once

#include <cstdint>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

/**
 * @brief Implementation class for sampling usage of a single hardware thread
 * (HART)
 */
class HartSampler {

  public:
    struct Metric {
        std::string id;
        uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
        double usage;

        uint64_t Active() const {
            return user + nice + system + irq + softirq + steal;
        }

        uint64_t Total() const { return Active() + idle + iowait; }
    };

    using Metrics = std::vector<Metric>;

    HartSampler(const HartSampler &) = delete;
    HartSampler(HartSampler &&) = delete;
    HartSampler()
        : cpu_count_(std::thread::hardware_concurrency()),
          last_sample_(ReadCpuStats()) {}

    Metrics Sample() const {
        Metrics current = ReadCpuStats();
        std::vector<double> usages = ComputeCoreUsage(last_sample_, current);
        for (size_t i = 0; i < usages.size(); ++i) {
            current[i].usage = usages[i];
        }
        last_sample_ = std::move(current);
        return last_sample_;
    }

    uint32_t CpuCount() const { return cpu_count_; }

  private:
    uint32_t cpu_count_;
    mutable Metrics last_sample_;

    std::vector<double> ComputeCoreUsage(const std::vector<Metric> &a,
                                         const std::vector<Metric> &b) const {
        std::vector<double> usage;
        for (size_t i = 0; i < a.size(); ++i) {
            uint64_t total_diff = b[i].Total() - a[i].Total();
            uint64_t active_diff = b[i].Active() - a[i].Active();
            double percent =
                total_diff == 0 ? 0.0 : 100.0 * active_diff / total_diff;
            usage.push_back(percent);
        }
        return usage;
    }

    Metrics ReadCpuStats() const {
        std::ifstream stat("/proc/stat");
        std::string line;
        std::vector<Metric> cores;

        while (std::getline(stat, line)) {
            if (line.rfind("cpu", 0) == 0 && line[3] != ' ') {
                std::istringstream ss(line);
                Metric snap;
                ss >> snap.id >> snap.user >> snap.nice >> snap.system >>
                    snap.idle >> snap.iowait >> snap.irq >> snap.softirq >>
                    snap.steal;
                cores.push_back(snap);
            }
        }
        return cores;
    }
};
