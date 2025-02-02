#include "ray.h"

#include <sys/time.h>
#include <wayland-server.h>
#include <z11-server-protocol.h>

#include "cuboid_window.h"
#include "opengl_render_component_back_state.h"
#include "ray_client.h"
#include "seat.h"
#include "types.h"
#include "util.h"

static const char* fragment_shader;
static const char* vertex_shader;

void zazen_ray_notify_motion(struct zazen_ray* ray,
                             struct zazen_ray_motion_event* event)
{
  glm_vec3_add(ray->line.begin, event->begin_delta, ray->line.begin);
  glm_vec3_add(ray->line.end, event->end_delta, ray->line.end);

  zazen_opengl_render_item_set_vertex_buffer(
      ray->render_item, (void*)&ray->line, sizeof(Line), sizeof(vec3));

  zazen_opengl_render_item_commit(ray->render_item);
}

void zazen_ray_notify_button(struct zazen_ray* ray, uint64_t time_usec,
                             int32_t button, enum wl_pointer_button_state state)
{
  struct wl_resource* resource;
  struct zazen_ray_client* ray_client;
  struct zazen_cuboid_window* focus_cuboid_window;
  uint32_t serial;

  focus_cuboid_window = ray->focus_cuboid_window;
  if (focus_cuboid_window == NULL) return;

  serial = wl_display_next_serial(ray->seat->display);

  ray_client = zazen_ray_find_ray_client(
      ray, wl_resource_get_client(focus_cuboid_window->resource));
  wl_resource_for_each(resource, &ray_client->ray_resources)
  {
    z11_ray_send_button(resource, serial, time_usec / 1000, button, state);
  }
}

static void zazen_ray_focus_cuboid_window_destroy_handler(
    struct wl_listener* listener, void* data)
{
  UNUSED(data);
  struct zazen_ray* ray;
  ray = wl_container_of(listener, ray, zazen_cuboid_window_destroy_listener);
  ray->focus_cuboid_window = NULL;
  wl_list_remove(&ray->zazen_cuboid_window_destroy_listener.link);
}

static void zazen_ray_enter(struct zazen_ray* ray,
                            struct zazen_cuboid_window* cuboid_window,
                            struct zazen_ray_half_line local_coord_half_line,
                            float min_distance)
{
  UNUSED(min_distance);
  struct zazen_ray_client* ray_client;
  struct wl_resource* ray_client_resource;
  uint32_t serial;
  zazen_cuboid_window_highlight(cuboid_window);

  ray_client = zazen_ray_find_ray_client(
      ray, wl_resource_get_client(cuboid_window->resource));
  if (ray_client == NULL) return;

  serial = wl_display_next_serial(ray->seat->display);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
  wl_resource_for_each(ray_client_resource, &ray_client->ray_resources)
  {
    z11_ray_send_enter(ray_client_resource, serial, cuboid_window->resource,
                       ((fixed_float)local_coord_half_line.origin[0]).fixed,
                       ((fixed_float)local_coord_half_line.origin[1]).fixed,
                       ((fixed_float)local_coord_half_line.origin[2]).fixed,
                       ((fixed_float)local_coord_half_line.direction[0]).fixed,
                       ((fixed_float)local_coord_half_line.direction[1]).fixed,
                       ((fixed_float)local_coord_half_line.direction[2]).fixed);
  }
#pragma GCC diagnostic pop
}

static void zazen_ray_motion(struct zazen_ray* ray,
                             struct zazen_cuboid_window* cuboid_window,
                             struct zazen_ray_half_line local_coord_half_line,
                             float min_distance)
{
  UNUSED(min_distance);
  struct zazen_ray_client* ray_client;
  struct wl_resource* ray_client_resource;
  struct timeval tv;
  uint32_t current_time_in_milles;

  ray_client = zazen_ray_find_ray_client(
      ray, wl_resource_get_client(cuboid_window->resource));
  if (ray_client == NULL) return;

  gettimeofday(&tv, NULL);
  current_time_in_milles = tv.tv_sec * 1000 + tv.tv_usec / 1000;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
  wl_resource_for_each(ray_client_resource, &ray_client->ray_resources)
  {
    z11_ray_send_motion(
        ray_client_resource, current_time_in_milles,
        ((fixed_float)local_coord_half_line.origin[0]).fixed,
        ((fixed_float)local_coord_half_line.origin[1]).fixed,
        ((fixed_float)local_coord_half_line.origin[2]).fixed,
        ((fixed_float)local_coord_half_line.direction[0]).fixed,
        ((fixed_float)local_coord_half_line.direction[1]).fixed,
        ((fixed_float)local_coord_half_line.direction[2]).fixed);
  }
#pragma GCC diagnostic pop
}

static void zazen_ray_leave(struct zazen_ray* ray,
                            struct zazen_cuboid_window* cuboid_window,
                            struct zazen_ray_half_line local_coord_half_line,
                            float min_distance)
{
  UNUSED(local_coord_half_line);
  UNUSED(min_distance);

  struct zazen_ray_client* ray_client;
  struct wl_resource* ray_client_resource;
  uint32_t serial;
  zazen_cuboid_window_remove_highlight(cuboid_window);

  ray_client = zazen_ray_find_ray_client(
      ray, wl_resource_get_client(cuboid_window->resource));
  if (ray_client == NULL) return;

  serial = wl_display_next_serial(ray->seat->display);
  wl_resource_for_each(ray_client_resource, &ray_client->ray_resources)
  {
    z11_ray_send_leave(ray_client_resource, serial, cuboid_window->resource);
  }
}

void zazen_ray_intersect(struct zazen_ray* ray,
                         struct zazen_cuboid_window* cuboid_window,
                         struct zazen_ray_half_line local_coord_half_line,
                         float min_distance)
{
  if (ray->focus_cuboid_window == cuboid_window) {
    if (cuboid_window == NULL) return;
    zazen_ray_motion(ray, cuboid_window, local_coord_half_line, min_distance);
    return;
  }

  if (ray->focus_cuboid_window) {
    zazen_ray_leave(ray, ray->focus_cuboid_window, local_coord_half_line,
                    min_distance);
    wl_list_remove(&ray->zazen_cuboid_window_destroy_listener.link);
  }

  if (cuboid_window) {
    zazen_ray_enter(ray, cuboid_window, local_coord_half_line, min_distance);
    wl_signal_add(&cuboid_window->destroy_signal,
                  &ray->zazen_cuboid_window_destroy_listener);
  }

  ray->focus_cuboid_window = cuboid_window;

  return;
}

struct zazen_ray_client* zazen_ray_find_ray_client(struct zazen_ray* ray,
                                                   struct wl_client* client)
{
  struct zazen_ray_client* ray_client;

  wl_list_for_each(ray_client, &ray->ray_clients, link)
  {
    if (ray_client->client == client) return ray_client;
  }

  return NULL;
}

struct zazen_ray* zazen_ray_create(struct zazen_seat* seat)
{
  struct zazen_ray* ray;

  ray = zalloc(sizeof *ray);
  if (ray == NULL) return NULL;

  ray->seat = seat;
  ray->focus_cuboid_window = NULL;

  ray->grab = NULL;

  wl_list_init(&ray->ray_clients);
  wl_signal_init(&ray->destroy_signal);

  glm_vec3_copy((vec3){10, -10, 30}, ray->line.begin);
  glm_vec3_copy((vec3){0, 50, 50}, ray->line.end);

  ray->render_item =
      zazen_opengl_render_item_create(seat->render_component_manager);
  if (ray->render_item == NULL) goto out;
  zazen_opengl_render_item_set_vertex_buffer(
      ray->render_item, (void*)&ray->line, sizeof(Line), sizeof(vec3));

  zazen_opengl_render_item_set_shader(ray->render_item, vertex_shader,
                                      fragment_shader);

  zazen_opengl_render_item_set_topology(ray->render_item,
                                        Z11_OPENGL_TOPOLOGY_LINES);

  zazen_opengl_render_item_append_vertex_input_attribute(
      ray->render_item, 0,
      Z11_OPENGL_VERTEX_INPUT_ATTRIBUTE_FORMAT_FLOAT_VECTOR3, 0);

  zazen_opengl_render_item_commit(ray->render_item);

  ray->zazen_cuboid_window_destroy_listener.notify =
      zazen_ray_focus_cuboid_window_destroy_handler;
  wl_list_init(&ray->zazen_cuboid_window_destroy_listener.link);

  return ray;

out:
  free(ray);

  return NULL;
}

void zazen_ray_destroy(struct zazen_ray* ray)
{
  if (ray->grab) ray->grab->interface->cancel(ray->grab);
  wl_signal_emit(&ray->destroy_signal, ray);
  if (ray->render_item) zazen_opengl_render_item_destroy(ray->render_item);
  wl_list_remove(&ray->zazen_cuboid_window_destroy_listener.link);
  free(ray);
}

static const char* vertex_shader =
    "#version 410\n"
    "uniform mat4 mvp;\n"
    "layout(location = 0) in vec4 position;\n"
    "layout(location = 1) in vec2 v2UVcoordsIn;\n"
    "layout(location = 2) in vec3 v3NormalIn;\n"
    "out vec2 v2UVcoords;\n"
    "void main()\n"
    "{\n"
    "  v2UVcoords = v2UVcoordsIn;\n"
    "  gl_Position = mvp * position;\n"
    "}\n";

static const char* fragment_shader =
    "#version 410 core\n"
    "in vec2 v2UVcoords;\n"
    "out vec4 outputColor;\n"
    "void main()\n"
    "{\n"
    "  outputColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
    "}\n";
