#include "walkman/time_machine/json.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace walkman::time_machine {

std::string JsonExporter::historyToJson(
    const ReconstructionHistory& history, const AudioData& audio,
    const std::string& inputName) {
    std::ostringstream output;
    output << "{\n"
           << "  \"file\": \"" << escape(inputName) << "\",\n"
           << "  \"sampleRate\": " << audio.sampleRate << ",\n"
           << "  \"channels\": " << audio.channels << ",\n"
           << "  \"durationSeconds\": "
           << number(static_cast<double>(audio.frameCount()) /
                     static_cast<double>(audio.sampleRate))
           << ",\n"
           << "  \"regions\": [\n";
    const auto ids = history.regionIds();
    for (std::size_t index = 0; index < ids.size(); ++index) {
        output << regionToJson(*history.region(ids[index]));
        if (index + 1U < ids.size()) {
            output << ",";
        }
        output << "\n";
    }
    output << "  ],\n"
           << "  \"snapshots\": [\n";
    const auto snapshots = history.snapshots();
    for (std::size_t index = 0; index < snapshots.size(); ++index) {
        const auto& snapshot = snapshots[index];
        output << "    {\n"
               << "      \"id\": \"" << escape(snapshot.id) << "\",\n"
               << "      \"label\": \"" << escape(snapshot.label) << "\",\n"
               << "      \"createdAt\": \"" << escape(snapshot.createdAt) << "\",\n"
               << "      \"selections\": [";
        for (std::size_t selection = 0; selection < snapshot.selections.size();
             ++selection) {
            const auto& item = snapshot.selections[selection];
            output << "\n        {\"regionId\": " << item.regionId
                   << ", \"hypothesisId\": \"" << escape(item.hypothesisId)
                   << "\", \"reverted\": "
                   << (item.reverted ? "true" : "false") << "}";
            if (selection + 1U < snapshot.selections.size()) {
                output << ",";
            }
        }
        if (!snapshot.selections.empty()) {
            output << "\n      ";
        }
        output << "]\n    }";
        if (index + 1U < snapshots.size()) {
            output << ",";
        }
        output << "\n";
    }
    output << "  ]\n}\n";
    return output.str();
}

std::string JsonExporter::timelineToJson(
    const AudioTimeline& timeline) {
    std::ostringstream output;
    output << "{\n  \"segments\": [\n";
    const auto& segments = timeline.segments();
    for (std::size_t index = 0; index < segments.size(); ++index) {
        const auto& segment = segments[index];
        output << "    {\"startSample\": " << segment.startSample
               << ", \"endSample\": " << segment.endSample
               << ", \"startSeconds\": " << number(segment.startSeconds)
               << ", \"endSeconds\": " << number(segment.endSeconds)
               << ", \"state\": \"" << escape(toString(segment.state))
               << "\", \"confidence\": " << number(segment.confidence)
               << ", \"regionId\": " << segment.regionId
               << ", \"reconstructionId\": \""
               << escape(segment.reconstructionId)
               << "\", \"algorithm\": \"" << escape(segment.selectedAlgorithm)
               << "\"}";
        if (index + 1U < segments.size()) {
            output << ",";
        }
        output << "\n";
    }
    output << "  ]\n}\n";
    return output.str();
}

std::string JsonExporter::confidenceToJson(
    const ConfidenceMap& confidence) {
    std::ostringstream output;
    output << "{\n  \"weightedConfidence\": "
           << number(confidence.averageConfidence()) << ",\n"
           << "  \"bands\": [\n";
    const auto& bands = confidence.bands();
    for (std::size_t index = 0; index < bands.size(); ++index) {
        const auto& band = bands[index];
        output << "    {\"startSample\": " << band.startSample
               << ", \"endSample\": " << band.endSample
               << ", \"startSeconds\": " << number(band.startSeconds)
               << ", \"endSeconds\": " << number(band.endSeconds)
               << ", \"confidence\": " << number(band.confidence)
               << ", \"state\": \"" << escape(toString(band.state))
               << "\", \"regionId\": " << band.regionId
               << ", \"reconstructionId\": \""
               << escape(band.reconstructionId)
               << "\", \"algorithm\": \"" << escape(band.selectedAlgorithm)
               << "\", \"evidence\": {"
               << "\"repairAlgorithmScore\": "
               << number(band.evidence.repairAlgorithmScore)
               << ", \"localWaveformContinuity\": "
               << number(band.evidence.localWaveformContinuity)
               << ", \"spectralContinuity\": "
               << number(band.evidence.spectralContinuity)
               << ", \"neighboringSimilarity\": "
               << number(band.evidence.neighboringSimilarity)
               << ", \"candidateAgreement\": "
               << number(band.evidence.candidateAgreement)
               << ", \"referenceComparison\": "
               << number(band.evidence.referenceComparison)
               << ", \"untouched\": "
               << (band.evidence.untouched ? "true" : "false")
               << ", \"explanation\": \""
               << escape(band.evidence.explanation) << "\"}}";
        if (index + 1U < bands.size()) {
            output << ",";
        }
        output << "\n";
    }
    output << "  ]\n}\n";
    return output.str();
}

std::string JsonExporter::completeToJson(
    const ReconstructionHistory& history, const AudioTimeline& timeline,
    const ConfidenceMap& confidence, const AudioData& audio,
    const std::string& inputName) {
    std::ostringstream output;
    output << "{\n"
           << "  \"history\": "
           << historyToJson(history, audio, inputName)
           << ",\n  \"timeline\": "
           << timelineToJson(timeline)
           << ",\n  \"confidenceMap\": "
           << confidenceToJson(confidence)
           << "}\n";
    return output.str();
}

void JsonExporter::writeFile(const std::string& path,
                             const std::string& content) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Unable to open JSON output: " + path);
    }
    output << content;
    if (!output) {
        throw std::runtime_error("Unable to write JSON output: " + path);
    }
}

std::string JsonExporter::escape(const std::string& value) {
    std::ostringstream output;
    for (const char character : value) {
        switch (character) {
        case '\\':
            output << "\\\\";
            break;
        case '"':
            output << "\\\"";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            output << character;
            break;
        }
    }
    return output.str();
}

std::string JsonExporter::number(double value) {
    if (!std::isfinite(value)) {
        return "null";
    }
    std::ostringstream output;
    output << std::setprecision(12) << value;
    return output.str();
}

std::string JsonExporter::metricsToJson(const QualityMetrics& metrics) {
    std::ostringstream output;
    output << "{\"mse\": " << number(metrics.mse)
           << ", \"snrDb\": " << number(metrics.snrDb)
           << ", \"correlation\": " << number(metrics.correlation)
           << ", \"rmsError\": " << number(metrics.rmsError)
           << ", \"spectralError\": " << number(metrics.spectralError) << "}";
    return output.str();
}

std::string JsonExporter::regionToJson(
    const RegionHypothesisSet& region) {
    std::ostringstream output;
    output << "    {\n"
           << "      \"id\": " << region.regionId << ",\n"
           << "      \"start\": " << region.region.startSample << ",\n"
           << "      \"end\": " << region.region.endSample << ",\n"
           << "      \"damageType\": \""
           << escape(toString(region.region.damageType)) << "\",\n"
           << "      \"selectedHypothesis\": \""
           << escape(region.selectedId) << "\",\n"
           << "      \"reverted\": " << (region.reverted ? "true" : "false")
           << ",\n"
           << "      \"hypotheses\": [\n";
    for (std::size_t index = 0; index < region.hypotheses.size(); ++index) {
        const auto& candidate = region.hypotheses[index];
        output << "        {\n"
               << "          \"id\": \"" << escape(candidate.id) << "\",\n"
               << "          \"algorithm\": \"" << escape(candidate.algorithmName)
               << "\",\n"
               << "          \"score\": " << number(candidate.qualityScore) << ",\n"
               << "          \"confidence\": " << number(candidate.confidence) << ",\n"
               << "          \"selected\": "
               << (candidate.selected ? "true" : "false") << ",\n"
               << "          \"metrics\": " << metricsToJson(candidate.metrics)
               << ",\n"
               << "          \"metadata\": {\"createdAt\": \""
               << escape(candidate.metadata.createdAt)
               << "\", \"engineVersion\": \""
               << escape(candidate.metadata.engineVersion)
               << "\", \"evaluationMode\": \""
               << escape(candidate.metadata.evaluationMode)
               << "\", \"candidateRank\": "
               << candidate.metadata.candidateRank << "},\n"
               << "          \"reasoning\": \""
               << escape(candidate.reasoning) << "\"\n"
               << "        }";
        if (index + 1U < region.hypotheses.size()) {
            output << ",";
        }
        output << "\n";
    }
    output << "      ]\n"
           << "    }";
    return output.str();
}

} // namespace walkman::time_machine