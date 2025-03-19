/**
 * @file api.c
 * @brief Implementation of OpenAI API integration for AISH
 */

#include "api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>


/* 
 * Include json-c header
 * On some systems, this might be in a different location.
 * If you get a compilation error, try changing this to match your system.
 */
#include <json-c/json.h>

#define OPENAI_API_URL "https://api.openai.com/v1/chat/completions"
#define USER_AGENT "AISH/0.1"
#define MAX_RESPONSE_SIZE (1024 * 1024) // 1MB max response size

// Static variables
static CURL *curl_handle = NULL;
static struct curl_slist *headers = NULL;

// Structure to hold response data
typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} ResponseData;

/**
 * @brief Callback function for libcurl to handle API response data
 */
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t real_size = size * nmemb;
    ResponseData *response = (ResponseData *)userdata;
    
    // Check if we're about to exceed the maximum response size
    if (response->size + real_size > MAX_RESPONSE_SIZE) {
        fprintf(stderr, "Error: Response size exceeds maximum allowed size\n");
        return 0; // This will cause curl to report an error
    }
    
    // Check if we need to resize the buffer
    if (response->size + real_size >= response->capacity) {
        // Calculate new capacity (double the current size)
        size_t new_capacity = response->capacity * 2;
        if (new_capacity < response->size + real_size + 1) {
            new_capacity = response->size + real_size + 1;
        }
        
        // Resize the buffer
        char *new_data = (char *)realloc(response->data, new_capacity);
        if (new_data == NULL) {
            fprintf(stderr, "Error: Memory allocation failed for API response\n");
            return 0; // This will cause curl to report an error
        }
        
        response->data = new_data;
        response->capacity = new_capacity;
    }
    
    // Copy the data to the buffer
    memcpy(response->data + response->size, ptr, real_size);
    response->size += real_size;
    response->data[response->size] = '\0';
    
    return real_size;
}

bool api_init(const Config *config) {
    if (config == NULL || config->openai_api_key == NULL) {
        fprintf(stderr, "Error: Invalid configuration or missing API key\n");
        return false;
    }
    
    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_handle = curl_easy_init();
    if (curl_handle == NULL) {
        fprintf(stderr, "Error: Failed to initialize libcurl\n");
        return false;
    }
    
    // Set up headers
    headers = curl_slist_append(NULL, "Content-Type: application/json");
    
    // Create Authorization header with API key
    char auth_header[1024];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", config->openai_api_key);
    headers = curl_slist_append(headers, auth_header);
    
    return true;
}

bool api_send_request(const char *user_input, const Config *config, ApiResponse *response) {
    if (curl_handle == NULL || user_input == NULL || config == NULL || response == NULL) {
        return false;
    }
    
    // Initialize response structure
    response->command = NULL;
    response->is_valid = false;
    response->error = NULL;
    
    // Create JSON request body
    struct json_object *request_obj = json_object_new_object();
    struct json_object *messages_array = json_object_new_array();
    
    // Add system message
    struct json_object *system_msg = json_object_new_object();
    json_object_object_add(system_msg, "role", json_object_new_string("system"));
    json_object_object_add(system_msg, "content", 
                          json_object_new_string("You are a CLI assistant that translates natural language to valid Bash commands. Always return structured JSON output with a 'command' field containing the bash command. Example: {\"command\": \"ls -la\"}"));
    json_object_array_add(messages_array, system_msg);
    
    // Add user message
    struct json_object *user_msg = json_object_new_object();
    json_object_object_add(user_msg, "role", json_object_new_string("user"));
    json_object_object_add(user_msg, "content", json_object_new_string(user_input));
    json_object_array_add(messages_array, user_msg);
    
    // Add messages array to request
    json_object_object_add(request_obj, "messages", messages_array);
    
    // Add model
    json_object_object_add(request_obj, "model", json_object_new_string(config->openai_model));
    
    // Add temperature
    json_object_object_add(request_obj, "temperature", json_object_new_double(config->temperature));
    
    // Add max_tokens
    json_object_object_add(request_obj, "max_tokens", json_object_new_int(config->max_tokens));
    
    // Add response format
    struct json_object *response_format = json_object_new_object();
    json_object_object_add(response_format, "type", json_object_new_string("json_object"));
    json_object_object_add(request_obj, "response_format", response_format);
    
    // Convert JSON object to string
    const char *request_str = json_object_to_json_string(request_obj);
    
    // Set up curl request
    curl_easy_setopt(curl_handle, CURLOPT_URL, OPENAI_API_URL);
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, request_str);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 30L); // 30 second timeout
    
    // Set up response handling
    ResponseData response_data;
    response_data.data = (char *)malloc(4096); // Initial 4KB buffer
    if (response_data.data == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for API response\n");
        json_object_put(request_obj);
        return false;
    }
    
    response_data.size = 0;
    response_data.capacity = 4096;
    response_data.data[0] = '\0';
    
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &response_data);
    
    // Perform the request
    CURLcode res = curl_easy_perform(curl_handle);
    
    // Clean up request object
    json_object_put(request_obj);
    
    // Check for errors
    if (res != CURLE_OK) {
        fprintf(stderr, "Error: API request failed: %s\n", curl_easy_strerror(res));
        response->error = strdup(curl_easy_strerror(res));
        free(response_data.data);
        return false;
    }
    
    // Get HTTP response code
    long http_code = 0;
    curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
    
    // Check HTTP response code
    if (http_code != 200) {
        fprintf(stderr, "Error: API returned HTTP code %ld\n", http_code);
        response->error = (char *)malloc(100);
        if (response->error != NULL) {
            snprintf(response->error, 100, "HTTP error %ld", http_code);
        }
        free(response_data.data);
        return false;
    }
    
    // Parse JSON response
    struct json_object *json_response = json_tokener_parse(response_data.data);
    if (json_response == NULL) {
        fprintf(stderr, "Error: Failed to parse API response as JSON\n");
        response->error = strdup("Failed to parse API response");
        free(response_data.data);
        return false;
    }
    
    // Extract command from response
    struct json_object *choices_array;
    if (!json_object_object_get_ex(json_response, "choices", &choices_array) ||
        json_object_get_type(choices_array) != json_type_array ||
        json_object_array_length(choices_array) == 0) {
        
        fprintf(stderr, "Error: Invalid API response format (missing choices array)\n");
        response->error = strdup("Invalid API response format");
        json_object_put(json_response);
        free(response_data.data);
        return false;
    }
    
    struct json_object *first_choice = json_object_array_get_idx(choices_array, 0);
    struct json_object *message_obj;
    if (!json_object_object_get_ex(first_choice, "message", &message_obj)) {
        fprintf(stderr, "Error: Invalid API response format (missing message)\n");
        response->error = strdup("Invalid API response format");
        json_object_put(json_response);
        free(response_data.data);
        return false;
    }
    
    struct json_object *content_obj;
    if (!json_object_object_get_ex(message_obj, "content", &content_obj)) {
        fprintf(stderr, "Error: Invalid API response format (missing content)\n");
        response->error = strdup("Invalid API response format");
        json_object_put(json_response);
        free(response_data.data);
        return false;
    }
    
    const char *content_str = json_object_get_string(content_obj);
    
    // Debug: Print the content string to see what the API is returning
    // fprintf(stderr, "API Response Content: %s\n", content_str);
    
    // Parse the content as JSON to extract the command
    struct json_object *command_json = json_tokener_parse(content_str);
    if (command_json != NULL) {
        // Content is valid JSON
        struct json_object *command_obj;
        if (json_object_object_get_ex(command_json, "command", &command_obj)) {
            // If the command field exists, use it
            const char *command_str = json_object_get_string(command_obj);
            response->command = strdup(command_str);
        } else {
            // If the command field doesn't exist, use the content string directly
            fprintf(stderr, "Warning: Command field not found in API response JSON, using content directly\n");
            response->command = strdup(content_str);
        }
        json_object_put(command_json);
    } else {
        // Content is not valid JSON, try to extract a command from it directly
        fprintf(stderr, "Warning: API response is not valid JSON, attempting to extract command\n");
        
        // For now, just use the content string directly
        response->command = strdup(content_str);
        
        // TODO: Implement more sophisticated command extraction
        // For example, look for patterns like "The command is: ls -la"
    }
    
    // Validate the command
    response->is_valid = api_validate_command(response->command);
    
    // Clean up
    json_object_put(json_response);
    free(response_data.data);
    
    return true;
}

bool api_validate_command(const char *command) {
    if (command == NULL) {
        return false;
    }
    
    // Basic validation: non-empty command
    if (strlen(command) == 0) {
        return false;
    }
    
    // TODO: Implement more sophisticated command validation
    // For example, check for dangerous commands like "rm -rf /"
    
    // For now, just do a simple check for "rm -rf /" or similar
    if (strstr(command, "rm -rf /") != NULL) {
        fprintf(stderr, "Warning: Potentially dangerous command detected: %s\n", command);
        return false;
    }
    
    return true;
}

void api_free_response(ApiResponse *response) {
    if (response == NULL) {
        return;
    }
    
    if (response->command != NULL) {
        free(response->command);
        response->command = NULL;
    }
    
    if (response->error != NULL) {
        free(response->error);
        response->error = NULL;
    }
    
    response->is_valid = false;
}

void api_cleanup(void) {
    // Clean up curl resources
    if (headers != NULL) {
        curl_slist_free_all(headers);
        headers = NULL;
    }
    
    if (curl_handle != NULL) {
        curl_easy_cleanup(curl_handle);
        curl_handle = NULL;
    }
    
    curl_global_cleanup();
}
