#include "../config.h"
#include "rxvt.h"
#include "blocks.h"

void BlockManager::handle_osc133_sequence() {
    last_osc133_time = time(nullptr);
}

void BlockManager::start_new_block(int row, const char * prompt) {
    handle_osc133_sequence();

    command_block new_block;
    new_block.start_row = row;
    new_block.prompt = prompt;
    new_block.state = BLOCK_COMMAND_START;
    new_block.timestamp = time(nullptr);
    blocks.push_back(new_block);
    current_block_id = blocks.size() - 1;
}

void BlockManager::set_prompt_end_position(int col) {
    handle_osc133_sequence();
    if (!blocks.empty()) {
        blocks.back().prompt_end_col = col;
    }
}

void BlockManager::set_command_text(const char * command) {
    if (!blocks.empty()) {
        blocks.back().command = command;
    }
}

void BlockManager::mark_command_execution_start() {
    handle_osc133_sequence();
    if (!blocks.empty()) {
        blocks.back().state = BLOCK_COMMAND_RUNNING;
    }
}

void BlockManager::end_current_block(int row, int exit_code) {
    handle_osc133_sequence();
    if (!blocks.empty()) {
        command_block& current = blocks.back();
        current.end_row = row;
        current.exit_code = exit_code;
        current.state = BLOCK_COMMAND_COMPLETE;
    }
}

void BlockManager::fold_block(int block_id, bool fold) {
    if (block_id >= 0 && block_id < blocks.size()) {
        blocks[block_id].folded = fold;
    }
}

const command_block* BlockManager::get_block_at_row(int row) const {
    for (const auto& block : blocks) {
        if (row >= block.start_row &&
            (block.end_row == -1 || row <= block.end_row)) {
            return &block;
        }
    }
    return nullptr;
}

const command_block* BlockManager::get_current_block() const {
    if (blocks.empty()) return nullptr;
    return &blocks.back();
}

bool BlockManager::is_row_in_folded_block(int row) const {
    const command_block* block = get_block_at_row(row);
    if (!block || !block->folded) return false;

    // Don't fold the command line itself, only the output
    return row > block->start_row;
}

vector<int> BlockManager::get_visible_rows() const {
    // Implementation for getting visible rows considering folding
    // This would be used by the terminal rendering system
    vector<int> visible;
    // Simplified implementation - would need terminal context
    return visible;
}

bool BlockManager::has_shell_integration() const {
    time_t now = time(nullptr);
    return last_osc133_time > 0 && (now - last_osc133_time < 60); // Active in last minute
}

bool BlockManager::is_command_running() const {
    if (!has_shell_integration()) return false;
    if (blocks.empty()) return false;

    const command_block& current = blocks.back();
    return current.state == BLOCK_COMMAND_RUNNING;
}

bool BlockManager::is_at_prompt() const {
    if (!has_shell_integration()) return false;

    if (blocks.empty()) {
        // No blocks yet, but we have shell integration - could be initial prompt
        return true;
    }

    const command_block& current = blocks.back();
    return current.state == BLOCK_NONE ||
           current.state == BLOCK_COMMAND_START;
}

bool BlockManager::is_showing_output() const {
    if (!has_shell_integration()) return false;
    if (blocks.empty()) return false;

    const command_block& current = blocks.back();
    return current.state == BLOCK_COMMAND_RUNNING ||
           current.state == BLOCK_COMMAND_COMPLETE;
}

block_state BlockManager::get_current_state() const {
    if (blocks.empty()) return BLOCK_NONE;
    return blocks.back().state;
}

bool BlockManager::has_active_input() const {
    return is_at_prompt() && !blocks.empty();
}

const command_block* BlockManager::get_input_block() const {
    if (!has_active_input()) return nullptr;
    return &blocks.back();
}

vector<int> BlockManager::search_in_blocks(const char * pattern, int block_id) const {
    vector<int> matches;
    // Implementation would search within specified block(s)
    // This would need terminal context to access actual text content
    return matches;
}