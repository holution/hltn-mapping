#include "mesh.h"
#include <algorithm>
#include <cassert>
#include <array>

Mesh::Mesh() { resize(2, 2); }

Mesh::Mesh(int columns, int rows) { resize(columns, rows); }

void Mesh::resize(int columns, int rows)
{
	assert(columns >= 2 && rows >= 2);
	m_columns = columns;
	m_rows = rows;
	m_vertices.resize(columns * rows);
	m_index_2d.resize(columns * rows);
	build();
}

void Mesh::build()
{
	rebuild_index();
	reorder_vertices();
}

void Mesh::rebuild_index()
{
	m_index_2d.resize(m_columns * m_rows);
	for (int y = 0; y < m_rows; y++) {
		for (int x = 0; x < m_columns; x++) {
			m_index_2d[idx(x, y)] = y * m_columns + x;
		}
	}
}

void Mesh::reorder_vertices()
{
	std::vector<Vec2> new_verts(m_vertices.size());
	for (int y = 0; y < m_rows; y++) {
		for (int x = 0; x < m_columns; x++) {
			new_verts[y * m_columns + x] =
				m_vertices[m_index_2d[idx(x, y)]];
		}
	}
	m_vertices = new_verts;
	rebuild_index();
}

void Mesh::init_full_rect(int columns, int rows,
			  float x0, float y0, float w, float h)
{
	resize(columns, rows);
	for (int y = 0; y < rows; y++) {
		for (int x = 0; x < columns; x++) {
			float fx = (float)x / (float)(columns - 1);
			float fy = (float)y / (float)(rows - 1);
			m_vertices[y * columns + x] = {
				x0 + fx * w,
				y0 + fy * h};
		}
	}
}

Vec2 Mesh::get_vertex_2d(int x, int y) const
{
	return m_vertices[m_index_2d[idx(x, y)]];
}

void Mesh::set_vertex_2d(int x, int y, Vec2 v)
{
	m_vertices[m_index_2d[idx(x, y)]] = v;
}

std::vector<std::array<int, 4>> Mesh::get_quads() const
{
	std::vector<std::array<int, 4>> quads;
	int hq = n_horizontal_quads();
	int vq = n_vertical_quads();
	if (hq < 1 || vq < 1)
		return quads;

	quads.reserve(hq * vq);
	for (int y = 0; y < vq; y++) {
		for (int x = 0; x < hq; x++) {
			int i0 = m_index_2d[idx(x, y)];
			int i1 = m_index_2d[idx(x + 1, y)];
			int i2 = m_index_2d[idx(x + 1, y + 1)];
			int i3 = m_index_2d[idx(x, y + 1)];
			quads.push_back({i0, i1, i2, i3});
		}
	}
	return quads;
}

void Mesh::add_column()
{
	int new_cols = m_columns + 1;
	std::vector<Vec2> new_verts(m_vertices.size() + m_rows);

	for (int y = 0; y < m_rows; y++) {
		Vec2 left = get_vertex_2d(0, y);
		Vec2 right = get_vertex_2d(m_columns - 1, y);
		float dx = (right.x - left.x) / (float)new_cols;
		float dy = (right.y - left.y) / (float)new_cols;

		for (int x = 0; x < m_columns - 1; x++) {
			Vec2 p = get_vertex_2d(x, y);
			p.x -= dx * (float)x * (1.0f / (float)(m_columns - 1) -
						 1.0f / (float)new_cols) *
				(float)(new_cols - 1);
			p.y -= dy * (float)x * (1.0f / (float)(m_columns - 1) -
						 1.0f / (float)new_cols) *
				(float)(new_cols - 1);
			new_verts[y * new_cols + x] = p;
		}

		Vec2 new_p = {right.x - dx, right.y - dy};
		new_verts[y * new_cols + (m_columns - 1)] = new_p;

		Vec2 final_right = get_vertex_2d(m_columns - 1, y);
		new_verts[y * new_cols + m_columns] = final_right;
	}

	m_columns = new_cols;
	m_vertices = new_verts;
	rebuild_index();
}

void Mesh::add_row()
{
	int new_rows = m_rows + 1;
	std::vector<Vec2> new_verts(m_vertices.size() + m_columns);

	int stride = m_columns;
	int new_stride = m_columns;

	for (int x = 0; x < m_columns; x++) {
		Vec2 top = get_vertex_2d(x, 0);
		Vec2 bottom = get_vertex_2d(x, m_rows - 1);
		float dx = (bottom.x - top.x) / (float)new_rows;
		float dy = (bottom.y - top.y) / (float)new_rows;

		for (int y = 0; y < m_rows - 1; y++) {
			Vec2 p = get_vertex_2d(x, y);
			float prop = (float)y * (1.0f / (float)(m_rows - 1) -
						1.0f / (float)new_rows);
			p.x -= dx * prop * (float)(new_rows - 1);
			p.y -= dy * prop * (float)(new_rows - 1);
			new_verts[y * new_stride + x] = p;
		}

		Vec2 new_p = {bottom.x - dx, bottom.y - dy};
		new_verts[(m_rows - 1) * new_stride + x] = new_p;

		Vec2 final_bottom = get_vertex_2d(x, m_rows - 1);
		new_verts[m_rows * new_stride + x] = final_bottom;
	}

	m_rows = new_rows;
	m_vertices = new_verts;
	rebuild_index();
}

void Mesh::remove_column(int idx)
{
	assert(idx >= 1 && idx < m_columns - 1);
	int new_cols = m_columns - 1;
	std::vector<Vec2> new_verts(m_vertices.size() - m_rows);
	int k = 0;
	for (int y = 0; y < m_rows; y++) {
		for (int x = 0; x < m_columns; x++) {
			if (x == idx) continue;
			new_verts[k++] = get_vertex_2d(x, y);
		}
	}
	m_columns = new_cols;
	m_vertices = new_verts;
	rebuild_index();
}

void Mesh::remove_row(int idx)
{
	assert(idx >= 1 && idx < m_rows - 1);
	int new_rows = m_rows - 1;
	std::vector<Vec2> new_verts;
	new_verts.reserve(m_vertices.size() - m_columns);
	for (int y = 0; y < m_rows; y++) {
		if (y == idx) continue;
		for (int x = 0; x < m_columns; x++) {
			new_verts.push_back(get_vertex_2d(x, y));
		}
	}
	m_rows = new_rows;
	m_vertices = new_verts;
	rebuild_index();
}

void Mesh::set_columns(int cols)
{
	while (cols < m_columns) remove_column(m_columns - 2);
	while (cols > m_columns) add_column();
}

void Mesh::set_rows(int rows)
{
	while (rows < m_rows) remove_row(m_rows - 2);
	while (rows > m_rows) add_row();
}
