// eval_api_internal.h — Shared helper for split EvalApi implementation files
#pragma once

#include "libsed/eval/eval_api.h"
#include "libsed/method/method_result.h"

namespace libsed {
namespace eval {

// Internal helper: send method on session, capture raw payloads and status.
//
// Session::sendMethod 가 method-level status 를 Result 로 propagate 하도록
// 변경됨에 따라, transportError/protocolError 의 의미를 분리해서 캡처한다:
//   - transportError: 순수 I/O 또는 token parse 실패 (method status 미확정)
//   - protocolError : TCG method-level 실패 (NotAuthorized/SpBusy/...)
static inline Result sendMethod(Session& session, const Bytes& methodTokens, RawResult& raw) {
    raw.rawSendPayload = methodTokens;

    auto r = session.sendMethod(methodTokens, raw.methodResult);

    if (raw.methodResult.isSuccess()) {
        // 성공 시 r 도 Success. 혹시 IO/parse 단계 실패라면 transportError 캡처.
        raw.transportError = r.code();
    } else {
        // method-level 실패 — r 이 methodResult.toResult() 와 동일.
        raw.protocolError = r.code();
    }

    return r;
}

} // namespace eval
} // namespace libsed
