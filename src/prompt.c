/**
 * @file prompt.c
 * @brief Prompt handling implementation for AISH (AI Shell)
 */

#include "prompt.h"
#include "aish.h"
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/**
 * @brief Display the appropriate prompt based on the current mode
 * 
 * @param state The AISH state
 * @return true if successful, false otherwise
 */
bool display_prompt(AishState *state) {
    if (state == NULL) {
        return false;
    }
    
    // Clear the current line
    const char *clear_line = "\r\033[2K"; // Carriage return + clear entire line
    
    if (write(STDOUT_FILENO, clear_line, strlen(clear_line)) == -1) {
        fprintf(stderr, "Error: Failed to write clear line: %s\n", strerror(errno));
        return false;
    }
    
    // If we're in Chat mode, display our custom prompt
    if (terminal_get_mode(&state->terminal) == MODE_CHAT) {
        const char *prompt = terminal_get_prompt(&state->terminal);
        
        if (write(STDOUT_FILENO, prompt, strlen(prompt)) == -1) {
            fprintf(stderr, "Error: Failed to write prompt: %s\n", strerror(errno));
            return false;
        }
    } else {
        // In Bash mode, send a newline to trigger Bash to display its prompt
        // We'll use a simple newline character to avoid any special characters
        if (write(state->bash_master_fd, "\n", 1) == -1) {
            fprintf(stderr, "Error: Failed to write newline to bash: %s\n", strerror(errno));
            return false;
        }
    }
    
    return true;
}
