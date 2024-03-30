#include <stdio.h>
#include <string.h>

#include <syscall.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>

#include <wayland-client.h>

#include "wayland-protocols/xdg-shell-client.h"
#include "wayland-protocols/xdg-decoration-unstable-v1-client.h"

struct wl_compositor *compositor;
struct wl_shm *shm;
struct xdg_wm_base *wm_base = NULL;
struct zxdg_decoration_manager_v1 *decoration_manager = NULL;

static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

void registry_global_handler(
    void *data,
    struct wl_registry *registry,
    uint32_t name,
    const char *interface,
    uint32_t version
) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        compositor = wl_registry_bind(registry, name,
            &wl_compositor_interface, 3);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        shm = wl_registry_bind(registry, name,
            &wl_shm_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        wm_base = wl_registry_bind(registry, name,
            &xdg_wm_base_interface, 1);
        //xdg_wm_base_add_listener(wm_base, &xdg_wm_base_listener, NULL);
    } else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
        decoration_manager = wl_registry_bind(registry, name,
            &zxdg_decoration_manager_v1_interface, version);
    }
}

void registry_global_remove_handler (
    void *data,
    struct wl_registry *registry,
    uint32_t name
) {}

const struct wl_registry_listener registry_listener = {
    .global = registry_global_handler,
    .global_remove = registry_global_remove_handler
};

struct surface_buffer {
    struct wl_surface *surface;
    struct wl_buffer *buffer;
};

static void xdg_surface_configure(
    void *data,
    struct xdg_surface *xdg_surface,
    uint32_t serial
) {
    fprintf(stderr, "xdg_surface_configure\n");
    struct surface_buffer *sb = data;
    xdg_surface_ack_configure(xdg_surface, serial);

    wl_surface_attach(sb->surface, sb->buffer, 0, 0);
    wl_surface_commit(sb->surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

//toplevel callback
static void toplevel_config(void *data,
              struct xdg_toplevel *xdg_toplevel,
              int32_t width,
              int32_t height,
              struct wl_array *states) {
    fprintf(stderr, "xdg_toplevel_config: width=%d, height=%d\n", width, height);
}

static void xdg_toplevel_handle_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    fprintf(stderr, "xdg_toplevel: hanle close.\n");
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = toplevel_config,
    .close = xdg_toplevel_handle_close,
};

int main(int argc, char** argv)
{
    int no_decor = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "no_decor") == 0) {
            no_decor = 1;
        }
    }

    struct wl_display *display = wl_display_connect(NULL);
    if (display == NULL) {
    	fprintf(stderr, "cannot open wl_display\n");
    	exit(1);
    }
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    // wait for the "initial" set of globals to appear
    wl_display_roundtrip(display);

    if (wm_base == NULL) {
        fprintf(stderr, "error: no xdg_wm_base\n");
        exit(1);
    }

    struct wl_surface *surface = wl_compositor_create_surface(compositor);
    struct xdg_surface *xdg_surface = xdg_wm_base_get_xdg_surface(wm_base, surface);
    struct xdg_toplevel *xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);

    struct surface_buffer sb;

    //add callbacks
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, &sb);
    xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, NULL);

    //set title
    xdg_toplevel_set_title(xdg_toplevel, "Hello, world!");

    if (decoration_manager != NULL) {
        struct zxdg_toplevel_decoration_v1 *win_decor =
            zxdg_decoration_manager_v1_get_toplevel_decoration(decoration_manager, xdg_toplevel);
        // SSD is default if decoration_manager exists
        //zxdg_toplevel_decoration_v1_set_mode(win_decor, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }

    int width = 200;
    int height = 200;
    int stride = width * 4;
    int size = stride * height;  // bytes

    // open an anonymous file and write some zero bytes to it
    int fd = syscall(SYS_memfd_create, "buffer", 0);
    ftruncate(fd, size);

    // map it to the memory
    unsigned char *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    // turn it into a shared memory pool
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);

    // allocate the buffer in that pool
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool,
        0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
    
    sb.surface = surface;
    sb.buffer = buffer;

    //wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);

    while (1) {
        wl_display_dispatch(display);
    }
}

