/// @file sed_discover.cpp
/// CLI tool: Discover and list SED-capable drives

#include <libsed/sed_library.h>
#include <libsed/transport/transport_factory.h>
#include <iostream>
#include <iomanip>

int main(int argc, char* argv[]) {
    libsed::initialize();

    if (argc < 2) {
        // Enumerate all devices
        std::cout << "Scanning for SED-capable drives...\n\n";
        auto devices = libsed::TransportFactory::enumerateDevices();

        if (devices.empty()) {
            std::cout << "No block devices found. Run with a device path: "
                      << argv[0] << " /dev/sda\n";
            return 0;
        }

        for (const auto& dev : devices) {
            std::cout << dev.path;
            if (!dev.model.empty()) std::cout << "  " << dev.model;
            std::cout << "  [";
            switch (dev.type) {
                case libsed::TransportType::ATA:  std::cout << "ATA"; break;
                case libsed::TransportType::NVMe: std::cout << "NVMe"; break;
                case libsed::TransportType::SCSI: std::cout << "SCSI"; break;
                default: std::cout << "?"; break;
            }
            std::cout << "]\n";
        }
    } else {
        // Discover specific device
        auto device = libsed::SedDevice::open(argv[1]);
        if (!device) {
            std::cerr << "Failed to discover " << argv[1] << "\n";
            return 1;
        }

        const auto& info = device->discovery();

        std::cout << "Device: " << argv[1] << "\n";
        std::cout << "SSC:    ";
        switch (info.primarySsc) {
            case libsed::SscType::Opal20:     std::cout << "Opal 2.0"; break;
            case libsed::SscType::Opal10:     std::cout << "Opal 1.0"; break;
            case libsed::SscType::Enterprise: std::cout << "Enterprise"; break;
            case libsed::SscType::Pyrite10:   std::cout << "Pyrite 1.0"; break;
            case libsed::SscType::Pyrite20:   std::cout << "Pyrite 2.0"; break;
            default: std::cout << "Not supported"; break;
        }
        std::cout << "\nComID:  0x" << std::hex << std::setw(4) << std::setfill('0')
                  << info.baseComId << std::dec
                  << "\nLocking: " << (info.lockingEnabled ? "enabled" : "disabled")
                  << " (" << (info.locked ? "locked" : "unlocked") << ")\n";
    }

    libsed::shutdown();
    return 0;
}
