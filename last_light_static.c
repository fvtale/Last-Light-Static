#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    Room_Safehouse,
    Room_Alley,
    Room_CourtSquare,
    Room_CoffeeShop,
    Room_ServiceArcade,
    Room_CourthouseSteps,
    Room_RecordsLobby,
    Room_SecurityAnnex,
    Room_EvidenceCorridor,
    Room_TunnelWest,
    Room_TunnelEast,
    Room_UtilityJunction,
    Room_CityHallExterior,
    Room_CityHallSteps,
    Room_CityHallLobby,
    Room_CouncilGallery,
    Room_DataArchive,
    Room_SecurityWing,
    Room_Escape,
    Room_Rooftop,
} LlsRoomId;

typedef enum {
    Outcome_None,
    Outcome_Best,
    Outcome_Compromised,
    Outcome_FailedExpose,
    Outcome_Timeout,
    Outcome_Sellout,
    Outcome_Sacrifice,
    Outcome_EarlyDeath,
} LlsOutcome;

typedef struct {
    bool cloned_rfid;
    bool deck_module;
    bool evidence_drive;
    bool crosslink_module;
    bool speaker_planted;
    bool speaker_timed_correctly;
    bool spared_guard;
    bool executed_guard;
    bool killed_witness;
    bool hostage_attempted;
    bool eric_betrayed;
    bool accepted_sellout;
    bool crosslink_revealed;
    bool evidence_delivered;
    bool media_distortion;
    bool sacrifice_made;
} LlsFlags;

typedef struct {
    uint16_t total_seconds;
    uint16_t next_banner_threshold;
    bool show_banner;
    uint16_t banner_ticks;
} LlsClock;

typedef struct {
    LlsRoomId room;
    LlsOutcome outcome;
    LlsFlags flags;
    LlsClock clock;
    uint8_t suspicion;
    uint8_t pressure;
    uint8_t health;
    uint8_t dialogue_node;
    bool in_dialogue;
    bool carrying_item;
    bool running;
    char banner[32];
    char message[64];
    FuriMutex* mutex;
} LlsGameState;

static void lls_set_message(LlsGameState* g, const char* text) {
    strncpy(g->message, text, sizeof(g->message) - 1);
    g->message[sizeof(g->message) - 1] = 0;
}

static void lls_set_banner(LlsGameState* g, const char* text) {
    strncpy(g->banner, text, sizeof(g->banner) - 1);
    g->banner[sizeof(g->banner) - 1] = 0;
    g->clock.show_banner = true;
    g->clock.banner_ticks = 0;
}

static void lls_init_state(LlsGameState* g) {
    memset(g, 0, sizeof(*g));
    g->room = Room_Safehouse;
    g->health = 5;
    g->clock.total_seconds = 30 * 60;
    g->clock.next_banner_threshold = 28 * 60;
    g->running = true;
    lls_set_message(g, "BOOT SEQUENCE INITIALIZED...");
}

static void lls_tick_clock(LlsGameState* g) {
    if(g->clock.total_seconds > 0) g->clock.total_seconds--;
    if(g->clock.show_banner) {
        g->clock.banner_ticks++;
        if(g->clock.banner_ticks > 16) g->clock.show_banner = false;
    }
    if(g->clock.total_seconds == g->clock.next_banner_threshold) {
        char buf[32];
        snprintf(buf, sizeof(buf), "WINDOW CLOSING // %02u:%02u", g->clock.total_seconds / 60, g->clock.total_seconds % 60);
        lls_set_banner(g, buf);
        if(g->clock.next_banner_threshold >= 120) g->clock.next_banner_threshold -= 120;
    }
}

static void lls_evaluate_outcome(LlsGameState* g) {
    if(g->clock.total_seconds == 0) g->outcome = Outcome_Timeout;
}

static void lls_draw(Canvas* canvas, void* ctx) {
    LlsGameState* g = ctx;
    furi_mutex_acquire(g->mutex, FuriWaitForever);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "LAST LIGHT STATIC");
    if(g->clock.show_banner) canvas_draw_str(canvas, 2, 18, g->banner);
    canvas_draw_str(canvas, 2, 32, g->message);
    char hud[32];
    snprintf(hud, sizeof(hud), "T:%02u:%02u HP:%u", g->clock.total_seconds / 60, g->clock.total_seconds % 60, g->health);
    canvas_draw_str(canvas, 2, 62, hud);
    furi_mutex_release(g->mutex);
}

static void lls_input(InputEvent* e, void* ctx) {
    LlsGameState* g = ctx;
    furi_mutex_acquire(g->mutex, FuriWaitForever);
    if(e->key == InputKeyBack && e->type == InputTypeShort) g->running = false;
    if(e->key == InputKeyOk && e->type == InputTypeShort && !g->in_dialogue) lls_set_message(g, "PRESS THROUGH.");
    furi_mutex_release(g->mutex);
}

int32_t last_light_static_app(void* p) {
    UNUSED(p);
    LlsGameState* g = malloc(sizeof(LlsGameState));
    if(!g) return 255;
    lls_init_state(g);
    g->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!g->mutex) { free(g); return 255; }

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, lls_draw, g);
    view_port_input_callback_set(view_port, lls_input, g);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    while(g->running) {
        furi_mutex_acquire(g->mutex, FuriWaitForever);
        lls_tick_clock(g);
        lls_evaluate_outcome(g);
        if(g->clock.total_seconds == 0) {
            lls_set_message(g, "You were too late.");
            g->running = false;
        }
        furi_mutex_release(g->mutex);
        view_port_update(view_port);
        furi_delay_ms(1000);
    }

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_mutex_free(g->mutex);
    free(g);
    return 0;
}
