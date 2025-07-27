#pragma once

struct Vector3
{
    // Constructor
    constexpr Vector3(
        const float x = 0.f,
        const float y = 0.f,
        const float z = 0.f) noexcept :
        x(x), y(y), z(z) {}

    // SIMD-optimized subtraction
    Vector3 operator-(const Vector3& other) const noexcept
    {
        if (std::is_constant_evaluated()) {
            return Vector3{ x - other.x, y - other.y, z - other.z };
        }
        __m128 a = _mm_set_ps(0.f, z, y, x);
        __m128 b = _mm_set_ps(0.f, other.z, other.y, other.x);
        __m128 r = _mm_sub_ps(a, b);
        alignas(16) float t[4]; _mm_store_ps(t, r);
        return Vector3{ t[0], t[1], t[2] };
    }

    // SIMD-optimized addition
    Vector3 operator+(const Vector3& other) const noexcept
    {
        if (std::is_constant_evaluated()) {
            return Vector3{ x + other.x, y + other.y, z + other.z };
        }
        __m128 a = _mm_set_ps(0.f, z, y, x);
        __m128 b = _mm_set_ps(0.f, other.z, other.y, other.x);
        __m128 r = _mm_add_ps(a, b);
        alignas(16) float t[4]; _mm_store_ps(t, r);
        return Vector3{ t[0], t[1], t[2] };
    }

    // SIMD-optimized scalar multiplication
    Vector3 operator*(const float f) const noexcept
    {
        if (std::is_constant_evaluated()) {
            return Vector3{ x * f, y * f, z * f };
        }
        __m128 v = _mm_set_ps(0.f, z, y, x);
        __m128 m = _mm_set1_ps(f);
        __m128 r = _mm_mul_ps(v, m);
        alignas(16) float t[4]; _mm_store_ps(t, r);
        return Vector3{ t[0], t[1], t[2] };
    }

    // SIMD-optimized scalar division
    Vector3 operator/(const float f) const noexcept
    {
        if (std::is_constant_evaluated()) {
            return Vector3{ x / f, y / f, z / f };
        }
        __m128 v = _mm_set_ps(0.f, z, y, x);
        __m128 d = _mm_set1_ps(f);
        __m128 r = _mm_div_ps(v, d);
        alignas(16) float t[4]; _mm_store_ps(t, r);
        return Vector3{ t[0], t[1], t[2] };
    }

    // Comparisons
    constexpr bool operator>(const Vector3& o) const noexcept { return x > o.x && y > o.y && z > o.z; }
    constexpr bool operator>=(const Vector3& o) const noexcept { return x >= o.x && y >= o.y && z >= o.z; }
    constexpr bool operator<(const Vector3& o) const noexcept { return x < o.x && y < o.y && z < o.z; }
    constexpr bool operator<=(const Vector3& o) const noexcept { return x <= o.x && y <= o.y && z <= o.z; }

    // Length squared in 2D
    float length2d_squared() const noexcept { return x * x + y * y; }

    // Full 3D length
    float length() const noexcept
    {
        if (std::is_constant_evaluated()) {
            return std::sqrt(x*x + y*y + z*z);
        }
        __m128 v = _mm_set_ps(0.f, z, y, x);
        __m128 sq = _mm_mul_ps(v, v);
        __m128 sum = _mm_hadd_ps(_mm_hadd_ps(sq, sq), sq);
        return std::sqrt(_mm_cvtss_f32(sum));
    }

    // 2D length
    float length2d() const noexcept
    {
        if (std::is_constant_evaluated()) {
            return std::sqrt(x*x + y*y);
        }
        __m128 v = _mm_set_ps(0.f, 0.f, y, x);
        __m128 sq = _mm_mul_ps(v, v);
        __m128 sum = _mm_hadd_ps(sq, sq);
        return std::sqrt(_mm_cvtss_f32(sum));
    }

    // Distance to another point
    float distance_to(const Vector3& p) const noexcept
    {
        if (std::is_constant_evaluated()) {
            float dx = p.x - x;
            float dy = p.y - y;
            float dz = p.z - z;
            return std::sqrt(dx*dx + dy*dy + dz*dz);
        }
        __m128 a = _mm_set_ps(0.f, z, y, x);
        __m128 b = _mm_set_ps(0.f, p.z, p.y, p.x);
        __m128 d = _mm_sub_ps(b, a);
        __m128 sq = _mm_mul_ps(d, d);
        __m128 sum = _mm_hadd_ps(_mm_hadd_ps(sq, sq), sq);
        return std::sqrt(_mm_cvtss_f32(sum));
    }

    // Dot product
    float dot(const Vector3& o) const noexcept
    {
        if (std::is_constant_evaluated()) {
            return x*o.x + y*o.y + z*o.z;
        }
        __m128 a = _mm_set_ps(0.f, z, y, x);
        __m128 b = _mm_set_ps(0.f, o.z, o.y, o.x);
        __m128 m = _mm_mul_ps(a, b);
        __m128 sum = _mm_hadd_ps(_mm_hadd_ps(m, m), m);
        return _mm_cvtss_f32(sum);
    }

    // SIMD-optimized cross product (corrected)
    Vector3 cross(const Vector3& o) const noexcept
    {
        if (std::is_constant_evaluated()) {
            return Vector3{
                y * o.z - z * o.y,
                z * o.x - x * o.z,
                x * o.y - y * o.x
            };
        }
        __m128 a = _mm_set_ps(0.f, z, y, x);
        __m128 b = _mm_set_ps(0.f, o.z, o.y, o.x);
        __m128 a_yzx = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1));
        __m128 b_zxy = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2));
        __m128 mul1  = _mm_mul_ps(a_yzx, b_zxy);
        __m128 a_zxy = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2));
        __m128 b_yzx = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1));
        __m128 mul2  = _mm_mul_ps(a_zxy, b_yzx);
        __m128 r     = _mm_sub_ps(mul1, mul2);
        alignas(16) float t[4]; _mm_store_ps(t, r);
        return Vector3{ t[0], t[1], t[2] };
    }

    // Convert to Euler angles (pitch, yaw)
    Vector3 ToAngle() const noexcept
    {
        float pitch = std::atan2(-z, std::hypot(x, y)) * (180.f / std::numbers::pi_v<float>);
        float yaw   = std::atan2(y, x) * (180.f / std::numbers::pi_v<float>);
        return Vector3{ pitch, yaw, 0.f };
    }

    // Normalize vector
    Vector3 normalize() const noexcept
    {
        float len = length();
        return len > 0.f ? *this / len : Vector3{};
    }

    // Check zero
    constexpr bool is_zero() const noexcept { return x == 0.f && y == 0.f && z == 0.f; }

    float x, y, z;
};