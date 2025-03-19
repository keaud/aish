/**
 * @file config.c
 * @brief Implementation of configuration handling for AISH
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

/* 
 * Include json-c header
 * On some systems, this might be in a different location.
 * If you get a compilation error, try changing this to match your system.
 */
#include <json-c/json.h>

#define CONFIG_FILE_NAME ".aish"
#define DEFAULT_MODEL "gpt-4-turbo"
#define DEFAULT_TEMPERATURE 0.2
#define DEFAULT_MAX_TOKENS 100

/**
 * @brief Get the path to the configuration file
 * 
 * @return Dynamically allocated string with the path (must be freed by caller)
 */
static char *get_config_path(void) {
    const char *home_dir = NULL;
    
    // Try to get home directory from environment variable
    home_dir = getenv("HOME");
    
    // If that fails, try to get it from password database
    if (home_dir == NULL || *home_dir == '\0') {
        struct passwd *pw = getpwuid(getuid());
        if (pw != NULL) {
            home_dir = pw->pw_dir;
        }
    }
    
    // If we still don't have a home directory, return NULL
    if (home_dir == NULL || *home_dir == '\0') {
        fprintf(stderr, "Error: Could not determine home directory\n");
        return NULL;
    }
    
    // Allocate memory for the full path (+2 for '/' and null terminator)
    size_t path_len = strlen(home_dir) + strlen(CONFIG_FILE_NAME) + 2;
    char *config_path = (char *)malloc(path_len);
    if (config_path == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }
    
    // Construct the full path
    snprintf(config_path, path_len, "%s/%s", home_dir, CONFIG_FILE_NAME);
    
    return config_path;
}

bool config_init(Config *config) {
    if (config == NULL) {
        return false;
    }
    
    // Initialize with NULL/default values
    config->openai_api_key = NULL;
    config->openai_model = strdup(DEFAULT_MODEL);
    config->temperature = DEFAULT_TEMPERATURE;
    config->max_tokens = DEFAULT_MAX_TOKENS;
    
    // Check if memory allocation for model name succeeded
    if (config->openai_model == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for model name\n");
        return false;
    }
    
    return true;
}

bool config_load(Config *config) {
    if (config == NULL) {
        return false;
    }
    
    char *config_path = get_config_path();
    if (config_path == NULL) {
        return false;
    }
    
    // Check if config file exists
    if (access(config_path, F_OK) != 0) {
        fprintf(stderr, "Warning: Configuration file %s not found. Using defaults.\n", config_path);
        free(config_path);
        return true; // Not a fatal error, we'll use defaults
    }
    
    // Open the config file
    FILE *file = fopen(config_path, "r");
    if (file == NULL) {
        fprintf(stderr, "Error: Could not open configuration file %s\n", config_path);
        free(config_path);
        return false;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Read the entire file
    char *json_string = (char *)malloc(file_size + 1);
    if (json_string == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for config file content\n");
        fclose(file);
        free(config_path);
        return false;
    }
    
    size_t read_size = fread(json_string, 1, file_size, file);
    json_string[read_size] = '\0';
    fclose(file);
    
    // Parse JSON
    struct json_object *json_obj = json_tokener_parse(json_string);
    free(json_string);
    
    if (json_obj == NULL) {
        fprintf(stderr, "Error: Failed to parse configuration file as JSON\n");
        free(config_path);
        return false;
    }
    
    // Extract API key
    struct json_object *api_key_obj;
    if (json_object_object_get_ex(json_obj, "openai_api_key", &api_key_obj)) {
        const char *api_key = json_object_get_string(api_key_obj);
        if (api_key != NULL) {
            // Free existing key if any
            if (config->openai_api_key != NULL) {
                free(config->openai_api_key);
            }
            config->openai_api_key = strdup(api_key);
            if (config->openai_api_key == NULL) {
                fprintf(stderr, "Error: Memory allocation failed for API key\n");
                json_object_put(json_obj);
                free(config_path);
                return false;
            }
        }
    } else {
        fprintf(stderr, "Error: 'openai_api_key' not found in configuration file\n");
        json_object_put(json_obj);
        free(config_path);
        return false;
    }
    
    // Extract model name (optional)
    struct json_object *model_obj;
    if (json_object_object_get_ex(json_obj, "openai_model", &model_obj)) {
        const char *model = json_object_get_string(model_obj);
        if (model != NULL) {
            // Free existing model if any
            if (config->openai_model != NULL) {
                free(config->openai_model);
            }
            config->openai_model = strdup(model);
            if (config->openai_model == NULL) {
                fprintf(stderr, "Error: Memory allocation failed for model name\n");
                json_object_put(json_obj);
                free(config_path);
                return false;
            }
        }
    }
    
    // Extract temperature (optional)
    struct json_object *temp_obj;
    if (json_object_object_get_ex(json_obj, "temperature", &temp_obj)) {
        config->temperature = json_object_get_double(temp_obj);
    }
    
    // Extract max tokens (optional)
    struct json_object *tokens_obj;
    if (json_object_object_get_ex(json_obj, "max_tokens", &tokens_obj)) {
        config->max_tokens = json_object_get_int(tokens_obj);
    }
    
    // Clean up
    json_object_put(json_obj);
    free(config_path);
    
    return true;
}

void config_free(Config *config) {
    if (config == NULL) {
        return;
    }
    
    // Free dynamically allocated strings
    if (config->openai_api_key != NULL) {
        free(config->openai_api_key);
        config->openai_api_key = NULL;
    }
    
    if (config->openai_model != NULL) {
        free(config->openai_model);
        config->openai_model = NULL;
    }
}
