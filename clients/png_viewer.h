#ifndef Z11_CLIENT_PNG_VIEWER_H
#define Z11_CLIENT_PNG_VIEWER_H

#include <wl_zext_client.h>
#include <z11-client-protocol.h>

#include "png_loader.h"
#include "z_window.h"

typedef struct {
  float x, y, z;
} Point;

typedef struct {
  float u, v;
} UV;

typedef struct {
  Point p;
  UV uv;
} Vertex;

typedef struct {
  Vertex vertices[3];
} Triangle;

typedef struct {
  Triangle triangles[2];
} Panel;

typedef struct {
  uint8_t b, g, r, a;
} ColorBGRA;

typedef struct {
  uint8_t r, g, b;
} ColorRGB;

typedef struct {
  uint8_t r, g, b, a;
} ColorRGBA;

class PngViewer
{
 public:
  explicit PngViewer(const char *filename);
  bool Init();
  void Run();
  struct wl_callback *MainLoop();
  void ResizeEvent(wl_fixed_t width, wl_fixed_t height, wl_fixed_t depth);
  void HandleRayEnter(struct z11_ray *ray, uint32_t serial,
                      struct z11_cuboid_window *cuboid_window,
                      float ray_origin_x, float ray_origin_y,
                      float ray_origin_z, float ray_direction_x,
                      float ray_direction_y, float ray_direction_z);
  void HandleRayMotion(struct z11_ray *ray, uint32_t time, float ray_origin_x,
                       float ray_origin_y, float ray_origin_z,
                       float ray_direction_x, float ray_direction_y,
                       float ray_direction_z);
  void HandleRayLeave(struct z11_ray *ray, uint32_t serial,
                      struct z11_cuboid_window *cuboid_window);
  void HandleRayButton(struct z11_ray *ray, uint32_t serial, uint32_t time,
                       uint32_t button, uint32_t state);

 private:
  const char *filename_;
  float width_;
  float height_;
  float depth_;
  float theta_;
  const char *socket_;
  ZWindow<PngViewer> *zwindow_;
  PngLoader *png_loader_;
  struct z11_virtual_object *virtual_object_;
  Panel *panel_data_;
  ColorBGRA *texture_data_;
  struct wl_zext_raw_buffer *panel_raw_buffer_;
  struct wl_zext_raw_buffer *texture_raw_buffer_;
  struct z11_opengl_texture_2d *texture_;
  struct z11_opengl_vertex_buffer *panel_vertex_buffer_;
  struct z11_opengl_shader_program *shader_program_;
  struct z11_opengl_render_component *render_component_;
  struct z11_cuboid_window *cuboid_window_;
};

#endif  //  Z11_CLIENT_PNG_VIEWER_H
