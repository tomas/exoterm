// Add to command.C or create blocks.C

// In rxvt_term class, add block manager
BlockManager block_manager;

// Add this method to handle OSC 133 sequences (similar to how OSC 0/1/2 are handled)
void rxvt_term::process_block_sequence(const char *str) {
    if (!str || !*str) return;
    
    char cmd = str[0];
    
    switch (cmd) {
        case 'A': // Prompt start
            {
                int current_row = screen.cur.row + screen.bscroll;
                std::string prompt_line = extract_current_line();
                block_manager.start_new_block(current_row, prompt_line);
                break;
            }
            
        case 'B': // Prompt end (command input start)
            {
                // Mark where prompt ends and command input begins
                const command_block* current = block_manager.get_current_block();
                if (current) {
                    block_manager.set_prompt_end_position(screen.cur.col);
                }
                break;
            }
            
        case 'C': // Command execution start (end of input)
            {
                std::string command = extract_command_from_line();
                block_manager.set_command_text(command);
                block_manager.mark_command_execution_start();
                break;
            }
            
        case 'D': // Command completed
            {
                int exit_code = -1;
                if (str[1] == ';') {
                    exit_code = atoi(str + 2);
                }
                int current_row = screen.cur.row + screen.bscroll;
                block_manager.end_current_block(current_row, exit_code);
                
                // Trigger screen refresh to show block boundaries
                scr_refresh();
                break;
            }
    }
}
// Helper methods
std::string rxvt_term::extract_current_line() {
    text_t *line = ROW(screen.cur.row).t;
    rend_t *rend = ROW(screen.cur.row).r;
    int ncol = ROW(screen.cur.row).l;
    
    std::string result;
    for (int col = 0; col < ncol; col++) {
        if (line[col] != NOCHAR) {
            result += (char)line[col];
        }
    }
    return result;
}

std::string rxvt_term::extract_command_from_line() {
    const command_block* current = block_manager.get_current_block();
    if (!current) return "";
    
    std::string line = extract_current_line();
    
    // Remove the prompt part
    if (line.length() > current->prompt.length()) {
        return line.substr(current->prompt.length());
    }
    return "";
}


/// shell integration

void rxvt_term::setup_shell_integration() {
    // Simply set TERM to indicate block support capability
    setenv("TERM", "exoterm", 1);
    
    // Optionally create a simple integration script
    const char* home = getenv("HOME");
    if (home) {
        std::string integration_path = std::string(home) + "/.exoterm_integration";
        FILE* f = fopen(integration_path.c_str(), "w");
        if (f) {
            fprintf(f, "%s", get_integration_script_content());
            fclose(f);
            
            // Inform user about the integration script
            printf("\nExoterm shell integration available at: %s\n", integration_path.c_str());
            printf("Source it in your ~/.bashrc or ~/.zshrc with: . %s\n\n", integration_path.c_str());
        }
    }
}

const char* rxvt_term::get_integration_script_content() {
    return R"(
# Exoterm shell integration
if [[ "$TERM" == "exoterm"* ]]; then
    EXOTERM_BLOCK_START='\e]133;A\a'
    EXOTERM_BLOCK_COMMAND='\e]133;B\a' 
    EXOTERM_BLOCK_END='\e]133;C;%s\a'
    
    exoterm_preexec() {
        printf "${EXOTERM_BLOCK_COMMAND}"
    }
    
    exoterm_precmd() {
        local exit_code=$?
        printf "${EXOTERM_BLOCK_END}" "$exit_code"
        printf "${EXOTERM_BLOCK_START}"
    }
    
    if [[ -n "$BASH_VERSION" ]]; then
        trap 'exoterm_preexec' DEBUG
        PROMPT_COMMAND="exoterm_precmd${PROMPT_COMMAND:+;$PROMPT_COMMAND}"
        printf "${EXOTERM_BLOCK_START}"
    elif [[ -n "$ZSH_VERSION" ]]; then
        autoload -U add-zsh-hook
        add-zsh-hook preexec exoterm_preexec  
        add-zsh-hook precmd exoterm_precmd
        printf "${EXOTERM_BLOCK_START}"
    fi
fi
)";
}

// Add to .Xdefaults documentation example
/*
! Block support configuration
URxvt.blockSupport: true
URxvt.blockAutoFold: false
URxvt.blockIndicatorColor: #666666
URxvt.blockPromptDetection: auto
