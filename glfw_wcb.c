
/* GLFW window and OpenGL context creation. */

#ifdef GLAVA_GLFW

#define GLAVA_RDX11

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <X11/Xlib.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "render.h"
#include "xwin.h"

#define GLFW_EXPOSE_NATIVE_X11

/* Hack to make GLFW 3.1 headers work with GLava. We don't use the context APIs from GLFW, but
   the old headers require one of them to be selected for exposure in glfw3native.h. */
#if GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR <= 1
#define GLFW_EXPOSE_NATIVE_GLX
#endif
#include <GLFW/glfw3native.h>

/* Fixes for old GLFW versions */
#ifndef GLFW_TRUE
#define GLFW_TRUE GL_TRUE
#endif
#ifndef GLFW_FALSE
#define GLFW_FALSE GL_FALSE
#endif

#define DECL_WINDOW_HINT(F, H) \
    static void F(bool var) { glfwWindowHint(H, var); }
#define DECL_WINDOW_HINT_STUB(F) \
    static void F(bool _) { fprintf(stderr, "Warning: " #F " not implemented for GLFW backend\n"); }

static void init(void) {
    if (!glfwInit()) {
        fprintf(stderr, "glfwInit(): failed\n");
        abort();
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
}


DECL_WINDOW_HINT(set_floating,  GLFW_FLOATING);
DECL_WINDOW_HINT(set_decorated, GLFW_DECORATED);
DECL_WINDOW_HINT(set_focused,   GLFW_FOCUSED);
#ifdef GLFW_MAXIMIZED
DECL_WINDOW_HINT(set_maximized, GLFW_MAXIMIZED);
#else
DECL_WINDOW_HINT_STUB(set_maximized);
#endif

extern struct gl_wcb wcb_glfw;

static void* create_and_bind(const char* name, const char* class,
                             const char* type, const char** states,
                             size_t states_sz,
                             int d, int h,
                             int x, int y,
                             int version_major, int version_minor,
                             bool clickthrough) {

    GLFWwindow* w;
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, version_major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, version_minor);
    
    if (!(w = glfwCreateWindow(d, h, class, NULL, NULL))) {
        fprintf(stderr, "glfwCreateWindow(): failed\n");
        glfwTerminate();
        return NULL;
    }
    
    if (type)
        xwin_settype(&wcb_glfw, w, type);

    for (size_t t = 0; t < states_sz; ++t)
        xwin_addstate(&wcb_glfw, w, states[t]);
    
    glfwSetWindowPos(w, x, y);
    glfwMakeContextCurrent(w);
    
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    
    return w;
}

static void set_transparent(bool transparent) {
    #ifdef GLFW_TRANSPARENT_FRAMEBUFFER
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, transparent ? GLFW_TRUE : GLFW_FALSE);
    #elif GLFW_TRANSPARENT
    glfwWindowHint(GLFW_TRANSPARENT, transparent ? GLFW_TRUE : GLFW_FALSE);
    #else
    if (transparent)
        fprintf(stderr, "Warning: the linked version of GLFW3 does not have transparency support"
               " (GLFW_TRANSPARENT[_FRAMEBUFFER])!\n");
    #endif
}

static void set_geometry(GLFWwindow* w, int x, int y, int d, int h) {
    glfwSetWindowPos(w, x, y);
    glfwSetWindowSize(w, d, h);
}

static void set_visible(GLFWwindow* w, bool visible) {
    if (visible) glfwShowWindow(w);
    else         glfwHideWindow(w);
}

static void swap_buffers(GLFWwindow* w) {
    glfwSwapBuffers(w);
    glfwPollEvents();
}
    
static Display* get_x11_display(void)                          { return glfwGetX11Display();      }
static Window   get_x11_window (GLFWwindow* w)                 { return glfwGetX11Window(w);      }
static bool     should_close   (GLFWwindow* w)                 { return glfwWindowShouldClose(w); }
static bool     should_render  (GLFWwindow* w)                 { return true; }
static bool     bg_changed     (GLFWwindow* w)                 { return false; }
static void     get_fbsize     (GLFWwindow* w, int* d, int* h) { glfwGetFramebufferSize(w, d, h); }
static void     get_pos        (GLFWwindow* w, int* x, int* y) { glfwGetWindowPos(w, x, y);       }
static double   get_time       (GLFWwindow* w)                 { return glfwGetTime();            }
static void     set_time       (GLFWwindow* w, double time)    { glfwSetTime(time);               }
static void     set_swap       (int i)                         { glfwSwapInterval(i); }
static void     raise          (GLFWwindow* w)                 { glfwShowWindow(w); }
static void     destroy        (GLFWwindow* w)                 { glfwDestroyWindow(w); }
static void     terminate      (void)                          { glfwTerminate(); }

static const char* get_environment(void) { return xwin_detect_wm(&wcb_glfw); }

WCB_ATTACH("glfw", wcb_glfw);

#endif /* GLAVA_GLFW */
