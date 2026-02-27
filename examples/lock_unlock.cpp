/// @file lock_unlock.cpp
/// Example: Lock and unlock an Opal drive

#include <libsed/sed_library.h>
#include <iostream>
#include <string>

void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " <device> <lock|unlock> <password> [range_id] [user_id]\n";
}

int main(int argc, char* argv[]) {
    if (argc < 4) { printUsage(argv[0]); return 1; }

    const std::string device   = argv[1];
    const std::string action   = argv[2];
    const std::string password = argv[3];
    uint32_t rangeId = (argc > 4) ? std::stoul(argv[4]) : 0;
    uint32_t userId  = (argc > 5) ? std::stoul(argv[5]) : 1;

    libsed::initialize();

    auto sed = libsed::SedDevice::open(device);
    if (!sed) {
        std::cerr << "Failed to open device\n";
        return 1;
    }

    libsed::Result r;
    if (action == "lock") {
        std::cout << "Locking range " << rangeId << "...\n";
        r = sed->lockRange(rangeId, password, userId);
    } else if (action == "unlock") {
        std::cout << "Unlocking range " << rangeId << "...\n";
        r = sed->unlockRange(rangeId, password, userId);
    } else {
        printUsage(argv[0]);
        return 1;
    }

    if (r.failed()) {
        std::cerr << "Operation failed: " << r.message() << "\n";
        return 1;
    }

    std::cout << "Success!\n";

    // Show range info
    libsed::LockingRangeInfo info;
    r = sed->getRangeInfo(rangeId, info, password, userId);
    if (r.ok()) {
        std::cout << "Range " << rangeId << " status:\n"
                  << "  ReadLocked:  " << (info.readLocked ? "yes" : "no") << "\n"
                  << "  WriteLocked: " << (info.writeLocked ? "yes" : "no") << "\n";
    }

    libsed::shutdown();
    return 0;
}
