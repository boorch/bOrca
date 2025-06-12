#include "tooltips.h"
#include "gbuffer.h"
#include "sim.h"
#include <stdio.h>

// Local index_of function (duplicated from sim.c)
static U8 const index_table[128] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  //   0-15
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  //  16-31
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  //  32-47
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0,  //  48-63
    0,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, //  64-79
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 0,  0,  0,  0,  0,  //  80-95
    0,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, //  96-111
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 0,  0,  0,  0,  0}; // 112-127
static Usz index_of(Glyph c) { return index_table[c & 0x7f]; }

// Scale and chord name mappings for dynamic tooltips

// Scale names for 0-9 (used by Scale operator)
static char const *scale_names[] = {
  "Major",        // 0
  "Minor",        // 1
  "Dorian",       // 2
  "Lydian",       // 3
  "Mixolydian",   // 4
  "Pentatonic",   // 5
  "Hirajoshi",    // 6
  "Iwato",        // 7
  "Tetratonic",   // 8
  "Fifths"        // 9
};

// Enriched chord names for 0-9 (used by Midichord operator)
static char const *enriched_chord_names[] = {
  "Major+Oct",      // 0
  "Minor+Oct",      // 1
  "Sus4+Oct",       // 2
  "Sus2+Oct",       // 3
  "Major7+Oct3rd",  // 4
  "Minor7+Oct3rd",  // 5
  "Dom7+Oct5th",    // 6
  "Major6+Oct",     // 7
  "Minor6+Oct",     // 8
  "Dim+Oct"         // 9
};

// Root position chord names for a-z
static char const *root_chord_names[] = {
  "Major",          // a
  "Minor",          // b
  "Sus4",           // c
  "Sus2",           // d
  "Major7",         // e
  "Minor7",         // f
  "Dom7",           // g
  "MinorMaj7",      // h
  "Minor6",         // i
  "Major6",         // j
  "Major9",         // k
  "Minor9",         // l
  "Major add9",     // m
  "Minor add9",     // n
  "Dim",            // o
  "Half Dim7",      // p
  "Dim7",           // q
  "Aug",            // r
  "Aug7",           // s
  "Dom9",           // t
  "Dom7b9",         // u
  "Dom7#9",         // v
  "Major 6/9",      // w
  "Minor 6/9",      // x
  "Minor11",        // y
  "Minor7b5"        // z
};

// First inversion chord names for A-Z (same as root but with "1st inv" suffix)
static char const *inversion_chord_names[] = {
  "Major 1st inv",          // A
  "Minor 1st inv",          // B
  "Sus4 1st inv",           // C
  "Sus2 1st inv",           // D
  "Major7 1st inv",         // E
  "Minor7 1st inv",         // F
  "Dom7 1st inv",           // G
  "MinorMaj7 1st inv",      // H
  "Minor6 1st inv",         // I
  "Major6 1st inv",         // J
  "Major9 1st inv",         // K
  "Minor9 1st inv",         // L
  "Major add9 1st inv",     // M
  "Minor add9 1st inv",     // N
  "Dim 1st inv",            // O
  "Half Dim7 1st inv",      // P
  "Dim7 1st inv",           // Q
  "Aug 1st inv",            // R
  "Aug7 1st inv",           // S
  "Dom9 1st inv",           // T
  "Dom7b9 1st inv",         // U
  "Dom7#9 1st inv",         // V
  "Major 6/9 1st inv",      // W
  "Minor 6/9 1st inv",      // X
  "Minor11 1st inv",        // Y
  "Minor7b5 1st inv"        // Z
};

// Function to get scale/chord name based on glyph and operator type
static char const *get_scale_chord_name(Glyph g, bool is_midichord_op) {
  Usz index = index_of(g);
  
  if (g >= '0' && g <= '9') {
    // Numeric indices 0-9
    if (is_midichord_op) {
      return enriched_chord_names[index];
    } else {
      return scale_names[index];
    }
  } else if (g >= 'a' && g <= 'z') {
    // Lowercase letters - root position chords
    return root_chord_names[index - 10]; // 'a' = index 10
  } else if (g >= 'A' && g <= 'Z') {
    // Uppercase letters - first inversion chords
    return inversion_chord_names[index - 10]; // 'A' = index 10
  }
  
  return NULL;
}

// Tooltip definitions for each operator
// These mirror the PORT calls in sim.c operators

static Port_tooltip midi_tooltips[] = {
  {0, 1, "Channel"},
  {0, 2, "Octave"},
  {0, 3, "Note"},
  {0, 4, "Velocity"},
  {0, 5, "Length"}
};

static Port_tooltip midicc_tooltips[] = {
  {0, 1, "Channel"},
  {0, 2, "Control (tens)"},
  {0, 3, "Control (ones)"},
  {0, 4, "Value"}
};

static Port_tooltip midipb_tooltips[] = {
  {0, 1, "Channel"},
  {0, 2, "MSB"},
  {0, 3, "LSB"}
};

static Port_tooltip scale_tooltips[] = {
  {0, 1, "Octave"},
  {0, 2, "Root"},
  {0, 3, "Scale"},
  {0, 4, "Degree"}
};

static Port_tooltip midichord_tooltips[] = {
  {0, 1, "Channel"},
  {0, 2, "Octave"},
  {0, 3, "Root note"},
  {0, 4, "Chord type"},
  {0, 5, "Velocity"},
  {0, 6, "Length"}
};

static Port_tooltip arpeggiator_tooltips[] = {
  {0, 1, "Range"},
  {0, 2, "Pattern"}
};

static Port_tooltip bouncer_tooltips[] = {
  {0, 1, "Start"},
  {0, 2, "End"},
  {0, 3, "Rate"},
  {0, 4, "Shape"}
};

static Port_tooltip add_tooltips[] = {
  {0, -1, "Value A"},
  {0, 1, "Value B"}
};

static Port_tooltip subtract_tooltips[] = {
  {0, -1, "Value A"},
  {0, 1, "Value B"}
};

static Port_tooltip multiply_tooltips[] = {
  {0, -1, "Factor A"},
  {0, 1, "Factor B"}
};

static Port_tooltip clock_tooltips[] = {
  {0, -1, "Rate"},
  {0, 1, "Modulo"}
};

static Port_tooltip delay_tooltips[] = {
  {0, -1, "Rate"},
  {0, 1, "Modulo"}
};

static Port_tooltip if_tooltips[] = {
  {0, -1, "Value A"},
  {0, 1, "Value B"}
};

static Port_tooltip generator_tooltips[] = {
  {0, -3, "X offset"},
  {0, -2, "Y offset"},
  {0, -1, "Length"}
};

static Port_tooltip halt_tooltips[] = {
  {1, 0, "Input"}
};

static Port_tooltip increment_tooltips[] = {
  {0, -1, "Rate"},
  {0, 1, "Max"}
};

static Port_tooltip jump_tooltips[] = {
  {-1, 0, "Input"}
};

static Port_tooltip konkat_tooltips[] = {
  {0, -1, "Length"}
};

static Port_tooltip lesser_tooltips[] = {
  {0, -1, "Value A"},
  {0, 1, "Value B"}
};

static Port_tooltip offset_tooltips[] = {
  {0, -2, "X offset"},
  {0, -1, "Y offset"}
};

static Port_tooltip push_tooltips[] = {
  {0, -2, "Key"},
  {0, -1, "Length"},
  {0, 1, "Input"}
};

static Port_tooltip query_tooltips[] = {
  {0, -3, "X offset"},
  {0, -2, "Y offset"},
  {0, -1, "Length"}
};

static Port_tooltip random_tooltips[] = {
  {0, -1, "Min"},
  {0, 1, "Max"}
};

static Port_tooltip track_tooltips[] = {
  {0, -2, "Key"},
  {0, -1, "Length"}
};

static Port_tooltip uclid_tooltips[] = {
  {0, -1, "Steps"},
  {0, 1, "Max"}
};

static Port_tooltip variable_tooltips[] = {
  {0, -1, "Variable"},
  {0, 1, "Value"}
};

static Port_tooltip teleport_tooltips[] = {
  {0, -2, "X offset"},
  {0, -1, "Y offset"},
  {0, 1, "Input"}
};

static Port_tooltip yump_tooltips[] = {
  {0, -1, "Input"}
};

static Port_tooltip lerp_tooltips[] = {
  {0, -1, "Rate"},
  {0, 1, "Target"}
};

// Master operator tooltips table
static Operator_tooltips operator_tooltips_table[] = {
  {':', midi_tooltips, ORCA_ARRAY_COUNTOF(midi_tooltips)},
  {'%', midi_tooltips, ORCA_ARRAY_COUNTOF(midi_tooltips)}, // mono uses same ports
  {'!', midicc_tooltips, ORCA_ARRAY_COUNTOF(midicc_tooltips)},
  {'?', midipb_tooltips, ORCA_ARRAY_COUNTOF(midipb_tooltips)},
  {'$', scale_tooltips, ORCA_ARRAY_COUNTOF(scale_tooltips)},
  {'=', midichord_tooltips, ORCA_ARRAY_COUNTOF(midichord_tooltips)},
  {';', arpeggiator_tooltips, ORCA_ARRAY_COUNTOF(arpeggiator_tooltips)},
  {'&', bouncer_tooltips, ORCA_ARRAY_COUNTOF(bouncer_tooltips)},
  {'A', add_tooltips, ORCA_ARRAY_COUNTOF(add_tooltips)},
  {'a', add_tooltips, ORCA_ARRAY_COUNTOF(add_tooltips)},
  {'B', subtract_tooltips, ORCA_ARRAY_COUNTOF(subtract_tooltips)},
  {'b', subtract_tooltips, ORCA_ARRAY_COUNTOF(subtract_tooltips)},
  {'C', clock_tooltips, ORCA_ARRAY_COUNTOF(clock_tooltips)},
  {'c', clock_tooltips, ORCA_ARRAY_COUNTOF(clock_tooltips)},
  {'D', delay_tooltips, ORCA_ARRAY_COUNTOF(delay_tooltips)},
  {'d', delay_tooltips, ORCA_ARRAY_COUNTOF(delay_tooltips)},
  {'F', if_tooltips, ORCA_ARRAY_COUNTOF(if_tooltips)},
  {'f', if_tooltips, ORCA_ARRAY_COUNTOF(if_tooltips)},
  {'G', generator_tooltips, ORCA_ARRAY_COUNTOF(generator_tooltips)},
  {'g', generator_tooltips, ORCA_ARRAY_COUNTOF(generator_tooltips)},
  {'H', halt_tooltips, ORCA_ARRAY_COUNTOF(halt_tooltips)},
  {'h', halt_tooltips, ORCA_ARRAY_COUNTOF(halt_tooltips)},
  {'I', increment_tooltips, ORCA_ARRAY_COUNTOF(increment_tooltips)},
  {'i', increment_tooltips, ORCA_ARRAY_COUNTOF(increment_tooltips)},
  {'J', jump_tooltips, ORCA_ARRAY_COUNTOF(jump_tooltips)},
  {'j', jump_tooltips, ORCA_ARRAY_COUNTOF(jump_tooltips)},
  {'K', konkat_tooltips, ORCA_ARRAY_COUNTOF(konkat_tooltips)},
  {'k', konkat_tooltips, ORCA_ARRAY_COUNTOF(konkat_tooltips)},
  {'L', lesser_tooltips, ORCA_ARRAY_COUNTOF(lesser_tooltips)},
  {'l', lesser_tooltips, ORCA_ARRAY_COUNTOF(lesser_tooltips)},
  {'M', multiply_tooltips, ORCA_ARRAY_COUNTOF(multiply_tooltips)},
  {'m', multiply_tooltips, ORCA_ARRAY_COUNTOF(multiply_tooltips)},
  {'O', offset_tooltips, ORCA_ARRAY_COUNTOF(offset_tooltips)},
  {'o', offset_tooltips, ORCA_ARRAY_COUNTOF(offset_tooltips)},
  {'P', push_tooltips, ORCA_ARRAY_COUNTOF(push_tooltips)},
  {'p', push_tooltips, ORCA_ARRAY_COUNTOF(push_tooltips)},
  {'Q', query_tooltips, ORCA_ARRAY_COUNTOF(query_tooltips)},
  {'q', query_tooltips, ORCA_ARRAY_COUNTOF(query_tooltips)},
  {'R', random_tooltips, ORCA_ARRAY_COUNTOF(random_tooltips)},
  {'r', random_tooltips, ORCA_ARRAY_COUNTOF(random_tooltips)},
  {'T', track_tooltips, ORCA_ARRAY_COUNTOF(track_tooltips)},
  {'t', track_tooltips, ORCA_ARRAY_COUNTOF(track_tooltips)},
  {'U', uclid_tooltips, ORCA_ARRAY_COUNTOF(uclid_tooltips)},
  {'u', uclid_tooltips, ORCA_ARRAY_COUNTOF(uclid_tooltips)},
  {'V', variable_tooltips, ORCA_ARRAY_COUNTOF(variable_tooltips)},
  {'v', variable_tooltips, ORCA_ARRAY_COUNTOF(variable_tooltips)},
  {'X', teleport_tooltips, ORCA_ARRAY_COUNTOF(teleport_tooltips)},
  {'x', teleport_tooltips, ORCA_ARRAY_COUNTOF(teleport_tooltips)},
  {'Y', yump_tooltips, ORCA_ARRAY_COUNTOF(yump_tooltips)},
  {'y', yump_tooltips, ORCA_ARRAY_COUNTOF(yump_tooltips)},
  {'Z', lerp_tooltips, ORCA_ARRAY_COUNTOF(lerp_tooltips)},
  {'z', lerp_tooltips, ORCA_ARRAY_COUNTOF(lerp_tooltips)}
};

char const *get_tooltip_at_cursor(Glyph const *gbuffer, Mark const *mbuffer,
                                  Usz field_h, Usz field_w, 
                                  Usz cursor_y, Usz cursor_x) {
  Enhanced_tooltip enhanced = get_enhanced_tooltip_at_cursor(gbuffer, mbuffer, field_h, field_w, cursor_y, cursor_x);
  if (enhanced.is_enhanced) {
    // For enhanced tooltips, create a single-line version for backward compatibility
    static char single_line_tooltip[64];
    snprintf(single_line_tooltip, sizeof(single_line_tooltip), "%s %s", enhanced.line1, enhanced.line2);
    return single_line_tooltip;
  }
  return enhanced.line1; // For regular tooltips, line1 contains the tooltip text
}

Enhanced_tooltip get_enhanced_tooltip_at_cursor(Glyph const *gbuffer, Mark const *mbuffer,
                                                Usz field_h, Usz field_w, 
                                                Usz cursor_y, Usz cursor_x) {
  Enhanced_tooltip result = {NULL, NULL, false};
  
  // Check bounds
  if (cursor_y >= field_h || cursor_x >= field_w)
    return result;
    
  // Check if cursor is on a PORT
  Mark cursor_mark = mbuffer[cursor_y * field_w + cursor_x];
  if (!(cursor_mark & (Mark_flag_input | Mark_flag_output)))
    return result;
    
  // Get the glyph at cursor position
  Glyph cursor_glyph = gbuffer[cursor_y * field_w + cursor_x];
    
  // Search for operator that owns this PORT
  // We need to look in the surrounding area for an operator character
  for (Isz dy = -10; dy <= 10; dy++) {
    for (Isz dx = -10; dx <= 10; dx++) {
      Isz op_y = (Isz)cursor_y + dy;
      Isz op_x = (Isz)cursor_x + dx;
      
      // Check bounds for operator position
      if (op_y < 0 || op_x < 0 || (Usz)op_y >= field_h || (Usz)op_x >= field_w)
        continue;
        
      Glyph op_char = gbuffer[(Usz)op_y * field_w + (Usz)op_x];
      
      // Skip if not an operator character or is a dot
      if (op_char == '.' || op_char == ' ')
        continue;
        
      // Find operator in tooltips table
      for (Usz i = 0; i < ORCA_ARRAY_COUNTOF(operator_tooltips_table); i++) {
        Operator_tooltips const *op_tooltips = &operator_tooltips_table[i];
        if (op_tooltips->operator_char != op_char)
          continue;
          
        // Check if cursor position matches any PORT relative to this operator
        Isz rel_y = (Isz)cursor_y - op_y;
        Isz rel_x = (Isz)cursor_x - op_x;
        
        for (Usz j = 0; j < op_tooltips->port_count; j++) {
          Port_tooltip const *port = &op_tooltips->ports[j];
          if (port->delta_y == rel_y && port->delta_x == rel_x) {
            // Check for special scale/chord tooltip enhancement
            bool is_scale_chord_port = false;
            bool is_midichord_op = (op_char == '=');
            
            // Check if this is a scale/chord input port
            if ((op_char == '$' && rel_y == 0 && rel_x == 3) ||  // Scale operator, scale/chord input
                (op_char == '=' && rel_y == 0 && rel_x == 4)) {  // Midichord operator, chord input
              is_scale_chord_port = true;
            }
            
            // If it's a scale/chord port and has a non-empty value, enhance the tooltip
            if (is_scale_chord_port && cursor_glyph != '.') {
              char const *scale_chord_name = get_scale_chord_name(cursor_glyph, is_midichord_op);
              if (scale_chord_name) {
                // Create enhanced two-line tooltip
                result.line1 = is_midichord_op ? "Chord type:" : "Scale:";
                result.line2 = scale_chord_name;
                result.is_enhanced = true;
                return result;
              }
            }
            
            // Regular tooltip
            result.line1 = port->tooltip;
            result.line2 = NULL;
            result.is_enhanced = false;
            return result;
          }
        }
      }
    }
  }
  
  return result;
}
