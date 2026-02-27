#pragma once

#include "../../core/types.h"
#include "../../core/error.h"
#include "../../transport/i_transport.h"
#include <memory>
#include <string>

namespace libsed {

/// Enterprise erase operations
class EnterpriseErase {
public:
    EnterpriseErase(std::shared_ptr<ITransport> transport, uint16_t comId);

    /// Erase a specific band (requires EraseMaster or BandMaster)
    Result eraseBand(const std::string& password, uint32_t bandId,
                      bool asEraseMaster = true);

    /// Crypto erase (generate new key for a band)
    Result cryptoErase(const std::string& eraseMasterPassword, uint32_t bandId);

    /// Erase all bands
    Result eraseAll(const std::string& eraseMasterPassword);

private:
    std::shared_ptr<ITransport> transport_;
    uint16_t comId_;
};

} // namespace libsed
