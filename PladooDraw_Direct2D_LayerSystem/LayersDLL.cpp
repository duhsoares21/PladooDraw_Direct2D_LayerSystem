// LayersDLL.cpp
#include "pch.h"
#include <windows.h>
#include <wincodec.h>
#include "LayersDLL.h"
#include <vector>
#include <sstream>
#include <d2d1_1.h>
#include <cstdio>
#include <wrl/client.h>
#include <queue>
#include <fstream>
#include <string>
#include <codecvt>
#include <algorithm>

using namespace Microsoft::WRL;

struct ACTION {
    int Tool;
    int Layer;
    D2D1_RECT_F Position;
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
    HDC hdc;
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
static bool isDrawingLine = false;
static bool isFloodFill = false;

std::vector<LayerOrder> layersOrder;
std::vector<Layer> layers;
std::vector<ACTION> Actions;
std::vector<ACTION> RedoActions;

std::vector<LayerButton> LayerButtons;

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
       
    AddLayer(false);

    RenderLayers();
    
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

extern "C" __declspec(dllexport) void AddLayerButton(HWND layerButton) {

    RECT rc;
    GetClientRect(layerButton, &rc);

    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HDC hdc = GetDC(layerButton);
    HDC hMemoryDC = CreateCompatibleDC(hdc);
        
    HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));

    HBITMAP hBitmap = CreateCompatibleBitmap(hdc, size.width, size.height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

    SelectObject(hMemoryDC, hBrush);

    FillRect(hMemoryDC, &rc, hBrush);

    SendMessage(layerButton, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap);
    
    LayerButtons.push_back({ layerButton , hdc });

    DeleteObject(hBrush);
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteDC(hMemoryDC);
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

extern "C" __declspec(dllexport) HRESULT AddLayer(bool tempLayer = false) {
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

    if (!tempLayer) {
        LayerOrder layerOrder = { layers.size() - 1, layers.size() - 1 };
        layersOrder.push_back(layerOrder);
    }

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
            
            layers[Actions[i].Layer].pBitmapRenderTarget->BeginDraw();

            D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(Actions[i].Position.left, Actions[i].Position.top), Actions[i].BrushSize, Actions[i].BrushSize);
            layers[Actions[i].Layer].pBitmapRenderTarget->FillEllipse(ellipse, brushColor);

            layers[Actions[i].Layer].pBitmapRenderTarget->EndDraw();
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
        else if (Actions[i].Tool == TPaintBucket) {
            //PaintBucketTool(Actions[i].Position.left, Actions[i].Position.top, RGB(0,255,0), mainHWND, false);
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

    UpdateLayerPreview();

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

    UpdateLayerPreview();

    if (FAILED(hr)) {
        MessageBox(NULL, L"Failed to render layers", L"Error", MB_OK);
    }    
}

extern "C" __declspec(dllexport) void LayerPreview(HWND hButton, int buttonIndex) {
    if (!hButton || buttonIndex < 0 || buttonIndex >= layers.size()) {
        return;
    }

    RECT rc;
    GetClientRect(hButton, &rc);

    D2D1_RECT_F rect = D2D1::RectF(
        (FLOAT)rc.left,
        (FLOAT)rc.top,
        (FLOAT)rc.right,
        (FLOAT)rc.bottom
    );
}

extern "C" __declspec(dllexport) void UpdateLayerPreview() {
    for (int i = 0; i < LayerButtons.size(); i++) {
        LayerPreview(LayerButtons[i].button, layersOrder[i].layerIndex);
    }
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

    RECT rc;
    GetClientRect(LayerButtons[layerIndex].button, &rc);

    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HDC hdc = LayerButtons[layerIndex].hdc;
    HDC hMemoryDC = CreateCompatibleDC(hdc);

    HBRUSH hBrush = CreateSolidBrush(hexColor);

    HBITMAP hBitmap = CreateCompatibleBitmap(hdc, size.width, size.height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

    SelectObject(hMemoryDC, hBrush);

    if (prevLeft == -1 && prevTop == -1) {
        prevLeft = left;
        prevTop = top;
    }

    pRenderTarget->BeginDraw();

    layers[layerIndex].pBitmapRenderTarget->BeginDraw();
    
    std::string positions = "Left: " + std::to_string(left)+"\n" + "PrevLeft: " + std::to_string(prevLeft) + "\n" + "Top: " + std::to_string(top) + "\n" + "PrevTop: " + std::to_string(prevTop) + "\n";
    CreateLogData("Position.txt", positions);

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

            D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(x, y), brushSize, brushSize);

            layers[layerIndex].pBitmapRenderTarget->FillEllipse(ellipse, brush);
            Ellipse(hMemoryDC, x - brushSize, y - brushSize, x + brushSize, y + brushSize);

            if (x != prevLeft || y != prevTop) {
                if (x != -1 && y != -1) {
                    D2D1_RECT_F position = { x, y, 0, 0 };
                    ACTION action;
                    action.Tool = 1;
                    action.Layer = layerIndex;
                    action.Position = position;
                    action.Color = hexColor;
                    action.BrushSize = brushSize;
                    Actions.push_back(action);
                }
            }
        }
    }
    
    D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(left, top), brushSize, brushSize);

    layers[layerIndex].pBitmapRenderTarget->FillEllipse(ellipse, brush);
    Ellipse(hMemoryDC, left - brushSize, top - brushSize, left + brushSize, top + brushSize);

    if (left != prevLeft || top != prevTop) {
        if (left != -1 && top != -1) {
            D2D1_RECT_F position = { left, top, 0, 0 };
            ACTION action;
            action.Tool = 1;
            action.Layer = layerIndex;
            action.Position = position;
            action.Color = hexColor;
            action.BrushSize = brushSize;
            Actions.push_back(action);
        }
    }

    layers[layerIndex].pBitmapRenderTarget->EndDraw();

    pRenderTarget->EndDraw();

    BitBlt(hdc, 0, 0, size.width, size.height, hMemoryDC, 0, 0, SRCCOPY);
    SendMessage(LayerButtons[layerIndex].button, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap);

    DeleteObject(hBrush);
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(LayerButtons[layerIndex].button, hdc);

    prevLeft = left;
    prevTop = top;
}

extern "C" __declspec(dllexport) void RectangleTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    ID2D1SolidColorBrush* brush = D2_CreateSolidBrush(hexColor);

    if (!isDrawingRectange) {
        AddLayer(false);
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
        AddLayer(false);
    }

    isDrawingEllipse = true;

    layers[LayersCount() - 1].pBitmapRenderTarget->BeginDraw();

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
        AddLayer(false);
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

extern "C" __declspec(dllexport) void PaintBucketTool(int xInitial, int yInitial, COLORREF hexFillColor, HWND hWnd, bool storeData = true) {
    
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