#include "tooltips.h"
#include "gbuffer.h"

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
  {0, -3, "Length"},
  {0, -2, "Y offset"},
  {0, -1, "X offset"}
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
  {0, -3, "Count"},
  {0, -2, "Y offset"},
  {0, -1, "X offset"}
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
  // Check bounds
  if (cursor_y >= field_h || cursor_x >= field_w)
    return NULL;
    
  // Check if cursor is on a PORT
  Mark cursor_mark = mbuffer[cursor_y * field_w + cursor_x];
  if (!(cursor_mark & (Mark_flag_input | Mark_flag_output)))
    return NULL;
    
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
            return port->tooltip;
          }
        }
      }
    }
  }
  
  return NULL;
}
