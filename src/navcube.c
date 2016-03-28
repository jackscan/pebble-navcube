#include <pebble.h>

#define BGCOL GColorDarkGreen
#define FGCOL GColorWhite

#define VEC3_NORM_LEN 1000

typedef union {
    struct {
        int32_t x, y, z;
    };
    int32_t d[3];
} vec3;

struct
{
    Window *window;
    vec3 accel;
    struct
    {
        int32_t da;
        int32_t last_angle;
        uint16_t dt;
        uint16_t last_time;
    } hdg;
} g;

static int32_t sqrti(int32_t i)
{
    int32_t r = 0;
    int32_t n = 1 << 30;

    if (i < 0) return 0;
    while (n > i) n /= 4;

    while (n != 0)
    {
        int32_t k = n + r;
        if (k <= i)
        {
            i -= k;
            r = r / 2 + n;
        }
        else
            r /= 2;
        n /= 4;
    }
    return r;
}

static inline int32_t vec3_sqrlen(vec3 *v)
{
    int32_t len2 = 0;
    for (int i = 0; i < 3; ++i) len2 += v->d[i] * v->d[i];
    return len2;
}

static inline void vec3_div(vec3 *v, int32_t d)
{
    for (int i = 0; i < 3; ++i) v->d[i] /= d;
}

static inline void vec3_add(vec3 *restrict a, vec3 *restrict b)
{
    for (int i = 0; i < 3; ++i) a->d[i] += b->d[i];
}

static inline void vec3_sub(vec3 *restrict a, vec3 *restrict b)
{
    for (int i = 0; i < 3; ++i) a->d[i] -= b->d[i];
}

static inline void vec3_normalize(vec3 *v)
{
    int32_t len2 = vec3_sqrlen(v);
    // TODO: handle overflow and zero
    if (len2 <= 0) return;
    int32_t len = sqrti(len2);
    for (int i = 0; i < 3; ++i) v->d[i] = VEC3_NORM_LEN * v->d[i] / len;
}

static inline void vec3_cross(vec3 *restrict c,
                              vec3 *restrict a, vec3 *restrict b)
{
    c->x = a->y * b->z - a->z * b->y;
    c->y = a->z * b->x - a->x * b->z;
    c->z = a->x * b->y - a->y * b->x;
}

static inline GPoint vec3_project(vec3 *v, GPoint center, int16_t scale)
{
    return GPoint(center.x + v->x * scale / (v->z / 4 + VEC3_NORM_LEN),
                  center.y - v->y * scale / (v->z / 4 + VEC3_NORM_LEN));
}

static inline void vec3_draw_line(vec3 *a, vec3 *b, GPoint c, int16_t s,
                                  GContext *ctx)
{
    graphics_draw_line(ctx, vec3_project(a, c, s), vec3_project(b, c, s));
}

static inline int32_t timediff_ms(uint16_t a, uint16_t b)
{
    return a > b ? a - b : a + 1000 - b;
}

static inline int32_t interpolate_heading(uint16_t t)
{
    int32_t dt = timediff_ms(t, g.hdg.last_time);
    if (g.hdg.dt == 0) g.hdg.dt = 1;
    if (dt > g.hdg.dt) dt = g.hdg.dt;
    int32_t a = g.hdg.da * dt / (int32_t)g.hdg.dt + g.hdg.last_angle;
    return a < 0 ? a + TRIG_MAX_ANGLE
                 : a >= TRIG_MAX_ANGLE ? a - TRIG_MAX_ANGLE : a;
}

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

    int32_t a = interpolate_heading(time_ms(NULL, NULL));
    int32_t cosa = sin_lookup(a);
    int32_t sina = cos_lookup(a);
    vec3 d[3];
    d[2] = g.accel;
    vec3_normalize(d + 2);
    int32_t cosax = (d[2].x * (TRIG_MAX_RATIO - cosa)) / TRIG_MAX_RATIO;
    d[0] = (vec3){ {
        cosax * d[2].x / VEC3_NORM_LEN + VEC3_NORM_LEN * cosa / TRIG_MAX_RATIO,
        cosax * d[2].y / VEC3_NORM_LEN + d[2].z * sina / TRIG_MAX_RATIO,
        cosax * d[2].z / VEC3_NORM_LEN - d[2].y * sina / TRIG_MAX_RATIO,
    } };

    vec3_cross(d + 1, d + 0, d + 2);
    vec3_div(d + 1, VEC3_NORM_LEN);
    vec3_cross(d + 0, d + 2, d + 1);
    vec3_div(d + 0, VEC3_NORM_LEN);
    vec3_normalize(d + 0);
    vec3_normalize(d + 1);

    vec3 p = {}, q = {};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
        {
            p.d[i] -= d[j].d[i] / 2;
            q.d[i] += d[j].d[i] / 2;
        }

    // clear layer
    graphics_context_set_fill_color(ctx, BGCOL);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    // draw lines
    graphics_context_set_stroke_color(ctx, FGCOL);
    for (int i = 0; i < 3; ++i)
    {
        vec3 s = p;
        vec3 t = q;
        vec3_add(&s, d + i);
        vec3_sub(&t, d + i);
        vec3 u = s;
        vec3 v = s;
        vec3_add(&u, d + ((i + 1) % 3));
        vec3_add(&v, d + ((i + 2) % 3));
        vec3_draw_line(&p, &s, c, r, ctx);
        vec3_draw_line(&s, &u, c, r, ctx);
        vec3_draw_line(&s, &v, c, r, ctx);
        vec3_draw_line(&q, &t, c, r, ctx);
    }
}

static void accel_handler(AccelRawData *data, uint32_t num, uint64_t ts)
{
    if (num < 1) return;
    g.accel.x = data->x;
    g.accel.y = data->y;
    g.accel.z = -data->z;

    layer_mark_dirty(window_get_root_layer(g.window));
}

static void compass_handler(CompassHeadingData heading)
{
    uint16_t ms = time_ms(NULL, NULL);
    int32_t curr = interpolate_heading(ms);
    int32_t next = heading.true_heading;
    g.hdg.dt = timediff_ms(ms, g.hdg.last_time);
    g.hdg.last_time = ms;
    g.hdg.last_angle = curr;
    g.hdg.da = next - curr;
    int32_t pia = TRIG_MAX_ANGLE / 2;
    if (g.hdg.da < -pia) g.hdg.da += TRIG_MAX_ANGLE;
    if (g.hdg.da > pia) g.hdg.da -= TRIG_MAX_ANGLE;
}

static void window_load(Window *window)
{
    Layer *window_layer = window_get_root_layer(window);
    layer_set_update_proc(window_layer, redraw);

    accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
    accel_raw_data_service_subscribe(1, accel_handler);

    compass_service_set_heading_filter(0);
    compass_service_subscribe(compass_handler);
}

static void window_unload(Window *window)
{
    accel_data_service_unsubscribe();
    compass_service_unsubscribe();
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
