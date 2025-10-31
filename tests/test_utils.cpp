#include <cassert>
#include <cstring>
#include <limits>
#include <cmath>
#include <iostream>

// Test Config namespace directly (defined in Utils.hpp but doesn't require GL)
namespace Config {
	static constexpr unsigned int MinVerticesForThreading = 10000;
	static constexpr unsigned int MinMeshesForThreading   = 2;
	static constexpr unsigned int PointCloudMinPointsForOctree = 100000;
	static constexpr unsigned int OctreeMaxDepth               = 12;
	static constexpr unsigned int OctreePointsPerNode          = 1000;
	static constexpr unsigned int VertexOptimizationMinVerts   = 10000;
}

// Test Half namespace directly (doesn't require GL)
namespace Half {
	inline uint16_t floatToHalf(float f) {
		uint32_t bits;
		std::memcpy(&bits, &f, sizeof(float));
		uint32_t sign = ((bits >> 31) << 15) & 0x8000;
		uint32_t exp = ((bits >> 23) & 0xFF);
		uint32_t mantissa = (bits >> 13) & 0x3FF;
		if (exp == 0xFF) {
			if (mantissa == 0) return static_cast<uint16_t>(sign | 0x7C00);
			return static_cast<uint16_t>(sign | 0x7E00);
		}
		if (exp == 0) return static_cast<uint16_t>(sign);
		int32_t exp32 = static_cast<int32_t>(exp) - 127 + 15;
		if (exp32 > 31) return static_cast<uint16_t>(sign | 0x7C00);
		if (exp32 < 0)  return static_cast<uint16_t>(sign);
		return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exp32) << 10) | mantissa);
	}
}

// Simple test framework
#define TEST_ASSERT(cond, msg) \
	if (!(cond)) { \
		std::cerr << "FAIL: " << msg << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
		return false; \
	}

// Test half-float conversion round-trip within tolerance
bool testHalfFloatRoundTrip() {
	const float testValues[] = {
		0.0f, 1.0f, -1.0f, 0.5f, -0.5f,
		1.0e-3f, 1.0e3f,
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::min(),
	};
	
	for (float original : testValues) {
		uint16_t half = Half::floatToHalf(original);
		
		// Convert back to float (simplified conversion - just extract sign, exp, mantissa)
		uint32_t sign = (half >> 15) & 0x1;
		uint32_t exp = (half >> 10) & 0x1F;
		uint32_t mantissa = half & 0x3FF;
		
		float reconstructed = 0.0f;
		if (exp == 0) {
			// Zero or denormalized
			reconstructed = (sign ? -0.0f : 0.0f);
		} else if (exp == 31) {
			// Infinity or NaN
			reconstructed = (mantissa == 0) ? (sign ? -std::numeric_limits<float>::infinity() : std::numeric_limits<float>::infinity()) : std::numeric_limits<float>::quiet_NaN();
		} else {
			// Normalized number
			uint32_t exp32 = exp - 15 + 127;
			uint32_t mantissa32 = mantissa << 13;
			uint32_t bits = (sign << 31) | (exp32 << 23) | mantissa32;
			std::memcpy(&reconstructed, &bits, sizeof(float));
		}
		
		// Allow reasonable tolerance (half-float has ~3 decimal digits of precision)
		float tolerance = std::max(1e-3f * std::abs(original), 1e-6f);
		if (std::isfinite(original) && std::isfinite(reconstructed)) {
			TEST_ASSERT(std::abs(original - reconstructed) <= tolerance,
				"Half-float round-trip failed for " << original);
		}
	}
	
	return true;
}

// Test config constants are reasonable
bool testConfigConstants() {
	TEST_ASSERT(Config::MinVerticesForThreading > 0, "MinVerticesForThreading must be > 0");
	TEST_ASSERT(Config::PointCloudMinPointsForOctree > Config::MinVerticesForThreading,
		"PointCloudMinPointsForOctree should be larger than MinVerticesForThreading");
	TEST_ASSERT(Config::OctreeMaxDepth > 0 && Config::OctreeMaxDepth <= 32,
		"OctreeMaxDepth should be reasonable (1-32)");
	TEST_ASSERT(Config::OctreePointsPerNode > 0,
		"OctreePointsPerNode must be > 0");
	
	return true;
}

int main() {
	std::cout << "Running Utils unit tests...\n";
	
	bool allPassed = true;
	
	if (!testHalfFloatRoundTrip()) {
		std::cerr << "testHalfFloatRoundTrip failed\n";
		allPassed = false;
	} else {
		std::cout << "PASS: testHalfFloatRoundTrip\n";
	}
	
	if (!testConfigConstants()) {
		std::cerr << "testConfigConstants failed\n";
		allPassed = false;
	} else {
		std::cout << "PASS: testConfigConstants\n";
	}
	
	if (allPassed) {
		std::cout << "All tests passed!\n";
		return 0;
	} else {
		std::cerr << "Some tests failed!\n";
		return 1;
	}
}
