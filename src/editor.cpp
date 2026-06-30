#include "editor.h"
#include "mesh-filter.h"
#include <vector>
#include <array>
#include <cmath>

static void editor_rebuild_overlay(mesh_editor *ed);

#define EDITOR_HANDLE_RADIUS 6.0f
#define EDITOR_HIT_RADIUS 12.0f

static const wchar_t *EDITOR_CLASS = L"HLTNMappingEditor";
static const wchar_t *EDITOR_TITLE = L"HLTN Mapping Editor";

static void editor_mouse_to_scene(mesh_editor *ed, int mx, int my, float *sx, float *sy)
{
	RECT rc;
	GetClientRect(ed->hwnd, &rc);
	float win_w = (float)(rc.right > 0 ? rc.right : 1);
	float win_h = (float)(rc.bottom > 0 ? rc.bottom : 1);
	*sx = ed->offset_x + ((float)mx / win_w) * ((float)ed->source_cx / ed->zoom);
	*sy = ed->offset_y + ((float)my / win_h) * ((float)ed->source_cy / ed->zoom);
}

static int editor_find_vertex(mesh_editor *ed, float sx, float sy)
{
	int best = -1;
	float best_d = EDITOR_HIT_RADIUS * EDITOR_HIT_RADIUS;
	int nv = ed->filter->mesh.n_vertices();
	for (int i = 0; i < nv; i++) {
		Vec2 v = ed->filter->mesh.get_vertex(i);
		float dx = sx - v.x;
		float dy = sy - v.y;
		float d = dx * dx + dy * dy;
		if (d < best_d) {
			best_d = d;
			best = i;
		}
	}
	return best;
}

static void editor_rebuild_mesh_buffers(mesh_editor *ed)
{
	obs_enter_graphics();

	if (ed->vbuf) {
		gs_vertexbuffer_destroy(ed->vbuf);
		ed->vbuf = nullptr;
	}

	int cols = ed->filter->n_columns;
	int rows = ed->filter->n_rows;
	auto quads = ed->filter->mesh.get_quads();
	if (quads.empty()) {
		obs_leave_graphics();
		return;
	}

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
			verts[i] = ed->filter->mesh.get_vertex(q[i]);

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

	ed->vbuf = gs_vertexbuffer_create(vb_data, GS_DYNAMIC);

	editor_rebuild_overlay(ed);

	obs_leave_graphics();
}

static void editor_destroy_overlay(mesh_editor *ed)
{
	obs_enter_graphics();
	if (ed->ol_lines) {
		gs_vertexbuffer_destroy(ed->ol_lines);
		ed->ol_lines = nullptr;
	}
	if (ed->ol_handles) {
		gs_vertexbuffer_destroy(ed->ol_handles);
		ed->ol_handles = nullptr;
	}
	obs_leave_graphics();
}

static void editor_rebuild_overlay(mesh_editor *ed)
{
	editor_destroy_overlay(ed);
	obs_enter_graphics();

	int cols = ed->filter->n_columns;
	int rows = ed->filter->n_rows;

	if (cols < 2 || rows < 2)
		return;

	int n_hseg = cols - 1;
	int n_vseg = rows - 1;
	int n_line_verts = (rows * n_hseg + cols * n_vseg) * 2;

	if (n_line_verts > 0) {
		struct gs_vb_data *ld = gs_vbdata_create();
		ld->num = (uint32_t)n_line_verts;
		ld->points = (vec3 *)bzalloc(n_line_verts * sizeof(vec3));
		ld->colors = (uint32_t *)bzalloc(n_line_verts * sizeof(uint32_t));
		uint32_t lc = 0x80FFFFFF;
		int li = 0;
		for (int r = 0; r < rows; r++) {
			for (int c = 0; c < n_hseg; c++) {
				Vec2 v0 = ed->filter->mesh.get_vertex_2d(c, r);
				Vec2 v1 = ed->filter->mesh.get_vertex_2d(c + 1, r);
				ld->points[li] = {v0.x, v0.y, 0.0f}; ld->colors[li] = lc; li++;
				ld->points[li] = {v1.x, v1.y, 0.0f}; ld->colors[li] = lc; li++;
			}
		}
		for (int c = 0; c < cols; c++) {
			for (int r = 0; r < n_vseg; r++) {
				Vec2 v0 = ed->filter->mesh.get_vertex_2d(c, r);
				Vec2 v1 = ed->filter->mesh.get_vertex_2d(c, r + 1);
				ld->points[li] = {v0.x, v0.y, 0.0f}; ld->colors[li] = lc; li++;
				ld->points[li] = {v1.x, v1.y, 0.0f}; ld->colors[li] = lc; li++;
			}
		}
		ed->ol_lines = gs_vertexbuffer_create(ld, GS_DYNAMIC);
	}

	int nv = cols * rows;
	int n_handle_verts = nv * 6;
	struct gs_vb_data *hd = gs_vbdata_create();
	hd->num = (uint32_t)n_handle_verts;
	hd->points = (vec3 *)bzalloc(n_handle_verts * sizeof(vec3));
	hd->colors = (uint32_t *)bzalloc(n_handle_verts * sizeof(uint32_t));

	float hr = EDITOR_HANDLE_RADIUS;
	uint32_t hc = 0xCCFFFFFF;
	uint32_t hov_c = 0xCC00FFFF;
	int hi = 0;
	for (int i = 0; i < nv; i++) {
		Vec2 v = ed->filter->mesh.get_vertex(i);
		float x0 = v.x - hr, y0 = v.y - hr;
		float x1 = v.x + hr, y1 = v.y + hr;
		uint32_t col = (i == ed->hover_idx || i == ed->drag_idx) ? hov_c : hc;
		hd->points[hi] = {x0, y0, 0.0f}; hd->colors[hi] = col; hi++;
		hd->points[hi] = {x1, y0, 0.0f}; hd->colors[hi] = col; hi++;
		hd->points[hi] = {x0, y1, 0.0f}; hd->colors[hi] = col; hi++;
		hd->points[hi] = {x1, y0, 0.0f}; hd->colors[hi] = col; hi++;
		hd->points[hi] = {x1, y1, 0.0f}; hd->colors[hi] = col; hi++;
		hd->points[hi] = {x0, y1, 0.0f}; hd->colors[hi] = col; hi++;
	}
	ed->ol_handles = gs_vertexbuffer_create(hd, GS_DYNAMIC);
	obs_leave_graphics();
}

static void editor_render_overlay(mesh_editor *ed)
{
	int cols = ed->filter->n_columns;
	int rows = ed->filter->n_rows;

	if (cols < 2 || rows < 2)
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

	if (ed->ol_lines) {
		gs_load_vertexbuffer(ed->ol_lines);
		gs_draw(GS_LINES, 0, 0);
	}

	if (ed->ol_handles) {
		gs_load_vertexbuffer(ed->ol_handles);
		gs_draw(GS_TRIS, 0, 0);
	}

	gs_load_vertexbuffer(nullptr);
	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

static void editor_draw(void *data, uint32_t cx, uint32_t cy)
{
	auto *ed = (mesh_editor *)data;

	gs_viewport_push();
	gs_projection_push();

	gs_ortho(ed->offset_x, ed->offset_x + (float)ed->source_cx / ed->zoom,
		 ed->offset_y, ed->offset_y + (float)ed->source_cy / ed->zoom,
		 -100.0f, 100.0f);
	gs_set_viewport(0, 0, cx, cy);

	editor_render_overlay(ed);

	gs_load_vertexbuffer(nullptr);

	gs_projection_pop();
	gs_viewport_pop();
}

static LRESULT CALLBACK editor_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	auto *ed = (mesh_editor *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
	if (!ed && msg != WM_CREATE)
		return DefWindowProcW(hwnd, msg, wparam, lparam);

	switch (msg) {
	case WM_CREATE: {
		auto *cs = (CREATESTRUCTW *)lparam;
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
		return 0;
	}
	case WM_DESTROY: {
		ed->running = false;
		return 0;
	}
	case WM_CLOSE: {
		DestroyWindow(hwnd);
		return 0;
	}
	case WM_LBUTTONDOWN: {
		float sx, sy;
		editor_mouse_to_scene(ed, (int)(short)LOWORD(lparam), (int)(short)HIWORD(lparam), &sx, &sy);
		int idx = editor_find_vertex(ed, sx, sy);
		if (idx >= 0) {
			ed->drag_idx = idx;
			Vec2 v = ed->filter->mesh.get_vertex(idx);
			ed->drag_off_x = v.x - sx;
			ed->drag_off_y = v.y - sy;
			SetCapture(hwnd);
			editor_rebuild_overlay(ed);
		}
		return 0;
	}
	case WM_LBUTTONUP: {
		if (ed->drag_idx >= 0) {
			ed->filter->mesh_dirty = true;
			if (!ed->filter->cx) ed->filter->cx = ed->source_cx;
			if (!ed->filter->cy) ed->filter->cy = ed->source_cy;
			ed->drag_idx = -1;
			ReleaseCapture();
			editor_rebuild_overlay(ed);

			obs_source_t *src = ed->filter->source;
			if (src) {
				obs_data_t *settings = obs_source_get_settings(src);
				if (settings) {
					int nv = ed->filter->mesh.n_vertices();
					obs_data_array_t *arr = obs_data_array_create();
					for (int i = 0; i < nv; i++) {
						Vec2 v = ed->filter->mesh.get_vertex(i);
						obs_data_t *obj = obs_data_create();
						obs_data_set_double(obj, "x", v.x);
						obs_data_set_double(obj, "y", v.y);
						obs_data_array_push_back(arr, obj);
						obs_data_release(obj);
					}
					obs_data_set_array(settings, "mesh_vertices", arr);
					obs_data_array_release(arr);
					obs_data_release(settings);
				}
			}
		}
		return 0;
	}
	case WM_MBUTTONDOWN: {
		ed->panning = true;
		ed->pan_last_x = (int)(short)LOWORD(lparam);
		ed->pan_last_y = (int)(short)HIWORD(lparam);
		SetCapture(hwnd);
		return 0;
	}
	case WM_MBUTTONUP: {
		ed->panning = false;
		ReleaseCapture();
		return 0;
	}
	case WM_MOUSEMOVE: {
		int mx = (int)(short)LOWORD(lparam);
		int my = (int)(short)HIWORD(lparam);

		if (ed->panning) {
			float dx = (float)(mx - ed->pan_last_x) * ((float)ed->source_cx / ed->zoom) / (float)(ed->source_cx > 0 ? ed->source_cx : 1);
			float dy = (float)(my - ed->pan_last_y) * ((float)ed->source_cy / ed->zoom) / (float)(ed->source_cy > 0 ? ed->source_cy : 1);
			ed->offset_x -= dx;
			ed->offset_y -= dy;
			ed->pan_last_x = mx;
			ed->pan_last_y = my;
		} else if (ed->drag_idx >= 0) {
			float sx, sy;
			editor_mouse_to_scene(ed, mx, my, &sx, &sy);
			float nx = sx + ed->drag_off_x;
			float ny = sy + ed->drag_off_y;
			if (nx < 0) nx = 0;
			if (ny < 0) ny = 0;
			if (nx > (float)ed->source_cx) nx = (float)ed->source_cx;
			if (ny > (float)ed->source_cy) ny = (float)ed->source_cy;
			Vec2 v = {nx, ny};
			ed->filter->mesh.set_vertex(ed->drag_idx, v);
			ed->filter->mesh_dirty = true;
			if (!ed->filter->cx) ed->filter->cx = ed->source_cx;
			if (!ed->filter->cy) ed->filter->cy = ed->source_cy;
			editor_rebuild_mesh_buffers(ed);
		} else {
			float sx, sy;
			editor_mouse_to_scene(ed, mx, my, &sx, &sy);
			int prev_hover = ed->hover_idx;
			ed->hover_idx = editor_find_vertex(ed, sx, sy);
			if (ed->hover_idx != prev_hover)
				editor_rebuild_overlay(ed);
		}
		return 0;
	}
	case WM_MOUSEWHEEL: {
		int delta = GET_WHEEL_DELTA_WPARAM(wparam);
		float zoom_before = ed->zoom;
		ed->zoom *= (delta > 0) ? 1.1f : (1.0f / 1.1f);
		if (ed->zoom < 0.1f) ed->zoom = 0.1f;
		if (ed->zoom > 50.0f) ed->zoom = 50.0f;

		POINT pt;
		pt.x = (int)(short)LOWORD(lparam);
		pt.y = (int)(short)HIWORD(lparam);
		ScreenToClient(hwnd, &pt);
		RECT rc;
		GetClientRect(hwnd, &rc);
		float win_w = (float)(rc.right > 0 ? rc.right : 1);
		float win_h = (float)(rc.bottom > 0 ? rc.bottom : 1);
		float mxf = (float)pt.x, myf = (float)pt.y;

		float sx = ed->offset_x + (mxf / win_w) * ((float)ed->source_cx / zoom_before);
		float sy = ed->offset_y + (myf / win_h) * ((float)ed->source_cy / zoom_before);
		ed->offset_x = sx - (mxf / win_w) * ((float)ed->source_cx / ed->zoom);
		ed->offset_y = sy - (myf / win_h) * ((float)ed->source_cy / ed->zoom);
		return 0;
	}
	case WM_SIZE: {
		RECT rc;
		GetClientRect(hwnd, &rc);
		if (ed->display) {
			obs_display_resize(ed->display, rc.right, rc.bottom);
		}
		return 0;
	}
	}
	return DefWindowProcW(hwnd, msg, wparam, lparam);
}

mesh_editor *mesh_editor_open(mesh_filter_data *filter)
{
	if (!filter)
		return nullptr;

	auto *ed = (mesh_editor *)bzalloc(sizeof(mesh_editor));
	if (!ed)
		return nullptr;

	ed->filter = filter;
	ed->drag_idx = -1;
	ed->hover_idx = -1;
	ed->panning = false;
	ed->offset_x = 0.0f;
	ed->offset_y = 0.0f;
	ed->zoom = 1.0f;
	ed->render = nullptr;
	ed->vbuf = nullptr;
	ed->ol_lines = nullptr;
	ed->ol_handles = nullptr;
	ed->running = true;

	obs_source_t *target = obs_filter_get_target(filter->source);
	if (target) {
		ed->source_cx = obs_source_get_base_width(target);
		ed->source_cy = obs_source_get_base_height(target);
	}

	if (ed->source_cx == 0) ed->source_cx = 1920;
	if (ed->source_cy == 0) ed->source_cy = 1080;

	// Ensure mesh is initialized before opening editor
	auto *f = ed->filter;
	if (f->mesh.n_columns() != f->n_columns || f->mesh.n_rows() != f->n_rows) {
		f->mesh.resize(f->n_columns, f->n_rows);
		f->mesh.init_full_rect(f->n_columns, f->n_rows, 0, 0,
				       (float)ed->source_cx, (float)ed->source_cy);
	}

	editor_rebuild_mesh_buffers(ed);

	// Register window class
	HINSTANCE hinst = GetModuleHandleW(nullptr);
	WNDCLASSW wc = {};
	wc.lpfnWndProc = editor_wndproc;
	wc.hInstance = hinst;
	wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
	wc.lpszClassName = EDITOR_CLASS;
	RegisterClassW(&wc);

	// Create window
	int w = (int)ed->source_cx + 16;
	int h = (int)ed->source_cy + 39;
	if (w < 640) w = 640;
	if (h < 480) h = 480;

	ed->hwnd = CreateWindowExW(0, EDITOR_CLASS, EDITOR_TITLE,
				   WS_OVERLAPPEDWINDOW,
				   CW_USEDEFAULT, CW_USEDEFAULT, w, h,
				   nullptr, nullptr, hinst, ed);
	if (!ed->hwnd) {
		bfree(ed);
		return nullptr;
	}

	ShowWindow(ed->hwnd, SW_SHOW);
	UpdateWindow(ed->hwnd);

	// Create OBS display
	RECT rc;
	GetClientRect(ed->hwnd, &rc);
	gs_init_data gid = {};
	gid.cx = rc.right;
	gid.cy = rc.bottom;
	gid.num_backbuffers = 0;
	gid.format = GS_BGRA;
	gid.zsformat = GS_ZS_NONE;
	gid.window.hwnd = ed->hwnd;

	ed->display = obs_display_create(&gid, 0);
	if (!ed->display) {
		DestroyWindow(ed->hwnd);
		bfree(ed);
		return nullptr;
	}

	obs_display_add_draw_callback(ed->display, editor_draw, ed);

	return ed;
}

void mesh_editor_close(mesh_editor *editor)
{
	if (!editor)
		return;

	if (editor->display) {
		obs_display_remove_draw_callback(editor->display, editor_draw, editor);
		obs_display_destroy(editor->display);
	}
	if (editor->hwnd)
		DestroyWindow(editor->hwnd);
	obs_enter_graphics();
	if (editor->vbuf) {
		gs_vertexbuffer_destroy(editor->vbuf);
		editor->vbuf = nullptr;
	}
	if (editor->render) {
		gs_texrender_destroy(editor->render);
		editor->render = nullptr;
	}
	obs_leave_graphics();
	editor_destroy_overlay(editor);
	bfree(editor);
}
