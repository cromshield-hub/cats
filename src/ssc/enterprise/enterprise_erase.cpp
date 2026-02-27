#include "libsed/ssc/enterprise/enterprise_erase.h"
#include "libsed/ssc/enterprise/enterprise_session.h"
#include "libsed/ssc/enterprise/enterprise_defs.h"
#include "libsed/table/table_ops.h"
#include "libsed/core/log.h"

namespace libsed {

EnterpriseErase::EnterpriseErase(std::shared_ptr<ITransport> transport, uint16_t comId)
    : transport_(transport), comId_(comId) {}

Result EnterpriseErase::eraseBand(const std::string& password, uint32_t bandId,
                                    bool asEraseMaster) {
    EnterpriseSession session(transport_, comId_);
    Result r;

    if (asEraseMaster) {
        r = session.openAsEraseMaster(password);
    } else {
        r = session.openAsBandMaster(bandId, password);
    }
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid bandUid = enterprise::makeBandUid(bandId);
    r = ops.erase(bandUid);

    session.close();
    LIBSED_INFO("Enterprise erase band %u: %s", bandId, r.ok() ? "success" : "failed");
    return r;
}

Result EnterpriseErase::cryptoErase(const std::string& eraseMasterPassword,
                                      uint32_t bandId) {
    EnterpriseSession session(transport_, comId_);
    auto r = session.openAsEraseMaster(eraseMasterPassword);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid bandUid = enterprise::makeBandUid(bandId);
    r = ops.genKey(bandUid);

    session.close();
    return r;
}

Result EnterpriseErase::eraseAll(const std::string& eraseMasterPassword) {
    EnterpriseSession session(transport_, comId_);
    auto r = session.openAsEraseMaster(eraseMasterPassword);
    if (r.failed()) return r;

    TableOps ops(*session.session());

    // Erase band 0 (global)
    r = ops.erase(enterprise::makeBandUid(0));
    if (r.failed()) {
        LIBSED_WARN("Failed to erase band 0");
    }

    session.close();
    LIBSED_INFO("Enterprise erase all: %s", r.ok() ? "success" : "partial");
    return r;
}

} // namespace libsed
