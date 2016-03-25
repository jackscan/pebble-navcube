#include <pebble.h>

#define BGCOL GColorDarkGreen
#define FGCOL GColorWhite

struct
{
    Window *window;
    AccelRawData accel;
} g;

static void redraw(struct Layer *layer, GContext *ctx)
{
    GRect bounds = layer_get_bounds(layer);
    int16_t w2 = bounds.size.w / 2;
    int16_t h2 = bounds.size.h / 2;
    // radius for line
    int16_t r = w2 < h2 ? w2 : h2;
    // center
    GPoint c = {
        bounds.origin.x + w2,
        bounds.origin.y + h2,
    };
    // accel data scaled to radius
    int16_t ax = (int)g.accel.x * r / 1000;
    int16_t ay = (int)g.accel.y * r / 1000;
    // point to draw line to
    GPoint p = { c.x + ax, c.y - ay };
    // clear layer
    graphics_context_set_fill_color(ctx, BGCOL);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
    // draw line
    graphics_context_set_stroke_color(ctx, FGCOL);
    graphics_draw_line(ctx, c, p);
}

static void accel_handler(AccelRawData *data, uint32_t num, uint64_t ts)
{
    if (num < 1) return;
    g.accel = *data;
    layer_mark_dirty(window_get_root_layer(g.window));
}

static void window_load(Window *window)
{
    Layer *window_layer = window_get_root_layer(window);
    layer_set_update_proc(window_layer, redraw);

    accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
    accel_raw_data_service_subscribe(1, accel_handler);
}

static void window_unload(Window *window)
{
    accel_data_service_unsubscribe();
}

static void init()
{
    g.window = window_create();
    window_set_window_handlers(g.window,
                               (WindowHandlers){
                                   .load = window_load, .unload = window_unload,
                               });
    window_stack_push(g.window, true);
}

static void deinit()
{
    window_destroy(g.window);
}

int main(void)
{
    init();
    app_event_loop();
    deinit();
}
