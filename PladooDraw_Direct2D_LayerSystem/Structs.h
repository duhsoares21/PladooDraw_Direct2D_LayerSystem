#pragma once
#include "Base.h"

struct VERTICE {
    float x;
    float y;
    int BrushSize;
};

struct EDGE {
    std::vector<VERTICE> vertices;
};

struct LINE {
    D2D1_POINT_2F startPoint;
    D2D1_POINT_2F endPoint;
};

struct FLOATPOINT {
    float x;
    float y;
};

struct ACTION {
    int Tool;
    int Layer;
    int FrameIndex;
    int isLayerVisible;
    D2D1_RECT_F Position;
    EDGE FreeForm;
    D2D1_ELLIPSE Ellipse;
    COLORREF FillColor;
    COLORREF Color;
    LINE Line;
    int BrushSize;
    bool IsFilled;
    bool isPixelMode;
    int mouseX;
    int mouseY;
    std::vector<FLOATPOINT> pixelsToFill;
    int PaintTarget;

    std::wstring Text;
    std::wstring FontFamily;   // e.g. "Arial"
    int FontSize;              // in DIPs (or tenths of a point if you prefer)
    int FontWeight;            // map to DWRITE_FONT_WEIGHT
    bool FontItalic;
    bool FontUnderline;
    bool FontStrike;
};

struct Layer {
    int LayerID;
    int FrameIndex;
    bool isActive;
    bool isVisible;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> pBitmap;
};

struct LayerOrder {
    int layerOrder;
    int layerIndex;
};

struct LineData {
    bool isClosed;
    int index;
};

struct LineGeometry {
    std::vector<D2D1_POINT_2F> closedLoop;
    COLORREF fillColor;
};

struct PIXEL {
    int x;
    int y;

    bool operator==(const PIXEL& other) const {
        return x == other.x && y == other.y;
    }
};

namespace std {
    template <>
    struct hash<PIXEL> {
        size_t operator()(const PIXEL& p) const {
            return std::hash<int>()(p.x) ^ (std::hash<int>()(p.y) << 1);
        }
    };
}

struct LayerButton {
    int LayerID;
    int FrameIndex;
    HWND button;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> deviceContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    bool isActive;
};

struct TimelineFrameButton {
    int LayerIndex;
    int FrameIndex; 
    HWND frame;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> deviceContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    ComPtr<ID2D1Bitmap1> bitmap;
};

struct PairHash {
    template <typename T1, typename T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

struct RenderData {
    D2D1_SIZE_U size;
    ComPtr<ID2D1DeviceContext> deviceContext;
    ComPtr<IDXGISwapChain1> swapChain;
    ComPtr<ID2D1Bitmap1> bitmap;
};