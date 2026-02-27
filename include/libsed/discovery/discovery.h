#pragma once

#include "../core/types.h"
#include "../core/error.h"
#include "../transport/i_transport.h"
#include "feature_descriptor.h"
#include <memory>
#include <vector>
#include <unordered_map>

namespace libsed {

/// @brief Level 0 Discovery 파서
class Discovery {
public:
    Discovery() = default;

    /// @brief Transport를 통해 Level 0 Discovery 수행
    /// @param transport TCG 통신에 사용할 Transport
    /// @return 성공 또는 오류 코드
    Result discover(std::shared_ptr<ITransport> transport);

    /// @brief 원시 Level 0 Discovery 응답 파싱
    /// @param data 원시 응답 데이터 포인터
    /// @param len 데이터 길이
    /// @return 성공 또는 오류 코드
    Result parse(const uint8_t* data, size_t len);
    /// @brief 원시 Level 0 Discovery 응답 파싱 (Bytes 오버로드)
    /// @param data 원시 응답 데이터
    /// @return 성공 또는 오류 코드
    Result parse(const Bytes& data) { return parse(data.data(), data.size()); }

    /// @brief Discovery 헤더의 전체 길이 반환
    uint32_t headerLength() const { return headerLength_; }
    /// @brief Discovery 헤더의 주 버전 번호 반환
    uint32_t majorVersion() const { return majorVersion_; }
    /// @brief Discovery 헤더의 부 버전 번호 반환
    uint32_t minorVersion() const { return minorVersion_; }

    /// @brief 모든 Feature Descriptor 반환
    const std::vector<std::unique_ptr<FeatureDescriptor>>& features() const { return features_; }

    /// @brief Feature Code로 Feature 검색
    /// @param featureCode 검색할 Feature Code
    /// @return 찾은 FeatureDescriptor 포인터, 없으면 nullptr
    const FeatureDescriptor* findFeature(uint16_t featureCode) const;

    /// @brief TPer Feature (0x0001) 존재 여부 확인
    bool hasTPerFeature() const { return findFeature(0x0001) != nullptr; }
    /// @brief Locking Feature (0x0002) 존재 여부 확인
    bool hasLockingFeature() const { return findFeature(0x0002) != nullptr; }
    /// @brief Geometry Reporting Feature (0x0003) 존재 여부 확인
    bool hasGeometryFeature() const { return findFeature(0x0003) != nullptr; }
    /// @brief Opal SSC v1.0 Feature (0x0200) 존재 여부 확인
    bool hasOpalV1Feature() const { return findFeature(0x0200) != nullptr; }
    /// @brief Opal SSC v2.0 Feature (0x0203) 존재 여부 확인
    bool hasOpalV2Feature() const { return findFeature(0x0203) != nullptr; }
    /// @brief Enterprise SSC Feature (0x0100) 존재 여부 확인
    bool hasEnterpriseFeature() const { return findFeature(0x0100) != nullptr; }
    /// @brief Pyrite SSC v1.0 Feature (0x0302) 존재 여부 확인
    bool hasPyriteV1Feature() const { return findFeature(0x0302) != nullptr; }
    /// @brief Pyrite SSC v2.0 Feature (0x0303) 존재 여부 확인
    bool hasPyriteV2Feature() const { return findFeature(0x0303) != nullptr; }

    /// @brief 감지된 SSC 유형 반환
    SscType detectSsc() const;

    /// @brief 감지된 SSC의 기본 ComID 반환
    uint16_t baseComId() const;

    /// @brief DiscoveryInfo 요약 구조체 생성
    /// @return 파싱된 Discovery 정보를 담은 DiscoveryInfo 구조체
    DiscoveryInfo buildInfo() const;

    /// @brief Level 0 Discovery용 프로토콜 ID
    static constexpr uint8_t PROTOCOL_ID = 0x01;
    /// @brief Level 0 Discovery용 ComID
    static constexpr uint16_t COMID = 0x0001;

private:
    Result parseFeature(const uint8_t* data, size_t len, size_t& offset);

    uint32_t headerLength_ = 0;
    uint32_t majorVersion_ = 0;
    uint32_t minorVersion_ = 0;
    std::vector<std::unique_ptr<FeatureDescriptor>> features_;
};

} // namespace libsed
