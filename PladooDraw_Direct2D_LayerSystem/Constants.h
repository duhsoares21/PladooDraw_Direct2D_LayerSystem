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

extern Microsoft::WRL::ComPtr<ID2D1Factory1> pD2DFactory;
extern ID2D1HwndRenderTarget* pRenderTarget;
extern ID2D1HwndRenderTarget* pLayerRenderTarget;
extern ID2D1SolidColorBrush* pBrush;
extern ID2D1SolidColorBrush* transparentBrush;

extern D2D1_ELLIPSE ellipse;
extern D2D1_RECT_F rectangle;
extern D2D1_RECT_F bitmapRect;
extern D2D1_RECT_F prevRectangle;
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

extern float zoomFactor;
extern int pixelSizeRatio;

extern bool isPixelMode;

extern bool isDrawingRectangle;
extern bool isDrawingEllipse;
extern bool isDrawingBrush;
extern bool isDrawingLine;
extern bool isPaintBucket;

extern std::vector<LayerOrder> layersOrder;
extern std::vector<Layer> layers;

extern std::vector<ACTION> Actions;
extern std::vector<ACTION> RedoActions;
extern std::vector<VERTICE> Vertices;
extern std::vector<std::pair<int, int>> pixelsToFill;

extern int width, height;

extern std::vector<LayerButton> LayerButtons;

extern POINT mouseLastClickPosition;

extern int selectedIndex;
extern bool selectedAction;

extern int layerIndex;
extern unsigned int lastHexColor;
