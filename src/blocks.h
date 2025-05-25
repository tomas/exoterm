#ifndef BLOCKS_H_
#define BLOCKS_H_

#include <vector>
#include <string>
#include <regex>
#include <ctime>

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
    // std::string command;    // The actual command text
    // std::string prompt;     // The prompt text
    const char * command;    // The actual command text
    const char * prompt;     // The prompt text
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
    int current_block_id;
    bool in_command;
    // std::string current_line_buffer;
    const char * current_line_buffer;
    time_t last_osc133_time;  // Track when we last received OSC 133

public:
    vector<command_block> blocks;

    BlockManager() : current_block_id(-1), in_command(false), last_osc133_time(0) {}

    // Core block management
    void start_new_block(int row, const char* prompt);
    void set_prompt_end_position(int col);
    void set_command_text(const char* command);
    void mark_command_execution_start();
    void end_current_block(int row, int exit_code = -1);
    void fold_block(int block_id, bool fold);

    // Query methods
    const command_block* get_block_at_row(int row) const;
    const command_block* get_current_block() const;
    bool is_row_in_folded_block(int row) const;
    vector<int> get_visible_rows() const;

    // State queries for enhanced input integration
    bool is_command_running() const;
    bool is_at_prompt() const;
    bool is_showing_output() const;
    bool has_shell_integration() const;
    block_state get_current_state() const;
    bool has_active_input() const;
    const command_block* get_input_block() const;

    // Search within blocks
    vector<int> search_in_blocks(const char * pattern,
                                    int block_id = -1) const;

private:
    void handle_osc133_sequence();
};

#endif // BLOCKS_H_