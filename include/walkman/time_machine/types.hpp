#pragma once

#include "walkman/types.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace walkman::time_machine {

using RegionId = std::size_t;
using HypothesisId = std::string;
using SnapshotId = std::string;

enum class TimelineState {
    Clean,
    Suspected,
    Damaged,
    Reconstructed,
    LowConfidence,
    Reverted,
};

std::string toString(TimelineState state);

struct ReconstructionMetadata {
    std::string createdAt{"deterministic-run"};
    std::string engineVersion{"WalkMan Audio Time Machine 1.0"};
    std::string evaluationMode{"local-continuity"};
    std::size_t candidateRank{0};
    std::string sourceRegionLabel;
};

struct ReconstructionHypothesis {
    HypothesisId id;
    RegionId regionId{0};
    AudioRegion region;
    std::string algorithmName;
    std::shared_ptr<const std::vector<double>> reconstructedSamples;
    double confidence{0.0};
    double qualityScore{0.0};
    QualityMetrics metrics;
    ReconstructionMetadata metadata;
    bool selected{false};
    std::string reasoning;

    [[nodiscard]] std::size_t sampleCount() const noexcept;
    [[nodiscard]] bool validFor(const AudioData& audio) const noexcept;
};

struct HypothesisComparisonRow {
    HypothesisId id;
    std::string algorithmName;
    double score{0.0};
    QualityMetrics metrics;
    bool selected{false};
    std::size_t rank{0};
};

struct HypothesisComparison {
    RegionId regionId{0};
    AudioRegion region;
    std::vector<HypothesisComparisonRow> rows;
    HypothesisId selectedId;
    std::string winnerReason;
};

struct RegionHypothesisSet {
    RegionId regionId{0};
    AudioRegion region;
    std::vector<ReconstructionHypothesis> hypotheses;
    HypothesisId selectedId;
    bool reverted{false};

    [[nodiscard]] bool hasSelection() const noexcept;
    [[nodiscard]] std::size_t hypothesisCount() const noexcept;
};

struct ConfidenceEvidence {
    double repairAlgorithmScore{0.0};
    double localWaveformContinuity{0.0};
    double spectralContinuity{0.0};
    double neighboringSimilarity{0.0};
    double candidateAgreement{0.0};
    double referenceComparison{0.5};
    bool untouched{false};
    std::string explanation;
};

struct ConfidenceBand {
    std::size_t startSample{0};
    std::size_t endSample{0};
    double startSeconds{0.0};
    double endSeconds{0.0};
    double confidence{1.0};
    TimelineState state{TimelineState::Clean};
    RegionId regionId{0};
    HypothesisId reconstructionId;
    std::string selectedAlgorithm;
    ConfidenceEvidence evidence;
};

struct TimelineSegment {
    std::size_t startSample{0};
    std::size_t endSample{0};
    double startSeconds{0.0};
    double endSeconds{0.0};
    TimelineState state{TimelineState::Clean};
    double confidence{1.0};
    RegionId regionId{0};
    HypothesisId reconstructionId;
    std::string selectedAlgorithm;
};

std::string formatTimestamp(double seconds);

} // namespace walkman::time_machine