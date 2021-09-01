#ifndef LIBZAZEN_COMPOSITOR_H
#define LIBZAZEN_COMPOSITOR_H

#include <wayland-server.h>

struct zazen_compositor {
  struct wl_signal frame_signal;
};

struct zazen_compositor* zazen_compositor_create(struct wl_display* display);

void zazen_compositor_destory(struct zazen_compositor* compositor);

#endif  //  LIBZAZEN_COMPOSITOR_H
