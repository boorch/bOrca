#include "sim.h"
#include "gbuffer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// stored unique random value
Usz last_random_unique = UINT_MAX;

// Note Sequence
static char note_sequence[] = "CcDdEFfGgAaB";

Usz find_note_index(Glyph root_note_glyph) {
  for (Usz i = 0; i < sizeof(note_sequence) - 1; i++) {
    if (note_sequence[i] == root_note_glyph)
      return i;
  }
  return UINT_MAX; // Indicate not found
}

//////// Utilities

static Glyph const glyph_table[36] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', //  0-11
    'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', // 12-23
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', // 24-35
};
enum { Glyphs_index_count = sizeof glyph_table };
static inline Glyph glyph_of(Usz index) {
  assert(index < Glyphs_index_count);
  return glyph_table[index];
}

static U8 const index_table[128] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  //   0-15
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  //  16-31
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  //  32-47
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0,  //  48-63
    0,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, //  64-79
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 0,  0,  0,  0,  0,  //  80-95
    0,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, //  96-111
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 0,  0,  0,  0,  0}; // 112-127
static ORCA_FORCEINLINE Usz index_of(Glyph c) { return index_table[c & 0x7f]; }

// Reference implementation:
// static Usz index_of(Glyph c) {
//   if (c >= '0' && c <= '9') return (Usz)(c - '0');
//   if (c >= 'A' && c <= 'Z') return (Usz)(c - 'A' + 10);
//   if (c >= 'a' && c <= 'z') return (Usz)(c - 'a' + 10);
//   return 0;
// }

static ORCA_FORCEINLINE bool glyph_is_lowercase(Glyph g) { return g & 1 << 5; }
static ORCA_FORCEINLINE Glyph glyph_lowered_unsafe(Glyph g) {
  return (Glyph)(g | 1 << 5);
}
static inline Glyph glyph_with_case(Glyph g, Glyph caser) {
  enum { Case_bit = 1 << 5, Alpha_bit = 1 << 6 };
  return (Glyph)((g & ~Case_bit) | ((~g & Alpha_bit) >> 1) |
                 (caser & Case_bit));
}

static ORCA_PURE bool oper_has_neighboring_bang(Glyph const *gbuf, Usz h, Usz w,
                                                Usz y, Usz x) {
  Glyph const *gp = gbuf + w * y + x;
  if (x < w - 1 && gp[1] == '*')
    return true;
  if (x > 0 && *(gp - 1) == '*')
    return true;
  if (y < h - 1 && gp[w] == '*')
    return true;
  // note: negative array subscript on rhs of short-circuit, may cause ub if
  // the arithmetic under/overflows, even if guarded the guard on lhs is false
  if (y > 0 && *(gp - w) == '*')
    return true;
  return false;
}

// Returns UINT8_MAX if not a valid note.
static U8 midi_note_number_of(Glyph g) {
  int sharp = (g & 1 << 5) >> 5; // sharp=1 if lowercase
  g &= (Glyph) ~(1 << 5);        // make uppercase
  if (g < 'A' || g > 'Z')        // A through Z only
    return UINT8_MAX;
  // We want C=0, D=1, E=2, etc. A and B are equivalent to H and I.
  int deg = g <= 'B' ? 'G' - 'B' + g - 'A' : g - 'C';
  return (U8)(deg / 7 * 12 + (I8[]){0, 2, 4, 5, 7, 9, 11}[deg % 7] + sharp);
}

typedef struct {
  Glyph *vars_slots;
  Oevent_list *oevent_list;
  Usz random_seed;
} Oper_extra_params;

static void oper_poke_and_stun(Glyph *restrict gbuffer, Mark *restrict mbuffer,
                               Usz height, Usz width, Usz y, Usz x, Isz delta_y,
                               Isz delta_x, Glyph g) {
  Isz y0 = (Isz)y + delta_y;
  Isz x0 = (Isz)x + delta_x;
  if (y0 < 0 || x0 < 0 || (Usz)y0 >= height || (Usz)x0 >= width)
    return;
  Usz offs = (Usz)y0 * width + (Usz)x0;
  gbuffer[offs] = g;
  mbuffer[offs] |= Mark_flag_sleep;
}

// For anyone editing this in the future: the "no inline" here is deliberate.
// You may think that inlining is always faster. Or even just letting the
// compiler decide. You would be wrong. Try it. If you really want this VM to
// run faster, you will need to use computed goto or assembly.
#define OPER_FUNCTION_ATTRIBS ORCA_NOINLINE static void

#define BEGIN_OPERATOR(_oper_name)                                             \
  OPER_FUNCTION_ATTRIBS oper_behavior_##_oper_name(                            \
      Glyph *const restrict gbuffer, Mark *const restrict mbuffer,             \
      Usz const height, Usz const width, Usz const y, Usz const x,             \
      Usz Tick_number, Oper_extra_params *const extra_params,                  \
      Mark const cell_flags, Glyph const This_oper_char) {                     \
    (void)gbuffer;                                                             \
    (void)mbuffer;                                                             \
    (void)height;                                                              \
    (void)width;                                                               \
    (void)y;                                                                   \
    (void)x;                                                                   \
    (void)Tick_number;                                                         \
    (void)extra_params;                                                        \
    (void)cell_flags;                                                          \
    (void)This_oper_char;

#define END_OPERATOR }

#define PEEK(_delta_y, _delta_x)                                               \
  gbuffer_peek_relative(gbuffer, height, width, y, x, _delta_y, _delta_x)
#define POKE(_delta_y, _delta_x, _glyph)                                       \
  gbuffer_poke_relative(gbuffer, height, width, y, x, _delta_y, _delta_x,      \
                        _glyph)
#define STUN(_delta_y, _delta_x)                                               \
  mbuffer_poke_relative_flags_or(mbuffer, height, width, y, x, _delta_y,       \
                                 _delta_x, Mark_flag_sleep)
#define POKE_STUNNED(_delta_y, _delta_x, _glyph)                               \
  oper_poke_and_stun(gbuffer, mbuffer, height, width, y, x, _delta_y,          \
                     _delta_x, _glyph)
#define LOCK(_delta_y, _delta_x)                                               \
  mbuffer_poke_relative_flags_or(mbuffer, height, width, y, x, _delta_y,       \
                                 _delta_x, Mark_flag_lock)

#define IN Mark_flag_input
#define OUT Mark_flag_output
#define NONLOCKING Mark_flag_lock
#define PARAM Mark_flag_haste_input

#define LOWERCASE_REQUIRES_BANG                                                \
  if (glyph_is_lowercase(This_oper_char) &&                                    \
      !oper_has_neighboring_bang(gbuffer, height, width, y, x))                \
  return

#define STOP_IF_NOT_BANGED                                                     \
  if (!oper_has_neighboring_bang(gbuffer, height, width, y, x))                \
  return

#define PORT(_delta_y, _delta_x, _flags)                                       \
  mbuffer_poke_relative_flags_or(mbuffer, height, width, y, x, _delta_y,       \
                                 _delta_x, (_flags) ^ Mark_flag_lock)
//////// Operators

#define UNIQUE_OPERATORS(_)                                                    \
  _('!', midicc)                                                               \
  _('#', comment)                                                              \
  _('$', scale)                                                                \
  _('%', midi)                                                                 \
  _('*', bang)                                                                 \
  _(':', midi)                                                                 \
  _(';', bouncer)                                                              \
  _('=', midichord)                                                            \
  _('?', midipb)                                                               \
  _('|', midipoly)                                                             \
  _('&', arpeggiator)

#define ALPHA_OPERATORS(_)                                                     \
  _('A', add)                                                                  \
  _('B', subtract)                                                             \
  _('C', clock)                                                                \
  _('D', delay)                                                                \
  _('E', movement)                                                             \
  _('F', if)                                                                   \
  _('G', generator)                                                            \
  _('H', halt)                                                                 \
  _('I', increment)                                                            \
  _('J', jump)                                                                 \
  _('K', konkat)                                                               \
  _('L', lesser)                                                               \
  _('M', multiply)                                                             \
  _('N', movement)                                                             \
  _('O', offset)                                                               \
  _('P', push)                                                                 \
  _('Q', query)                                                                \
  _('R', random)                                                               \
  _('S', movement)                                                             \
  _('T', track)                                                                \
  _('U', uclid)                                                                \
  _('V', variable)                                                             \
  _('W', movement)                                                             \
  _('X', teleport)                                                             \
  _('Y', yump)                                                                 \
  _('Z', lerp)

BEGIN_OPERATOR(movement)
  if (glyph_is_lowercase(This_oper_char) &&
      !oper_has_neighboring_bang(gbuffer, height, width, y, x))
    return;
  Isz delta_y, delta_x;
  switch (glyph_lowered_unsafe(This_oper_char)) {
  case 'n':
    delta_y = -1;
    delta_x = 0;
    break;
  case 'e':
    delta_y = 0;
    delta_x = 1;
    break;
  case 's':
    delta_y = 1;
    delta_x = 0;
    break;
  case 'w':
    delta_y = 0;
    delta_x = -1;
    break;
  default:
    // could cause strict aliasing problem, maybe
    delta_y = 0;
    delta_x = 0;
    break;
  }
  Isz y0 = (Isz)y + delta_y;
  Isz x0 = (Isz)x + delta_x;
  if (y0 >= (Isz)height || x0 >= (Isz)width || y0 < 0 || x0 < 0) {
    gbuffer[y * width + x] = '*';
    return;
  }
  Glyph *restrict g_at_dest = gbuffer + (Usz)y0 * width + (Usz)x0;
  if (*g_at_dest == '.') {
    *g_at_dest = This_oper_char;
    gbuffer[y * width + x] = '.';
    mbuffer[(Usz)y0 * width + (Usz)x0] |= Mark_flag_sleep;
  } else {
    gbuffer[y * width + x] = '*';
  }
END_OPERATOR

// BEGIN_OPERATOR(midicc)
//   for (Usz i = 1; i < 4; ++i) {
//     PORT(0, (Isz)i, IN);
//   }
//   STOP_IF_NOT_BANGED;
//   Glyph channel_g = PEEK(0, 1);
//   Glyph control_g = PEEK(0, 2);
//   Glyph value_g = PEEK(0, 3);
//   if (channel_g == '.' || control_g == '.')
//     return;
//   Usz channel = index_of(channel_g);
//   if (channel > 15)
//     return;
//   PORT(0, 0, OUT);
//   Oevent_midi_cc *oe =
//       (Oevent_midi_cc *)oevent_list_alloc_item(extra_params->oevent_list);
//   oe->oevent_type = Oevent_type_midi_cc;
//   oe->channel = (U8)channel;
//   oe->control = (U8)index_of(control_g);
//   oe->value = (U8)(index_of(value_g) * 127 / 35); // 0~35 -> 0~127
// END_OPERATOR

// BOORCH's INTERPOLATED MIDI CC
// Updated Midicc_state with floating-point values
// typedef struct {
//   bool active;
//   double current_value; // Current interpolated MIDI CC value
//   double target_value;  // Target MIDI CC value
//   double step_size;     // Increment per step
//   Usz steps_remaining;  // Steps left for interpolation
//   Usz channel;          // MIDI channel
//   Usz control;          // MIDI control number
// } Midicc_state;

// // Define the state array for midicc operators
// #define MAX_SIM_GRID_SIZE 4096 // Adjust based on your grid dimensions
// static Midicc_state midicc_states[MAX_SIM_GRID_SIZE] = {0};

// // Convert ASCII character to hex value (0-15)
// static inline U8 hex_value(Glyph g) {
//   Usz idx = index_of(g);
//   if (idx <= 9) {
//     return (U8)idx; // 0-9
//   }
//   if (idx >= 10 && idx <= 15) {
//     return (U8)idx; // A-F -> 10-15
//   }
//   return 0;
// }

// Updated midicc operator with HEX value for CC number and optional interpolate (rightmost input, increase resolution mostly at the cost of speed & range)
// BEGIN_OPERATOR(midicc)
//   // Define input ports - now with 5 inputs
//   for (Usz i = 1; i < 6; ++i) {
//     PORT(0, (Isz)i, IN);
//   }

//   PORT(0, 0, OUT); // Mark output immediately

//   STOP_IF_NOT_BANGED;

//   // Calculate state index and bounds check
//   Usz state_idx = y * width + x;
//   if (state_idx >= MAX_SIM_GRID_SIZE)
//     return;

//   Midicc_state *state = &midicc_states[state_idx];

//   // Reset state on first tick
//   if (Tick_number == 0) {
//     state->active = false;
//     state->current_value = 0;
//     state->target_value = 0;
//     state->step_size = 0;
//     state->steps_remaining = 0;
//     state->channel = 0;
//     state->control = 0;
//   }

//   Glyph channel_g = PEEK(0, 1);
//   Glyph control_high_g = PEEK(0, 2);
//   Glyph control_low_g = PEEK(0, 3);
//   Glyph value_g = PEEK(0, 4);
//   Glyph rate_g = PEEK(0, 5);

//   // Validate inputs
//   if (channel_g == '.' || control_high_g == '.' || control_low_g == '.' ||
//       value_g == '.') {
//     state->active = false;
//     return;
//   }

//   Usz channel = index_of(channel_g);
//   if (channel > 15) {
//     state->active = false;
//     return;
//   }

//   // Calculate control number from high and low parts (hex interpretation)
//   U8 control_high = hex_value(control_high_g); // 0-15
//   U8 control_low = hex_value(control_low_g);   // 0-15
//   U8 control = (U8)((control_high & 0x0F) << 4) | (control_low & 0x0F);

//   // Clamp to valid CC range
//   if (control > 127)
//     control = 127;

//   // Map glyph value to 0-127 MIDI range
//   double target_value = (double)(index_of(value_g) * 127) / 35.0;
//   if (target_value > 127.0)
//     target_value = 127.0;
//   if (target_value < 0.0)
//     target_value = 0.0;

//   Usz rate = index_of(rate_g);
//   if (rate == 0)
//     rate = 1;

// // Cap the rate to ensure it does not exceed 24 PPU
// #define MAX_RATE 24
//   if (rate > MAX_RATE)
//     rate = MAX_RATE;

//   // Calculate steps based on rate for two ticks
//   Usz steps = rate * 2; // Ensures interpolation completes within two ticks
//   if (steps == 0)
//     steps = 1;

//   // Reset state if channel or control changes
//   if (state->active &&
//       (state->channel != channel || state->control != control)) {
//     state->active = false;
//   }

//   // On bang or active state, initialize or update
//   if (oper_has_neighboring_bang(gbuffer, height, width, y, x) ||
//       state->active) {
//     if (!state->active) {
//       // New activation
//       state->active = true;
//       state->channel = channel;
//       state->control = control;
//       state->target_value = target_value;
//       state->current_value =
//           target_value; // Start from target on first activation
//       state->steps_remaining =
//           0; // Will start interpolating on next value change
//     } else {
//       // Already active, update target and recalculate
//       double current_value = state->current_value;
//       double delta = target_value - current_value;
//       state->target_value = target_value;
//       state->steps_remaining = steps;
//       state->step_size = delta / (double)steps;
//       if (state->step_size == 0.0 && delta != 0.0) {
//         state->step_size = (delta > 0.0) ? 1.0 : -1.0;
//       }
//     }
//   }

//   // If active, send interpolated MIDI CC message
//   if (state->active && state->steps_remaining > 0) {
//     state->current_value += state->step_size;

//     // Clamp to target value if we've exceeded it
//     if ((state->step_size > 0.0 &&
//          state->current_value > state->target_value) ||
//         (state->step_size < 0.0 &&
//          state->current_value < state->target_value)) {
//       state->current_value = state->target_value;
//     }

//     // Clamp to valid MIDI range
//     if (state->current_value > 127.0)
//       state->current_value = 127.0;
//     if (state->current_value < 0.0)
//       state->current_value = 0.0;

//     // Allocate and send MIDI CC event
//     Oevent_midi_cc *oe =
//         (Oevent_midi_cc *)oevent_list_alloc_item(extra_params->oevent_list);
//     oe->oevent_type = Oevent_type_midi_cc;
//     oe->channel = (U8)state->channel;
//     oe->control = control;
//     oe->value = (U8)(state->current_value + 0.5); // Round to nearest integer

//     // Update steps remaining and deactivate if done
//     state->steps_remaining--;
//     if (state->steps_remaining == 0) {
//       state->active = false;
//       state->current_value = state->target_value; // Ensure exact target reached
//     }
//   }
// END_OPERATOR

BEGIN_OPERATOR(midicc)
  for (Usz i = 1; i < 5; ++i) {
    PORT(0, (Isz)i, IN);
  }
  STOP_IF_NOT_BANGED;
  Glyph channel_g = PEEK(0, 1);
  Glyph control_a = PEEK(0, 2);
  Glyph control_b = PEEK(0, 3);
  Glyph value_g = PEEK(0, 4);

  if (channel_g == '.' || control_a == '.' || control_b == '.' ||
      value_g == '.')
    return;
  Usz channel = index_of(channel_g);
  if (channel > 15)
    return;
  PORT(0, 0, OUT);
  Oevent_midi_cc *oe =
      (Oevent_midi_cc *)oevent_list_alloc_item(extra_params->oevent_list);
  oe->oevent_type = Oevent_type_midi_cc;
  oe->channel = (U8)channel;
  oe->control = (U8)((index_of(control_a) * 10) + (index_of(control_b)));
  oe->value = (U8)(index_of(value_g) * 127 / 35); // 0~35 → 0~127
END_OPERATOR

BEGIN_OPERATOR(comment)
  // restrict probably ok here...
  Glyph const *restrict gline = gbuffer + y * width;
  Mark *restrict mline = mbuffer + y * width;
  Usz max_x = x + 255;
  if (width < max_x)
    max_x = width;
  for (Usz x0 = x + 1; x0 < max_x; ++x0) {
    Glyph g = gline[x0];
    mline[x0] |= (Mark)Mark_flag_lock;
    if (g == '#')
      break;
  }
END_OPERATOR

BEGIN_OPERATOR(bang)
  gbuffer_poke(gbuffer, height, width, y, x, '.');
END_OPERATOR

BEGIN_OPERATOR(midi)
  for (Usz i = 1; i < 6; ++i) {
    PORT(0, (Isz)i, IN);
  }
  STOP_IF_NOT_BANGED;
  Glyph channel_g = PEEK(0, 1);
  Glyph octave_g = PEEK(0, 2);
  Glyph note_g = PEEK(0, 3);
  Glyph velocity_g = PEEK(0, 4);
  Glyph length_g = PEEK(0, 5);
  U8 octave_num = (U8)index_of(octave_g);
  if (octave_g == '.')
    return;
  if (octave_num > 9)
    octave_num = 9;
  U8 note_num = midi_note_number_of(note_g);
  if (note_num == UINT8_MAX)
    return;
  Usz channel_num = index_of(channel_g);
  if (channel_num > 15)
    channel_num = 15;
  Usz vel_num;
  if (velocity_g == '.') {
    vel_num = 127;
  } else {
    vel_num = index_of(velocity_g);
    if (vel_num == 0)
      return;
    vel_num = vel_num * 8 - 1;
    if (vel_num > 127)
      vel_num = 127;
  }
  PORT(0, 0, OUT);
  Oevent_midi_note *oe =
      (Oevent_midi_note *)oevent_list_alloc_item(extra_params->oevent_list);
  oe->oevent_type = (U8)Oevent_type_midi_note;
  oe->channel = (U8)channel_num;
  oe->octave = octave_num;
  oe->note = note_num;
  oe->velocity = (U8)vel_num;
  oe->duration = (U8)(index_of(length_g) & 0x7Fu);
  oe->mono = This_oper_char == '%' ? 1 : 0;
END_OPERATOR

// BEGIN_OPERATOR(udp)
//   Usz n = width - x - 1;
//   if (n > 16)
//     n = 16;
//   Glyph const *restrict gline = gbuffer + y * width + x + 1;
//   Mark *restrict mline = mbuffer + y * width + x + 1;
//   Glyph cpy[Oevent_udp_string_count];
//   Usz i;
//   for (i = 0; i < n; ++i) {
//     Glyph g = gline[i];
//     if (g == '.')
//       break;
//     cpy[i] = g;
//     mline[i] |= Mark_flag_lock;
//   }
//   n = i;
//   STOP_IF_NOT_BANGED;
//   PORT(0, 0, OUT);
//   Oevent_udp_string *oe =
//       (Oevent_udp_string *)oevent_list_alloc_item(extra_params->oevent_list);
//   oe->oevent_type = (U8)Oevent_type_udp_string;
//   oe->count = (U8)n;
//   for (i = 0; i < n; ++i) {
//     oe->chars[i] = cpy[i];
//   }
// END_OPERATOR

// BEGIN_OPERATOR(osc)
//   PORT(0, 1, IN | PARAM);
//   PORT(0, 2, IN | PARAM);
//   Usz len = index_of(PEEK(0, 2));
//   if (len > Oevent_osc_int_count)
//     len = Oevent_osc_int_count;
//   for (Usz i = 0; i < len; ++i) {
//     PORT(0, (Isz)i + 3, IN);
//   }
//   STOP_IF_NOT_BANGED;
//   Glyph g = PEEK(0, 1);
//   if (g != '.') {
//     PORT(0, 0, OUT);
//     U8 buff[Oevent_osc_int_count];
//     for (Usz i = 0; i < len; ++i) {
//       buff[i] = (U8)index_of(PEEK(0, (Isz)i + 3));
//     }
//     Oevent_osc_ints *oe =
//         &oevent_list_alloc_item(extra_params->oevent_list)->osc_ints;
//     oe->oevent_type = (U8)Oevent_type_osc_ints;
//     oe->glyph = g;
//     oe->count = (U8)len;
//     for (Usz i = 0; i < len; ++i) {
//       oe->numbers[i] = buff[i];
//     }
//   }
// END_OPERATOR

// BOORCH's MIDIChord operator
// Note: Chord definitions moved to unified scales_and_chords system below
// Midichord operator implementation moved after unified system definition

BEGIN_OPERATOR(midipb)
  for (Usz i = 1; i < 4; ++i) {
    PORT(0, (Isz)i, IN);
  }
  PORT(0, 0, OUT); // Mark output immediately
  STOP_IF_NOT_BANGED;
  Glyph channel_g = PEEK(0, 1);
  Glyph msb_g = PEEK(0, 2);
  Glyph lsb_g = PEEK(0, 3);
  if (channel_g == '.')
    return;
  Usz channel = index_of(channel_g);
  if (channel > 15)
    return;
  Oevent_midi_pb *oe =
      (Oevent_midi_pb *)oevent_list_alloc_item(extra_params->oevent_list);
  oe->oevent_type = Oevent_type_midi_pb;
  oe->channel = (U8)channel;
  oe->msb = (U8)(index_of(msb_g) * 127 / 35); // 0~35 -> 0~127
  oe->lsb = (U8)(index_of(lsb_g) * 127 / 35);
END_OPERATOR

BEGIN_OPERATOR(add)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, OUT);
  Glyph a = PEEK(0, -1);
  Glyph b = PEEK(0, 1);
  Glyph g = glyph_table[(index_of(a) + index_of(b)) % Glyphs_index_count];
  POKE(1, 0, glyph_with_case(g, b));
END_OPERATOR

BEGIN_OPERATOR(subtract)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, OUT);
  Glyph a = PEEK(0, -1);
  Glyph b = PEEK(0, 1);
  Isz val = (Isz)index_of(b) - (Isz)index_of(a);
  if (val < 0)
    val = -val;
  POKE(1, 0, glyph_with_case(glyph_of((Usz)val), b));
END_OPERATOR

BEGIN_OPERATOR(clock)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, OUT);
  Glyph b = PEEK(0, 1);
  Usz rate = index_of(PEEK(0, -1));
  Usz mod_num = index_of(b);
  if (rate == 0)
    rate = 1;
  if (mod_num == 0)
    mod_num = 8;
  Glyph g = glyph_of(Tick_number / rate % mod_num);
  POKE(1, 0, glyph_with_case(g, b));
END_OPERATOR

BEGIN_OPERATOR(delay)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, OUT);
  Usz rate = index_of(PEEK(0, -1));
  Usz mod_num = index_of(PEEK(0, 1));
  if (rate == 0)
    rate = 1;
  if (mod_num == 0)
    mod_num = 8;
  Glyph g = Tick_number % (rate * mod_num) == 0 ? '*' : '.';
  POKE(1, 0, g);
END_OPERATOR

BEGIN_OPERATOR(if)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, OUT);
  Glyph g0 = PEEK(0, -1);
  Glyph g1 = PEEK(0, 1);
  POKE(1, 0, g0 == g1 ? '*' : '.');
END_OPERATOR

BEGIN_OPERATOR(generator)
  LOWERCASE_REQUIRES_BANG;
  Isz out_x = (Isz)index_of(PEEK(0, -3));
  Isz out_y = (Isz)index_of(PEEK(0, -2)) + 1;
  Isz len = (Isz)index_of(PEEK(0, -1));
  PORT(0, -3, IN | PARAM); // x
  PORT(0, -2, IN | PARAM); // y
  PORT(0, -1, IN | PARAM); // len
  for (Isz i = 0; i < len; ++i) {
    PORT(0, i + 1, IN);
    PORT(out_y, out_x + i, OUT | NONLOCKING);
    Glyph g = PEEK(0, i + 1);
    POKE_STUNNED(out_y, out_x + i, g);
  }
END_OPERATOR

BEGIN_OPERATOR(halt)
  LOWERCASE_REQUIRES_BANG;
  PORT(1, 0, IN | PARAM);
END_OPERATOR

BEGIN_OPERATOR(increment)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, IN | OUT);
  Glyph ga = PEEK(0, -1);
  Glyph gb = PEEK(0, 1);
  Usz rate = 1;
  if (ga != '.' && ga != '*')
    rate = index_of(ga);
  Usz max = index_of(gb);
  Usz val = index_of(PEEK(1, 0));
  if (max == 0)
    max = 36;
  val = val + rate;
  val = val % max;
  POKE(1, 0, glyph_with_case(glyph_of(val), gb));
END_OPERATOR

BEGIN_OPERATOR(jump)
  LOWERCASE_REQUIRES_BANG;
  Glyph g = PEEK(-1, 0);
  if (g == This_oper_char)
    return;
  PORT(-1, 0, IN);
  for (Isz i = 1; i <= 256; ++i) {
    if (PEEK(i, 0) != This_oper_char) {
      PORT(i, 0, OUT);
      POKE(i, 0, g);
      break;
    }
    STUN(i, 0);
  }
END_OPERATOR

// Note: this is merged from a pull request without being fully tested or
// optimized
BEGIN_OPERATOR(konkat)
  LOWERCASE_REQUIRES_BANG;
  Isz len = (Isz)index_of(PEEK(0, -1));
  if (len == 0)
    len = 1;
  PORT(0, -1, IN | PARAM);
  for (Isz i = 0; i < len; ++i) {
    PORT(0, i + 1, IN);
    Glyph var = PEEK(0, i + 1);
    if (var != '.') {
      Usz var_idx = index_of(var);
      Glyph result = extra_params->vars_slots[var_idx];
      PORT(1, i + 1, OUT);
      POKE(1, i + 1, result);
    }
  }
END_OPERATOR

BEGIN_OPERATOR(lesser)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, OUT);
  Glyph ga = PEEK(0, -1);
  Glyph gb = PEEK(0, 1);
  if (ga == '.' || gb == '.') {
    POKE(1, 0, '.');
  } else {
    Usz ia = index_of(ga);
    Usz ib = index_of(gb);
    Usz out = ia < ib ? ia : ib;
    POKE(1, 0, glyph_with_case(glyph_of(out), gb));
  }
END_OPERATOR

BEGIN_OPERATOR(multiply)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, OUT);
  Glyph a = PEEK(0, -1);
  Glyph b = PEEK(0, 1);
  Glyph g = glyph_table[(index_of(a) * index_of(b)) % Glyphs_index_count];
  POKE(1, 0, glyph_with_case(g, b));
END_OPERATOR

BEGIN_OPERATOR(offset)
  LOWERCASE_REQUIRES_BANG;
  Isz in_x = (Isz)index_of(PEEK(0, -2)) + 1;
  Isz in_y = (Isz)index_of(PEEK(0, -1));
  PORT(0, -1, IN | PARAM);
  PORT(0, -2, IN | PARAM);
  PORT(in_y, in_x, IN);
  PORT(1, 0, OUT);
  POKE(1, 0, PEEK(in_y, in_x));
END_OPERATOR

BEGIN_OPERATOR(push)
  LOWERCASE_REQUIRES_BANG;
  Usz key = index_of(PEEK(0, -2));
  Usz len = index_of(PEEK(0, -1));
  PORT(0, -1, IN | PARAM);
  PORT(0, -2, IN | PARAM);
  PORT(0, 1, IN);
  if (len == 0)
    return;
  Isz out_x = (Isz)(key % len);
  for (Usz i = 0; i < len; ++i) {
    LOCK(1, (Isz)i);
  }
  PORT(1, out_x, OUT);
  POKE(1, out_x, PEEK(0, 1));
END_OPERATOR

BEGIN_OPERATOR(query)
  LOWERCASE_REQUIRES_BANG;

  // Get parameters in new order: length, y, x
  Isz len = (Isz)index_of(PEEK(0, -3));
  Isz in_y = (Isz)index_of(PEEK(0, -2));
  Isz in_x = (Isz)index_of(PEEK(0, -1)) + 1;

  Isz out_x = 1 - len;

  // Mark parameter ports in new order
  PORT(0, -3, IN | PARAM); // len
  PORT(0, -2, IN | PARAM); // y
  PORT(0, -1, IN | PARAM); // x

  for (Isz i = 0; i < len; ++i) {
    PORT(in_y, in_x + i, IN);
    PORT(1, out_x + i, OUT);
    Glyph g = PEEK(in_y, in_x + i);
    POKE(1, out_x + i, g);
  }
END_OPERATOR

BEGIN_OPERATOR(track)
  LOWERCASE_REQUIRES_BANG;
  Usz key = index_of(PEEK(0, -2));
  Usz len = index_of(PEEK(0, -1));
  PORT(0, -2, IN | PARAM);
  PORT(0, -1, IN | PARAM);
  if (len == 0)
    return;
  Isz read_val_x = (Isz)(key % len) + 1;
  for (Usz i = 0; i < len; ++i) {
    LOCK(0, (Isz)(i + 1));
  }
  PORT(0, (Isz)read_val_x, IN);
  PORT(1, 0, OUT);
  POKE(1, 0, PEEK(0, read_val_x));
END_OPERATOR

// https://www.computermusicdesign.com/
// simplest-euclidean-rhythm-algorithm-explained/
BEGIN_OPERATOR(uclid)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, OUT);
  Glyph left = PEEK(0, -1);
  Usz steps = 1;
  if (left != '.' && left != '*')
    steps = index_of(left);
  Usz max = index_of(PEEK(0, 1));
  if (max == 0)
    max = 8;
  Usz bucket = (steps * (Tick_number + max - 1)) % max + steps;
  Glyph g = (bucket >= max) ? '*' : '.';
  POKE(1, 0, g);
END_OPERATOR

BEGIN_OPERATOR(variable)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  Glyph left = PEEK(0, -1);
  Glyph right = PEEK(0, 1);
  if (left != '.') {
    // Write
    Usz var_idx = index_of(left);
    extra_params->vars_slots[var_idx] = right;
  } else if (right != '.') {
    // Read
    PORT(1, 0, OUT);
    Usz var_idx = index_of(right);
    Glyph result = extra_params->vars_slots[var_idx];
    POKE(1, 0, result);
  }
END_OPERATOR

BEGIN_OPERATOR(teleport)
  LOWERCASE_REQUIRES_BANG;

  // Get count from leftmost input
  Usz count = 1;
  Glyph count_g = PEEK(0, -3);
  if (count_g != '.' && count_g != '0') {
    count = index_of(count_g);
  }

  // Return if count is 0
  if (count == 0)
    return;

  // Mark parameter inputs
  PORT(0, -3, IN | PARAM); // Count
  PORT(0, -2, IN | PARAM); // Y offset
  PORT(0, -1, IN | PARAM); // X offset

  // Get offsets
  Glyph out_y_g = PEEK(0, -2);
  Glyph out_x_g = PEEK(0, -1);

  // If either offset is '.', treat as 0
  Usz out_y = out_y_g == '.' ? 0 : index_of(out_y_g);
  Usz out_x = out_x_g == '.' ? 0 : index_of(out_x_g);

  // Validate offsets based on the rules:

  // Horizontal offset of 0 is only valid with vertical offset >= 1
  if (out_x == 0 && out_y < 1)
    return;

  // Horizontal offset of 1 is only valid with count of 1
  if (out_x == 1 && count > 1)
    return;

  // For same row (y=0), horizontal offset must be > count
  if (out_y == 0 && out_x <= count)
    return;

  // Mark input ports for each input cell
  for (Usz i = 0; i < count; ++i) {
    PORT(0, (Isz)i + 1, IN);
  }

  // Lock and read inputs
  Glyph inputs[256];
  for (Usz i = 0; i < count; ++i) {
    inputs[i] = PEEK(0, (Isz)i + 1);
  }

  // Write outputs
  for (Usz i = 0; i < count; ++i) {
    PORT((Isz)out_y, (Isz)(out_x + i), OUT | NONLOCKING);
    POKE_STUNNED((Isz)out_y, (Isz)(out_x + i), inputs[i]);
  }
END_OPERATOR

BEGIN_OPERATOR(yump)
  LOWERCASE_REQUIRES_BANG;
  Glyph g = PEEK(0, -1);
  if (g == This_oper_char)
    return;
  PORT(0, -1, IN);
  for (Isz i = 1; i <= 256; ++i) {
    if (PEEK(0, i) != This_oper_char) {
      PORT(0, i, OUT);
      POKE(0, i, g);
      break;
    }
    STUN(0, i);
  }
END_OPERATOR

BEGIN_OPERATOR(lerp)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM);
  PORT(0, 1, IN);
  PORT(1, 0, IN | OUT);
  Glyph g = PEEK(0, -1);
  Glyph b = PEEK(0, 1);
  Isz rate = g == '.' || g == '*' ? 1 : (Isz)index_of(g);
  Isz goal = (Isz)index_of(b);
  Isz val = (Isz)index_of(PEEK(1, 0));
  Isz mod = val <= goal - rate ? rate : val >= goal + rate ? -rate : goal - val;
  POKE(1, 0, glyph_with_case(glyph_of((Usz)(val + mod)), b));
END_OPERATOR

// BOORCH's new Scale OP - Unified Scale/Chord System

// SCALES (0-9) - Essential scales only
static Usz scale_major[] = {0, 2, 4, 5, 7, 9, 11};         // 0: Major
static Usz scale_minor[] = {0, 2, 3, 5, 7, 8, 10};         // 1: Minor
static Usz scale_dorian[] = {0, 2, 3, 5, 7, 9, 10};        // 2: Dorian
static Usz scale_lydian[] = {0, 2, 4, 6, 7, 9, 11};        // 3: Lydian
static Usz scale_mixolydian[] = {0, 2, 4, 5, 7, 9, 10};    // 4: Mixolydian
static Usz scale_pentatonic[] = {0, 2, 4, 7, 9};           // 5: Pentatonic
static Usz scale_hirajoshi[] = {0, 2, 3, 7, 8};            // 6: Hirajoshi
static Usz scale_iwato[] = {0, 1, 5, 6, 10};               // 7: Iwato
static Usz scale_tetratonic[] = {0, 4, 7, 11};             // 8: Tetratonic
static Usz scale_fifths[] = {0, 7};                        // 9: Fifths

// ENRICHED CHORDS FOR MIDICHORD 0-9 (Approach 2: Enriched versions of a-j)
static Usz chord_major_rich[] = {0, 4, 7, 12};             // 0: Major + octave root (C-E-G-C)
static Usz chord_minor_rich[] = {0, 3, 7, 12};             // 1: Minor + octave root (C-Eb-G-C)
static Usz chord_sus4_rich[] = {0, 5, 7, 12};              // 2: Sus4 + octave root (C-F-G-C)
static Usz chord_sus2_rich[] = {0, 2, 7, 12};              // 3: Sus2 + octave root (C-D-G-C)
static Usz chord_major7_rich[] = {0, 4, 7, 11, 16};        // 4: Major7 + octave 3rd (C-E-G-B-E)
static Usz chord_minor7_rich[] = {0, 3, 7, 10, 15};        // 5: Minor7 + octave 3rd (C-Eb-G-Bb-Eb)
static Usz chord_dom7_rich[] = {0, 4, 7, 10, 19};          // 6: Dom7 + octave 5th (C-E-G-Bb-G)
static Usz chord_major6_rich[] = {0, 4, 7, 9, 12};         // 7: Major6 + octave root (C-E-G-A-C)
static Usz chord_minor6_rich[] = {0, 3, 7, 9, 12};         // 8: Minor6 + octave root (C-Eb-G-A-C)
static Usz chord_dim_rich[] = {0, 3, 6, 12};               // 9: Dim + octave root (C-Eb-Gb-C)

// Separate scale arrays for Scale operator (0-9)
static Usz *scales[] = {
    scale_major, scale_minor, scale_dorian, scale_lydian, scale_mixolydian,
    scale_pentatonic, scale_hirajoshi, scale_iwato, scale_tetratonic, scale_fifths
};

static Usz scale_lengths[] = {7, 7, 7, 7, 7, 5, 5, 5, 4, 2};

// CHORDS ROOT POSITION (a-z) - 26 most common chords
static Usz chord_major[] = {0, 4, 7};                      // a: Major
static Usz chord_minor[] = {0, 3, 7};                      // b: Minor
static Usz chord_sus4[] = {0, 5, 7};                       // c: Sus4
static Usz chord_sus2[] = {0, 2, 7};                       // d: Sus2
static Usz chord_major7[] = {0, 4, 7, 11};                 // e: Major 7
static Usz chord_minor7[] = {0, 3, 7, 10};                 // f: Minor 7
static Usz chord_dom7[] = {0, 4, 7, 10};                   // g: Dominant 7
static Usz chord_min_maj7[] = {0, 3, 7, 11};               // h: Minor Major 7
static Usz chord_minor6[] = {0, 3, 7, 9};                  // i: Minor 6
static Usz chord_major6[] = {0, 4, 7, 9};                  // j: Major 6
static Usz chord_major9[] = {0, 4, 7, 11, 14};             // k: Major 9
static Usz chord_minor9[] = {0, 3, 7, 10, 14};             // l: Minor 9
static Usz chord_major_add9[] = {0, 4, 7, 14};             // m: Major add9
static Usz chord_minor_add9[] = {0, 3, 7, 14};             // n: Minor add9
static Usz chord_dim[] = {0, 3, 6};                        // o: Diminished
static Usz chord_half_dim[] = {0, 3, 6, 10};               // p: Half Diminished
static Usz chord_dim7[] = {0, 3, 6, 9};                    // q: Diminished 7
static Usz chord_aug[] = {0, 4, 8};                        // r: Augmented
static Usz chord_aug7[] = {0, 4, 8, 10};                   // s: Augmented 7
static Usz chord_dom9[] = {0, 4, 7, 10, 14};               // t: Dominant 9
static Usz chord_dom7b9[] = {0, 4, 7, 10, 13};             // u: Dominant 7b9
static Usz chord_dom7sharp9[] = {0, 4, 7, 10, 15};         // v: Dominant 7#9
static Usz chord_maj_6_9[] = {0, 4, 7, 9, 14};             // w: Major 6/9
static Usz chord_min_6_9[] = {0, 3, 7, 9, 14};             // x: Minor 6/9
static Usz chord_min11[] = {0, 3, 7, 10, 17};              // y: Minor 11
static Usz chord_min7b5[] = {0, 3, 6, 10};                 // z: Minor 7b5 (alt. half-dim)

// CHORDS FIRST INVERSION (A-Z) - Same chords but inverted
static Usz chord_major_inv[] = {0, 3, 8};                  // A: Major 1st inv
static Usz chord_minor_inv[] = {0, 4, 9};                  // B: Minor 1st inv  
static Usz chord_sus4_inv[] = {0, 2, 7};                   // C: Sus4 1st inv
static Usz chord_sus2_inv[] = {0, 5, 10};                  // D: Sus2 1st inv
static Usz chord_major7_inv[] = {0, 3, 7, 8};              // E: Major 7 1st inv
static Usz chord_minor7_inv[] = {0, 4, 7, 9};              // F: Minor 7 1st inv
static Usz chord_dom7_inv[] = {0, 3, 6, 8};                // G: Dominant 7 1st inv
static Usz chord_min_maj7_inv[] = {0, 4, 8, 9};            // H: Minor Major 7 1st inv
static Usz chord_minor6_inv[] = {0, 4, 6, 9};              // I: Minor 6 1st inv
static Usz chord_major6_inv[] = {0, 3, 5, 8};              // J: Major 6 1st inv
static Usz chord_major9_inv[] = {0, 3, 7, 10, 11};         // K: Major 9 1st inv
static Usz chord_minor9_inv[] = {0, 4, 7, 11, 12};         // L: Minor 9 1st inv
static Usz chord_major_add9_inv[] = {0, 3, 10, 11};        // M: Major add9 1st inv
static Usz chord_minor_add9_inv[] = {0, 4, 11, 12};        // N: Minor add9 1st inv
static Usz chord_dim_inv[] = {0, 3, 9};                    // O: Diminished 1st inv
static Usz chord_half_dim_inv[] = {0, 3, 7, 9};            // P: Half Diminished 1st inv
static Usz chord_dim7_inv[] = {0, 3, 6, 9};                // Q: Diminished 7 1st inv
static Usz chord_aug_inv[] = {0, 4, 8};                    // R: Augmented 1st inv (same as root)
static Usz chord_aug7_inv[] = {0, 4, 6, 8};                // S: Augmented 7 1st inv
static Usz chord_dom9_inv[] = {0, 3, 6, 10, 11};           // T: Dominant 9 1st inv
static Usz chord_dom7b9_inv[] = {0, 3, 6, 9, 11};          // U: Dominant 7b9 1st inv
static Usz chord_dom7sharp9_inv[] = {0, 3, 6, 11, 12};     // V: Dominant 7#9 1st inv
static Usz chord_maj_6_9_inv[] = {0, 3, 5, 10, 11};        // W: Major 6/9 1st inv
static Usz chord_min_6_9_inv[] = {0, 4, 6, 11, 12};        // X: Minor 6/9 1st inv
static Usz chord_min11_inv[] = {0, 4, 7, 14, 15};          // Y: Minor 11 1st inv
static Usz chord_min7b5_inv[] = {0, 3, 7, 9};              // Z: Minor 7b5 1st inv

// Unified array of all scales and chords (0-9, a-z, A-Z)
static Usz *scales_and_chords[] = {
    // Enriched chords for Midichord (0-9)
    chord_major_rich, chord_minor_rich, chord_sus4_rich, chord_sus2_rich, chord_major7_rich,
    chord_minor7_rich, chord_dom7_rich, chord_major6_rich, chord_minor6_rich, chord_dim_rich,
    // Chords root position (a-z)
    chord_major, chord_minor, chord_sus4, chord_sus2, chord_major7, chord_minor7,
    chord_dom7, chord_min_maj7, chord_minor6, chord_major6, chord_major9, chord_minor9,
    chord_major_add9, chord_minor_add9, chord_dim, chord_half_dim, chord_dim7, chord_aug,
    chord_aug7, chord_dom9, chord_dom7b9, chord_dom7sharp9, chord_maj_6_9, chord_min_6_9,
    chord_min11, chord_min7b5,
    // Chords first inversion (A-Z)
    chord_major_inv, chord_minor_inv, chord_sus4_inv, chord_sus2_inv, chord_major7_inv, 
    chord_minor7_inv, chord_dom7_inv, chord_min_maj7_inv, chord_minor6_inv, chord_major6_inv,
    chord_major9_inv, chord_minor9_inv, chord_major_add9_inv, chord_minor_add9_inv, chord_dim_inv,
    chord_half_dim_inv, chord_dim7_inv, chord_aug_inv, chord_aug7_inv, chord_dom9_inv,
    chord_dom7b9_inv, chord_dom7sharp9_inv, chord_maj_6_9_inv, chord_min_6_9_inv, chord_min11_inv,
    chord_min7b5_inv
};

// Lengths for scales and chords
static Usz scale_chord_lengths[] = {
    // Enriched chords for Midichord (0-9)
    4, 4, 4, 4, 5, 5, 5, 5, 5, 4,
    // Chords root position (a-z) 
    3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 4, 4, 3, 4, 4, 3, 4, 5, 5, 5, 5, 5, 5, 4,
    // Chords first inversion (A-Z)
    3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 4, 4, 3, 4, 4, 3, 4, 5, 5, 5, 5, 5, 5, 4
};

BEGIN_OPERATOR(scale)
  PORT(0, 1, IN);   // Octave input
  PORT(0, 2, IN);   // Root note (like C, c, D etc)
  PORT(0, 3, IN);   // Scale/Chord (0-9 scales, a-z chords, A-Z first inversions)
  PORT(0, 4, IN);   // Degree
  PORT(1, -1, OUT); // Octave output
  PORT(1, 0, OUT);  // Note output

  // Lock inputs
  LOCK(0, 1);
  LOCK(0, 2);
  LOCK(0, 3);
  LOCK(0, 4);

  Glyph octave_g = PEEK(0, 1);
  Glyph root_note_glyph = PEEK(0, 2);
  Glyph scale_glyph = PEEK(0, 3);
  Glyph degree_glyph = PEEK(0, 4);

  // Handle empty inputs
  if (root_note_glyph == '.' || scale_glyph == '.' || degree_glyph == '.') {
    POKE(1, 0, '.');
    return;
  }

  // Get root note number (0-11)
  U8 root_note_num = midi_note_number_of(root_note_glyph);
  if (root_note_num == UINT8_MAX)
    return;

  // Get base octave if provided
  Usz base_octave = 0;
  if (octave_g != '.') {
    base_octave = index_of(octave_g);
    if (base_octave > 9)
      base_octave = 9;
  }

  // Get scale/chord index - supports 0-9, a-z, A-Z (total 62 options)
  Usz scale_index = index_of(scale_glyph);
  Usz degree_index = index_of(degree_glyph);

  Usz scale_length, scale_offset;
  
  if (scale_index <= 9) {
    // Use scales for 0-9
    if (scale_index >= sizeof(scales) / sizeof(scales[0]))
      return;
    scale_length = scale_lengths[scale_index];
    Usz octave_increment = degree_index / scale_length;
    degree_index = degree_index % scale_length;
    scale_offset = scales[scale_index][degree_index];
    
    // Calculate total semitones including octave increment
    Usz total_semitones = root_note_num + scale_offset + (octave_increment * 12);

    // Calculate final note and octave
    Usz final_note = total_semitones % 12;
    Usz octave_offset = total_semitones / 12;
    Usz final_octave = base_octave + octave_offset;

    if (final_octave > 9)
      return;

    // Output note
    Glyph output_note_glyph = note_sequence[final_note];
    POKE(1, 0, output_note_glyph);

    // Output octave if input octave was provided
    if (octave_g != '.') {
      POKE(1, -1, glyph_of(final_octave));
    }
  } else {
    // Use unified array for a-z, A-Z (indices 10-61)
    Usz num_scales_chords = sizeof(scales_and_chords) / sizeof(scales_and_chords[0]);
    if (scale_index >= num_scales_chords)
      return;

    // Get scale/chord length and calculate octave increment
    scale_length = scale_chord_lengths[scale_index];
    Usz octave_increment = degree_index / scale_length;

    // Calculate scale degree within current octave
    degree_index = degree_index % scale_length;
    scale_offset = scales_and_chords[scale_index][degree_index];

    // Calculate total semitones
    Usz total_semitones = root_note_num + scale_offset + (octave_increment * 12);

    // Calculate final note and octave
    Usz final_note = total_semitones % 12;
    Usz octave_offset = total_semitones / 12;
    Usz final_octave = base_octave + octave_offset;

    if (final_octave > 9)
      return;

    // Output note
    Glyph output_note_glyph = note_sequence[final_note];
    POKE(1, 0, output_note_glyph);

    // Output octave if input octave was provided
    if (octave_g != '.') {
      POKE(1, -1, glyph_of(final_octave));
    }
  }
END_OPERATOR

//BOORCH's MIDIChord operator (using unified scales_and_chords system)
BEGIN_OPERATOR(midichord)
  // Check all required input ports
  for (Usz i = 1; i < 7; ++i) {
    PORT(0, (Isz)i, IN);
  }
  PORT(0, 0, OUT); // Mark output immediately
  STOP_IF_NOT_BANGED;

  // Get chord type and validate range (supports a-z and A-Z)
  Glyph chord_glyph = PEEK(0, 4);
  Usz chord_idx = index_of(chord_glyph);
  
  // Map chord input to unified scales_and_chords array
  // 0-9: enriched chords for Midichord
  // a-z (lowercase): indices 10-35 (root position chords)
  // A-Z (uppercase): indices 36-61 (first inversion chords)
  if (chord_idx <= 9) {
    // 0-9: enriched chords (use as-is, already correct index)
  } else if (chord_idx >= 10 && chord_idx <= 35) {
    // Lowercase a-z: use as-is (already correct index for scales_and_chords)
  } else if (chord_idx >= 36 && chord_idx <= 61) {
    // Uppercase A-Z: use as-is (already correct index for scales_and_chords)
  } else {
    // Invalid chord type
    return;
  }

  // Validate against total array size
  Usz num_scales_chords = sizeof(scales_and_chords) / sizeof(scales_and_chords[0]);
  if (chord_idx >= num_scales_chords)
    return;

  // Get base note information with local scope
  Usz channel = index_of(PEEK(0, 1));
  if (channel > 15)
    channel = 15;

  // Get velocity and duration with standardized handling
  Glyph velocity_g = PEEK(0, 5);
  Glyph length_g = PEEK(0, 6);

  // Standardized velocity
  U8 velocity =
      (velocity_g == '.' ? 127 : (U8)(index_of(velocity_g) * 127 / 35));
  if (velocity > 127)
    velocity = 127;

  // Get initial octave
  int current_octave = (int)index_of(PEEK(0, 2));
  if (current_octave > 9)
    current_octave = 9;

  // Get root note and validate
  U8 root_note = midi_note_number_of(PEEK(0, 3));
  if (root_note == UINT8_MAX)
    return;

  // Get pointer to the selected chord array and its length
  Usz *chord = scales_and_chords[chord_idx];
  Usz chord_len = scale_chord_lengths[chord_idx];

  // Track highest note played so far
  int last_note_absolute = (current_octave * 12) + root_note - 1;

  // Create and output midi events for each note in the chord
  for (Usz i = 0; i < chord_len; i++) {
    // Calculate this note's absolute MIDI note number
    int note_absolute = (current_octave * 12) + root_note + (int)chord[i];

    // If this note would be lower than previous note, move it up an octave
    while (note_absolute <= last_note_absolute) {
      current_octave++;
      note_absolute += 12;
    }

    // Skip if we exceeded MIDI range
    if (current_octave > 9 || note_absolute > 127)
      continue;

    // Keep track of highest note
    last_note_absolute = note_absolute;

    // Calculate final octave and note numbers
    U8 final_octave = (U8)(note_absolute / 12);
    U8 final_note = (U8)(note_absolute % 12);

    // Create MIDI event
    Oevent_midi_note *oe =
        (Oevent_midi_note *)oevent_list_alloc_item(extra_params->oevent_list);
    oe->oevent_type = Oevent_type_midi_note;
    oe->channel = (U8)channel;
    oe->octave = final_octave;
    oe->note = final_note;
    oe->velocity = velocity;
    oe->duration = (U8)(index_of(length_g) & 0x7Fu);
    oe->mono = 0;
  }

END_OPERATOR

//BOORCH's new Midipoly OP
BEGIN_OPERATOR(midipoly)
  for (Usz i = 1; i < 8; ++i) {
    PORT(0, (Isz)i, IN);
  }
  PORT(0, 0, OUT); // Mark output immediately
  STOP_IF_NOT_BANGED;
  Glyph channel_g = PEEK(0, 1);
  Glyph octave_g = PEEK(0, 2);
  Glyph note_gs[3] = {PEEK(0, 3), PEEK(0, 4), PEEK(0, 5)};
  Glyph velocity_g = PEEK(0, 6);
  Glyph length_g = PEEK(0, 7);

  Usz channel = index_of(channel_g);
  int base_octave =
      (int)index_of(octave_g); // Explicitly cast for safe addition

  if (channel > 15)
    return;

  int last_note_absolute =
      -1; // Store the absolute midi note number of the last note

  for (int i = 0; i < 3; i++) {
    U8 note_num = midi_note_number_of(note_gs[i]);
    if (note_num == UINT8_MAX)
      continue; // Skip invalid notes

    int note_absolute =
        base_octave * 12 + note_num; // Calculate the absolute midi note number
    if (note_absolute <= last_note_absolute) {
      // Ensure each note is higher than the previous
      base_octave = (last_note_absolute / 12) + 1;
    }
    last_note_absolute =
        base_octave * 12 + note_num; // Update for the next iteration

    if (base_octave > 9)
      continue; // Skip notes with octaves out of MIDI bounds

    U8 velocity =
        (velocity_g == '.' ? 127 : (U8)(index_of(velocity_g) * 127 / 35));

    // if (velocity == 0)
    //   return;

    if (velocity > 127)
      velocity = 127;

    Oevent_midi_note *oe =
        (Oevent_midi_note *)oevent_list_alloc_item(extra_params->oevent_list);
    oe->oevent_type = Oevent_type_midi_note;
    oe->channel = (U8)channel;
    oe->octave = (U8)base_octave;
    oe->note = note_num;
    oe->velocity = velocity;
    oe->duration = (U8)(index_of(length_g) & 0x7Fu);
    oe->mono = 0;
  }

END_OPERATOR

// BOORCH's Simple Arpeggiator - outputs degree numbers for Scale operator
typedef enum {
  ARP_UP = 0,        // 0: Ascending
  ARP_DOWN,          // 1: Descending  
  ARP_UP_DOWN,       // 2: Up and down (without repeating highest)
  ARP_DOWN_UP,       // 3: Down and up (without repeating lowest)
  ARP_UP_DOWN_PLUS,  // 4: Up and down with repeated end points
  ARP_DOWN_UP_PLUS,  // 5: Down and up with repeated end points
  ARP_CONVERGE,      // 6: Outside in (highest, lowest, 2nd highest, 2nd lowest...)
  ARP_DIVERGE,       // 7: Inside out (middle outward)
  ARP_PINKY_UP,      // 8: Alternate between notes and highest note
  ARP_THUMB_UP,      // 9: Alternate between lowest and other notes
  ARP_UP_DOWN_ALT,   // a: Up-down alternating pattern
  ARP_DOWN_UP_ALT,   // b: Down-up alternating pattern  
  ARP_RANDOM,        // c: Random selection
  ARP_BOUNCE,        // d: Bouncing pattern
  ARP_PATTERN_COUNT
} ArpPatternType;

// Arpeggiator state for tracking position
typedef struct {
  Usz step_counter;
  Usz last_pattern;
  Usz last_range;
} Arp_state;

#define MAX_ARP_GRID_SIZE 4096
static Arp_state arp_states[MAX_ARP_GRID_SIZE] = {0};

// Function to get the degree based on pattern type and step
static Usz get_arp_degree(ArpPatternType pattern, Usz step, Usz range, 
                         Oper_extra_params *extra_params) {
  // Assume a 7-note scale and multiply by range for total degrees
  Usz scale_length = 7;
  Usz total_degrees = scale_length * range;
  
  if (total_degrees == 0)
    return 0;

  switch (pattern) {
  case ARP_UP:
    return step % total_degrees;

  case ARP_DOWN:
    return (total_degrees - 1) - (step % total_degrees);

  case ARP_UP_DOWN: {
    Usz period = (total_degrees * 2) - 2;
    if (period < 2) period = 2;
    Usz pos = step % period;
    return pos < total_degrees ? pos : period - pos;
  }

  case ARP_DOWN_UP: {
    Usz period = (total_degrees * 2) - 2;
    if (period < 2) period = 2;
    Usz pos = step % period;
    return pos < total_degrees ? (total_degrees - 1) - pos : pos - total_degrees + 1;
  }

  case ARP_UP_DOWN_PLUS: {
    Usz period = total_degrees * 2;
    Usz pos = step % period;
    return pos < total_degrees ? pos : period - pos - 1;
  }

  case ARP_DOWN_UP_PLUS: {
    Usz period = total_degrees * 2;
    Usz pos = step % period;
    return pos < total_degrees ? total_degrees - pos - 1 : pos - total_degrees;
  }

  case ARP_CONVERGE: {
    Usz pos = step % total_degrees;
    Usz left = 0, right = total_degrees - 1;
    for (Usz i = 0; i < pos; i++) {
      if (i % 2 == 0) right--; else left++;
    }
    return (pos % 2 == 0) ? right : left;
  }

  case ARP_DIVERGE: {
    Usz pos = step % total_degrees;
    Usz mid = total_degrees / 2;
    Usz offset = pos / 2;
    if (pos % 2 == 0) {
      return (mid + offset < total_degrees) ? mid + offset : total_degrees - 1;
    } else {
      return (mid > offset) ? mid - offset - 1 : 0;
    }
  }

  case ARP_PINKY_UP: {
    if (total_degrees <= 1) return 0;
    Usz highest = total_degrees - 1;
    Usz pos = step % (total_degrees * 2 - 2);
    return (pos % 2 == 0) ? pos / 2 : highest;
  }

  case ARP_THUMB_UP: {
    if (total_degrees <= 1) return 0;
    Usz pos = step % ((total_degrees - 1) * 2);
    return (pos % 2 == 0) ? 0 : (pos / 2) + 1;
  }

  case ARP_UP_DOWN_ALT: {
    Usz half_period = total_degrees;
    Usz pos = step % (half_period * 2);
    if (pos < half_period) {
      return pos;
    } else {
      return total_degrees - 1 - (pos - half_period);
    }
  }

  case ARP_DOWN_UP_ALT: {
    Usz half_period = total_degrees;
    Usz pos = step % (half_period * 2);
    if (pos < half_period) {
      return total_degrees - 1 - pos;
    } else {
      return pos - half_period;
    }
  }

  case ARP_RANDOM: {
    Usz key = (extra_params->random_seed + step) ^ (step << 16);
    key = (key ^ 61) ^ (key >> 16);
    key = key + (key << 3);
    key = key ^ (key >> 4);
    key = key * 0x27d4eb2d;
    key = key ^ (key >> 15);
    return key % total_degrees;
  }

  case ARP_BOUNCE: {
    Usz bounce_size = total_degrees / 3;
    if (bounce_size < 1) bounce_size = 1;
    Usz pos = step % (bounce_size * 4);
    if (pos < bounce_size) return pos;
    else if (pos < bounce_size * 2) return bounce_size - 1 - (pos - bounce_size);
    else if (pos < bounce_size * 3) return pos - bounce_size * 2;
    else return bounce_size - 1 - (pos - bounce_size * 3);
  }

  default:
    return step % total_degrees;
  }
}

BEGIN_OPERATOR(arpeggiator)
  LOWERCASE_REQUIRES_BANG;
  PORT(0, -1, IN | PARAM); // Pattern (0-9, a-d)
  PORT(0, 1, IN | PARAM);  // Range (1-4)
  PORT(1, 0, OUT);         // Degree output

  // Calculate state index
  Usz state_idx = y * width + x;
  if (state_idx >= MAX_ARP_GRID_SIZE)
    return;

  Arp_state *state = &arp_states[state_idx];

  // Get inputs
  Glyph pattern_g = PEEK(0, -1);
  Glyph range_g = PEEK(0, 1);

  if (pattern_g == '.' || range_g == '.')
    return;

  Usz pattern = index_of(pattern_g) % ARP_PATTERN_COUNT;
  Usz range = index_of(range_g);
  if (range == 0) range = 1;
  if (range > 4) range = 4;

  // Reset counter if pattern or range changed
  if (state->last_pattern != pattern || state->last_range != range) {
    state->step_counter = 0;
    state->last_pattern = pattern;
    state->last_range = range;
  }

  // Get degree for current step
  Usz degree = get_arp_degree((ArpPatternType)pattern, state->step_counter, range, extra_params);

  // Output degree
  POKE(1, 0, glyph_of(degree));

  // Increment step counter for next bang
  state->step_counter++;
END_OPERATOR

// BOORCH's new Random Unique
#define MAX_SEQUENCE_SIZE 36 // For values 0-9 and A-Z

typedef struct {
  Usz sequence[MAX_SEQUENCE_SIZE];
  Usz current_index;
  Usz sequence_size;
  bool initialized;
  Usz last_min; // Add these to detect range changes
  Usz last_max; // and force reinitialization
} Unique_random_state;

static Unique_random_state unique_random_state = {0};

static void shuffle_sequence(Usz *array, Usz n) {
  if (n <= 1)
    return;

  for (Usz i = n - 1; i > 0; i--) {
    // Use existing random generator from ORCA
    Usz j = (Usz)(((U32)rand()) % (i + 1));
    // Swap
    Usz temp = array[i];
    array[i] = array[j];
    array[j] = temp;
  }
}

static void initialize_sequence(Usz min, Usz max) {
  unique_random_state.sequence_size = (max >= min) ? (max - min + 1) : 0;
  if (unique_random_state.sequence_size > MAX_SEQUENCE_SIZE) {
    unique_random_state.sequence_size = MAX_SEQUENCE_SIZE;
  }

  // Fill sequence with values from min to max
  for (Usz i = 0; i < unique_random_state.sequence_size; i++) {
    unique_random_state.sequence[i] = min + i;
  }

  shuffle_sequence(unique_random_state.sequence,
                   unique_random_state.sequence_size);
  unique_random_state.current_index = 0;
}

void reset_last_unique_value(void) { unique_random_state.initialized = false; }

// Modified random operator for lowercase 'r' - requires bang, uses shuffle to avoid consecutive duplicates
BEGIN_OPERATOR(random)
  // Check if this is lowercase 'r' (shuffle/unique random) or uppercase 'R' (pure random)
  if (glyph_is_lowercase(This_oper_char)) {
    // Lowercase 'r' - requires bang and uses shuffle algorithm
    LOWERCASE_REQUIRES_BANG;
    PORT(0, -1, IN | PARAM); // Min
    PORT(0, 1, IN);          // Max
    PORT(1, 0, OUT);         // Output

    Glyph min_glyph = PEEK(0, -1);
    Glyph max_glyph = PEEK(0, 1);

    if (min_glyph == '.' || max_glyph == '.') {
      return;
    }

    Usz min = index_of(min_glyph);
    Usz max = index_of(max_glyph);

    if (max < min) {
      Usz temp = min;
      min = max;
      max = temp;
    }

    // Initialize or reinitialize if needed
    if (!unique_random_state.initialized ||
        unique_random_state.current_index >= unique_random_state.sequence_size ||
        min != unique_random_state.last_min ||
        max != unique_random_state.last_max) {
      initialize_sequence(min, max);
      unique_random_state.initialized = true;
      unique_random_state.last_min = min;
      unique_random_state.last_max = max;
    }

    // Get next value from sequence
    Usz result = unique_random_state.sequence[unique_random_state.current_index];
    unique_random_state.current_index++;

    // Reshuffle if we've used all values
    if (unique_random_state.current_index >= unique_random_state.sequence_size) {
      shuffle_sequence(unique_random_state.sequence,
                       unique_random_state.sequence_size);
      unique_random_state.current_index = 0;
    }

    POKE(1, 0, glyph_of(result));
  } else {
    // Uppercase 'R' - pure random, evaluated every tick
    PORT(0, -1, IN | PARAM);
    PORT(0, 1, IN);
    PORT(1, 0, OUT);
    Glyph gb = PEEK(0, 1);
    Usz a = index_of(PEEK(0, -1));
    Usz b = index_of(gb);
    if (b == 0)
      b = 36;
    Usz min, max;
    if (a == b) {
      POKE(1, 0, glyph_of(a));
      return;
    } else if (a < b) {
      min = a;
      max = b;
    } else {
      min = b;
      max = a;
    }
    // Initial input params for the hash
    Usz key = (extra_params->random_seed + y * width + x) ^
              (Tick_number << UINT32_C(16));
    // 32-bit shift_mult hash to evenly distribute bits
    key = (key ^ UINT32_C(61)) ^ (key >> UINT32_C(16));
    key = key + (key << UINT32_C(3));
    key = key ^ (key >> UINT32_C(4));
    key = key * UINT32_C(0x27d4eb2d);
    key = key ^ (key >> UINT32_C(15));
    // Hash finished. Restrict to desired range of numbers.
    Usz val = key % (max - min) + min;
    POKE(1, 0, glyph_with_case(glyph_of(val), gb));
  }
END_OPERATOR

// BOORCH's BOUNCER OP
// Predefined waveform sequences
static const char *waveforms[] = {
    // Triangle (0)
    "00112233445566778899aabbccddeeffgghhiijjkkllmmnnooppqqrrstuvwxyzzyxwvutsrr"
    "qqppoonnmmllkkjjiihhggffeeddccbbaa99887766554433221100",
    // Inverted Triangle (1)
    "zzyyxxwwvvuuttssrrqqppoonnmmllkkjjiihhggffeeddccbbaa9988765432100123456788"
    "99aabbccddeeffgghhiijjkkllmmnnooppqqrrssttuuvvwwxxyyzz",
    // Sine (2)
    "000000011111133333555558888cccfffiiilllooorrruuuwwwxxxyyyyyyzzzzzzzzyyyyyy"
    "xxxwwwuuurrrooollliiifffccc888855555333331111110000000",
    // Inverted Sine (3)
    "zzzzzzzyyyyyywwwwwtttttrrrrnnnkkkhhheeebbb88855533322211111100000000111111"
    "222333555888bbbeeehhhkkknnnrrrrtttttwwwwwyyyyyyzzzzzzz",
    // Square (4)
    "0000000000000000000000000000000000000000000000000000000000000000zzzzzzzzzz"
    "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz",
    // Inverted Square (5)
    "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz0000000000"
    "000000000000000000000000000000000000000000000000000000",
    // Saw (6)
    "0000111122223333444455556666777788889999aaaabbbbccccddddeeeeffffgggghhhhii"
    "iijjjjkkklllmmmnnnooopppqqqrrrssstttuuuvvvwwwxxxyyyzzz",
    // Inverted Saw (7)
    "zzzzyyyyxxxxwwwwvvvvuuuuttttssssrrrrqqqqppppoooonnnnmmmmllllkkkkjjjjiiiihh"
    "hhggggfffeeedddcccbbbaaa999888777666555444333222111000"};

#define WAVE_LENGTH 128

typedef struct {
  Usz current_index; // Current position in waveform
  bool initialized;
  Usz last_rate;  // Track rate changes
  Usz last_shape; // Track shape changes
} Bouncer_state;

static Bouncer_state bouncer_states[4096] = {0};

BEGIN_OPERATOR(bouncer)
  PORT(0, -2, IN | PARAM); // Start value (a)
  PORT(0, -1, IN | PARAM); // End value (b)
  PORT(0, 1, IN);          // Rate (ticks per cycle)
  PORT(0, 2, IN);          // Shape (0-7 for different waveforms)
  PORT(1, 0, OUT);

  Glyph start_g = PEEK(0, -2);
  Glyph end_g = PEEK(0, -1);
  Glyph rate_g = PEEK(0, 1);
  Glyph shape_g = PEEK(0, 2);

  if (start_g == '.' || end_g == '.')
    return;

  Usz state_idx = y * width + x;
  Bouncer_state *state = &bouncer_states[state_idx];

  Usz start = index_of(start_g);
  Usz end = index_of(end_g);
  Usz rate = index_of(rate_g);
  Usz shape = index_of(shape_g);

  // Initialize or reset on bang
  if (!state->initialized ||
      oper_has_neighboring_bang(gbuffer, height, width, y, x) ||
      rate != state->last_rate || shape != state->last_shape) {
    state->current_index = 0;
    state->initialized = true;
    state->last_rate = rate;
    state->last_shape = shape;
  }

  // Only advance if rate > 0
  if (rate > 0 && rate_g != '.') {
    state->current_index = (state->current_index + rate) % WAVE_LENGTH;
  }

  // Get raw waveform value first
  shape = shape < 8 ? shape : 0;
  char wave_value = waveforms[shape][state->current_index];

  // Get normalized position (0-1) in waveform range
  float normalized_pos = (float)index_of(wave_value) / 35.0f;

  // Map normalized position to output range
  Usz range = end > start ? end - start : start - end;
  Usz output_value = start;
  if (range > 0) {
    if (end > start) {
      output_value = start + (Usz)(normalized_pos * (float)range);
    } else {
      output_value = start - (Usz)(normalized_pos * (float)range);
    }
  }

  POKE(1, 0, glyph_of(output_value));
END_OPERATOR

//////// Run simulation

void orca_run(Glyph *restrict gbuf, Mark *restrict mbuf, Usz height, Usz width,
              Usz tick_number, Oevent_list *oevent_list, Usz random_seed) {
  Glyph vars_slots[Glyphs_index_count];
  memset(vars_slots, '.', sizeof(vars_slots));
  Oper_extra_params extras;
  extras.vars_slots = &vars_slots[0];
  extras.oevent_list = oevent_list;
  extras.random_seed = random_seed;

  for (Usz iy = 0; iy < height; ++iy) {
    Glyph const *glyph_row = gbuf + iy * width;
    Mark const *mark_row = mbuf + iy * width;
    for (Usz ix = 0; ix < width; ++ix) {
      Glyph glyph_char = glyph_row[ix];
      if (ORCA_LIKELY(glyph_char == '.'))
        continue;
      Mark cell_flags = mark_row[ix] & (Mark_flag_lock | Mark_flag_sleep);
      if (cell_flags & (Mark_flag_lock | Mark_flag_sleep))
        continue;
      switch (glyph_char) {
#define UNIQUE_CASE(_oper_char, _oper_name)                                    \
  case _oper_char:                                                             \
    oper_behavior_##_oper_name(gbuf, mbuf, height, width, iy, ix, tick_number, \
                               &extras, cell_flags, glyph_char);               \
    break;

#define ALPHA_CASE(_upper_oper_char, _oper_name)                               \
  case _upper_oper_char:                                                       \
  case (char)(_upper_oper_char | 1 << 5):                                      \
    oper_behavior_##_oper_name(gbuf, mbuf, height, width, iy, ix, tick_number, \
                               &extras, cell_flags, glyph_char);               \
    break;
        UNIQUE_OPERATORS(UNIQUE_CASE)
        ALPHA_OPERATORS(ALPHA_CASE)
#undef UNIQUE_CASE
#undef ALPHA_CASE
      }
    }
  }
}
