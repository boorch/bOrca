#include "operator_help.h"
#include <stdio.h>
#include <string.h>

// Detailed operator help data
static Operator_help_info operator_help_data[] = {
  // Movement operators
  {'N', "north", "Moves northward, or bangs",
   "Moves one cell north, or bangs if lowercase 'n' and has neighboring bang.",
   "No inputs required",
   "Moves operator position or triggers bang",
   "N  -> moves operator north\nn* -> bangs if * is adjacent"},
   
  {'E', "east", "Moves eastward, or bangs",
   "Moves one cell east, or bangs if lowercase 'e' and has neighboring bang.",
   "No inputs required",
   "Moves operator position or triggers bang",
   "E  -> moves operator east\ne* -> bangs if * is adjacent"},
   
  {'S', "south", "Moves southward, or bangs", 
   "Moves one cell south, or bangs if lowercase 's' and has neighboring bang.",
   "No inputs required",
   "Moves operator position or triggers bang",
   "S  -> moves operator south\ns* -> bangs if * is adjacent"},
   
  {'W', "west", "Moves westward, or bangs",
   "Moves one cell west, or bangs if lowercase 'w' and has neighboring bang.",
   "No inputs required", 
   "Moves operator position or triggers bang",
   "W  -> moves operator west\nw* -> bangs if * is adjacent"},

  // Arithmetic operators
  {'A', "add", "Outputs sum of inputs",
   "Adds two values together with base-36 arithmetic. Lowercase 'a' requires bang.",
   "Left: First addend (0-z)\nRight: Second addend (0-z)",
   "Bottom: Sum of inputs",
   ".A.\n123\n.4. -> outputs 4 (1+3)"},
   
  {'B', "subtract", "Outputs difference of inputs",
   "Subtracts right input from left input. Lowercase 'b' requires bang.",
   "Left: Minuend (0-z)\nRight: Subtrahend (0-z)",
   "Bottom: Absolute difference",
   ".B.\n531\n.2. -> outputs 2 (|5-3|)"},
   
  {'M', "multiply", "Outputs product of inputs",
   "Multiplies two inputs with base-36 arithmetic. Lowercase 'm' requires bang.",
   "Left: First factor (0-z)\nRight: Second factor (0-z)",
   "Bottom: Product of inputs",
   ".M.\n234\n.8. -> outputs 8 (2*4)"},

  // Clock operators  
  {'C', "clock", "Outputs modulo of frame",
   "Outputs the current frame number modulo the right input at the rate specified by left input.",
   "Left: Rate (1-z, 0=1)\nRight: Modulo (1-z, 0=8)",
   "Bottom: Frame count % modulo",
   ".C.\n138\n.3. -> outputs frame%8 every 1 tick"},
   
  {'D', "delay", "Bangs on modulo of frame",
   "Outputs '*' when (frame % (rate * modulo)) == 0. Lowercase 'd' requires bang.",
   "Left: Rate (1-z, 0=1)\nRight: Modulo (1-z, 0=8)",
   "Bottom: '*' or '.'",
   ".D.\n128\n.*. -> bangs every 2 ticks"},

  // Logic operators
  {'F', "if", "Bangs if inputs are equal",
   "Outputs '*' if left and right inputs are equal, '.' otherwise. Lowercase 'f' requires bang.",
   "Left: First value\nRight: Second value",
   "Bottom: '*' if equal, '.' if not",
   ".F.\n333\n.*. -> outputs '*' (3==3)"},
   
  {'L', "lesser", "Outputs smallest input",
   "Compares two inputs and outputs the smaller value. Lowercase 'l' requires bang.",
   "Left: First value (0-z)\nRight: Second value (0-z)",
   "Bottom: Smaller of the two inputs",
   ".L.\n359\n.5. -> outputs 5 (min(3,9))"},

  // Timing operators
  {'I', "increment", "Increments southward operand",
   "Increments the value below by the rate specified on the left. Lowercase 'i' requires bang.",
   "Left: Rate (1-z, 0=1)\nRight: Max value (1-z, 0=36)",
   "Bottom: Input/Output value",
   ".I.\n13z\n.a. -> increments 'a' by 1, max 'z'"},

  // Reading/Writing operators
  {'O', "read", "Reads operand with offset",
   "Reads a value from a position offset by the specified coordinates.",
   "Left: X offset\nRight: Y offset",
   "Bottom: Value at offset position",
   ".O.\n120\n.a. -> reads value at (1,2) offset"},
   
  {'P', "push", "Writes eastward operand", 
   "Writes the value from the right input to a position determined by key and length.",
   "Left: Key (position selector)\nRight: Value to write",
   "Output: Writes value eastward based on key",
   ".P.\n2a.\n... -> writes 'a' to position based on key 2"},
   
  {'Q', "query", "Reads operands with offset",
   "Reads multiple values from offset positions. REFACTORED: Length, Y offset, X offset.",
   "Left-3: Length\nLeft-2: Y offset\nLeft-1: X offset",
   "Right: Multiple outputs based on length",
   "Q\n321\n... -> reads 3 values from offset (1,2)"},
   
  {'T', "track", "Reads eastward operand",
   "Reads values from eastward positions based on key and length parameters.",
   "Left-2: Key (position selector)\nLeft-1: Length",
   "Right: Values read from tracked positions",
   ".T.\n2a.\n.b. -> tracks values eastward"},
   
  {'X', "teleport", "Writes operand with offset", 
   "Teleports multiple glyphs to offset position. REFACTORED: Length, Y offset, X offset.",
   "Left-3: Length\nLeft-2: Y offset\nLeft-1: X offset\nRight: Values to teleport",
   "Output: Writes values at offset position",
   "X\n321abc\n...... -> teleports 'abc' to offset (1,2)"},

  // Variable operators
  {'V', "variable", "Reads and writes variable",
   "Stores or retrieves values from global variables. Lowercase 'v' requires bang.",
   "Left: Variable name (to write)\nRight: Variable name (to read) or value",
   "Bottom: Retrieved value (when reading)",
   ".V.\naa5\n.5. -> stores 5 in var 'a', reads back 5"},
   
  {'K', "konkat", "Reads multiple variables",
   "Reads multiple variables and outputs their values. Lowercase 'k' requires bang.",
   "Left: Length of variable list\nRight: Variable names",
   "Bottom: Values from variables",
   ".K.\n2ab\n.xy -> reads vars 'a','b', outputs values"},

  // Pattern operators
  {'G', "generator", "Writes operands with offset",
   "Generates a sequence of values at specified offset positions.",
   "Left-3: X offset\nLeft-2: Y offset\nLeft-1: Length\nRight: Values to generate",
   "Output: Writes sequence at offset",
   "G\n123abc\n...... -> writes 'abc' at offset (1,2)"},
   
  {'U', "uclid", "Bangs on Euclidean rhythm",
   "Generates Euclidean rhythm patterns. Lowercase 'u' requires bang.",
   "Left: Steps in pattern\nRight: Maximum steps",
   "Bottom: '*' for beats, '.' for rests",
   ".U.\n38.\n.*. -> Euclidean 3/8 rhythm"},
   
  {'R', "random", "Outputs random value", 
   "Generates random values. 'R' runs every tick, 'r' requires bang and avoids duplicates.",
   "Left: Minimum value\nRight: Maximum value",
   "Bottom: Random value in range",
   ".R.\n0z.\n.f. -> random value 0-z"},

  // Control operators
  {'H', "halt", "Halts southward operand",
   "Stops execution of the operator below. Lowercase 'h' requires bang.",
   "Bottom: Operator to halt",
   "Output: Prevents operator below from executing",
   "H\n* -> halts the '*' bang below"},
   
  {'J', "jump", "Outputs northward operand",
   "Reads the value above and outputs it below. Lowercase 'j' requires bang.",
   "Top: Value to jump",
   "Bottom: Jumped value",
   "a\nJ\n.a -> jumps 'a' downward"},
   
  {'Y', "yump", "Outputs westward operand", 
   "Reads value from the left and outputs it to the right. Lowercase 'y' requires bang.",
   "Left: Value to yump",
   "Right: Jumped value",
   "aY.a -> yumps 'a' rightward"},
   
  {'Z', "lerp", "Transitions operand to target",
   "Linear interpolation between current value and target. Lowercase 'z' requires bang.",
   "Left: Rate (interpolation speed)\nRight: Target value\nBottom: Current value",
   "Bottom: Interpolated value",
   ".Z.\n25a\n.b. -> lerps from 'a' to '5' at rate '2'"},

  // MIDI operators
  {':', "midi", "Sends MIDI note",
   "Sends MIDI note messages with specified parameters.",
   "Right-1: Channel (0-F)\nRight-2: Octave (0-9)\nRight-3: Note (0-C)\nRight-4: Velocity (0-z)\nRight-5: Duration (0-z)",
   "Output: MIDI note event",
   ":03C88 -> MIDI note C3, channel 0, vel 8, dur 8"},
   
  {'!', "cc", "Sends MIDI control change",
   "Sends MIDI Control Change with hex control number and interpolation.",
   "Right-1: Channel (0-F)\nRight-2: Control High nibble (0-F)\nRight-3: Control Low nibble (0-F)\nRight-4: Value (0-z)\nRight-5: Lerp amount (0-z)",
   "Output: MIDI CC event", 
   "!04A8f -> CC #4A on channel 0, value 8, lerp f"},
   
  {'?', "pb", "Sends MIDI pitch bend",
   "Sends MIDI pitch bend messages.",
   "Right-1: Channel (0-F)\nRight-2: MSB (0-z)\nRight-3: LSB (0-z)",
   "Output: MIDI pitch bend event",
   "?088 -> Pitch bend channel 0, MSB 8, LSB 8"},
   
  {'%', "mono", "Sends MIDI monophonic note",
   "Same as MIDI operator but monophonic (one note at a time).",
   "Right-1: Channel (0-F)\nRight-2: Octave (0-9)\nRight-3: Note (0-C)\nRight-4: Velocity (0-z)\nRight-5: Duration (0-z)",
   "Output: Monophonic MIDI note",
   "%03C88 -> Mono MIDI note C3"},

  // Custom bOrca operators
  {'$', "scale", "Outputs note based on root, scale, degree",
   "Unified scale/chord system with 62 options: scales (0-9), chords (a-z), inversions (A-Z).",
   "Right-1: Octave (0-9)\nRight-2: Root note (C,c,D,d,etc)\nRight-3: Scale/Chord (0-9,a-z,A-Z)\nRight-4: Degree (0-z)",
   "Top: Octave output\nRight: Note output",
   "$3C02 -> C Major scale, 3rd degree = octave 3, note E"},
   
  {'=', "midichord", "Sends preset chords over MIDI",
   "Outputs MIDI chord using unified scale system.",
   "Right-1: Channel (0-F)\nRight-2: Octave (0-9)\nRight-3: Root note\nRight-4: Chord type\nRight-5: Velocity\nRight-6: Duration",
   "Output: MIDI chord notes",
   "=13Ca88 -> C Major chord, channel 1, octave 3"},
   
  {';', "arpeggiator", "Outputs degree numbers for Scale operator",
   "Degree-based arpeggiator with multiple patterns, feeds Scale operator.",
   "Left: Range (1-4)\nRight: Pattern (0-9,a-d)",
   "Right: Degree numbers (0-z)",
   ";12 -> Range 1, Up-Down pattern"},
   
  {'&', "bouncer", "A rudimentary LFO-like operator",
   "Creates smooth transitions between values using waveform patterns.",
   "Right-1: Start value (0-z)\nRight-2: End value (0-z)\nRight-3: Rate (0-z)\nRight-4: Shape (0-7)",
   "Bottom: Interpolated value",
   "&3a22 -> Triangle wave from 3 to 'a', rate 2"},

  // Special operators
  {'*', "bang", "Bangs neighboring operands",
   "Triggers adjacent operators that require bangs for execution.",
   "No inputs required",
   "Output: Triggers neighboring operators",
   "* -> bangs adjacent lowercase operators"},
   
  {'#', "comment", "Halts line",
   "Comments out the rest of the line, preventing execution.",
   "No inputs required",
   "Output: Stops line execution",
   "abc#def -> only 'abc' executes"},
};

static const size_t operator_help_count = sizeof(operator_help_data) / sizeof(operator_help_data[0]);

// Get detailed help text for a specific operator
char const *get_operator_help_text(Glyph operator_char) {
  // Convert to uppercase for lookup since we store uppercase versions
  Glyph lookup_char = glyph_lowered_unsafe(operator_char);
  if (lookup_char >= 'a' && lookup_char <= 'z') {
    lookup_char = lookup_char - 'a' + 'A';
  }
  
  // Find the operator in our help data
  for (size_t i = 0; i < operator_help_count; i++) {
    if (operator_help_data[i].operator_char == lookup_char || 
        operator_help_data[i].operator_char == operator_char) {
      // Build formatted help text
      static char help_buffer[2048];
      snprintf(help_buffer, sizeof(help_buffer),
        "OPERATOR: %c (%s)\n\n"
        "%s\n\n"
        "DESCRIPTION:\n%s\n\n"
        "INPUTS:\n%s\n\n"
        "OUTPUTS:\n%s\n\n"
        "EXAMPLE:\n%s",
        operator_char,
        operator_help_data[i].name,
        operator_help_data[i].brief_desc,
        operator_help_data[i].detailed_desc,
        operator_help_data[i].input_desc,
        operator_help_data[i].output_desc,
        operator_help_data[i].example
      );
      return help_buffer;
    }
  }
  
  // No help found
  return NULL;
}

// Check if operator has help available
bool has_operator_help(Glyph operator_char) {
  return get_operator_help_text(operator_char) != NULL;
}
