#pragma once
#include "base.h"
#include "vmio.h"

void orca_run(Glyph *restrict gbuffer, Mark *restrict mbuffer, Usz height,
              Usz width, Usz tick_number, Oevent_list *oevent_list,
              Usz random_seed);

// BOORCH
Usz last_random_unique = UINT_MAX; // Or another suitable initial value

void reset_last_unique_value(void) {
    last_random_unique = UINT_MAX; // Reset the value
}

