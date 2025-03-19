/**
 * @file aish.c
 * @brief Main implementation for AISH (AI Shell)
 */

#include "aish.h"
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
    
    return true;
}

bool aish_process_input(AishState *state, const char *input, size_t input_len) {
    if (state == NULL || input == NULL || input_len == 0) {
        return false;
    }
    
    // Check if we're in Chat mode
    if (terminal_get_mode(&state->terminal) == MODE_CHAT) {
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
        
        // Display the command
        fprintf(stderr, "\n[AISH: Generated command] %s\n", response.command);
        
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
        terminal_toggle_mode(&state->terminal);
        
    } else {
        // In Bash mode, just forward the input to bash
        if (write(state->bash_master_fd, input, input_len) == -1) {
            fprintf(stderr, "Error: Failed to write input to bash: %s\n", strerror(errno));
            return false;
        }
    }
    
    return true;
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
    bool at_start_of_line = true;
    
    fprintf(stderr, "AISH - AI Shell v0.1\n");
    fprintf(stderr, "Press Tab at the start of a line to toggle between Bash and Chat modes.\n");
    
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
                if (terminal_process_key(&state->terminal, c, at_start_of_line)) {
                    // Mode was toggled, reset input buffer
                    input_pos = 0;
                    at_start_of_line = true;
                    continue;
                }
                
                // Handle special keys
                if (c == '\r' || c == '\n') {
                    // Enter key - process the input
                    input_buffer[input_pos] = '\0';
                    aish_process_input(state, input_buffer, input_pos);
                    input_pos = 0;
                    at_start_of_line = true;
                } else if (c == 127 || c == '\b') {
                    // Backspace - delete the last character
                    if (input_pos > 0) {
                        input_pos--;
                        // Echo the backspace
                        if (write(state->bash_master_fd, "\b \b", 3) == -1) {
                            fprintf(stderr, "Error: Failed to write backspace to bash: %s\n", strerror(errno));
                        }
                    }
                } else {
                    // Regular character - add to buffer
                    if (input_pos < INPUT_BUFFER_SIZE - 1) {
                        input_buffer[input_pos++] = c;
                        at_start_of_line = false;
                        
                        // Echo the character
                        if (write(STDOUT_FILENO, &c, 1) == -1) {
                            fprintf(stderr, "Error: Failed to echo character: %s\n", strerror(errno));
                        }
                    }
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
