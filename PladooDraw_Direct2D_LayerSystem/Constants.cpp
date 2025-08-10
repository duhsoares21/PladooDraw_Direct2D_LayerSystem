#include "pch.h"
#include "constants.h"

const int DX[4] = { -1, 1, 0, 0 };
const int DY[4] = { 0, 0, -1, 1 };

std::unordered_map<std::pair<int, int>, COLORREF, PairHash> bitmapData;

HWND mainHWND = NULL;
HWND docHWND = NULL;
HWND layersHWND = NULL;

Microsoft::WRL::ComPtr<ID2D1Factory1> pD2DFactory;
ID2D1HwndRenderTarget* pRenderTarget = nullptr;
ID2D1HwndRenderTarget* pLayerRenderTarget = nullptr;
ID2D1SolidColorBrush* pBrush = nullptr;
ID2D1SolidColorBrush* transparentBrush = nullptr;

D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(0, 0), 0, 0);
D2D1_RECT_F rectangle = D2D1::RectF(0, 0, 0, 0);
D2D1_RECT_F bitmapRect = D2D1::RectF(0, 0, 0, 0);
D2D1_RECT_F prevRectangle = D2D1::RectF(0, 0, 0, 0);
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

float zoomFactor = 1.0f;
int pixelSizeRatio = -1;

bool isPixelMode = false;

bool isDrawingRectangle = false;
bool isDrawingEllipse = false;
bool isDrawingBrush = false;
bool isDrawingLine = false;
bool isPaintBucket = false;

std::vector<LayerOrder> layersOrder;
std::vector<Layer> layers;

std::vector<ACTION> Actions;
std::vector<ACTION> RedoActions;
std::vector<VERTICE> Vertices;
std::vector<std::pair<int, int>> pixelsToFill;

int width, height;

std::vector<LayerButton> LayerButtons;

POINT mouseLastClickPosition = { 0, 0 };

int selectedIndex = -1;
bool selectedAction = false;

int layerIndex = 0;
unsigned int lastHexColor = UINT_MAX;
