enum block_state {
    BLOCK_NONE,
    BLOCK_COMMAND_START,
    BLOCK_COMMAND_RUNNING, 
    BLOCK_COMMAND_COMPLETE
};

struct command_block {
    int start_row;          // Starting row of the block
    int end_row;            // Ending row (-1 if ongoing)
    int prompt_end_col;     // Column where prompt ends and command begins
    std::string command;    // The actual command text
    std::string prompt;     // The prompt text
    block_state state;      // Current state of the block
    bool folded;           // Whether block output is folded
    time_t timestamp;      // When command was executed
    int exit_code;         // Command exit code (-1 if unknown)
    
    command_block() : start_row(-1), end_row(-1), prompt_end_col(0), 
                     state(BLOCK_NONE), folded(false), 
                     timestamp(0), exit_code(-1) {}
};

class BlockManager {
private:
    std::vector<command_block> blocks;
    int current_block_id;
    bool in_command;
    std::string current_line_buffer;
    
public:
    BlockManager() : current_block_id(-1), in_command(false) {}
    
    // Core block management
    void start_new_block(int row, const std::string& prompt);
    void set_prompt_end_position(int col);
    void set_command_text(const std::string& command);
    void mark_command_execution_start();
    void end_current_block(int row, int exit_code = -1);
    void fold_block(int block_id, bool fold);
    
    // Query methods
    const command_block* get_block_at_row(int row) const;
    const command_block* get_current_block() const;
    bool is_row_in_folded_block(int row) const;
    std::vector<int> get_visible_rows() const;
    
    // Search within blocks
    std::vector<int> search_in_blocks(const std::string& pattern, 
                                    int block_id = -1) const;
}