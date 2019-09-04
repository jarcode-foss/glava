.PHONY: all install clean ninja

# In case these were specified explicitly as options instead of environment variables, export them to child processes
export DESTDIR
export CFLAGS

BUILD_DIR = build

MESON_CONF = $(BUILD_DIR) -Ddisable_obs=true -Ddisable_config=true --prefix /usr

# Support assigning standalone/debug builds as the old Makefile did, otherwise complain

ifeq ($(BUILD),debug)
    MESON_CONF += --buildtype=debug
else
    ifdef BUILD
        $(warning WARNING: ignoring build option '$(BUILD)' in compatibility Makefile)
    endif
endif

ifeq ($(INSTALL),standalone)
    MESON_CONF += -Dstandalone=true
else
    ifdef INSTALL
        $(warning WARNING: ignoring install option '$(INSTALL)' in compatibility Makefile)
    endif
endif

# Store relevant variables that may change depending on the environment or user input
STATE = $(BUILD),$(INSTALL),$(PYTHON),$(CC),$(CFLAGS),$(DESTDIR)
# Only update the file if the contents changed, `make` just looks at the timestamp
$(shell if [[ ! -e build_state ]]; then touch build_state; fi)
$(shell if [ '$(STATE)' != "`cat build_state`" ]; then echo '$(STATE)' > build_state; fi)

all: ninja

# Rebuild if the makefile state changes to maintain old behaviour and smooth rebuilds with altered parameters
build: build_state
	$(warning !!PACKAGE MAINTAINER NOTICE!!)
	$(warning Configuring build for compatibility with old makefile. Some new features may be missing.)
	$(warning If you are a package maintainer consider using meson directly!)
	@rm -rf $(BUILD_DIR)
	meson $(BUILD_DIR)
	meson configure $(MESON_CONF)

ninja: build
	ninja -C $(BUILD_DIR)

install:
	ninja -C build install

clean:
	rm -rf $(BUILD_DIR)
