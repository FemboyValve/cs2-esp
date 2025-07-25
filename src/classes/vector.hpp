#pragma once

struct Vector3
{
    // Constructor - maintains exact same signature
    constexpr Vector3(
        const float x = 0.f,
        const float y = 0.f,
        const float z = 0.f) noexcept :
        x(x), y(y), z(z) { }

    // SIMD-optimized operators
    Vector3 operator-(const Vector3& other) const noexcept
    {
        if (std::is_constant_evaluated()) {
            // Constexpr fallback for compile-time evaluation
            return Vector3{ x - other.x, y - other.y, z - other.z };
        }
        
        __m128 a = _mm_set_ps(0.0f, z, y, x);
        __m128 b = _mm_set_ps(0.0f, other.z, other.y, other.x);
        __m128 result = _mm_sub_ps(a, b);
        
        alignas(16) float temp[4];
        _mm_store_ps(temp, result);
        return Vector3{ temp[0], temp[1], temp[2] };
    }

	float length2d_squared() const noexcept {
		return x * x + y * y;
	}

    Vector3 operator+(const Vector3& other) const noexcept
    {
        if (std::is_constant_evaluated()) {
            return Vector3{ x + other.x, y + other.y, z + other.z };
        }
        
        __m128 a = _mm_set_ps(0.0f, z, y, x);
        __m128 b = _mm_set_ps(0.0f, other.z, other.y, other.x);
        __m128 result = _mm_add_ps(a, b);
        
        alignas(16) float temp[4];
        _mm_store_ps(temp, result);
        return Vector3{ temp[0], temp[1], temp[2] };
    }

    Vector3 operator/(const float factor) const noexcept
    {
        if (std::is_constant_evaluated()) {
            return Vector3{ x / factor, y / factor, z / factor };
        }
        
        __m128 vec = _mm_set_ps(0.0f, z, y, x);
        __m128 divisor = _mm_set1_ps(factor);
        __m128 result = _mm_div_ps(vec, divisor);
        
        alignas(16) float temp[4];
        _mm_store_ps(temp, result);
        return Vector3{ temp[0], temp[1], temp[2] };
    }

    Vector3 operator*(const float factor) const noexcept
    {
        if (std::is_constant_evaluated()) {
            return Vector3{ x * factor, y * factor, z * factor };
        }
        
        __m128 vec = _mm_set_ps(0.0f, z, y, x);
        __m128 multiplier = _mm_set1_ps(factor);
        __m128 result = _mm_mul_ps(vec, multiplier);
        
        alignas(16) float temp[4];
        _mm_store_ps(temp, result);
        return Vector3{ temp[0], temp[1], temp[2] };
    }

    // Comparison operators remain the same for compatibility
    constexpr bool operator>(const Vector3& other) const noexcept {
        return x > other.x && y > other.y && z > other.z;
    }

    constexpr bool operator>=(const Vector3& other) const noexcept {
        return x >= other.x && y >= other.y && z >= other.z;
    }

    constexpr bool operator<(const Vector3& other) const noexcept {
        return x < other.x && y < other.y && z < other.z;
    }

    constexpr bool operator<=(const Vector3& other) const noexcept {
        return x <= other.x && y <= other.y && z <= other.z;
    }

    // Utility functions - enhanced with SIMD where beneficial
    Vector3 ToAngle() const noexcept
    {
        if (std::is_constant_evaluated()) {
            return Vector3{
                std::atan2(-z, std::hypot(x, y)) * (180.0f / std::numbers::pi_v<float>),
                std::atan2(y, x) * (180.0f / std::numbers::pi_v<float>),
                0.0f };
        }
        
        // For trigonometric functions, standard library is often optimal
        return Vector3{
            std::atan2(-z, std::hypot(x, y)) * (180.0f / std::numbers::pi_v<float>),
            std::atan2(y, x) * (180.0f / std::numbers::pi_v<float>),
            0.0f };
    }

    float length() const noexcept {
        if (std::is_constant_evaluated()) {
            return std::sqrt(x * x + y * y + z * z);
        }
        
        __m128 vec = _mm_set_ps(0.0f, z, y, x);
        __m128 squared = _mm_mul_ps(vec, vec);
        
        // Horizontal add to sum x², y², z²
        __m128 sum1 = _mm_hadd_ps(squared, squared);
        __m128 sum2 = _mm_hadd_ps(sum1, sum1);
        
        return std::sqrt(_mm_cvtss_f32(sum2));
    }

    float length2d() const noexcept {
        if (std::is_constant_evaluated()) {
            return std::sqrt(x * x + y * y);
        }
        
        __m128 vec = _mm_set_ps(0.0f, 0.0f, y, x);
        __m128 squared = _mm_mul_ps(vec, vec);
        
        __m128 sum1 = _mm_hadd_ps(squared, squared);
        return std::sqrt(_mm_cvtss_f32(sum1));
    }

    constexpr bool IsZero() const noexcept
    {
        return x == 0.f && y == 0.f && z == 0.f;
    }

    float calculate_distance(const Vector3& point) const noexcept {
        if (std::is_constant_evaluated()) {
            float dx = point.x - x;
            float dy = point.y - y;
            float dz = point.z - z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }
        
        __m128 a = _mm_set_ps(0.0f, z, y, x);
        __m128 b = _mm_set_ps(0.0f, point.z, point.y, point.x);
        __m128 diff = _mm_sub_ps(b, a);
        __m128 squared = _mm_mul_ps(diff, diff);
        
        __m128 sum1 = _mm_hadd_ps(squared, squared);
        __m128 sum2 = _mm_hadd_ps(sum1, sum1);
        
        return std::sqrt(_mm_cvtss_f32(sum2));
    }

    // Additional SIMD-optimized utility functions
    float dot(const Vector3& other) const noexcept {
        if (std::is_constant_evaluated()) {
            return x * other.x + y * other.y + z * other.z;
        }
        
        __m128 a = _mm_set_ps(0.0f, z, y, x);
        __m128 b = _mm_set_ps(0.0f, other.z, other.y, other.x);
        __m128 mul = _mm_mul_ps(a, b);
        
        __m128 sum1 = _mm_hadd_ps(mul, mul);
        __m128 sum2 = _mm_hadd_ps(sum1, sum1);
        
        return _mm_cvtss_f32(sum2);
    }

    Vector3 cross(const Vector3& other) const noexcept {
        if (std::is_constant_evaluated()) {
            return Vector3{
                y * other.z - z * other.y,
                z * other.x - x * other.z,
                x * other.y - y * other.x
            };
        }
        
        __m128 a = _mm_set_ps(0.0f, x, z, y);  // [0, x, z, y]
        __m128 b = _mm_set_ps(0.0f, other.y, other.x, other.z);  // [0, y, x, z]
        __m128 c = _mm_set_ps(0.0f, y, x, z);  // [0, y, x, z]
        __m128 d = _mm_set_ps(0.0f, other.x, other.z, other.y);  // [0, x, z, y]
        
        __m128 mul1 = _mm_mul_ps(a, b);
        __m128 mul2 = _mm_mul_ps(c, d);
        __m128 result = _mm_sub_ps(mul1, mul2);
        
        alignas(16) float temp[4];
        _mm_store_ps(temp, result);
        return Vector3{ temp[0], temp[1], temp[2] };
    }

    Vector3 normalize() const noexcept {
        float len = length();
        if (len == 0.0f) return Vector3{};
        return *this / len;
    }

    // Data members - unchanged for full compatibility
    float x, y, z;
};

// Helper function to check SIMD support at runtime
inline bool has_sse_support() noexcept {
    #ifdef __SSE__
        return true;
    #else
        return false;
    #endif
}