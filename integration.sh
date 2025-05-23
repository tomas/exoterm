# Add this to user's .bashrc/.zshrc when exoterm starts
# Shell integration for command block detection

# OSC 133 escape sequences for shell integration (standard)
EXOTERM_PROMPT_START='\e]133;A\a'    # Prompt start
EXOTERM_PROMPT_END='\e]133;B\a'      # Prompt end (command input start)
EXOTERM_COMMAND_START='\e]133;C\a'   # Command execution start
EXOTERM_COMMAND_END='\e]133;D;%s\a'  # Command finished with exit code

# For bash
if [[ "$TERM" == "exoterm"* ]]; then
    # Function called before command execution
    exoterm_preexec() {
        printf "${EXOTERM_COMMAND_START}"
    }
    
    # Function called before each prompt
    exoterm_precmd() {
        local exit_code=$?
        printf "${EXOTERM_COMMAND_END}" "$exit_code"
        printf "${EXOTERM_PROMPT_START}"
    }
    
    # Set up hooks
    if [[ -n "$BASH_VERSION" ]]; then
        # Bash setup
        trap 'exoterm_preexec' DEBUG
        PROMPT_COMMAND="exoterm_precmd${PROMPT_COMMAND:+;$PROMPT_COMMAND}"
        
        # Send initial prompt start
        printf "${EXOTERM_PROMPT_START}"
        
    elif [[ -n "$ZSH_VERSION" ]]; then
        # Zsh setup
        autoload -U add-zsh-hook
        add-zsh-hook preexec exoterm_preexec
        add-zsh-hook precmd exoterm_precmd
        
        # Send initial prompt start
        printf "${EXOTERM_PROMPT_START}"
    fi
fi

# Alternative method: PS1 modification for simpler shells
# This injects escape sequences directly into the prompt
if [[ "$TERM" == "exoterm"* && -z "$BASH_VERSION" && -z "$ZSH_VERSION" ]]; then
    # For basic POSIX shells, modify PS1 to include prompt markers
    PS1='\e]133;A\a'"$PS1"'\e]133;B\a'
fi