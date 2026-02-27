#pragma once

/// @file Debug.h
/// @brief Master include for the TCG SED Debug / Test Layer.
///
/// Include this single header to get access to:
///   - TestContext (singleton, global config/fault/workaround/trace)
///   - FaultBuilder (fluent fault rule construction)
///   - TestSession  (RAII scoped debug session)
///   - All macros (LIBSED_CHECK_FAULT, LIBSED_WA_ACTIVE, etc.)

#include "libsed/debug/test_context.h"
#include "libsed/debug/fault_builder.h"
#include "libsed/debug/test_session.h"
