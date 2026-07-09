#pragma once

#include <CDT/CDT.h>
#include "FeatureObject.h"

struct EarClipping {
	std::vector<int> prev, next;

	// 정점 제거
	void Remove(int i) {
		prev[next[i]] = prev[i];
		next[prev[i]] = next[i];
	}

	void Clear(int resize) {
		prev.clear();
		next.clear();
		
		prev.resize(resize);
		next.resize(resize);
	}
};

class Triangulate
{
private:
	EarClipping earClipping;
public:
	std::vector<uint32_t> TriangulatePolygonCDT (const PolyObject& polygon, std::vector<glm::dvec2>& flatRingOut); // CDT 들로네 삼각분할
	std::vector<uint32_t> TriangulateEarClipping(const PolyObject& polygon, std::vector<glm::dvec2>& flatRingOut); // Ear Clipping 삼각분할
	std::vector<uint32_t> EarClipping(const std::vector<glm::dvec2>& points);
};