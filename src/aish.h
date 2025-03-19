/**
 * @file aish.h
 * @brief Main header for AISH (AI Shell)
 */

#ifndef AISH_H
#define AISH_H

#include "config.h"
#include "terminal.h"
#include "api.h"
#include <stdbool.h>
#include <termios.h>
#include <sys/types.h>

/**
 * @struct AishState
 * @brief Structure to hold the state of the AISH program
 */
typedef struct {
    Config config;              /**< Configuration settings */
    TerminalState terminal;     /**< Terminal state */
    pid_t bash_pid;             /**< PID of the spawned Bash process */
    int bash_master_fd;         /**< Master file descriptor for pty */
    bool running;               /**< Flag indicating if the program is running */
} AishState;

/**
 * @brief Initialize AISH state
 * 
 * @param state Pointer to AishState structure to initialize
 * @return true if initialization was successful, false otherwise
 */
bool aish_init(AishState *state);

/**
 * @brief Spawn a Bash process
 * 
 * @param state Pointer to AishState structure
 * @return true if spawning was successful, false otherwise
 */
bool aish_spawn_bash(AishState *state);

/**
 * @brief Main program loop
 * 
 * @param state Pointer to AishState structure
 * @return Exit code
 */
int aish_run(AishState *state);

/**
 * @brief Process user input
 * 
 * @param state Pointer to AishState structure
 * @param input User input string
 * @param input_len Length of user input
 * @return true if processing was successful, false otherwise
 */
bool aish_process_input(AishState *state, const char *input, size_t input_len);

/**
 * @brief Process Bash output
 * 
 * @param state Pointer to AishState structure
 * @return true if processing was successful, false otherwise
 */
bool aish_process_bash_output(AishState *state);

/**
 * @brief Handle signals
 * 
 * @param signal Signal number
 */
void aish_signal_handler(int signal);

/**
 * @brief Clean up AISH state and free resources
 * 
 * @param state Pointer to AishState structure
 */
void aish_cleanup(AishState *state);

#endif /* AISH_H */
