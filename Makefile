.DEFAULT_GOAL := ALL

bin = connect-wayland list-interface wl-shell-window xdg-window

ALL: $(bin)

wayland-protocols:
	mkdir wayland-protocols

wayland-protocols/%.c: wayland-protocols
	wayland-scanner private-code \
		< $(shell find /usr/share/wayland-protocols/ -name $*.xml) \
		> $@

wayland-protocols/%-client.h: wayland-protocols
	wayland-scanner client-header \
		< $(shell find /usr/share/wayland-protocols/ -name $*.xml) \
		> $@

connect-wayland: connect-wayland.c
	gcc -o connect-wayland connect-wayland.c -lwayland-client -lwayland-server -lwayland-cursor -lwayland-egl

list-interface: list-interface.c
	gcc -o list-interface list-interface.c -lwayland-client -lwayland-server -lwayland-cursor -lwayland-egl

wl-shell-window: wl-shell-window.c
	gcc -g -o wl-shell-window wl-shell-window.c -lwayland-client -lwayland-server -lwayland-cursor -lwayland-egl

xdg-window: xdg-window.c \
	wayland-protocols/xdg-shell.c wayland-protocols/xdg-shell-client.h \
	wayland-protocols/xdg-decoration-unstable-v1.c wayland-protocols/xdg-decoration-unstable-v1-client.h
	gcc -g -o xdg-window xdg-window.c \
		wayland-protocols/xdg-shell.c \
		wayland-protocols/xdg-decoration-unstable-v1.c \
		-lwayland-client -lwayland-server -lwayland-cursor -lwayland-egl

clean:
	rm $(bin)
	rm -rf wayland-protocols
