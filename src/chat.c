/**
 * @file chat.c
 * @brief Chat input processing implementation for AISH (AI Shell)
 */

#include "chat.h"
#include "aish.h"
#include "api.h"
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/**
 * @brief Process input in Chat mode
 * 
 * @param state The AISH state
 * @param input The input string
 * @param input_len The length of the input string
 * @return true if successful, false otherwise
 */
bool process_chat_input(AishState *state, const char *input, size_t input_len) {
    if (state == NULL || input == NULL || input_len == 0) {
        return false;
    }
    
    // Process input as a natural language query
    ApiResponse response;
    
    // Send request to OpenAI API
    if (!api_send_request(input, &state->config, &response)) {
        fprintf(stderr, "Error: Failed to send API request\n");
        if (response.error != NULL) {
            fprintf(stderr, "API Error: %s\n", response.error);
        }
        api_free_response(&response);
        return false;
    }
    
    // Check if we got a valid command
    if (!response.is_valid || response.command == NULL) {
        fprintf(stderr, "Error: Invalid command received from API\n");
        api_free_response(&response);
        return false;
    }
    
    // Display the command with proper formatting
    const char *cmd_prefix = "\r\n[AISH: Generated command] ";
    //write(STDERR_FILENO, cmd_prefix, strlen(cmd_prefix));
    //write(STDERR_FILENO, response.command, strlen(response.command));
    const char *cmd_suffix = "\r\n";
    //write(STDERR_FILENO, cmd_suffix, strlen(cmd_suffix));
    
    // Write the command to the bash process
    if (write(state->bash_master_fd, response.command, strlen(response.command)) == -1) {
        fprintf(stderr, "Error: Failed to write command to bash: %s\n", strerror(errno));
        api_free_response(&response);
        return false;
    }
    
    // Write a newline to execute the command
    if (write(state->bash_master_fd, "\n", 1) == -1) {
        fprintf(stderr, "Error: Failed to write newline to bash: %s\n", strerror(errno));
        api_free_response(&response);
        return false;
    }
    
    // Clean up
    api_free_response(&response);
    
    // Switch back to Bash mode
    // Note: We'll set the mode directly instead of calling terminal_toggle_mode
    state->terminal.current_mode = MODE_BASH;
    
    return true;
}
