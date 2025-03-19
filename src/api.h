/**
 * @file api.h
 * @brief OpenAI API integration for AISH (AI Shell)
 */

#ifndef API_H
#define API_H

#include "config.h"
#include <stdbool.h>

/**
 * @struct ApiResponse
 * @brief Structure to hold API response data
 */
typedef struct {
    char *command;      /**< Extracted command from API response */
    bool is_valid;      /**< Flag indicating if the command is valid */
    char *error;        /**< Error message if any */
} ApiResponse;

/**
 * @brief Initialize API module
 * 
 * @param config Pointer to Config structure with API settings
 * @return true if initialization was successful, false otherwise
 */
bool api_init(const Config *config);

/**
 * @brief Send user input to OpenAI API and get command response
 * 
 * @param user_input The user's natural language input
 * @param config Pointer to Config structure with API settings
 * @param response Pointer to ApiResponse structure to populate
 * @return true if API request was successful, false otherwise
 */
bool api_send_request(const char *user_input, const Config *config, ApiResponse *response);

/**
 * @brief Validate a command before execution
 * 
 * @param command The command to validate
 * @return true if the command is safe to execute, false otherwise
 */
bool api_validate_command(const char *command);

/**
 * @brief Free resources allocated for API response
 * 
 * @param response Pointer to ApiResponse structure to free
 */
void api_free_response(ApiResponse *response);

/**
 * @brief Clean up API module and free resources
 */
void api_cleanup(void);

#endif /* API_H */
