#include "pch.h"
#include "constants.h"

const int DX[4] = { -1, 1, 0, 0 };
const int DY[4] = { 0, 0, -1, 1 };

const D2D1_COLOR_F COLOR_UNDEFINED = { -1.0f, -1.0f, -1.0f, -1.0f };

std::unordered_map<std::pair<int, int>, COLORREF, PairHash> bitmapData;

HWND mainHWND = NULL;
HWND docHWND = NULL;
HWND layerWindowHWND = NULL;
HWND layersHWND = NULL;
HWND layersControlButtonsGroupHWND = NULL;
HWND toolsHWND = NULL;
HWND timelineHWND = NULL;
HWND* hLayerButtons = NULL;

Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> hWndLayerRenderTarget;
Microsoft::WRL::ComPtr<ID2D1Bitmap1> pD2DTargetBitmap;
Microsoft::WRL::ComPtr<ID2D1Factory1> pD2DFactory;

Microsoft::WRL::ComPtr <ID2D1SolidColorBrush> pBrush = nullptr;
Microsoft::WRL::ComPtr <ID2D1DeviceContext> pRenderTarget = nullptr;
Microsoft::WRL::ComPtr <ID2D1DeviceContext> pRenderTargetLayer = nullptr;

Microsoft::WRL::ComPtr<ID3D11Device> g_pD3DDevice;
Microsoft::WRL::ComPtr<ID3D11DeviceContext> g_pD3DContext;
Microsoft::WRL::ComPtr<IDXGISwapChain1> g_pSwapChain;
Microsoft::WRL::ComPtr<ID2D1Device> g_pD2DDevice;

Microsoft::WRL::ComPtr<ID3D11RenderTargetView> g_pRenderTargetView;
Microsoft::WRL::ComPtr<IDCompositionDevice> g_pDCompDevice;
Microsoft::WRL::ComPtr<IDCompositionTarget> g_pDCompTarget;
Microsoft::WRL::ComPtr<IDCompositionVisual> g_pDCompVisual;

Microsoft::WRL::ComPtr<IDWriteFactory> pDWriteFactory;

float logicalWidth = 0.0f;
float logicalHeight = 0.0f;

int lastActiveReplayFrame = 0;
int g_scrollOffsetTimeline = 0;

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

int CurrentFrameIndex = 0;
int selectedTool = 1;

int prevLeft = -1;
int prevTop = -1;

float dpiX = 96.0f;
float dpiY = 96.0f;
float zoomFactor = 1.0f;
int pixelSizeRatio = -1;

bool isPixelMode = false;
int isReplayMode = 0;
int isAnimationMode = 0;
bool isPlayingAnimation = false;

bool isDrawingRectangle = false;
bool isDrawingEllipse = false;
bool isDrawingBrush = false;
bool isDrawingLine = false;
bool isDrawingWindowText = false;
bool isPaintBucket = false;
bool isWritingText = false;

HWND hTextInput = nullptr;
HWND highlightFrame = nullptr;
WNDPROC oldEditProc;

std::string loadedFileName;

Microsoft::WRL::ComPtr<IDWriteTextFormat> pTextFormat;

std::vector<LayerOrder> layersOrder;
std::vector<Layer> layerBitmaps;
std::vector<std::optional<Layer>> layers;
std::vector<std::optional<Layer>> animationLayers;

std::vector<ACTION> Actions;
std::vector<ACTION> RedoActions;

std::vector<ACTION> ReplayActions;
std::vector<ACTION> ReplayRedoActions;

std::vector<VERTICE> Vertices;
std::vector<std::pair<int, int>> pixelsToFill;

int width, height;
int btnWidth = 90, btnHeight = 90;

HWND buttonUp, buttonDown, buttonPlus, buttonMinus;

std::vector<std::optional<LayerButton>> LayerButtons;
std::vector<std::optional<TimelineFrameButton>> TimelineFrameButtons;

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
