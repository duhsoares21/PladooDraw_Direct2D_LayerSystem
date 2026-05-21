#include "pch.h"
#include "constants.h"

const int DX[4] = { -1, 1, 0, 0 };
const int DY[4] = { 0, 0, -1, 1 };

const ColorRGBA COLOR_UNDEFINED = { -1.0f, -1.0f, -1.0f, -1.0f };

std::unordered_map<std::pair<int, int>, COLORREF, PairHash> bitmapData;

HWND mainHWND = NULL;
HWND docHWND = NULL;
HWND layerWindowHWND = NULL;
HWND layersHWND = NULL;
HWND layersControlButtonsGroupHWND = NULL;
HWND toolsHWND = NULL;
HWND timelineHWND = NULL;
HWND* hLayerButtons = NULL;
std::unique_ptr<IGraphicsBackend> gGraphicsBackend;

float logicalWidth = 0.0f;
float logicalHeight = 0.0f;

int lastActiveReplayFrame = 0;
int g_scrollOffsetTimeline = 0;
int replayPartialStepCount = 0;

bool hideShadow = false;

EllipseF ellipse = MakeEllipseF(MakePointF(0.0f, 0.0f), 0.0f, 0.0f);
RectF rectangle = MakeRectF(0.0f, 0.0f, 0.0f, 0.0f);
RectF textArea = MakeRectF(0.0f, 0.0f, 0.0f, 0.0f);
RectF bitmapRect = MakeRectF(0.0f, 0.0f, 0.0f, 0.0f);
RectF prevRectangle = MakeRectF(0.0f, 0.0f, 0.0f, 0.0f);
RectF prevTextArea = MakeRectF(0.0f, 0.0f, 0.0f, 0.0f);
EllipseF prevEllipse = MakeEllipseF(MakePointF(0.0f, 0.0f), 0.0f, 0.0f);

PointF startPoint = MakePointF(0.0f, 0.0f);
PointF endPoint = MakePointF(0.0f, 0.0f);

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

bool isMovingAction = false;
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

std::vector<LayerOrder> layersOrder;
std::vector<Layer> layerBitmaps;
std::vector<std::optional<Layer>> layers;

std::vector<ACTION> Actions;
std::vector<ACTION> RedoActions;

std::vector<ACTION> ReplayActions;
std::vector<ACTION> ReplayRedoActions;

std::vector<VERTICE> Vertices;
std::vector<std::pair<int, int>> pixelsToFill;

ACTION Clipboard;

int width, height;
int btnWidth = 90, btnHeight = 90;

HWND buttonUp, buttonDown, buttonPlus, buttonMinus;

std::vector<std::optional<LayerButton>> LayerButtons;
std::vector<std::optional<TimelineFrameButton>> TimelineFrameButtons;

POINT mouseLastClickPosition = { 0, 0 };

int actionId = 0;
int selectedActionId = -1;
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
