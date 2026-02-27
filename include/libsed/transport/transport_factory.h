#pragma once

#include "i_transport.h"
#include <memory>
#include <string>

namespace libsed {

/// Factory that detects drive type and creates appropriate transport
class TransportFactory {
public:
    /// Auto-detect drive type and create transport
    /// @param devicePath  e.g. "/dev/sda", "/dev/nvme0n1", "\\\\.\\PhysicalDrive0"
    static std::shared_ptr<ITransport> create(const std::string& devicePath);

    /// Create a specific transport type
    static std::shared_ptr<ITransport> createAta(const std::string& devicePath);
    static std::shared_ptr<ITransport> createNvme(const std::string& devicePath);
    static std::shared_ptr<ITransport> createScsi(const std::string& devicePath);

    /// Detect the transport type for a device
    static TransportType detect(const std::string& devicePath);

    /// Enumerate available SED-capable devices
    struct DeviceInfo {
        std::string path;
        TransportType type;
        std::string model;
        std::string serial;
    };

    static std::vector<DeviceInfo> enumerateDevices();
};

} // namespace libsed
