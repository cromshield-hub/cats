#include "libsed/table/acl_ops.h"
#include "libsed/method/method_call.h"
#include "libsed/method/method_result.h"
#include "libsed/method/method_uids.h"
#include "libsed/core/log.h"

namespace libsed {

Result AclOps::getAcl(const Uid& objectUid, const Uid& methodUid,
                       std::vector<Uid>& aceList) {
    TokenEncoder paramEnc;
    paramEnc.encodeUid(methodUid);

    MethodCall call(objectUid, Uid(method::GETACL));
    call.setParams(paramEnc.data());
    auto tokens = call.build();

    MethodResult result;
    auto r = session_.sendMethod(tokens, result);
    if (r.failed()) return r;
    if (!result.isSuccess()) return result.toResult();

    auto stream = result.resultStream();
    while (stream.hasMore()) {
        auto uid = stream.readUid();
        if (uid) aceList.push_back(*uid);
        else break;
    }

    return ErrorCode::Success;
}

Result AclOps::checkAccess(const Uid& objectUid, const Uid& methodUid,
                            const Uid& authority, bool& hasAccess) {
    std::vector<Uid> aceList;
    auto r = getAcl(objectUid, methodUid, aceList);
    if (r.failed()) return r;

    hasAccess = false;
    for (const auto& ace : aceList) {
        if (ace == authority) {
            hasAccess = true;
            break;
        }
    }

    return ErrorCode::Success;
}

} // namespace libsed
