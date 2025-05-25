#include "../config.h"
#include "rxvt.h"
#include "enhanced_input.h"
#include <regex>
#include <algorithm>

// InputLineManager implementation
void InputLineManager::reset_input() {
    current_input = InputLine{};
    current_input.row = -1;
    current_input.prompt_end_col = 0;
    current_input.input_start_col = 0;
    current_input.input_end_col = 0;
    current_input.last_update = time(nullptr);
    input_active = false;
}

void InputLineManager::start_input_line(int row, const std::string& line_content) {
    current_input.row = row;
    current_input.last_update = time(nullptr);

    parse_prompt_and_input(line_content);
    input_active = true;
}

void InputLineManager::update_cursor_position(int row, int col) {
    if (!input_active || row != current_input.row) return;

    current_input.input_end_col = std::max(current_input.input_end_col, col);
    current_input.last_update = time(nullptr);
}

void InputLineManager::update_input_text(const std::string& new_line_content) {
    if (!input_active) return;

    parse_prompt_and_input(new_line_content);
    current_input.last_update = time(nullptr);
}

void InputLineManager::update_from_block_state(const BlockManager& block_manager) {
    // Only work if shell integration is active
    if (!block_manager.has_shell_integration()) {
        if (input_active) {
            cancel_input();
        }
        return;
    }

    if (block_manager.is_command_running()) {
        // Command is executing - disable input line editing
        if (input_active) {
            complete_input();
        }
        return;
    }

    if (!block_manager.is_at_prompt()) {
        // Not at prompt - disable input
        if (input_active) {
            cancel_input();
        }
        return;
    }

    // At prompt with shell integration - input editing is allowed
}

bool InputLineManager::is_position_in_input(int row, int col) const {
    if (!input_active || row != current_input.row) return false;

    return col >= current_input.input_start_col &&
           col <= current_input.input_end_col;
}

int InputLineManager::screen_col_to_input_pos(int col) const {
    if (!input_active) return 0;

    int input_col = col - current_input.input_start_col;
    return std::max(0, std::min(input_col, (int)current_input.input_text.length()));
}

int InputLineManager::input_pos_to_screen_col(int pos) const {
    if (!input_active) return 0;

    return current_input.input_start_col + pos;
}

void InputLineManager::complete_input() {
    input_active = false;
}

void InputLineManager::cancel_input() {
    input_active = false;
}

void InputLineManager::parse_prompt_and_input(const std::string& line_content) {
    current_input.prompt_end_col = detect_prompt_end(line_content);
    current_input.input_start_col = current_input.prompt_end_col;

    if (current_input.prompt_end_col < line_content.length()) {
        current_input.prompt_text = line_content.substr(0, current_input.prompt_end_col);
        current_input.input_text = line_content.substr(current_input.prompt_end_col);
    } else {
        current_input.prompt_text = line_content;
        current_input.input_text = "";
    }

    // Remove trailing whitespace from input
    size_t end = current_input.input_text.find_last_not_of(" \t");
    if (end != std::string::npos) {
        current_input.input_text = current_input.input_text.substr(0, end + 1);
    }

    current_input.input_end_col = current_input.input_start_col + current_input.input_text.length();
}

int InputLineManager::detect_prompt_end(const std::string& line) {
    // Look for common prompt endings
    std::vector<std::string> prompt_endings = {"$ ", "# ", "> ", "% "};

    for (const auto& ending : prompt_endings) {
        size_t pos = line.find(ending);
        if (pos != std::string::npos) {
            return pos + ending.length();
        }
    }

    // Fallback: look for pattern like "user@host:path$ "
    std::regex prompt_regex("^[^$#%>]*[$#%>]\\s+");
    std::smatch match;
    if (std::regex_search(line, match, prompt_regex)) {
        return match.position() + match.length();
    }

    // Last resort: assume first space or tab after some text
    for (size_t i = 1; i < line.length(); i++) {
        if ((line[i] == ' ' || line[i] == '\t') && line[i-1] != ' ' && line[i-1] != '\t') {
            return i + 1;
        }
    }

    return 0; // No prompt detected
}

// MouseInputHandler implementation
bool MouseInputHandler::handle_mouse_press(int button, int row, int col, unsigned int state) {
    if (!mouse_input_enabled || button != Button1) return false;

    // Check if click is in input area
    if (!input_manager->is_position_in_input(row, col)) {
        return false; // Not in input area, let normal handling proceed
    }

    // Position cursor at clicked location
    return position_cursor_at(row, col);
}

bool MouseInputHandler::handle_mouse_drag(int row, int col, unsigned int state) {
    if (!mouse_input_enabled) return false;

    // Only handle drag if it starts or is within input area
    if (!input_manager->is_position_in_input(row, col)) {
        return false;
    }

    // Let selection handling proceed, but track that it's in input area
    return false; // Allow normal selection to work
}

bool MouseInputHandler::position_cursor_at(int row, int col) {
    if (!input_manager->is_input_active()) return false;

    const auto& input = input_manager->get_current_input();
    if (row != input.row) return false;

    // Calculate the desired cursor position in the input text
    int input_pos = input_manager->screen_col_to_input_pos(col);

    // Send cursor movement commands to move to that position
    return send_cursor_movement_sequence(input_pos);
}

bool MouseInputHandler::apply_selection_to_input(const std::string& selected_text, bool replace_mode) {
    if (!input_manager->is_input_active()) return false;

    // Check if current selection overlaps with input area
    if (!selection_overlaps_input()) return false;

    if (replace_mode) {
        return replace_selected_input(selected_text);
    } else {
        return delete_selected_input();
    }
}

bool MouseInputHandler::send_cursor_movement_sequence(int target_pos) {
    const auto& input = input_manager->get_current_input();

    // Get current cursor position in input text
    // This would need access to terminal's current cursor position
    // For now, simplified implementation
    int current_input_pos = 0; // Would get from terminal

    // Calculate how many positions to move
    int move_distance = target_pos - current_input_pos;

    if (move_distance == 0) return true; // Already at target

    // Send appropriate arrow key sequences
    if (move_distance > 0) {
        // Move right
        for (int i = 0; i < move_distance; i++) {
            term->tt_write("\x1b[C", 3); // Right arrow
        }
    } else {
        // Move left
        for (int i = 0; i < -move_distance; i++) {
            term->tt_write("\x1b[D", 3); // Left arrow
        }
    }

    return true;
}

bool MouseInputHandler::selection_overlaps_input() {
    // This would need access to terminal's selection state
    // Simplified implementation
    return false;
}

bool MouseInputHandler::replace_selected_input(const std::string& replacement_text) {
    if (!delete_selected_input()) return false;

    // Insert replacement text at cursor position
    for (char c : replacement_text) {
        if (c >= 32 && c <= 126) { // Printable characters only
            term->tt_write(&c, 1);
        }
    }

    return true;
}

bool MouseInputHandler::delete_selected_input() {
    // This would need access to terminal's selection state
    // Simplified implementation
    return false;
}