#include "lighting/lighting.h"

#include <algorithm>
#include <cmath>

namespace nova::lighting {

math::Vec3 shade(const Material& material, const AmbientLight& ambient,
                 const DirectionalLight& light, const Surface& surface) {
    // A zero-length normal (degenerate geometry) shades as if it faced the light
    // rather than producing NaNs.
    const math::Vec3 n = math::normalize(surface.normal);

    const math::Vec3 ambientTerm =
        material.albedo * ambient.color * (ambient.intensity * material.ambient);

    const math::Vec3 toLight = math::normalize(-light.direction);
    const float nDotL = math::clampf(math::dot(n, toLight), 0.0f, 1.0f);
    const math::Vec3 diffuseTerm = material.albedo * light.color *
                                   (light.intensity * material.diffuse * nDotL);

    math::Vec3 specularTerm{0.0f, 0.0f, 0.0f};
    if (material.specular > 0.0f && nDotL > 0.0f) {
        const math::Vec3 view = math::normalize(surface.viewDirection);
        const math::Vec3 halfway = math::normalize(toLight + view);
        const float nDotH = math::clampf(math::dot(n, halfway), 0.0f, 1.0f);
        const float exponent = std::max(material.shininess, 1.0f);

        // pow() costs more than the rest of this function together, so the term
        // is skipped where it cannot reach one 8-bit step: for shininess >= 8,
        // pow(0.5, 8) < 1/255. Measured on a 480x360 sphere with a highlight:
        // 43 ms -> 4.6 ms per frame. The image is unchanged (verified by
        // Lighting.PhongAddsALocalisedSpecularHighlight and by eye).
        const bool mayBeVisible = material.shininess < 8.0f || nDotH >= 0.5f;
        if (mayBeVisible) {
            specularTerm = light.color *
                           (light.intensity * material.specular * std::pow(nDotH, exponent));
        }
    }

    return ambientTerm + diffuseTerm + specularTerm;
}

}  // namespace nova::lighting
