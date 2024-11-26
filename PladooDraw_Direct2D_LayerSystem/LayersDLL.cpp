// LayersDLL.cpp
#include "pch.h"
#include <windows.h>
#include <wincodec.h>
#include "LayersDLL.h"
#include <vector>
#include <sstream>
#include <d2d1_1.h>

struct Layer {
    ID2D1BitmapRenderTarget* pBitmapRenderTarget;
    ID2D1Bitmap* pBitmap;
};

IWICImagingFactory* pWICFactory = nullptr;
ID2D1Factory* pD2DFactory = nullptr;
ID2D1HwndRenderTarget* pRenderTarget = nullptr;
ID2D1SolidColorBrush* pBrush = nullptr;
ID2D1SolidColorBrush* transparentBrush = nullptr;

D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(0, 0), 0, 0);
D2D1_RECT_F rectangle = D2D1::RectF(0, 0, 0, 0);
D2D1_RECT_F prevRectangle = D2D1::RectF(0, 0, 0, 0);
D2D1_ELLIPSE prevEllipse = D2D1::Ellipse(D2D1::Point2F(0, 0), 0, 0);

D2D1_POINT_2F startPoint = { 0, 0 };
D2D1_POINT_2F endPoint = { 0, 0 };

static int currentBrushSize = 0;

static int prevLeft = -1;
static int prevTop = -1;

static bool isDrawingRectange = false;
static bool isDrawingEllipse = false;
static bool isDrawingLine = false;

static unsigned int currentColor = 0;

std::vector<Layer> layers;

int layerIndex = 0;
unsigned int lastHexColor = UINT_MAX;

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

ID2D1SolidColorBrush* D2_CreateSolidBrush(unsigned int hexColor) {
    currentColor = hexColor;

    if (lastHexColor == UINT_MAX || hexColor != lastHexColor) {
        float r = (hexColor & 0xFF) / 255.0f;         
        float g = ((hexColor >> 8) & 0xFF) / 255.0f;  
        float b = ((hexColor >> 16) & 0xFF) / 255.0f; 
        float a = 1.0f;                               

        D2D1_COLOR_F color = {r, g, b, a};  

        pRenderTarget->CreateSolidColorBrush(color, &pBrush);
    }

    lastHexColor = hexColor;

    return pBrush;
}

extern "C" __declspec(dllexport) HRESULT Initialize(HWND hWnd) {
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
    
    return S_OK;
}

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

    return S_OK;
}

extern "C" __declspec(dllexport) void handleMouseUp() {

    if (isDrawingRectange || isDrawingEllipse || isDrawingLine) {
        ID2D1SolidColorBrush* brush = D2_CreateSolidBrush(currentColor);

        layers[layerIndex].pBitmapRenderTarget->BeginDraw();              

        if (isDrawingRectange) {
            layers[layerIndex].pBitmapRenderTarget->PushAxisAlignedClip(rectangle, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            layers[layerIndex].pBitmapRenderTarget->FillRectangle(rectangle, brush);
            layers[layerIndex].pBitmapRenderTarget->PopAxisAlignedClip();
            rectangle = D2D1::RectF(0, 0, 0, 0);
            isDrawingRectange = false;
        }
        
        if (isDrawingEllipse) {
            layers[layerIndex].pBitmapRenderTarget->FillEllipse(ellipse, brush);
            ellipse = D2D1::Ellipse(D2D1::Point2F(0, 0), 0, 0);
            isDrawingEllipse = false;
        }

        if (isDrawingLine) {
            layers[layerIndex].pBitmapRenderTarget->DrawLine(startPoint, endPoint, brush, currentBrushSize, nullptr);
            isDrawingLine = false;
        }
               
        layers[layerIndex].pBitmapRenderTarget->EndDraw();

        layers.pop_back();
    }

    startPoint = D2D1::Point2F(0, 0);
    endPoint = D2D1::Point2F(0, 0);
 
    prevRectangle = D2D1::RectF(0, 0, 0, 0);
    prevEllipse = D2D1::Ellipse(D2D1::Point2F(0, 0), 0, 0);
    prevLeft = -1;
    prevTop = -1;
}

extern "C" __declspec(dllexport) void SetLayer(int index) {
    layerIndex = index;
    return;
}

extern "C" __declspec(dllexport) int LayersCount() {
    return layers.size();
}

extern "C" __declspec(dllexport) void RenderLayers() {
    if (!pRenderTarget) {
        return;
    }

    pRenderTarget->BeginDraw();
    pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

    for (size_t i = 0; i < layers.size(); ++i) {
        if (layers[i].pBitmap) {
            pRenderTarget->DrawBitmap(layers[i].pBitmap);
        }
    }

    HRESULT hr = pRenderTarget->EndDraw();
    if (FAILED(hr)) {
        MessageBox(NULL, L"Failed to render layers", L"Error", MB_OK);
    }
}

extern "C" __declspec(dllexport) void BrushTool(int left, int top, unsigned int hexColor, int brushSize) {
    ID2D1SolidColorBrush* brush = D2_CreateSolidBrush(hexColor);

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

            D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(x, y), brushSize, brushSize);
            layers[layerIndex].pBitmapRenderTarget->FillEllipse(ellipse, brush);
        }
    }

    D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(left, top), brushSize, brushSize);
    layers[layerIndex].pBitmapRenderTarget->FillEllipse(ellipse, brush);

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

extern "C" __declspec(dllexport) void PaintBucketTool(int xInitial, int yInitial, unsigned int hexFillColor) {
    return;
}

extern "C" __declspec(dllexport) void EraserTool(int left, int top, int brushSize) {
    auto target = layers[layerIndex].pBitmapRenderTarget;

    target->BeginDraw();

    D2D1_RECT_F rect = D2D1::RectF(
        left - brushSize,
        top - brushSize,
        left + brushSize,
        top + brushSize
    );

    target->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    target->Clear(D2D1::ColorF(0, 0, 0, 0));

    target->PopAxisAlignedClip();

    target->EndDraw();
}

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