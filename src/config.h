/**
 * @file config.h
 * @brief Configuration handling for AISH (AI Shell)
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

/**
 * @struct Config
 * @brief Structure to hold AISH configuration settings
 */
typedef struct {
    char *openai_api_key;    /**< OpenAI API key */
    char *openai_model;      /**< OpenAI model to use (e.g., "gpt-4-turbo") */
    double temperature;      /**< Temperature parameter for API requests */
    int max_tokens;          /**< Maximum tokens for API responses */
} Config;

/**
 * @brief Initialize configuration with default values
 * 
 * @param config Pointer to Config structure to initialize
 * @return true if initialization was successful, false otherwise
 */
bool config_init(Config *config);

/**
 * @brief Load configuration from file (~/.aish)
 * 
 * @param config Pointer to Config structure to populate
 * @return true if loading was successful, false otherwise
 */
bool config_load(Config *config);

/**
 * @brief Free resources allocated for configuration
 * 
 * @param config Pointer to Config structure to free
 */
void config_free(Config *config);

#endif /* CONFIG_H */
