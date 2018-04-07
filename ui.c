#ifdef GLAVA_UI

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <glad/glad.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#define GLAVA_RD_INTERNAL
#include "render.h"

#include "ui.h"

#define UI_TEXTURE_SLOT 4

/*
  FONT RENDERING: We take the codepoint from each UTF-8 character, then split it
  x, y, and t components. The x and y components are used to index a character
  texture map, and the t component indexes from an array of texture maps. The
  bits that these components use depends on the resolution of the character maps
  (char_map_res, char_map_bits).
  
  Each character has a fixed space to render in. Fixed space for each character
  avoids packing issues, but has the downside of wasting VRAM.
  
  With a 32x32 character space, a completely filled set of maps will consume
  approximately 1G of VRAM, so it's ideal to limit the ranges of characters
  that can be used (language settings).
*/

/* UTF-8 code point range */
#define CM_MAX 1114112
/* character texture space, must be power of 2 */
#define CM_CHAR_SPACE 32

/* create mask of S length */
#define CM_MASK(S) (0xFFFFFFFF >> (32 - (S)))
/* bits [0, m) */
#define CM_X(V) \
    (V & (uint32_t) CM_MASK(use_cache->char_map_bits))
/* bits [m, 2m) */
#define CM_Y(V) \
    ((V >> use_cache->char_map_bits) & (uint32_t) CM_MASK(use_cache->char_map_bits))
/* bits [2m, 32) */
#define CM_T(V) \
    ((V >> (2 * use_cache->char_map_bits)) & (uint32_t) CM_MASK(32 - (2 * use_cache->char_map_bits)))

#define CM_XYT(X, Y, T) \
    ((X) | ((Y) << use_cache->char_map_bits) | ((T) << (2 * use_cache->char_map_bits)))

/* amount of texture maps needed (produces integers) */
#define CM_NUM_TEXTURES(C)                                              \
    (CM_MAX / ((C->char_map_res / CM_CHAR_SPACE) * (C->char_map_res / CM_CHAR_SPACE)))

#define CM_TEX_RESERVE 3

/* unsafe round for +'ve values */
#define uround(d, from, to) (d - ((from) ((to) (d))) > 0.5D ? ((to) (d)) + 1 : (to) (d))

/*
  8192 x 8192 and 4096 x 4096 textures are widely supported on most dedicated
  GPUs, however they consume a lot of memory for each map.
  
  The 'bits' field is for the amount of bits from the codepoint we use for
  coordinates. They equal log(res / CM_CHAR_SPACE) / log(2).
*/
static const struct { int res, bits; } char_map_resolutions[] = {
    #ifdef CM_USE_HUGE_MAPS
    { .res = 8192, .bits = 8 }, /* ~ 64MB */
    #endif
    #ifdef CM_USE_LARGE_MAPS
    { .res = 4096, .bits = 7 }, /* ~ 16MB */
    #endif
    { .res = 2048, .bits = 6 }, /* ~ 4MB  */
    { .res = 1024, .bits = 5 }  /* ~ 1MB  */
};

static int char_map_loaded;

static FT_Library ft;

struct char_cache {
    int char_map_res; /* charmap tex resolution in use */
    int char_map_bits;

    GLuint* char_maps;          /* maps CP_T(codepoint) to the corresponding texture */
    struct cm_info** char_info; /* info structure [CM_NUM_TEXTURES][char_map_res ^ 2] */
    volatile bool* char_flags;  /* atomic flags for whether the map has been loaded */

    FT_Face face;
    struct font_info face_info;
};

static struct char_cache* use_cache = NULL;
static bool font_set = false;

/*
  Get the code point from a UTF-8 character, starting at *str.
  THIS FUNCTION IS UNSAFE, strings passed to this should be sanitized,
  otherwise security issues could occur in code using this function.
*/
uint32_t utf8_codepoint(const char* str) {
    if (!(str[0] & 0x80)) return (uint32_t) str[0]; /* ASCII */
    if ((str[0] & 0xE0) == 0xC0) /* & 1110 0000 == 1100 0000 */
        return (uint32_t) ((str[0] & 0x1F) << 6) |
            (uint32_t) ((str[1] & 0x3F) << 0);
    if ((str[0] & 0xF0) == 0xE0) /* & 1111 0000 == 110 0000 */
        return (uint32_t) ((str[0] & 0x0F) << 12) |
            (uint32_t) ((str[1] & 0x3F) << 6) |
            (uint32_t) ((str[2] & 0x3F));
    if ((str[0] & 0xF8) == 0xF0) /* & 1111 1000 == 1111 0000 */
        return (uint32_t) ((str[0] & 0x07) << 18) |
            (uint32_t) ((str[1] & 0x3F) << 12) |
            (uint32_t) ((str[2] & 0x3F) << 6) |
            (uint32_t) ((str[3] & 0x3F) << 0);
    return (uint32_t) ' ';
}

/* returns the size of the next UTF-8 character in a string */
size_t utf8_next(const char* str) {
    if (!(str[0] & 0x80)) return 1;
    if ((str[0] & 0xE0) == 0xC0) return 2;
    if ((str[0] & 0xF0) == 0xE0) return 3;
    if ((str[0] & 0xF8) == 0xF0) return 4;
    return 0; /* invalid encoding, or str pointer is not offset correctly */
}

static struct char_cache* charmap_init(struct char_cache* cache) {
    
    cache->char_map_res = 0;
    
    GLint ts, t;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &ts);
    for (t = 0; t < sizeof(char_map_resolutions) / sizeof(char_map_resolutions[0]); ++t) {
        if (char_map_resolutions[t].res <= ts) {
            cache->char_map_res  = char_map_resolutions[t].res;
            cache->char_map_bits = char_map_resolutions[t].bits;
            break;
        }
    }
    if (!cache->char_map_res) {
        fprintf(stderr, "OpenGL driver does not support texture size (1024 x 1024) or larger");
        abort();
    }
    size_t sz = CM_NUM_TEXTURES(cache) * sizeof(GLuint);
    cache->char_maps = malloc(sz);
    cache->char_info = malloc(sz);
    cache->char_flags = calloc(sz, sizeof(bool));
    memset(cache->char_maps, 0, sz);

    return cache;
}

static void charmap_load(uint32_t t, FT_Face face) {
    if (!use_cache->char_maps[t]) {
        if (char_map_loaded + CM_TEX_RESERVE > GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS) {
            fprintf(stderr, "OpenGL driver does not support more"
                    " than %d active textures, tried to use %d!",
                    GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                    char_map_loaded + CM_TEX_RESERVE);
            abort();
        }
        
        uint32_t
            csz = use_cache->char_map_res / CM_CHAR_SPACE,
            bsz = use_cache->char_map_res * use_cache->char_map_res;
        uint8_t* buf = malloc(bsz);
        use_cache->char_info[t] = malloc(csz * csz * sizeof(struct cm_info));
        memset(buf, 0, bsz);
        
        printf("loading character map: %d (%d x %d), range: 0x%08X -> 0x%08X\n",
               t, use_cache->char_map_res, use_cache->char_map_res, CM_XYT(0, 0, t),
               CM_XYT(csz - 1, csz - 1, t));

        /* load each glyph in the charmap */
        uint32_t x, y, bx, by, idx;
        for (y = 0; y < csz; ++y) {
            for (x = 0; x < csz; ++x) {
                FT_ULong cp = (FT_ULong) (uint32_t) CM_XYT(x, y, t);
                if (FT_Load_Char(face, cp, FT_LOAD_RENDER))
                    continue;
                FT_GlyphSlot g = face->glyph;
                idx = (y * csz) + x;
                bx = x * CM_CHAR_SPACE;
                by = y * CM_CHAR_SPACE;
                if (g->bitmap.width > CM_CHAR_SPACE) g->bitmap.width = CM_CHAR_SPACE;
                if (g->bitmap.rows > CM_CHAR_SPACE) g->bitmap.rows = CM_CHAR_SPACE;
                
                double dx = g->advance.x / 64.0d;
                double dy = g->advance.y / 64.0d;
                use_cache->char_info[t][idx].xb = uround(dx, double, uint8_t);
                use_cache->char_info[t][idx].yb = uround(dy, double, uint8_t);
                
                bool hor;
                int height = 0, gx, gy;
                
                for (gy = g->bitmap.rows - 1; gy >= 0; --gy) {
                    hor = false;
                    for (gx = 0; gx < g->bitmap.width; ++gx) {
                        uint8_t bit = g->bitmap.buffer[(gy * g->bitmap.width) + gx];
                        buf[((by + (g->bitmap.rows - gy)) * use_cache->char_map_res) + (bx + gx)] = bit;
                        if (bit) hor = true;
                    }
                    if (hor) /* row contains data */
                        height = g->bitmap.rows - gy + 1;
                }
                
                use_cache->char_info[t][idx].h = height;
                use_cache->char_info[t][idx].w = g->bitmap.width;
                use_cache->char_info[t][idx].xt = g->bitmap_left;
                use_cache->char_info[t][idx].yt = g->bitmap_top - g->bitmap.rows;
                // char_info[t][idx].xb = g->bitmap.width;
            }
        }
        glGenTextures(1, &use_cache->char_maps[t]);
        glBindTexture(GL_TEXTURE_2D, use_cache->char_maps[t]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, use_cache->char_map_res, use_cache->char_map_res, 0,
                       GL_RED, GL_UNSIGNED_BYTE, buf);

        /* no filtering */
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        
        /* clamp */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        free(buf);
        ++char_map_loaded;
        __atomic_store_n(use_cache->char_flags + t, true, __ATOMIC_RELAXED);
    }
}

struct tex_uniforms {
    GLuint tex;
    GLuint tex_coords;
    GLuint tex_off;
    GLuint tex_sz;
    GLuint tex_ignore;
    GLuint tex_color;
};

/* Shader program IDs */

static GLuint sh_fontprog; /* for font rendering */
static GLuint sh_normal;   /* for texturing */

/* Font shader uniforms */
static GLuint uf_screen, uf_screen_pos, uf_color;

/* Shader uniforms */
static GLuint u_screen, u_screen_pos;

struct tex_uniforms tu_normal; /* old: tu_init */
struct tex_uniforms tu_font;

static void tuniforms(struct tex_uniforms* tu, GLuint shader) {
    tu->tex        = glGetUniformLocation(shader, "tex");
    tu->tex_coords = glGetUniformLocation(shader, "tex_coords");
    tu->tex_off    = glGetUniformLocation(shader, "tex_off");
    tu->tex_sz     = glGetUniformLocation(shader, "tex_sz");
    tu->tex_ignore = glGetUniformLocation(shader, "tex_ignore");
    tu->tex_color  = glGetUniformLocation(shader, "tex_color");
}

/* (re)build box_data's vertex buffers with its data */
void ui_box(struct box_data* d) {
    /* note: the - 0.001f is to ensure the value is within the last pixel */
    GLfloat buf[18] = {
        [0] = d->geometry.x,                          [1] = d->geometry.y,
        [3] = d->geometry.x + d->geometry.w - 0.001f, [4] = d->geometry.y,
        [6] = d->geometry.x,                          [7] = d->geometry.y + d->geometry.h - 0.001f,
        
        [9]  = d->geometry.x + d->geometry.w - 0.001f, [10] = d->geometry.y + d->geometry.h - 0.001f,
        [12] = d->geometry.x + d->geometry.w - 0.001f, [13] = d->geometry.y,
        [15] = d->geometry.x,                          [16] = d->geometry.y + d->geometry.h - 0.001f
    };
    
    if (!d->vbuf)
        glGenBuffers(1, &d->vbuf);
    glBindBuffer(GL_ARRAY_BUFFER, d->vbuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 18, buf, GL_DYNAMIC_DRAW);

    /* we should only need to bind data to the VAO once */
    if (!d->vao) {
        glGenVertexArrays(1, &d->vao);
        glBindVertexArray(d->vao);
        
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, d->vbuf);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*) 0);
        glDisableVertexAttribArray(0);
        
        glBindVertexArray(0); // note: used _d(...) before
    }
}

/* release resources associated with this box */
void ui_box_release(struct box_data* d) {
    if (d->vbuf) {
        glDeleteBuffers(1, &d->vbuf);
        d->vbuf = 0;
    }
    if (d->vao) {
        glDeleteVertexArrays(1, &d->vao);
        d->vao = 0;
    }
}

/* draw a box represented by 'd', using the texture uniforms specified by 'tu' (if not NULL) */
static void ui_box_render(const struct box_data* d, struct tex_uniforms* tu) {
    glBindVertexArray(d->vao);
    if (tu) {
        if (d->tex) {
            glActiveTexture(GL_TEXTURE0 + UI_TEXTURE_SLOT);
            glBindTexture(GL_TEXTURE_2D, d->tex);
            glUniform1i(tu->tex, UI_TEXTURE_SLOT);
            
            GLint tex_w, tex_h;
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tex_w);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &tex_h);
            
            glUniform2i(tu->tex_coords, d->tex_pos.x, d->tex_pos.y);
            glUniform2i(tu->tex_off, d->geometry.x, d->geometry.y);
            glUniform2i(tu->tex_sz, tex_w, tex_h);
        }
        glUniform1i(tu->tex_ignore, !d->tex);
        glUniform4f(tu->tex_color,
                    d->tex_color.r * d->tex_color.a,
                    d->tex_color.g * d->tex_color.a,
                    d->tex_color.b * d->tex_color.a, d->tex_color.a);
    }
    
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    
    glBindVertexArray(0);
}

void ui_box_draw(const struct box_data* d) { ui_box_render(d, &tu_normal); }

void ui_layer(struct layer_data* d, uint32_t w, uint32_t h) {
    glGenFramebuffers(1, &d->fbo);
    glGenTextures(1, &d->frame.tex);
    setup_fbo_tex(d->fbo, d->frame.tex, w, h);
    d->frame.geometry  = (struct geometry) { .x = d->frame.geometry.x, .y = d->frame.geometry.y,
                                             .w = w, .h = h };
    d->frame.tex_pos   = (struct position) { .x = 0, .y = 0                                   };
    d->frame.tex_color = (struct color   ) { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 0.0f       };
    ui_box(&d->frame);
}

void ui_layer_release(struct layer_data* d) {
    glDeleteTextures(1, &d->frame.tex);
    glDeleteFramebuffers(1, &d->fbo);
    ui_box_release(&d->frame);
}

void ui_layer_resize(struct layer_data* d, uint32_t w, uint32_t h) {
    glBindTexture(GL_TEXTURE_2D, d->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
    d->frame.geometry.w = w;
    d->frame.geometry.h = h;
    ui_box(&d->frame);
}

void ui_layer_draw(struct layer_data* d) {
    
    glUseProgram(sh_normal);
    
    glUniform2i(u_screen, d->frame.geometry.w, d->frame.geometry.h);
    glUniform2i(u_screen_pos, 0, 0);
    
    /*
      We need to fix tex_color to read 0 values, to avoid color masking (we use
      the value as the clear color for the framebuffer)
    */
    struct color tmp_color = d->frame.tex_color;
    d->frame.tex_color = (struct color) { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 0.0f };
    ui_box_draw(&d->frame);
    d->frame.tex_color = tmp_color;
}

void ui_layer_draw_contents(struct layer_data* d) {
    
    /* clear to the frame color */
    GLint old;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old);
    glBindFramebuffer(GL_FRAMEBUFFER, d->fbo);
    glClearColor(d->frame.tex_color.r, d->frame.tex_color.g, d->frame.tex_color.b, d->frame.tex_color.a);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, d->frame.geometry.w, d->frame.geometry.h);
    
    /* stage: UI (screen positioning) */
    
    glUseProgram(sh_normal);
    
    glUniform2i(u_screen, d->frame.geometry.w, d->frame.geometry.h);
    glUniform2i(u_screen_pos, 0, 0);
    
    if (d->render_cb) d->render_cb(d->udata);
    
    /* stage: UI font (screen positioning) */
    
    glUseProgram(sh_fontprog);
    
    glUniform2i(uf_screen, d->frame.geometry.w, d->frame.geometry.h);
    glUniform3f(uf_color, 1.0f, 1.0f, 1.0f);
    glUniform2i(uf_screen_pos, 0, 0);
    
    if (d->font_cb) d->font_cb(d->udata);
    
    glBindFramebuffer(GL_FRAMEBUFFER, old > 0 ? (GLuint) old : 0); /* restore old framebuffer */
    
    // glUseProgram(sh_init);
    /* and ensure the viewport is set correctly (OLD) */
    // glViewport(0, 0, ws, hs);
    // glUniform2i(u_screen, ws, hs);
}

/* release all resources associated with the given text_data */
void ui_text_release(struct text_data* d) {
    if (d->boxes) {
        size_t t;
        for (t = 0; t < d->num_boxes; ++t)
            ui_box_release(&d->boxes[t]);
        free(d->boxes);
        d->boxes = NULL;
        d->num_boxes = 0;
    }
}

/* positions are set by 'shifting' all the underlying boxes */
void ui_text_set_pos(struct text_data* d, struct position p) {
    size_t t;
    for (t = 0; t < d->num_boxes; ++t) {
        struct box_data* b = d->boxes + t;
        b->geometry.x += p.x - d->position.x;
        b->geometry.y += p.y - d->position.y;
        ui_box(b);
    }
    d->position = p;
}

int32_t ui_get_advance_for(uint32_t cp) {
    uint32_t x = CM_X(cp), y = CM_Y(cp), t = CM_T(cp);
    uint32_t idx = ((y * (use_cache->char_map_res / CM_CHAR_SPACE)) + x);
    
    charmap_load(t, use_cache->face);
    
    return (int32_t) use_cache->char_info[t][idx].xb;
}

/* (re)build a specific character (with codepoint) */
void ui_text_char(struct text_data* d, size_t n, uint32_t cp, uint32_t off,
                            struct color color) {
    uint32_t x = CM_X(cp), y = CM_Y(cp), t = CM_T(cp);
    uint32_t idx = ((y * (use_cache->char_map_res / CM_CHAR_SPACE)) + x);
    
    charmap_load(t, use_cache->face);
        
    struct cm_info* i = &use_cache->char_info[t][idx];
        
    struct box_data* bd = &d->boxes[n];
    bd->geometry.x = d->position.x + off + i->xt;
    bd->geometry.y = d->position.y + i->yt;
    bd->geometry.w = i->w;
    bd->geometry.h = i->yb + i->h;
    bd->tex_pos.x = CM_CHAR_SPACE * x;
    bd->tex_pos.y = CM_CHAR_SPACE * y;
    bd->tex = use_cache->char_maps[t];
    bd->tex_color = color;
    ui_box(bd);
}

/* resize the underlying box array for 's' amount of characters */
void ui_text_size(struct text_data* d, size_t s) {
    size_t t;
    if (!d->boxes) {
        d->num_boxes = s;
        d->boxes = malloc(sizeof(struct box_data) * s);
        /* initialize */
        for (t = 0; t < s; ++t) {
            d->boxes[t].vao  = 0;
            d->boxes[t].vbuf = 0;
        }
    } else {
        /* release box resources if the new size is shorter */
        for (t = s; t < d->num_boxes; ++t) {
            ui_box_release(&d->boxes[t]);
        }
        /* realloc to new size */
        d->boxes = realloc(d->boxes, sizeof(struct box_data) * s);
        /* initialize new memory (the rest of the elements will be set later) */
        for (t = d->num_boxes; t < s; ++t) {
            d->boxes[t].vao  = 0;
            d->boxes[t].vbuf = 0;
        }
        d->num_boxes = s;
    }
}

void ui_text_contents(struct text_data* d, const char* str, size_t str_len, struct color color) {
    size_t cp_len = 0;
    for (utf8_iter_s(str, str_len, _))
        cp_len++;
    
    ui_text_size(d, cp_len);

    size_t   idx = 0;
    uint32_t adv = 0;
    for (utf8_iter_s(str, str_len, c)) {
        ui_text_char(d, idx, (uint32_t) *c, adv, color);
        adv += ui_get_advance_for((uint32_t) *c);
        idx++;
    }
}

/* draw text represented by 'd' */
void ui_text_draw(const struct text_data* d) {
    size_t t;
    for (t = 0; t < d->num_boxes; ++t) {
        ui_box_render(&d->boxes[t], &tu_font);
    }
}

struct char_cache* ui_load_font(const char* path, int32_t size) {

    FT_Face face;
    
    if (FT_New_Face(ft, path, 0, &face)) {
        fprintf(stderr, "Could not load font: `%s` (size %d)\n", path, (int) size);
        return NULL;
    }

    struct char_cache* cache = charmap_init(malloc(sizeof(struct char_cache)));
    cache->face = face;
    
    FT_Set_Pixel_Sizes(face, 0, size);
    
    double a = (face->size->metrics.ascender / 64.0d);
    double d = -(face->size->metrics.descender / 64.0d);
    double h = (face->size->metrics.height / 64.0d);
    double m = (face->size->metrics.max_advance / 64.0d);
    
    cache->face_info = (struct font_info) {
        .ascender    = uround(a, double, uint8_t), 
        .descender   = uround(d, double, uint8_t),
        .height      = uround(h, double, uint8_t),
        .max_advance = uround(m, double, uint8_t)
    };

    return cache;
}

void ui_select_font(struct char_cache* cache) {
    use_cache = cache;
    font_set = true;
}

/* vertex shader, translates from screen coords to OpenGL [-1.0, 1.0] bounds */
static const char* src_translate_vert =
    "uniform ivec2 screen;"                                                             "\n"
    "uniform ivec2 screen_pos;"                                                         "\n"
    "layout(location = 0) in vec3 pos;"                                                 "\n"
    "void main() {"                                                                     "\n"
    "    float fpx = pos.x - screen_pos.x;"                                             "\n"
    "    float fpy = pos.y - screen_pos.y;"                                             "\n"
    "    float fsx = screen.x;"                                                         "\n"
    "    float fsy = screen.y;"                                                         "\n"
    "    gl_Position = vec4(((fpx / fsx) * 2F) - 1F, ((fpy / fsy) * 2F) - 1F, 0F, 1F);" "\n"
    "}"                                                                                 "\n";

/* font fragment shader, maps font textures in screen coords */
static const char* src_font_frag =
    "layout(pixel_center_integer) in vec4 gl_FragCoord;"                                                 "\n"
    
    "uniform sampler2D  tex;"        /* texture sheet */                                                 "\n"
    "uniform ivec2      tex_coords;" /* positon of tile in texture */                                    "\n"
    "uniform ivec2      tex_off;"    /* render offset (position in screen space) */                      "\n"
    "uniform ivec2      tex_sz;"     /* texture dimensions */                                            "\n"
    "uniform bool       tex_ignore;"                                                                     "\n"
    "uniform vec4       tex_color;"  /* font color */                                                    "\n"

    "out vec4 fragment;" /* output */

    "void main() {"                                                                                      "\n"
    "    float fx = (float(tex_coords.x + (int(gl_FragCoord.x) - tex_off.x)) + 0.5F) / float(tex_sz.x);" "\n"
    "    float fy = (float(tex_coords.y + (int(gl_FragCoord.y) - tex_off.y)) + 0.5F) / float(tex_sz.y);" "\n"
    "    fragment = vec4(tex_color.rgb, tex_color.a * texture(tex, vec2(fx, fy)).r);"                    "\n"
    "    fragment.rgb *= fragment.a; "                                                                   "\n"
    "}"                                                                                                  "\n";

/* general texturing shader, maps textures in screen coords */
static const char* src_texture_frag =
    "in vec4 gl_FragCoord;"                                                                "\n"
    "uniform sampler2D  tex;"        /* texture sheet */                                   "\n"
    "uniform ivec2      tex_coords;" /* positon of tile in texture */                      "\n"
    "uniform ivec2      tex_off;"    /* render offset (position in screen space) */        "\n"
    "uniform ivec2      tex_sz;"     /* texture dimensions */                              "\n"
    "uniform bool       tex_ignore;"                                                       "\n"
    "uniform vec4       tex_color;"                                                        "\n"

    "out vec4 fragment;" /* output */                                                      "\n"

    "void main() {"                                                                        "\n"
    "    if (!tex_ignore) {"                                                               "\n"
    "        float fx = (float(tex_coords.x + (int(gl_FragCoord.x) - tex_off.x)) + 0.5F)"  "\n"
    "                   / float(tex_sz.x);"                                                "\n"
    "        float fy = (float(tex_coords.y + (int(gl_FragCoord.y) - tex_off.y)) + 0.5F)"  "\n"
    "                   / float(tex_sz.y);"                                                "\n"
    "        fragment = clamp(texture(tex, vec2(fx, fy)) + tex_color, 0F, 1F);"            "\n"
    "    } else fragment = tex_color;"                                                     "\n"
    "}"                                                                                    "\n";

void ui_init(struct renderer* r) {
    static bool ft_init = false;
    if (!ft_init) {
        if (FT_Init_FreeType(&ft)) {
            fprintf(stderr, "Could not initializer FreeType library (FT_Init_FreeType)\n");
            abort();
        } else ft_init = true;
    }

    sh_normal = simple_shaderbuild(r, src_translate_vert, src_texture_frag);
    glUseProgram(sh_normal);
    u_screen     = glGetUniformLocation(sh_normal, "screen");
    u_screen_pos = glGetUniformLocation(sh_normal, "screen_pos");
    glBindFragDataLocation(sh_normal, 1, "fragment");
    tuniforms(&tu_normal, sh_normal);
    glUseProgram(0);

    sh_fontprog = simple_shaderbuild(r, src_translate_vert, src_font_frag);
    glUseProgram(sh_fontprog);
    uf_screen     = glGetUniformLocation(sh_fontprog, "screen");
    uf_screen_pos = glGetUniformLocation(sh_fontprog, "screen_pos");
    uf_color      = glGetUniformLocation(sh_fontprog, "font_color");
    glBindFragDataLocation(sh_fontprog, 1, "fragment");
    tuniforms(&tu_font, sh_fontprog);
    glUseProgram(0);
}

#endif /* GLAVA_UI */
