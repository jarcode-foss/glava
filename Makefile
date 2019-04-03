.PHONY: all install clean

all: ninja

build:
	meson build

ninja: build
	ninja -C build

install:
	cd build && meson install

clean:
	rm -rf build