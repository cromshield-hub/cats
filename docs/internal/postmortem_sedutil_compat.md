# Postmortem — sedutil Wire Compatibility (2026-04 ~ 2026-05)

> **Audience:** Library Maintainer · Wire-level Eval Engineer
> **Prereq:** `tcg_sed_primer.md` (프로토콜 기본), `rosetta_stone.md` §1~§3 (토큰 인코딩)
> **See also:** `hammurabi_code.md` (이 문서가 도출한 21개 법칙), `work_history.md` (시간순 changelog)

이 문서는 신규 개발자(특히 libsed 를 이어받아 수정하게 될 사람)가
**커밋 하나하나 추적하지 않고도 "왜 libsed 가 지금 이런 모양인지"** 를
이해할 수 있도록 쓴다. 시간순 changelog 가 아닌 *주제별 narrative* 다.
세부 commit hash, 라인 번호, 파일 위치는 `work_history.md` 에 있고
이 문서는 의도적으로 그것들을 거의 인용하지 않는다.

---

## TL;DR

- **무엇이 일어났나** — 2026-04 중반 ~ 2026-05 초까지, libsed 가 만들어
  내는 와이어 바이트를 sedutil-cli 의 그것과 한 바이트 단위로 비교하면
  서, 7개 큰 주제에 걸쳐 **"spec 만 보고 짠 코드"의 함정**을 차례로
  드러냈다. 그 결과 `hammurabi_code.md` 에 21 개의 immutable law 가
  쌓였고, 검증 인프라는 `sed_compare` 에서 `golden_validator` 로 한 단계
  더 내려갔으며, `Session::sendMethod` 의 의미론, 패스워드 해시 정책,
  ownership idempotency 등 **라이브러리 외형이 아니라 의미론**이 여러
  곳에서 바뀌었다.

- **왜 중요한가** — TCG 드라이브에서 와이어 바이트가 단 하나라도 다르면
  AUTH_FAIL 이 누적되고, 임계값을 넘기면 SID/Owner authority 가 잠긴다.
  대부분의 경우 **PSID Revert 외엔 복구 수단이 없으며 PSID Revert 는
  데이터와 키를 모두 파괴**한다. sedutil 호환성은 단순한 "그쪽도 같이
  지원"이 아니라 **데이터 손실 방지의 마지막 방어선**이다.

- **영향 범위** — 8 commit, 21 LAW, 1 신규 ErrorCode
  (`AlreadyOwnedDifferentCredential = 603`), 신규 진단 도구 3 종 (`pwhash`,
  `packet_decode`, `golden_validator`), encoder/decoder **비대칭 계약** 도입,
  intent-aware step output 패턴, examples 05/14/18/19/22/23 흐름 정리.

---

## Background — sedutil 과의 byte-identity 가 왜 ground truth 인가

신규 개발자 입장에서 가장 먼저 이해해야 할 것은 다음 한 줄이다.

> **TCG Core Spec 을 읽고 짠 코드는 그 자체로는 정답이 아니다.
> 정답은 "실 펌웨어가 받아주는 바이트"이며, 그것을 이미 검증해놓은
> 참조 구현이 sedutil-cli 다.**

이 입장이 왜 필요한지를 알아야 다음에 등장할 7개 테마가 이해된다.

### Spec 만으로는 부족한 이유

1. **펌웨어별 해석 차이**. TCG Core Spec 은 같은 의미를 표현하는 여러
   합법 인코딩을 허용한다. 그러나 실제 펌웨어는 그중 **sedutil-cli 가
   주로 쏘는 부분집합** 만 신뢰성 있게 받는다. 예: short atom 의 width
   는 spec 상 0–15 모두 합법이지만, 일부 펌웨어가 0x83 (3-byte) 같은
   non-power-of-2 width 를 송신단에서 거부한다.
2. **Spec 텍스트 자체의 모호함**. 예를 들어 `Get [ Cellblock ]` 의
   CellBlock 이 자기 자신을 `STARTLIST/ENDLIST` 로 감싸야 하는지 / 아닌
   지를 spec 본문만 읽어 결론짓기 어렵다. 두 해석 모두 spec-legal 처럼
   보이지만 한쪽만 펌웨어가 받는다.
3. **동작 순서/idempotency 같은 "spec 외 영역"**. Properties exchange
   를 StartSession 전에 반드시 한 번 해야 한다든가, RevertSP 실패 후
   SP busy 상태가 잔존한다든가 하는 행동은 spec 텍스트에 없다.

### Validation hierarchy

이 세션을 거치면서 굳어진 신뢰도 순서:

```
Level 1 (decisive)  : real hardware capture   (.bin fixture 로 영구 보존)
Level 2 (strong)    : sedutil-cli 소스 코드   (수년간 수십 종 펌웨어로 검증됨)
Level 3 (weak)      : 손코딩 reference         (sed_compare 의 DtaCommand)
Level 4 (untrusted) : spec 텍스트만 읽고 추론
```

**오직 Level 1 만 결정적이다**. Level 3 PASS 는 "내 가정과 내 reference
가 같은 misreading 을 공유했다" 는 뜻일 수 있다 (실제로 두 번 그랬다 —
Theme 2 참조). LAW 17 이 이 위계를 박아둔 이유.

---

## 7 가지 테마

각 테마는 동일한 4단 구조로 적는다 — **증상 → root cause → fix → lesson**.
신규 개발자는 lesson 만 빠르게 훑어도 라이브러리의 현재 모습을 큰
그림으로 이해할 수 있다.

---

### Theme 1 — Encoder / Decoder 비대칭 (Postel's Law)

**증상.** "Encoder 가 보내는 형태와 Decoder 가 받아주는 형태는 같아야
한다" 는 직관적 대칭 가정이 두 개의 버그를 만들었다.
(a) 디코더가 0x83 같은 non-power-of-2 short atom width 를 거부.
(b) 디코더가 `0xFA` 단독 CloseSession 만 인식하고 `SessionManager.
CloseSession()` method-form 응답을 무시 → 일부 펌웨어가 method-form 으로
세션 종료를 알릴 때 session leak.

**Root cause.** "보낼 때 안 쓰면 받을 때도 거절하는 게 일관적" 이라는
**잘못된 일관성** 추구. 실제 환경은 그 반대를 요구한다 — 우리 송신은
"sedutil-cli 가 늘 쏘는 부분집합" 으로 좁히되, 우리 수신은 "TCG Core
Spec 이 허용하는 모든 형태" 를 받아야 한다.

**Fix.** Encoder 는 power-of-2 widths 만 송신, Decoder 는 0x80–0x8F
전 범위 파싱. CloseSession 송신은 0xFA token 단독, Decoder 는
`MethodResult::recvMethodUid() == SM_CLOSE_SESSION` 도 server-initiated
close 로 인식. 그리고 — **이것이 핵심** — `rosetta_stone.md` 와
`hammurabi_code.md` 에 "이건 비대칭이 정상" 임을 명시적으로 박았다.
신규 개발자가 디코더 코드를 보고 "왜 spec 에 없는 것까지 받지?" 라고
오해하고 단순화하지 않도록.

**Lesson.** Postel's law — *"보낼 때는 보수적으로, 받을 때는 관대하게."*
이 원칙은 코드뿐 아니라 **문서와 주석에까지** 박혀 있어야 한다. 코드만
비대칭이고 문서가 대칭이면, 다음 사람이 코드를 "고쳐서" 망가뜨린다.

---

### Theme 2 — 패킷 구조 wrapping 의 함정

**증상.** 두 번에 걸쳐, "spec 텍스트 + 손코딩 reference" 만 가지고 짠
인코더가 진짜 펌웨어를 통과하지 못하는 바이트를 만들어냈다.
(a) `Get` 의 CellBlock 인자가 자기 자신을 `STARTLIST/ENDLIST` 로 감싸야
함에도 그 wrap 을 빼버림 → 0x0F (TPER_MALFUNCTION).
(b) `Object.Set` 에 spec 상 존재하지 않는 empty Where 5 바이트
(`f2 00 f0 f1 f3`) 를 추가 송신 → cats 의 subpacket 이 sedutil 보다
정확히 5 바이트 길어짐.

두 경우 모두 **`sed_compare` 가 매번 PASS 했다.**

**Root cause.** `sed_compare` 의 reference (vendored `DtaCommand`) 가
cats 의 인코더와 **같은 spec 오독을 공유**했다. 같은 오독을 가진 두
구현을 비교하면 결과는 항상 일치한다. 9 일~10 일 동안 false PASS.
진실은 사용자가 실제 sedutil-cli 를 실 하드웨어에서 돌려서 캡처한
hex dump 가 들어온 시점에 비로소 드러났다.

**Fix.** CellBlock 은 inner list wrapping 복원, Object.Set 은 Where 제거.
그러나 더 중요한 변화는 검증 인프라 자체였다. `golden_validator` 를
도입해 `tests/fixtures/golden/*.bin` 에 **실 하드웨어가 토해낸 바이트
그대로** 를 보관하기 시작했고, "sed_compare PASS 만으로는 'validated'
라 부르지 않는다" 라는 규칙을 LAW 17 로 박았다.

**Lesson.** *"구현 A 와 구현 B 가 같은 바이트를 만든다"* 는 *"그 바이트가
정답이다"* 를 의미하지 않는다. 같은 정보원에서 만들어진 두 구현은 같이
틀릴 수 있다. **유일한 ground truth 는 실 하드웨어 캡처** 다. 이것이
신규 개발자가 새 명령을 추가할 때 따라야 할 절차의 핵심이다 — spec 으로
구현 → sed_compare 로 sanity → 실 하드웨어 캡처를 fixture 로 → golden
validator 가 통과해야 비로소 "validated".

---

### Theme 3 — Session Manager 행동 차이

Spec 에 명시되어 있지 않거나 모호하지만 펌웨어가 강제하는 행동들이
한꺼번에 드러났다. 세 가지 갈래.

**(3a) StartSession 전 Properties 누락.** Example 05 가 Discovery 직후
바로 `StartSession(SP_ADMIN, MSID)` 로 진입해, 두 번째 StartSession
(SID + 새 비번) 에서 NotAuthorized 를 받았다. 일부 펌웨어가 "Properties
한 번도 안 한 호스트의 in-session 호출" 자체를 거부한다. → 현재 모든
세션 진입 경로는 Discovery → Properties → StartSession 의 canonical
pattern 을 따른다 (LAW 19).

**(3b) Properties 응답 순서 비대칭.** TPer 가 `TPerProperties` 를
먼저 보내는 펌웨어와 `HostProperties` 를 먼저 보내는 펌웨어가 모두
존재. 이전 파서는 "TPer 가 먼저" 로 가정했다가 다른 펌웨어에서 값을
잘못 추출. → 파서를 **이름 기반 매칭** 으로 재작성. "위치로 파싱하지
말라, 이름으로 파싱하라" (LAW 10).

**(3c) `Session::sendMethod` 의 status 삼킴.** 가장 systemic 한 결함.
Wire 단계가 OK 면 method-level 상태(NotAuthorized, SpBusy, …)와 무관하게
`Result::Success` 를 반환했었다. 그 결과 `~20 개 simplified overload`
와 모든 example 의 step() 출력이 *"패킷은 갔는데 메서드는 실패"* 인
케이스를 OK 로 표시. → `sendMethod` 끝에서 `result.toResult()` 를 반환
하도록 일관화 (`startSession` 과 같은 의미론).

**Lesson.** "Spec 텍스트에 없다" 와 "그래서 자유다" 는 다르다. 펌웨어
호환성을 위해서는 *spec 외 영역의 행동마저도 sedutil 을 모방하는 것이
기본값* 이어야 한다. 그리고 **method-level 결과는 transport-level 결과와
다른 차원**이라는 것을 라이브러리의 모든 호출 경로가 일관되게 표현해야
한다. 그렇지 않으면 example/test/도구가 거짓 PASS 를 누적한다.

---

### Theme 4 — 패스워드 해시 분기 (LAW 21)

**증상.** 사용자가 "cats 와 sedutil 이 SetCPin 시 password payload 를
같은 방식으로 처리하는가?" 라고 물어보았고, **답은 "절대 아니다"** 였다.

| 도구 | 알고리즘 | Salt | Iter | Output | Wire form |
|------|----------|------|------|--------|-----------|
| libsed (default) | SHA-256 | (없음) | 1 | 32 B | `D0 20 [32 B]` |
| sedutil-cli | PBKDF2-HMAC-SHA1 | drive serial / MSID | 75000 | 20 B | `D0 14 [20 B]` |

같은 평문에 대해 **두 알고리즘 결과는 절대 일치하지 않는다.**

**Root cause.** "보내는 바이트가 spec 상 합법" 인지와 "그게 드라이브에
저장된 바이트와 같은지" 는 별개의 질문이다. TCG 드라이브는 어떤 ≥20B
바이트열이든 PIN 으로 저장한다. 그러나 인증 시에는 **저장 시점에
보낸 바이트와 정확히 같은 바이트** 를 다시 보내야 통과한다. cats 로
SID 비번을 set 한 드라이브에 sedutil 이 같은 평문으로 auth 하면 →
mismatch → AUTH_FAIL → 누적 → SID 잠김 → **PSID Revert (데이터 파괴)**
외 복구 불가.

설상가상 — `HashPassword::passwordToBytes` 의 코드 주석은
*"This matches sedutil behavior: sha256(password)"* 라고 적혀 있었다.
이는 **사실과 정반대** 다. unit test (`SedutilDivergence_Sha256VsPbkdf2Sha256`)
는 정확하게 divergence 를 pin 하고 있었지만, 사용자 대상 문서와 코드
주석엔 그 위험이 노출되지 않았다.

**Fix.** Default 알고리즘은 그대로 두었다 (SHA-256). 변경하면 기존
libsed 로 set 된 모든 드라이브를 영구적으로 잠기게 만들기 때문이다.
대신 — 1) 거짓 주석 제거, 2) `HashPassword::sedutilHash()` opt-in 헬퍼
+ `EvalApi::getNvmeSerial()` 추가, 3) 사전 계산된 PIN 을 그대로 보내는
`Bytes` overload 가 `string` overload 와 짝으로 존재함을 명문화,
4) `tools/pwhash` 진단 도구로 cats/sedutil 양쪽 해시를 모두 출력 가능
하게, 5) `examples/23_sedutil_compat_setup.cpp` 가 sedutil-호환 흐름의
참조 구현. 6) LAW 21 신설.

**Lesson.** *"드라이브가 받아준다"* ≠ *"저장된 자격증명과 일치한다"* 다.
그리고 — **단일 도구로 lifecycle 을 일관해라.** cats 로 set 한 드라이브
는 cats 로만, sedutil 로 set 한 드라이브는 sedutil 로만. 도구 간 전환은
사전 계산된 PBKDF2 출력을 `Bytes` overload 로 전달하는 명시적 경로를
거쳐야만 한다. 더 큰 lesson — **코드 주석의 거짓은 unit test 가 정정해
주지 않는다.** 위험은 user-facing 문서, doxygen, CLI help 의 모든
지점에 동시에 노출되어야 한다.

---

### Theme 5 — Revert UID 혼동

**증상.** PSID revert 흐름에서 cats 가 `AdminSP.RevertSP()` (UID 0x0011)
를 호출하고 있었다. sedutil 은 같은 시점에 `AdminSP.Revert()` (0x0202)
를 보낸다. 실 하드웨어에서 cats 는 NotAuthorized 를 받았다.

**Root cause.** Spec 에 두 개의 비슷한 메서드가 있고, 누가 호출하는가
(SID? Owner? PSID?) 와 어떤 SP 에서 호출하는가 (Admin SP? Locking SP?)
에 따라 적절한 메서드가 달라진다. cats 는 PSID 권한으로 LockingSP
revert 가 아니라 *AdminSP 전체 revert* 를 의도하는 흐름에서 잘못된
메서드를 골랐다.

**Fix.** `composite::revertToFactory`, `EvalApi::psidRevert`,
`SedDrive::revert/psidRevert` 모두 0x0202 를 호출하도록 정정.
`include/libsed/eval/eval_api.h` 의 `revert` / `revertSP` doxygen 을
**UID, 권한, @warning, @see** 까지 모두 적어 "이름이 비슷한데 의미가
다른 함수" 임을 호출 시점에 분명히 보이게 했다.

**Lesson.** "이름이 비슷한 함수가 둘 있다" 는 **버그를 유도하는 신호**.
신규 개발자가 IDE 자동완성에서 두 함수를 보고 잘못 고를 수 있다. 이런
함수쌍은 doxygen 에 단순한 한 줄이 아니라 *어느 권한이 부르는지, 어떤
UID 로 가는지, 사촌 함수와 무엇이 다른지* 를 명시해야 한다. — `eval_api.h`
의 revert 계열을 본보기로 삼을 것.

---

### Theme 6 — Ownership Idempotency + SpBusy

**증상.** 두 가지가 쌍으로 발견되었다.
(a) 이미 ownership 이 잡힌 드라이브에 `takeOwnership()` 을 한 번 더 호출
하면 **무한 NotAuthorized 루프**. 라이브러리에 "이미 owned 인지 확인"
경로가 없었다.
(b) RevertSP 가 실패 응답으로 끝나면 SP 가 *busy* 상태로 잔존, 다음
익명 세션 시도가 St=3 (SpBusy) 로 차단. 단일 회 retry 로는 안 풀림.

**Root cause.** 라이브러리가 **메서드 한 번** 의 결과만 책임지고,
**SP 의 살아남는 상태** 에 대한 책임을 호출자에게 떠넘기고 있었다.
Idempotency 는 "같은 호출을 두 번 해도 안전" 인 의미론인데, takeOwnership
은 그것을 만족하지 않았다. 또 SpBusy 는 retry + 짧은 sleep + StackReset
이 sedutil 의 사실상 표준 복구 방식인데 라이브러리는 그 패턴을
helper 로 노출하지 않고 있었다.

**Fix.**

- `composite::takeOwnership` 의 의미론을 명문화:
  - factory 상태 → 정상 take_own
  - 같은 비번으로 owned → `Result::Success` (no-op)
  - **다른** 비번으로 owned → 신규 `ErrorCode::AlreadyOwnedDifferentCredential = 603`
- `composite::withSpBusyRetry` helper 추가 — `MethodSpBusy` 응답 시
  StackReset + 50 ms × 최대 3 회 retry. `takeOwnership` 의 step 2 와
  `revertToFactory` 의 첫 StartSession 에 적용.

**Lesson.** *SP 의 상태는 메서드 경계 너머에 살아남는다.* 그래서
"메서드를 두 번 호출했을 때 무슨 일이 일어나는가" 와 "메서드가 실패한
직후 SP 가 어떤 상태인가" 는 **라이브러리가 책임지는 의미론**이지
호출자의 숙제가 아니다. 새 composite 을 추가할 때 이 두 질문을 매번
점검할 것 — *idempotent 한가? 실패 후 SP 상태를 잘 정리하는가?*

---

### Theme 7 — 검증 인프라가 곧 결함의 원천이었다

**증상.** Theme 2 에서 보았듯, `sed_compare` 만 신뢰한 9 일은 false
PASS 의 9 일이었다. 그리고 hash divergence (Theme 4) 는 unit test 에
박혀 있었지만 user-facing 문서에 없었기 때문에 조용히 잠복해 있었다.
검증 인프라 자체에 빈자리가 있을 때 *"테스트가 통과하니 괜찮다"* 는
가짜 안전감이 만들어진다.

**Root cause.** 검증 채널의 위계가 코드와 문화에 박혀 있지 않았다.
"sed_compare PASS = 검증됨" 이 암묵적 default 였다.

**Fix — 새로 들어간 도구들과 그 순서:**

| 도구 | 역할 | 어떤 결함이 만들어 냈나 |
|------|------|--------------------------|
| `tools/pwhash` | SHA-256 / PBKDF2-HMAC-SHA1 양쪽 출력 + salt 옵션 (`--sedutil`, `--salt-hex`, `--salt-ascii`, `--serial`) | LAW 21 — hash divergence 진단 |
| `tools/packet_decode` | 사용자가 캡처한 hex 스트림 → ComPacket 헤더 + 토큰 트리. 소스 없이도 wire 분석 가능 | Theme 2 의 사용자 캡처 hex 분석 |
| `tests/integration/golden_validator.cpp` | 실 하드웨어 `.bin` fixture vs libsed 출력 byte diff (TSN/HSN 마스킹 포함) | LAW 17 의 시행체 |
| `tests/integration/ioctl_validator.cpp` | 손코딩 reference vs libsed. **Sanity 만**. | 더 이상 결정적이지 않음 (LAW 17) |
| ~~`tools/sed_compare`~~ (2026-05 폐기) | 같은 level-3 채널이 `ioctl_validator` 와 중복 + false PASS 의 진앙이었음 | 폐기 결정 — narrative 본문 Theme 7 참조 |

**Lesson.** *검증 채널이 없는 곳에 결함이 산다.* 같은 가정을 공유한 두
구현은 같이 틀린다. 새 명령/메서드를 추가할 때 — *어떤 위계의 검증을
받았는가?* 를 commit 메시지에 적을 수 있어야 한다. level-3 PASS 는
**충분조건이 아니다**. 신규 개발자가 새 byte-emitting code 를 추가할
때 따라야 할 절차는 LAW 17 의 5 단계 (spec → 손코딩 ref → 실 HW capture
→ golden builder → 둘 다 PASS) 이다.

**후속 정리 (2026-05).** `golden_validator` 가 자리잡은 뒤에는 `tools/
sed_compare/` 의 역할이 `ioctl_validator` 와 중복되었고, false PASS 의
진앙이라는 *위험 신호* 도 동일하게 따라다녔다. *level-3 채널을 두 개
유지하면 false confidence 가 두 배가 된다*. 따라서 `sed_compare/` 트리
(약 1450 LoC, 18 commands) 는 폐기되었다. `third_party/sedutil/` (vendored
DtaCommand 등) 은 `ioctl_validator` 가 계속 사용하므로 그대로 둔다.

---

## 남은 작업 (Tier 1 / 2 / 3)

이 세션이 끝난 시점의 backlog. 신규 개발자가 이어받아 작업을 시작할
지점.

### Tier 1 — 곧 해야 할 것

1. **Fix 적용 후 와이어 캡처 1 회**. 이 세션에 들어간 fix 들이 실
   하드웨어에서 의도대로 동작하는지 확인 (특히 takeOwnership 의 새
   idempotency 경로, Revert(0x0202) 의 St=0, intent-aware step 출력).
2. **`tools/pwhash` 골든 벡터 등록**. 같은 (평문, salt) 에서 cats
   `sedutilHash()` 와 sedutil-cli 의 출력이 byte-identical 임을 한 번
   캡처해 `tests/fixtures/golden/` 에 commit.
3. **Hash compat 정책 결정**. examples 05 default 를 sedutil-compat
   로 전환할지. 현재는 SHA-256 default — cats 와 sedutil 의 SetCPin
   payload 가 byte 다름. 사용자 결정 대기.

### Tier 2 — 미커버 영역

4. `tests/fixtures/golden/` 에 Activate / MBR / setRange / Revert / RevertSP
   의 실 HW 캡처 추가 — 이 항목들이 아직 level-1 검증을 받지 못했다.
5. TPER_MALFUNCTION (0x0F) 의 사용자 가이던스 — power cycle 안내.
   auto-retry 는 부적절.
6. `docs/README.md`, `docs/cookbook.md`, `docs/examples.md` 에 이번
   세션 변경 (idempotency · SpBusy 복구 · Revert vs RevertSP · intent-
   aware step · SM_CLOSE_SESSION 응답) 반영.

### Tier 3 — 인프라

7. Mock transport 의 ACE 시뮬레이션 — RevertSP 같은 권한 버그를
   unit test 에서 조기 검출. SimTransport 에 부분적 ACE 모델 이미 있음
   (`include/libsed/transport/sim_transport.h`).
8. CI 통합 (현재 하드웨어 부재로 보류).
9. examples 22 (libsed default) / 23 (sedutil-compat) 가 byte-identical
   인지 캡처해서 확인 (Tier 1 #2 의 연장).

---

## 부록 — 이 세션이 도출한 LAW (한 줄 요약)

전문은 `hammurabi_code.md` 참조. 여기는 본 포스트모템의 7 테마와
LAW 의 매핑.

| LAW | 한 줄 요약 | 어느 테마에서 나왔나 |
|-----|------------|---------------------|
| 1 | sedutil 와의 byte-identity 가 ground truth. | Background |
| 2 | Encoder 는 power-of-2 widths 만 송신, Decoder 는 spec 전체 수용. | Theme 1 |
| 3 | `Object.Set` 은 Where 없음. `Byte-Table.Set` 만 Where 사용. | Theme 2 |
| 4 | Properties 인코딩 구조 (HostProperties wrapper + 각 pair STARTNAME) 는 신성. | Theme 3 (보조) |
| 5 | `MaxSubpackets` (소문자 'p'). 한 글자 다르면 InvalidParameter. | Theme 3 (보조) |
| 6 | StartSession 의 named param index 는 0, 3, 4. **Not** 5, 1, 2. | Theme 3 (보조) |
| 7 | StackReset 은 Properties 직전 항상. | Theme 3a |
| 8 | ComPacket 최소 2048 B. | Theme 3 (보조) |
| 9 | SM 응답에 CALL 헤더 있음 — 파서가 skip. | Theme 3 (보조) |
| 10 | 응답 파싱은 순서 무관 — 이름으로 매칭. | Theme 3b |
| 11 | CloseSession 송신은 0xFA, 디코더는 method-form 도 인식. | Theme 1 |
| 12 | UID 는 항상 8-byte byte string. | (이 세션 외) |
| 13 | AI 의 "spec fix" 제안은 byte-validation 전엔 신뢰 X. | Theme 2 |
| 14 | `ifRecv` 는 ComPacket.length > 0 까지 polling. | (이 세션 외) |
| 15 | TC 개발자용 단일 헤더 `<libsed/sed_library.h>`. | (이 세션 외) |
| 16 | CellBlock named pairs 는 inner STARTLIST/ENDLIST 로 wrap. | Theme 2 |
| 17 | `golden_validator` (실 HW fixture) > `sed_compare` (손코딩 ref). | Theme 7 |
| 18 | SyncSession TSN ≠ 0 — defensive check. | (이 세션 외) |
| 19 | `exchangeProperties()` 는 모든 세션 시작 전 항상. | Theme 3a |
| 20 | sedutil 의 session-lifecycle 을 정확히 모방, 임의 reset 추가 금지. | Theme 3 (보조) |
| 21 | 패스워드 해시는 sedutil 과 분기 — 도구 혼용 금지, 거짓 주석 금지. | Theme 4 |

**참고** — LAW 12, 14, 15, 18 은 이 세션 이전에 도출되었으나 같은
"sedutil byte-identity" 정신의 일부이므로 표에 포함했다. 본 포스트모템
의 7 테마 narrative 에는 등장하지 않는다.

---

## 후속 자료

- `hammurabi_code.md` — 21 개 LAW 의 전문 (각 LAW 마다 발생 commit /
  파일 / 라인까지 박혀 있음).
- `work_history.md` Session 2026-04-27 ~ 2026-05-07 — 시간순
  changelog. 각 commit 의 정확한 변경점.
- `rosetta_stone.md` §3, §4a, §4e/§4e′, §4g, §8, §10, §12, §14, §15 —
  encoder / decoder 비대칭의 토큰별 표현, hash divergence, validation
  hierarchy.
- `tests/integration/golden_validator.cpp` + `tests/fixtures/golden/` —
  본 세션이 도입한 검증 인프라의 본체.

신규 개발자에게 권하는 읽기 순서:
**이 문서 (Background + 7 Theme + Tier 1) → `hammurabi_code.md` 통독 →
필요 시 `work_history.md` 의 해당 Session 으로 drill-down.**
