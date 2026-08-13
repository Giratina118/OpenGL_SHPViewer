#include <pch.h>
#include "Triangulate.h"
#include <algorithm>

// 한 폴리곤을 CDT 들로네 삼각분할
std::vector<uint32_t> Triangulate::TriangulatePolygonCDT(const PolyObject& polygon, std::vector<glm::dvec2>& flatRingOut)
{
	flatRingOut.clear();
	std::vector<uint32_t> triIndices;
	std::vector<CDT::Edge> edges;

	if (polygon.points.size() < 3) return triIndices;

	// 중복 정점 제거 및 인덱스 매핑
	struct PointIndex { double x, y; size_t originalIndex; }; // 정렬 후에도 "이 점이 원본에서 몇 번째였는가"를 추적하기 위해
	std::vector<PointIndex> sortPts;
	sortPts.reserve(polygon.points.size());

	// 원본 정점과 원래 인덱스를 함께 저장
	for (size_t i = 0; i < polygon.points.size(); ++i) {
		sortPts.push_back({ polygon.points[i].x, polygon.points[i].y, i });
	}

	// X 좌표 기준 정렬 (X가 같으면 Y 기준)
	std::sort(sortPts.begin(), sortPts.end(), [](const PointIndex& pointA, const PointIndex& pointB) {
		return (pointA.x != pointB.x) ? pointA.x < pointB.x : pointA.y < pointB.y;
	});

	// 원본 인덱스가 중복이 제거된 새로운 flatRingOut(외곽점)의 몇 번 인덱스로 매핑되는지 기록
	std::vector<uint32_t> pointToUniqueIndex(polygon.points.size());

	for (size_t i = 0; i < sortPts.size(); ++i) {
		// 이전 점과 완전히 동일한 좌표라면 (중복점)
		if (i > 0 && sortPts[i].x == sortPts[i - 1].x && sortPts[i].y == sortPts[i - 1].y) {
			pointToUniqueIndex[sortPts[i].originalIndex] = pointToUniqueIndex[sortPts[i - 1].originalIndex];
		}
		// 새로운 좌표라면
		else {
			pointToUniqueIndex[sortPts[i].originalIndex] = (uint32_t)flatRingOut.size();
			flatRingOut.push_back({sortPts[i].x, sortPts[i].y });
		}
	}

	// 중복 제거 후 유효한 점이 3개 미만이면 면을 만들 수 없음
	if (flatRingOut.size() < 3) return triIndices;


	// 외곽선(Ring) 제약 엣지 구성
	size_t partCount = polygon.parts.size();
	for (size_t partNum = 0; partNum < partCount; partNum++) {
		size_t startPoint = polygon.parts[partNum];
		size_t endPoint = polygon.points.size();
		if (partNum + 1 < partCount) endPoint = polygon.parts[partNum + 1];

		if (endPoint - startPoint < 3) continue; // 최소 점 3개 필요

		for (size_t point = startPoint; point < endPoint; point++) {
			// 현재 점과 다음 점을 연결 (마지막 점의 다음 점은 시작점으로 순환)
			size_t nextPoint = (point + 1 == endPoint) ? startPoint : (point + 1);

			uint32_t u1 = pointToUniqueIndex[point];
			uint32_t u2 = pointToUniqueIndex[nextPoint];

			// 두 점이 다르다(길이가 0이 아니다)면 엣지로 추가
			if (u1 != u2) {
				edges.emplace_back((CDT::VertInd)u1, (CDT::VertInd)u2);
			}
		}
	}

	// CDT 삼각분할 수행 (try-bridgeatch 안전망 적용)
	CDT::Triangulation<double> cdt(
		CDT::VertexInsertionOrder::Auto,
		CDT::IntersectingConstraintEdges::TryResolve,
		1e-10
	);

	// CDT 호출 → eraseOuterTrianglesAndHoles로 outer/hole 자동 처리
	try {
		cdt.insertVertices( // 정점 등록
			flatRingOut.begin(), flatRingOut.end(),
			[](const glm::dvec2& point) { return point.x; },
			[](const glm::dvec2& point) { return point.y; }
		);

		cdt.insertEdges( // 제약 등록
			edges.begin(), edges.end(),
			[](const CDT::Edge& edge) { return edge.v1(); },
			[](const CDT::Edge& edge) { return edge.v2(); }
		);

		cdt.eraseOuterTrianglesAndHoles(); // outer/hole 처리

		triIndices.reserve(cdt.triangles.size() * 3); // 처리 후 남은 삼각형들을 인덱스 배열로 추가
		for (const auto& tri : cdt.triangles) {
			triIndices.push_back((uint32_t)tri.vertices[0]);
			triIndices.push_back((uint32_t)tri.vertices[1]);
			triIndices.push_back((uint32_t)tri.vertices[2]);
		}
	}
	catch (const std::exception& /*e*/) {
		// 에러가 발생한 다각형은 빈 인덱스를 반환하여 화면에 그리지 않고 무시함
	}

	return triIndices;
}