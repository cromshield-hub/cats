#pragma once

#include "../core/types.h"
#include "../core/error.h"
#include "../session/session.h"
#include <vector>

namespace libsed {

/// ACE/ACL operations utility
class AclOps {
public:
    explicit AclOps(Session& session) : session_(session) {}

    /// Get the ACL for a method on an object
    Result getAcl(const Uid& objectUid, const Uid& methodUid,
                  std::vector<Uid>& aceList);

    /// Check if an authority has access to a method
    Result checkAccess(const Uid& objectUid, const Uid& methodUid,
                       const Uid& authority, bool& hasAccess);

private:
    Session& session_;
};

} // namespace libsed
