.PHONY: all install clean

all:
	meson build
	ninja -C build

install:
	cd build && meson install

clean:
	rm -rf build