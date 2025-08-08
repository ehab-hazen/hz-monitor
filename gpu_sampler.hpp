#pragma once

#include <cstdint>
#include <fstream>
#include <iostream>
#include <nvml.h>
#include <string>
#include <utility>
#include <vector>

class GpuSampler {

  public:
    struct Metric {
        /**
         * @brief GPU utilization
         */
        uint32_t gpu_;
        /**
         * @brief GPU memory usage
         */
        uint32_t memory_;
        uint32_t power_;
        uint32_t temperature_;

        uint32_t graphics_clocks_;
        uint32_t mem_clocks_;
        uint32_t sm_clocks_;
        uint32_t vid_clocks_;

        uint32_t graphics_clock_util_;
        uint32_t mem_clock_util_;
        uint32_t sm_clock_util_;
        uint32_t vid_clock_util_;

        uint32_t encoder_utilization_;
        uint32_t decoder_utilization_;
    };

    struct Metadata {
        std::string name_;
        uint32_t memory_total_;
        uint32_t power_limit_;
        uint32_t sm_max_clock_;
        uint32_t vid_max_clock_;
        uint32_t graphics_max_clock_;
        uint32_t mem_max_clock_;
    };

    using Metrics = std::vector<Metric>;

    GpuSampler() { Init(); }
    GpuSampler(const GpuSampler &) = delete;
    GpuSampler(GpuSampler &&rhs) {
        std::swap(device_count_, rhs.device_count_);
        std::swap(devices_, rhs.devices_);
        std::swap(metadata_, rhs.metadata_);
    }

    ~GpuSampler() { Shutdown(); }

    uint32_t DeviceCount() const { return device_count_; }

    const Metadata &GetDeviceMetadataByIndex(uint32_t index) const {
        return metadata_[index];
    }

    void LogMetadata(std::ofstream &fout) const {
        for (uint32_t i = 0; i < DeviceCount(); ++i) {
            const auto &meta = GetDeviceMetadataByIndex(i);
            fout << "GPU " << i << ":\n\t" << meta.name_
                 << "\n\tMemory: " << meta.memory_total_ << " MB"
                 << "\n\tPower Limit: " << meta.power_limit_ << " mW"
                 << "\n\tMax GPU Clock: " << meta.graphics_max_clock_ << " MHz"
                 << "\n\tMax SM Clock: " << meta.sm_max_clock_ << " MHz"
                 << "\n\tMax Video Clock: " << meta.vid_max_clock_ << " MHz"
                 << "\n\tMax Mem Clock: " << meta.mem_max_clock_ << " MHz\n";
        }
    }

    Metrics Sample() const {
        Metrics metrics;

        for (uint32_t i = 0; i < device_count_; ++i) {
            Metric metric;
            nvmlUtilization_t utilization;
            uint32_t ignore;

            // Temperature & Power
            nvmlDeviceGetTemperature(devices_[i], NVML_TEMPERATURE_GPU,
                                     &metric.temperature_);
            nvmlDeviceGetPowerUsage(devices_[i], &metric.power_);

            // SM compute & VRAM utilization
            nvmlDeviceGetUtilizationRates(devices_[i], &utilization);
            metric.gpu_ = utilization.gpu;
            metric.memory_ = utilization.memory;

            // Video Encoder & Decoder utilization
            nvmlDeviceGetEncoderUtilization(
                devices_[i], &metric.encoder_utilization_, &ignore);
            nvmlDeviceGetDecoderUtilization(
                devices_[i], &metric.decoder_utilization_, &ignore);

            // clock frequencies
            nvmlDeviceGetClockInfo(devices_[i], NVML_CLOCK_GRAPHICS,
                                   &metric.graphics_clocks_);
            nvmlDeviceGetClockInfo(devices_[i], NVML_CLOCK_MEM,
                                   &metric.mem_clocks_);
            nvmlDeviceGetClockInfo(devices_[i], NVML_CLOCK_VIDEO,
                                   &metric.vid_clocks_);
            nvmlDeviceGetClockInfo(devices_[i], NVML_CLOCK_SM,
                                   &metric.sm_clocks_);

            // clock utilization
            metric.sm_clock_util_ =
                metric.sm_clocks_ / metadata_[i].sm_max_clock_;
            metric.mem_clock_util_ =
                metric.mem_clocks_ / metadata_[i].mem_max_clock_;
            metric.vid_clock_util_ =
                metric.vid_clocks_ / metadata_[i].vid_max_clock_;
            metric.graphics_clock_util_ =
                metric.graphics_clocks_ / metadata_[i].graphics_max_clock_;

            metrics.push_back(metric);
        }

        return metrics;
    }

  private:
    uint32_t device_count_;
    std::vector<nvmlDevice_t> devices_;
    std::vector<Metadata> metadata_;

    void Init() {
        nvmlReturn_t result;

        // Initialize nvml
        result = nvmlInit();
        if (result != NVML_SUCCESS) {
            std::cerr << "Failed to initialize NVML: "
                      << nvmlErrorString(result) << "\n";
            return Shutdown();
        }

        // Get GPU count
        result = nvmlDeviceGetCount(&device_count_);
        if (result != NVML_SUCCESS) {
            printf("Failed to query device count: %s\n",
                   nvmlErrorString(result));
            return Shutdown();
        }

        // Get handles and metadata for all devices
        for (uint32_t i = 0; i < device_count_; ++i) {
            nvmlDevice_t device;
            result = nvmlDeviceGetHandleByIndex(i, &device);
            if (result != NVML_SUCCESS) {
                printf("Failed to get handle for device %i: %s\n", i,
                       nvmlErrorString(result));
                return Shutdown();
            }

            devices_.push_back(device);
            metadata_.push_back(GetDeviceMetadata(device));
        }
    }

    Metadata GetDeviceMetadata(nvmlDevice_t device) {
        Metadata meta{};
        char name_buf[64];
        nvmlDeviceGetName(device, name_buf, sizeof(name_buf));
        meta.name_ = name_buf;

        nvmlMemory_t mem;
        nvmlDeviceGetMemoryInfo(device, &mem);
        meta.memory_total_ = mem.total / 1024 / 1024;

        nvmlDeviceGetPowerManagementLimit(device, &meta.power_limit_);

        nvmlDeviceGetMaxClockInfo(device, NVML_CLOCK_GRAPHICS,
                                  &meta.graphics_max_clock_);
        nvmlDeviceGetMaxClockInfo(device, NVML_CLOCK_MEM, &meta.mem_max_clock_);
        nvmlDeviceGetMaxClockInfo(device, NVML_CLOCK_VIDEO,
                                  &meta.vid_max_clock_);
        nvmlDeviceGetMaxClockInfo(device, NVML_CLOCK_SM, &meta.sm_max_clock_);

        return meta;
    }

    void Shutdown() {
        nvmlReturn_t result = nvmlShutdown();
        if (result != NVML_SUCCESS) {
            printf("Failed to shutdown NVML: %s\n", nvmlErrorString(result));
        }
    }
};
