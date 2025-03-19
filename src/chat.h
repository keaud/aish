/**
 * @file chat.h
 * @brief Chat input processing for AISH (AI Shell)
 */

#ifndef CHAT_H
#define CHAT_H

#include "aish.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Process input in Chat mode
 * 
 * @param state The AISH state
 * @param input The input string
 * @param input_len The length of the input string
 * @return true if successful, false otherwise
 */
bool process_chat_input(AishState *state, const char *input, size_t input_len);

#endif /* CHAT_H */
