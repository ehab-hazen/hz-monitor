#pragma once

#include <cstdint>
#include <fstream>
#include <limits>
#include <sched.h>
#include <utility>

class RamSampler {

  public:
    RamSampler() = delete;

    RamSampler(pid_t pid) : pid_(pid) {}
    RamSampler(const RamSampler &rhs) : pid_(rhs.pid_) {}
    RamSampler &operator=(const RamSampler &rhs) {
        pid_ = rhs.pid_;
        return *this;
    }
    RamSampler(RamSampler &&rhs) { std::swap(pid_, rhs.pid_); }
    RamSampler &operator=(RamSampler &&rhs) {
        std::swap(pid_, rhs.pid_);
        return *this;
    }

    uint64_t Sample() const {
        std::ifstream status("/proc/" + std::to_string(pid_) + "/status");
        if (!status.is_open()) {
            return static_cast<uint64_t>(0); // process likely exited
        }

        std::string key;
        uint64_t value;
        std::string unit;
        while (status >> key) {
            if (key == "VmRSS:") {
                status >> value >> unit; // value in kB
                return value;
            }
            status.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
        return 0;
    }

  private:
    pid_t pid_;
};
