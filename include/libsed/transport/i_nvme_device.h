#pragma once

/// @file i_nvme_device.h
/// @brief 의존성 주입을 위한 추상 NVMe 장치 인터페이스
///
/// 기존 libnvme 파사드를 TCG 전송 계층에 주입할 수 있습니다.
/// 전송 계층은 이 인터페이스의 ifSend/ifRecv만 사용하지만,
/// EvalApi는 TCG 평가 중 임의의 NVMe admin/IO 명령을 실행하기 위해
/// 전체 INvmeDevice에 접근할 수 있습니다.
///
/// Architecture:
///
///   ┌─────────────────────────────────────────────┐
///   │  Your libnvme (existing facade)             │
///   │  - Identify, Format, Sanitize, FW, etc.     │
///   │  - implements INvmeDevice                    │
///   └─────────────┬───────────────────────────────┘
///                  │ DI (shared_ptr)
///   ┌─────────────▼───────────────────────────────┐
///   │  NvmeTransport (implements ITransport)       │
///   │  - ifSend() → nvme_.securitySend()          │
///   │  - ifRecv() → nvme_.securityRecv()          │
///   │  - nvmeDevice() → exposes INvmeDevice*       │
///   └─────────────┬───────────────────────────────┘
///                  │
///   ┌─────────────▼───────────────────────────────┐
///   │  EvalApi / Session / Discovery              │
///   │  - Uses ITransport for TCG                  │
///   │  - Can get INvmeDevice* for NVMe commands   │
///   └─────────────────────────────────────────────┘

#include "../core/types.h"
#include "../core/error.h"
#include <cstdint>
#include <string>
#include <memory>

namespace libsed {

/// @brief NVMe 명령 완료 상태
struct NvmeCompletion {
    uint32_t cdw0  = 0;   ///< 완료 큐 항목의 DW0
    uint16_t status = 0;   ///< 상태 필드 (SF)

    /// @brief 에러 여부 확인
    /// @return 상태 코드에 에러가 있으면 true
    bool     isError() const { return (status & 0xFFFE) != 0; }

    /// @brief 상태 코드 타입 (SCT) 반환
    /// @return SCT 값 (3비트)
    uint8_t  sct() const { return (status >> 9) & 0x07; }

    /// @brief 상태 코드 (SC) 반환
    /// @return SC 값 (8비트)
    uint8_t  sc()  const { return (status >> 1) & 0xFF; }
};

/// @brief 범용 NVMe Admin 명령 디스크립터
struct NvmeAdminCmd {
    uint8_t  opcode   = 0;      ///< Admin 명령 opcode
    uint32_t nsid     = 0;      ///< 네임스페이스 식별자
    uint32_t cdw2     = 0;      ///< 명령 DW2
    uint32_t cdw3     = 0;      ///< 명령 DW3
    uint32_t cdw10    = 0;      ///< 명령 DW10
    uint32_t cdw11    = 0;      ///< 명령 DW11
    uint32_t cdw12    = 0;      ///< 명령 DW12
    uint32_t cdw13    = 0;      ///< 명령 DW13
    uint32_t cdw14    = 0;      ///< 명령 DW14
    uint32_t cdw15    = 0;      ///< 명령 DW15
    void*    data     = nullptr; ///< 데이터 버퍼 포인터
    uint32_t dataLen  = 0;      ///< 데이터 버퍼 길이 (바이트)
    uint32_t timeoutMs = 30000; ///< 명령 타임아웃 (밀리초)
};

/// @brief 범용 NVMe I/O 명령 디스크립터
struct NvmeIoCmd {
    uint8_t  opcode   = 0;      ///< I/O 명령 opcode
    uint32_t nsid     = 0;      ///< 네임스페이스 식별자
    uint64_t slba     = 0;      ///< 시작 LBA (Starting Logical Block Address)
    uint16_t nlb      = 0;      ///< 논리 블록 수 - 1 (Number of Logical Blocks - 1)
    uint32_t cdw12    = 0;      ///< 명령 DW12
    uint32_t cdw13    = 0;      ///< 명령 DW13
    void*    data     = nullptr; ///< 데이터 버퍼 포인터
    uint32_t dataLen  = 0;      ///< 데이터 버퍼 길이 (바이트)
    uint32_t timeoutMs = 30000; ///< 명령 타임아웃 (밀리초)
};

/// @brief 추상 NVMe 장치 인터페이스
/// 사용자의 libnvme 파사드가 이 인터페이스를 구현해야 합니다.
class INvmeDevice {
public:
    virtual ~INvmeDevice() = default;

    // ── Security Protocol (used by ITransport) ──────

    /// @brief Security Send 명령 실행 (Admin opcode 0x81)
    /// @param protocolId  보안 프로토콜 번호
    /// @param comId       ComID
    /// @param data        전송할 데이터 포인터
    /// @param dataLen     전송할 데이터 길이 (바이트)
    /// @return 명령 실행 결과
    virtual Result securitySend(uint8_t protocolId, uint16_t comId,
                                const uint8_t* data, uint32_t dataLen) = 0;

    /// @brief Security Receive 명령 실행 (Admin opcode 0x82)
    /// @param protocolId     보안 프로토콜 번호
    /// @param comId          ComID
    /// @param data           수신 데이터를 저장할 버퍼 포인터
    /// @param dataLen        버퍼 크기 (바이트)
    /// @param bytesReceived  실제 수신된 바이트 수
    /// @return 명령 실행 결과
    virtual Result securityRecv(uint8_t protocolId, uint16_t comId,
                                uint8_t* data, uint32_t dataLen,
                                uint32_t& bytesReceived) = 0;

    // ── Generic Admin/IO Commands ───────────────────

    /// @brief 임의의 Admin 명령 제출
    /// @param cmd  Admin 명령 디스크립터 (입출력)
    /// @param cpl  완료 상태 (출력)
    /// @return 명령 실행 결과
    virtual Result adminCommand(NvmeAdminCmd& cmd, NvmeCompletion& cpl) = 0;

    /// @brief 임의의 I/O 명령 제출
    /// @param cmd  I/O 명령 디스크립터 (입출력)
    /// @param cpl  완료 상태 (출력)
    /// @return 명령 실행 결과
    virtual Result ioCommand(NvmeIoCmd& cmd, NvmeCompletion& cpl) = 0;

    // ── Common Admin Commands (convenience) ─────────

    /// @brief 컨트롤러/네임스페이스 식별 (CNS 값으로 지정)
    /// @param cns   CNS(Controller or Namespace Structure) 값
    /// @param nsid  네임스페이스 식별자
    /// @param data  식별 데이터를 저장할 버퍼 (출력)
    /// @return 명령 실행 결과
    virtual Result identify(uint8_t cns, uint32_t nsid,
                            Bytes& data) = 0;

    /// @brief 로그 페이지 읽기
    /// @param logId    로그 페이지 식별자
    /// @param nsid     네임스페이스 식별자
    /// @param data     로그 데이터를 저장할 버퍼 (출력)
    /// @param dataLen  요청 데이터 길이 (바이트)
    /// @return 명령 실행 결과
    virtual Result getLogPage(uint8_t logId, uint32_t nsid,
                              Bytes& data, uint32_t dataLen) = 0;

    /// @brief Feature 값 읽기
    /// @param featureId  Feature 식별자
    /// @param nsid       네임스페이스 식별자
    /// @param cdw0       반환된 CDW0 값 (출력)
    /// @param data       Feature 데이터를 저장할 버퍼 (출력)
    /// @return 명령 실행 결과
    virtual Result getFeature(uint8_t featureId, uint32_t nsid,
                              uint32_t& cdw0, Bytes& data) = 0;

    /// @brief Feature 값 설정
    /// @param featureId  Feature 식별자
    /// @param nsid       네임스페이스 식별자
    /// @param cdw11      설정할 CDW11 값
    /// @param data       설정할 Feature 데이터 (선택 사항)
    /// @return 명령 실행 결과
    virtual Result setFeature(uint8_t featureId, uint32_t nsid,
                              uint32_t cdw11, const Bytes& data = {}) = 0;

    /// @brief NVM 포맷 실행
    /// @param nsid  네임스페이스 식별자
    /// @param lbaf  LBA 포맷 인덱스
    /// @param ses   Secure Erase 설정 (기본값: 0)
    /// @param pi    Protection Information (기본값: 0)
    /// @return 명령 실행 결과
    virtual Result formatNvm(uint32_t nsid, uint8_t lbaf,
                             uint8_t ses = 0, uint8_t pi = 0) = 0;

    /// @brief Sanitize 명령 실행
    /// @param action  Sanitize 액션 값
    /// @param owPass  Overwrite 패스 횟수 (기본값: 0)
    /// @return 명령 실행 결과
    virtual Result sanitize(uint8_t action, uint32_t owPass = 0) = 0;

    /// @brief 펌웨어 이미지 다운로드
    /// @param fwImage  펌웨어 이미지 데이터
    /// @param offset   다운로드 오프셋 (바이트)
    /// @return 명령 실행 결과
    virtual Result fwDownload(const Bytes& fwImage, uint32_t offset) = 0;

    /// @brief 펌웨어 커밋 (활성화)
    /// @param slot    펌웨어 슬롯 번호
    /// @param action  커밋 액션 값
    /// @return 명령 실행 결과
    virtual Result fwCommit(uint8_t slot, uint8_t action) = 0;

    /// @brief 네임스페이스 생성
    /// @param nsData  네임스페이스 생성 데이터
    /// @param nsid    생성된 네임스페이스 식별자 (출력)
    /// @return 명령 실행 결과
    virtual Result nsCreate(const Bytes& nsData, uint32_t& nsid) = 0;

    /// @brief 네임스페이스 삭제
    /// @param nsid  삭제할 네임스페이스 식별자
    /// @return 명령 실행 결과
    virtual Result nsDelete(uint32_t nsid) = 0;

    /// @brief 네임스페이스 연결/분리
    /// @param nsid          네임스페이스 식별자
    /// @param controllerId  컨트롤러 식별자
    /// @param attach        true이면 연결, false이면 분리
    /// @return 명령 실행 결과
    virtual Result nsAttach(uint32_t nsid, uint16_t controllerId, bool attach) = 0;

    // ── Device Info ─────────────────────────────────

    /// @brief 장치 경로 반환
    /// @return 장치 경로 문자열 (예: "/dev/nvme0")
    virtual std::string devicePath() const = 0;

    /// @brief 장치 핸들 열림 상태 확인
    /// @return 장치 핸들이 열려 있으면 true
    virtual bool isOpen() const = 0;

    /// @brief 장치 핸들 닫기
    virtual void close() = 0;

    /// @brief 장치 파일 디스크립터 반환 (Linux) 또는 핸들 (Windows)
    /// @return 파일 디스크립터 값
    virtual int fd() const = 0;
};

} // namespace libsed
