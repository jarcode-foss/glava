
#ifndef UI_H
#define UI_H
#ifdef GLAVA_UI

struct char_cache;

struct cm_info {
    uint8_t xb, yb; /* advance       */
    uint8_t h, w;   /* height, width */
    int8_t xt, yt;  /* translation   */
};

#ifndef RENDER_H
struct renderer; /* define incomplete type */
#endif

void ui_init(struct renderer* r);

uint32_t utf8_codepoint(const char* str);
size_t   utf8_next     (const char* str);

struct font_info {
    uint8_t ascender, /* distance between baseline and highest glyph */
        descender,    /* distance between baseline and lowest glyph */
        height,       /* distance between baselines */
        max_advance;  /* maximum advance */
};

/*
  Iterate over a C char string 'str', setting the start of each UTF-8
  sequence to 'p'.
*/
#define utf8_iter(str, p)                               \
    typeof(str) p = str; *p != '\0'; p += utf8_next(p)

#define utf8_iter_s(str, len, p)                                        \
    typeof(str) p = str; (uintptr_t) p - (uintptr_t) str < len; p += utf8_next(p)

/*
  Same as above, but breaks on invalid character start and stores the
  size of the current character in 'n'.
*/
#define utf8_iter_n(str, p, n)                              \
    typeof(str) p;                                          \
    size_t n = utf8_next(str);                              \
    for (p = str; n && *p != '\0'; p += (n = utf8_next(p)))

struct color {
    float r, g, b, a;
};

struct geometry {
    int32_t x, y, w, h;
};

struct position {
    int32_t x, y;
};

struct box_data {
    struct geometry geometry;  /* box position and size                           */
    struct position tex_pos;   /* position of the element to use in box->tex      */
    struct color    tex_color; /* added texture color (or flat color if tex == 0) */
    uint32_t tex;              /* texture to use, 0 to ignore                     */
    uint32_t vbuf, vao;        /* internal, set to 0 on creation                  */
};

struct text_data {
    struct position  position;  /* baseline position  */
    struct box_data* boxes;     /* internal           */
    size_t           num_boxes; /* internal           */
};

struct layer_data {
    uint32_t tex;
    uint32_t fbo;
    struct box_data frame;

    /* callbacks for when the layer is repainted */
    void* udata;
    void (*render_cb)(void* udata);
    void (*font_cb)  (void* udata);
};

void ui_box                (struct box_data* d);
void ui_box_release        (struct box_data* d);
void ui_box_draw           (const struct box_data* d);

void ui_layer              (struct layer_data* d, uint32_t w, uint32_t h);
void ui_layer_draw_contents(struct layer_data* d);
void ui_layer_draw         (struct layer_data* d);
void ui_layer_resize       (struct layer_data* d, uint32_t w, uint32_t h);
void ui_layer_release      (struct layer_data* d);

int32_t ui_get_advance_for (uint32_t cp);

void ui_text_char          (struct text_data* d, size_t n, uint32_t cp, uint32_t off, struct color color);
void ui_text_size          (struct text_data* d, size_t s);
void ui_text_draw          (const struct text_data* d);
void ui_text_contents      (struct text_data* d, const char* str, size_t str_len, struct color color);
void ui_text_set_pos       (struct text_data* d, struct position p);
void ui_text_release       (struct text_data* d);

struct char_cache* ui_load_font  (const char* path, int32_t size);
void               ui_select_font(struct char_cache*);

#endif /* GLAVA_UI */
#endif /* UI_H */
