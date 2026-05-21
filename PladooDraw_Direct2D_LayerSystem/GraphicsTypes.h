#pragma once
#include "CoreBase.h"

struct PointF {
    float x = 0.0f;
    float y = 0.0f;
};

struct RectF {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

struct EllipseF {
    PointF point;
    float radiusX = 0.0f;
    float radiusY = 0.0f;
};

struct SizeU {
    uint32_t width = 0;
    uint32_t height = 0;
};

struct SizeF {
    float width = 0.0f;
    float height = 0.0f;
};

struct ColorRGBA {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct Matrix3x2 {
    float m11 = 1.0f;
    float m12 = 0.0f;
    float m21 = 0.0f;
    float m22 = 1.0f;
    float dx = 0.0f;
    float dy = 0.0f;
};

inline PointF MakePointF(float x, float y) {
    return PointF{ x, y };
}

inline RectF MakeRectF(float left, float top, float right, float bottom) {
    return RectF{ left, top, right, bottom };
}

inline EllipseF MakeEllipseF(PointF point, float radiusX, float radiusY) {
    return EllipseF{ point, radiusX, radiusY };
}

inline Matrix3x2 MakeIdentityMatrix3x2() {
    return Matrix3x2{};
}

inline Matrix3x2 MakeScaleMatrix3x2(float scaleX, float scaleY) {
    Matrix3x2 matrix;
    matrix.m11 = scaleX;
    matrix.m22 = scaleY;
    return matrix;
}

inline SizeF MakeSizeF(float width, float height) {
    return SizeF{ width, height };
}
