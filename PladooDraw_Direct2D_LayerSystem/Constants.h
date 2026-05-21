#pragma once
#include "CoreBase.h"
#include "GraphicsBackend.h"
#include "Structs.h"

#define BTN_WIDTH_DEFAULT 120
#define BTN_WIDTH_WIDE_DEFAULT 190

#define BTN_HEIGHT_DEFAULT 120
#define BTN_HEIGHT_WIDE_DEFAULT 120

// Arrays constantes
extern const int DX[4];
extern const int DY[4];

extern const ColorRGBA COLOR_UNDEFINED;

// Vari�veis globais
extern std::unordered_map<std::pair<int, int>, COLORREF, PairHash> bitmapData;

extern HWND mainHWND;
extern HWND docHWND;
extern HWND layerWindowHWND;
extern HWND layersHWND;
extern HWND layersControlButtonsGroupHWND;
extern HWND toolsHWND;
extern HWND timelineHWND;
extern HWND highlightFrame;
extern HWND* hLayerButtons;
extern std::unique_ptr<IGraphicsBackend> gGraphicsBackend;

extern float logicalWidth;
extern float logicalHeight;

extern int g_scrollOffsetTimeline;
extern int lastActiveReplayFrame;
extern int replayPartialStepCount;

extern bool hideShadow;

extern EllipseF ellipse;
extern RectF rectangle;
extern RectF textArea;
extern RectF bitmapRect;
extern RectF prevRectangle;
extern RectF prevTextArea;
extern EllipseF prevEllipse;

extern PointF startPoint;
extern PointF endPoint;

extern COLORREF currentColor;

extern float defaultBrushSize;
extern float defaultEraserSize;
extern float currentBrushSize;
extern float currentEraserSize;

extern int CurrentFrameIndex;
extern int selectedTool;

extern int prevLeft;
extern int prevTop;

extern float dpiX;
extern float dpiY;
extern float zoomFactor;
extern int pixelSizeRatio;

extern bool isPixelMode;
extern int isReplayMode;
extern int isAnimationMode;
extern bool isPlayingAnimation;

extern bool isMovingAction;
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

extern std::vector<LayerOrder> layersOrder;
extern std::vector<Layer> layerBitmaps;
extern std::vector<std::optional<Layer>> layers;

extern std::vector<ACTION> Actions;
extern std::vector<ACTION> RedoActions;

extern std::vector<ACTION> ReplayActions;
extern std::vector<ACTION> ReplayRedoActions;

extern std::vector<VERTICE> Vertices;
extern std::vector<std::pair<int, int>> pixelsToFill;

extern ACTION Clipboard;

extern int width, height;

extern int btnWidth, btnHeight;

extern HWND buttonUp, buttonDown, buttonPlus, buttonMinus;

extern std::vector<std::optional<LayerButton>> LayerButtons;
extern std::vector<std::optional<TimelineFrameButton>> TimelineFrameButtons;

extern POINT mouseLastClickPosition;

extern int actionId;
extern int selectedActionId;
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
