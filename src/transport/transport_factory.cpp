#include "libsed/transport/transport_factory.h"
#include "libsed/transport/ata_transport.h"
#include "libsed/transport/nvme_transport.h"
#include "libsed/transport/scsi_transport.h"
#include "libsed/core/log.h"

#include <string>
#include <fstream>
#include <cctype>

#if defined(__linux__) && !defined(__ANDROID__)
#include <sys/stat.h>
#include <dirent.h>
#endif

namespace libsed {

namespace {
#if defined(__linux__) && !defined(__ANDROID__)
    bool pathExists(const std::string& path) {
        struct stat st;
        return ::stat(path.c_str(), &st) == 0;
    }
#endif
} // anonymous

TransportType TransportFactory::detect(const std::string& devicePath) {
#if defined(__linux__) && !defined(__ANDROID__)
    // Heuristic detection based on device path and sysfs
    if (devicePath.find("nvme") != std::string::npos) {
        return TransportType::NVMe;
    }

    // Extract device name (e.g., "sda" from "/dev/sda")
    std::string devName = devicePath;
    auto pos = devName.rfind('/');
    if (pos != std::string::npos) {
        devName = devName.substr(pos + 1);
    }
    // Remove partition number
    while (!devName.empty() && std::isdigit(devName.back())) {
        devName.pop_back();
    }

    // Check if it's SCSI/SATA via sysfs
    std::string sysPath = "/sys/block/" + devName;
    if (pathExists(sysPath)) {
        std::string deviceSysPath = sysPath + "/device";
        if (pathExists(deviceSysPath)) {
            std::string transportPath = deviceSysPath + "/transport";
            if (pathExists(transportPath)) {
                return TransportType::SCSI;
            }
        }

        if (devName.substr(0, 2) == "sd") {
            return TransportType::ATA;
        }
    }

    return TransportType::SCSI;
#else
    (void)devicePath;
    return TransportType::Unknown;
#endif
}

std::shared_ptr<ITransport> TransportFactory::create(const std::string& devicePath) {
    TransportType type = detect(devicePath);

    switch (type) {
        case TransportType::ATA:
            LIBSED_INFO("Creating ATA transport for %s", devicePath.c_str());
            return createAta(devicePath);
        case TransportType::NVMe:
            LIBSED_INFO("Creating NVMe transport for %s", devicePath.c_str());
            return createNvme(devicePath);
        case TransportType::SCSI:
            LIBSED_INFO("Creating SCSI transport for %s", devicePath.c_str());
            return createScsi(devicePath);
        default:
            LIBSED_ERROR("Could not detect transport type for %s", devicePath.c_str());
            return nullptr;
    }
}

std::shared_ptr<ITransport> TransportFactory::createAta(const std::string& devicePath) {
    return std::make_shared<AtaTransport>(devicePath);
}

std::shared_ptr<ITransport> TransportFactory::createNvme(const std::string& devicePath) {
    return std::make_shared<NvmeTransport>(devicePath);
}

std::shared_ptr<ITransport> TransportFactory::createScsi(const std::string& devicePath) {
    return std::make_shared<ScsiTransport>(devicePath);
}

std::vector<TransportFactory::DeviceInfo> TransportFactory::enumerateDevices() {
    std::vector<DeviceInfo> devices;

#if defined(__linux__) && !defined(__ANDROID__)
    const std::string sysBlock = "/sys/block";
    DIR* dir = ::opendir(sysBlock.c_str());
    if (!dir) return devices;

    struct dirent* entry;
    while ((entry = ::readdir(dir)) != nullptr) {
        std::string devName = entry->d_name;

        // Skip . and ..
        if (devName == "." || devName == "..") continue;

        // Skip virtual devices
        if (devName.find("loop") == 0 || devName.find("ram") == 0 ||
            devName.find("dm-") == 0 || devName.find("md") == 0) {
            continue;
        }

        std::string devPath = "/dev/" + devName;

        DeviceInfo info;
        info.path = devPath;
        info.type = detect(devPath);

        // Try to read model from sysfs
        std::string modelPath = sysBlock + "/" + devName + "/device/model";
        std::ifstream modelFile(modelPath);
        if (modelFile.is_open()) {
            std::getline(modelFile, info.model);
            while (!info.model.empty() && std::isspace(info.model.back())) {
                info.model.pop_back();
            }
        }

        // Try to read serial
        std::string serialPath = sysBlock + "/" + devName + "/device/serial";
        std::ifstream serialFile(serialPath);
        if (serialFile.is_open()) {
            std::getline(serialFile, info.serial);
            while (!info.serial.empty() && std::isspace(info.serial.back())) {
                info.serial.pop_back();
            }
        }

        devices.push_back(std::move(info));
    }
    ::closedir(dir);
#endif

    return devices;
}

} // namespace libsed
