#pragma once
#include "base.h"
#include "gbuffer.h"

// Tooltip system for ORCA operators
// Provides tooltip text for PORTs when cursor is positioned on them

typedef struct {
  Isz delta_y, delta_x;
  char const *tooltip;
} Port_tooltip;

typedef struct {
  Glyph operator_char;
  Port_tooltip const *ports;
  Usz port_count;
} Operator_tooltips;

// Enhanced tooltip structure for scale/chord tooltips
typedef struct {
  char const *line1;  // First line (e.g., "Scale:" or "Chord type:")
  char const *line2;  // Second line (e.g., "Major" or "Minor 1st inv")
  bool is_enhanced;   // true if this is a two-line enhanced tooltip
} Enhanced_tooltip;

// Get tooltip text for position under cursor
// Returns NULL if no tooltip should be displayed
char const *get_tooltip_at_cursor(Glyph const *gbuffer, Mark const *mbuffer,
                                  Usz field_h, Usz field_w, 
                                  Usz cursor_y, Usz cursor_x);

// Get enhanced tooltip information for position under cursor
// Returns enhanced tooltip info, including whether it's a two-line tooltip
Enhanced_tooltip get_enhanced_tooltip_at_cursor(Glyph const *gbuffer, Mark const *mbuffer,
                                                Usz field_h, Usz field_w, 
                                                Usz cursor_y, Usz cursor_x);
