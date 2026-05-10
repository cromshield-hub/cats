# TC Authoring Guide

> **Audience:** TC Developer
> **Prereq:** `tcg_sed_primer.md` ch.1–3, examples 01–06 직접 실행
> **See also:** `cookbook.md` (정형 레시피), `eval_platform_guide.md` (와이어 레벨), `test_scenarios.md` (104 시나리오 카탈로그)

이 문서는 *libsed 위에서* 새로운 테스트 케이스(TC)를 작성하려는 개발자
를 위한 가이드다. 아래 한 줄을 채울 수 있게 한다.

> "방금 테스트하고 싶은 시나리오가 떠올랐다. 어디서부터 시작하는가?"

빌더가 아니라 **작성자**의 시점이다. libsed 자체를 수정하는 사람은
`internal/hammurabi_code.md`, `internal/postmortem_sedutil_compat.md`,
`internal/work_history.md` 를 본다.

---

## 1. TC 한 개를 작성하는 다섯 단계

```
1) Intent 정의   — 무엇을 검증하는가? 왜?
2) 환경 선택     — Mock / Sim / Real 중 무엇이 적합한가?
3) 시나리오 작성 — scenarioIntent + step + EXPECT_*
4) 검증          — ctest / 직접 실행 / 와이어 캡처
5) 회귀 등록     — test_scenarios.md 한 줄 추가
```

각 단계는 다음 절에서 자세히 본다. 이 다섯 단계를 한 번 거친 TC 는
*"내일의 나" 를 포함해 누가 보든 같은 의미로 읽힌다.* 그것이 목적이다.

---

## 2. 환경 선택 — Mock vs Sim vs Real

가장 자주 묻는 질문. 답은 *무엇을 검증하고 싶은가* 에 따라 다르다.

| 검증 목적 | 선택 | 위치 |
|-----------|------|------|
| "내 layer 가 **올바른 바이트** 를 보내는가?" | **Mock** | `tests/mock/mock_transport.h` |
| "프로토콜 흐름이 **끝까지 도는가**?" | **Sim** | `include/libsed/transport/sim_transport.h` |
| "실제 펌웨어가 **같은 답** 을 주는가?" | **Real** | `tools/cats-cli`, `examples/*` 하드웨어 모드 |

세 환경은 **신뢰도 위계** 가 정해져 있다 — Real > Sim > Mock. 그러나
*속도/비용/위험성* 의 위계는 정확히 반대 — Mock > Sim > Real.

### 2.1 Mock — 큐 기반, 빠름, 응답 수동 작성

```cpp
auto mock = std::make_shared<MockTransport>();
mock->queueDiscoveryResponse(SscType::Opal20);   // 헬퍼 사용
// 그 외에는 Bytes 를 손으로 만들어 mock->queueRecv(bytes) 로 추가

EvalApi api;
DiscoveryInfo info;
EXPECT_OK(api.discovery0(mock, info));
```

- 강점: 1 ms 미만 실행, deterministic, 의도치 않은 외부 상태 없음.
- 한계: **MockTransport 에는 프로토콜 로직이 없다.** 세션 상태도, ACE
  도, SP 라이프사이클도 시뮬하지 않는다. 응답 한 개 한 개를 호출자가
  큐에 직접 넣어야 한다. 따라서 Mock 으로 검증할 수 있는 것은
  *"libsed 가 이 시점에 이 입력을 받았을 때 이 바이트를 보내는가"*
  뿐이다.
- 적합: 인코더 회귀, single-method 의 input/output 매핑, 새 method
  추가 시 byte sanity 같은 좁은 단위 검증.

### 2.2 Sim — TPer 시뮬레이터, 중간 속도, 프로토콜 끝까지

```cpp
auto sim = std::make_shared<SimTransport>();
SedDrive drive(sim);
EXPECT_OK(drive.query());
EXPECT_OK(drive.takeOwnership("test_pw"));
```

- 강점: 응답을 큐에 넣을 필요 없음 — `SimTransport` 가 Discovery / Properties /
  Session / Get / Set / Authenticate / Activate / Revert / GenKey /
  CryptoErase 를 **상태 기반으로** 시뮬레이션한다. ACE/Authority/CPIN/
  Range/MBR/DataStore 모두 내부 모델 보유. `recursive_mutex` 로
  thread-safe.
- 한계: 시뮬레이터가 *우리가 짠 spec 해석* 을 따른다 — 따라서
  byte-identity 검증에는 부적합 (정의상 tautology). 또한 펌웨어 별
  비표준 행동을 재현하지 않는다.
- 적합: 멀티스텝 흐름의 의미론 검증 (`takeOwnership` 후 같은 비번
  재호출 시 idempotent? 다른 비번 재호출 시 `AlreadyOwnedDifferentCredential`?),
  세션 lifecycle, 권한 분리, 부정 케이스 (ACE 거부) 의 빠른 검증.

`SimConfig` 로 SSC type, ComID, maxPackets, ranges, users, dataStore
등 조정 가능. `tests/scenarios/test_sim_basic.cpp` 가 모범 사용 예.

### 2.3 Real — 실 하드웨어, 느림, **데이터 위험**

- 강점: ground truth. 같은 흐름이 실 펌웨어에서 통과하는가가 최종
  답.
- 한계: 1) 데이터 손실 위험 (revert/erase 류는 destructive), 2) 잠금
  위험 (TryLimit 누적), 3) 환경 의존성 (드라이브 모델별 행동 차이),
  4) 느림.
- 적합: golden fixture 캡처 (`tests/fixtures/golden/*.bin`), Tier 1
  최종 회귀, 신규 명령의 `golden_validator` 통과 확인.

**가드레일:**

- `confirmDestructive(opts, "...")` 를 통과하지 않으면 destructive 호출
  금지. examples/example_common.h 가 이 helper 를 제공하며 `--force`
  플래그가 없으면 stdin 확인을 받는다.
- 새 비번을 set 한 직후 즉시 *같은 비번으로 auth* 가 통과하는지
  확인하는 round-trip 을 같은 세션에서 끼워라. 그래야 PSID Revert
  가 필요해지기 전에 이상을 알아챈다.
- **단일 도구로 lifecycle 일관 유지** — cats 로 set 한 드라이브를
  sedutil 로 auth 하지 말 것. 이유는 LAW 21 / `postmortem_sedutil_
  compat.md` Theme 4 참조.

### 2.4 결정 흐름

```
새 TC 를 짜야 한다.
  │
  ├─ "특정 메서드의 송신 바이트가 정확한가?"
  │     → Mock. 빠르고 충분.
  │
  ├─ "여러 메서드를 묶은 흐름의 의미론이 맞는가?"
  │  "ACE/권한 거부가 잘 작동하는가?"
  │     → Sim. SimTransport 만 있으면 끝까지 돈다.
  │
  ├─ "정말 펌웨어가 받는가? byte-identical 인가?"
  │     → Real. 단, golden fixture 캡처 + 가드레일 동반.
  │
  └─ "위 셋 다 해야 한다."
        → 동일 시나리오를 Mock + Sim + Real 세 변형으로 작성.
          각각 `TS_<level>_<id>_*` 처럼 식별자만 분리.
```

---

## 3. 표준 패턴 — `scenarioIntent()` / `stepExpect()`

cats 의 example 과 scenario 는 **자기-문서화** 가 강제된다. 다음
패턴은 코드를 읽지 않고 출력만 봐도 의도를 이해할 수 있게 한다.

### 3.1 `scenarioIntent()` — 시나리오 헤더

```cpp
scenarioIntent(1, "Take Ownership",
    { "factory-state 에서 SID 비번을 MSID -> NEW 로 교체하고",
      "옛 MSID 가 거부되는지도 함께 검증." },
    { "익명 세션 OK",
      "MSID 읽기 OK",
      "옛 MSID 거부 (NEGATIVE — NotAuthorized 가 정답)" });
```

출력:

```
── Scenario 1: Take Ownership ──

  Intent:   factory-state 에서 SID 비번을 MSID -> NEW 로 교체하고
            옛 MSID 가 거부되는지도 함께 검증.
  Expected: 1) 익명 세션 OK
            2) MSID 읽기 OK
            3) 옛 MSID 거부 (NEGATIVE — NotAuthorized 가 정답)
```

**규칙:**
- 모든 시나리오는 `scenarioIntent()` 로 시작한다. (단순 `scenario()`
  는 더 이상 권장되지 않는다.)
- Intent 는 *왜* 이 시나리오를 짜는지를 두 줄 안에 적는다.
- Expected 는 단계 결과를 *번호로* 적는다. 부정 케이스는 항목 끝에
  "(NEGATIVE — ... 가 정답)" 같은 인라인 라벨을 붙인다.

### 3.2 `step()` vs `stepExpect()` — 단계 결과 출력

```cpp
step(1, "open anonymous session", api.startSession(...));
                                       // OK / FAIL 만

stepExpect(6, "old MSID rejected", Expect::Failure,
           api.startSession(/* 옛 비번 */));
                                       // expect FAIL 일 때 PASS, 거절 사유 출력
```

**규칙:**
- positive flow 단계는 `step()` 으로 충분.
- 부정 케이스 (실패가 정답) 는 **반드시** `stepExpect(..., Expect::Failure, ...)`
  를 쓴다. `step()` 으로 부정 케이스를 적으면 출력이 `[Step N] ... FAIL`
  로 찍혀서 "버그인지 의도된 실패인지" 헷갈린다 — 이건 실제로
  이번 sedutil 비교 세션에서 사용자가 가장 강하게 피드백한 부분이다.
  postmortem Theme 3c (status 삼킴) 와 같은 가족.
- positive 케이스에 `stepExpect(..., Expect::Success, ...)` 를 쓰는
  것도 허용된다 (의도가 더 분명해짐).

### 3.3 부정 케이스의 Expected 라벨

`Expected:` 항목에 부정 케이스가 섞여 있으면 *반드시* 인라인 라벨을
붙여라. 그렇지 않으면 출력의 `[expect FAIL] PASS` 가 무엇을 뜻하는지
독자가 모른다.

---

## 4. 시나리오 템플릿

새 시나리오 파일은 다음 골격을 복붙한 뒤 4 영역만 채우면 통과한다.

```cpp
/// @file test_my_scenario.cpp
/// @brief <한 줄 요약 — 이 파일이 무엇을 검증하는가>

#include "test_helper.h"
#include "libsed/transport/sim_transport.h"   // Sim 환경 사용 시

using namespace libsed;
using namespace libsed::test;
using namespace libsed::uid;
using namespace libsed::eval;

// ── (A) 시나리오 1 ─────────────────────────────────────
TEST_SCENARIO(MY_SUITE, TS_001_What_It_Checks) {
    // (B) Setup — transport / EvalApi / state
    EvalApi api;
    auto sim = std::make_shared<SimTransport>();

    // (C) Act — 실 호출
    DiscoveryInfo info;
    auto r = api.discovery0(sim, info);

    // (D) Assert — EXPECT_* / CHECK_*
    EXPECT_OK(r);
    CHECK_EQ(info.primarySsc, SscType::Opal20);
    return true;
}

// ── 추가 시나리오는 같은 패턴으로 ──
```

`tests/scenarios/scenario_main.cpp` 가 `RUN_SCENARIO(MY_SUITE, TS_001_*)`
를 호출하도록 등록하면 ctest 가 자동 발견한다.

**채워야 하는 4 영역:**

| 영역 | 의미 |
|------|------|
| (A) | 시나리오 식별자 — `TS_<level>_<id>_<purpose>` 패턴 권장 |
| (B) | 환경 setup — Mock / Sim / Real 중 §2 의 결정 따라 |
| (C) | 검증 대상 호출 — 한 메서드든 여러 메서드든 |
| (D) | EXPECT_OK / EXPECT_FAIL / EXPECT_ERR / CHECK_EQ / CHECK_GT 등 |

**예제(예시 출력 포함) 가 필요하면 example 파일** (예: `examples/05_take_
ownership.cpp`) 을 본다. 이 파일은 시나리오를 `main()` 안에서 직접 출력
하는 형태로, intent/expected 패턴이 가장 풍부하게 적용되어 있다.

---

## 5. 흔한 실수 (anti-pattern → fix)

이번 sedutil 비교 세션 + 그 이전 시리즈에서 반복된 실수들. **각 항목은
실제로 발생해서 fix 된 것** 이다.

### 5.1 세션을 close 하지 않고 종료
```cpp
// BAD
api.startSession(s, uid::SP_ADMIN, false, ssr);
api.getMsid(s, msid);
return;   // 세션 leak — 다음 시나리오에서 SpBusy 재현

// GOOD
api.startSession(s, uid::SP_ADMIN, false, ssr);
api.getMsid(s, msid);
api.closeSession(s);   // 또는 RAII: composite::withSession(...)
```
SP 의 상태는 메서드 경계 너머에 살아남는다 (postmortem Theme 6).

### 5.2 method status 를 Result 로만 체크
```cpp
// BAD — wire OK 인데 method 가 NotAuthorized 인 경우 OK 로 보임
auto r = session.sendMethod(...);
if (r.ok()) { /* 사실은 method 가 실패했을 수 있음 */ }
```
`Session::sendMethod` 는 이번 세션부터 `result.toResult()` 를
propagate 한다 (postmortem Theme 3c). 그래도 method-level 의미가
중요한 곳에서는 `MethodResult` 를 직접 받아 `result.status` 를 보는
편이 명시적이다.

### 5.3 Discovery 응답을 매번 손코딩
```cpp
// BAD
mock->queueRecv(myHandcraftedDiscoveryBytes);

// GOOD
mock->queueDiscoveryResponse(SscType::Opal20);
```
`MockTransport` 가 minimal Opal/Pyrite/Enterprise discovery 응답을
이미 만들어준다.

### 5.4 패스워드를 hardcode
```cpp
// BAD
api.takeOwnership(transport, "TestSIDPassword123");

// GOOD
api.takeOwnership(transport, getPassword(opts));
```
`getPassword(opts)` 는 `--password` CLI → `SED_PASSWORD` env →
default 순으로 해결한다 (`example_common.h`).

### 5.5 destructive 호출에 가드레일 누락
```cpp
// BAD
api.psidRevert(transport, psid);   // 사용자 동의 없음

// GOOD
if (!confirmDestructive(opts, "PSID Revert (DESTROYS ALL DATA)")) return 0;
api.psidRevert(transport, psid);
```

### 5.6 도구 혼용 (cats + sedutil 같은 드라이브)
**Real 환경에서만 해당.** cats 로 set 한 SID 비번을 sedutil 로
auth 하지 말 것 (또는 그 반대). 해시 알고리즘이 다르다 → AUTH_FAIL
누적 → 잠김 → PSID Revert (데이터 파괴). 도구 간 호환이 필요하면
`HashPassword::sedutilHash()` + `Bytes` overload 의 명시적 경로를
쓴다. 자세한 내용은 LAW 21.

---

## 6. 디버깅 플레이북

TC 가 실패했다. 무엇을 보는가? 다음 결정 트리를 따른다.

```
실패 발생
  │
  ├─ 1) 출력 모드 격상
  │     example/tool 에 --dump (decoded) 또는 --dump2 (decoded+hex) 추가
  │     → 토큰 단위로 어디서 깨졌는지 확인
  │
  ├─ 2) hex 캡처가 있으면 packet_decode
  │     ./build/packet_decode <hexfile>
  │     → ComPacket/Packet/SubPacket 헤더 + 토큰 트리 출력
  │
  ├─ 3) 비번 관련 의심
  │     ./build/pwhash <password>
  │     ./build/pwhash --sedutil --serial <SN> <password>
  │     → cats SHA-256 vs sedutil PBKDF2-HMAC-SHA1 출력 비교
  │
  ├─ 4) 인코더 단위 sanity (sedutil 손코딩 ref 와 비교)
  │     ./build/tests/ioctl_validator
  │     → 17 unit tests + 5 sequences. 회귀 검출의 빠른 첫 관문.
  │     주의: PASS 가 결정적 증거 아님 (LAW 17). 같은 misreading 가능.
  │
  ├─ 5) 실 하드웨어와의 차이가 결정적
  │     tests/fixtures/golden/*.bin 에 캡처가 있으면
  │     ./build/tests/golden_validator
  │     → 실 HW byte vs libsed byte (TSN/HSN 마스킹). 결정적.
  │
  └─ 6) 위 단계로도 안 잡히면
        rosetta_stone.md 의 해당 토큰/메서드 절을 다시 읽고,
        hammurabi_code.md 의 LAW 들로 자가 점검 (특히 LAW 4, 5, 6, 16).
```

**진단 도구의 역할:**

| 도구 | 입력 | 출력 |
|------|------|------|
| `--dump` / `--dump2` | example 실행 | 토큰 / hex 레벨 와이어 |
| `tools/packet_decode` | hex string | 파싱된 ComPacket + 토큰 트리 |
| `tools/pwhash` | 평문 (+ optional salt) | cats SHA-256 / sedutil PBKDF2 hash |
| `tests/ioctl_validator` | (자체 내장 시나리오) | libsed vs sedutil 손코딩 ref byte diff (sanity) |
| `tests/golden_validator` | (자체 내장 시나리오) | libsed vs 실 HW fixture byte diff (decisive) |

자세한 내부 도구는 `internal/postmortem_sedutil_compat.md` Theme 7
참조.

---

## 7. 회귀 등록

새 TC 가 통과하면 다음 두 곳에 흔적을 남긴다.

### 7.1 `docs/test_scenarios.md` 에 한 줄 추가
104-시나리오 카탈로그. Level 1~6 분류:

- L1 — 단위 메서드
- L2 — 시퀀스
- L3 — cross-feature
- L4 — negative / error
- L5 — advanced (multi-session, fault injection, …)
- L6 — real-device only

식별자 패턴 `TS_<L><sub>_<id>_<purpose>` 을 따른다. 예: `TS_2A_006`.

### 7.2 `tests/scenarios/test_L<N>_*.cpp` 에 등록 + ctest 자동 발견

- 같은 Level 의 기존 파일에 `TEST_SCENARIO(L<N>, TS_<id>_*)` 로 추가.
- `tests/scenarios/scenario_main.cpp` 에 `RUN_SCENARIO(...)` 한 줄
  추가.
- `cd build && ctest` 가 자동으로 잡는다.

새 *example* 을 추가하는 경우 (TC 라기보다 학습 자료) 는 `examples/`
에 `<NN>_<topic>.cpp` 를 만들고 `examples/CMakeLists.txt` 에
`add_example(<NN>_<topic>)` 한 줄 추가, 그리고 `docs/examples.md` 의
표에 한 줄 추가.

---

## 8. 더 읽기

- `cookbook.md` — 11 개 정형 레시피 (idempotent ops, SpBusy retry,
  error layering 등). TC 의 **공통 부품** 소스로 활용.
- `eval_platform_guide.md` — 와이어 레벨 통제, fault injection,
  threading.
- `examples.md` — 20 examples 의 전체 학습 트랙. 각 example 이 TCG spec
  의 어떤 부분을 가르치는지의 매핑.
- `internal/postmortem_sedutil_compat.md` — 이번 세션에서 드러난
  7 테마. *왜 Sim 만으로는 부족한가, 왜 Real 캡처가 결정적인가* 의
  근거.
- `internal/hammurabi_code.md` — 21 LAW. TC 작성 중 byte-emitting code
  를 직접 만지게 되면 통독 권장.
