#pragma once

#include "walkman/time_machine/confidence.hpp"
#include "walkman/time_machine/timeline.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace walkman::time_machine {

struct AuditSummary {
    bool valid{false};
    std::size_t regionCount{0};
    std::size_t hypothesisCount{0};
    std::size_t selectedCount{0};
    std::size_t revertedCount{0};
    std::size_t lowConfidenceCount{0};
    std::size_t reconstructedSampleCount{0};
    std::size_t damagedSampleCount{0};
    double meanHypothesisScore{0.0};
    double bestHypothesisScore{0.0};
    double lowestHypothesisScore{100.0};
    double meanSelectedConfidence{0.0};
    std::map<std::string, std::size_t> algorithmCounts;
    std::vector<std::string> warnings;
    std::vector<std::string> observations;

    [[nodiscard]] bool hasWarnings() const noexcept;
    [[nodiscard]] std::size_t algorithmUseCount(
        const std::string& algorithm) const noexcept;
};

class ReconstructionAudit {
public:
    [[nodiscard]] static AuditSummary run(
        const ReconstructionHistory& history,
        const AudioTimeline& timeline,
        const ConfidenceMap& confidence,
        const AudioData& audio);
    [[nodiscard]] static std::string render(const AuditSummary& summary);
    [[nodiscard]] static std::string toJson(const AuditSummary& summary);

private:
    static void auditRegions(const ReconstructionHistory& history,
                             const AudioData& audio,
                             AuditSummary& summary);
    static void auditDerivedViews(const AudioTimeline& timeline,
                                  const ConfidenceMap& confidence,
                                  const AudioData& audio,
                                  AuditSummary& summary);
    static void addScoreObservation(const ReconstructionHypothesis& hypothesis,
                                    AuditSummary& summary);
    static std::string escape(const std::string& value);
};

} // namespace walkman::time_machine