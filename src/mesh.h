#pragma once
#include <vector>
#include <cstdint>

struct Vec2 {
	float x, y;
};

class Mesh {
public:
	Mesh();
	Mesh(int columns, int rows);

	void resize(int columns, int rows);
	void build();

	// Initialize mesh as a full rectangle subdivided into grid
	void init_full_rect(int columns, int rows,
			    float x0, float y0, float w, float h);

	// Read/write vertices
	int n_columns() const { return m_columns; }
	int n_rows() const { return m_rows; }
	int n_vertices() const { return (int)m_vertices.size(); }
	int n_horizontal_quads() const {
		return (m_columns > 1) ? (m_columns - 1) : 0;
	}
	int n_vertical_quads() const {
		return (m_rows > 1) ? (m_rows - 1) : 0;
	}

	Vec2 get_vertex(int i) const { return m_vertices[i]; }
	void set_vertex(int i, Vec2 v) { m_vertices[i] = v; }

	Vec2 get_vertex_2d(int x, int y) const;
	void set_vertex_2d(int x, int y, Vec2 v);

	// Get all quads: each entry = 4 indices [a,b,c,d] (counter-clockwise)
	std::vector<std::array<int, 4>> get_quads() const;

	// Column/row manipulation
	void add_column();
	void add_row();
	void remove_column(int idx);
	void remove_row(int idx);

	void set_columns(int cols);
	void set_rows(int rows);

private:
	int m_columns = 2;
	int m_rows = 2;
	std::vector<Vec2> m_vertices;
	std::vector<int> m_index_2d; // m_index_2d[x * m_rows + y] = vertex index

	int idx(int x, int y) const { return x * m_rows + y; }
	void rebuild_index();
	void reorder_vertices();
};
