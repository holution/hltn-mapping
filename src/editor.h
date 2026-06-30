#pragma once
#include <Windows.h>
#include <obs-module.h>
#include <graphics/graphics.h>
#include "mesh.h"

struct mesh_filter_data;

struct mesh_editor {
	HWND hwnd;
	mesh_filter_data *filter;
	obs_display_t *display;

	int drag_idx;
	float drag_off_x, drag_off_y;
	int hover_idx;

	bool panning;
	int pan_last_x, pan_last_y;
	float offset_x, offset_y;
	float zoom;

	bool running;

	gs_texrender_t *render;
	gs_vertbuffer_t *vbuf;

	gs_vertbuffer_t *ol_lines;
	gs_vertbuffer_t *ol_handles;

	uint32_t source_cx;
	uint32_t source_cy;
};

mesh_editor *mesh_editor_open(mesh_filter_data *filter);
void mesh_editor_close(mesh_editor *editor);
