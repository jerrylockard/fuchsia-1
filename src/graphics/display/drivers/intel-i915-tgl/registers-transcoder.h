// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_GRAPHICS_DISPLAY_DRIVERS_INTEL_I915_TGL_REGISTERS_TRANSCODER_H_
#define SRC_GRAPHICS_DISPLAY_DRIVERS_INTEL_I915_TGL_REGISTERS_TRANSCODER_H_

#include <zircon/assert.h>

#include <cstdint>
#include <optional>

#include <hwreg/bitfields.h>

#include "src/graphics/display/drivers/intel-i915-tgl/hardware-common.h"

namespace tgl_registers {

// TRANS_HTOTAL, TRANS_HBLANK,
// TRANS_VTOTAL, TRANS_VBLANK
class TransHVTotal : public hwreg::RegisterBase<TransHVTotal, uint32_t> {
 public:
  DEF_FIELD(29, 16, count_total);  // same as blank_end
  DEF_FIELD(13, 0, count_active);  // same as blank_start
};

// TRANS_HSYNC, TRANS_VSYNC
class TransHVSync : public hwreg::RegisterBase<TransHVSync, uint32_t> {
 public:
  DEF_FIELD(29, 16, sync_end);
  DEF_FIELD(13, 0, sync_start);
};

// TRANS_VSYNCSHIFT
class TransVSyncShift : public hwreg::RegisterBase<TransVSyncShift, uint32_t> {
 public:
  DEF_FIELD(12, 0, second_field_vsync_shift);
};

// TRANS_DDI_FUNC_CTL (Transcoder DDI Function Control)
//
// This register has reserved bits that are not documented as MBZ (must be
// zero), so it should be accessed using read-modify-write.
//
// Tiger Lake: IHD-OS-TGL-Vol 2c-1.22-Rev2.0 Part 2 pages 1370-1375
// Kaby Lake: IHD-OS-KBL-Vol 2c-1.17 Part 2 pages 952-956
// Skylake: IHD-OS-SKL-Vol 2c-05.16 Part 2 pages 926-930
class TranscoderDdiControl : public hwreg::RegisterBase<TranscoderDdiControl, uint32_t> {
 public:
  // Enables the transcoder's DDI functionality.
  DEF_BIT(31, enabled);

  // Selects the DDI that the transcoder will connect to.
  //
  // This field has a non-trivial value encoding. The ddi_*() and set_ddi_*()
  // helpers should be preferred to accessing the field directly.
  //
  // This field is tagged `_subtle` because the definition matches the bits used
  // on Tiger Lake, but it's used on all supported models. Kaby Lake and Skylake
  // have a very similar field, which only takes up bits 30-28. Fortunately, bit
  // 27 is reserved MBZ (must be zero). So, there's still a 1:1 mapping between
  // DDI selection and the values of bits 30-27.
  //
  // We take advantage of this to avoid forking the entire (fairly large)
  // register definition by papering over this difference in the helpers
  // `ddi_kaby_lake()` and `set_ddi_kaby_lake()`.
  DEF_FIELD(30, 27, ddi_select_subtle);

  // The DDI that the transcoder will connect to.
  //
  // This helper works for Kaby Lake and Skylake.
  //
  // This field must not be changed while `enabled` is true. Directing multiple
  // transcoders to the same DDI is only valid for DisplayPort Multi-Streaming.
  //
  // The underlying field is ignored by the EDP transcoder, which is attached to
  // DDI A.
  std::optional<Ddi> ddi_kaby_lake() const {
    // The cast is lossless because `ddi_select_subtle()` is a 4-bit field.
    const int8_t ddi_select_raw_value = static_cast<int8_t>(ddi_select_subtle());
    if (ddi_select_raw_value == 0) {
      return std::nullopt;
    }

    // Convert from the Tiger Lake field representation.
    const int ddi_index = (ddi_select_raw_value >> 1);
    return static_cast<Ddi>(ddi_index);
  }

  // The DDI that the transcoder will connect to.
  //
  // This helper works for Tiger Lake.
  //
  // This field must not be changed while `enabled` is true. Directing multiple
  // transcoders to the same DDI is only valid for DisplayPort Multi-Streaming.
  //
  // The underlying field is ignored by the DSI transcoders. Each DSI transcoder
  // is attached to a DDI.
  std::optional<Ddi> ddi_tiger_lake() const {
    // The cast is lossless because `ddi_select_subtle()` is a 4-bit field.
    const int8_t ddi_select_raw_value = static_cast<int8_t>(ddi_select_subtle());
    if (ddi_select_raw_value == 0) {
      return std::nullopt;
    }
    const int ddi_index = ddi_select_raw_value - 1;
    return static_cast<Ddi>(ddi_index);
  }

  // See `ddi_kaby_lake()` for details.
  TranscoderDdiControl& set_ddi_kaby_lake(std::optional<Ddi> ddi) {
    if (!ddi.has_value()) {
      return set_ddi_select_subtle(0);
    }

    ZX_DEBUG_ASSERT_MSG(*ddi != Ddi::DDI_A, "DDI A cannot be explicitly connected to a transcoder");
    const int ddi_index = *ddi - Ddi::DDI_A;

    // Convert to the Tiger Lake field representation.
    return set_ddi_select_subtle(ddi_index << 1);
  }

  // See `ddi_tiger_lake()` for details.
  TranscoderDdiControl& set_ddi_tiger_lake(std::optional<Ddi> ddi) {
    if (!ddi.has_value()) {
      return set_ddi_select_subtle(0);
    }
    const int ddi_index = *ddi - Ddi::DDI_A;
    return set_ddi_select_subtle(ddi_index + 1);
  }

  // The transcoder's mode of operation.
  //
  // This field must not be changed while `enabled` is true.
  //
  // This field must be changed in the same MMIO write as the
  // `display_port_transport_tiger_lake` field.
  //
  // In HDMI mode, the transcoder sends a null packet (32 zero bytes) when
  // Vsync is asserted. The transcoder also sends a preamble and guardband
  // before each null packet. These behaviors match the HDMI specification.
  //
  // In DVI mode, enabling DIP (Data Island Packets) or audio causes the
  // transcoder to adopt the HDMI behavior described above.
  //
  // DisplayPort modes SST (Single Stream) or MST (Multi-Stream) must match the
  // mode selected in the `DpTransportControl` register.
  //
  // On Tiger Lake, the DSI transcoders ignore this field.
  //
  // On Kaby Lake, transcoder EDP (and therefore DDI A) must be in the
  // DisplayPort SST (Single Stream) mode.
  DEF_FIELD(26, 24, ddi_mode);

  // TODO(fxbug.dev/110690): Move the constants below into an enum class once we
  // figure out how to handle invalid field values.
  static constexpr uint32_t kModeHdmi = 0;
  static constexpr uint32_t kModeDvi = 1;
  static constexpr uint32_t kModeDisplayPortSingleStream = 2;
  static constexpr uint32_t kModeDisplayPortMultiStream = 3;

  // Selects the bpc (number of bits per color) output on the connected DDI.
  //
  // This field must not be changed while `enabled` is true.
  //
  // HDMI and DSC (Display Stream Compression) don't support 6bpc.
  //
  // On Tiger Lake, the DSI transcoder ignores this field, and uses the pixel
  // format in the TRANS_DSI_FUNC_CONF register.
  DEF_FIELD(22, 20, bits_per_color);

  // TODO(fxbug.dev/110690): Move the constants below into an enum class once we
  // figure out how to handle invalid field values.
  static constexpr uint32_t k8bpc = 0;
  static constexpr uint32_t k10bpc = 1;
  static constexpr uint32_t k6bpc = 2;
  static constexpr uint32_t k12bpc = 3;

  // When operating as a port sync secondary, selects the primary transcoder.
  //
  // This field has a non-trivial value encoding. The
  // `port_sync_primary_transcoder_kaby_lake()` and
  // `set_port_sync_primary_transcoder_kaby_lake()` helpers should be preferred
  // to accessing the field directly.
  DEF_FIELD(19, 18, port_sync_primary_transcoder_select_kaby_lake);

  // When operating as a port sync secondary, selects the primary transcoder.
  //
  // This field is ignored by the EDP transcoder, because it cannot function as
  // a port sync secondary.
  //
  // This field's bits are reserved MBZ (must be zero) on Tiger Lake. The field
  // was moved to the TRANS_DDI_FUNC_CTL2 register and widened.
  Trans port_sync_primary_transcoder_kaby_lake() const {
    // The cast is lossless because `port_sync_primary_select_kaby_lake()` is a
    // 2-bit field.
    const int8_t raw_port_sync_primary_select =
        static_cast<int8_t>(port_sync_primary_transcoder_select_kaby_lake());
    if (raw_port_sync_primary_select == 0) {
      return Trans::TRANS_EDP;
    }

    // The subtraction result is non-negative, because we checked for zero
    // above. The addition will not overflow because
    // `port_sync_primary_select_kaby_lake()` is a 2-bit field.
    return static_cast<Trans>(Trans::TRANS_A + (raw_port_sync_primary_select - 1));
  }

  // See `port_sync_primary_kaby_lake()`.
  TranscoderDdiControl& set_port_sync_primary_kaby_lake(Trans transcoder) {
    if (transcoder == Trans::TRANS_EDP) {
      return set_port_sync_primary_transcoder_select_kaby_lake(0);
    }

    ZX_ASSERT(transcoder >= Trans::TRANS_A);
    ZX_ASSERT(transcoder <= Trans::TRANS_C);
    return set_port_sync_primary_transcoder_select_kaby_lake((transcoder - Trans::TRANS_A) + 1);
  }

  // If true, VSync is active high. If false, VSync is active low.
  //
  // On Tiger Lake, the DSI transcoders ignore this field.
  //
  // Active high is the default, and considered the standard polarity. Active
  // low is considered an inverted polarity.
  DEF_BIT(17, vsync_polarity_not_inverted);

  // If true, HSync is active high. If false, HSync is active low.
  //
  // On Tiger Lake, the DSI transcoders ignore this field.
  //
  // Active high is the default, and considered the standard polarity. Active
  // low is considered an inverted polarity.
  DEF_BIT(16, hsync_polarity_not_inverted);

  // If true, this transcoder operates as a port sync secondary transcoder.
  //
  // Only the secondary transcoders must be explicitly configured for port sync.
  // This is set to false for the port sync primary transcoder.
  //
  // This field is ignored by the EDP transcoder, because it cannot function as
  // a port sync secondary.
  //
  // This field's bits are reserved MBZ (must be zero) on Tiger Lake. The field
  // was moved to the TRANS_DDI_FUNC_CTL2 register.
  DEF_BIT(15, is_port_sync_secondary_kaby_lake);

  // Selects the input pipe, for transcoders that are not attached to pipes.
  //
  // This field has a non-trivial value encoding. The input_pipe_*() and
  // set_input_pipe_*() helpers should be preferred to accessing the field
  // directly.
  DEF_FIELD(14, 12, input_pipe_select);

  // Selects the input pipe, for transcoders that are not attached to pipes.
  //
  // On Tiger Lake, this field is only used by the DSI transcoders. On Kaby
  // Lake, the field is only used by the EDP transcoder. These are the
  // transcoders that are not attached to pipes.
  //
  // This field is not documented on Skylake, and its bits are documented as
  // reserved. However, several PRM locations (IHD-OS-SKL-Vol 12-05.16 section
  // "Display Connections" pages 103, section "Pipe to Transcoder to DDI
  // Mappings" page 107) mention that the EDP transcoder can connect to pipes
  // A-C. So, the field likely works the same way as on Kaby Lake.
  Pipe input_pipe() const {
    switch (input_pipe_select()) {
      case kInputSelectPipeA:
        return Pipe::PIPE_A;
      case kInputSelectPipeB:
        return Pipe::PIPE_B;
      case kInputSelectPipeC:
        return Pipe::PIPE_C;

        // TODO(fxbug.dev/109278): Add pipe D, once we support it.
    };

    return Pipe::PIPE_INVALID;
  }

  // See `input_pipe()` for details.
  TranscoderDdiControl& set_input_pipe(Pipe input_pipe) {
    switch (input_pipe) {
      case Pipe::PIPE_A:
        return set_input_pipe_select(kInputSelectPipeA);
      case Pipe::PIPE_B:
        return set_input_pipe_select(kInputSelectPipeB);
      case Pipe::PIPE_C:
        return set_input_pipe_select(kInputSelectPipeC);

        // TODO(fxbug.dev/109278): Add pipe D, once we support it.

      case Pipe::PIPE_INVALID:
          // The code handling the explicit invalid pipe value is outside the
          // switch() so it also applies to values that aren't Pipe enum members,
          // which are also invalid.
          ;
    };

    ZX_DEBUG_ASSERT_MSG(false, "Invalid pipe: %d", input_pipe);
    return *this;
  }

  // Values for `display_port_transport_tiger_lake`.
  enum class DisplayPortTransportTigerLake {
    kA = 0,
    kB = 1,
    kC = 2,
    kD = 3,
  };

  // Selects the DisplayPort transport that receives this transcoder's data.
  //
  // This field is only used when DisplayPort MST (multi-streaming) is enabled.
  //
  // This must be changed in the same MMIO operation as `ddi_mode`.
  DEF_ENUM_FIELD(DisplayPortTransportTigerLake, 11, 10, display_port_transport_tiger_lake);

  // If true, VC (Virtual Channel) payload allocation is enabled.
  //
  // This field is ignored by the transcoders attached to DDIs that don't
  // support multi-streaming. These are the DSI transcoders On Tiger Lake, and
  // the EDP transcoder on Kaby Lake and Skylake.
  DEF_BIT(8, allocate_display_port_virtual_circuit_payload);

  // If true, the HDMI scrambler is in CTS (Compliance Test Specification) mode.
  //
  // This field must not be changed while `hdmi_scrambler_enabled` is true.
  //
  // This field is not documented on Kaby Lake and Skylake. The bit is reserved
  // MBZ (must be zero). This extends the good read semantics of `hdmi_enabled_`
  // -- reading zero means that the CTS mode is disabled, which makes perfect
  // sense while the HDMI scrambler is disabled.
  DEF_BIT(7, hdmi_scrambler_cts_mode);

  // If false, the HDMI scrambler is reset on every line.
  //
  // This field is only used when the HDMI scrambler is in CTS mode. In that
  // case, it determines whether the transceiver sends a SSCP (Scrambler
  // Synchronization Control Period) during HSync for every line, or for every
  // other line.
  //
  // This field must be not be set while `hdmi_scrambler_cts_mode` is true.
  //
  // This field is not documented on Kaby Lake and Skylake. The bit is reserved
  // MBZ (must be zero). This extends the good read semantics of
  // `hdmi_scrambler_cts_mode` -- the CTS mode is never enabled, and this field
  // can always be ignored.
  DEF_BIT(6, hdmi_scrambler_resets_every_other_line);

  // If true, the high TMDS character rate is enabled over the HDMI link.
  //
  // This field must be set to true if and only if the HDMI link symbol rate is
  // greater than 340 MHz.
  //
  // This field is not documented on Kaby Lake and Skylake. The bits are
  // reserved MBZ (must be zero), which makes for good read semantics -- reading
  // zero means that the high TMDS character rate is not enabled.
  DEF_BIT(4, high_tmds_character_rate_tiger_lake);

  // Selects the number of DisplayPort or DSI lanes enabled.
  //
  // This field has a non-trivial value encoding. The
  // `display_port_lane_count()` and `set_display_port_lane_count()` helpers
  // should be preferred to accessing the field directly.
  DEF_FIELD(3, 1, display_port_lane_count_selection);

  // The number of DisplayPort lanes enabled.
  //
  // This field is ignored for HDMI or DVI, as these modes always use 4 lanes.
  // Only the DSI transcoders support using 3 lanes.
  //
  // When the transcoder mode is a DisplayPort mode, the field must match the
  // `display_port_lane_count` in the attached DDI's DdiBufferControl register.
  uint8_t display_port_lane_count() const {
    // The addition will not overflow and the cast is lossless because
    // display_port_lane_count_selection() is a 3-bit field.
    return static_cast<int8_t>(display_port_lane_count_selection() + 1);
  }

  // See `display_port_lane_count()` for details.
  TranscoderDdiControl& set_display_port_lane_count(uint8_t lane_count) {
    ZX_DEBUG_ASSERT(lane_count >= 1);
    ZX_DEBUG_ASSERT(lane_count <= 4);
    return set_display_port_lane_count_selection(lane_count - 1);
  }

  // If true, scrambling is enabled over the HDMI link.
  //
  // Scrambling must be enabled for HDMI link symbol rates above 340 MHz.
  // Scrambling should also be enabled at lower speeds, when the receiver
  // supports scrambling at those speeds.
  //
  // This field is not documented on Kaby Lake and Skylake. The bits are
  // reserved MBZ (must be zero), which makes for good read semantics -- reading
  // zero means that no HDMI scrambler is enabled.
  DEF_BIT(0, hdmi_scrambler_enabled_tiger_lake);

  static auto GetForKabyLakeTranscoder(Trans transcoder) {
    if (transcoder == Trans::TRANS_EDP) {
      return hwreg::RegisterAddr<TranscoderDdiControl>(0x6f400);
    }

    ZX_ASSERT(transcoder >= Trans::TRANS_A);
    ZX_ASSERT(transcoder <= Trans::TRANS_C);
    const int transcoder_index = transcoder - Trans::TRANS_A;
    return hwreg::RegisterAddr<TranscoderDdiControl>(0x60400 + 0x1000 * transcoder_index);
  }

  static auto GetForTigerLakeTranscoder(Trans transcoder) {
    ZX_ASSERT(transcoder >= Trans::TRANS_A);

    // TODO(fxbug.dev/109278): Allow transcoder D, once we support it.
    ZX_ASSERT(transcoder <= Trans::TRANS_C);

    const int transcoder_index = transcoder - Trans::TRANS_A;
    return hwreg::RegisterAddr<TranscoderDdiControl>(0x60400 + 0x1000 * transcoder_index);
  }

 private:
  static constexpr uint32_t kInputSelectPipeA = 0;
  static constexpr uint32_t kInputSelectPipeB = 5;
  static constexpr uint32_t kInputSelectPipeC = 6;
  // TODO(fxbug.dev/109278): Add pipe D, once we support it. The value is 7.
};

// TRANS_CONF (Transcoder Configuration)
//
// This register has reserved bits that are not documented as MBZ (must be
// zero), so it should be accessed using read-modify-write.
//
// Tiger Lake: IHD-OS-TGL-Vol 2c-1.22-Rev2.0 Part 2 pages 1365-1366
// Kaby Lake: IHD-OS-KBL-Vol 2c-1.17 Part 2 pages 949-951
// Skylake: IHD-OS-SKL-Vol 2c-05.16 Part 2 pages 924-925
class TranscoderConfig : public hwreg::RegisterBase<TranscoderConfig, uint32_t> {
 public:
  // Set to true/false to eventually enable/disable the transcoder.
  //
  // Turning off the transcoder disables the timing generator and the
  // synchronization pulses to the display.
  //
  // Timing registers must be set to valid values before this field is enabled.
  DEF_BIT(31, enabled_target);

  // Read-only, reflects the current state.
  DEF_BIT(30, enabled);

  // If false, the transcoder operates in Progressive Fetch mode.
  //
  // The following features are not supported with Interlaced Fetch mode:
  // * Y tiling
  // * 90 or 270 rotation
  // * scaling
  // * YUV 4:2:0 hybrid planar source pixel formats
  DEF_BIT(22, interlaced_fetch);

  // If false, the transcoder operates in Progressive Display mode.
  //
  // Must be true if `interlaced_fetch` is true.
  //
  // When `interlaced_fetch` is false and `interlaced_display` is true:
  // * Pipe scaling is required
  // * The vertical resolution doubles
  // * The maximum supported pixel rate is cut down in half
  DEF_BIT(21, interlaced_display);

  // The number of symbols that must be in the DisplayPort audio symbol RAM
  // before it starts to drain during horizontal blank.
  //
  // The value must be between 2 and 64.
  //
  // This field does not exist (must be zero) on Kaby Lake or Skylake.
  DEF_FIELD(6, 0, display_port_audio_symbol_watermark_tiger_lake);

  static auto GetForKabyLakeTranscoder(Trans transcoder) {
    if (transcoder == Trans::TRANS_EDP) {
      return hwreg::RegisterAddr<TranscoderConfig>(0x7f008);
    }

    ZX_ASSERT(transcoder >= Trans::TRANS_A);
    ZX_ASSERT(transcoder <= Trans::TRANS_C);
    const int transcoder_index = transcoder - Trans::TRANS_A;
    return hwreg::RegisterAddr<TranscoderConfig>(0x70008 + 0x1000 * transcoder_index);
  }

  static auto GetForTigerLakeTranscoder(Trans transcoder) {
    ZX_ASSERT(transcoder >= Trans::TRANS_A);

    // TODO(fxbug.dev/109278): Allow transcoder D, once we support it.
    ZX_ASSERT(transcoder <= Trans::TRANS_C);

    const int transcoder_index = transcoder - Trans::TRANS_A;
    return hwreg::RegisterAddr<TranscoderConfig>(0x70008 + 0x1000 * transcoder_index);
  }
};

// TRANS_CLK_SEL (Transcoder Clock Select).
//
// On Kaby Lake and Skylake, the EDP transcoder always uses the DDI A clock, so
// it doesn't have a Clock Select register.
//
// On Tiger Lake, all reserved bits are MBZ (must be zero), so this register can
// be safely written without reading it first. On Kaby Lake and Skylake, the
// reserved bits are not documented as MBZ, so this register should be accessed
// using read-modify-write.
//
// Tiger Lake: IHD-OS-TGL-Vol 2c-1.22-Rev2.0 Part 2 pages 1365-1366
// Kaby Lake: IHD-OS-KBL-Vol 2c-1.17 Part 2 pages 947-948
// Skylake: IHD-OS-SKL-Vol 2c-05.16 Part 2 pages 922-923
class TranscoderClockSelect : public hwreg::RegisterBase<TranscoderClockSelect, uint32_t> {
 public:
  // Selects the DDI whose port clock is used by this transcoder.
  //
  // This field has a non-trivial value encoding. The ddi_*() and set_ddi_*()
  // helpers should be preferred to accessing the field directly.
  //
  // This field is tagged `_subtle` because the definition matches the bits used
  // on Tiger Lake, but it's used on all supported models. Kaby Lake and Skylake
  // have a very similar field, which only takes up bits 30-28. Fortunately,
  // bit 27 is reserved, and we can still paper over the field width difference
  // in the helpers `ddi_clock_kaby_lake()` and `set_ddi_clock_kaby_lake()`.
  DEF_FIELD(31, 28, ddi_clock_select_subtle);

  // The DDI whose port clock is used by the transcoder.
  //
  // This helper works for Kaby Lake and Skylake.
  //
  // This field must not be changed while the transcoder is enabled.
  std::optional<Ddi> ddi_clock_kaby_lake() const {
    // Shifting converts from the Tiger Lake field width. The cast is lossless
    // because `ddi_clock_select_subtle()` is a 4-bit field.
    const int8_t ddi_clock_select_raw_value = static_cast<int8_t>(ddi_clock_select_subtle() >> 1);
    if (ddi_clock_select_raw_value == 0) {
      return std::nullopt;
    }
    const int ddi_index = ddi_clock_select_raw_value - 1;
    return static_cast<Ddi>(ddi_index);
  }

  // The DDI whose port clock is used by the transcoder.
  //
  // This helper works for Tiger Lake.
  //
  // This field must not be changed while the transcoder is enabled.
  std::optional<Ddi> ddi_clock_tiger_lake() const {
    // The cast is lossless because `ddi_clock_select_subtle()` is a 4-bit field.
    const int8_t ddi_select_raw_value = static_cast<int8_t>(ddi_clock_select_subtle());
    if (ddi_select_raw_value == 0) {
      return std::nullopt;
    }
    const int ddi_index = ddi_select_raw_value - 1;
    return static_cast<Ddi>(ddi_index);
  }

  // See `ddi_clock_kaby_lake()` for details.
  TranscoderClockSelect& set_ddi_clock_kaby_lake(std::optional<Ddi> ddi) {
    ZX_DEBUG_ASSERT_MSG(!ddi.has_value() || ddi != Ddi::DDI_A,
                        "DDI A cannot be explicitly connected to a transcoder");

    const int8_t ddi_select_raw = RawDdiClockSelect(ddi);

    // Convert to the Tiger Lake field representation.
    const uint32_t reserved_bit = (ddi_clock_select_subtle() & 1);
    return set_ddi_clock_select_subtle((ddi_select_raw << 1) | reserved_bit);
  }

  // See `ddi_clock_tiger_lake()` for details.
  TranscoderClockSelect& set_ddi_clock_tiger_lake(std::optional<Ddi> ddi) {
    return set_ddi_clock_select_subtle(RawDdiClockSelect(ddi));
  }

  static auto GetForTranscoder(Trans transcoder) {
    ZX_ASSERT(transcoder >= Trans::TRANS_A);

    // TODO(fxbug.dev/109278): Allow transcoder D, once we support it.
    ZX_ASSERT(transcoder <= Trans::TRANS_C);

    const int transcoder_index = transcoder - Trans::TRANS_A;
    return hwreg::RegisterAddr<TranscoderClockSelect>(0x46140 + 4 * transcoder_index);
  }

 private:
  static int8_t RawDdiClockSelect(std::optional<Ddi> ddi) {
    if (!ddi.has_value()) {
      return 0;
    }
    // The cast is lossless because DDI indices fit in 4 bits.
    const int8_t ddi_index = static_cast<int8_t>(*ddi - Ddi::DDI_A);
    // The addition doesn't overflow and the cast is lossless because DDI
    // indices fit in 4 bits.
    return static_cast<int8_t>(ddi_index + 1);
  }
};

// DATAM / TRANS_DATAM1 (Transcoder Data M Value 1)
//
// This register is double-buffered and the update triggers when the first
// MSA (Main Stream Attributes packet) that is sent after LINKN is modified.
//
// All unassigned bits in this register are MBZ (must be zero), so it's safe to
// assign this register without reading its old value.
//
// Tiger Lake: IHD-OS-TGL-Vol 2c-1.22-Rev2.0 Part 1 pages 328-329
// Kaby Lake: IHD-OS-KBL-Vol 2c-1.17 Part 1 pages 427-428
// Skylake: IHD-OS-SKL-Vol 2c-05.16 Part 1 pages 422-423
class TranscoderDataM : public hwreg::RegisterBase<TranscoderDataM, uint32_t> {
 public:
  DEF_RSVDZ_BIT(31);

  // Selects the TU (transfer unit) or VC (Virtual Channel) payload size.
  //
  // This field has a non-trivial value encoding. The `payload_size()` and
  // `set_payload_size()` helpers should be preferred to accessing the field
  // directly.
  DEF_FIELD(30, 25, payload_size_select);

  // Selects the TU (transfer unit) or VC (Virtual Channel) payload size.
  //
  // In DisplayPort SST (Single Stream) mode, this field represents the TU
  // (transfer unit size), which is typically set to 64.
  //
  // In DisplayPort MST (Multi-Stream) mode, this field represents the Virtual
  // Channel payload size, which must be at most 63. This field must not be
  // changed while the transcoder is in MST mode, even if the transcoder is
  // disabled.
  int32_t payload_size() const {
    // The cast is lossless and the addition does not overflow (which would be
    // UB) because `payload_size_select()` is a 24-bit field.
    return static_cast<int32_t>(static_cast<int32_t>(payload_size_select()) + 1);
  }

  // See `payload_size()`.
  TranscoderDataM& set_payload_size(int payload_size) {
    ZX_DEBUG_ASSERT(payload_size > 0);
    return set_payload_size_select(payload_size - 1);
  }

  DEF_RSVDZ_BIT(24);

  // The M value in the data M/N ratio, which is used by the transcoder.
  DEF_FIELD(23, 0, m);

  static auto GetForKabyLakeTranscoder(Trans transcoder) {
    if (transcoder == Trans::TRANS_EDP) {
      return hwreg::RegisterAddr<TranscoderDataM>(0x6f030);
    }

    ZX_ASSERT(transcoder >= Trans::TRANS_A);
    ZX_ASSERT(transcoder <= Trans::TRANS_C);
    const int transcoder_index = transcoder - Trans::TRANS_A;
    return hwreg::RegisterAddr<TranscoderDataM>(0x60030 + 0x1000 * transcoder_index);
  }

  static auto GetForTigerLakeTranscoder(Trans transcoder) {
    ZX_ASSERT(transcoder >= Trans::TRANS_A);

    // TODO(fxbug.dev/109278): Allow transcoder D, once we support it.
    ZX_ASSERT(transcoder <= Trans::TRANS_C);

    const int transcoder_index = transcoder - Trans::TRANS_A;
    return hwreg::RegisterAddr<TranscoderDataM>(0x60030 + 0x1000 * transcoder_index);
  }
};

// DATAN / TRANS_DATAN1 (Transcoder Data N Value 1)
//
// This register is double-buffered and the update triggers when the first
// MSA (Main Stream Attributes packet) that is sent after LINKN is modified.
//
// All unassigned bits in this register are MBZ (must be zero), so it's safe to
// assign this register without reading its old value.
//
// Tiger Lake: IHD-OS-TGL-Vol 2c-1.22-Rev2.0 Part 1 page 330
// Kaby Lake: IHD-OS-KBL-Vol 2c-1.17 Part 1 page 429
// Skylake: IHD-OS-SKL-Vol 2c-05.16 Part 1 pages 424-425
class TranscoderDataN : public hwreg::RegisterBase<TranscoderDataN, uint32_t> {
 public:
  DEF_RSVDZ_FIELD(31, 24);

  // The N value in the data M/N ratio, which is used by the transcoder.
  DEF_FIELD(23, 0, n);

  static auto GetForKabyLakeTranscoder(Trans transcoder) {
    if (transcoder == Trans::TRANS_EDP) {
      return hwreg::RegisterAddr<TranscoderDataN>(0x6f034);
    }

    ZX_ASSERT(transcoder >= Trans::TRANS_A);
    ZX_ASSERT(transcoder <= Trans::TRANS_C);
    const int transcoder_index = transcoder - Trans::TRANS_A;
    return hwreg::RegisterAddr<TranscoderDataN>(0x60034 + 0x1000 * transcoder_index);
  }

  static auto GetForTigerLakeTranscoder(Trans transcoder) {
    ZX_ASSERT(transcoder >= Trans::TRANS_A);

    // TODO(fxbug.dev/109278): Allow transcoder D, once we support it.
    ZX_ASSERT(transcoder <= Trans::TRANS_C);

    const int transcoder_index = transcoder - Trans::TRANS_A;
    return hwreg::RegisterAddr<TranscoderDataN>(0x60034 + 0x1000 * transcoder_index);
  }
};

// LINKM / TRANS_LINKM1 (Transcoder Link M Value 1)
//
// This register is double-buffered and the update triggers when the first
// MSA (Main Stream Attributes packet) that is sent after LINKN is modified.
//
// All unassigned bits in this register are MBZ (must be zero), so it's safe to
// assign this register without reading its old value.
//
// Tiger Lake: IHD-OS-TGL-Vol 2c-1.22-Rev2.0 Part 1 page 1300
// Kaby Lake: IHD-OS-KBL-Vol 2c-1.17 Part 1 page 1123
// Skylake: IHD-OS-SKL-Vol 2c-05.16 Part 1 pages 1112-1113
class TranscoderLinkM : public hwreg::RegisterBase<TranscoderLinkM, uint32_t> {
 public:
  DEF_RSVDZ_FIELD(31, 24);

  // The M value in the link M/N ratio transmitted in the MSA packet.
  DEF_FIELD(23, 0, m);

  static auto GetForKabyLakeTranscoder(Trans transcoder) {
    if (transcoder == Trans::TRANS_EDP) {
      return hwreg::RegisterAddr<TranscoderLinkM>(0x6f040);
    }

    ZX_ASSERT(transcoder >= Trans::TRANS_A);
    ZX_ASSERT(transcoder <= Trans::TRANS_C);
    const int transcoder_index = transcoder - Trans::TRANS_A;
    return hwreg::RegisterAddr<TranscoderLinkM>(0x60040 + 0x1000 * transcoder_index);
  }

  static auto GetForTigerLakeTranscoder(Trans transcoder) {
    ZX_ASSERT(transcoder >= Trans::TRANS_A);

    // TODO(fxbug.dev/109278): Allow transcoder D, once we support it.
    ZX_ASSERT(transcoder <= Trans::TRANS_C);

    const int transcoder_index = transcoder - Trans::TRANS_A;
    return hwreg::RegisterAddr<TranscoderLinkM>(0x60040 + 0x1000 * transcoder_index);
  }
};

// LINKN / TRANS_LINKN1 (Transcoder Link N Value 1)
//
// Updating this register triggers an update of all double-buffered M/N
// registers (DATAM, DATAN, LINKM, LINKN) for the transcoder.
//
// All unassigned bits in this register are MBZ (must be zero), so it's safe to
// assign this register without reading its old value.
//
// Tiger Lake: IHD-OS-TGL-Vol 2c-1.22-Rev2.0 Part 1 page 1301
// Kaby Lake: IHD-OS-KBL-Vol 2c-1.17 Part 1 page 1124
// Skylake: IHD-OS-SKL-Vol 2c-05.16 Part 1 pages 1114-1115
class TranscoderLinkN : public hwreg::RegisterBase<TranscoderLinkN, uint32_t> {
 public:
  DEF_RSVDZ_FIELD(31, 24);

  // The N value in the link M/N ratio transmitted in the MSA packet. This is
  // also transmitted in the VB-ID (Vertical Blanking ID).
  DEF_FIELD(23, 0, n);

  static auto GetForKabyLakeTranscoder(Trans transcoder) {
    if (transcoder == Trans::TRANS_EDP) {
      return hwreg::RegisterAddr<TranscoderLinkN>(0x6f044);
    }

    ZX_ASSERT(transcoder >= Trans::TRANS_A);
    ZX_ASSERT(transcoder <= Trans::TRANS_C);
    const int transcoder_index = transcoder - Trans::TRANS_A;
    return hwreg::RegisterAddr<TranscoderLinkN>(0x60044 + 0x1000 * transcoder_index);
  }

  static auto GetForTigerLakeTranscoder(Trans transcoder) {
    ZX_ASSERT(transcoder >= Trans::TRANS_A);

    // TODO(fxbug.dev/109278): Allow transcoder D, once we support it.
    ZX_ASSERT(transcoder <= Trans::TRANS_C);

    const int transcoder_index = transcoder - Trans::TRANS_A;
    return hwreg::RegisterAddr<TranscoderLinkN>(0x60044 + 0x1000 * transcoder_index);
  }
};

// Documented values for the DisplayPort MSA MISC0 field's bits 7:5.
//
// The values come from the VESA DisplayPort Standard Version 2.0, Table 2-96
// "MSA MISC1 and MISC0 Fields for Pixel Encoding/Colorimetry Format Indication"
// at page 158. The table belongs to Section 2.2.4 "MSA Data Transport".
//
// The encoding here is correct for all modes except for RAW, which uses a
// different encoding.
//
// TODO(fxbug.dev/105221): This covers a general DisplayPort concept, so it
// belongs in a general-purpose DisplayPort support library.
enum class DisplayPortMsaBitsPerComponent {
  k6Bpc = 0,
  k8Bpc = 1,
  k10Bpc = 2,
  k12Bpc = 3,
  k16Bpc = 4,
};

// Documented values for the DisplayPort MSA MISC0 field's bits 4:1.
//
// The values come from the VESA DisplayPort Standard Version 2.0, Table 2-96
// "MSA MISC1 and MISC0 Fields for Pixel Encoding/Colorimetry Format Indication"
// at page 158. The table belongs to Section 2.2.4 "MSA Data Transport".
//
// TODO(fxbug.dev/105221): This covers a general DisplayPort concept, so it
// belongs in a general-purpose DisplayPort support library.
enum class DisplayPortMsaColorimetry {
  kRgbUnspecifiedLegacy = 0b0'0'00,
  kCtaSrgb = 0b0'1'00,
  kRgbWideGamutFixed = 0b0'0'11,
  kRgbWideGamutFloating = 0b1'0'00,
  kYCbCr422Bt601 = 0b0'1'01,
  kYCbCr422Bt709 = 0b1'1'01,
  kYCbCr444Bt601 = 0b0'1'10,
  kYCbCr444Bt709 = 0b1'1'10,
  kAdobeRgb = 0b1'1'00,
  kDciP3 = 0b0'1'11,

  // The color profile will be sent as a MCCS (VESA Monitor Control Command)
  // VCP (Virtual Control Panel).
  kVcpColorProfile = 0b0'1'11,
};

// TRANS_MSA_MISC (Transcoder Main Stream Attribute Miscellaneous)
//
// All reserved fields in this register are MBZ (must be zero), so it can be
// safely written without a prior read.
//
// Tiger Lake: IHD-OS-TGL-Vol 2c-1.22-Rev2.0 Part 2 pages 1394-1395
// Kaby Lake: IHD-OS-KBL-Vol 2c-1.17 Part 2 pages 947-948
// Skylake: IHD-OS-SKL-Vol 2c-05.16 Part 2 pages 922-923
//
// MISC fields: VESA DisplayPort Standard Version 2.0, Section 2.2.4
// "MSA Data Transport", pages 149-151 and 157-158.
class TranscoderMainStreamAttributeMisc
    : public hwreg::RegisterBase<TranscoderMainStreamAttributeMisc, uint32_t> {
 public:
  // TODO(fxbug.dev/105221): The MSA field definitions are a general DisplayPort
  // concept, and belong in a general-purpose DisplayPort support library. Once
  // we have that, this register's definition should only map MSA fields to
  // register bytes, matching the PRM.

  // Bits 31:16 are document as the value transmitted in the MSA unused fields.
  //
  // The VESA DisplayPort Standard Version 2.0, Figure 2-18 "DP MSA Packet
  // Transport Mapping over Main-Link", page 152 states this field must be zero.
  DEF_RSVDZ_FIELD(31, 16);

  // Bits 15:8 are the MISC1 MSA field from the DisplayPort standard.

  // True for Y (luminance)-only and RAW formats.
  //
  // We don't currently support these color formats.
  DEF_BIT(15, colorimetry_top_bit);

  // If true, the colorimetry information is sent separately, in a VSC SDP.
  //
  // This must only be used if the sink's DPRX_FEATURE_ENUMERATION_LIST register
  // has VSC_SDP_EXTENSION_FOR_COLORIMETRY_SUPPORTED set.
  //
  // Including colorimetry information in the VSC (Video Stream Configuration)
  // SDP (Secondary Data Packet) is described in the VESA DisplayPort Standard
  // Version 2.0, Section 2.2.5.6.5 "VSC SDP Payload for Pixel
  // Encoding/Colorimetry Format", pages 203-205
  //
  // This field was introduced in DisplayPort 1.3. Prior to that, the underlying
  // bit was MBZ (must be zero).
  //
  // We don't currently support this feature.
  DEF_BIT(14, colorimetry_in_vsc_sdp);

  // Reserved in the DisplayPort 2.0 standard, must be zero.
  DEF_RSVDZ_FIELD(13, 11);

  // If the "FS MSA MISC1 Drive Enable" field in the TRANS_STEREO3D_CTL register
  // is true, this field is ignored, and the display hardware drives the
  // corresponding MSA bits.
  DEF_FIELD(10, 9, stereo_video);

  // True iff the number of lines per interlaced frame (two fields) is even.
  DEF_BIT(8, interlaced_vertical_total_even);

  // Bits 7:0 are the MSA MISC0 field from the DisplayPort standard.

  // The bpc (number of bits per color component) for the selected format.
  //
  // Some bpc values are not supported by some colorimetry modes. For example,
  // the RGB wide gamut fixed point only supports 8, 10, and 12bpc.
  DEF_ENUM_FIELD(DisplayPortMsaBitsPerComponent, 7, 5, bits_per_component_select);

  // Selects the pixel encoding and colorimetry format.
  //
  // See the `DisplayPortMainStreamAttributeColorimetry` comments for details.
  DEF_ENUM_FIELD(DisplayPortMsaColorimetry, 4, 1, colorimetry_select);

  // If true, the main link clock and video stream clock are synchronous.
  //
  // Before DisplayPort is enabled, this field must be set to true.
  DEF_BIT(0, video_stream_clock_sync_with_link_clock);

  static auto GetForKabyLakeTranscoder(Trans transcoder) {
    if (transcoder == Trans::TRANS_EDP) {
      return hwreg::RegisterAddr<TranscoderMainStreamAttributeMisc>(0x6f410);
    }

    ZX_ASSERT(transcoder >= Trans::TRANS_A);
    ZX_ASSERT(transcoder <= Trans::TRANS_C);
    const int transcoder_index = transcoder - Trans::TRANS_A;
    return hwreg::RegisterAddr<TranscoderMainStreamAttributeMisc>(0x60410 +
                                                                  0x1000 * transcoder_index);
  }

  static auto GetForTigerLakeTranscoder(Trans transcoder) {
    ZX_ASSERT(transcoder >= Trans::TRANS_A);

    // TODO(fxbug.dev/109278): Allow transcoder D, once we support it.
    ZX_ASSERT(transcoder <= Trans::TRANS_C);

    const int transcoder_index = transcoder - Trans::TRANS_A;
    return hwreg::RegisterAddr<TranscoderClockSelect>(0x60410 + 0x1000 * transcoder_index);
  }
};

class TranscoderRegs {
 public:
  explicit TranscoderRegs(Trans transcoder)
      : transcoder_(transcoder),
        offset_(transcoder_ == TRANS_EDP ? 0xf000 : (transcoder_ * 0x1000)) {}

  hwreg::RegisterAddr<TransHVTotal> HTotal() { return GetReg<TransHVTotal>(0x60000); }
  hwreg::RegisterAddr<TransHVTotal> HBlank() { return GetReg<TransHVTotal>(0x60004); }
  hwreg::RegisterAddr<TransHVSync> HSync() { return GetReg<TransHVSync>(0x60008); }
  hwreg::RegisterAddr<TransHVTotal> VTotal() { return GetReg<TransHVTotal>(0x6000c); }
  hwreg::RegisterAddr<TransHVTotal> VBlank() { return GetReg<TransHVTotal>(0x60010); }
  hwreg::RegisterAddr<TransHVSync> VSync() { return GetReg<TransHVSync>(0x60014); }
  hwreg::RegisterAddr<TransVSyncShift> VSyncShift() { return GetReg<TransVSyncShift>(0x60028); }

  hwreg::RegisterAddr<TranscoderDdiControl> DdiControl() {
    // This works for Tiger Lake too, because the supported transcoders are a
    // subset of the Kaby Lake transcoders, and the MMIO addresses for these
    // transcoders are the same.
    return TranscoderDdiControl::GetForKabyLakeTranscoder(transcoder_);
  }
  hwreg::RegisterAddr<TranscoderConfig> Config() {
    // This works for Tiger Lake too, because the supported transcoders are a
    // subset of the Kaby Lake transcoders, and the MMIO addresses for these
    // transcoders are the same.
    return TranscoderConfig::GetForKabyLakeTranscoder(transcoder_);
  }
  hwreg::RegisterAddr<TranscoderClockSelect> ClockSelect() {
    return TranscoderClockSelect::GetForTranscoder(transcoder_);
  }
  hwreg::RegisterAddr<TranscoderMainStreamAttributeMisc> MainStreamAttributeMisc() {
    // This works for Tiger Lake too, because the supported transcoders are a
    // subset of the Kaby Lake transcoders, and the MMIO addresses for these
    // transcoders are the same.
    return TranscoderMainStreamAttributeMisc::GetForKabyLakeTranscoder(transcoder_);
  }

  hwreg::RegisterAddr<TranscoderDataM> DataM() {
    // This works for Tiger Lake too, because the supported transcoders are a
    // subset of the Kaby Lake transcoders, and the MMIO addresses for these
    // transcoders are the same.
    return TranscoderDataM::GetForKabyLakeTranscoder(transcoder_);
  }
  hwreg::RegisterAddr<TranscoderDataN> DataN() {
    // This works for Tiger Lake too, because the supported transcoders are a
    // subset of the Kaby Lake transcoders, and the MMIO addresses for these
    // transcoders are the same.
    return TranscoderDataN::GetForKabyLakeTranscoder(transcoder_);
  }
  hwreg::RegisterAddr<TranscoderLinkM> LinkM() {
    // This works for Tiger Lake too, because the supported transcoders are a
    // subset of the Kaby Lake transcoders, and the MMIO addresses for these
    // transcoders are the same.
    return TranscoderLinkM::GetForKabyLakeTranscoder(transcoder_);
  }
  hwreg::RegisterAddr<TranscoderLinkN> LinkN() {
    // This works for Tiger Lake too, because the supported transcoders are a
    // subset of the Kaby Lake transcoders, and the MMIO addresses for these
    // transcoders are the same.
    return TranscoderLinkN::GetForKabyLakeTranscoder(transcoder_);
  }

 private:
  template <class RegType>
  hwreg::RegisterAddr<RegType> GetReg(uint32_t base_addr) {
    return hwreg::RegisterAddr<RegType>(base_addr + offset_);
  }

  Trans transcoder_;
  uint32_t offset_;
};
}  // namespace tgl_registers

#endif  // SRC_GRAPHICS_DISPLAY_DRIVERS_INTEL_I915_TGL_REGISTERS_TRANSCODER_H_
