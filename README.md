# AISH (AI Shell)

## 1. Overview
AISH (AI Shell) is a lightweight C-based shell wrapper that integrates with the OpenAI API to assist users in constructing and executing shell commands. It transparently passes input/output to an embedded Bash shell but intercepts the **Tab key** at the start of a line to toggle between:

- **Bash Mode** – behaves like a standard Bash shell.
- **Chat Mode** – sends user input to OpenAI's API (GPT-4-turbo) in **structured output mode**, returning a validated Bash command for execution.

## 2. Features

- **Seamless Integration**: Works as a wrapper around Bash, providing all standard shell functionality.
- **Mode Switching**: Press Tab at the start of a line to toggle between Bash and Chat modes.
- **Natural Language Commands**: In Chat Mode, describe what you want to do in plain English.
- **Command Validation**: AI-generated commands are validated before execution for safety.
- **Configurable**: Customize API settings through a simple configuration file.

## 3. Requirements

- Linux or macOS with `bash` installed
- `libcurl` for API communication
- `json-c` for JSON parsing
- OpenAI API key

**Important Note**: Before building, ensure that `json-c` and `libcurl` are properly installed on your system. The build process assumes these libraries are available in standard locations or in `/usr/local/include` and `/opt/homebrew/include`.

## 4. Installation

### Dependencies

#### On Debian/Ubuntu:
```bash
sudo apt-get update
sudo apt-get install build-essential libcurl4-openssl-dev libjson-c-dev
```

#### On macOS (using Homebrew):
```bash
brew install curl json-c
```

If you encounter build errors related to missing headers, you may need to modify the Makefile to point to the correct include and library paths for your system. Look for the `CFLAGS` and `LDFLAGS` variables in the Makefile.

### Building from Source

1. Clone the repository:
```bash
git clone https://github.com/yourusername/aish.git
cd aish
```

2. Build the project:
```bash
make
```

3. (Optional) Install system-wide:
```bash
sudo make install
```

## 5. Configuration

Create a configuration file at `~/.aish` with the following format:

```json
{
    "openai_api_key": "sk-...",
    "openai_model": "gpt-4-turbo",
    "temperature": 0.2,
    "max_tokens": 100
}
```

## 6. Usage

1. Start AISH:
```bash
aish
```

2. Use it like a normal Bash shell.

3. To switch to Chat Mode, press Tab at the start of a line.

4. In Chat Mode, type your request in natural language:
```
Show me all files modified in the last 5 days
```

5. AISH will convert your request to a Bash command and execute it:
```
[AISH: Generated command] find . -type f -mtime -5
```

6. Press Tab again to switch back to Bash Mode.

## 7. Development

### Project Structure

- `src/aish.c` - Main program loop and process management
- `src/config.c` - Configuration handling
- `src/terminal.c` - Terminal input handling
- `src/api.c` - OpenAI API integration

### Building for Development

```bash
make
```

### Cleaning Build Files

```bash
make clean
```

## 8. License

[MIT License](LICENSE)

## 9. Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
