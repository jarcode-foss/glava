
struct cm_info {
    uint8_t xb, yb; /* advance       */
    uint8_t h, w;   /* height, width */
    int8_t xt, yt;  /* translation   */
};

void ui_init(void);

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
#define utf8_iter(str, p)                           \
    typeof(str) p;                                  \
    for (p = str; *p != '\0'; p += utf8_next(p))

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
    struct position g_pos;     /* global position (for UI)                        */
    uint32_t tex;              /* texture to use, 0 to ignore                     */
    uint32_t vbuf, vao;        /* internal, set to 0 on creation                  */
};

struct text_data {
    struct position  position;  /* baseline position  */
    struct position  g_pos;     /* global position    */
    struct box_data* boxes;     /* internal           */
    size_t           num_boxes; /* internal           */
};

struct layer_data {
    uint32_t tex;
    uint32_t fbo;
    struct box_data frame;
};

struct tex_uniforms {
    GLuint tex;
    GLuint tex_coords;
    GLuint tex_off;
    GLuint tex_sz;
    GLuint tex_ignore;
    GLuint tex_color;
};

void ui_box(struct box_data* d);
void ui_box_release(struct box_data* d);
void ui_box_draw(const struct box_data* d);
