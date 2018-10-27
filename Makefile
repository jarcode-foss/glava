src = $(wildcard *.c)
obj = $(src:.c=.o)

# Build type parameter

ifeq ($(BUILD),debug)
    CFLAGS_BUILD = -O0 -ggdb -Wall -DGLAVA_DEBUG
    GLAD_GEN = c-debug
	STRIP_CMD = $(info Skipping `strip` for debug builds)
else
    CFLAGS_BUILD = -O2 -Wstringop-overflow=0
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

ifndef EXECDIR
    EXECDIR = /usr/bin/
endif

# Install type parameter

ifeq ($(INSTALL),standalone)
    CFLAGS_INSTALL = -DGLAVA_STANDALONE
endif

ifeq ($(INSTALL),unix)
    CFLAGS_INSTALL = -DGLAVA_UNIX
    ifndef SHADERDIR
        ifdef XDG_CONFIG_DIRS
            SHADERDIR = /$(firstword $(subst :, ,$(XDG_CONFIG_DIRS)))/glava/
        else
            SHADERDIR = /etc/xdg/glava/
        endif
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
    ifndef SHADERDIR
        SHADERDIR = /Library/glava
    endif
endif

LDFLAGS += $(ASAN) -lpulse -lpulse-simple -pthread $(LDFLAGS_GLFW) -ldl -lm -lX11 -lXext $(LDFLAGS_GLX)

PYTHON = python

GLAVA_VERSION = \"$(shell git describe --tags 2>/dev/null)\"
ifeq ($(GLAVA_VERSION),\"\")
    GLAVA_VERSION = \"unknown\"
endif

ifdef DESTDIR
	DESTDIR += /
endif

GLAD_INSTALL_DIR = glad
GLAD_SRCFILE = glad.c
GLAD_ARGS = --generator=$(GLAD_GEN) --extensions=GL_EXT_framebuffer_multisample,GL_EXT_texture_filter_anisotropic
CFLAGS_COMMON = -DGLAVA_VERSION="$(GLAVA_VERSION)" -DSHADER_INSTALL_PATH="\"$(SHADERDIR)\""
CFLAGS_USE = $(CFLAGS_COMMON) $(CFLAGS_GLX) $(CFLAGS_GLFW) $(CFLAGS_BUILD) $(CFLAGS_INSTALL) $(CFLAGS)

# Store relevant variables that may change depending on the environment or user input
STATE = $(BUILD),$(INSTALL),$(PREFIX),$(ENABLE_GLFW),$(DISABLE_GLX),$(PYTHON),$(CC),$(CFLAGS_USE)
# Only update the file if the contents changed, `make` just looks at the timestamp
$(shell if [[ ! -e build_state ]]; then touch build_state; fi)
$(shell if [ '$(STATE)' != "`cat build_state`" ]; then echo '$(STATE)' > build_state; fi)

all: glava

%.o: %.c build_state
	@$(CC) $(CFLAGS_USE) -o $@ -c $(firstword $<)
	@echo "CC $@"

glava: $(obj)
	@$(CC) -o glava $(obj) $(LDFLAGS)
	@echo "CC glava"
	$(STRIP_CMD)

.PHONY: glad
glad: build_state
	@cd $(GLAD_INSTALL_DIR) && $(PYTHON) -m glad $(GLAD_ARGS) --local-files --out-path=.
	@cp glad/*.h .
	@cp glad/glad.c .

# Empty build state goal, used to force some of the above rules to re-run if `build_state` was updated
build_state: ;

.PHONY: clean
clean:
	rm -f $(obj) glava glad.o build_state

EXECTARGET = $(shell readlink -m "$(DESTDIR)$(EXECDIR)/glava")
SHADERTARGET = $(shell readlink -m "$(DESTDIR)$(SHADERDIR)")

.PHONY: install
install:
	install -Dm755 glava $(EXECTARGET)
	install -d $(SHADERTARGET)
	cp -Rv shaders/* $(SHADERTARGET)

.PHONY: uninstall
uninstall:
	rm $(EXECTARGET)
	rm -rf $(SHADERTARGET)

