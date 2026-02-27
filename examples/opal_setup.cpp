/// @file opal_setup.cpp
/// Example: Full initial Opal setup (take ownership + activate + configure)

#include <libsed/sed_library.h>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <device> <new_sid_password>\n";
        return 1;
    }

    const std::string device = argv[1];
    const std::string sidPassword = argv[2];

    libsed::initialize();

    auto sed = libsed::SedDevice::open(device);
    if (!sed) {
        std::cerr << "Failed to open device\n";
        return 1;
    }

    auto* opal = sed->asOpal();
    if (!opal) {
        std::cerr << "Device is not Opal\n";
        return 1;
    }

    // 1. Take ownership
    std::cout << "[1/4] Taking ownership...\n";
    auto r = opal->takeOwnership(sidPassword);
    if (r.failed()) {
        std::cerr << "Take ownership failed: " << r.message() << "\n";
        return 1;
    }

    // 2. Activate Locking SP
    std::cout << "[2/4] Activating Locking SP...\n";
    r = opal->activateLockingSP(sidPassword);
    if (r.failed()) {
        std::cerr << "Activate failed: " << r.message() << "\n";
        return 1;
    }

    // 3. Set Admin1 password (same as SID for simplicity)
    std::cout << "[3/4] Setting Admin1 password...\n";
    r = opal->user().setAdmin1Password(sidPassword, sidPassword);
    if (r.failed()) {
        std::cerr << "Set Admin1 password failed: " << r.message() << "\n";
        return 1;
    }

    // 4. Enable global locking
    std::cout << "[4/4] Enabling global locking...\n";
    r = opal->locking().setLockEnabled(sidPassword, 0, true, true);
    if (r.failed()) {
        std::cerr << "Enable locking failed: " << r.message() << "\n";
        return 1;
    }

    std::cout << "Opal setup complete!\n";
    libsed::shutdown();
    return 0;
}
