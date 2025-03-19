# AISH (AI Shell) - Technical Specification

## 1. Overview
AISH (AI Shell) is a lightweight C-based shell wrapper that integrates with the OpenAI API to assist users in constructing and executing shell commands. It transparently passes input/output to an embedded Bash shell but intercepts the **Tab key** at the start of a line to toggle between:

- **Bash Mode** – behaves like a standard Bash shell.
- **Chat Mode** – sends user input to OpenAI’s API (GPT-4-turbo) in **structured output mode**, returning a validated Bash command for execution.

## 2. Requirements

### 2.1. System Requirements
- Linux or macOS with `bash` installed.
- Make, GCC or Clang for compilation.
- `libcurl` for API communication.
- `json-c` (or similar) for JSON parsing.

### 2.2. Configuration File (`~/.aish`)
AISH reads configuration settings from `~/.aish`. Example format:

```json
{
    "openai_api_key": "sk-...",
    "openai_model": "gpt-4-turbo",
    "temperature": 0.2,
    "max_tokens": 100
}
```

## 3. Architecture & Design

### 3.1. Process Model
AISH spawns a **Bash process** via `forkpty()`, acting as an intermediary:
- **Intercepts user input** before passing it to Bash.
- **Detects Tab at the start of a line**, toggling input mode.
- **In Chat Mode**, sends user input to OpenAI API.
- **Validates API response** and executes the suggested command in Bash.

### 3.2. Input Handling
- **Raw terminal mode** is enabled using `termios` to capture keypresses.
- **Tab key (`\t`)** at the start of a line toggles between Bash and Chat modes.
- **Regular input** is forwarded to Bash unless in Chat Mode.

### 3.3. OpenAI API Integration
- **Uses OpenAI’s structured output mode** to ensure responses are well-formatted.
- The request JSON sent to OpenAI includes a structured system prompt:

```json
{
    "model": "gpt-4-turbo",
    "messages": [
        {
            "role": "system",
            "content": "You are a CLI assistant that translates natural language to valid Bash commands. Always return structured JSON output."
        },
        {
            "role": "user",
            "content": "Show all files modified in the last 5 days"
        }
    ],
    "response_format": { "type": "json_object" },
    "temperature": 0.2,
    "max_tokens": 100
}
```

- **Expected OpenAI response format:**
```json
{
    "command": "find . -type f -mtime -5"
}
```
- The extracted `command` is **validated** and executed in Bash.

## 4. Implementation Details

### 4.1. Core Components

| Component  | Description |
|------------|-------------|
| `aish.c`   | Main program loop, handles input/output and process management. |
| `config.c/.h` | Reads and parses `~/.aish` for API keys and settings. |
| `terminal.c/.h` | Manages raw input mode, detects Tab presses, and switches between modes. |
| `api.c/.h` | Handles HTTP requests to OpenAI via `libcurl` and parses JSON responses. |

### 4.2. Bash Process Management
- Uses `forkpty()` to spawn and interact with a Bash shell.
- `select()` or `poll()` monitors both user input and Bash output.
- **Signal handling** ensures clean exit on `SIGINT` (Ctrl+C).

### 4.3. Handling API Calls
- `libcurl` is used for HTTP POST requests to OpenAI.
- JSON responses are parsed with `json-c`.
- Extracted commands are **sanitized** before execution.

## 5. Security Considerations
- **Sanitize API-generated commands** before execution.
- **Disable destructive operations by default** (e.g., `rm -rf` without explicit user confirmation).
- **Limit network calls** to prevent excessive API usage.

## 6. Future Enhancements
- **Local LLM support** (e.g., `llama.cpp`) for offline use.
- **AI-driven command history** to suggest corrections and improvements.
- **Interactive corrections** where users can confirm or refine AI-generated commands before execution.
