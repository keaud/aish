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

bool terminal_process_key(TerminalState *term, char key, bool at_start_of_line) {
    if (term == NULL) {
        return false;
    }
    
    // Check if Tab key was pressed at the start of a line
    if (key == TAB_KEY && at_start_of_line) {
        terminal_toggle_mode(term);
        return true;
    }
    
    return false;
}

void terminal_toggle_mode(TerminalState *term) {
    if (term == NULL) {
        return;
    }
    
    // Toggle between Bash and Chat modes
    if (term->current_mode == MODE_BASH) {
        term->current_mode = MODE_CHAT;
        fprintf(stderr, "\n[AISH: Chat Mode - Type your query]\n");
    } else {
        term->current_mode = MODE_BASH;
        fprintf(stderr, "\n[AISH: Bash Mode]\n");
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
