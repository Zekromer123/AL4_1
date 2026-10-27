#pragma once
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include <string>
#include <vector>
#include <cstdint>

struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
	uint32_t boneIndices[4] = { 0, 0, 0, 0 };
	float boneWeights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

struct Material {
	Vector4 color;
	int32_t enableLighting;
	int32_t lightingType; // 0=None, 1=Lambert, 2=HalfLambert
	// Ink outline on box-shaped models (local space +-1). 0 = off.
	// Bits: 1=top, 2=bottom, 4=left, 8=right edge of the front face, 16=enable
	int32_t inkEdgeMask;
	float padding;
};

struct MaterialData {
	std::string textureFilePath;
};

struct ModelData {
	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
	MaterialData material;
};
