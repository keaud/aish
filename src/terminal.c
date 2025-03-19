/**
 * @file terminal.c
 * @brief Implementation of terminal handling for AISH
 */

#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>

#define INITIAL_BUFFER_SIZE 1024
#define TAB_KEY '\t'

bool terminal_init(TerminalState *term) {
    if (term == NULL) {
        return false;
    }
    
    // Initialize terminal state
    term->raw_mode_enabled = false;
    term->current_mode = MODE_BASH;
    
    // Allocate input buffer
    term->buffer_size = INITIAL_BUFFER_SIZE;
    term->input_buffer = (char *)malloc(term->buffer_size);
    if (term->input_buffer == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for input buffer\n");
        return false;
    }
    
    term->buffer_pos = 0;
    term->input_buffer[0] = '\0';
    
    return true;
}

bool terminal_enable_raw_mode(TerminalState *term) {
    if (term == NULL) {
        return false;
    }
    
    // Get current terminal attributes
    if (tcgetattr(STDIN_FILENO, &term->original_termios) == -1) {
        fprintf(stderr, "Error: Failed to get terminal attributes: %s\n", strerror(errno));
        return false;
    }
    
    // Create a copy to modify
    struct termios raw = term->original_termios;
    
    // Disable echo, canonical mode, and various control signals
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    
    // Disable software flow control and special handling of CR/NL
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    
    // Disable output processing
    raw.c_oflag &= ~(OPOST);
    
    // Set character size to 8 bits
    raw.c_cflag |= (CS8);
    
    // Set read timeout to 0.1 seconds
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    
    // Apply the modified attributes
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        fprintf(stderr, "Error: Failed to set terminal attributes: %s\n", strerror(errno));
        return false;
    }
    
    term->raw_mode_enabled = true;
    return true;
}

void terminal_disable_raw_mode(TerminalState *term) {
    if (term == NULL || !term->raw_mode_enabled) {
        return;
    }
    
    // Restore original terminal attributes
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &term->original_termios) == -1) {
        fprintf(stderr, "Warning: Failed to restore terminal attributes: %s\n", strerror(errno));
    }
    
    term->raw_mode_enabled = false;
}

bool terminal_process_key(TerminalState *term, char key, size_t input_pos) {
    if (term == NULL) {
        return false;
    }
    
    // In Bash mode, only process Tab when input buffer is empty
    // In Chat mode, allow Tab to toggle back to Bash mode regardless of position
    if (key == TAB_KEY && (input_pos == 0 || term->current_mode == MODE_CHAT)) {
        // We don't call terminal_toggle_mode here because we need the bash_fd parameter
        // The caller will call terminal_toggle_mode with the bash_fd parameter
        if (term->current_mode == MODE_BASH) {
            term->current_mode = MODE_CHAT;
        } else {
            term->current_mode = MODE_BASH;
        }
        return true;
    }
    
    return false;
}

void terminal_toggle_mode(TerminalState *term, int bash_fd) {
    if (term == NULL) {
        return;
    }
    
    // Toggle between Bash and Chat modes
    if (term->current_mode == MODE_BASH) {
        term->current_mode = MODE_CHAT;
    } else {
        term->current_mode = MODE_BASH;
    }
    
    // Update the prompt
    terminal_update_prompt(term, bash_fd);
}

void terminal_update_prompt(TerminalState *term, int bash_fd) {
    if (term == NULL || bash_fd < 0) {
        return;
    }
    
    // No need to display any message here
    // The prompt will be displayed by the caller
}

const char *terminal_get_prompt(TerminalState *term) {
    if (term == NULL) {
        return "";
    }
    
    if (term->current_mode == MODE_CHAT) {
        return "aish (CHAT): ";
    } else {
        // In Bash mode, we don't display our own prompt
        // We let Bash handle its own prompt
        return "";
    }
}

InputMode terminal_get_mode(TerminalState *term) {
    if (term == NULL) {
        return MODE_BASH; // Default to Bash mode if term is NULL
    }
    
    return term->current_mode;
}

void terminal_cleanup(TerminalState *term) {
    if (term == NULL) {
        return;
    }
    
    // Disable raw mode if it's enabled
    if (term->raw_mode_enabled) {
        terminal_disable_raw_mode(term);
    }
    
    // Free input buffer
    if (term->input_buffer != NULL) {
        free(term->input_buffer);
        term->input_buffer = NULL;
    }
}
