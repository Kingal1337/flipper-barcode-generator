#include "text_input.h"

#ifndef FW_ORIGIN_Momentum
extern const Icon I_KeySaveSelected_22x11;
extern const Icon I_KeySave_22x11;
extern const Icon I_KeyKeyboardSelected_10x11;
extern const Icon I_KeyKeyboard_10x11;
extern const Icon I_KeyBackspaceSelected_17x11;
extern const Icon I_KeyBackspace_17x11;
/* removed unresolved icon */
#include <gui/elements.h>
#include <furi.h>

struct TextInput {
    View* view;
    FuriTimer* timer;
};

typedef struct {
    const char text;
    const uint8_t x;
    const uint8_t y;
} TextInputKey;

typedef struct {
    const TextInputKey* rows[3];
    const uint8_t keyboard_index;
} Keyboard;

typedef struct {
    const char* header;
    char* text_buffer;
    size_t text_buffer_size;
    size_t minimum_length;
    bool clear_default_text;

    TextInputCallback callback;
    void* callback_context;

    uint8_t selected_row;
    uint8_t selected_column;

    TextInputValidatorCallback validator_callback;
    void* validator_callback_context;
    FuriString* validator_text;
    bool validator_message_visible;

    bool illegal_symbols;
    bool cursor_select;
    uint8_t selected_keyboard;
    size_t cursor_pos;
} TextInputModel;

static const uint8_t keyboard_origin_x = 1;
static const uint8_t keyboard_origin_y = 29;
static const uint8_t keyboard_row_count = 3;
static const uint8_t keyboard_count = 2;

#define ENTER_KEY           '\r'
#define BACKSPACE_KEY       '\b'
#define SWITCH_KEYBOARD_KEY '\t'

static const TextInputKey keyboard_keys_row_1[] = {
    {'q', 1, 8},
    {'w', 10, 8},
    {'e', 19, 8},
    {'r', 28, 8},
    {'t', 37, 8},
    {'y', 46, 8},
    {'u', 55, 8},
    {'i', 64, 8},
    {'o', 73, 8},
    {'p', 82, 8},
    {'0', 92, 8},
    {'1', 102, 8},
    {'2', 111, 8},
    {'3', 120, 8},
};

static const TextInputKey keyboard_keys_row_2[] = {
    {'a', 1, 20},
    {'s', 10, 20},
    {'d', 19, 20},
    {'f', 28, 20},
    {'g', 37, 20},
    {'h', 46, 20},
    {'j', 55, 20},
    {'k', 64, 20},
    {'l', 73, 20},
    {BACKSPACE_KEY, 82, 11},
    {'4', 102, 20},
    {'5', 111, 20},
    {'6', 120, 20},
};

static const TextInputKey keyboard_keys_row_3[] = {
    {SWITCH_KEYBOARD_KEY, 0, 23},
    {'z', 13, 32},
    {'x', 21, 32},
    {'c', 29, 32},
    {'v', 37, 32},
    {'b', 45, 32},
    {'n', 53, 32},
    {'m', 61, 32},
    {'_', 69, 32},
    {ENTER_KEY, 77, 23},
    {'7', 102, 32},
    {'8', 111, 32},
    {'9', 120, 32},
};

static const TextInputKey symbol_keyboard_keys_row_1[] = {
    {'!', 2, 8},
    {'@', 12, 8},
    {'#', 22, 8},
    {'$', 32, 8},
    {'%', 42, 8},
    {'^', 52, 8},
    {'&', 62, 8},
    {'(', 71, 8},
    {')', 81, 8},
    {'0', 92, 8},
    {'1', 102, 8},
    {'2', 111, 8},
    {'3', 120, 8},
};

static const TextInputKey symbol_keyboard_keys_row_2[] = {
    {'~', 2, 20},
    {'+', 12, 20},
    {'-', 22, 20},
    {'=', 32, 20},
    {'[', 42, 20},
    {']', 52, 20},
    {'{', 62, 20},
    {'}', 72, 20},
    {BACKSPACE_KEY, 82, 11},
    {'4', 102, 20},
    {'5', 111, 20},
    {'6', 120, 20},
};

static const TextInputKey symbol_keyboard_keys_row_3[] = {
    {SWITCH_KEYBOARD_KEY, 0, 23},
    {'.', 15, 32},
    {',', 29, 32},
    {';', 41, 32},
    {'`', 53, 32},
    {'\'', 65, 32},
    {ENTER_KEY, 77, 23},
    {'7', 102, 32},
    {'8', 111, 32},
    {'9', 120, 32},
};

static const Keyboard keyboard = {
    .rows =
        {
            keyboard_keys_row_1,
            keyboard_keys_row_2,
            keyboard_keys_row_3,
        },
    .keyboard_index = 0,
};

static const Keyboard symbol_keyboard = {
    .rows =
        {
            symbol_keyboard_keys_row_1,
            symbol_keyboard_keys_row_2,
            symbol_keyboard_keys_row_3,
        },
    .keyboard_index = 1,
};

static const Keyboard* keyboards[] = {
    &keyboard,
    &symbol_keyboard,
};

static void switch_keyboard(TextInputModel* model) {
    model->selected_keyboard = (model->selected_keyboard + 1) % keyboard_count;
}

static uint8_t get_row_size(const Keyboard* keyboard, uint8_t row_index) {
    uint8_t row_size = 0;
    if(keyboard == &symbol_keyboard) {
        switch(row_index + 1) {
        case 1:
            row_size = COUNT_OF(symbol_keyboard_keys_row_1);
            break;
        case 2:
            row_size = COUNT_OF(symbol_keyboard_keys_row_2);
            break;
        case 3:
            row_size = COUNT_OF(symbol_keyboard_keys_row_3);
            break;
        default:
            furi_crash();
        }
    } else {
        switch(row_index + 1) {
        case 1:
            row_size = COUNT_OF(keyboard_keys_row_1);
            break;
        case 2:
            row_size = COUNT_OF(keyboard_keys_row_2);
            break;
        case 3:
            row_size = COUNT_OF(keyboard_keys_row_3);
            break;
        default:
            furi_crash();
        }
    }

    return row_size;
}

static const TextInputKey* get_row(const Keyboard* keyboard, uint8_t row_index) {
    const TextInputKey* row = NULL;
    if(row_index < 3) {
        row = keyboard->rows[row_index];
    } else {
        furi_crash();
    }

    return row;
}

static char get_selected_char(TextInputModel* model) {
    return get_row(
               keyboards[model->selected_keyboard], model->selected_row)[model->selected_column]
        .text;
}

static bool char_is_lowercase(char letter) {
    return letter >= 0x61 && letter <= 0x7A;
}

static char char_to_uppercase(const char letter) {
    if(letter == '_') {
        return 0x20;
    } else if(char_is_lowercase(letter)) {
        return letter - 0x20;
    } else {
        return letter;
    }
}

static char char_to_illegal_symbol(char original) {
    switch(original) {
    default:
        return original;
    case '0':
        return '_';
    case '1':
        return '<';
    case '2':
        return '>';
    case '3':
        return ':';
    case '4':
        return '"';
    case '5':
        return '/';
    case '6':
        return '\\';
    case '7':
        return '|';
    case '8':
        return '?';
    case '9':
        return '*';
    }
}

static void text_input_backspace_cb(TextInputModel* model) {
    if(model->clear_default_text) {
        model->text_buffer[0] = 0;
        model->cursor_pos = 0;
    } else if(model->cursor_pos > 0) {
        char* move = model->text_buffer + model->cursor_pos;
        memmove(move - 1, move, strlen(move) + 1);
        model->cursor_pos--;
    }
}

static void text_input_view_draw_callback(Canvas* canvas, void* _model) {
    TextInputModel* model = _model;
    uint8_t text_length = model->text_buffer ? strlen(model->text_buffer) : 0;
    uint8_t needed_string_width = canvas_width(canvas) - 8;
    uint8_t start_pos = 4;

    model->cursor_pos = model->cursor_pos > text_length ? text_length : model->cursor_pos;
    size_t cursor_pos = model->cursor_pos;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_str(canvas, 2, 8, model->header);
    elements_slightly_rounded_frame(canvas, 1, 12, 126, 15);

    char buf[text_length + 1];
    if(model->text_buffer) {
        strlcpy(buf, model->text_buffer, sizeof(buf));
    }
    char* str = buf;

    if(model->clear_default_text) {
        elements_slightly_rounded_box(
            canvas, start_pos - 1, 14, canvas_string_width(canvas, str) + 2, 10);
        canvas_set_color(canvas, ColorWhite);
    } else {
        char* move = str + cursor_pos;
        memmove(move + 1, move, strlen(move) + 1);
        str[cursor_pos] = '|';
    }

    if(cursor_pos > 0 && canvas_string_width(canvas, str) > needed_string_width) {
        canvas_draw_str(canvas, start_pos, 22, "...");
        start_pos += 6;
        needed_string_width -= 8;
        for(uint32_t off = 0;
            strlen(str) && canvas_string_width(canvas, str) > needed_string_width &&
            off < cursor_pos;
            off++) {
            str++;
        }
    }

    if(canvas_string_width(canvas, str) > needed_string_width) {
        needed_string_width -= 4;
        size_t len = strlen(str);
        while(len && canvas_string_width(canvas, str) > needed_string_width) {
            str[len--] = '\0';
        }
        strlcat(str, "...", sizeof(buf) - (str - buf));
    }

    canvas_draw_str(canvas, start_pos, 22, str);

    canvas_set_font(canvas, FontKeyboard);

    bool uppercase = model->clear_default_text || text_length == 0;
    bool symbols = model->selected_keyboard == symbol_keyboard.keyboard_index;
    for(uint8_t row = 0; row < keyboard_row_count; row++) {
        const uint8_t column_count = get_row_size(keyboards[model->selected_keyboard], row);
        const TextInputKey* keys = get_row(keyboards[model->selected_keyboard], row);

        for(size_t column = 0; column < column_count; column++) {
            bool selected = !model->cursor_select && model->selected_row == row &&
                            model->selected_column == column;
            const Icon* icon = NULL;
            if(keys[column].text == ENTER_KEY) {
                icon = selected ? &I_KeySaveSelected_22x11 : &I_KeySave_22x11;
            } else if(keys[column].text == SWITCH_KEYBOARD_KEY) {
                icon = selected ? &I_KeyKeyboardSelected_10x11 : &I_KeyKeyboard_10x11;
            } else if(keys[column].text == BACKSPACE_KEY) {
                icon = selected ? &I_KeyBackspaceSelected_17x11 : &I_KeyBackspace_17x11;
            }
            canvas_set_color(canvas, ColorBlack);
            if(icon != NULL) {
                canvas_draw_icon(
                    canvas,
                    keyboard_origin_x + keys[column].x,
                    keyboard_origin_y + keys[column].y,
                    icon);
            } else {
                if(selected) {
                    elements_slightly_rounded_box(
                        canvas,
                        keyboard_origin_x + keys[column].x - 2,
                        keyboard_origin_y + keys[column].y - 9,
                        9,
                        11);
                    canvas_set_color(canvas, ColorWhite);
                }

                char glyph = keys[column].text;
                if(uppercase && !symbols) {
                    canvas_draw_glyph(
                        canvas,
                        keyboard_origin_x + keys[column].x,
                        keyboard_origin_y + keys[column].y,
                        char_to_uppercase(glyph));
                } else {
                    canvas_draw_glyph(
                        canvas,
                        keyboard_origin_x + keys[column].x,
                        keyboard_origin_y + keys[column].y -
                            (glyph == '_' || char_is_lowercase(glyph)),
                        (symbols && model->illegal_symbols) ? char_to_illegal_symbol(glyph) :
                                                              glyph);
                }
            }
        }
    }
    if(model->validator_message_visible) {
        canvas_set_font(canvas, FontSecondary);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 8, 10, 110, 48);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_icon(canvas, 10, 14, NULL /* removed */);
        canvas_draw_rframe(canvas, 8, 8, 112, 50, 3);
        canvas_draw_rframe(canvas, 9, 9, 110, 48, 2);
        elements_multiline_text(canvas, 62, 20, furi_string_get_cstr(model->validator_text));
        canvas_set_font(canvas, FontKeyboard);
    }
}

static void text_input_handle_up(TextInput* text_input, TextInputModel* model) {
    UNUSED(text_input);
    if(model->selected_row > 0) {
        model->selected_row--;
        if(model->selected_row == 0 &&
           model->selected_column >
               get_row_size(keyboards[model->selected_keyboard], model->selected_row) - 6) {
            model->selected_column = model->selected_column + 1;
        }
        if(model->selected_row == 1 &&
           model->selected_keyboard == symbol_keyboard.keyboard_index) {
            if(model->selected_column > 5)
                model->selected_column += 2;
            else if(model->selected_column > 1)
                model->selected_column += 1;
        }
    } else {
        model->cursor_select = true;
        model->clear_default_text = false;
    }
}

static void text_input_handle_down(TextInput* text_input, TextInputModel* model) {
    UNUSED(text_input);
    if(model->cursor_select) {
        model->cursor_select = false;
    } else if(model->selected_row < keyboard_row_count - 1) {
        model->selected_row++;
        if(model->selected_row == 1 &&
           model->selected_column >
               get_row_size(keyboards[model->selected_keyboard], model->selected_row) - 4) {
            model->selected_column = model->selected_column - 1;
        }
        if(model->selected_row == 2 &&
           model->selected_keyboard == symbol_keyboard.keyboard_index) {
            if(model->selected_column > 6)
                model->selected_column -= 2;
            else if(model->selected_column > 1)
                model->selected_column -= 1;
        }
    }
}

static void text_input_handle_left(TextInput* text_input, TextInputModel* model) {
    UNUSED(text_input);
    if(model->cursor_select) {
        model->clear_default_text = false;
        if(model->cursor_pos > 0) {
            model->cursor_pos = CLAMP(model->cursor_pos - 1, strlen(model->text_buffer), 0u);
        }
    } else if(model->selected_column > 0) {
        model->selected_column--;
    } else {
        model->selected_column =
            get_row_size(keyboards[model->selected_keyboard], model->selected_row) - 1;
    }
}

static void text_input_handle_right(TextInput* text_input, TextInputModel* model) {
    UNUSED(text_input);
    if(model->cursor_select) {
        model->clear_default_text = false;
        model->cursor_pos = CLAMP(model->cursor_pos + 1, strlen(model->text_buffer), 0u);
    } else if(
        model->selected_column <
        get_row_size(keyboards[model->selected_keyboard], model->selected_row) - 1) {
        model->selected_column++;
    } else {
        model->selected_column = 0;
    }
}

static void text_input_handle_ok(TextInput* text_input, TextInputModel* model, InputType type) {
    if(model->cursor_select) {
        model->clear_default_text = !model->clear_default_text;
        return;
    }
    bool shift = type == InputTypeLong;
    bool repeat = type == InputTypeRepeat;
    char selected = get_selected_char(model);
    size_t text_length = strlen(model->text_buffer);

    if(selected == ENTER_KEY) {
        if(model->validator_callback &&
           (!model->validator_callback(
               model->text_buffer, model->validator_text, model->validator_callback_context))) {
            model->validator_message_visible = true;
            furi_timer_start(text_input->timer, furi_kernel_get_tick_frequency() * 4);
        } else if(model->callback != 0 && text_length >= model->minimum_length) {
            model->callback(model->callback_context);
        }
    } else if(selected == SWITCH_KEYBOARD_KEY) {
        switch_keyboard(model);
    } else {
        if(selected == BACKSPACE_KEY) {
            text_input_backspace_cb(model);
        } else if(!repeat) {
            if(model->clear_default_text) {
                text_length = 0;
            }
            if(text_length < (model->text_buffer_size - 1)) {
                if(shift != (text_length == 0) &&
                   model->selected_keyboard != symbol_keyboard.keyboard_index) {
                    selected = char_to_uppercase(selected);
                }
                if(model->selected_keyboard == symbol_keyboard.keyboard_index &&
                   model->illegal_symbols) {
                    selected = char_to_illegal_symbol(selected);
                }
                if(model->clear_default_text) {
                    model->text_buffer[0] = selected;
                    model->text_buffer[1] = '\0';
                    model->cursor_pos = 1;
                } else {
                    char* move = model->text_buffer + model->cursor_pos;
                    memmove(move + 1, move, strlen(move) + 1);
                    model->text_buffer[model->cursor_pos] = selected;
                    model->cursor_pos++;
                }
            }
        }
        model->clear_default_text = false;
    }
}

static bool text_input_view_input_callback(InputEvent* event, void* context) {
    TextInput* text_input = context;
    furi_assert(text_input);

    bool consumed = false;

    // Acquire model
    TextInputModel* model = view_get_model(text_input->view);

    if((!(event->type == InputTypePress) && !(event->type == InputTypeRelease)) &&
       model->validator_message_visible) {
        model->validator_message_visible = false;
        consumed = true;
    } else if(event->type == InputTypeShort) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            text_input_handle_up(text_input, model);
            break;
        case InputKeyDown:
            text_input_handle_down(text_input, model);
            break;
        case InputKeyLeft:
            text_input_handle_left(text_input, model);
            break;
        case InputKeyRight:
            text_input_handle_right(text_input, model);
            break;
        case InputKeyOk:
            text_input_handle_ok(text_input, model, event->type);
            break;
        default:
            consumed = false;
            break;
        }
    } else if(event->type == InputTypeLong) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            text_input_handle_up(text_input, model);
            break;
        case InputKeyDown:
            text_input_handle_down(text_input, model);
            break;
        case InputKeyLeft:
            text_input_handle_left(text_input, model);
            break;
        case InputKeyRight:
            text_input_handle_right(text_input, model);
            break;
        case InputKeyOk:
            text_input_handle_ok(text_input, model, event->type);
            break;
        case InputKeyBack:
            text_input_backspace_cb(model);
            break;
        default:
            consumed = false;
            break;
        }
    } else if(event->type == InputTypeRepeat) {
        consumed = true;
        switch(event->key) {
        case InputKeyUp:
            text_input_handle_up(text_input, model);
            break;
        case InputKeyDown:
            text_input_handle_down(text_input, model);
            break;
        case InputKeyLeft:
            text_input_handle_left(text_input, model);
            break;
        case InputKeyRight:
            text_input_handle_right(text_input, model);
            break;
        case InputKeyOk:
            text_input_handle_ok(text_input, model, event->type);
            break;
        case InputKeyBack:
            text_input_backspace_cb(model);
            break;
        default:
            consumed = false;
            break;
        }
    }

    // Commit model
    view_commit_model(text_input->view, consumed);

    return consumed;
}

void text_input_timer_callback(void* context) {
    furi_assert(context);
    TextInput* text_input = context;

    with_view_model(
        text_input->view,
        TextInputModel * model,
        { model->validator_message_visible = false; },
        true);
}

TextInput* text_input_alloc(void) {
    TextInput* text_input = malloc(sizeof(TextInput));
    text_input->view = view_alloc();
    view_set_context(text_input->view, text_input);
    view_allocate_model(text_input->view, ViewModelTypeLocking, sizeof(TextInputModel));
    view_set_draw_callback(text_input->view, text_input_view_draw_callback);
    view_set_input_callback(text_input->view, text_input_view_input_callback);

    text_input->timer = furi_timer_alloc(text_input_timer_callback, FuriTimerTypeOnce, text_input);

    with_view_model(
        text_input->view,
        TextInputModel * model,
        {
            model->validator_text = furi_string_alloc();
            model->minimum_length = 1;
            model->illegal_symbols = false;
            model->cursor_pos = 0;
            model->cursor_select = false;
        },
        false);

    text_input_reset(text_input);

    return text_input;
}

void text_input_free(TextInput* text_input) {
    furi_check(text_input);
    with_view_model(
        text_input->view,
        TextInputModel * model,
        { furi_string_free(model->validator_text); },
        false);

    // Send stop command
    furi_timer_stop(text_input->timer);
    // Release allocated memory
    furi_timer_free(text_input->timer);

    view_free(text_input->view);

    free(text_input);
}

void text_input_reset(TextInput* text_input) {
    furi_check(text_input);
    with_view_model(
        text_input->view,
        TextInputModel * model,
        {
            model->header = "";
            model->selected_row = 0;
            model->selected_column = 0;
            model->selected_keyboard = 0;
            model->minimum_length = 1;
            model->illegal_symbols = false;
            model->clear_default_text = false;
            model->cursor_pos = 0;
            model->cursor_select = false;
            model->text_buffer = NULL;
            model->text_buffer_size = 0;
            model->callback = NULL;
            model->callback_context = NULL;
            model->validator_callback = NULL;
            model->validator_callback_context = NULL;
            furi_string_reset(model->validator_text);
            model->validator_message_visible = false;
        },
        true);
}

View* text_input_get_view(TextInput* text_input) {
    furi_check(text_input);
    return text_input->view;
}

void text_input_set_result_callback(
    TextInput* text_input,
    TextInputCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text) {
    furi_check(text_input);
    with_view_model(
        text_input->view,
        TextInputModel * model,
        {
            model->callback = callback;
            model->callback_context = callback_context;
            model->text_buffer = text_buffer;
            model->text_buffer_size = text_buffer_size;
            model->clear_default_text = clear_default_text;
            model->cursor_select = false;
            if(text_buffer && text_buffer[0] != '\0') {
                model->cursor_pos = strlen(text_buffer);
                // Set focus on Save
                model->selected_row = 2;
                model->selected_column = 9;
                model->selected_keyboard = 0;
            } else {
                model->cursor_pos = 0;
            }
        },
        true);
}

void text_input_set_minimum_length(TextInput* text_input, size_t minimum_length) {
    furi_check(text_input);
    with_view_model(
        text_input->view,
        TextInputModel * model,
        { model->minimum_length = minimum_length; },
        true);
}

void text_input_show_illegal_symbols(TextInput* text_input, bool show) {
    furi_check(text_input);
    with_view_model(
        text_input->view, TextInputModel * model, { model->illegal_symbols = show; }, true);
}

void text_input_set_validator(
    TextInput* text_input,
    TextInputValidatorCallback callback,
    void* callback_context) {
    furi_check(text_input);
    with_view_model(
        text_input->view,
        TextInputModel * model,
        {
            model->validator_callback = callback;
            model->validator_callback_context = callback_context;
        },
        true);
}

TextInputValidatorCallback text_input_get_validator_callback(TextInput* text_input) {
    furi_check(text_input);
    TextInputValidatorCallback validator_callback = NULL;
    with_view_model(
        text_input->view,
        TextInputModel * model,
        { validator_callback = model->validator_callback; },
        false);
    return validator_callback;
}

void* text_input_get_validator_callback_context(TextInput* text_input) {
    furi_check(text_input);
    void* validator_callback_context = NULL;
    with_view_model(
        text_input->view,
        TextInputModel * model,
        { validator_callback_context = model->validator_callback_context; },
        false);
    return validator_callback_context;
}

void text_input_set_header_text(TextInput* text_input, const char* text) {
    furi_check(text_input);
    with_view_model(text_input->view, TextInputModel * model, { model->header = text; }, true);
}

#endif


void text_input_force_page(TextInput* text_input, bool symbols) {
    furi_check(text_input);
    with_view_model(
        text_input->view,
        TextInputModel * model,
        {
            model->selected_keyboard = symbols ? symbol_keyboard.keyboard_index : 0;
            model->selected_row = 0;
            model->selected_column = 0;
            model->cursor_select = false;
        },
        true);
}
