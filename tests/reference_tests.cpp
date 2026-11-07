// Executable scenarios for features/references.feature: BONKR against recordings of the real
// instruments. Every referenced sound must pass the quality gate (no further from its recordings
// than twice their own spread, or 4; 5 with one recording) and may not score worse than
// references/baseline.tsv. Needs no network: the recordings' features are in references/.
#include <cstdio>
#include <string>

#include "reference_match.hpp"
#include "spec.hpp"

SCENARIO(every_referenced_sound_passes_the_gate_and_keeps_its_score) {
  GIVEN("the features of the reference recordings and BONKR's last scores");
  const auto refs = reference::load_features("references/features.tsv");
  const auto baseline = reference::load_table("references/baseline.tsv");
  CHECK(refs.size() >= 150, "the recordings' features are present");
  WHEN("every referenced sound is struck through the engine and measured the same way");
  const auto results = reference::match(refs);
  CHECK(results.size() >= 60, "at least sixty sounds have recordings");
  THEN("each is within its gate and no worse than its baseline");
  for (const auto& r : results) {
    const std::string id = reference::sound_id(r.sound);
    const double total = r.model.total();
    std::printf("    %-18s %5.2f  (gate %5.2f, recordings' spread %5.2f, %d recording%s)\n", id.c_str(), total, r.gate(), r.spread.total(), r.recordings, r.recordings == 1 ? "" : "s");
    CHECK(total <= r.gate(), id + spec::fmt(" within its gate: %.2f against %.2f", total, r.gate()));
    const auto b = baseline.find(id);
    CHECK(b != baseline.end(), id + " has a baseline");
    if (b != baseline.end()) CHECK(total <= b->second + 0.05, id + spec::fmt(" no worse than its baseline: %.2f against %.2f", total, b->second));
  }
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
