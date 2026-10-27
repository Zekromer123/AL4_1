#pragma once
#include "3d/PrimitiveDrawer.h"
#include "3d/Collider.h"
#include "math/Vector3.h"
#include "math/Vector4.h"

namespace DrawHelper {

    void DrawWireBox(const Vector3& center, const Vector3& halfSize, const Vector4& color);

    void DrawWireBox(const AABBCollider& aabb, const Vector4& color);

    void DrawWireSphere(const Vector3& center, float radius, const Vector4& color, int subdivisions = 16);

    void DrawWireSphere(const SphereCollider& sphere, const Vector4& color, int subdivisions = 16);

} // namespace DrawHelper