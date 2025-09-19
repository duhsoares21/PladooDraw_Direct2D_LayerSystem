#include "pch.h"
#include "constants.h"

const int DX[4] = { -1, 1, 0, 0 };
const int DY[4] = { 0, 0, -1, 1 };

std::unordered_map<std::pair<int, int>, COLORREF, PairHash> bitmapData;

HWND mainHWND = NULL;
HWND docHWND = NULL;
HWND layersHWND = NULL;

Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> hWndLayerRenderTarget;
Microsoft::WRL::ComPtr<ID2D1Bitmap1> pD2DTargetBitmap;
Microsoft::WRL::ComPtr<ID2D1Factory1> pD2DFactory;

Microsoft::WRL::ComPtr <ID2D1SolidColorBrush> pBrush = nullptr;
ID2D1DeviceContext* pRenderTarget = nullptr;
ID2D1DeviceContext* pRenderTargetLayer = nullptr;

Microsoft::WRL::ComPtr<ID3D11Device> g_pD3DDevice;
Microsoft::WRL::ComPtr<ID3D11DeviceContext> g_pD3DContext;
Microsoft::WRL::ComPtr<IDXGISwapChain1> g_pSwapChain;
Microsoft::WRL::ComPtr<ID2D1Device> g_pD2DDevice;

Microsoft::WRL::ComPtr<IDWriteFactory> pDWriteFactory;

float logicalWidth = 0.0f;
float logicalHeight = 0.0f;

D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(0, 0), 0, 0);
D2D1_RECT_F rectangle = D2D1::RectF(0, 0, 0, 0);
D2D1_RECT_F textArea = D2D1::RectF(0, 0, 0, 0);
D2D1_RECT_F bitmapRect = D2D1::RectF(0, 0, 0, 0);
D2D1_RECT_F prevRectangle = D2D1::RectF(0, 0, 0, 0);
D2D1_RECT_F prevTextArea = D2D1::RectF(0, 0, 0, 0);
D2D1_ELLIPSE prevEllipse = D2D1::Ellipse(D2D1::Point2F(0, 0), 0, 0);

D2D1_POINT_2F startPoint = { 0, 0 };
D2D1_POINT_2F endPoint = { 0, 0 };

COLORREF currentColor = 0;

float defaultBrushSize = 1.0f;
float defaultEraserSize = 18.0f;
float currentBrushSize = 1.0f;
float currentEraserSize = 18.0f;

int selectedTool = 1;

int prevLeft = -1;
int prevTop = -1;

float dpiX = 96.0f;
float dpiY = 96.0f;
float zoomFactor = 1.0f;
int pixelSizeRatio = -1;

bool isPixelMode = false;

bool isDrawingRectangle = false;
bool isDrawingEllipse = false;
bool isDrawingBrush = false;
bool isDrawingLine = false;
bool isPaintBucket = false;
bool isWritingText = false;

std::vector<LayerOrder> layersOrder;
std::vector<std::optional<Layer>> layers;

std::vector<ACTION> Actions;
std::vector<ACTION> RedoActions;
std::vector<VERTICE> Vertices;
std::vector<std::pair<int, int>> pixelsToFill;

int width, height;

std::vector<std::optional<LayerButton>> LayerButtons;

POINT mouseLastClickPosition = { 0, 0 };

int selectedIndex = -1;
bool selectedAction = false;

int layerIndex = 0;
unsigned int lastHexColor = UINT_MAX;

std::wstring fontFace;
LONG fontWidth;
LONG fontWeight;
BYTE fontItalic;
BYTE fontStrike;
BYTE fontUnderline;
INT fontSize = 0;
COLORREF fontColor;
