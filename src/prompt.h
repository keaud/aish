/**
 * @file prompt.h
 * @brief Prompt handling for AISH (AI Shell)
 */

#ifndef PROMPT_H
#define PROMPT_H

#include "aish.h"
#include <stdbool.h>

/**
 * @brief Display the appropriate prompt based on the current mode
 * 
 * @param state The AISH state
 * @return true if successful, false otherwise
 */
bool display_prompt(AishState *state);

#endif /* PROMPT_H */
