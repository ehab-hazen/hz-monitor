#include "resource_monitor.hpp"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <future>
#include <ios>
#include <iostream>
#include <sched.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

uint32_t parse_refresh_rate(int argc, char **argv) {
    uint32_t refresh_rate = 2; // default

    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--refresh_rate=", 15) == 0) {
            refresh_rate = std::stoul(argv[i] + 15);
            // remove from argv to avoid passing to execvp
            for (int j = i; j < argc - 1; ++j)
                argv[j] = argv[j + 1];
            argv[argc - 1] = nullptr;
            --argc;
            break;
        }
    }

    return refresh_rate;
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <program> [args...] [--refresh_rate=N]\n";
        return 1;
    }

    uint32_t refresh_rate = parse_refresh_rate(argc, argv);

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return 1;
    }

    if (pid == 0) {
        execvp(argv[1], &argv[1]);
        perror("exec failed");
        return 1;
    } else {
        std::atomic<bool> stop = false;
        auto fut = std::async(std::launch::async, [&stop, refresh_rate] {
            ResourceMonitor resource_monitor(refresh_rate, "metrics.csv");
            std::ofstream fout("monitor.log", std::ios::out);
            resource_monitor.LogMetadata(fout);
            fout.close();
            resource_monitor.Run(stop);
        });

        int status = 0;
        waitpid(pid, &status, 0);
        stop.store(true);
        return 0;
    }
}
