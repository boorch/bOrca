#pragma once
#include "base.h"
#include "vmio.h"

// Port information for tooltips
typedef struct {
  Isz delta_y, delta_x;
  char const *name;
} Port_info;

#define MAX_PORTS_PER_OPERATOR 32

// Function to get port names for an operator
char const *get_operator_port_name(Glyph operator_char, Usz operator_y, Usz operator_x, 
                                   Usz port_y, Usz port_x, Usz grid_height, Usz grid_width,
                                   Glyph const *gbuffer, Mark const *mbuffer);

void orca_run(Glyph *restrict gbuffer, Mark *restrict mbuffer, Usz height,
              Usz width, Usz tick_number, Oevent_list *oevent_list,
              Usz random_seed);

// BOORCH
extern Usz last_random_unique;
void reset_last_unique_value(void);

void midi_panic(Oevent_list *oevent_list);
