#include "sim.h"
#include "gbuffer.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// stored unique random value
Usz last_random_unique = UINT_MAX;

// Note Seuqnce
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
  _('%', midi)                                                                 \
  _('*', bang)                                                                 \
  _(':', midi)                                                                 \
  _(';', udp)                                                                  \
  _('=', osc)                                                                  \
  _('?', midipb)                                                               \
  _('^', scale)                                                                \
  _('|', midichord)                                                            \
  _('$', randomunique)                                                         \
  _('&', midiarpeggiator)

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

BEGIN_OPERATOR(midicc)
  for (Usz i = 1; i < 4; ++i) {
    PORT(0, (Isz)i, IN);
  }
  STOP_IF_NOT_BANGED;
  Glyph channel_g = PEEK(0, 1);
  Glyph control_g = PEEK(0, 2);
  Glyph value_g = PEEK(0, 3);
  if (channel_g == '.' || control_g == '.')
    return;
  Usz channel = index_of(channel_g);
  if (channel > 15)
    return;
  PORT(0, 0, OUT);
  Oevent_midi_cc *oe =
      (Oevent_midi_cc *)oevent_list_alloc_item(extra_params->oevent_list);
  oe->oevent_type = Oevent_type_midi_cc;
  oe->channel = (U8)channel;
  oe->control = (U8)index_of(control_g);
  oe->value = (U8)(index_of(value_g) * 127 / 35); // 0~35 -> 0~127
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
    // If no velocity is specified, set it to full.
    vel_num = 127;
  } else {
    vel_num = index_of(velocity_g);
    // MIDI notes with velocity zero are actually note-offs. (MIDI has two ways
    // to send note offs. Zero-velocity is the alternate way.) If there is a zero
    // velocity, we'll just not do anything.
    if (vel_num == 0)
      return;
    vel_num = vel_num * 8 - 1; // 1~16 -> 7~127
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
  // Mask used here to suppress bad GCC Wconversion for bitfield. This is bad
  // -- we should do something smarter than this.
  oe->duration = (U8)(index_of(length_g) & 0x7Fu);
  oe->mono = This_oper_char == '%' ? 1 : 0;
END_OPERATOR

BEGIN_OPERATOR(udp)
  Usz n = width - x - 1;
  if (n > 16)
    n = 16;
  Glyph const *restrict gline = gbuffer + y * width + x + 1;
  Mark *restrict mline = mbuffer + y * width + x + 1;
  Glyph cpy[Oevent_udp_string_count];
  Usz i;
  for (i = 0; i < n; ++i) {
    Glyph g = gline[i];
    if (g == '.')
      break;
    cpy[i] = g;
    mline[i] |= Mark_flag_lock;
  }
  n = i;
  STOP_IF_NOT_BANGED;
  PORT(0, 0, OUT);
  Oevent_udp_string *oe =
      (Oevent_udp_string *)oevent_list_alloc_item(extra_params->oevent_list);
  oe->oevent_type = (U8)Oevent_type_udp_string;
  oe->count = (U8)n;
  for (i = 0; i < n; ++i) {
    oe->chars[i] = cpy[i];
  }
END_OPERATOR

BEGIN_OPERATOR(osc)
  PORT(0, 1, IN | PARAM);
  PORT(0, 2, IN | PARAM);
  Usz len = index_of(PEEK(0, 2));
  if (len > Oevent_osc_int_count)
    len = Oevent_osc_int_count;
  for (Usz i = 0; i < len; ++i) {
    PORT(0, (Isz)i + 3, IN);
  }
  STOP_IF_NOT_BANGED;
  Glyph g = PEEK(0, 1);
  if (g != '.') {
    PORT(0, 0, OUT);
    U8 buff[Oevent_osc_int_count];
    for (Usz i = 0; i < len; ++i) {
      buff[i] = (U8)index_of(PEEK(0, (Isz)i + 3));
    }
    Oevent_osc_ints *oe =
        &oevent_list_alloc_item(extra_params->oevent_list)->osc_ints;
    oe->oevent_type = (U8)Oevent_type_osc_ints;
    oe->glyph = g;
    oe->count = (U8)len;
    for (Usz i = 0; i < len; ++i) {
      oe->numbers[i] = buff[i];
    }
  }
END_OPERATOR

BEGIN_OPERATOR(midipb)
  for (Usz i = 1; i < 4; ++i) {
    PORT(0, (Isz)i, IN);
  }
  STOP_IF_NOT_BANGED;
  Glyph channel_g = PEEK(0, 1);
  Glyph msb_g = PEEK(0, 2);
  Glyph lsb_g = PEEK(0, 3);
  if (channel_g == '.')
    return;
  Usz channel = index_of(channel_g);
  if (channel > 15)
    return;
  PORT(0, 0, OUT);
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
  Isz in_x = (Isz)index_of(PEEK(0, -3)) + 1;
  Isz in_y = (Isz)index_of(PEEK(0, -2));
  Isz len = (Isz)index_of(PEEK(0, -1));
  Isz out_x = 1 - len;
  PORT(0, -3, IN | PARAM); // x
  PORT(0, -2, IN | PARAM); // y
  PORT(0, -1, IN | PARAM); // len
  // todo direct buffer manip
  for (Isz i = 0; i < len; ++i) {
    PORT(in_y, in_x + i, IN);
    PORT(1, out_x + i, OUT);
    Glyph g = PEEK(in_y, in_x + i);
    POKE(1, out_x + i, g);
  }
END_OPERATOR

BEGIN_OPERATOR(random)
  LOWERCASE_REQUIRES_BANG;
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
  Isz out_x = (Isz)index_of(PEEK(0, -2));
  Isz out_y = (Isz)index_of(PEEK(0, -1)) + 1;
  PORT(0, -2, IN | PARAM); // x
  PORT(0, -1, IN | PARAM); // y
  PORT(0, 1, IN);
  PORT(out_y, out_x, OUT | NONLOCKING);
  POKE_STUNNED(out_y, out_x, PEEK(0, 1));
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

// BOORCH's new Scale OP

// Scale intervals with base36 (Orca) to decimal conversion for C
static Usz major_scale[] = {0, 2, 4, 5, 7, 9, 11};            // "024579b"
static Usz minor_scale[] = {0, 2, 3, 5, 7, 8, 10};            // "023578a"
static Usz major_pentatonic_scale[] = {0, 2, 4, 7, 9};        // "02479"
static Usz minor_pentatonic_scale[] = {0, 3, 5, 7, 10};       // "0357a"
static Usz blues_major_scale[] = {0, 2, 3, 4, 7, 9};          // "023479"
static Usz blues_minor_scale[] = {0, 3, 5, 6, 7, 10};         // "03567a"
static Usz lydian_scale[] = {0, 2, 4, 6, 7, 9, 11};           // "024679b"
static Usz whole_scale[] = {0, 2, 4, 6, 8, 10};               // "02468a"
static Usz diminished_scale[] = {0, 1, 3, 4, 6, 7, 9, 10};    // "0134679a"
static Usz super_locrian_scale[] = {0, 1, 3, 4, 6, 8, 10};    // "013468a"
static Usz locrian_scale[] = {0, 1, 3, 5, 6, 8, 10};          // "013568a"
static Usz phrygian_scale[] = {0, 1, 3, 5, 7, 8, 10};         // "013578a"
static Usz neapolitan_minor_scale[] = {0, 1, 3, 5, 7, 8, 11}; // "013578b"
static Usz neapolitan_major_scale[] = {0, 1, 3, 5, 7, 9, 11}; // "013579b"
static Usz hex_phrygian_scale[] = {0, 1, 3, 5, 8, 10};        // "01358a"
static Usz pelog_scale[] = {0, 1, 3, 7, 8};                   // "01378"
static Usz spanish_scale[] = {0, 1, 4, 5, 7, 8, 10};          // "014578a"
static Usz bhairav_scale[] = {0, 1, 4, 5, 7, 8, 11};          // "014578b"
static Usz ahirbhairav_scale[] = {0, 1, 4, 5, 7, 9, 10};      // "014579a"
static Usz augmented2_scale[] = {0, 1, 4, 5, 8, 9};           // "014589"
static Usz purvi_scale[] = {0, 1, 4, 6, 7, 8, 11};            // "014678b"
static Usz marva_scale[] = {0, 1, 4, 6, 7, 9, 11};            // "014679b"
static Usz enigmatic_scale[] = {0, 1, 4, 6, 8, 10, 11};       // "01468ab"
static Usz scriabin_scale[] = {0, 1, 4, 7, 9};                // "01479"
static Usz indian_scale[] = {0, 4, 5, 7, 10};                 // "0457a"

// Scale array pointers
static Usz *scales[] = {major_scale,
                        minor_scale,
                        major_pentatonic_scale,
                        minor_pentatonic_scale,
                        blues_major_scale,
                        blues_minor_scale,
                        lydian_scale,
                        whole_scale,
                        diminished_scale,
                        super_locrian_scale,
                        locrian_scale,
                        phrygian_scale,
                        neapolitan_minor_scale,
                        neapolitan_major_scale,
                        hex_phrygian_scale,
                        pelog_scale,
                        spanish_scale,
                        bhairav_scale,
                        ahirbhairav_scale,
                        augmented2_scale,
                        purvi_scale,
                        marva_scale,
                        enigmatic_scale,
                        scriabin_scale,
                        indian_scale};

// Lengths of each scale
static Usz scale_lengths[] = {7, 7, 5, 5, 6, 6, 7, 6, 8, 7, 7, 7, 7,
                              7, 6, 5, 7, 7, 7, 6, 7, 7, 7, 5, 5};

BEGIN_OPERATOR(scale)
  PORT(0, -2, IN | PARAM); // Root note (0-'b')
  PORT(0, -1, IN | PARAM); // Scale
  PORT(0, 1, IN);          // Degree

  Glyph root_note_glyph = PEEK(0, -2);
  Glyph scale_glyph = PEEK(0, -1);
  Glyph degree_glyph = PEEK(0, 1);

  // If degree is empty, output should also be empty
  if (degree_glyph == '.') {
    POKE(1, 0, '.'); // Output empty
    LOCK(1,
         0); // Ensure the output is locked to prevent execution as an operator
    return;
  }

  Usz root_note_index =
      index_of(root_note_glyph); // Now directly gives the index
  Usz scale_index = index_of(scale_glyph);
  Usz degree_index = index_of(degree_glyph);

  // Ensure valid scale and root note
  Usz num_scales = sizeof(scales) / sizeof(scales[0]);
  if (scale_index >= num_scales || root_note_index > 11)
    return; // Check root_note_index for valid note range (0-'b')

  Usz scale_length = scale_lengths[scale_index];
  Usz note_index =
      (root_note_index + scales[scale_index][degree_index % scale_length]) % 12;

  // Assuming note_sequence is a mapping that aligns with '0'-'b' input for C-B
  Glyph output_note_glyph = note_sequence[note_index];
  POKE(1, 0, output_note_glyph); // Output the note
  LOCK(1, 0); // Ensure the output is locked to prevent execution as an operator
END_OPERATOR

//BOORCH's new Midichord OP
BEGIN_OPERATOR(midichord)
  for (Usz i = 1; i < 8; ++i) {
    PORT(0, (Isz)i, IN);
  }
  STOP_IF_NOT_BANGED;
  Glyph channel_g = PEEK(0, 1);
  Glyph octave_g = PEEK(0, 2);
  Glyph note_gs[3] = {PEEK(0, 3), PEEK(0, 4), PEEK(0, 5)};
  Glyph velocity_g = PEEK(0, 6);
  Glyph length_g = PEEK(0, 7);

  Usz channel = index_of(channel_g);
  int base_octave =
      (int)index_of(octave_g); // Explicitly cast for safe addition
  U8 length = (U8)(index_of(length_g) &
                   0x7Fu); // Correctly declare and initialize length

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

    Oevent_midi_note *oe =
        (Oevent_midi_note *)oevent_list_alloc_item(extra_params->oevent_list);
    oe->oevent_type = Oevent_type_midi_note;
    oe->channel = (U8)channel;
    oe->octave = (U8)base_octave;
    oe->note = note_num;
    oe->velocity = velocity;
    oe->duration = (U8)(length & 0x7F);
    oe->mono = 0;

    PORT(0, 0, OUT);
  }

END_OPERATOR

// BOORCH's new MidiArpeggiator
// Arpeggio patterns
static Usz arp00[] = {1, 2, 3};          // up
static Usz arp01[] = {3, 2, 1};          // down
static Usz arp02[] = {1, 3, 2};          // converge up
static Usz arp03[] = {3, 1, 2};          // converge down
static Usz arp04[] = {2, 1, 3};          // diverge up
static Usz arp05[] = {2, 3, 1};          // diverge down
static Usz arp06[] = {1, 2, 3, 2};       // up bounce triangle
static Usz arp07[] = {3, 2, 1, 2};       // down bounce triangle
static Usz arp08[] = {1, 2, 3, 3, 2, 1}; // up bounce sine
static Usz arp09[] = {3, 2, 1, 1, 2, 3}; // down bounce sine
static Usz arp10[] = {1, 2, 3, 0};       // up with rest
static Usz arp11[] = {3, 2, 1, 0};       // down with rest
static Usz arp12[] = {1, 3, 2, 0};       // converge up with rest
static Usz arp13[] = {3, 1, 2, 0};       // converge down with rest
static Usz arp14[] = {2, 1, 3, 0};       // diverge up with rest
static Usz arp15[] = {2, 3, 1, 0};       // diverge down with rest
static Usz arp16[] = {1, 2, 3, 2, 0};    // up bounce triangle with rest
static Usz arp17[] = {3, 2, 1, 2, 0};    // down bounce triangle with rest
static Usz arp18[] = {1, 0, 2, 3, 0};    // riff
static Usz arp19[] = {1, 0, 3, 2, 0};    // riff
static Usz arp20[] = {1, 2, 0, 3, 0};    // riff
static Usz arp21[] = {1, 3, 0, 2, 0};    // riff
static Usz arp22[] = {1, 2, 0, 1, 3};    // riff
static Usz arp23[] = {1, 3, 0, 1, 2};    // riff
static Usz arp24[] = {1, 2, 0, 1, 3, 0}; // riff
static Usz arp25[] = {1, 0, 2, 1, 0, 3}; // riff
static Usz arp26[] = {1, 0, 3, 1, 0, 2}; // riff

// Arpeggio pattern pointers
static Usz *arpPatterns[] = {arp00, arp01, arp02, arp03, arp04, arp05, arp06,
                             arp07, arp08, arp09, arp10, arp11, arp12, arp13,
                             arp14, arp15, arp16, arp17, arp18, arp19, arp20,
                             arp21, arp22, arp23, arp24, arp25, arp26};

// Lengths of each arpeggio pattern
static size_t arpPatternLengths[] = {
    sizeof(arp00) / sizeof(arp00[0]), sizeof(arp01) / sizeof(arp01[0]),
    sizeof(arp02) / sizeof(arp02[0]), sizeof(arp03) / sizeof(arp03[0]),
    sizeof(arp04) / sizeof(arp04[0]), sizeof(arp05) / sizeof(arp05[0]),
    sizeof(arp06) / sizeof(arp06[0]), sizeof(arp07) / sizeof(arp07[0]),
    sizeof(arp08) / sizeof(arp08[0]), sizeof(arp09) / sizeof(arp09[0]),
    sizeof(arp10) / sizeof(arp10[0]), sizeof(arp11) / sizeof(arp11[0]),
    sizeof(arp12) / sizeof(arp12[0]), sizeof(arp13) / sizeof(arp13[0]),
    sizeof(arp14) / sizeof(arp14[0]), sizeof(arp15) / sizeof(arp15[0]),
    sizeof(arp16) / sizeof(arp16[0]), sizeof(arp17) / sizeof(arp17[0]),
    sizeof(arp18) / sizeof(arp18[0]), sizeof(arp19) / sizeof(arp19[0]),
    sizeof(arp20) / sizeof(arp20[0]), sizeof(arp21) / sizeof(arp21[0]),
    sizeof(arp22) / sizeof(arp22[0]), sizeof(arp23) / sizeof(arp23[0]),
    sizeof(arp24) / sizeof(arp24[0]), sizeof(arp25) / sizeof(arp25[0]),
    sizeof(arp26) / sizeof(arp26[0])};

BEGIN_OPERATOR(midiarpeggiator)
  // Define input ports for pattern index, current note position, octave range and direction, and MIDI parameters
  PORT(0, -3, IN | PARAM); // Arpeggio Pattern Index
  PORT(0, -2,
       IN |
           PARAM); // Note to play (based on selected arpeggio pattern's offset)
  PORT(0, -1, IN | PARAM); // Octave range and direction

  // Additional inputs for MIDI event
  PORT(0, 1, IN); // Channel
  PORT(0, 2, IN); // Base Octave
  PORT(0, 3, IN); // Note 1
  PORT(0, 4, IN); // Note 2
  PORT(0, 5, IN); // Note 3
  PORT(0, 6, IN); // Velocity
  PORT(0, 7, IN); // Length

  STOP_IF_NOT_BANGED;

  Usz arp_pattern_index = index_of(PEEK(0, -3));
  Usz current_position = index_of(PEEK(0, -2));
  Glyph octave_range_glyph = PEEK(0, -1);
  Usz octave_range_index = index_of(octave_range_glyph);

  // Determine octave span and direction
  bool direction_down = false;
  bool mono = false;
  Usz octave_span = 1;

  // Parse octave range parameter
  if (octave_range_index >= 1 && octave_range_index <= 4) {
    // 1-4: Ascending monophonic
    direction_down = false;
    octave_span = octave_range_index;
    mono = true;
  } else if (octave_range_index >= 5 && octave_range_index <= 8) {
    // 5-8: Ascending polyphonic
    direction_down = false;
    octave_span = octave_range_index - 4;
    mono = false;
  } else if (octave_range_index >= 10 && octave_range_index <= 13) {
    // a-d: Descending monophonic
    direction_down = true;
    octave_span = octave_range_index - 9;
    mono = true;
  } else if (octave_range_index >= 14 && octave_range_index <= 17) {
    // e-h: Descending polyphonic
    direction_down = true;
    octave_span = octave_range_index - 13;
    mono = false;
  } else {
    // All other values (0, 9, i+): No output
    return;
  }

  // Get pattern length and current octave
  size_t pattern_length =
      arpPatternLengths[arp_pattern_index % (sizeof(arpPatternLengths) /
                                             sizeof(arpPatternLengths[0]))];

  // Calculate current note in pattern and adjust octave if necessary
  Usz base_octave = index_of(PEEK(0, 2));
  Usz current_octave = base_octave;
  Usz note_in_pattern_index;
  if (!direction_down) {
    current_octave += (current_position / pattern_length) % octave_span;
    note_in_pattern_index = current_position % pattern_length;
  } else {
    current_octave +=
        octave_span - 1 - ((current_position / pattern_length) % octave_span);
    note_in_pattern_index =
        pattern_length - 1 - (current_position % pattern_length);
  }

  // Ensure current_octave is within MIDI limits
  if (current_octave > 9)
    current_octave = 9;

  // Select the note to play from the pattern
  Usz *current_pattern =
      arpPatterns[arp_pattern_index %
                  (sizeof(arpPatterns) / sizeof(arpPatterns[0]))];
  Usz note_to_play_index =
      current_pattern[note_in_pattern_index] - 1; // Adjusted for 0-based index

  Glyph note_gs[3] = {PEEK(0, 3), PEEK(0, 4), PEEK(0, 5)};
  U8 note_num = midi_note_number_of(note_gs[note_to_play_index]);
  if (note_num == UINT8_MAX)
    return; // Skip if invalid note

  // Channel, velocity, and length
  U8 channel = (U8)index_of(PEEK(0, 1));
  U8 velocity =
      (PEEK(0, 6) == '.' ? 127 : (U8)(index_of(PEEK(0, 6)) * 127 / 35));
  U8 length = (U8)(index_of(PEEK(0, 7)) & 0x7Fu);

  // Before sending the MIDI note, check if the note to play is a rest (0)
  if (note_to_play_index == (Usz)-1) { // If it's a rest
    PORT(0, 0, OUT); // Optionally mark output or maintain visual indication
    return;          // Skip this iteration, ensuring a rest
  }

  // Use previously declared variables without re-declaration
  note_num = midi_note_number_of(note_gs[note_to_play_index]);
  if (note_num == UINT8_MAX)
    return; // Skip if invalid note

  // Normal note playing logic using already declared variables
  // Note: No need to re-declare 'channel', 'velocity', and 'length' here

  // Send MIDI note event
  Oevent_midi_note *oe =
      (Oevent_midi_note *)oevent_list_alloc_item(extra_params->oevent_list);
  oe->oevent_type = Oevent_type_midi_note;
  oe->channel = channel;
  oe->octave = (U8)current_octave;
  oe->note = note_num;
  oe->velocity = velocity;
  oe->duration = (U8)(length & 0x7F);
  oe->mono = mono ? 1 : 0; // Set mono flag based on octave range

  PORT(0, 0, OUT); // Mark output to indicate operation
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

BEGIN_OPERATOR(randomunique)
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
