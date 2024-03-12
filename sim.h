#pragma once
#include "base.h"
#include "vmio.h"

void orca_run(Glyph *restrict gbuffer, Mark *restrict mbuffer, Usz height,
              Usz width, Usz tick_number, Oevent_list *oevent_list,
              Usz random_seed);

// BOORCH
extern Usz last_random_unique;
void reset_last_unique_value(void);


