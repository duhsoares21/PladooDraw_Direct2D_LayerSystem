// LayersDLL.cpp
#include "pch.h"
#include "LayersDLL.h"

#include <algorithm>
#include <cmath>
#include <codecvt>
#include <corecrt_math_defines.h>
#include <cstdio>
#include <d2d1_1.h>
#include <fstream>
#include <numbers>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_set>
#include <vector>
#include <wincodec.h>
#include <windows.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;
using namespace std;

struct VERTICE {
    float x;
    float y;
};

struct EDGE {
	std::vector<VERTICE> vertices;
};

struct ACTION {
    int Tool;
    int Layer;
    D2D1_RECT_F Position;
	EDGE FreeForm;
    D2D1_ELLIPSE Ellipse;
    COLORREF FillColor;
    COLORREF Color;
    D2D1_POINT_2F startPoint;
    D2D1_POINT_2F endPoint;
    int BrushSize;
    bool IsFilled;
};

struct PIXEL {
    int x;
    int y;

    bool operator==(const PIXEL& other) const {
        return x == other.x && y == other.y;
    }
};

struct Layer {
    ID2D1BitmapRenderTarget* pBitmapRenderTarget;
    ID2D1Bitmap* pBitmap;
};

struct LayerOrder {
    int layerOrder;
    int layerIndex;
};

struct LayerButton {
    HWND button;
    HBITMAP hBitmap;
    bool isInitialPainted;
};

enum Tools
{
   TEraser,
   TBrush,
   TRectangle,
   TEllipse,
   TLine,
   TPaintBucket
};

HWND mainHWND = NULL;
HWND layersHWND = NULL;

IWICImagingFactory* pWICFactory = nullptr;
ID2D1Factory* pD2DFactory = nullptr;
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

static int currentBrushSize = 0;

static int prevLeft = -1;
static int prevTop = -1;

static bool isDrawingRectange = false;
static bool isDrawingEllipse = false;
static bool isDrawingBrush = false;
static bool isDrawingLine = false;
static bool isFloodFill = false;

std::vector<LayerOrder> layersOrder;
std::vector<Layer> layers;
std::vector<ACTION> Actions;
std::vector<ACTION> RedoActions;
std::vector<VERTICE> Vertices;

std::vector<LayerButton> LayerButtons;

POINT mouseLastClickPosition = { 0, 0 };

int layerIndex = 0;
unsigned int lastHexColor = UINT_MAX;

/* MAIN */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH: 

            break;

        case DLL_PROCESS_DETACH:
        
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }

    return TRUE;
}

extern "C" __declspec(dllexport) HRESULT Initialize(HWND hWnd) {
    Actions.reserve(10000);

    mainHWND = hWnd;

    HRESULT factoryResult = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);

    if (FAILED(factoryResult)) {
        MessageBox(hWnd, L"Erro ao criar Factory", L"Erro", MB_OK);

        return factoryResult;
    }

    RECT rc;
    GetClientRect(hWnd, &rc);

    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HRESULT renderResult = pD2DFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_HARDWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            0, 0,
            D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE
        ),
        D2D1::HwndRenderTargetProperties(hWnd, size),
        &pRenderTarget
    );

    if (FAILED(renderResult))
    {
        MessageBox(hWnd, L"Erro ao criar hWndRenderTarget", L"Erro", MB_OK);
        return renderResult;
    }
       
    AddLayer();

    RenderLayers();

    DrawLayerPreview(layerIndex);
    
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT InitializeLayers(HWND hWnd) {
    layersHWND = hWnd;

    return S_OK;
}
/* HELPERS */

void CreateLogData(std::string fileName, std::string content) {
    // Open the file in append mode
    std::ofstream outFile(fileName, std::ios::app);

    // Check if the file is open
    if (!outFile.is_open()) {
        MessageBox(nullptr, L"Failed to open file for writing", L"Error", MB_OK);
        return;
    }

    // Write the text to the file
    outFile << content << std::endl;

    // Close the file
    outFile.close();

    return;
}

ID2D1SolidColorBrush* D2_CreateSolidBrush(COLORREF hexColor) {
    currentColor = hexColor;

    if (lastHexColor == UINT_MAX || hexColor != lastHexColor) {

        DeleteObject(pBrush);

        D2D1_COLOR_F color = GetRGBColor(hexColor);

        pRenderTarget->CreateSolidColorBrush(color, &pBrush);
    }

    lastHexColor = hexColor;

    return pBrush;
}

D2D1_COLOR_F GetRGBColor(COLORREF color) {
    float r = (color & 0xFF) / 255.0f;         
    float g = ((color >> 8) & 0xFF) / 255.0f;  
    float b = ((color >> 16) & 0xFF) / 255.0f; 
    float a = 1.0f;                            

    D2D1_COLOR_F RGBColor = { r, g, b, a };

    return RGBColor;
}

RECT scalePointsToButton(int x, int y, int drawingAreaWidth, int drawingAreaHeight, int buttonWidth, int buttonHeight) {
    float scaleX = buttonWidth / static_cast<float>(drawingAreaWidth);
    float scaleY = buttonHeight / static_cast<float>(drawingAreaHeight);

    float scale = min(scaleX, scaleY);

    int offsetX = (buttonWidth - static_cast<int>(drawingAreaWidth * scale)) / 2;
    int offsetY = (buttonHeight - static_cast<int>(drawingAreaHeight * scale)) / 2;

    x = static_cast<int>(x * scale) + offsetX;
    y = static_cast<int>(y * scale) + offsetY;

    RECT rect = { x, y, 0, 0 };
    return rect;
}

extern "C" __declspec(dllexport) void AddLayerButton(HWND layerButton) {

    RECT rc;
    GetClientRect(layerButton, &rc);

    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HDC hdc = GetDC(layerButton);
            
    HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
    HBITMAP hBitmap = CreateCompatibleBitmap(hdc, size.width, size.height);

    SelectObject(hdc, hBrush);
    FillRect(hdc, &rc, hBrush);

    SendMessage(layerButton, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap);
    
    LayerButtons.push_back({ layerButton , hBitmap });

    DeleteObject(hBrush);
    ReleaseDC(layerButton, hdc);
}

/* ACTIONS */

extern "C" __declspec(dllexport) void handleMouseUp() {

    if (isDrawingRectange || isDrawingEllipse || isDrawingLine) {
        ID2D1SolidColorBrush* brush = D2_CreateSolidBrush(currentColor);

        layers[layerIndex].pBitmapRenderTarget->BeginDraw();              

        if (isDrawingRectange) {
            layers[layerIndex].pBitmapRenderTarget->PushAxisAlignedClip(rectangle, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            layers[layerIndex].pBitmapRenderTarget->FillRectangle(rectangle, brush);
            layers[layerIndex].pBitmapRenderTarget->PopAxisAlignedClip();

            ACTION action;
            action.Tool = 2;
            action.Layer = layerIndex;
            action.Position = rectangle;
            action.Color = currentColor;
            Actions.push_back(action);

            rectangle = D2D1::RectF(0, 0, 0, 0);
            isDrawingRectange = false;
        }
        
        if (isDrawingEllipse) {
            layers[layerIndex].pBitmapRenderTarget->FillEllipse(ellipse, brush);

            ACTION action;
            action.Tool = 3;
            action.Layer = layerIndex;
            action.Position = rectangle;
            action.Ellipse = ellipse;
            action.Color = currentColor;
            Actions.push_back(action);

            ellipse = D2D1::Ellipse(D2D1::Point2F(0, 0), 0, 0);
            isDrawingEllipse = false;
        }

        if (isDrawingLine) {
            layers[layerIndex].pBitmapRenderTarget->DrawLine(startPoint, endPoint, brush, currentBrushSize, nullptr);

            ACTION action;
            action.Tool = 4;
            action.Layer = layerIndex;
            action.startPoint = startPoint;
            action.endPoint = endPoint;
            action.Color = currentColor;
            action.BrushSize = currentBrushSize;
            Actions.push_back(action);

            isDrawingLine = false;
        }
    
        layers[layerIndex].pBitmapRenderTarget->EndDraw();

        layersOrder.pop_back();
        layers.pop_back();
    }
   
	if (isDrawingBrush) {

        ACTION action;
        action.Tool = TBrush;
        action.Layer = layerIndex;
        action.Color = currentColor;
        action.BrushSize = currentBrushSize;
		action.FillColor = RGB(255, 255, 255);
		action.FreeForm.vertices = Vertices;
        action.IsFilled = false;
        Actions.push_back(action);

		Vertices.clear();

		isDrawingBrush = false;
	}

    startPoint = D2D1::Point2F(0, 0);
    endPoint = D2D1::Point2F(0, 0);
 
    bitmapRect = D2D1::RectF(0, 0, 0, 0);
    prevRectangle = D2D1::RectF(0, 0, 0, 0);
    prevEllipse = D2D1::Ellipse(D2D1::Point2F(0, 0), 0, 0);
    prevLeft = -1;
    prevTop = -1;
}

extern "C" __declspec(dllexport) void Undo() {
    if (Actions.size() > 0) {
        RedoActions.push_back(Actions[Actions.size() - 1]);
        Actions.pop_back();
    }
}

extern "C" __declspec(dllexport) void Redo() {
    if (RedoActions.size() > 0) {
        Actions.push_back(RedoActions[RedoActions.size() - 1]);
        RedoActions.pop_back();
    }
}

/* LAYERS */

extern "C" __declspec(dllexport) HRESULT AddLayer() {
    ID2D1BitmapRenderTarget* pBitmapRenderTarget = nullptr;

    D2D1_SIZE_F size = pRenderTarget->GetSize();

    D2D1_PIXEL_FORMAT pixelFormat = D2D1::PixelFormat(
        DXGI_FORMAT_B8G8R8A8_UNORM,
        D2D1_ALPHA_MODE_PREMULTIPLIED
    );

    D2D1_BITMAP_PROPERTIES properties = D2D1::BitmapProperties(pixelFormat);

    HRESULT renderTargetCreated = pRenderTarget->CreateCompatibleRenderTarget(
        D2D1::SizeF(size.width, size.height), &pBitmapRenderTarget);

    if (FAILED(renderTargetCreated)) {
        MessageBox(NULL, L"Erro ao criar BitmapRenderTarget", L"Erro", MB_OK);
        return renderTargetCreated;
    }

    ID2D1Bitmap* pBitmap = nullptr;

    renderTargetCreated = pBitmapRenderTarget->CreateBitmap(
        D2D1::SizeU(static_cast<UINT32>(size.width), static_cast<UINT32>(size.height)),
        nullptr, 0, properties, &pBitmap);

    pBitmapRenderTarget->GetBitmap(&pBitmap);

    Layer layer = { pBitmapRenderTarget, pBitmap };
    layers.push_back(layer);

    LayerOrder layerOrder = { layers.size() - 1, layers.size() - 1 };
    layersOrder.push_back(layerOrder);

    return S_OK;
}

extern "C" __declspec(dllexport) void SetLayer(int index) {
    layerIndex = index;
    return;
}

extern "C" __declspec(dllexport) void ReorderLayerUp() {
    if (layersOrder[layerIndex].layerOrder > 0) {
        int previousOrder = layersOrder[layerIndex].layerOrder;
        int targetOrder = previousOrder - 1;

        int index = std::distance(layersOrder.begin(),
            std::find_if(layersOrder.begin(), layersOrder.end(),
                [targetOrder](const LayerOrder& l) { return l.layerOrder == targetOrder; })
        );

        layersOrder[layerIndex].layerOrder = layersOrder[index].layerOrder;
        layersOrder[index].layerOrder = previousOrder;
    }
    return;
}

extern "C" __declspec(dllexport) void ReorderLayerDown() {
    if (layersOrder[layerIndex].layerOrder < layers.size() - 1) {
        int previousOrder = layersOrder[layerIndex].layerOrder;
        int targetOrder = previousOrder + 1;

        int index = std::distance(layersOrder.begin(),
            std::find_if(layersOrder.begin(), layersOrder.end(),
                [targetOrder](const LayerOrder& l) { return l.layerOrder == targetOrder; })
        );

        layersOrder[layerIndex].layerOrder = layersOrder[index].layerOrder;
        layersOrder[index].layerOrder = previousOrder;
    }
    return;
}

extern "C" __declspec(dllexport) int LayersCount() {
    return layers.size();
}

float CrossProduct(const VERTICE& p1, const VERTICE& p2, const VERTICE& p3) {
    return (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);
}

bool DoLineSegmentsIntersect(const VERTICE& A, const VERTICE& B, const VERTICE& C, const VERTICE& D) {
    float d1 = CrossProduct(C, D, A);
    float d2 = CrossProduct(C, D, B);
    float d3 = CrossProduct(A, B, C);
    float d4 = CrossProduct(A, B, D);

    return (d1 * d2 < 0) && (d3 * d4 < 0);
}

bool IsSelfIntersecting(const EDGE& edge) {
    size_t n = edge.vertices.size();

    for (size_t i = 0; i < n - 1; i++) {
        for (size_t j = i + 2; j < n - 1; j++) {
            if (DoLineSegmentsIntersect(edge.vertices[i], edge.vertices[i + 1], edge.vertices[j], edge.vertices[j + 1])) {
                return true;
            }
        }
    }

    return false;
}

bool IsPointInsidePolygon(const EDGE& edge, const VERTICE& point) {
    size_t n = edge.vertices.size();
    bool isInside = false;

    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const VERTICE& vi = edge.vertices[i];
        const VERTICE& vj = edge.vertices[j];

        if (((vi.y > point.y) != (vj.y > point.y)) && // Check if point is between y-coordinates of the edge
            (point.x < (vj.x - vi.x) * (point.y - vi.y) / (vj.y - vi.y) + vi.x)) { // Calculate intersection
            isInside = !isInside;
        }
    }

    return isInside;
}

bool IsPointInsideRectF(const D2D1_RECT_F& rect, const VERTICE& point) {
    return (point.x >= rect.left && point.x <= rect.right &&
        point.y >= rect.top && point.y <= rect.bottom);
}

bool IsPointInsideEllipse(const D2D1_ELLIPSE& ellipse, const VERTICE& point) {
    float dx = point.x - ellipse.point.x;
    float dy = point.y - ellipse.point.y;

    return (dx * dx) / (ellipse.radiusX * ellipse.radiusX) +
        (dy * dy) / (ellipse.radiusY * ellipse.radiusY) <= 1.0f;
}

bool IsProximityClose(EDGE edge, float closureThreshold) {
    bool isProximityClosed = (std::hypot(
        edge.vertices[0].x - edge.vertices.back().x,
        edge.vertices[0].y - edge.vertices.back().y) <= closureThreshold);

    float area = 0.0f;
    size_t n = edge.vertices.size();

    for (size_t j = 0; j < n; ++j) {
        size_t k = (j + 1) % n; // Next vertex, wrapping around at the end
        area += edge.vertices[j].x * edge.vertices[k].y;
        area -= edge.vertices[k].x * edge.vertices[j].y;
    }

    return isProximityClosed && (std::abs(area) / 2.0f > 0.0f);
}

float CalculateAngle(const VERTICE& p1, const VERTICE& p2, const VERTICE& reference) {
    float dx1 = p1.x - reference.x;
    float dy1 = p1.y - reference.y;
    float dx2 = p2.x - reference.x;
    float dy2 = p2.y - reference.y;

    // atan2 calculates the angle in radians (-π to π)
    float angle1 = std::atan2(dy1, dx1);
    float angle2 = std::atan2(dy2, dx2);

    // Difference between angles
    float angle = angle2 - angle1;

    // Normalize the angle to the range (-π to π)
    if (angle > M_PI) {
        angle -= 2 * M_PI;
    }
    else if (angle < -M_PI) {
        angle += 2 * M_PI;
    }

    return angle;
}

bool IsClosedByWindingNumber(const EDGE& edge) {
    if (edge.vertices.size() < 3) {
        return false;
    }

    float totalWindingNumber = 0.0f;

    for (size_t i = 0; i < edge.vertices.size(); ++i) {
        size_t nextIndex = (i + 1) % edge.vertices.size();
        totalWindingNumber += CalculateAngle(
            edge.vertices[i],
            edge.vertices[nextIndex],
            edge.vertices[0]
        );
    }

    return std::abs(totalWindingNumber) > 1e-5;
}

bool toolExists(const std::vector<ACTION>& Actions, const Tools& targetTool) {
    return std::any_of(Actions.begin(), Actions.end(), [&](const ACTION& action) {
        return action.Tool == targetTool;
    });
}

std::vector<ACTION> getActionsWithTool(const std::vector<ACTION>& actions, Tools tool) {
    std::vector<ACTION> result;

    for (const auto& action : actions) {
        if (action.Tool == tool) {
            result.push_back(action);
        }
    }

    return result;
}

extern "C" __declspec(dllexport) void UpdateLayers() {
    if (!pRenderTarget) {
        return;
    }

    pRenderTarget->BeginDraw();
    pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

    for (size_t i = 0; i < layers.size(); ++i) {
        if (layers[i].pBitmap) {
            layers[i].pBitmapRenderTarget->BeginDraw();
            layers[i].pBitmapRenderTarget->Clear(D2D1::ColorF(0,0,0,0));
            layers[i].pBitmapRenderTarget->EndDraw();
        }
    }

    if (toolExists(Actions, TPaintBucket)) {
        std::vector<ACTION> paintActions = getActionsWithTool(Actions, TPaintBucket);
        for (size_t i = 0; i < paintActions.size(); i++) {
            D2D1_COLOR_F fillColor = GetRGBColor(paintActions[i].FillColor);

            layers[paintActions[i].Layer].pBitmapRenderTarget->BeginDraw();
            layers[paintActions[i].Layer].pBitmapRenderTarget->Clear(fillColor);
            layers[paintActions[i].Layer].pBitmapRenderTarget->EndDraw();
        }
    }

    for (size_t i = 0; i < Actions.size(); i++) {

        if (Actions[i].Tool == TEraser){
            layers[Actions[i].Layer].pBitmapRenderTarget->BeginDraw();

            layers[Actions[i].Layer].pBitmapRenderTarget->PushAxisAlignedClip(Actions[i].Position, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

            layers[Actions[i].Layer].pBitmapRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));

            layers[Actions[i].Layer].pBitmapRenderTarget->PopAxisAlignedClip();

            layers[Actions[i].Layer].pBitmapRenderTarget->EndDraw();
        }
        else if (Actions[i].Tool == TBrush) {
            ID2D1SolidColorBrush* brushColor = D2_CreateSolidBrush(Actions[i].Color);
            ID2D1SolidColorBrush* brushFillColor = D2_CreateSolidBrush(Actions[i].FillColor);

            ID2D1PathGeometry* pPathGeometry = nullptr;
            pD2DFactory->CreatePathGeometry(&pPathGeometry);

            ID2D1GeometrySink* pSink = nullptr;
            pPathGeometry->Open(&pSink);

            pSink->BeginFigure(
                D2D1::Point2F(Actions[i].FreeForm.vertices[0].x, Actions[i].FreeForm.vertices[0].y),
                D2D1_FIGURE_BEGIN_FILLED
            );

			for (size_t j = 1; j < Actions[i].FreeForm.vertices.size(); j++) {
                pSink->AddLine(D2D1::Point2F(Actions[i].FreeForm.vertices[j].x, Actions[i].FreeForm.vertices[j].y));
			}

            pSink->EndFigure(D2D1_FIGURE_END_OPEN);

            pSink->Close();
            pSink->Release();

            bool isSelfIntersecting = IsSelfIntersecting(Actions[i].FreeForm);
            bool isProximityClose = IsProximityClose(Actions[i].FreeForm, 15.0f);
            bool isCloseByWinding = IsClosedByWindingNumber(Actions[i].FreeForm);

            bool isClosed = isSelfIntersecting || isProximityClose || isCloseByWinding;

            layers[Actions[i].Layer].pBitmapRenderTarget->BeginDraw();
            layers[Actions[i].Layer].pBitmapRenderTarget->DrawGeometry(pPathGeometry, brushColor, Actions[i].BrushSize);
			
            if (isClosed && Actions[i].IsFilled) {
				layers[Actions[i].Layer].pBitmapRenderTarget->FillGeometry(pPathGeometry, brushFillColor);
			}

            layers[Actions[i].Layer].pBitmapRenderTarget->EndDraw();

            pPathGeometry->Release();
        }
        else if (Actions[i].Tool == TRectangle) {
            ID2D1SolidColorBrush* brushColor = D2_CreateSolidBrush(Actions[i].Color);

            layers[Actions[i].Layer].pBitmapRenderTarget->BeginDraw();

            layers[Actions[i].Layer].pBitmapRenderTarget->PushAxisAlignedClip(Actions[i].Position, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

            layers[Actions[i].Layer].pBitmapRenderTarget->FillRectangle(Actions[i].Position, brushColor);

            layers[Actions[i].Layer].pBitmapRenderTarget->PopAxisAlignedClip();

            layers[Actions[i].Layer].pBitmapRenderTarget->EndDraw();
        }
        else if (Actions[i].Tool == TEllipse) {
            ID2D1SolidColorBrush* brushColor = D2_CreateSolidBrush(Actions[i].Color);

            layers[Actions[i].Layer].pBitmapRenderTarget->BeginDraw();

            layers[Actions[i].Layer].pBitmapRenderTarget->FillEllipse(Actions[i].Ellipse, brushColor);

            layers[Actions[i].Layer].pBitmapRenderTarget->EndDraw();
        }
        else if (Actions[i].Tool == TLine) {
            ID2D1SolidColorBrush* brushColor = D2_CreateSolidBrush(Actions[i].Color);

            layers[Actions[i].Layer].pBitmapRenderTarget->BeginDraw();

            layers[Actions[i].Layer].pBitmapRenderTarget->DrawLine(Actions[i].startPoint, Actions[i].endPoint, brushColor, Actions[i].BrushSize, nullptr);

            layers[Actions[i].Layer].pBitmapRenderTarget->EndDraw();
        }
    }

    for (size_t i = 0; i < layersOrder.size(); ++i) {

        std::vector<LayerOrder> sortedLayers = layersOrder;

        std::sort(sortedLayers.begin(), sortedLayers.end(), [](const LayerOrder& a, const LayerOrder& b) {
            return a.layerOrder < b.layerOrder;
        });

        if (layers[sortedLayers[i].layerIndex].pBitmap) {
           pRenderTarget->DrawBitmap(layers[sortedLayers[i].layerIndex].pBitmap);
        }
    }

    prevLeft = -1;
    prevTop = -1;

    HRESULT hr = pRenderTarget->EndDraw();

    DrawLayerPreview(layerIndex);

    if (FAILED(hr)) {
        MessageBox(NULL, L"Failed to update layers", L"Error", MB_OK);
    }
}

extern "C" __declspec(dllexport) void RenderLayers() {
    if (!pRenderTarget) {
        return;
    }

    pRenderTarget->BeginDraw();
    pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

    for (size_t i = 0; i < layersOrder.size(); ++i) {
        std::vector<LayerOrder> sortedLayers = layersOrder;

        std::sort(sortedLayers.begin(), sortedLayers.end(), [](const LayerOrder& a, const LayerOrder& b) {
            return a.layerOrder < b.layerOrder;
            });

        if (layers[sortedLayers[i].layerIndex].pBitmap) {
            pRenderTarget->DrawBitmap(layers[sortedLayers[i].layerIndex].pBitmap);
        }
    }

    HRESULT hr = pRenderTarget->EndDraw();

    if (FAILED(hr)) {
        MessageBox(NULL, L"Failed to render layers", L"Error", MB_OK);
    }    
}

extern "C" __declspec(dllexport) void DrawLayerPreview(int currentLayer) {

    HBITMAP hBitmap = LayerButtons[layerIndex].hBitmap;

    RECT WindowRC;
    GetClientRect(mainHWND, &WindowRC);

    RECT rc;
    GetClientRect(LayerButtons[layerIndex].button, &rc);

    HDC hdc = GetDC(LayerButtons[layerIndex].button);
    HDC compatibleDC = CreateCompatibleDC(hdc);

    SelectObject(compatibleDC, hBitmap);
    
	if (!LayerButtons[layerIndex].isInitialPainted) {
		HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
		SelectObject(compatibleDC, whiteBrush);

		COLORREF initialClickedColor = GetPixel(compatibleDC, 0, 0);

		ExtFloodFill(compatibleDC, 0, 0, initialClickedColor, FLOODFILLSURFACE);

		DeleteObject(whiteBrush);

        LayerButtons[layerIndex].isInitialPainted = true;
	}

    for(int i = 0; i < Actions.size(); i++) {
        if (Actions[i].Layer == layerIndex) {
            if (Actions[i].Tool == TBrush) {
				for (int j = 0; j < Actions[i].FreeForm.vertices.size(); j++) {
                    HBRUSH hBrush = CreateSolidBrush(Actions[i].Color);
                    HPEN hPen = CreatePen(PS_SOLID, 1, Actions[i].Color);

                    SelectObject(compatibleDC, hBrush);
                    SelectObject(compatibleDC, hPen);

                    RECT pXY = scalePointsToButton(Actions[i].FreeForm.vertices[j].x, Actions[i].FreeForm.vertices[j].y, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top));

                    RECT rectPoint = { pXY.left - 1, pXY.top - 1, pXY.left + 1, pXY.top + 1 };

                    Ellipse(compatibleDC, rectPoint.left, rectPoint.top, rectPoint.right, rectPoint.bottom);

                    if (Actions[i].IsFilled) {

                        RECT XY = scalePointsToButton(mouseLastClickPosition.x, mouseLastClickPosition.y, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top));

                        COLORREF clickedColor = GetPixel(compatibleDC, XY.left, XY.top);

                        HBRUSH fillBrush = CreateSolidBrush(Actions[i].FillColor);
                        SelectObject(compatibleDC, fillBrush);

                        ExtFloodFill(compatibleDC, XY.left, XY.top, clickedColor, FLOODFILLSURFACE);

                        DeleteObject(fillBrush);
                    }

                    DeleteObject(hBrush);
                    DeleteObject(hPen);
				}
            }
            else if (Actions[i].Tool == TEraser) {
                HBRUSH hBrush = CreateSolidBrush(RGB(255,255,255));
                HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));

                SelectObject(compatibleDC, hBrush);
                SelectObject(compatibleDC, hPen);

                RECT pXY = scalePointsToButton(Actions[i].Position.left, Actions[i].Position.top, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top));
                RECT pWZ= scalePointsToButton(Actions[i].Position.right, Actions[i].Position.bottom, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top));

                RECT rectPoint = { pXY.left - 1, pXY.top - 1, pWZ.left + 1, pWZ.top + 1 };
                
                Ellipse(compatibleDC, rectPoint.left, rectPoint.top, rectPoint.right, rectPoint.bottom);

                DeleteObject(hBrush);
                DeleteObject(hPen);
            }
            else if (Actions[i].Tool == TRectangle) {
                HBRUSH hBrush = CreateSolidBrush(Actions[i].Color);
                HPEN hPen = CreatePen(PS_SOLID, 1, Actions[i].Color);

                SelectObject(compatibleDC, hBrush);
                SelectObject(compatibleDC, hPen);

                RECT pXY = scalePointsToButton(Actions[i].Position.left, Actions[i].Position.top, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top));
                RECT pWZ = scalePointsToButton(Actions[i].Position.right, Actions[i].Position.bottom, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top));

                RECT rectPoint = { pXY.left, pXY.top, pWZ.left, pWZ.top };
                
                Rectangle(compatibleDC, rectPoint.left, rectPoint.top, rectPoint.right, rectPoint.bottom);

                DeleteObject(hBrush);
                DeleteObject(hPen);
            }
            else if (Actions[i].Tool == TEllipse) {
                HBRUSH hBrush = CreateSolidBrush(Actions[i].Color);
                HPEN hPen = CreatePen(PS_SOLID, 1, Actions[i].Color);

                SelectObject(compatibleDC, hBrush);
                SelectObject(compatibleDC, hPen);

                RECT pXY = scalePointsToButton(Actions[i].Position.left, Actions[i].Position.top, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top));
                RECT pWZ = scalePointsToButton(Actions[i].Position.right, Actions[i].Position.bottom, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top));

                RECT rectPoint = { pXY.left - 1, pXY.top - 1, pWZ.left + 1, pWZ.top + 1 };

                Ellipse(compatibleDC, rectPoint.left, rectPoint.top, rectPoint.right, rectPoint.bottom);

                DeleteObject(hBrush);
                DeleteObject(hPen);
            }
            else if (Actions[i].Tool == TLine) {
                HPEN hPen = CreatePen(PS_SOLID, 1, Actions[i].Color);

                SelectObject(compatibleDC, hPen);

                RECT pXY = scalePointsToButton(Actions[i].startPoint.x, Actions[i].startPoint.y, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top));
                RECT pWZ = scalePointsToButton(Actions[i].endPoint.x, Actions[i].endPoint.y, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top));

                RECT rectPoint = { pXY.left - 1, pXY.top - 1, pWZ.left + 1, pWZ.top + 1 };

                MoveToEx(compatibleDC,  rectPoint.left, rectPoint.top, NULL);
                LineTo(compatibleDC, rectPoint.right, rectPoint.bottom);

                DeleteObject(hPen);
            }
            else if (Actions[i].Tool == TPaintBucket) {
                
                COLORREF clickedColor = GetPixel(compatibleDC,0, 0);

				HBRUSH fillBrush = CreateSolidBrush(Actions[i].FillColor);
				SelectObject(compatibleDC, fillBrush);

                RECT XY = scalePointsToButton(mouseLastClickPosition.x, mouseLastClickPosition.y, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top));
				ExtFloodFill(compatibleDC, XY.left, XY.top, clickedColor, FLOODFILLSURFACE);

				DeleteObject(fillBrush);
            }
        }
    }

    DeleteDC(compatibleDC);
    ReleaseDC(LayerButtons[layerIndex].button, hdc);

    RedrawWindow(layersHWND, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

/* TOOLS */

extern "C" __declspec(dllexport) void EraserTool(int left, int top, int brushSize) {
    
    if (prevLeft == -1 && prevTop == -1) {
        prevLeft = left;
        prevTop = top;
    }

    auto target = layers[layerIndex].pBitmapRenderTarget;

    target->BeginDraw();    

    float dx = static_cast<float>(left - prevLeft);
    float dy = static_cast<float>(top - prevTop);
    float distance = sqrt(dx * dx + dy * dy);

    float step = brushSize * 0.5f;

    if (distance > step) {
        float steps = distance / step;
        float deltaX = dx / steps;
        float deltaY = dy / steps;

        for (float i = 0; i < steps; i++) {
            float x = prevLeft + i * deltaX;
            float y = prevTop + i * deltaY;

            D2D1_RECT_F rect = D2D1::RectF(
                x - brushSize,
                y - brushSize,
                x + brushSize,
                y + brushSize
            );


            target->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

            target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

            target->PopAxisAlignedClip();
            
            if (x != prevLeft || y != prevTop) {
                if (x != -1 && y != -1) {
                    ACTION action;
                    action.Tool = 0;
                    action.Layer = layerIndex;
                    action.Position = rect;
                    action.BrushSize = brushSize;
                    action.IsFilled = false;
                    Actions.push_back(action);
                }
            }
        }
    }

    D2D1_RECT_F rect = D2D1::RectF(
        left - brushSize,
        top - brushSize,
        left + brushSize,
        top + brushSize
    );

    target->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    target->Clear(D2D1::ColorF(0, 0, 0, 0));

    target->PopAxisAlignedClip();

    if (left != prevLeft || top != prevTop) {
        if (left != -1 && top != -1) {
            ACTION action;
            action.Tool = 0;
            action.Layer = layerIndex;
            action.Position = rect;
            action.BrushSize = brushSize;
            Actions.push_back(action);
        }
    }

    prevLeft = left;
    prevTop = top;

    target->EndDraw();
}

extern "C" __declspec(dllexport) void BrushTool(int left, int top, COLORREF hexColor, int brushSize) {
    ID2D1SolidColorBrush* brush = D2_CreateSolidBrush(hexColor);
    currentBrushSize = brushSize;

    isDrawingBrush = true;

    if (prevLeft == -1 && prevTop == -1) {
        prevLeft = left;
        prevTop = top;
    }

    pRenderTarget->BeginDraw();

    layers[layerIndex].pBitmapRenderTarget->BeginDraw();
    
    float dx = static_cast<float>(left - prevLeft);
    float dy = static_cast<float>(top - prevTop);

    float distance = sqrt(dx * dx + dy * dy);

    float step = brushSize * 0.5f;

    if (distance > step) {
        float steps = distance / step;
        float deltaX = dx / steps;
        float deltaY = dy / steps;

        for (float i = 0; i < steps; i++) {
            float x = prevLeft + i * deltaX;
            float y = prevTop + i * deltaY;

            D2D1_RECT_F rect = D2D1::RectF(
                x - brushSize * 0.5f, // Left
                y - brushSize * 0.5f, // Top
                x + brushSize * 0.5f, // Right
                y + brushSize * 0.5f  // Bottom
            );

            layers[layerIndex].pBitmapRenderTarget->FillRectangle(rect, brush);

            if (x != prevLeft || y != prevTop) {
                if (x != -1 && y != -1) {
                    Vertices.push_back(VERTICE{ x, y });
                }
            }
        }
    }
    
    D2D1_RECT_F rect = D2D1::RectF(
        left - brushSize * 0.5f, // Left
        top - brushSize * 0.5f, // Top
        left + brushSize * 0.5f, // Right
        top + brushSize * 0.5f  // Bottom
    );

    layers[layerIndex].pBitmapRenderTarget->FillRectangle(rect, brush);

    if (left != prevLeft || top != prevTop) {
        if (left != -1 && top != -1) {
            Vertices.push_back(VERTICE{ static_cast<float>(left), static_cast<float>(top) });
        }
    }

    layers[layerIndex].pBitmapRenderTarget->EndDraw();

    pRenderTarget->EndDraw();

    prevLeft = left;
    prevTop = top;
}

extern "C" __declspec(dllexport) void RectangleTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    ID2D1SolidColorBrush* brush = D2_CreateSolidBrush(hexColor);

    if (!isDrawingRectange) {
        AddLayer();
    }

    isDrawingRectange = true;

    layers[LayersCount() - 1].pBitmapRenderTarget->BeginDraw();

    rectangle = D2D1::RectF(left, top, right, bottom);

    if (prevRectangle.left != 0 || prevRectangle.top != 0 || prevRectangle.right != 0 || prevRectangle.bottom != 0) {

        layers[LayersCount() - 1].pBitmapRenderTarget->PushAxisAlignedClip(prevRectangle, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

        layers[LayersCount() - 1].pBitmapRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));

        layers[LayersCount() - 1].pBitmapRenderTarget->PopAxisAlignedClip();
    }

    layers[LayersCount() - 1].pBitmapRenderTarget->PushAxisAlignedClip(rectangle, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    layers[LayersCount() - 1].pBitmapRenderTarget->FillRectangle(rectangle, brush);

    layers[LayersCount() - 1].pBitmapRenderTarget->PopAxisAlignedClip();

    prevRectangle = D2D1::RectF(left, top, right, bottom);

    layers[LayersCount() - 1].pBitmapRenderTarget->EndDraw();
}

extern "C" __declspec(dllexport) void EllipseTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    ID2D1SolidColorBrush* brush = D2_CreateSolidBrush(hexColor);
    
    if (!isDrawingEllipse) {
        AddLayer();
    }

    isDrawingEllipse = true;

    layers[LayersCount() - 1].pBitmapRenderTarget->BeginDraw();

    rectangle = D2D1::RectF(left, top, right, bottom);

    ellipse = D2D1::Ellipse(D2D1::Point2F(left, top), abs(right - left), abs(bottom - top));

    if (prevEllipse.point.x != 0 || prevEllipse.point.y != 0 || prevEllipse.radiusX != 0 || prevEllipse.radiusY != 0) {
        
        D2D1_RECT_F prevRect = D2D1::RectF(
            prevEllipse.point.x - prevEllipse.radiusX,
            prevEllipse.point.y - prevEllipse.radiusY,
            prevEllipse.point.x + prevEllipse.radiusX,
            prevEllipse.point.y + prevEllipse.radiusY
        );

        layers[LayersCount() - 1].pBitmapRenderTarget->PushAxisAlignedClip(prevRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

        layers[LayersCount() - 1].pBitmapRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));

        layers[LayersCount() - 1].pBitmapRenderTarget->PopAxisAlignedClip();
    }

    layers[LayersCount() - 1].pBitmapRenderTarget->FillEllipse(ellipse, brush);

    prevEllipse = ellipse;

    layers[LayersCount() - 1].pBitmapRenderTarget->EndDraw();
}

extern "C" __declspec(dllexport) void LineTool(int xInitial, int yInitial, int x, int y, unsigned int hexColor, int brushSize) {
    ID2D1SolidColorBrush* brush = D2_CreateSolidBrush(hexColor);
    currentBrushSize = brushSize;

    if (!isDrawingLine) {
        AddLayer();
    }

    isDrawingLine = true;
    
    layers[LayersCount() - 1].pBitmapRenderTarget->BeginDraw();

    if (startPoint.x != 0 || startPoint.y != 0 || endPoint.x != 0 || endPoint.y != 0) {
        D2D1_RECT_F lineBounds = D2D1::RectF(
            min(startPoint.x, endPoint.x) - brushSize,
            min(startPoint.y, endPoint.y) - brushSize,
            max(startPoint.x, endPoint.x) + brushSize,
            max(startPoint.y, endPoint.y) + brushSize
        );

        layers[LayersCount() - 1].pBitmapRenderTarget->PushAxisAlignedClip(lineBounds, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        layers[LayersCount() - 1].pBitmapRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
        layers[LayersCount() - 1].pBitmapRenderTarget->PopAxisAlignedClip();
    }

    startPoint = D2D1::Point2F(xInitial, yInitial);
    endPoint = D2D1::Point2F(x, y);

    layers[LayersCount() - 1].pBitmapRenderTarget->DrawLine(startPoint, endPoint, brush, brushSize, nullptr);

    layers[LayersCount() - 1].pBitmapRenderTarget->EndDraw();
}

extern "C" __declspec(dllexport) void PaintBucketTool(int xInitial, int yInitial, COLORREF hexFillColor, HWND hWnd) {

    bool isClickInsideClosed = false;

	mouseLastClickPosition = { xInitial, yInitial };

    for (size_t i = 0; i < Actions.size(); i++) {
        if (Actions[i].Tool == TBrush) {
            bool isSelfIntersecting = IsSelfIntersecting(Actions[i].FreeForm);
			bool isProximityClose = IsProximityClose(Actions[i].FreeForm, 15.0f);
			bool isCloseByWinding = IsClosedByWindingNumber(Actions[i].FreeForm);

            bool isClosed = isSelfIntersecting || isProximityClose || isCloseByWinding;

            if (isClosed) {
                if (IsPointInsidePolygon(Actions[i].FreeForm, VERTICE{ static_cast<float>(xInitial), static_cast<float>(yInitial) })) {
                    Actions[i].FillColor = hexFillColor;
					Actions[i].IsFilled = true;
                    isClickInsideClosed = true;
                }
            }
		}
        else if (Actions[i].Tool == TRectangle)
        {
            if(IsPointInsideRectF(Actions[i].Position, VERTICE{ static_cast<float>(xInitial), static_cast<float>(yInitial) })) {
                Actions[i].Color = hexFillColor;
                Actions[i].IsFilled = true;
                isClickInsideClosed = true;
            }
        }
        else if (Actions[i].Tool == TEllipse) {
            if (IsPointInsideEllipse(Actions[i].Ellipse, VERTICE{ static_cast<float>(xInitial), static_cast<float>(yInitial) })) {
                Actions[i].Color = hexFillColor;
                Actions[i].IsFilled = true;
                isClickInsideClosed = true;
            }
        }
    }

    if (!isClickInsideClosed) {
        ACTION action;
        action.Tool = TPaintBucket;
        action.Layer = layerIndex;
        action.FillColor = hexFillColor;
        Actions.push_back(action);
    }
}

/* CLEANUP */

extern "C" __declspec(dllexport) void Cleanup() {
    SafeRelease(&pBrush);
    SafeRelease(&pRenderTarget);
    SafeRelease(&pD2DFactory);
    SafeRelease(&transparentBrush);

    for (Layer& layer : layers) {
        if (layer.pBitmapRenderTarget) {
            layer.pBitmapRenderTarget->Release();
        }
        if (layer.pBitmap) {
            layer.pBitmap->Release();
        }
    }

    layers.clear();
}

template <class T> void SafeRelease(T** ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}