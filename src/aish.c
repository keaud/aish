/**
 * @file aish.c
 * @brief Main implementation for AISH (AI Shell)
 */

#include "aish.h"
#include "prompt.h"
#include "chat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <termios.h>

#if defined(__linux__)
#include <pty.h>
#include <utmp.h>
#elif defined(__APPLE__)
#include <util.h>
#endif

#define BUFFER_SIZE 4096
#define INPUT_BUFFER_SIZE 1024

// Global state for signal handling
static AishState *g_state = NULL;

// Signal handler
void aish_signal_handler(int signal) {
    (void)signal; // Suppress unused parameter warning
    if (g_state != NULL) {
        g_state->running = false;
    }
}

bool aish_init(AishState *state) {
    if (state == NULL) {
        return false;
    }
    
    // Initialize state
    memset(state, 0, sizeof(AishState));
    state->bash_master_fd = -1;
    state->bash_pid = -1;
    state->running = false;
    
    // Initialize configuration
    if (!config_init(&state->config)) {
        fprintf(stderr, "Error: Failed to initialize configuration\n");
        return false;
    }
    
    // Load configuration
    if (!config_load(&state->config)) {
        fprintf(stderr, "Error: Failed to load configuration\n");
        config_free(&state->config);
        return false;
    }
    
    // Check if API key is available
    if (state->config.openai_api_key == NULL) {
        fprintf(stderr, "Error: OpenAI API key not found in configuration\n");
        config_free(&state->config);
        return false;
    }
    
    // Initialize terminal state
    if (!terminal_init(&state->terminal)) {
        fprintf(stderr, "Error: Failed to initialize terminal\n");
        config_free(&state->config);
        return false;
    }
    
    // Initialize API
    if (!api_init(&state->config)) {
        fprintf(stderr, "Error: Failed to initialize API\n");
        terminal_cleanup(&state->terminal);
        config_free(&state->config);
        return false;
    }
    
    // Set up signal handling
    g_state = state;
    signal(SIGINT, aish_signal_handler);
    signal(SIGTERM, aish_signal_handler);
    
    return true;
}

bool aish_spawn_bash(AishState *state) {
    if (state == NULL) {
        return false;
    }
    
    // Get the current window size
    struct winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) {
        fprintf(stderr, "Error: Failed to get window size: %s\n", strerror(errno));
        // Use default values if we can't get the window size
        ws.ws_row = 24;
        ws.ws_col = 80;
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;
    }
    
    // Fork a new process with a pseudo-terminal
    state->bash_pid = forkpty(&state->bash_master_fd, NULL, NULL, &ws);
    
    if (state->bash_pid == -1) {
        fprintf(stderr, "Error: Failed to fork pty: %s\n", strerror(errno));
        return false;
    }
    
    if (state->bash_pid == 0) {
        // Child process
        
        // Set environment variables
        setenv("TERM", "xterm-256color", 1);
        
        // Execute bash
        char *args[] = { "bash", "--login", NULL };
        execvp("bash", args);
        
        // If execvp returns, it failed
        fprintf(stderr, "Error: Failed to execute bash: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Parent process
    
    // Set the master fd to non-blocking mode
    int flags = fcntl(state->bash_master_fd, F_GETFL, 0);
    if (flags == -1) {
        fprintf(stderr, "Error: Failed to get file flags: %s\n", strerror(errno));
        return false;
    }
    
    if (fcntl(state->bash_master_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        fprintf(stderr, "Error: Failed to set non-blocking mode: %s\n", strerror(errno));
        return false;
    }
    
    // Set the initial prompt
    terminal_update_prompt(&state->terminal, state->bash_master_fd);
    
    return true;
}

/**
 * @brief Process input in Bash mode
 * 
 * @param state The AISH state
 * @param input The input string
 * @param input_len The length of the input string
 * @return true if successful, false otherwise
 */
bool process_bash_input(AishState *state, const char *input, size_t input_len) {
    if (state == NULL || input == NULL || input_len == 0) {
        return false;
    }
    
    // In Bash mode, forward the input to bash
    if (write(state->bash_master_fd, input, input_len) == -1) {
        fprintf(stderr, "Error: Failed to write input to bash: %s\n", strerror(errno));
        return false;
    }
    
    // Write a newline to execute the command
    if (write(state->bash_master_fd, "\n", 1) == -1) {
        fprintf(stderr, "Error: Failed to write newline to bash: %s\n", strerror(errno));
        return false;
    }
    
    return true;
}

/**
 * @brief Process a single character of input in Chat mode
 * 
 * @param state The AISH state
 * @param c The character to process
 * @param input_buffer The input buffer
 * @param input_pos Pointer to the current position in the input buffer
 * @return true if successful, false otherwise
 */
bool process_chat_keypress(AishState *state, char c, char *input_buffer, size_t *input_pos) {
    if (state == NULL || input_buffer == NULL || input_pos == NULL) {
        return false;
    }
    
    if (c == '\r' || c == '\n') {
        // Enter key - process the input
        input_buffer[*input_pos] = '\0';
        
        // Echo a newline
        const char *newline = "\r\n";
        if (write(STDOUT_FILENO, newline, strlen(newline)) == -1) {
            fprintf(stderr, "Error: Failed to write newline: %s\n", strerror(errno));
            return false;
        }
        
        // Process the input (send to OpenAI API)
        process_chat_input(state, input_buffer, *input_pos);
        *input_pos = 0;
        
        // Display the prompt
        display_prompt(state);
    } else if (c == 127 || c == '\b') {
        // Backspace - delete the last character
        if (*input_pos > 0) {
            (*input_pos)--;
            // Echo the backspace
            const char *backspace = "\b \b";
            if (write(STDOUT_FILENO, backspace, 3) == -1) {
                fprintf(stderr, "Error: Failed to write backspace: %s\n", strerror(errno));
                return false;
            }
        }
    } else {
        // Regular character - add to buffer
        if (*input_pos < INPUT_BUFFER_SIZE - 1) {
            input_buffer[(*input_pos)++] = c;
            
            // Echo the character
            if (write(STDOUT_FILENO, &c, 1) == -1) {
                fprintf(stderr, "Error: Failed to echo character: %s\n", strerror(errno));
                return false;
            }
        }
    }
    
    return true;
}

/**
 * @brief Process a single character of input in Bash mode
 * 
 * @param state The AISH state
 * @param c The character to process
 * @param input_buffer The input buffer
 * @param input_pos Pointer to the current position in the input buffer
 * @return true if successful, false otherwise
 */
bool process_bash_keypress(AishState *state, char c, char *input_buffer, size_t *input_pos) {
    if (state == NULL || input_buffer == NULL || input_pos == NULL) {
        return false;
    }
    
    // In Bash mode, forward all keypresses directly to bash
    // (except Tab at start of line, which was handled above)
    if (write(state->bash_master_fd, &c, 1) == -1) {
        fprintf(stderr, "Error: Failed to write character to bash: %s\n", strerror(errno));
        return false;
    }
    
    // Update input buffer for Tab key detection
    if (c == '\r' || c == '\n') {
        *input_pos = 0;
    } else if (c == 127 || c == '\b') {
        // Backspace - update input buffer position
        if (*input_pos > 0) {
            (*input_pos)--;
        }
    } else {
        if (*input_pos < INPUT_BUFFER_SIZE - 1) {
            input_buffer[(*input_pos)++] = c;
        }
    }
    
    return true;
}

bool aish_process_input(AishState *state, const char *input, size_t input_len) {
    if (state == NULL || input == NULL || input_len == 0) {
        return false;
    }
    
    // Check if we're in Chat mode
    if (terminal_get_mode(&state->terminal) == MODE_CHAT) {
        return process_chat_input(state, input, input_len);
    } else {
        return process_bash_input(state, input, input_len);
    }
}

bool aish_process_bash_output(AishState *state) {
    if (state == NULL || state->bash_master_fd == -1) {
        return false;
    }
    
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    
    // Read output from bash
    bytes_read = read(state->bash_master_fd, buffer, BUFFER_SIZE - 1);
    
    if (bytes_read > 0) {
        // Write output to stdout
        buffer[bytes_read] = '\0';
        if (write(STDOUT_FILENO, buffer, bytes_read) == -1) {
            fprintf(stderr, "Error: Failed to write bash output to stdout: %s\n", strerror(errno));
            return false;
        }
    } else if (bytes_read == -1 && errno != EAGAIN) {
        // Error reading from bash
        fprintf(stderr, "Error: Failed to read from bash: %s\n", strerror(errno));
        return false;
    } else if (bytes_read == 0) {
        // EOF - bash has exited
        fprintf(stderr, "Bash process has exited\n");
        state->running = false;
    }
    
    return true;
}

int aish_run(AishState *state) {
    if (state == NULL) {
        return EXIT_FAILURE;
    }
    
    // Enable raw mode for terminal input
    if (!terminal_enable_raw_mode(&state->terminal)) {
        fprintf(stderr, "Error: Failed to enable raw mode\n");
        return EXIT_FAILURE;
    }
    
    // Set running flag
    state->running = true;
    
    // Main loop
    char input_buffer[INPUT_BUFFER_SIZE];
    size_t input_pos = 0;
    
    // Use write instead of fprintf to ensure proper formatting
    const char *welcome_msg = "AISH - AI Shell v0.1\r\n";
    write(STDERR_FILENO, welcome_msg, strlen(welcome_msg));
    const char *help_msg = "Press Tab when the input is empty to toggle between Bash and Chat modes.\r\n";
    write(STDERR_FILENO, help_msg, strlen(help_msg));
    
    while (state->running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(state->bash_master_fd, &read_fds);
        
        int max_fd = (STDIN_FILENO > state->bash_master_fd) ? STDIN_FILENO : state->bash_master_fd;
        
        // Wait for input or output
        int ready = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        
        if (ready == -1) {
            if (errno == EINTR) {
                // Interrupted by signal, check if we should exit
                if (!state->running) {
                    break;
                }
                continue;
            }
            
            fprintf(stderr, "Error: select() failed: %s\n", strerror(errno));
            break;
        }
        
        // Check for input from user
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            char c;
            ssize_t bytes_read = read(STDIN_FILENO, &c, 1);
            
            if (bytes_read > 0) {
                // Process the keypress
                
                if (terminal_process_key(&state->terminal, c, input_pos)) {
                    // Mode was toggled, reset input buffer
                    input_pos = 0;
                    
                    // Note: terminal_process_key already toggled the mode, so we don't need to call terminal_toggle_mode
                    // terminal_toggle_mode(&state->terminal, state->bash_master_fd);
                    
                    // Display the appropriate prompt based on the current mode
                    display_prompt(state);
                    
                    continue;
                }
                
                // Process the keypress based on the current mode
                if (terminal_get_mode(&state->terminal) == MODE_BASH) {
                    // Process the keypress in Bash mode
                    process_bash_keypress(state, c, input_buffer, &input_pos);
                } else {
                    // Process the keypress in Chat mode
                    process_chat_keypress(state, c, input_buffer, &input_pos);
                }
            } else if (bytes_read == -1 && errno != EAGAIN) {
                fprintf(stderr, "Error: Failed to read from stdin: %s\n", strerror(errno));
                break;
            }
        }
        
        // Check for output from bash
        if (FD_ISSET(state->bash_master_fd, &read_fds)) {
            if (!aish_process_bash_output(state)) {
                break;
            }
        }
        
        // Check if bash process has exited
        int status;
        pid_t result = waitpid(state->bash_pid, &status, WNOHANG);
        
        if (result == state->bash_pid) {
            fprintf(stderr, "Bash process has exited with status %d\n", WEXITSTATUS(status));
            state->running = false;
        } else if (result == -1) {
            fprintf(stderr, "Error: waitpid() failed: %s\n", strerror(errno));
            break;
        }
    }
    
    // Disable raw mode
    terminal_disable_raw_mode(&state->terminal);
    
    return EXIT_SUCCESS;
}

void aish_cleanup(AishState *state) {
    if (state == NULL) {
        return;
    }
    
    // Clean up terminal
    terminal_cleanup(&state->terminal);
    
    // Clean up API
    api_cleanup();
    
    // Clean up configuration
    config_free(&state->config);
    
    // Close bash master fd if it's open
    if (state->bash_master_fd != -1) {
        close(state->bash_master_fd);
        state->bash_master_fd = -1;
    }
    
    // Reset global state
    g_state = NULL;
}

int main(int argc, char *argv[]) {
    (void)argc; // Suppress unused parameter warning
    (void)argv; // Suppress unused parameter warning
    AishState state;
    int exit_code = EXIT_FAILURE;
    
    // Initialize AISH
    if (!aish_init(&state)) {
        fprintf(stderr, "Error: Failed to initialize AISH\n");
        return exit_code;
    }
    
    // Spawn bash process
    if (!aish_spawn_bash(&state)) {
        fprintf(stderr, "Error: Failed to spawn bash process\n");
        aish_cleanup(&state);
        return exit_code;
    }
    
    // Run main loop
    exit_code = aish_run(&state);
    
    // Clean up
    aish_cleanup(&state);
    
    return exit_code;
}
