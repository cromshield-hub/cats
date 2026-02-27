#pragma once

#include "../../core/types.h"
#include "../../core/error.h"
#include "../../transport/i_transport.h"
#include "../../discovery/discovery.h"
#include "opal_defs.h"
#include "opal_admin.h"
#include "opal_locking.h"
#include "opal_user.h"
#include "opal_mbr.h"
#include "opal_data_store.h"
#include <memory>
#include <string>

namespace libsed {

/// High-level Opal device facade
/// Provides a unified API for all Opal operations
class OpalDevice {
public:
    OpalDevice(std::shared_ptr<ITransport> transport,
               uint16_t comId,
               const DiscoveryInfo& info);

    // ── Discovery Info ───────────────────────────────
    const DiscoveryInfo& info() const { return info_; }

    // ── Admin Operations ─────────────────────────────
    OpalAdmin& admin() { return admin_; }

    Result takeOwnership(const std::string& newSidPassword,
                          const std::string& msidPassword = "") {
        return admin_.takeOwnership(newSidPassword, msidPassword);
    }

    Result activateLockingSP(const std::string& sidPassword) {
        return admin_.activateLockingSP(sidPassword);
    }

    Result revertTPer(const std::string& sidPassword) {
        return admin_.revertTPer(sidPassword);
    }

    Result psidRevert(const std::string& psidPassword) {
        return admin_.psidRevert(psidPassword);
    }

    // ── Locking Operations ───────────────────────────
    OpalLocking& locking() { return locking_; }

    Result setupRange(const std::string& admin1Password,
                       uint32_t rangeId,
                       uint64_t rangeStart, uint64_t rangeLength) {
        return locking_.configureRange(admin1Password, rangeId,
                                        rangeStart, rangeLength);
    }

    Result lock(const std::string& userPassword,
                uint32_t rangeId, uint32_t userId = 1) {
        return locking_.lock(userPassword, rangeId, userId);
    }

    Result unlock(const std::string& userPassword,
                  uint32_t rangeId, uint32_t userId = 1) {
        return locking_.unlock(userPassword, rangeId, userId);
    }

    // ── User Operations ──────────────────────────────
    OpalUser& user() { return user_; }

    Result setupUser(const std::string& admin1Password,
                      uint32_t userId,
                      const std::string& userPassword,
                      uint32_t rangeId) {
        auto r = user_.enableUser(admin1Password, userId);
        if (r.failed()) return r;
        r = user_.setUserPassword(admin1Password, userId, userPassword, true);
        if (r.failed()) return r;
        return user_.assignUserToRange(admin1Password, userId, rangeId);
    }

    // ── MBR Operations ───────────────────────────────
    OpalMbr& mbr() { return mbr_; }

    // ── DataStore Operations ─────────────────────────
    OpalDataStore& dataStore() { return dataStore_; }

    /// Full initial setup: takeOwnership + activate + configure
    Result initialSetup(const std::string& sidPassword,
                         const std::string& admin1Password);

private:
    std::shared_ptr<ITransport> transport_;
    uint16_t comId_;
    DiscoveryInfo info_;

    OpalAdmin admin_;
    OpalLocking locking_;
    OpalUser user_;
    OpalMbr mbr_;
    OpalDataStore dataStore_;
};

} // namespace libsed
