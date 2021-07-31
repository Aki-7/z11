#include <GL/glew.h>
#include <wayland-server.h>

#include "internal.h"
#include "z11-server-protocol.h"

struct z_render_block {
  struct z_compositor* compositor;
  struct wl_list link;
  GLuint vertex_array_object;
  struct z_render_block_state* current_state;
  struct z_render_block_state* next_state;
};

void z_render_block_destroy(struct z_render_block* render_block);

static void z_render_block_handle_destroy(struct wl_resource* resource)
{
  struct z_render_block* render_block = wl_resource_get_user_data(resource);

  z_render_block_destroy(render_block);
}

static void z_render_block_protocol_destroy(struct wl_client* client, struct wl_resource* resource)
{
  UNUSED(client);
  struct z_render_block* render_block = wl_resource_get_user_data(resource);

  z_render_block_destroy(render_block);
}

static void z_render_block_protocol_attach_vertex_buffer(struct wl_client* client,
                                                         struct wl_resource* resource,
                                                         struct wl_resource* vertex_buffer_resource)
{
  UNUSED(client);
  struct z_render_block* render_block = wl_resource_get_user_data(resource);
  struct z_gl_vertex_buffer* vertex_buffer = wl_resource_get_user_data(vertex_buffer_resource);

  z_render_block_state_attach_vertex_buffer(render_block->next_state, vertex_buffer);
}

static void z_render_block_protocol_commit(struct wl_client* client, struct wl_resource* resource)
{
  UNUSED(client);
  struct z_render_block* render_block = wl_resource_get_user_data(resource);

  z_render_block_state_destroy(render_block->current_state);
  render_block->current_state = render_block->next_state;
  render_block->next_state = z_render_block_state_create();

  struct z_gl_vertex_buffer* vertex_buffer =
      z_render_block_state_get_vertex_buffer(render_block->current_state);

  if (vertex_buffer == NULL) return;

  glBindVertexArray(render_block->vertex_array_object);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer->id);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
  glBindVertexArray(0);
  glDisableVertexAttribArray(0);
}

static const struct z11_render_block_interface z_render_block_interface = {
    .destroy = z_render_block_protocol_destroy,
    .attach_vertex_buffer = z_render_block_protocol_attach_vertex_buffer,
    .commit = z_render_block_protocol_commit,
};

struct wl_list* z_render_block_get_link(struct z_render_block* render_block) { return &render_block->link; }

void z_render_block_draw(struct z_render_block* render_block)
{
  struct z_gl_vertex_buffer* vertex_buffer =
      z_render_block_state_get_vertex_buffer(render_block->current_state);
  if (vertex_buffer == NULL) return;

  glBindVertexArray(render_block->vertex_array_object);
  glDrawArrays(GL_LINES, 0, vertex_buffer->size / (sizeof(float) * 3));
  glBindVertexArray(0);
}

struct z_render_block* z_render_block_from_link(struct wl_list* list)
{
  struct z_render_block* render_block;
  render_block = wl_container_of(list, render_block, link);
  return render_block;
}

struct z_render_block* z_render_block_create(struct wl_client* client, uint32_t id,
                                             struct z_compositor* compositor)
{
  struct z_render_block* render_block;
  struct wl_resource* resource;

  render_block = zalloc(sizeof *render_block);
  if (render_block == NULL) goto no_mem_render_block;

  render_block->current_state = z_render_block_state_create();
  if (render_block->current_state == NULL) goto no_mem_current_state;

  render_block->next_state = z_render_block_state_create();
  if (render_block->next_state == NULL) goto no_mem_next_state;

  resource = wl_resource_create(client, &z11_render_block_interface, 1, id);
  if (resource == NULL) goto no_mem_resource;

  render_block->compositor = compositor;

  wl_list_init(&render_block->link);

  glGenVertexArrays(1, &render_block->vertex_array_object);

  wl_resource_set_implementation(resource, &z_render_block_interface, render_block,
                                 z_render_block_handle_destroy);

  z_compositor_append_render_block(compositor, render_block);

  return render_block;

no_mem_resource:
  free(render_block->next_state);

no_mem_next_state:
  free(render_block->current_state);

no_mem_current_state:
  free(render_block);

no_mem_render_block:
  wl_client_post_no_memory(client);
  return NULL;
}

void z_render_block_destroy(struct z_render_block* render_block)
{
  wl_list_remove(&render_block->link);
  z_render_block_state_destroy(render_block->current_state);
  z_render_block_state_destroy(render_block->next_state);
  glDeleteVertexArrays(1, &render_block->vertex_array_object);
  free(render_block);
}