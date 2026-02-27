#pragma once

#include "../../core/types.h"
#include "../../core/error.h"
#include "../../transport/i_transport.h"
#include <memory>
#include <string>

namespace libsed {

/// Opal DataStore table operations
class OpalDataStore {
public:
    OpalDataStore(std::shared_ptr<ITransport> transport, uint16_t comId);

    /// Write data to DataStore table
    Result write(const std::string& password,
                  const uint8_t* data, size_t len,
                  uint32_t tableNumber = 0,
                  uint64_t offset = 0,
                  uint32_t userId = 1);

    /// Read data from DataStore table
    Result read(const std::string& password,
                 Bytes& data,
                 uint32_t tableNumber = 0,
                 uint64_t offset = 0,
                 uint64_t length = 0,
                 uint32_t userId = 1);

private:
    std::shared_ptr<ITransport> transport_;
    uint16_t comId_;
};

} // namespace libsed
