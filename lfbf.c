#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <input/input.h>

#include <gui/elements.h>

#include <toolbox/protocols/protocol_dict.h>
#include <lfrfid/protocols/lfrfid_protocols.h>
#include <lib/lfrfid/lfrfid_worker.h>

#define TAG "lfbf"
#define HEXCHAR(i) ( (i) < 10 ? '0' + (i) : 'A' + (i) - 10 )

typedef struct {
    bool active;
    uint8_t data[16]; // but we can only print 10 :(
    uint8_t data_length; // length in use for the current protocol
    int8_t data_current_byte; // which byte (little endian) we're currently editing, -1 == protocol
    ProtocolDict* dict;
    ProtocolId protocol_id;
    LFRFIDWorker* worker;
} LFBFState;

static LFBFState state = {
    .active = false,
    .data = {0},
    .data_length = 16,
    .data_current_byte = -1,
    .dict = NULL,
    .protocol_id = 0,
    .worker = NULL,
};

// Screen is 128x64 px
static void app_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    const char *protocol_name;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontKeyboard);

    protocol_name = protocol_dict_get_name(state.dict, state.protocol_id);
    canvas_draw_str(canvas, 0, 7, protocol_name);
    if (state.data_current_byte == -1) {
        for (uint8_t i = 0; i < strlen(protocol_name); i++) {
            canvas_draw_str(canvas, 6 *  i, 7, "_");
        }
    }

    if (state.active) {
        canvas_draw_str(canvas, 90, 7, "ACTIVE");
    }

    for (uint8_t i = 0; i < state.data_length; i++) {
        char hex_byte[3];
        hex_byte[0] = HEXCHAR(state.data[i] >> 4);
        hex_byte[1] = HEXCHAR(state.data[i] & 0xf);
        hex_byte[2] = '\0';
        canvas_draw_str(canvas, 12*i, 18, hex_byte);
    }
    if (state.data_current_byte != -1) {
        canvas_draw_str(canvas, 12 * state.data_current_byte, 18, "__");
    }

    canvas_set_font(canvas, FontSecondary);
    FuriString* render_data;
    render_data = furi_string_alloc();
    protocol_dict_render_data(state.dict, render_data, state.protocol_id);
    elements_multiline_text(canvas, 0, 29, furi_string_get_cstr(render_data));
    furi_string_free(render_data);
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

    // Set up LFBF
    state.dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    state.protocol_id = 0;
    state.data_length = protocol_dict_get_data_size(state.dict, state.protocol_id);
    protocol_dict_get_data(state.dict, state.protocol_id, state.data, state.data_length);

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
                        protocol_dict_set_data(state.dict, state.protocol_id, state.data, state.data_length);
                        FURI_LOG_I(TAG, "set data length %u", state.data_length);
                    } else {
                        state.protocol_id = (state.protocol_id + LFRFIDProtocolMax - 1) % LFRFIDProtocolMax;
                        state.data_length = protocol_dict_get_data_size(state.dict, state.protocol_id);
                        protocol_dict_get_data(state.dict, state.protocol_id, state.data, state.data_length);
                    }
                    break;
                case InputKeyDown:
                    if (state.data_current_byte >= 0) {
                        state.data[state.data_current_byte] -= 1;
                        protocol_dict_set_data(state.dict, state.protocol_id, state.data, state.data_length);
                        FURI_LOG_I(TAG, "set data length %u", state.data_length);
                    } else {
                        state.protocol_id = (state.protocol_id + 1) % LFRFIDProtocolMax;
                        state.data_length = protocol_dict_get_data_size(state.dict, state.protocol_id);
                        protocol_dict_get_data(state.dict, state.protocol_id, state.data, state.data_length);
                    }
                    break;
                case InputKeyOk:
                    state.active = ! state.active;
                    if (state.active) {
                        state.worker = lfrfid_worker_alloc(state.dict);
                        lfrfid_worker_start_thread(state.worker);
                        lfrfid_worker_emulate_start(state.worker, state.protocol_id);
                    } else {
                        lfrfid_worker_stop(state.worker);
                        lfrfid_worker_stop_thread(state.worker);
                        lfrfid_worker_free(state.worker);
                        state.worker = NULL;
                    }
                    break;
                default:
                    running = false;
                    break;
                }
            }
        }
        view_port_update(view_port);
    }

    // tear down LFBF
    protocol_dict_free(state.dict);

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);

    furi_record_close(RECORD_GUI);

    return 0;
}
