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

// TODO: 이어 클리핑
std::vector<uint32_t> Triangulate::TriangulateEarClipping(const PolyObject& polygon, std::vector<glm::dvec2>& flatRingOut)
{
	flatRingOut.clear();
	std::vector<uint32_t> triIndices;

	if (polygon.points.size() < 3) return triIndices;
	earClipping.Clear(polygon.points.size());

	int32_t flatRingOutPart = 0;
	std::vector<int32_t> holePolygonPart;

	// 1. 파츠 분석 및 초기 연결 (신발끈 공식으로 시계/반시계 판별)
	for (int part = 0; part < polygon.parts.size(); part++) {
		int32_t startPoint = polygon.parts[part];
		int32_t endPoint = (part != polygon.parts.size() - 1) ? polygon.parts[part + 1] - 1 : polygon.points.size() - 1;

		if (polygon.points[startPoint] == polygon.points[endPoint]) endPoint--;
		if (endPoint - startPoint < 2) continue;

		double crossSum = 0.0;
		for (int pointId = startPoint; pointId <= endPoint; pointId++) {
			int nextPointId = (pointId == endPoint) ? startPoint : pointId + 1;
			crossSum += (polygon.points[pointId].x * polygon.points[nextPointId].y - polygon.points[pointId].y * polygon.points[nextPointId].x);

			earClipping.next[pointId] = nextPointId;
			earClipping.prev[nextPointId] = pointId;
		}

		if (crossSum < 0) holePolygonPart.emplace_back(part); // 반시계: 구멍
		else              flatRingOutPart = part;             // 시계: 외곽선
	}

	// 2. 내부 구멍과 외곽선 연결 (Bridge)
	int32_t outerStart = polygon.parts[flatRingOutPart];
	int32_t outerEnd = (flatRingOutPart != polygon.parts.size() - 1) ? polygon.parts[flatRingOutPart + 1] - 1 : polygon.points.size() - 1;

	for (int32_t holePart : holePolygonPart) {
		int32_t holeStart = polygon.parts[holePart];
		int32_t holeEnd   = (holePart != polygon.parts.size() - 1) ? polygon.parts[holePart + 1] - 1 : polygon.points.size() - 1;
		if (polygon.points[holeStart] == polygon.points[holeEnd]) holeEnd--;

		struct Bridge { int outerIndex; double distance; };
		std::vector<Bridge> bridges;

		for (int outPoint = outerStart; outPoint <= outerEnd; outPoint++) {
			double dx = polygon.points[holeStart].x - polygon.points[outPoint].x;
			double dy = polygon.points[holeStart].y - polygon.points[outPoint].y;
			bridges.push_back({ outPoint, dx * dx + dy * dy });
		}

		std::sort(bridges.begin(), bridges.end(), [](const Bridge& a, const Bridge& b) { return a.distance < b.distance; });

		int bestOuter = -1;
		glm::dvec2 point1 = polygon.points[holeStart];

		for (auto& bridge : bridges) {
			glm::dvec2 point2 = polygon.points[bridge.outerIndex];
			double isCross = -1.0;

			// 교차 검사 로직
			for (int pointId = outerStart; pointId <= outerEnd && !isCross; pointId++) {
				int nextPointId = earClipping.next[pointId];
				if (pointId == bridge.outerIndex || nextPointId == bridge.outerIndex) continue;
				isCross = CrossCheck(point1, point2, polygon.points[pointId], polygon.points[nextPointId]);
			}
			for (int i = holeStart; i <= holeEnd && !isCross; i++) {
				int next_i = earClipping.next[i];
				if (i == holeStart || next_i == holeStart) continue;
				isCross = CrossCheck(point1, point2, polygon.points[i], polygon.points[next_i]);
			}

			if (isCross >= 0.0) {
				bestOuter = bridge.outerIndex;
				break;
			}
		}

		if (bestOuter == -1) bestOuter = bridges[0].outerIndex; // 안전장치

		// 왕복 브리지 연결 (One Continuous Loop)
		int outerNext = earClipping.next[bestOuter];
		int holeLast  = earClipping.prev[holeStart];

		earClipping.next[bestOuter] = holeStart;
		earClipping.prev[holeStart] = bestOuter;

		earClipping.next[holeLast]  = outerNext;
		earClipping.prev[outerNext] = holeLast;
	}

	// 3. 만들어진 단일 루프를 따라 flatRingOut 배열 생성
	int32_t curPointIndex = outerStart;
	int safety = 0;
	int maxPoints = polygon.points.size() * 2; // 브리지 왕복 고려한 최대점

	do {
		flatRingOut.push_back(polygon.points[curPointIndex]);
		curPointIndex = earClipping.next[curPointIndex];
		safety++;
	} while (curPointIndex != outerStart && safety < maxPoints);

	// 4. 완성된 정점 배열을 새로 만든 이어 클리핑 함수에 던지고 결과 반환
	return EarClipping(flatRingOut);
}

std::vector<uint32_t> Triangulate::EarClipping(const std::vector<glm::dvec2>& points)
{
	int N = points.size();
	std::vector<uint32_t> triIndices;
	if (N < 3) return triIndices;

	// 1. O(1) 삭제를 위한 로컬 연결 리스트 (캐시 친화적)
	std::vector<int> prev(N), next(N);
	for (int i = 0; i < N; ++i) {
		prev[i] = (i - 1 + N) % N;
		next[i] = (i + 1) % N;
	}

	// 외적 람다 (CCW, CW 판별)
	auto cross = [&](int a, int b, int c) {
		glm::dvec2 v1 = points[b] - points[a];
		glm::dvec2 v2 = points[c] - points[b];
		return v1.x * v2.y - v1.y * v2.x;
	};

	int cur = 0;
	int remainCount = N;
	int safety = 0; // 무한 루프 방지용

	// 2. 핵심 루프
	while (remainCount >= 3 && safety < 2 * N) {
		int p = prev[cur];
		int n = next[cur];

		// 조건 1: 볼록(Convex)한가? (시계 방향 검사)
		if (cross(p, cur, n) >= 0.0) {
			cur = n; safety++; continue;
		}

		// 조건 2: 삼각형 내부에 다른 점이 없는가?
		bool isEar = true;
		for (int check = next[n]; check != p; check = next[check]) {
			// 점이 삼각형 p-cur-n 내부에 있는지 검사 (모두 같은 방향인지)
			if (cross(p, cur, check) < 0.0 &&
				cross(cur, n, check) < 0.0 &&
				cross(n, p, check)   < 0.0) {
				isEar = false;
				break;
			}
		}

		if (!isEar) {
			cur = n; safety++; continue;
		}

		// 귀 잘라내기 (삼각형 확정)
		triIndices.push_back(p);
		triIndices.push_back(cur);
		triIndices.push_back(n);

		// 노드 삭제 (현재 점 cur을 건너뛰도록 링크 수정)
		next[p] = n;
		prev[n] = p;

		remainCount--;
		cur = p;       // 자른 후, 이전 점으로 돌아가서 다시 검사 (최적화)
		safety = 0;    // 진행되었으므로 안전 카운터 초기화
	}

	return triIndices;
}