#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>

// --- CONFIGURATION DU SIGNAL RAW ---
static const int32_t raw_signal_timings[] = {
    500, -500,  // Impulsion courte, silence court
    1000, -500, // Impulsion longue, silence court
    500, -1000, // Impulsion courte, silence long
    1000, -1000 // Impulsion longue, silence long
};
static const size_t raw_signal_length = COUNT_OF(raw_signal_timings);

static size_t current_timing_index = 0;
static bool is_transmitting = false;

// --- CALLBACK DMA POUR LA RADIO ---
static LevelDuration tx_yield_callback(void* context) {
    UNUSED(context);
    if(current_timing_index >= raw_signal_length) {
        return level_duration_make(false, 0);
    }
    int32_t duration = raw_signal_timings[current_timing_index];
    current_timing_index++;
    if(duration > 0) {
        return level_duration_make(true, (uint32_t)duration);
    } else {
        return level_duration_make(false, (uint32_t)(-duration));
    }
}

// --- FONCTION D'ÉMISSION ---
static void transmit_signal() {
    if(is_transmitting) return;
    is_transmitting = true;
    current_timing_index = 0;

    furi_hal_subghz_reset();
    furi_hal_subghz_load_preset(FuriHalSubGhzPresetOok650Async);
    furi_hal_subghz_set_frequency_and_path(433920000); // 433.92 MHz

    furi_hal_subghz_start_async_tx(tx_yield_callback, NULL);

    while(!furi_hal_subghz_is_async_tx_complete()) {
        furi_delay_ms(10);
    }

    furi_hal_subghz_stop_async_tx();
    furi_hal_subghz_sleep();
    is_transmitting = false;
}

// --- INTERFACE GRAPHIQUE ---
static void draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 10, 15, "Sub-GHz RAW Emitter");
    canvas_set_font(canvas, FontSecondary);
    if(is_transmitting) {
        canvas_draw_str(canvas, 10, 35, ">> ENVOI EN COURS... <<");
    } else {
        canvas_draw_str(canvas, 10, 35, "Appuyez sur OK pour envoyer");
    }
    canvas_draw_str(canvas, 10, 55, "Retour pour quitter");
}

static void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    if(input_event->type == InputTypeShort) {
        furi_message_queue_put(event_queue, input_event, FuriWaitForever);
    }
}

// --- POINT D'ENTRÉE ---
int32_t custom_subghz_main(void* p) {
    UNUSED(p);
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, NULL);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;

    while(running) {
        if(furi_message_queue_get(event_queue, &event, 50) == FuriStatusOk) {
            if(event.key == InputKeyBack) {
                running = false;
            }
            if(event.key == InputKeyOk) {
                transmit_signal();
            }
        }
        view_port_update(view_port);
    }

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);
    return 0;
}
