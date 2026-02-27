#pragma once

/// @file SedLibrary.h
/// Master include for TCG SED Library

#include "version.h"
#include "core/types.h"
#include "core/error.h"
#include "core/uid.h"
#include "core/log.h"
#include "sed_device.h"
#include "transport/transport_factory.h"
#include "discovery/discovery.h"

namespace libsed {

/// Initialize the library (call once at startup)
void initialize();

/// Shutdown and cleanup
void shutdown();

/// Get library version string
const char* versionString();

} // namespace libsed
