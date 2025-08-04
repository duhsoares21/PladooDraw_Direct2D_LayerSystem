#pragma once
#include <windows.h>
#include <d2d1.h>
#include <vector>
#include <wrl/client.h>

struct VERTICE {
    float x;
    float y;
};

struct EDGE {
    std::vector<VERTICE> vertices;
};

struct LINE {
    D2D1_POINT_2F startPoint;
    D2D1_POINT_2F endPoint;
};

struct ACTION {
    int Tool;
    int Layer;
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
    std::vector<POINT> pixelsToFill;
};

    struct Layer {
        Microsoft::WRL::ComPtr <ID2D1BitmapRenderTarget> pBitmapRenderTarget;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> pBitmap;
        HBITMAP hBitmap;
        bool tempLayer;
    };

    struct LayerOrder {
        int layerOrder;
        int layerIndex;
        std::vector<D2D1_COLOR_F> indexedColors;
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
        HWND button;
        HBITMAP hBitmap;
        bool isInitialPainted;
    };

struct PairHash {
    template <typename T1, typename T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

D2D1_COLOR_F GetRGBColor(COLORREF color);
template <class T> void SafeRelease(T** ppT);
extern "C" __declspec(dllexport) void AddLayerButton(HWND layerButton);

extern "C" __declspec(dllexport) HRESULT Initialize(HWND pmainHWND, HWND pdocHWND, int pWidth, int pHeight, int pPixelSizeRatio);
HRESULT InitializeDocument(HWND hWnd, int pWidth, int pHeight, int pPixelSizeRatio);
extern "C" __declspec(dllexport) HRESULT InitializeLayerRenderPreview();
extern "C" __declspec(dllexport) HRESULT InitializeLayers(HWND hWnd);

extern "C" __declspec(dllexport) void SetSelectedTool(int pselectedTool);

extern "C" __declspec(dllexport) void __stdcall SaveProjectDll(const char* pathAnsi);
extern "C" __declspec(dllexport) void __stdcall LoadProjectDll(const char* pathAnsi, HWND hWndLayer, HINSTANCE hLayerInstance, int btnWidth, int btnHeight, HWND* hLayerButtons, int* layerID, const wchar_t* szButtonClass, const wchar_t* msgText);

extern "C" __declspec(dllexport) void handleMouseUp();
extern "C" __declspec(dllexport) void Undo();
extern "C" __declspec(dllexport) void Redo();
extern "C" __declspec(dllexport) float __stdcall GetZoomFactor();
extern "C" __declspec(dllexport) void ZoomIn_Default();
extern "C" __declspec(dllexport) void ZoomOut_Default();
extern "C" __declspec(dllexport) void ZoomIn(float zoomIncrement);
extern "C" __declspec(dllexport) void ZoomOut(float zoomIncrement);
extern "C" __declspec(dllexport) void Zoom();
extern "C" __declspec(dllexport) void IncreaseBrushSize_Default();
extern "C" __declspec(dllexport) void DecreaseBrushSize_Default();
extern "C" __declspec(dllexport) void IncreaseBrushSize(float sizeIncrement);
extern "C" __declspec(dllexport) void DecreaseBrushSize(float sizeIncrement);

extern "C" __declspec(dllexport) HRESULT AddLayer(bool fromFile);	
extern "C" __declspec(dllexport) HRESULT RemoveLayer();
extern "C" __declspec(dllexport) HRESULT __stdcall RecreateLayers(HWND hWndLayer, HINSTANCE hLayerInstance, int btnWidth, int btnHeight, HWND* hLayerButtons, int& layerID, const wchar_t* szButtonClass, const wchar_t* msgText);
extern "C" __declspec(dllexport) int GetLayer();
extern "C" __declspec(dllexport) void SetLayer(int index);
extern "C" __declspec(dllexport) void ReorderLayerUp();
extern "C" __declspec(dllexport) void ReorderLayerDown();
extern "C" __declspec(dllexport) int LayersCount();
extern "C" __declspec(dllexport) void UpdateLayers(int layerIndexTarget);
extern "C" __declspec(dllexport) void RenderLayers();
extern "C" __declspec(dllexport) void DrawLayerPreview(int currentLayer);
extern "C" __declspec(dllexport) void __stdcall SelectTool(int xInitial, int yInitial);
extern "C" __declspec(dllexport) void __stdcall UnSelectTool();
extern "C" __declspec(dllexport) void __stdcall MoveTool(int xInitial, int yInitial, int x, int y);
extern "C" __declspec(dllexport) void EraserTool(int left, int top);
extern "C" __declspec(dllexport) void BrushTool(int left, int top, COLORREF hexColor, bool pixelMode, int pPixelSizeRatio);
extern "C" __declspec(dllexport) void RectangleTool(int left, int top, int right, int bottom, unsigned int hexColor);
extern "C" __declspec(dllexport) void EllipseTool(int left, int top, int right, int bottom, unsigned int hexColor);
extern "C" __declspec(dllexport) void LineTool(int xInitial, int yInitial, int x, int y, unsigned int hexColor);
extern "C" __declspec(dllexport) void PaintBucketTool(int xInitial, int yInitial, COLORREF hexFillColor, HWND hWnd);
extern "C" __declspec(dllexport) void Cleanup();