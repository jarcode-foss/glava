src = $(wildcard *.c)
obj = $(src:.c=.o)

# Build type parameter

ifeq ($(BUILD),debug)
    CFLAGS_BUILD = -O0 -ggdb -Wall #-fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls
    GLAD_GEN = c-debug
	STRIP_CMD = $(info Skipping `strip` for debug builds)
#    ASAN = -lasan
else
    CFLAGS_BUILD = -O2 -march=native -Wstringop-overflow=0
    GLAD_GEN = c
	STRIP_CMD = strip --strip-all glava
endif

# Detect OS if not specified (OSX, Linux, BSD are supported)

ifndef INSTALL
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        INSTALL = osx
    else
        INSTALL = unix
    endif
endif

# Install type parameter

ifeq ($(INSTALL),standalone)
    CFLAGS_INSTALL = -DGLAVA_STANDALONE
endif

ifeq ($(INSTALL),unix)
    CFLAGS_INSTALL = -DGLAVA_UNIX
    ifdef XDG_CONFIG_DIRS
        SHADER_DIR = $(firstword $(subst :, ,$(XDG_CONFIG_DIRS)))/glava
    else
        SHADER_DIR = etc/xdg/glava
    endif
endif

ifdef ENABLE_GLFW
    CFLAGS_GLFW = -DGLAVA_GLFW
    LDFLAGS_GLFW = -lglfw
endif

ifndef DISABLE_GLX
    CFLAGS_GLX = -DGLAVA_GLX
    LDFLAGS_GLX = -lXrender
endif

ifeq ($(INSTALL),osx)
    CFLAGS_INSTALL = -DGLAVA_OSX
    SHADER_DIR = Library/glava
endif

LDFLAGS += $(ASAN) -lpulse -lpulse-simple -pthread $(LDFLAGS_GLFW) -ldl -lm -lX11 -lXext $(LDFLAGS_GLX)

PYTHON = python

GLAVA_VERSION = \"$(shell git describe --tags 2>/dev/null)\"
ifeq ($(GLAVA_VERSION),\"\")
    GLAVA_VERSION = \"unknown\"
endif

GLAD_INSTALL_DIR = glad
GLAD_SRCFILE = ./glad/src/glad.c
GLAD_ARGS = --generator=$(GLAD_GEN) --extensions=GL_EXT_framebuffer_multisample,GL_EXT_texture_filter_anisotropic
CFLAGS_COMMON = -I glad/include -DGLAVA_VERSION="$(GLAVA_VERSION)"
CFLAGS_USE = $(CFLAGS_COMMON) $(CFLAGS_GLX) $(CFLAGS_GLFW) $(CFLAGS_BUILD) $(CFLAGS_INSTALL) $(CFLAGS)

# Store relevant variables that may change depending on the environment or user input
STATE = $(BUILD),$(INSTALL),$(ENABLE_GLFW),$(DISABLE_GLX),$(PYTHON),$(CC),$(CFLAGS_USE)
# Only update the file if the contents changed, `make` just looks at the timestamp
$(shell if [[ ! -e build_state ]]; then touch build_state; fi)
$(shell if [ '$(STATE)' != "`cat build_state`" ]; then echo '$(STATE)' > build_state; fi)

all: glava

%.o: %.c glad.o build_state
	$(CC) $(CFLAGS_USE) -o $@ -c $(firstword $<)

glava: $(obj)
	$(CC) -o glava $(obj) glad.o $(LDFLAGS)
	$(STRIP_CMD)

glad.o: build_state
	cd $(GLAD_INSTALL_DIR) && $(PYTHON) -m glad $(GLAD_ARGS) --out-path=.
	$(CC) $(CFLAGS_USE) -o glad.o $(GLAD_SRCFILE) -c

# Empty build state goal, used to force some of the above rules to re-run if `build_state` was updated
build_state: ;

.PHONY: clean
clean:
	rm -f $(obj) glava glad.o build_state

.PHONY: install
install:
	install -Dm755 glava "$(DESTDIR)/usr/bin/glava"
	install -d "$(DESTDIR)/$(SHADER_DIR)"
	cp -Rv shaders/* "$(DESTDIR)/$(SHADER_DIR)"

.PHONY: uninstall
uninstall:
	rm /usr/bin/glava
	rm -rf $(SHADER_DIR)/glava

