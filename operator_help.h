#pragma once
#include "base.h"

typedef struct {
  Glyph operator_char;
  char const *name;
  char const *brief_desc;
  char const *detailed_desc;
  char const *input_desc;
  char const *output_desc;
  char const *example;
} Operator_help_info;

// Function to get detailed help for an operator under cursor
char const *get_operator_help_text(Glyph operator_char);

// Function to check if an operator has detailed help available
bool has_operator_help(Glyph operator_char);
