/// @file enterprise_band.cpp
/// Example: Enterprise SSC band management

#include <libsed/sed_library.h>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <device> <band_id> <password> [lock|unlock|info]\n";
        return 1;
    }

    const std::string device   = argv[1];
    uint32_t bandId            = std::stoul(argv[2]);
    const std::string password = argv[3];
    const std::string action   = (argc > 4) ? argv[4] : "info";

    libsed::initialize();

    auto sed = libsed::SedDevice::open(device);
    if (!sed) {
        std::cerr << "Failed to open device\n";
        return 1;
    }

    auto* ent = sed->asEnterprise();
    if (!ent) {
        std::cerr << "Device is not Enterprise SSC\n";
        return 1;
    }

    libsed::Result r;

    if (action == "lock") {
        r = ent->lockBand(password, bandId);
        std::cout << (r.ok() ? "Band locked" : "Lock failed") << "\n";
    } else if (action == "unlock") {
        r = ent->unlockBand(password, bandId);
        std::cout << (r.ok() ? "Band unlocked" : "Unlock failed") << "\n";
    } else {
        libsed::enterprise::BandInfo info;
        r = ent->band().getBandInfo(password, bandId, info);
        if (r.ok()) {
            std::cout << "Band " << bandId << ":\n"
                      << "  Start:  " << info.rangeStart << "\n"
                      << "  Length: " << info.rangeLength << "\n"
                      << "  Locked: " << (info.locked ? "yes" : "no") << "\n";
        }
    }

    if (r.failed()) {
        std::cerr << "Error: " << r.message() << "\n";
        return 1;
    }

    libsed::shutdown();
    return 0;
}
