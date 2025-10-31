#include "SpatialIndex.hpp"
#include "RenderUtils.hpp"  // For Frustum class
#include <cmath>

namespace Graphics {

void Octree::build(const std::vector<Point>& points, const glm::vec3& min, const glm::vec3& max,
                   unsigned int maxPointsPerNode, unsigned int maxDepth) {
	mNodeCount = 0;
	mMaxDepth = 0;
	mRoot = std::make_unique<Node>();
	mRoot->min = min;
	mRoot->max = max;
	mRoot->level = 0;
	mRoot->isLeaf = true;
	
	// Collect all point indices
	std::vector<unsigned int> allIndices;
	allIndices.reserve(points.size());
	for (unsigned int i = 0; i < points.size(); ++i) {
		allIndices.push_back(i);
	}
	
	buildRecursive(mRoot.get(), points, allIndices, maxPointsPerNode, maxDepth);
}

void Octree::buildRecursive(Node* node, const std::vector<Point>& points,
                            const std::vector<unsigned int>& indices,
                            unsigned int maxPointsPerNode, unsigned int maxDepth) {
	mNodeCount++;
	if (indices.size() <= maxPointsPerNode || node->level >= maxDepth) {
		// Leaf node - store point indices
		node->pointIndices = indices;
		node->isLeaf = true;
		mMaxDepth = std::max(mMaxDepth, node->level);
		return;
	}
	
	// Subdivide into 8 children
	node->isLeaf = false;
	glm::vec3 center = node->center();
	
	// Create 8 child octants
	glm::vec3 childMins[8], childMaxs[8];
	for (int i = 0; i < 8; ++i) {
		childMins[i] = glm::vec3(
			(i & 1) ? center.x : node->min.x,
			(i & 2) ? center.y : node->min.y,
			(i & 4) ? center.z : node->min.z
		);
		childMaxs[i] = glm::vec3(
			(i & 1) ? node->max.x : center.x,
			(i & 2) ? node->max.y : center.y,
			(i & 4) ? node->max.z : center.z
		);
	}
	
	// Distribute points to children
	std::vector<std::vector<unsigned int>> childIndices(8);
	for (unsigned int idx : indices) {
		const Point& p = points[idx];
		int childIdx = getChildIndex(p.position, center);
		if (childIdx >= 0 && childIdx < 8) {
			childIndices[childIdx].push_back(idx);
		}
	}
	
	// Recursively build children
	for (int i = 0; i < 8; ++i) {
		if (!childIndices[i].empty()) {
			node->children[i] = std::make_unique<Node>();
			node->children[i]->min = childMins[i];
			node->children[i]->max = childMaxs[i];
			node->children[i]->level = node->level + 1;
			buildRecursive(node->children[i].get(), points, childIndices[i], maxPointsPerNode, maxDepth);
		}
	}
	
	// If node has no children (all points outside or at boundaries), make it a leaf
	bool hasChildren = false;
	for (int i = 0; i < 8; ++i) {
		if (node->children[i] != nullptr) {
			hasChildren = true;
			break;
		}
	}
	
	if (!hasChildren) {
		node->isLeaf = true;
		node->pointIndices = indices;  // Keep original indices if no children were created
	}
}

std::vector<unsigned int> Octree::getVisiblePoints(const glm::mat4& viewProj, const glm::vec3& camPos,
                                                    float maxDistance) const {
	if (!mRoot) return {};
	
	std::vector<unsigned int> result;
	result.reserve(10000);  // Pre-allocate
	
	// Extract frustum planes
	Frustum frustum;
	frustum.extractFromMatrix(viewProj);
	
	getVisiblePointsRecursive(mRoot.get(), viewProj, camPos, maxDistance, result);
	
	return result;
}

void Octree::getVisiblePointsRecursive(const Node* node, const glm::mat4& viewProj,
                                       const glm::vec3& camPos, float maxDistance,
                                       std::vector<unsigned int>& outIndices) const {
	if (!node) return;
	
	// Check distance
	float dist = distanceToAABB(camPos, node->min, node->max);
	if (dist > maxDistance) return;
	
	// Check frustum intersection
	Frustum frustum;
	frustum.extractFromMatrix(viewProj);
	if (!frustum.intersectsAABB(node->min, node->max)) return;
	
	if (node->isLeaf) {
		// Leaf node - add all point indices
		outIndices.insert(outIndices.end(), node->pointIndices.begin(), node->pointIndices.end());
	} else {
		// Internal node - recurse into children
		for (int i = 0; i < 8; ++i) {
			if (node->children[i]) {
				getVisiblePointsRecursive(node->children[i].get(), viewProj, camPos, maxDistance, outIndices);
			}
		}
	}
}

std::vector<unsigned int> Octree::getLODPoints(const glm::vec3& camPos, float distance,
                                                float farThreshold, float nearThreshold) const {
	if (!mRoot) return {};
	
	std::vector<unsigned int> result;
	result.reserve(10000);
	
	getLODPointsRecursive(mRoot.get(), camPos, distance, farThreshold, nearThreshold, result);
	
	return result;
}

void Octree::getLODPointsRecursive(const Node* node, const glm::vec3& camPos, float distance,
                                    float farThreshold, float nearThreshold,
                                    std::vector<unsigned int>& outIndices) const {
	if (!node) return;
	
	// Calculate distance to node center
	float nodeDist = glm::length(camPos - node->center());
	float nodeSize = node->maxDim();
	
	// Distance-based LOD: if node is far or small, use it directly; otherwise subdivide
	bool useNode = (nodeDist > farThreshold) || (nodeSize / nodeDist < 0.01f);
	
	if (useNode || node->isLeaf) {
		// Use this node's points
		if (node->isLeaf) {
			outIndices.insert(outIndices.end(), node->pointIndices.begin(), node->pointIndices.end());
		} else {
			// Collect from children (or sample if too many)
			for (int i = 0; i < 8; ++i) {
				if (node->children[i] && node->children[i]->isLeaf) {
					outIndices.insert(outIndices.end(), 
					                  node->children[i]->pointIndices.begin(), 
					                  node->children[i]->pointIndices.end());
				}
			}
		}
	} else {
		// Subdivide - recurse into children
		for (int i = 0; i < 8; ++i) {
			if (node->children[i]) {
				getLODPointsRecursive(node->children[i].get(), camPos, distance, farThreshold, nearThreshold, outIndices);
			}
		}
	}
}

bool Octree::nodeIntersectsFrustum(const Node& node, const glm::mat4& viewProj) {
	Frustum frustum;
	frustum.extractFromMatrix(viewProj);
	return frustum.intersectsAABB(node.min, node.max);
}

float Octree::distanceToAABB(const glm::vec3& point, const glm::vec3& min, const glm::vec3& max) {
	glm::vec3 closest;
	closest.x = std::max(min.x, std::min(point.x, max.x));
	closest.y = std::max(min.y, std::min(point.y, max.y));
	closest.z = std::max(min.z, std::min(point.z, max.z));
	return glm::length(point - closest);
}

bool Octree::pointInAABB(const glm::vec3& point, const glm::vec3& min, const glm::vec3& max) {
	return point.x >= min.x && point.x <= max.x &&
	       point.y >= min.y && point.y <= max.y &&
	       point.z >= min.z && point.z <= max.z;
}

int Octree::getChildIndex(const glm::vec3& point, const glm::vec3& center) {
	int index = 0;
	if (point.x >= center.x) index |= 1;
	if (point.y >= center.y) index |= 2;
	if (point.z >= center.z) index |= 4;
	return index;
}

// Frustum is already implemented in RenderUtils.hpp, so we don't need to implement it here

} // namespace Graphics

