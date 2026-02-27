#pragma once

#include "../core/types.h"
#include "../core/error.h"
#include <cstdint>
#include <memory>
#include <string>

namespace libsed {

/// @brief IF-SEND / IF-RECV를 위한 추상 전송 인터페이스
class ITransport {
public:
    virtual ~ITransport() = default;

    /// @brief TCG Trusted Send를 통해 데이터 전송
    /// @param protocolId  보안 프로토콜 번호
    /// @param comId       프로토콜에 특정한 ComID
    /// @param payload     전송할 데이터
    /// @return 전송 결과
    virtual Result ifSend(uint8_t protocolId,
                          uint16_t comId,
                          ByteSpan payload) = 0;

    /// @brief TCG Trusted Receive를 통해 데이터 수신
    /// @param protocolId     보안 프로토콜 번호
    /// @param comId          ComID
    /// @param buffer         수신 데이터를 저장할 버퍼
    /// @param bytesReceived  실제 수신된 바이트 수
    /// @return 수신 결과
    virtual Result ifRecv(uint8_t protocolId,
                          uint16_t comId,
                          MutableByteSpan buffer,
                          size_t& bytesReceived) = 0;

    /// @brief 전송 타입 반환
    /// @return 현재 전송 계층의 타입 (NVMe, ATA, SCSI 등)
    virtual TransportType type() const = 0;

    /// @brief 장치 경로 반환
    /// @return 장치 경로 문자열 (예: "/dev/nvme0")
    virtual std::string devicePath() const = 0;

    /// @brief 장치 열림 상태 확인
    /// @return 장치가 열려 있고 유효하면 true
    virtual bool isOpen() const = 0;

    /// @brief 전송 닫기
    virtual void close() = 0;

    // ── Convenience wrappers ─────────────────────────

    /// @brief 자동 할당 버퍼를 사용하는 IF-RECV
    /// @param protocolId  보안 프로토콜 번호
    /// @param comId       ComID
    /// @param outBuffer   수신 데이터가 저장될 자동 할당 버퍼
    /// @param maxSize     최대 버퍼 크기 (기본값: 65536)
    /// @return 수신 결과
    Result ifRecv(uint8_t protocolId, uint16_t comId,
                  Bytes& outBuffer, size_t maxSize = 65536) {
        outBuffer.resize(maxSize);
        size_t received = 0;
        auto result = ifRecv(protocolId, comId,
                             MutableByteSpan(outBuffer.data(), outBuffer.size()),
                             received);
        if (result.ok()) {
            outBuffer.resize(received);
        }
        return result;
    }
};

} // namespace libsed
