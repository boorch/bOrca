#pragma once
#include "base.h"
#include "vmio.h"

void orca_run(Glyph *restrict gbuffer, Mark *restrict mbuffer, Usz height,
              Usz width, Usz tick_number, Oevent_list *oevent_list,
              Usz random_seed);

// MIDI CC Interpolation functions
void process_interpolated_midi_cc_event(Oevent_midi_cc_interpolated const *event, Usz tick_number);
void advance_midi_cc_interpolations(double delta_time, Oevent_list *oevent_list);

// BOORCH
extern Usz last_random_unique;
void reset_last_unique_value(void);

void midi_panic(Oevent_list *oevent_list);
