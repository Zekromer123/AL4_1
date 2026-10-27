#include "DrawHelper.h"
#include <cmath>

namespace DrawHelper {

    void DrawWireBox(const Vector3& center, const Vector3& halfSize, const Vector4& color) {
        Vector3 min = center - halfSize;
        Vector3 max = center + halfSize;

        
        Vector3 p0 = { min.x, min.y, min.z };
        Vector3 p1 = { max.x, min.y, min.z };
        Vector3 p2 = { max.x, max.y, min.z };
        Vector3 p3 = { min.x, max.y, min.z };

        Vector3 p4 = { min.x, min.y, max.z };
        Vector3 p5 = { max.x, min.y, max.z };
        Vector3 p6 = { max.x, max.y, max.z };
        Vector3 p7 = { min.x, max.y, max.z };

        PrimitiveDrawer* drawer = PrimitiveDrawer::GetInstance();

        drawer->DrawLine3d(p0, p1, color);
        drawer->DrawLine3d(p1, p2, color);
        drawer->DrawLine3d(p2, p3, color);
        drawer->DrawLine3d(p3, p0, color);

       
        drawer->DrawLine3d(p4, p5, color);
        drawer->DrawLine3d(p5, p6, color);
        drawer->DrawLine3d(p6, p7, color);
        drawer->DrawLine3d(p7, p4, color);

       
        drawer->DrawLine3d(p0, p4, color);
        drawer->DrawLine3d(p1, p5, color);
        drawer->DrawLine3d(p2, p6, color);
        drawer->DrawLine3d(p3, p7, color);
    }

    void DrawWireBox(const AABBCollider& aabb, const Vector4& color) {
        const Vector3& min = aabb.min;
        const Vector3& max = aabb.max;

        Vector3 p0 = { min.x, min.y, min.z };
        Vector3 p1 = { max.x, min.y, min.z };
        Vector3 p2 = { max.x, max.y, min.z };
        Vector3 p3 = { min.x, max.y, min.z };

        Vector3 p4 = { min.x, min.y, max.z };
        Vector3 p5 = { max.x, min.y, max.z };
        Vector3 p6 = { max.x, max.y, max.z };
        Vector3 p7 = { min.x, max.y, max.z };

        PrimitiveDrawer* drawer = PrimitiveDrawer::GetInstance();

        drawer->DrawLine3d(p0, p1, color);
        drawer->DrawLine3d(p1, p2, color);
        drawer->DrawLine3d(p2, p3, color);
        drawer->DrawLine3d(p3, p0, color);

        drawer->DrawLine3d(p4, p5, color);
        drawer->DrawLine3d(p5, p6, color);
        drawer->DrawLine3d(p6, p7, color);
        drawer->DrawLine3d(p7, p4, color);

        drawer->DrawLine3d(p0, p4, color);
        drawer->DrawLine3d(p1, p5, color);
        drawer->DrawLine3d(p2, p6, color);
        drawer->DrawLine3d(p3, p7, color);
    }

    void DrawWireSphere(const Vector3& center, float radius, const Vector4& color, int subdivisions) {
        if (subdivisions < 8) subdivisions = 8;
        PrimitiveDrawer* drawer = PrimitiveDrawer::GetInstance();

        const float kTwoPi = 6.28318530718f;
        float step = kTwoPi / static_cast<float>(subdivisions);

        
        for (int i = 0; i < subdivisions; ++i) {
            float angle1 = i * step;
            float angle2 = (i + 1) * step;

            Vector3 p1 = { center.x + std::cos(angle1) * radius, center.y + std::sin(angle1) * radius, center.z };
            Vector3 p2 = { center.x + std::cos(angle2) * radius, center.y + std::sin(angle2) * radius, center.z };
            drawer->DrawLine3d(p1, p2, color);
        }

        
        for (int i = 0; i < subdivisions; ++i) {
            float angle1 = i * step;
            float angle2 = (i + 1) * step;

            Vector3 p1 = { center.x + std::cos(angle1) * radius, center.y, center.z + std::sin(angle1) * radius };
            Vector3 p2 = { center.x + std::cos(angle2) * radius, center.y, center.z + std::sin(angle2) * radius };
            drawer->DrawLine3d(p1, p2, color);
        }
    }

    void DrawWireSphere(const SphereCollider& sphere, const Vector4& color, int subdivisions) {
        DrawWireSphere(sphere.center, sphere.radius, color, subdivisions);
    }

} // namespace DrawHelper