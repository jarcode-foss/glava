src = $(wildcard *.c)
obj = $(src:.c=.o)

LDFLAGS = -lpulse -lpulse-simple -pthread -lOpenGL -lglfw -ldl -lm -lX11

PYTHON = python

GLAD_INSTALL_DIR = glad
GLAD_SRCFILE = ./glad/src/glad.c
GLAD_ARGS_RELEASE = --generator=c --extensions=GL_EXT_framebuffer_multisample,GL_EXT_texture_filter_anisotropic
GLAD_ARGS_DEBUG = --generator=c-debug --extensions=GL_EXT_framebuffer_multisample,GL_EXT_texture_filter_anisotropic
CFLAGS_COMMON = -I glad/include

CFLAGS_USE = $(CFLAGS_COMMON) $(CFLAGS)

all: glad glava

%.o: %.c
	$(CC) $(CFLAGS_USE) -o $@ -c $<

glava: $(obj)
	$(CC) -o $@ $^ glad.o $(LDFLAGS)

.PHONY: glad
glad:
	cd $(GLAD_INSTALL_DIR) && $(PYTHON) -m glad $(GLAD_ARGS_RELEASE) --out-path=.
	$(CC) $(CFLAGS_USE) -o glad.o $(GLAD_SRCFILE) -c

.PHONY: glad-debug
glad-debug:
	cd $(GLAD_INSTALL_DIR) && $(PYTHON) -m glad $(GLAD_ARGS_DEBUG) --out-path=.
	$(CC) $(CFLAGS_USE) -o glad.o $(GLAD_SRCFILE) -c

.PHONY: clean
clean:
	rm -f $(obj) glava glad.o

CFLAGS = -ggdb
