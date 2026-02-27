#pragma once

/// @file i_nvme_device.h
/// @brief Abstract NVMe device interface for Dependency Injection.
///
/// This allows your existing libnvme facade to be injected into the TCG
/// transport layer.  The transport uses only ifSend/ifRecv from this
/// interface, but the EvalApi can access the full INvmeDevice to issue
/// arbitrary NVMe admin/IO commands during TCG evaluation.
///
/// Architecture:
///
///   ┌─────────────────────────────────────────────┐
///   │  Your libnvme (existing facade)             │
///   │  - Identify, Format, Sanitize, FW, etc.     │
///   │  - implements INvmeDevice                    │
///   └─────────────┬───────────────────────────────┘
///                  │ DI (shared_ptr)
///   ┌─────────────▼───────────────────────────────┐
///   │  NvmeTransport (implements ITransport)       │
///   │  - ifSend() → nvme_.securitySend()          │
///   │  - ifRecv() → nvme_.securityRecv()          │
///   │  - nvmeDevice() → exposes INvmeDevice*       │
///   └─────────────┬───────────────────────────────┘
///                  │
///   ┌─────────────▼───────────────────────────────┐
///   │  EvalApi / Session / Discovery              │
///   │  - Uses ITransport for TCG                  │
///   │  - Can get INvmeDevice* for NVMe commands   │
///   └─────────────────────────────────────────────┘

#include "../core/types.h"
#include "../core/error.h"
#include <cstdint>
#include <string>
#include <memory>

namespace libsed {

/// NVMe command completion status
struct NvmeCompletion {
    uint32_t cdw0  = 0;   ///< DW0 of completion
    uint16_t status = 0;   ///< Status Field (SF)
    bool     isError() const { return (status & 0xFFFE) != 0; }
    uint8_t  sct() const { return (status >> 9) & 0x07; }
    uint8_t  sc()  const { return (status >> 1) & 0xFF; }
};

/// Generic NVMe admin command descriptor
struct NvmeAdminCmd {
    uint8_t  opcode   = 0;
    uint32_t nsid     = 0;
    uint32_t cdw2     = 0;
    uint32_t cdw3     = 0;
    uint32_t cdw10    = 0;
    uint32_t cdw11    = 0;
    uint32_t cdw12    = 0;
    uint32_t cdw13    = 0;
    uint32_t cdw14    = 0;
    uint32_t cdw15    = 0;
    void*    data     = nullptr;
    uint32_t dataLen  = 0;
    uint32_t timeoutMs = 30000;
};

/// Generic NVMe IO command descriptor
struct NvmeIoCmd {
    uint8_t  opcode   = 0;
    uint32_t nsid     = 0;
    uint64_t slba     = 0;   ///< Starting LBA
    uint16_t nlb      = 0;   ///< Number of logical blocks - 1
    uint32_t cdw12    = 0;
    uint32_t cdw13    = 0;
    void*    data     = nullptr;
    uint32_t dataLen  = 0;
    uint32_t timeoutMs = 30000;
};

/// Abstract NVMe device interface
/// Your libnvme facade should implement this.
class INvmeDevice {
public:
    virtual ~INvmeDevice() = default;

    // ── Security Protocol (used by ITransport) ──────

    /// Security Send (Admin opcode 0x81)
    virtual Result securitySend(uint8_t protocolId, uint16_t comId,
                                const uint8_t* data, uint32_t dataLen) = 0;

    /// Security Receive (Admin opcode 0x82)
    virtual Result securityRecv(uint8_t protocolId, uint16_t comId,
                                uint8_t* data, uint32_t dataLen,
                                uint32_t& bytesReceived) = 0;

    // ── Generic Admin/IO Commands ───────────────────

    /// Submit arbitrary admin command
    virtual Result adminCommand(NvmeAdminCmd& cmd, NvmeCompletion& cpl) = 0;

    /// Submit arbitrary IO command
    virtual Result ioCommand(NvmeIoCmd& cmd, NvmeCompletion& cpl) = 0;

    // ── Common Admin Commands (convenience) ─────────

    /// Identify Controller (CNS=01)
    virtual Result identify(uint8_t cns, uint32_t nsid,
                            Bytes& data) = 0;

    /// Get Log Page
    virtual Result getLogPage(uint8_t logId, uint32_t nsid,
                              Bytes& data, uint32_t dataLen) = 0;

    /// Get Feature
    virtual Result getFeature(uint8_t featureId, uint32_t nsid,
                              uint32_t& cdw0, Bytes& data) = 0;

    /// Set Feature
    virtual Result setFeature(uint8_t featureId, uint32_t nsid,
                              uint32_t cdw11, const Bytes& data = {}) = 0;

    /// Format NVM
    virtual Result formatNvm(uint32_t nsid, uint8_t lbaf,
                             uint8_t ses = 0, uint8_t pi = 0) = 0;

    /// Sanitize
    virtual Result sanitize(uint8_t action, uint32_t owPass = 0) = 0;

    /// Firmware Download
    virtual Result fwDownload(const Bytes& fwImage, uint32_t offset) = 0;

    /// Firmware Commit/Activate
    virtual Result fwCommit(uint8_t slot, uint8_t action) = 0;

    /// Namespace Management - Create
    virtual Result nsCreate(const Bytes& nsData, uint32_t& nsid) = 0;

    /// Namespace Management - Delete
    virtual Result nsDelete(uint32_t nsid) = 0;

    /// Namespace Attachment
    virtual Result nsAttach(uint32_t nsid, uint16_t controllerId, bool attach) = 0;

    // ── Device Info ─────────────────────────────────

    /// Get the device path (e.g. "/dev/nvme0")
    virtual std::string devicePath() const = 0;

    /// Check if device handle is open
    virtual bool isOpen() const = 0;

    /// Close device handle
    virtual void close() = 0;

    /// Get device file descriptor (Linux) or handle (Windows)
    virtual int fd() const = 0;
};

} // namespace libsed
