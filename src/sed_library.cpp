#include "libsed/sed_library.h"
#include "libsed/core/log.h"

namespace libsed {

void initialize() {
    LIBSED_INFO("TCG SED Library %s initialized", LIBSED_VERSION_STRING);
}

void shutdown() {
    LIBSED_INFO("TCG SED Library shutdown");
}

const char* versionString() {
    return LIBSED_VERSION_STRING;
}

} // namespace libsed
