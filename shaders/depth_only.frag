#version 330 core
// Minimal fragment shader for depth-only pass
// Output nothing (depth is written automatically)
// This allows Early-Z to skip occluded fragments in the main pass

void main() {
	// Empty fragment shader - depth is written automatically by hardware
	// No color output needed (color writes are disabled in prepass)
}

