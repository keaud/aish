/**
 * @file terminal.h
 * @brief Terminal handling for AISH (AI Shell)
 */

#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdbool.h>
#include <stddef.h>
#include <termios.h>

/**
 * @enum InputMode
 * @brief Enumeration of input modes for AISH
 */
typedef enum {
    MODE_BASH,  /**< Standard Bash mode */
    MODE_CHAT   /**< Chat mode for AI assistance */
} InputMode;

/**
 * @struct TerminalState
 * @brief Structure to hold terminal state information
 */
typedef struct {
    struct termios original_termios;  /**< Original terminal settings */
    bool raw_mode_enabled;            /**< Flag indicating if raw mode is enabled */
    InputMode current_mode;           /**< Current input mode */
    char *input_buffer;               /**< Buffer for user input */
    size_t buffer_size;               /**< Size of input buffer */
    size_t buffer_pos;                /**< Current position in input buffer */
} TerminalState;

/**
 * @brief Initialize terminal state
 * 
 * @param term Pointer to TerminalState structure to initialize
 * @return true if initialization was successful, false otherwise
 */
bool terminal_init(TerminalState *term);

/**
 * @brief Enable raw mode for terminal input
 * 
 * @param term Pointer to TerminalState structure
 * @return true if enabling raw mode was successful, false otherwise
 */
bool terminal_enable_raw_mode(TerminalState *term);

/**
 * @brief Disable raw mode and restore original terminal settings
 * 
 * @param term Pointer to TerminalState structure
 */
void terminal_disable_raw_mode(TerminalState *term);

/**
 * @brief Process a keypress and determine if mode should be toggled
 * 
 * @param term Pointer to TerminalState structure
 * @param key The key that was pressed
 * @param input_pos Current position in the input buffer
 * @return true if mode was toggled, false otherwise
 */
bool terminal_process_key(TerminalState *term, char key, size_t input_pos);

/**
 * @brief Toggle between Bash and Chat modes
 * 
 * @param term Pointer to TerminalState structure
 */
void terminal_toggle_mode(TerminalState *term);

/**
 * @brief Get the current input mode
 * 
 * @param term Pointer to TerminalState structure
 * @return Current InputMode
 */
InputMode terminal_get_mode(TerminalState *term);

/**
 * @brief Clean up terminal state and free resources
 * 
 * @param term Pointer to TerminalState structure
 */
void terminal_cleanup(TerminalState *term);

#endif /* TERMINAL_H */
