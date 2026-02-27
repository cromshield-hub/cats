#include "libsed/ssc/enterprise/enterprise_auth.h"
#include "libsed/ssc/enterprise/enterprise_session.h"
#include "libsed/table/table_ops.h"
#include "libsed/core/uid.h"
#include "libsed/core/log.h"
#include "libsed/security/hash_password.h"

namespace libsed {

EnterpriseAuth::EnterpriseAuth(std::shared_ptr<ITransport> transport, uint16_t comId)
    : transport_(transport), comId_(comId) {}

Result EnterpriseAuth::setBandMasterPassword(const std::string& currentPassword,
                                                const std::string& newPassword,
                                                uint32_t bandId) {
    EnterpriseSession session(transport_, comId_);
    auto r = session.openAsBandMaster(bandId, currentPassword);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid cpinUid(0x0000000B00008000ULL + bandId); // C_PIN for BandMasterN
    Bytes newPin = HashPassword::passwordToBytes(newPassword);
    r = ops.setPin(cpinUid, newPin);

    session.close();
    return r;
}

Result EnterpriseAuth::setEraseMasterPassword(const std::string& currentPassword,
                                                 const std::string& newPassword) {
    EnterpriseSession session(transport_, comId_);
    auto r = session.openAsEraseMaster(currentPassword);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid cpinUid(0x0000000B00008401ULL); // C_PIN for EraseMaster
    Bytes newPin = HashPassword::passwordToBytes(newPassword);
    r = ops.setPin(cpinUid, newPin);

    session.close();
    return r;
}

Result EnterpriseAuth::verifyBandMaster(const std::string& password, uint32_t bandId) {
    EnterpriseSession session(transport_, comId_);
    auto r = session.openAsBandMaster(bandId, password);
    session.close();
    return r;
}

Result EnterpriseAuth::verifyEraseMaster(const std::string& password) {
    EnterpriseSession session(transport_, comId_);
    auto r = session.openAsEraseMaster(password);
    session.close();
    return r;
}

} // namespace libsed
