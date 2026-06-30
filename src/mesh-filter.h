#pragma once
#include <obs-module.h>
#include <graphics/graphics.h>
#include "mesh.h"

struct mesh_filter_data {
	obs_source_t *source;

	gs_texrender_t *render;
	gs_vertbuffer_t *vbuf;

	gs_vertbuffer_t *ol_lines;
	gs_vertbuffer_t *ol_handles;

	Mesh mesh;
	bool mesh_dirty;

	int n_columns;
	int n_rows;

	uint32_t cx;
	uint32_t cy;
	uint32_t num_verts;

	int hover_idx;
	bool show_overlay;
	struct mesh_editor *editor;
};

