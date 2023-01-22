#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <input/input.h>

#define HEXCHAR(i) ( (i) >= 10 ? 'A' + (i) - 10 : '0' + (i) )
#define PROTOCOLNAME ("Protocol")

typedef struct {
    bool active;
    uint8_t data[10]; // max length we can print on the screen in hex
    uint8_t data_length; // length in use for the current protocol
    int8_t data_current_byte; // which byte (little endian) we're currently editing, -1 == protocol
} LFBFState;

static LFBFState state = { .active = false, .data = "\0\0\0\0\0\0\0\0\0\0", .data_length = 10, .data_current_byte = 0 };

// Screen is 128x64 px
static void app_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontKeyboard);

    canvas_draw_str(canvas, 0, 7, PROTOCOLNAME);
    if (state.data_current_byte == -1) {
        for (uint8_t i = 0; i < strlen(PROTOCOLNAME); i++) {
            canvas_draw_str(canvas, 6 *  i, 7, "_");
        }
    }

    for (uint8_t i = 0; i < state.data_length; i++) {
        char byte[3] = "  ";
        byte[0] = HEXCHAR(state.data[i] >> 4);
        byte[1] = HEXCHAR(state.data[i] & 0xf);
        canvas_draw_str(canvas, 12*i, 17, byte);
    }
    if (state.data_current_byte != -1) {
        canvas_draw_str(canvas, 12 * state.data_current_byte, 17, "__");
    }
    if (state.active) {
        canvas_draw_str(canvas, 0, 64, "ACTIVE");
    }
}

static void app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);

    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

int32_t lfbf_main(void* p) {
    UNUSED(p);
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // Configure view port
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, app_draw_callback, view_port);
    view_port_input_callback_set(view_port, app_input_callback, event_queue);

    // Register view port in GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;

    bool running = true;
    while(running) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            if((event.type == InputTypePress) || (event.type == InputTypeRepeat)) {
                switch(event.key) {
                case InputKeyLeft:
                    state.data_current_byte -= 1;
                    if (state.data_current_byte == -2) {
                        state.data_current_byte = state.data_length - 1;
                    }
                    break;
                case InputKeyRight:
                    state.data_current_byte += 1;
                    if (state.data_current_byte == state.data_length) {
                        state.data_current_byte = -1;
                    }
                    break;
                case InputKeyUp:
                    if (state.data_current_byte >= 0) {
                        state.data[state.data_current_byte] += 1;
                    }
                    break;
                case InputKeyDown:
                    if (state.data_current_byte >= 0) {
                        state.data[state.data_current_byte] -= 1;
                    }
                    break;
                case InputKeyOk:
                    state.active = ! state.active;
                    break;
                default:
                    running = false;
                    break;
                }
            }
        }
        view_port_update(view_port);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);

    furi_record_close(RECORD_GUI);

    return 0;
}
