#include "walkman/corruption.hpp"
#include "walkman/engine.hpp"

#include <cassert>

int main() {
    const auto clean = walkman::makeSyntheticAudio(4000, 1, 2.0);
    const auto damaged = walkman::AudioCorruptor(7).corrupt(clean).damaged;
    walkman::AudioData repaired;
    const auto report = walkman::HealingEngine().heal(damaged, &clean, repaired);
    assert(repaired.valid());
    assert(repaired.samples.size() == clean.samples.size());
    assert(report.repairedRegions > 0);
    return 0;
}