#pragma once

#include <CDT/CDT.h>
#include "FeatureObject.h"

class Triangulate
{
public:
	std::vector<uint32_t> TriangulatePolygonCDT (const PolyObject& polygon, std::vector<glm::dvec2>& flatRingOut); // CDT 들로네 삼각분할
};