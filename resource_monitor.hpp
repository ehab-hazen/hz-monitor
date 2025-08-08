#pragma once

#include "cpu_ram_sampler.hpp"
#include "gpu_sampler.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <ios>
#include <nvml.h>
#include <sys/resource.h>
#include <tuple>

class ResourceMonitor {

  public:
    using CpuRamMetrics = std::vector<CpuRamSampler::Metrics>;
    using GpuMetrics = std::vector<GpuSampler::Metrics>;
    using Metrics = std::tuple<CpuRamMetrics, GpuMetrics>;

    ResourceMonitor() = delete;
    ResourceMonitor(const ResourceMonitor &) = delete;
    /**
     * @brief Moving an instance will not move the already taken measurements
     * had that instance been already running
     */
    ResourceMonitor(ResourceMonitor &&rhs) {
        refresh_rate_.store(rhs.refresh_rate_.load());
    }
    ResourceMonitor(uint32_t refresh_rate, const std::string &usage_file)
        : refresh_rate_(refresh_rate), usage_file_(usage_file) {}

    void SetRefreshRate(uint32_t refresh_rate) {
        refresh_rate_.store(refresh_rate);
    }

    void Run(const std::atomic<bool> &stop) const {
        std::ofstream fout(usage_file_, std::ios::out);

        WriteCsvHeader(fout);

        uint32_t i = 0;
        while (!stop.load()) {
            // Wait before the first measurement to warm the caches.
            const uint32_t warmup_preiod_ms = 1000 / refresh_rate_;
            std::this_thread::sleep_for(
                std::chrono::milliseconds(warmup_preiod_ms));

            CpuRamSampler::Metrics cpu_ram_sample = cpu_ram_sampler_.Sample();
            GpuSampler::Metrics gpu_sample = gpu_sampler_.Sample();

            const uint64_t timestamp_ms =
                (1000.0 * i / refresh_rate_) + warmup_preiod_ms;
            WriteSample(fout, timestamp_ms, cpu_ram_sample, gpu_sample);
            ++i;
        }

        fout.close();
        return;
    }

    void LogMetadata(std::ofstream &fout) const {
        cpu_ram_sampler_.LogMetadata(fout);
        fout << "\n";
        gpu_sampler_.LogMetadata(fout);
    }

    uint32_t GpuDeviceCount() const { return gpu_sampler_.DeviceCount(); }

    const GpuSampler::Metadata &GetGpuMetadata(uint32_t index) const {
        return gpu_sampler_.GetDeviceMetadataByIndex(index);
    }

  private:
    /**
     * @brief number of measurements per second
     */
    std::atomic<uint32_t> refresh_rate_;
    std::string usage_file_;

    CpuRamSampler cpu_ram_sampler_;
    GpuSampler gpu_sampler_;

    void WriteCsvHeader(std::ofstream &fout) const {
        fout << "timestamp_ms,cpu_user_ms,cpu_sys_ms";
        for (uint32_t c = 0; c < cpu_ram_sampler_.CpuCount(); ++c) {
            fout << ",cpu" << c << "_usage";
        }
        fout << ",ram_kib";
        for (uint32_t g = 0; g < gpu_sampler_.DeviceCount(); ++g) {
            fout << ",gpu" << g << "_util"
                 << ",gpu" << g << "_mem"
                 << ",gpu" << g << "_enc_util"
                 << ",gpu" << g << "_dec_util"
                 << ",gpu" << g << "_temp"
                 << ",gpu" << g << "_power"
                 << ",gpu" << g << "_gpu_clock"
                 << ",gpu" << g << "_mem_clock"
                 << ",gpu" << g << "_sm_clock"
                 << ",gpu" << g << "_vid_clock"
                 << ",gpu" << g << "_gpu_clock_util"
                 << ",gpu" << g << "_mem_clock_util"
                 << ",gpu" << g << "_sm_clock_util"
                 << ",gpu" << g << "_vid_clock_util";
        }
        fout << "\n";
    }

    void WriteSample(std::ofstream &out, const uint64_t timestamp_ms,
                     const CpuRamSampler::Metrics &cpu_sample,
                     const GpuSampler::Metrics &gpu_sample) const {

        uint64_t cpu_user_ms = cpu_sample.usage_.ru_utime.tv_sec * 1000 +
                               cpu_sample.usage_.ru_utime.tv_usec / 1000;
        uint64_t cpu_sys_ms = cpu_sample.usage_.ru_stime.tv_sec * 1000 +
                              cpu_sample.usage_.ru_stime.tv_usec / 1000;

        out << timestamp_ms << "," << cpu_user_ms << "," << cpu_sys_ms;

        for (const auto &core : cpu_sample.hardware_threads_) {
            out << "," << core.usage;
        }

        uint64_t ram_kb = cpu_sample.usage_.ru_maxrss;
        out << "," << ram_kb;

        for (const auto &g : gpu_sample) {
            out << "," << g.gpu_ << "," << g.memory_ << ","
                << g.encoder_utilization_ << "," << g.decoder_utilization_
                << "," << g.temperature_ << "," << g.power_ << ","
                << g.graphics_clocks_ << "," << g.mem_clocks_ << ","
                << g.sm_clocks_ << "," << g.vid_clocks_ << ","
                << g.graphics_clock_util_ << "," << g.mem_clock_util_ << ","
                << g.sm_clock_util_ << "," << g.vid_clock_util_;
        }

        out << "\n";
    }
};
