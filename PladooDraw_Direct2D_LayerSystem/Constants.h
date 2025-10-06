#pragma once
#include "Base.h"
#include "Structs.h"

// Arrays constantes
extern const int DX[4];
extern const int DY[4];

// Variáveis globais
extern std::unordered_map<std::pair<int, int>, COLORREF, PairHash> bitmapData;

extern HWND mainHWND;
extern HWND docHWND;
extern HWND layersHWND;
extern HWND* hLayerButtons;

extern Microsoft::WRL::ComPtr <ID2D1SolidColorBrush> pBrush;
extern Microsoft::WRL::ComPtr<ID2D1Bitmap1> pD2DTargetBitmap;
extern Microsoft::WRL::ComPtr<ID2D1Factory1> pD2DFactory;
extern Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> hWndLayerRenderTarget;

extern Microsoft::WRL::ComPtr <ID2D1DeviceContext> pRenderTarget;
extern Microsoft::WRL::ComPtr <ID2D1DeviceContext> pRenderTargetLayer;

extern Microsoft::WRL::ComPtr<ID3D11Device> g_pD3DDevice;
extern Microsoft::WRL::ComPtr<ID3D11DeviceContext> g_pD3DContext;
extern Microsoft::WRL::ComPtr<IDXGISwapChain1> g_pSwapChain;
extern Microsoft::WRL::ComPtr<ID2D1Device> g_pD2DDevice;

extern Microsoft::WRL::ComPtr<IDCompositionDevice> g_pDCompDevice;
extern Microsoft::WRL::ComPtr<IDCompositionTarget> g_pDCompTarget;
extern Microsoft::WRL::ComPtr<IDCompositionVisual> g_pDCompVisual;

extern Microsoft::WRL::ComPtr<IDWriteFactory> pDWriteFactory;

extern float logicalWidth;
extern float logicalHeight;

extern D2D1_ELLIPSE ellipse;
extern D2D1_RECT_F rectangle;
extern D2D1_RECT_F textArea;
extern D2D1_RECT_F bitmapRect;
extern D2D1_RECT_F prevRectangle;
extern D2D1_RECT_F prevTextArea;
extern D2D1_ELLIPSE prevEllipse;

extern D2D1_POINT_2F startPoint;
extern D2D1_POINT_2F endPoint;

extern COLORREF currentColor;

extern float defaultBrushSize;
extern float defaultEraserSize;
extern float currentBrushSize;
extern float currentEraserSize;

extern int selectedTool;

extern int prevLeft;
extern int prevTop;

extern float dpiX;
extern float dpiY;
extern float zoomFactor;
extern int pixelSizeRatio;

extern bool isPixelMode;

extern bool isDrawingRectangle;
extern bool isDrawingEllipse;
extern bool isDrawingBrush;
extern bool isDrawingLine;
extern bool isDrawingWindowText;
extern bool isWritingText;
extern bool isPaintBucket;

extern HWND hTextInput;
extern bool isWritingText;
extern WNDPROC oldEditProc;

extern std::string loadedFileName;

extern Microsoft::WRL::ComPtr<IDWriteTextFormat> pTextFormat;

extern std::vector<LayerOrder> layersOrder;
extern std::vector<std::optional<Layer>> layers;

extern std::vector<ACTION> Actions;
extern std::vector<ACTION> RedoActions;
extern std::vector<VERTICE> Vertices;
extern std::vector<std::pair<int, int>> pixelsToFill;

extern int width, height;

extern int btnWidth, btnHeight;

extern HWND buttonUp, buttonDown, buttonPlus, buttonMinus;

extern std::vector<std::optional<LayerButton>> LayerButtons;
extern std::vector<std::optional<LayerItem>> LayerItems;

extern POINT mouseLastClickPosition;

extern int selectedIndex;
extern bool selectedAction;

extern int layerIndex;
extern unsigned int lastHexColor;

extern std::wstring fontFace;
extern LONG fontWeight;
extern BYTE fontItalic;
extern BYTE fontStrike;
extern BYTE fontUnderline;
extern INT fontSize;
extern COLORREF fontColor;