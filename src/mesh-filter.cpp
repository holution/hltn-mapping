#include "mesh-filter.h"
#include "editor.h"
#include <array>
#include <cmath>

#define HANDLE_RADIUS 6.0f
#define HIT_RADIUS 12.0f

static const char *filter_get_name(void *)
{
	return obs_module_text("HLTNMapping");
}

static void *filter_create(obs_data_t *settings, obs_source_t *source)
{
	auto *f = (mesh_filter_data *)bzalloc(sizeof(mesh_filter_data));
	f->source = source;
	f->n_columns = (int)obs_data_get_int(settings, "columns");
	f->n_rows = (int)obs_data_get_int(settings, "rows");
	if (f->n_columns < 2) f->n_columns = 4;
	if (f->n_rows < 2) f->n_rows = 4;
	f->vbuf = nullptr;
	f->render = nullptr;
	f->ol_lines = nullptr;
	f->ol_handles = nullptr;
	f->hover_idx = -1;
	f->editor = nullptr;
	f->mesh_dirty = true;

	obs_source_t *target = obs_filter_get_target(source);
	if (target) {
		f->cx = obs_source_get_base_width(target);
		f->cy = obs_source_get_base_height(target);
	}

	return f;
}

static void destroy_overlay_buffers(mesh_filter_data *f)
{
	if (f->ol_lines) {
		gs_vertexbuffer_destroy(f->ol_lines);
		f->ol_lines = nullptr;
	}
	if (f->ol_handles) {
		gs_vertexbuffer_destroy(f->ol_handles);
		f->ol_handles = nullptr;
	}
}

static void filter_destroy(void *data)
{
	auto *f = (mesh_filter_data *)data;
	if (f->vbuf)
		gs_vertexbuffer_destroy(f->vbuf);
	if (f->render)
		gs_texrender_destroy(f->render);
	destroy_overlay_buffers(f);
	if (f->editor)
		mesh_editor_close(f->editor);
	bfree(f);
}

static void filter_update(void *data, obs_data_t *settings)
{
	auto *f = (mesh_filter_data *)data;
	f->n_columns = (int)obs_data_get_int(settings, "columns");
	f->n_rows = (int)obs_data_get_int(settings, "rows");
	if (f->n_columns < 2) f->n_columns = 2;
	if (f->n_rows < 2) f->n_rows = 2;
	f->show_overlay = obs_data_get_bool(settings, "show_overlay");
	f->mesh_dirty = true;
}

static void rebuild_overlay(mesh_filter_data *f)
{
	destroy_overlay_buffers(f);

	int cols = f->n_columns;
	int rows = f->n_rows;
	int n_hseg = cols - 1;
	int n_vseg = rows - 1;

	int n_line_verts = (rows * n_hseg + cols * n_vseg) * 2;
	if (n_line_verts <= 0)
		return;

	struct gs_vb_data *line_data = gs_vbdata_create();
	line_data->num = (uint32_t)n_line_verts;
	line_data->points = (vec3 *)bzalloc(n_line_verts * sizeof(vec3));
	line_data->colors = (uint32_t *)bzalloc(n_line_verts * sizeof(uint32_t));
	uint32_t line_color = 0x80FFFFFF;

	int li = 0;
	for (int r = 0; r < rows; r++) {
		for (int c = 0; c < n_hseg; c++) {
			Vec2 v0 = f->mesh.get_vertex_2d(c, r);
			Vec2 v1 = f->mesh.get_vertex_2d(c + 1, r);
			line_data->points[li] = {v0.x, v0.y, 0.0f};
			line_data->colors[li] = line_color; li++;
			line_data->points[li] = {v1.x, v1.y, 0.0f};
			line_data->colors[li] = line_color; li++;
		}
	}
	for (int c = 0; c < cols; c++) {
		for (int r = 0; r < n_vseg; r++) {
			Vec2 v0 = f->mesh.get_vertex_2d(c, r);
			Vec2 v1 = f->mesh.get_vertex_2d(c, r + 1);
			line_data->points[li] = {v0.x, v0.y, 0.0f};
			line_data->colors[li] = line_color; li++;
			line_data->points[li] = {v1.x, v1.y, 0.0f};
			line_data->colors[li] = line_color; li++;
		}
	}

	f->ol_lines = gs_vertexbuffer_create(line_data, GS_DYNAMIC);

	int nv = cols * rows;
	int n_handle_verts = nv * 6;
	struct gs_vb_data *handle_data = gs_vbdata_create();
	handle_data->num = (uint32_t)n_handle_verts;
	handle_data->points = (vec3 *)bzalloc(n_handle_verts * sizeof(vec3));
	handle_data->colors = (uint32_t *)bzalloc(n_handle_verts * sizeof(uint32_t));

	float hr = HANDLE_RADIUS;
	uint32_t handle_color = 0xCCFFFFFF;
	uint32_t hover_color = 0xCC00FFFF;

	int hi = 0;
	for (int i = 0; i < nv; i++) {
		Vec2 v = f->mesh.get_vertex(i);
		float x = v.x, y = v.y;
		float x0 = x - hr, y0 = y - hr;
		float x1 = x + hr, y1 = y + hr;
		uint32_t col = (i == f->hover_idx) ? hover_color : handle_color;

		handle_data->points[hi] = {x0, y0, 0.0f}; handle_data->colors[hi] = col; hi++;
		handle_data->points[hi] = {x1, y0, 0.0f}; handle_data->colors[hi] = col; hi++;
		handle_data->points[hi] = {x0, y1, 0.0f}; handle_data->colors[hi] = col; hi++;
		handle_data->points[hi] = {x1, y0, 0.0f}; handle_data->colors[hi] = col; hi++;
		handle_data->points[hi] = {x1, y1, 0.0f}; handle_data->colors[hi] = col; hi++;
		handle_data->points[hi] = {x0, y1, 0.0f}; handle_data->colors[hi] = col; hi++;
	}

	f->ol_handles = gs_vertexbuffer_create(handle_data, GS_DYNAMIC);
}

static void load_saved_vertices(mesh_filter_data *f)
{
	obs_source_t *src = f->source;
	if (!src)
		return;

	obs_data_t *settings = obs_source_get_settings(src);
	if (!settings)
		return;

	obs_data_array_t *arr = obs_data_get_array(settings, "mesh_vertices");
	if (!arr) {
		obs_data_release(settings);
		return;
	}

	size_t count = obs_data_array_count(arr);
	int nv = f->mesh.n_vertices();
	for (size_t i = 0; i < count && (int)i < nv; i++) {
		obs_data_t *obj = obs_data_array_item(arr, i);
		float x = (float)obs_data_get_double(obj, "x");
		float y = (float)obs_data_get_double(obj, "y");
		f->mesh.set_vertex((int)i, {x, y});
		obs_data_release(obj);
	}
	obs_data_array_release(arr);
	obs_data_release(settings);
}

static void rebuild_mesh(mesh_filter_data *f)
{
	if (!f->cx || !f->cy)
		return;

	int cols = f->n_columns;
	int rows = f->n_rows;

	if (f->mesh.n_columns() != cols || f->mesh.n_rows() != rows || f->mesh.n_vertices() == 0) {
		f->mesh.resize(cols, rows);
		f->mesh.init_full_rect(cols, rows, 0, 0, (float)f->cx, (float)f->cy);
		load_saved_vertices(f);
	}

	if (f->vbuf) {
		gs_vertexbuffer_destroy(f->vbuf);
		f->vbuf = nullptr;
	}

	auto quads = f->mesh.get_quads();
	if (quads.empty())
		return;

	size_t n_verts = quads.size() * 6;
	struct gs_vb_data *vb_data = gs_vbdata_create();
	vb_data->num = (uint32_t)n_verts;
	vb_data->points = (vec3 *)bzalloc(n_verts * sizeof(vec3));
	vb_data->num_tex = 1;
	vb_data->tvarray = (struct gs_tvertarray *)bzalloc(sizeof(struct gs_tvertarray));
	vb_data->tvarray[0].width = 2;
	vb_data->tvarray[0].array = bzalloc(n_verts * sizeof(vec2));

	float inv_cols = 1.0f / (float)(cols - 1);
	float inv_rows = 1.0f / (float)(rows - 1);
	size_t idx = 0;
	vec2 *tverts = (vec2 *)vb_data->tvarray[0].array;

	for (auto &q : quads) {
		Vec2 verts[4];
		for (int i = 0; i < 4; i++)
			verts[i] = f->mesh.get_vertex(q[i]);

		auto set_vert = [&](int vi) {
			vb_data->points[idx] = {verts[vi].x, verts[vi].y, 0.0f};
			int ci = q[vi] % cols;
			int ri = q[vi] / cols;
			tverts[idx] = {(float)ci * inv_cols, (float)ri * inv_rows};
			idx++;
		};

		set_vert(0); set_vert(1); set_vert(2);
		set_vert(0); set_vert(2); set_vert(3);
	}

	f->vbuf = gs_vertexbuffer_create(vb_data, GS_DYNAMIC);
	f->num_verts = (uint32_t)n_verts;
	f->mesh_dirty = false;

	rebuild_overlay(f);
}

static void filter_video_tick(void *data, float seconds)
{
	auto *f = (mesh_filter_data *)data;
	(void)seconds;

	obs_source_t *target = obs_filter_get_target(f->source);
	if (!target)
		return;

	if (f->render)
		gs_texrender_reset(f->render);

	uint32_t cx = obs_source_get_base_width(target);
	uint32_t cy = obs_source_get_base_height(target);

	if (cx != f->cx || cy != f->cy) {
		f->cx = cx;
		f->cy = cy;
		f->mesh_dirty = true;
	}
}

static void draw_overlay(mesh_filter_data *f)
{
	if (!f->ol_lines && !f->ol_handles)
		return;

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	if (!solid)
		return;

	gs_technique_t *tech = gs_effect_get_technique(solid, "SolidColored");
	if (!tech)
		return;

	vec4 color;
	vec4_set(&color, 1.0f, 1.0f, 1.0f, 1.0f);
	gs_effect_set_vec4(gs_effect_get_param_by_name(solid, "color"), &color);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	if (f->ol_lines) {
		gs_load_vertexbuffer(f->ol_lines);
		gs_draw(GS_LINES, 0, 0);
	}

	if (f->ol_handles) {
		gs_load_vertexbuffer(f->ol_handles);
		gs_draw(GS_TRIS, 0, 0);
	}

	gs_load_vertexbuffer(nullptr);
	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

static void filter_video_render(void *data, gs_effect_t *effect)
{
	auto *f = (mesh_filter_data *)data;
	obs_source_t *target = obs_filter_get_target(f->source);
	obs_source_t *parent = obs_filter_get_parent(f->source);

	if (!target || !parent || !f->cx || !f->cy) {
		obs_source_skip_video_filter(f->source);
		return;
	}

	if (f->mesh_dirty || !f->vbuf)
		rebuild_mesh(f);

	if (!f->vbuf) {
		obs_source_skip_video_filter(f->source);
		return;
	}

	const enum gs_color_format format = GS_RGBA;

	if (f->render) {
		if (gs_texrender_get_format(f->render) != format) {
			gs_texrender_destroy(f->render);
			f->render = nullptr;
		}
	}
	if (!f->render)
		f->render = gs_texrender_create(format, GS_ZS_NONE);

	enum gs_color_space space = gs_get_color_space();
	if (gs_texrender_begin_with_color_space(f->render, f->cx, f->cy, space)) {
		gs_blend_state_push();
		gs_blend_function_separate(
			GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA,
			GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

		struct vec4 clear;
		vec4_zero(&clear);
		gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
		gs_ortho(0.0f, (float)f->cx, 0.0f, (float)f->cy, -100.0f, 100.0f);
		obs_source_video_render(target);

		gs_blend_state_pop();
		gs_texrender_end(f->render);
	}

	gs_texture_t *tex = gs_texrender_get_texture(f->render);
	if (!tex) {
		obs_source_skip_video_filter(f->source);
		return;
	}

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);

	gs_effect_t *def_effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	if (def_effect) {
		gs_eparam_t *image = gs_effect_get_param_by_name(def_effect, "image");
		if (image)
			gs_effect_set_texture_srgb(image, tex);

		while (gs_effect_loop(def_effect, "Draw")) {
			gs_load_vertexbuffer(f->vbuf);
			gs_draw(GS_TRIS, 0, f->num_verts);
		}
	}

	if (f->show_overlay)
		draw_overlay(f);

	gs_enable_framebuffer_srgb(previous);

	UNUSED_PARAMETER(effect);
}

static uint32_t filter_get_width(void *data)
{
	auto *f = (mesh_filter_data *)data;
	return f->cx;
}

static uint32_t filter_get_height(void *data)
{
	auto *f = (mesh_filter_data *)data;
	return f->cy;
}

static bool edit_mesh_button(obs_properties_t *props, obs_property_t *property, void *data)
{
	auto *f = (mesh_filter_data *)data;
	if (f->editor) {
		mesh_editor_close(f->editor);
		f->editor = nullptr;
	} else {
		f->editor = mesh_editor_open(f);
	}
	return true;

	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
}

static obs_properties_t *filter_properties(void *)
{
	auto *ppts = obs_properties_create();
	obs_properties_add_int(ppts, "columns", obs_module_text("Columns"), 2, 32, 1);
	obs_properties_add_int(ppts, "rows", obs_module_text("Rows"), 2, 32, 1);
	obs_properties_add_bool(ppts, "show_overlay", obs_module_text("ShowOverlay"));
	obs_properties_add_button(ppts, "edit_mesh", obs_module_text("EditMesh"), edit_mesh_button);
	return ppts;
}

static void filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "columns", 4);
	obs_data_set_default_int(settings, "rows", 4);
	obs_data_set_default_bool(settings, "show_overlay", false);
}

struct obs_source_info hltn_mapping_filter = {
	.id = "hltn_mapping_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB,
	.get_name = filter_get_name,
	.create = filter_create,
	.destroy = filter_destroy,
	.get_width = filter_get_width,
	.get_height = filter_get_height,
	.get_defaults = filter_defaults,
	.get_properties = filter_properties,
	.update = filter_update,
	.video_tick = filter_video_tick,
	.video_render = filter_video_render,
};
