#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <memory>
#include <limits>
#include <algorithm>

namespace Graphics {

// Simple octree for point cloud spatial indexing
// Used for view-dependent culling and hierarchical LOD
class Octree {
public:
	struct Point {
		glm::vec3 position;
		unsigned int index;  // Index in original vertex array
	};
	
	struct Node {
		glm::vec3 min, max;  // AABB of this node
		std::vector<unsigned int> pointIndices;  // Indices of points in this node
		std::unique_ptr<Node> children[8];  // 8 children (octree)
		unsigned int level = 0;  // Depth level
		bool isLeaf = true;
		
		// AABB access
		glm::vec3 center() const { return 0.5f * (min + max); }
		glm::vec3 size() const { return max - min; }
		float maxDim() const { 
			glm::vec3 s = size();
			return std::max(s.x, std::max(s.y, s.z));
		}
	};
	
	Octree() = default;
	~Octree() = default;
	Octree(const Octree&) = delete;
	Octree& operator=(const Octree&) = delete;
	Octree(Octree&&) = default;
	Octree& operator=(Octree&&) = default;
	
	// Build octree from point cloud
	void build(const std::vector<Point>& points, const glm::vec3& min, const glm::vec3& max, 
	           unsigned int maxPointsPerNode = 1000, unsigned int maxDepth = 8);
	
	// Get points visible from camera (frustum culling)
	std::vector<unsigned int> getVisiblePoints(const glm::mat4& viewProj, const glm::vec3& camPos, 
	                                            float maxDistance = std::numeric_limits<float>::max()) const;
	
	// Get points within distance-based LOD (simplified for distance)
	std::vector<unsigned int> getLODPoints(const glm::vec3& camPos, float distance, 
	                                       float farThreshold = 100.0f, float nearThreshold = 10.0f) const;
	
	// Check if node intersects frustum
	static bool nodeIntersectsFrustum(const Node& node, const glm::mat4& viewProj);
	
	// Get root node
	const Node* root() const { return mRoot.get(); }
	
	// Check if octree is valid
	bool valid() const { return mRoot != nullptr; }
	
	// Get statistics
	unsigned int nodeCount() const { return mNodeCount; }
	unsigned int maxDepth() const { return mMaxDepth; }
	
private:
	std::unique_ptr<Node> mRoot;
	unsigned int mNodeCount = 0;
	unsigned int mMaxDepth = 0;
	
	void buildRecursive(Node* node, const std::vector<Point>& points, 
	                    const std::vector<unsigned int>& indices,
	                    unsigned int maxPointsPerNode, unsigned int maxDepth);
	
	void getVisiblePointsRecursive(const Node* node, const glm::mat4& viewProj, 
	                                const glm::vec3& camPos, float maxDistance,
	                                std::vector<unsigned int>& outIndices) const;
	
	void getLODPointsRecursive(const Node* node, const glm::vec3& camPos, float distance,
	                           float farThreshold, float nearThreshold,
	                           std::vector<unsigned int>& outIndices) const;
	
	// Helper: calculate distance from point to AABB
	static float distanceToAABB(const glm::vec3& point, const glm::vec3& min, const glm::vec3& max);
	
	// Helper: check if point is inside AABB
	static bool pointInAABB(const glm::vec3& point, const glm::vec3& min, const glm::vec3& max);
	
	// Helper: get child index for point
	static int getChildIndex(const glm::vec3& point, const glm::vec3& center);
};

// Forward declare Frustum from RenderUtils (we'll include it in the .cpp)
// This avoids duplication since RenderUtils already has a Frustum class

} // namespace Graphics

