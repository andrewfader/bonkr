// The plug-in's side of the editor: an EditorHost that reads and writes Plugin parameters, the
// help text and relevance rules, and the per-page artwork. Included inside the plug-in's
// anonymous namespace so it can see Plugin. Layout, widgets and input live in editor.hpp.
#pragma once

#include <array>
#include <cmath>
#include <vector>

#include "canvas.hpp"
#include "editor.hpp"
#include "layout.hpp"
#include "pages.hpp"

// Whether a control does anything with the current layout, kit and sound. Irrelevant controls
// stay where they are but are dimmed, so the layout never jumps.
bool gui_param_relevant(const Plugin* p, clap_id id) {
  std::array<double, kParamCount> values{};
  for (size_t i = 0; i < kParamCount; ++i) values[i] = p->params[i].load(std::memory_order_relaxed);
  return bonkr::ui::param_relevant(static_cast<int>(id), values.data());
}

const char* gui_help(clap_id id) {
  switch (id) {
    case kLayout: return "Kit: every key a different drum, on the General MIDI drum map (kick on C1, snare on D1, hats on F#1, G#1, A#1, cymbals above). One sound: the chosen sound across the whole keyboard, C4 at its own pitch, a semitone a key.";
    case kKit: return "The kit on the drum map: acoustic sets (studio, jazz brushes, funk, a big room, gated 80s), orchestral, Latin, taiko and world percussion, the TR-808, TR-909 and CR-78 circuits, the LinnDrum, DMX, SP-1200, MPC60 and HR-16 heard through their converters, a junkyard, foley and machines, artillery and fireworks.";
    case kSound: return "The one sound played across the keyboard: any drum, cymbal, hand percussion, circuit, junk, machine or blast in BONKR.";
    case kMasterDb: return "The output level, before the limiter.";
    case kTune: return "Tunes every drum: a head's tension, a bar's length, a circuit's oscillators; for blasts it scales the charge (a lower note is a bigger charge).";
    case kStriker: return "What strikes: the sound's own (sticks on kits, mallets on timpani, hands on congas), or a stick, a felt mallet, a hand, a brush, rods or a hard mallet on everything. Each meets the surface by Hertzian contact.";
    case kHardness: return "How hard the striker's face is: harder is a shorter contact, so more of the high modes; softer is rounder.";
    case kPosition: return "Where the stroke lands, from the centre to the edge: the centre rings only the breathing modes (dull); toward the edge the higher modes and the overtones speak.";
    case kDynamics: return "How much the key's velocity reaches the stroke's speed (0.4 to 12 m/s): at none every stroke is the same.";
    case kDecay: return "Multiplies how long everything rings.";
    case kDamping: return "A hand, a felt or a pillow on the body: shortens the ring, the high modes most.";
    case kTension: return "How far a hard stroke raises the head's tension, so its pitch starts high and falls as it dies (and a gong's rises): Kirchhoff-Carrier tension modulation.";
    case kSnares: return "The snare wires against the bottom head: off (thrown off) to on.";
    case kSnareTension: return "How hard the strainer pulls the wires on: loose wires part from the head sooner and buzz longer; tight ones are crisp and short.";
    case kResonance: return "The other head and the shell: how much the resonant head rings with the batter (the low, breathing pair of modes).";
    case kConverter: return "The converter the kit is heard through: its own (none for acoustic kits), or the LinnDrum's or DMX's companded 8 bits, the SP-1200's 12 bits at 26 kHz with no filters, the MPC60's 12 bits, the HR-16's 16 bits.";
    case kSwing: return "MPC swing: notes on the second sixteenth of each eighth play later, to this share of the eighth (50% straight, 66% triplet, 75% dotted). Follows the host's transport.";
    case kNoteRepeat: return "While a key is held it strikes again at this division of the host's tempo, as an MPC's note repeat does.";
    case kDrive: return "Drives the kit into the machine's or the desk's input: rounder and louder hits, the 808's boom.";
    case kChoke: return "Closing the hi-hat (or the pedal) stops the open hat ringing, as on a real stand.";
    case kCrush: return "The listen-mic compressor: a fixed, very high ratio, so the room's tail comes up to the hit.";
    case kGate: return "A gate on the room: above this level it opens, holds, then shuts fast. With a big room and the compressor it is the gated drum sound of the early 1980s.";
    case kGateHold: return "How long the gate stays open after the hit.";
    case kStereoWidth: return "How wide the kit is spread, from the audience: hats left, floor tom right.";
    case kPan: return "Where the whole kit sits, left to right.";
    case kRoomSize: return "The room the kit plays in, from a vocal booth to a church, or none.";
    case kRoomAbsorption: return "How much the walls absorb: a stone church at a few percent, a curtained studio near the top.";
    case kMicDistance: return "How far the microphones stand from the kit: close is dry and bright; far away the room takes over and the air dulls the top.";
    case kHumidity: return "The air's humidity: dry air absorbs the treble more over the way to the microphones, and over a blast's, a storm's or a passing jet's long path outdoors.";
    case kTemperature: return "The air's temperature: the speed of sound (a jet's Doppler shift, an echo's delay, a mortar tube's note) and the air's absorption, in the room and outdoors.";
    case kMicPattern: return "The microphones' pattern: omni hears the room all round, cardioid less of it, figure-8 the front and back.";
    case kMicSetup: return "A stereo pair, or one microphone in mono.";
    case kPreset: return "Factory presets: a kit or a sound in a room, ready to play.";
    default: return nullptr;
  }
}

void gui_request_flush(Plugin* p) {
  if (p->host && p->host->get_extension)
    if (const auto* hp = static_cast<const clap_host_params_t*>(p->host->get_extension(p->host, CLAP_EXT_PARAMS)))
      if (hp->request_flush) hp->request_flush(p->host);
}

void gui_mark_state(Plugin* p) {
  if (p->host && p->host->get_extension)
    if (const auto* hs = static_cast<const clap_host_state_t*>(p->host->get_extension(p->host, CLAP_EXT_STATE)))
      if (hs->mark_dirty) hs->mark_dirty(p->host);
}

class PluginEditorHost final : public bonkr::ui::EditorHost {
 public:
  explicit PluginEditorHost(Plugin* p) : p_(p) {}

  int param_count() const override { return static_cast<int>(kParamCount); }
  bonkr::ui::ParamInfo info(int id) const override {
    const auto& s = kSpecs[static_cast<size_t>(id)];
    return {s.name, s.min, s.max, s.def, s.stepped};
  }
  double value(int id) const override { return p_->params[static_cast<size_t>(id)].load(std::memory_order_relaxed); }
  void begin_edit(int id) override { gui_push(p_, GuiEvent::Begin, static_cast<clap_id>(id), 0); gui_request_flush(p_); }
  void edit(int id, double v) override {
    if (static_cast<clap_id>(id) == kPreset) apply_preset(p_, static_cast<int>(std::lround(v)));
    else set_param(p_, static_cast<clap_id>(id), v);
    snapshot_settings(p_);
    gui_push(p_, GuiEvent::Value, static_cast<clap_id>(id), value(id));
    gui_request_flush(p_);
  }
  void end_edit(int id) override { gui_push(p_, GuiEvent::End, static_cast<clap_id>(id), 0); gui_request_flush(p_); gui_mark_state(p_); }
  void format(int id, double v, char* out, size_t capacity) const override {
    format_value(static_cast<uint32_t>(id), v, out, static_cast<uint32_t>(capacity));
  }
  const char* help(int id) const override { return gui_help(static_cast<clap_id>(id)); }
  const char* short_label(int id) const override {
    if (const char* name = bonkr::ui::bonkr_short_name(id)) return name;
    return label(id);
  }
  bool relevant(int id) const override { return gui_param_relevant(p_, static_cast<clap_id>(id)); }

  int size_percent() const override {
    return static_cast<int>(std::lround(100.0 * bonkr::ui::uniform_scale(static_cast<int>(p_->gui_width), static_cast<int>(p_->gui_height))));
  }
  void request_size(int percent) override {
    p_->gui_request_width = static_cast<uint32_t>(bonkr::ui::width * percent / 100);
    p_->gui_request_height = static_cast<uint32_t>(bonkr::ui::height * percent / 100);
  }

  const char* strip_help(int page) const override {
    if (page == 0) return "The keyboard with the kit's keys (dark keys are empty), lit while they sound, and the last stroke: its sound, speed, contact time and pitch; the output scope below.";
    if (page == 1) return "The striker's force against time at three stroke speeds, from the Hertzian contact the voices use.";
    if (page == 2) return "The struck body's modes on a log frequency axis: taller and brighter for the ones that ring longest.";
    if (page == 3) return "A quiet decaying tone through the converter (its clock and steps), and the swing grid.";
    if (page == 4) return "A stroke and its room's tail in dB against time, and what the compressor and the gate leave of it.";
    if (page == 5) return "The room from above: the kit, the microphones at their distance.";
    return nullptr;
  }

  void draw_meters(bonkr::ui::Painter& g, const bonkr::ui::Rect& r) override {
    using namespace bonkr::ui::theme;
    const float peak_l = p_->peak_l.load(std::memory_order_relaxed), peak_r = p_->peak_r.load(std::memory_order_relaxed);
    const bool clipped = p_->clipped.load(std::memory_order_relaxed);
    g.text(r.x, r.y + 24, "OUT", muted, 50, bonkr::ui::TextSize::Small);
    const int bar_x = r.x + 52, bar_w = 230;
    g.rect(bar_x, r.y + 6, bar_w, 9, border);
    g.rect(bar_x, r.y + 20, bar_w, 9, border);
    const uint32_t level = clipped ? red : green;
    g.rect(bar_x, r.y + 6, static_cast<int>(bar_w * std::clamp(peak_l, 0.0f, 1.0f)), 9, level);
    g.rect(bar_x, r.y + 20, static_cast<int>(bar_w * std::clamp(peak_r, 0.0f, 1.0f)), 9, level);
    char voices[32]{};
    std::snprintf(voices, sizeof(voices), clipped ? "CLIP" : "%d SOUNDING", p_->active_voices.load(std::memory_order_relaxed));
    g.text(bar_x + bar_w + 18, r.y + 24, voices, clipped ? red : muted, r.right() - bar_x - bar_w - 18, bonkr::ui::TextSize::Small);
  }

  void draw_strip(int page, bonkr::ui::Painter& g, const bonkr::ui::Rect& r) override {
    bonkr::ui::LiveState live;
    const uint32_t write = p_->scope_write.load(std::memory_order_acquire);
    float energy = 0.0f;
    for (size_t i = 0; i < live.scope.size(); ++i) {
      live.scope[i] = p_->scope[(write + static_cast<uint32_t>(i)) & (Plugin::kScopeSize - 1)].load(std::memory_order_relaxed);
      energy += live.scope[i] * live.scope[i];
    }
    live.energy = std::sqrt(energy / static_cast<float>(live.scope.size()));
    live.sample_rate = p_->sample_rate;
    for (int k = 0; k < 128; ++k) live.sounding[static_cast<size_t>(k)] = p_->sounding[static_cast<size_t>(k)].load(std::memory_order_relaxed);
    live.voices = p_->active_voices.load(std::memory_order_relaxed);
    live.key = p_->live_key.load(std::memory_order_relaxed);
    live.sound = p_->live_sound.load(std::memory_order_relaxed);
    live.speed = p_->live_speed.load(std::memory_order_relaxed);
    live.contact_ms = p_->live_contact_ms.load(std::memory_order_relaxed);
    live.hz = p_->live_hz.load(std::memory_order_relaxed);
    live.gate = p_->live_gate.load(std::memory_order_relaxed);
    live.reduction_db = p_->live_reduction.load(std::memory_order_relaxed);
    bonkr::ui::Artwork(*this, live).draw(page, g, r);
  }

  bool strip_pointer(int page, int button, int x, int y, const bonkr::ui::Rect& r) override {
    (void)page; (void)button; (void)x; (void)y; (void)r;
    return false;
  }

 private:
  Plugin* p_;
};
static_assert(Plugin::kScopeSize == bonkr::ui::kScopeSamples, "the scope ring and the artwork agree on its length");

bonkr::ui::Editor& gui_editor(Plugin* p) {
  if (!p->editor) {
    p->editor_host = std::make_unique<PluginEditorHost>(p);
    p->editor = std::make_unique<bonkr::ui::Editor>(*p->editor_host, bonkr::ui::bonkr_pages(), bonkr::ui::bonkr_header_control());
  }
  return *p->editor;
}

void gui_draw(Plugin* p, bonkr::ui::Canvas& canvas) { gui_editor(p).draw(canvas); }

using GuiPointer = bonkr::ui::PointerAction;

bool gui_set_size(const clap_plugin_t* plugin, uint32_t w, uint32_t h);

// Asks the host to resize its editor frame; only once it agrees does the editor window follow.
void gui_apply_requested_size(Plugin* p) {
  const uint32_t w = p->gui_request_width, h = p->gui_request_height;
  p->gui_request_width = p->gui_request_height = 0;
  if (!w || !h || (w == p->gui_width && h == p->gui_height) || !p->host || !p->host->get_extension) return;
  const auto* host_gui = static_cast<const clap_host_gui_t*>(p->host->get_extension(p->host, CLAP_EXT_GUI));
  if (!host_gui || !host_gui->request_resize || !host_gui->request_resize(p->host, w, h)) return;
  gui_set_size(&p->api, w, h);
  gui_mark_state(p);
}

void gui_input(Plugin* p, GuiPointer action, int button, int x, int y, unsigned modifiers = 0) {
  gui_editor(p).pointer(action, button, x, y, modifiers, bonkr::ui::now_ms());
  gui_apply_requested_size(p);
}

// True when the editor needs a repaint for its own animations.
bool gui_tick(Plugin* p) { return gui_editor(p).tick(bonkr::ui::now_ms()); }

[[maybe_unused]] int gui_size_percent_for_test(Plugin* p) { gui_editor(p); return p->editor_host->size_percent(); }
