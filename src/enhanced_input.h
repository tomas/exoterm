#ifndef ENHANCED_INPUT_H_
#define ENHANCED_INPUT_H_

#include <string>
#include <ctime>
#include "blocks.h"

// Forward declaration
struct rxvt_term;

struct InputLine {
    int row;                    // Screen row of input line
    int prompt_end_col;         // Column where prompt ends
    int input_start_col;        // Column where user input starts
    int input_end_col;          // Column where user input ends
    std::string prompt_text;    // The prompt portion
    std::string input_text;     // The user input portion
    bool is_continuation;       // Multi-line input continuation
    time_t last_update;         // When this was last modified
};

class InputLineManager {
private:
    InputLine current_input;
    bool input_active;
    bool mouse_input_enabled;

public:
    InputLineManager() : input_active(false), mouse_input_enabled(true) {
        reset_input();
    }

    void reset_input();
    void start_input_line(int row, const std::string& line_content);
    void update_cursor_position(int row, int col);
    void update_input_text(const std::string& new_line_content);
    void update_from_block_state(const BlockManager& block_manager);

    bool is_position_in_input(int row, int col) const;
    int screen_col_to_input_pos(int col) const;
    int input_pos_to_screen_col(int pos) const;

    const InputLine& get_current_input() const { return current_input; }
    bool is_input_active() const { return input_active; }

    void complete_input();
    void cancel_input();

private:
    void parse_prompt_and_input(const std::string& line_content);
    int detect_prompt_end(const std::string& line);
};

class MouseInputHandler {
private:
    rxvt_term* term;
    InputLineManager* input_manager;
    bool mouse_input_enabled;

public:
    MouseInputHandler(rxvt_term* t, InputLineManager* im)
        : term(t), input_manager(im), mouse_input_enabled(true) {}

    bool handle_mouse_press(int button, int row, int col, unsigned int state);
    bool handle_mouse_drag(int row, int col, unsigned int state);
    bool position_cursor_at(int row, int col);
    bool apply_selection_to_input(const std::string& selected_text, bool replace_mode);

private:
    bool send_cursor_movement_sequence(int target_pos);
    bool selection_overlaps_input();
    bool replace_selected_input(const std::string& replacement_text);
    bool delete_selected_input();
};

#endif // ENHANCED_INPUT_H_