﻿// LayersDLL.cpp
#include "pch.h"
#include "LayersDLL.h"

#include <algorithm>
#include <chrono>
#include <CL/cl.h>
#include <cmath>
#include <codecvt>
#include <corecrt_math_defines.h>
#include <cstdio>
#include <d2d1_1.h>
#include <dxgi1_2.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <numbers>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <wincodec.h>
#include <windows.h>
#include <wrl/client.h>

#include <d3dcompiler.h> // Para D3DCompileFromFile

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")

using namespace Microsoft::WRL;
using namespace std;

const int DX[] = { -1, 1, 0, 0 };
const int DY[] = { 0, 0, -1, 1 };

std::unordered_map<std::pair<int, int>, COLORREF, PairHash> bitmapData;

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

static int currentBrushSize = 0;

static int prevLeft = -1;
static int prevTop = -1;

static int zoomFactorW = 1;
static int zoomFactorH = 1;

static bool isDrawingRectangle = false;
static bool isDrawingEllipse = false;
static bool isDrawingBrush = false;
static bool isDrawingLine = false;
static bool isPaintBucket = false;

std::vector<LayerOrder> layersOrder;
std::vector<Layer> layers;
std::vector<ACTION> Actions;
std::vector<ACTION> RedoActions;
std::vector<VERTICE> Vertices;
std::vector<std::pair<int, int>> pixelsToFill;

int width, height;

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
            Cleanup();
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }

    return TRUE;
}

extern "C" __declspec(dllexport) HRESULT Initialize(HWND hWnd, int pZoomFactorW, int pZoomFactorH) {
    std::cout << "INITIALIZE" << std::endl;

    SetProcessDPIAware();

    zoomFactorW = pZoomFactorW;
    zoomFactorH = pZoomFactorH;

    mainHWND = hWnd;

    D2D1_FACTORY_OPTIONS options = { D2D1_DEBUG_LEVEL_INFORMATION }; // Habilitar depuração
    HRESULT factoryResult = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_MULTI_THREADED,
        __uuidof(ID2D1Factory1),
        &options,
        reinterpret_cast<void**>(pD2DFactory.GetAddressOf())
    );

    if (FAILED(factoryResult)) {
        MessageBox(hWnd, L"Erro ao criar Factory", L"Erro", MB_OK);

        return factoryResult;
    }

    InitializeDocument(hWnd);
        
    return S_OK;
}

HRESULT InitializeDocument(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);

    width = rc.right - rc.left;
    height = rc.bottom - rc.top;

    D2D1_SIZE_U size = D2D1::SizeU(width, height);
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(hWnd, size);

    // Lista com tentativas: hardware depois software
    D2D1_RENDER_TARGET_PROPERTIES attempts[] = {
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_HARDWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            0, 0,
            D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE
        ),
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            0, 0,
            D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE
        )
    };

    HRESULT renderResult = E_FAIL;
    for (const auto& props : attempts) {
        renderResult = pD2DFactory->CreateHwndRenderTarget(props, hwndProps, &pRenderTarget);
        if (SUCCEEDED(renderResult)) {
            break;
        }
    }

    if (FAILED(renderResult)) {
        wchar_t errorMessage[512];
        FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            renderResult,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            errorMessage,
            (sizeof(errorMessage) / sizeof(wchar_t)),
            nullptr
        );

        wchar_t finalMessage[1024];
        swprintf_s(finalMessage, L"Erro ao criar hWndRenderTarget\nCódigo: 0x%08X\nMensagem: %s", renderResult, errorMessage);
        MessageBox(hWnd, finalMessage, L"Erro", MB_OK | MB_ICONERROR);
        return renderResult;
    }

    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Scale(zoomFactorW, zoomFactorH));
    pRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT InitializeLayerRenderPreview() {
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

void SaveBinaryProject(const std::wstring& filename) {
    std::cout << "SAVE BINARY CALLED" << std::endl;
    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open()) {
        CreateLogData("error.log", "Failed to open file for writing: " + std::string(filename.begin(), filename.end()));
        return;
    }

    std::cout << "S-A" << std::endl;

    int magic = 0x30444450; // 'PDD0'
    int version = 1;
    out.write((char*)&magic, sizeof(magic));
    out.write((char*)&version, sizeof(version));
    out.write((char*)&width, sizeof(width));
    out.write((char*)&height, sizeof(height));
    if (!out.good()) {
        CreateLogData("error.log", "Error writing header data");
        out.close();
        return;
    }

    std::cout << "S-B" << std::endl;

    // Save layersOrder
    int layerOrderCount = static_cast<int>(layersOrder.size());
    out.write((char*)&layerOrderCount, sizeof(layerOrderCount));
    for (const auto& lo : layersOrder) {
        out.write((char*)&lo.layerOrder, sizeof(lo.layerOrder));
        out.write((char*)&lo.layerIndex, sizeof(lo.layerIndex));
        int colorCount = static_cast<int>(lo.indexedColors.size());
        out.write((char*)&colorCount, sizeof(colorCount));
        out.write((char*)lo.indexedColors.data(), colorCount * sizeof(D2D1_COLOR_F));
        if (!out.good()) {
            CreateLogData("error.log", "Error writing layerOrder data");
            out.close();
            return;
        }
    }

    std::cout << "S-C" << std::endl;

    // Save actions
    int actionCount = static_cast<int>(Actions.size());
    out.write((char*)&actionCount, sizeof(actionCount));
    for (const auto& a : Actions) {
        std::cout << "S-CB" << std::endl;
        std::cout << a.Color << std::endl;
        std::cout << "S-CF" << std::endl;
        std::cout << a.FillColor << std::endl;
        out.write((char*)&a.Tool, sizeof(a.Tool));
        out.write((char*)&a.Layer, sizeof(a.Layer));
        out.write((char*)&a.Position, sizeof(a.Position));
        out.write((char*)&a.Ellipse, sizeof(a.Ellipse));
        out.write((char*)&a.FillColor, sizeof(a.FillColor));
        out.write((char*)&a.Color, sizeof(a.Color));
        out.write((char*)&a.BrushSize, sizeof(a.BrushSize));
        out.write((char*)&a.IsFilled, sizeof(a.IsFilled));
        out.write((char*)&a.mouseX, sizeof(a.mouseX));
        out.write((char*)&a.mouseY, sizeof(a.mouseY));

        int vertexCount = static_cast<int>(a.FreeForm.vertices.size());
        out.write((char*)&vertexCount, sizeof(vertexCount));
        if (vertexCount > 0) {
            out.write((char*)a.FreeForm.vertices.data(), vertexCount * sizeof(VERTICE));
        }

        int pixelCount = static_cast<int>(a.pixelsToFill.size());
        out.write((char*)&pixelCount, sizeof(pixelCount));
        if (pixelCount > 0) {
            out.write((char*)a.pixelsToFill.data(), pixelCount * sizeof(POINT));
        }

        if (!out.good()) {
            CreateLogData("error.log", "Error writing action data");
            out.close();
            return;
        }
    }

    std::cout << "S-D" << std::endl;

    out.close();

    std::cout << "S-E" << std::endl;
}

void LoadBinaryProject(const std::wstring& filename, HWND hWndLayer, HINSTANCE hLayerInstance, int btnHeight, HWND* hLayerButtons, int& layerID, const wchar_t* szButtonClass, const wchar_t* msgText) {
    std::cout << "LOAD BINARY CALLED" << std::endl;

    std::ifstream in(filename, std::ios::binary);

    if (!in.is_open()) {
        CreateLogData("error.log", "Failed to open file for reading: " + std::string(filename.begin(), filename.end()));
        return;
    }

    Cleanup();
    layerID = 0; // Reset layerID to start from 0, as in assembly
    LayerButtons.clear(); // Ensure LayerButtons is empty

    int magic = 0, version = 0;
    in.read((char*)&magic, sizeof(magic));
    in.read((char*)&version, sizeof(version));
    if (magic != 0x30444450 || version != 1) {
        CreateLogData("error.log", "Invalid file format or version");
        in.close();
        return;
    }

    in.read((char*)&width, sizeof(width));
    in.read((char*)&height, sizeof(height));
    if (!in.good() || width <= 0 || height <= 0 || width > 10000 || height > 10000) {
        CreateLogData("error.log", "Invalid dimensions or read error");
        in.close();
        return;
    }

    // Load layersOrder
    int layerOrderCount = 0;
    in.read((char*)&layerOrderCount, sizeof(layerOrderCount));
    if (!in.good() || layerOrderCount < 0 || layerOrderCount > 10000) {
        CreateLogData("error.log", "Invalid layerOrder count: " + std::to_string(layerOrderCount));
        in.close();
        return;
    }

    layersOrder.clear();
    for (int i = 0; i < layerOrderCount; ++i) {
        LayerOrder lo = {};
        int colorCount = 0;
        in.read((char*)&lo.layerOrder, sizeof(lo.layerOrder));
        in.read((char*)&lo.layerIndex, sizeof(lo.layerIndex));
        in.read((char*)&colorCount, sizeof(colorCount));
        if (!in.good() || colorCount < 0 || colorCount > width * height) {
            CreateLogData("error.log", "Invalid color count for layerOrder " + std::to_string(i));
            in.close();
            return;
        }
        lo.indexedColors.resize(colorCount);
        in.read((char*)lo.indexedColors.data(), colorCount * sizeof(D2D1_COLOR_F));
        if (!in.good()) {
            CreateLogData("error.log", "Error reading indexedColors for layerOrder " + std::to_string(i));
            in.close();
            return;
        }
        layersOrder.push_back(lo);
    }

    // Load actions
    int actionCount = 0;
    in.read((char*)&actionCount, sizeof(actionCount));
    if (!in.good() || actionCount < 0 || actionCount > 1000000) {
        CreateLogData("error.log", "Invalid action count: " + std::to_string(actionCount));
        in.close();
        return;
    }

    Actions.clear();
    RedoActions.clear();
    int maxLayer = -1;
    for (int i = 0; i < actionCount; ++i) {
        ACTION a = {};
        int vertexCount = 0, pixelCount = 0;

        in.read((char*)&a.Tool, sizeof(a.Tool));
        in.read((char*)&a.Layer, sizeof(a.Layer));
        if (!in.good() || a.Layer < 0 || a.Layer > 10000) {
            CreateLogData("error.log", "Invalid layer index for action " + std::to_string(i) + ": " + std::to_string(a.Layer));
            in.close();
            Actions.clear();
            return;
        }
        in.read((char*)&a.Position, sizeof(a.Position));
        in.read((char*)&a.Ellipse, sizeof(a.Ellipse));
        in.read((char*)&a.FillColor, sizeof(a.FillColor));
        in.read((char*)&a.Color, sizeof(a.Color));
        in.read((char*)&a.BrushSize, sizeof(a.BrushSize));
        in.read((char*)&a.IsFilled, sizeof(a.IsFilled));
        in.read((char*)&a.mouseX, sizeof(a.mouseX));
        in.read((char*)&a.mouseY, sizeof(a.mouseY));

        in.read((char*)&vertexCount, sizeof(vertexCount));
        if (!in.good() || vertexCount < 0 || vertexCount > 1000000) {
            CreateLogData("error.log", "Invalid vertex count for action " + std::to_string(i));
            in.close();
            Actions.clear();
            return;
        }
        a.FreeForm.vertices.resize(vertexCount);
        if (vertexCount > 0) {
            in.read((char*)a.FreeForm.vertices.data(), vertexCount * sizeof(VERTICE));
            if (!in.good()) {
                CreateLogData("error.log", "Error reading vertices for action " + std::to_string(i));
                in.close();
                Actions.clear();
                return;
            }
        }

        in.read((char*)&pixelCount, sizeof(pixelCount));
        if (!in.good() || pixelCount < 0 || pixelCount > width * height) {
            CreateLogData("error.log", "Invalid pixel count for action " + std::to_string(i));
            in.close();
            Actions.clear();
            return;
        }
        a.pixelsToFill.resize(pixelCount);
        if (pixelCount > 0) {
            in.read((char*)a.pixelsToFill.data(), pixelCount * sizeof(POINT));
            if (!in.good()) {
                CreateLogData("error.log", "Error reading pixels for action " + std::to_string(i));
                in.close();
                Actions.clear();
                return;
            }
        }

        Actions.push_back(a);
        maxLayer = (std::max)(maxLayer, a.Layer);
    }

    in.close();

    // Initialize rendering pipeline after layers and buttons are set up
    HRESULT hr = Initialize(mainHWND, zoomFactorW, zoomFactorH);
    if (FAILED(hr)) {
        std::cout << "I-ERR" << std::endl;
        CreateLogData("error.log", "Failed to initialize after layer setup: HRESULT " + std::to_string(hr));
        layers.clear();
        Actions.clear();
        LayerButtons.clear();
        return;
    }

    // Create layers and buttons
    layers.clear();
    layerIndex = 0;
    for (int i = 0; i <= maxLayer; ++i) {
        HRESULT hr = AddLayer();
        if (FAILED(hr)) {
            std::cout << "FAIL ADD LAYER" << std::endl;
            CreateLogData("error.log", "Failed to add layer " + std::to_string(i) + ": HRESULT " + std::to_string(hr));
            layers.clear();
            Actions.clear();
            LayerButtons.clear();
            return;
        }

        // Create button for the layer (mimicking assembly WM_COMMAND wParam == 1001)
        int yPos = btnHeight * layerID;
        HWND button = CreateWindowEx(
            0,                            // dwExStyle
            szButtonClass,               // lpClassName
            msgText,                     // lpWindowName
            WS_CHILD | WS_VISIBLE | BS_BITMAP, // dwStyle
            0,                           // x
            yPos,                        // y
            120,                         // nWidth
            btnHeight,                   // nHeight
            hWndLayer,                   // hWndParent
            (HMENU)(intptr_t)layerID,    // hMenu (control ID)
            hLayerInstance,              // hInstance
            NULL                         // lpParam
        );

        if (button == NULL) {
            std::cout << "FAIL BTN LAYER" << std::endl;
            DWORD dwError = GetLastError();
            wchar_t buffer[256];
            FormatMessageW(
                FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                dwError,
                0,
                buffer,
                sizeof(buffer) / sizeof(wchar_t),
                NULL
            );

            std::wcerr << L"CreateWindowEx failed. Error: " << dwError << L" - " << buffer << std::endl;
            CreateLogData("error.log", "Failed to create button for layer " + std::to_string(layerID) + ": " + std::to_string(dwError));

            layers.clear();
            Actions.clear();
            LayerButtons.clear();
            return;
        }

        // Store button HWND in hLayerButtons
        hLayerButtons[layerID] = button;

        // Add button to LayerButtons
        AddLayerButton(button);

        if (i < maxLayer) {
            layerID++;
        }
    }

   InitializeLayerRenderPreview();
   UpdateLayers();
}

extern "C" __declspec(dllexport) void __stdcall SaveProjectDll(const char* pathAnsi) {
    int wlen = MultiByteToWideChar(CP_ACP, 0, pathAnsi, -1, nullptr, 0);
    std::wstring wpath(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, pathAnsi, -1, &wpath[0], wlen);

    SaveBinaryProject(wpath);
}

extern "C" __declspec(dllexport) void __stdcall LoadProjectDll(const char* pathAnsi, HWND hWndLayer, HINSTANCE hLayerInstance, int btnHeight, HWND* hLayerButtons, int* layerID, const wchar_t* szButtonClass, const wchar_t* msgText) {
    std::cout << "LOAD" << std::endl;
    int wlen = MultiByteToWideChar(CP_ACP, 0, pathAnsi, -1, nullptr, 0);
    std::wstring wpath(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, pathAnsi, -1, &wpath[0], wlen);

    std::cout << "LOAD BINARY" << std::endl;

    LoadBinaryProject(wpath, hWndLayer, hLayerInstance, btnHeight, hLayerButtons, *layerID, L"Button", msgText);

    std::cout << "LOAD BINARY START" << std::endl;
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

    x = static_cast<int>(x * scale * zoomFactorW) + offsetX;
    y = static_cast<int>(y * scale * zoomFactorH) + offsetY;

    RECT rect = { x, y, 0, 0 };
    return rect;
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

COLORREF GetColorAtPixel(int mouseX, int mouseY) {
    HDC hdc = GetDC(mainHWND);

    return GetPixel(hdc, mouseX, mouseY);
}   

COLORREF GetPixelColor(int x, int y, COLORREF defaultColor = RGB(255, 255, 255)) {
    auto it = bitmapData.find(std::make_pair(x, y));
    if (it != bitmapData.end()) {
        return it->second; // Return the color if found
    }
    return defaultColor; // Return default color if not found
}

int findLayerIndex(const std::vector<LayerOrder>& layersOrder, int searchLayerIndex) {
    auto it = std::find_if(layersOrder.begin(), layersOrder.end(),
        [searchLayerIndex](const LayerOrder& layer) {
            return layer.layerIndex == searchLayerIndex;
        });

    // If found, return the index
    if (it != layersOrder.end()) {
        return std::distance(layersOrder.begin(), it);
    }

    // Return -1 if not found
    return -1;
}

bool isSameColor(const D2D1_COLOR_F& a, const D2D1_COLOR_F& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

// Function to find indices of a specific color in indexedColors
std::vector<size_t> findColorIndices(const std::vector<D2D1_COLOR_F>& indexedColors, const D2D1_COLOR_F& searchColor) {
    std::vector<size_t> indices;

    for (size_t i = 0; i < indexedColors.size(); ++i) {
        if (isSameColor(indexedColors[i], searchColor)) {
            indices.push_back(i); // Store the index of matching color
        }
    }

    return indices;
}

void FillRectangleColors(const D2D1_RECT_F& rect, std::vector<D2D1_COLOR_F>& indexedColors, D2D1_COLOR_F color)
{
    // Convert rect coordinates to integer indices
    int left = static_cast<int>(rect.left);
    int top = static_cast<int>(rect.top);
    int right = static_cast<int>(rect.right);
    int bottom = static_cast<int>(rect.bottom);

    // Ensure boundaries don't exceed document size
    left = (std::max)(0, left);
    top = (std::max)(0, top);
    right = (std::min)(width, right);
    bottom = (std::min)(height, bottom);

    // Fill the rectangle area in indexedColors
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            int index = y * width + x; // Convert (x, y) to linear index
            indexedColors[index] = color; // Directly replace color at index
        }
    }
}

/* ACTIONS */

extern "C" __declspec(dllexport) void handleMouseUp() {

    if (isDrawingRectangle || isDrawingEllipse || isDrawingLine) {
        
        ComPtr<ID2D1SolidColorBrush> brush;
        pRenderTarget->CreateSolidColorBrush(GetRGBColor(currentColor), &brush);

        layers[layerIndex].pBitmapRenderTarget->BeginDraw();              

        if (isDrawingRectangle) {
            layers[layerIndex].pBitmapRenderTarget->PushAxisAlignedClip(rectangle, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            layers[layerIndex].pBitmapRenderTarget->FillRectangle(rectangle, brush.Get());
            layers[layerIndex].pBitmapRenderTarget->PopAxisAlignedClip();

            //OutputDebugStringA("CC: " + currentColor);
            //OutputDebugStringA("HC: " + hexColor);

            ACTION action;
            action.Tool = TRectangle;
            action.Layer = layerIndex;
            action.Position = rectangle;
            action.Color = currentColor;
            Actions.push_back(action);

            //FillRectangleColors(rectangle, layersOrder[findLayerIndex(layersOrder, layerIndex)].indexedColors, GetRGBColor(currentColor));

            rectangle = D2D1::RectF(0, 0, 0, 0);
            isDrawingRectangle = false;
        }
        
        if (isDrawingEllipse) {
            layers[layerIndex].pBitmapRenderTarget->FillEllipse(ellipse, brush.Get());

            ACTION action;
            action.Tool = TEllipse;
            action.Layer = layerIndex;
            action.Position = rectangle;
            action.Ellipse = ellipse;
            action.Color = currentColor;
            Actions.push_back(action);

            ellipse = D2D1::Ellipse(D2D1::Point2F(0, 0), 0, 0);
            isDrawingEllipse = false;
        }

        if (isDrawingLine) {
            layers[layerIndex].pBitmapRenderTarget->DrawLine(startPoint, endPoint, brush.Get(), currentBrushSize, nullptr);

            ACTION action;
            action.Tool = TLine;
            action.Layer = layerIndex;
            action.Line = { startPoint, endPoint };
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
        std::cout << "CC" << std::endl;
        std::cout << currentColor << std::endl;

        ACTION action;
        action.Tool = TBrush;
        action.Layer = layerIndex;
        action.Color = currentColor;
        action.BrushSize = currentBrushSize;
		action.FillColor = RGB(255, 255, 255);
		action.FreeForm.vertices = Vertices;
        action.IsFilled = false;
        Actions.push_back(action);

        /*int layerOrderIndex = findLayerIndex(layersOrder, layerIndex);
        D2D1_COLOR_F paintColor = GetRGBColor(currentColor);

        for (int i = 0; i < Vertices.size(); i++) {
            //std::cout << i << std::endl;
            layersOrder[layerOrderIndex].indexedColors[Vertices[i].y * width + Vertices[i].x] = paintColor;
        }*/

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

    DrawLayerPreview(layerIndex);
}

extern "C" __declspec(dllexport) void Undo() {
    if (Actions.size() > 0) {
        ACTION lastAction = Actions.back();
        RedoActions.push_back(lastAction);
        Actions.pop_back();
 
        DrawLayerPreview(layerIndex);
    }
}

/*extern "C" __declspec(dllexport) void Redo() {
    if (RedoActions.size() > 0) {
        Actions.push_back(RedoActions[RedoActions.size() - 1]);
        RedoActions.pop_back();
    }
}*/

extern "C" __declspec(dllexport) void Redo() {
    if (RedoActions.size() > 0) {
        ACTION action = RedoActions.back();
        Actions.push_back(action);
        RedoActions.pop_back();

        RenderLayers();
        DrawLayerPreview(layerIndex);
    }
}

/* LAYERS */

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

extern "C" __declspec(dllexport) HRESULT AddLayer() {
    ComPtr<ID2D1BitmapRenderTarget> pBitmapRenderTarget;

    D2D1_SIZE_F size = pRenderTarget->GetSize();

    HRESULT renderTargetCreated = pRenderTarget->CreateCompatibleRenderTarget(
        D2D1::SizeF(size.width, size.height), &pBitmapRenderTarget);

    if (FAILED(renderTargetCreated)) {
        MessageBox(NULL, L"Erro ao criar BitmapRenderTarget", L"Erro", MB_OK);
        return renderTargetCreated;
    }

    ComPtr<ID2D1Bitmap> pBitmap;
    pBitmapRenderTarget->GetBitmap(&pBitmap);

    std::vector<D2D1_COLOR_F> indexedColors;
    D2D1_COLOR_F defaultColor = D2D1::ColorF(255, 255, 255, 0);
    indexedColors.resize(width * height, defaultColor);

    Layer layer = { pBitmapRenderTarget, pBitmap };
    layers.push_back(layer);

    LayerOrder layerOrder = { static_cast<int>(layers.size()) - 1, static_cast<int>(layers.size()) - 1, indexedColors };
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

extern "C" __declspec(dllexport) void UpdateLayers() {
    if (!pRenderTarget) {
        return;
    }

    pRenderTarget->BeginDraw();
    pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

    for (size_t i = 0; i < layers.size(); ++i) {
        if (layers[i].pBitmap) {
            layers[i].pBitmapRenderTarget->BeginDraw();
            layers[i].pBitmapRenderTarget->Clear(D2D1::ColorF(255,255,255,0));
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
        else if (Actions[i].Tool == TRectangle) {

            ComPtr<ID2D1SolidColorBrush> brushColor;
            pRenderTarget->CreateSolidColorBrush(GetRGBColor(Actions[i].Color), &brushColor);

            layers[Actions[i].Layer].pBitmapRenderTarget->BeginDraw();

            layers[Actions[i].Layer].pBitmapRenderTarget->PushAxisAlignedClip(Actions[i].Position, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

            layers[Actions[i].Layer].pBitmapRenderTarget->FillRectangle(Actions[i].Position, brushColor.Get());

            layers[Actions[i].Layer].pBitmapRenderTarget->PopAxisAlignedClip();

            layers[Actions[i].Layer].pBitmapRenderTarget->EndDraw();

        }
        else if (Actions[i].Tool == TEllipse) {
            ComPtr<ID2D1SolidColorBrush> brushColor;
            pRenderTarget->CreateSolidColorBrush(GetRGBColor(Actions[i].Color), &brushColor);

            layers[Actions[i].Layer].pBitmapRenderTarget->BeginDraw();

            layers[Actions[i].Layer].pBitmapRenderTarget->FillEllipse(Actions[i].Ellipse, brushColor.Get());

            layers[Actions[i].Layer].pBitmapRenderTarget->EndDraw();
        }
        else if (Actions[i].Tool == TLine) {
            ComPtr<ID2D1SolidColorBrush> brushColor;
            pRenderTarget->CreateSolidColorBrush(GetRGBColor(Actions[i].Color), &brushColor);

            layers[Actions[i].Layer].pBitmapRenderTarget->BeginDraw();

            layers[Actions[i].Layer].pBitmapRenderTarget->DrawLine(Actions[i].Line.startPoint, Actions[i].Line.endPoint, brushColor.Get(), Actions[i].BrushSize, nullptr);

            layers[Actions[i].Layer].pBitmapRenderTarget->EndDraw();
        }
        else if (Actions[i].Tool == TBrush) {
            ID2D1SolidColorBrush* pBrush = nullptr;

            layers[Actions[i].Layer].pBitmapRenderTarget->BeginDraw();

            for (size_t j = 0; j < Actions[i].FreeForm.vertices.size(); j++) {
                D2D1_COLOR_F paintColor = GetRGBColor(Actions[i].Color);

                layers[Actions[i].Layer].pBitmapRenderTarget->CreateSolidColorBrush(paintColor, &pBrush);

                D2D1_RECT_F rect = D2D1::RectF(
                    Actions[i].FreeForm.vertices[j].x - Actions[i].BrushSize * 0.5f, // Left
                    Actions[i].FreeForm.vertices[j].y - Actions[i].BrushSize * 0.5f, // Top
                    Actions[i].FreeForm.vertices[j].x + Actions[i].BrushSize * 0.5f, // Right
                    Actions[i].FreeForm.vertices[j].y + Actions[i].BrushSize * 0.5f  // Bottom
                );

                layers[Actions[i].Layer].pBitmapRenderTarget->DrawRectangle(rect, pBrush);

                pBrush->Release();
                pBrush = nullptr;
            }

            layers[Actions[i].Layer].pBitmapRenderTarget->EndDraw();
        }
        else if (Actions[i].Tool == TPaintBucket) {
            const int threadCount = std::thread::hardware_concurrency();
            std::vector<std::thread> threads;
            std::mutex drawMutex;

            // Supõe que você tenha configurado fillBrush e layers[layerIndex].pBitmapRenderTarget

            ComPtr<ID2D1SolidColorBrush> brushColor;
            pRenderTarget->CreateSolidColorBrush(GetRGBColor(Actions[i].FillColor), &brushColor);

            layers[Actions[i].Layer].pBitmapRenderTarget->BeginDraw();

            for (int t = 0; t < threadCount; ++t) {
                threads.emplace_back([&, t]() {

                    for (size_t j = t; j < Actions[i].pixelsToFill.size(); j += threadCount) {
                        POINT p = Actions[i].pixelsToFill[j];
                        D2D1_RECT_F rect = D2D1::RectF((float)p.x, (float)p.y, (float)(p.x + 1), (float)(p.y + 1));

                        std::lock_guard<std::mutex> lock(drawMutex);
                        layers[Actions[i].Layer].pBitmapRenderTarget->FillRectangle(&rect, brushColor.Get());
                    }
                });
            }

            for (auto& th : threads) th.join();

            layers[layerIndex].pBitmapRenderTarget->EndDraw();           
        }
    }

    prevLeft = -1;
    prevTop = -1;

    HRESULT hr = pRenderTarget->EndDraw();

    DrawLayerPreview(layerIndex);

    if (FAILED(hr)) {
        MessageBox(NULL, L"Failed to update layers", L"Error", MB_OK);
    }

    RenderLayers();
}

/*extern "C" __declspec(dllexport) void UpdateLayers() {
    HDC hdc = GetDC(mainHWND);
    if (!hdc) return;

    RECT rect;
    GetClientRect(mainHWND, &rect);

    //HDC memDC = CreateCompatibleDC(hdc);
    //HBITMAP hBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
    //SelectObject(memDC, hBitmap);

    HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &rect, whiteBrush);
    DeleteObject(whiteBrush);

    for (size_t i = 0; i < layers.size(); ++i) {
        if (layers[i].hBitmap) {
            HDC layerDC = CreateCompatibleDC(hdc);
            SelectObject(layerDC, layers[i].hBitmap);

            RECT layerRect = { 0, 0, rect.right, rect.bottom };
            HBRUSH clearBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(layerDC, &layerRect, clearBrush);
            DeleteObject(clearBrush);

            DeleteDC(layerDC);
        }
    }

    for (const auto& action : Actions) {
        if (action.Tool == TEraser) {
            HDC layerDC = CreateCompatibleDC(hdc);
            SelectObject(layerDC, layers[action.Layer].hBitmap);

            RECT rectPosition = { action.Position.left, action.Position.top, action.Position.right, action.Position.bottom };

            HRGN clipRegion = CreateRectRgnIndirect(&rectPosition);
            SelectClipRgn(layerDC, clipRegion);

            HBRUSH transparentBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(layerDC, &rectPosition, transparentBrush);
            DeleteObject(transparentBrush);

            DeleteObject(clipRegion);
            DeleteDC(layerDC);
        }
        else if (action.Tool == TBrush) {
            SelectObject(hdc, layers[action.Layer].hBitmap);

            HPEN pen = CreatePen(PS_SOLID, action.BrushSize, action.Color);
            SelectObject(hdc, pen);

            for (const auto& point : action.FreeForm.vertices) {
                Rectangle(hdc,
                    point.x - action.BrushSize / 2, point.y - action.BrushSize / 2,
                    point.x + action.BrushSize / 2, point.y + action.BrushSize / 2);
            }

            DeleteObject(pen);
        }
        else if (action.Tool == TRectangle) {
            HDC layerDC = CreateCompatibleDC(hdc);
            SelectObject(layerDC, layers[action.Layer].hBitmap);

            HBRUSH brush = CreateSolidBrush(action.Color);
            RECT rectPosition = { action.Position.left, action.Position.top, action.Position.right, action.Position.bottom };
            FillRect(layerDC, &rectPosition, brush);
            DeleteObject(brush);

            DeleteDC(layerDC);
        }
        else if (action.Tool == TEllipse) {
            HDC layerDC = CreateCompatibleDC(hdc);
            SelectObject(layerDC, layers[action.Layer].hBitmap);

            HBRUSH brush = CreateSolidBrush(action.Color);
            SelectObject(layerDC, brush);
            
            int left = (int)(action.Ellipse.point.x - action.Ellipse.radiusX);
            int top = (int)(action.Ellipse.point.y - action.Ellipse.radiusY);
            int right = (int)(action.Ellipse.point.x + action.Ellipse.radiusX);
            int bottom = (int)(action.Ellipse.point.y + action.Ellipse.radiusY);

            Ellipse(layerDC, left, top, right, bottom);
            DeleteObject(brush);

            DeleteDC(layerDC);
        }
        else if (action.Tool == TLine) {
            HDC layerDC = CreateCompatibleDC(hdc);
            SelectObject(layerDC, layers[action.Layer].hBitmap);

            HPEN pen = CreatePen(PS_SOLID, action.BrushSize, action.Color);
            SelectObject(layerDC, pen);

            MoveToEx(layerDC, action.Line.startPoint.x, action.Line.startPoint.y, NULL);
            LineTo(layerDC, action.Line.endPoint.x, action.Line.endPoint.y);

            DeleteObject(pen);
            DeleteDC(layerDC);
        }
    }

    for (const auto& layer : layersOrder) {
        if (layers[layer.layerIndex].hBitmap) {
            HDC layerDC = CreateCompatibleDC(hdc);
            SelectObject(layerDC, layers[layer.layerIndex].hBitmap);

            //BitBlt(memDC, 0, 0, rect.right, rect.bottom, layerDC, 0, 0, SRCCOPY);

            DeleteDC(layerDC);
        }
    }

    for (const auto& action : getActionsWithTool(Actions, TPaintBucket)) {
        HBRUSH fillBrush = CreateSolidBrush(action.FillColor);
        SelectObject(hdc, fillBrush);

        COLORREF initialClickedColor = GetPixel(hdc, action.mouseX, action.mouseY);
        ExtFloodFill(hdc, action.mouseX, action.mouseY, initialClickedColor, FLOODFILLSURFACE);

        DeleteObject(fillBrush);
    }

    //BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

    ReleaseDC(mainHWND, hdc);

    RedrawWindow(mainHWND, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}*/

extern "C" __declspec(dllexport) void RenderLayers() {
    if (!pRenderTarget) {
        return;
    }

    pRenderTarget->BeginDraw();
    pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

    std::vector<LayerOrder> sortedLayers = layersOrder;
    std::sort(sortedLayers.begin(), sortedLayers.end(), [](const LayerOrder& a, const LayerOrder& b) {
        return a.layerOrder < b.layerOrder;
    });

    for (size_t i = 0; i < sortedLayers.size(); ++i) {
        const auto& layer = layers[sortedLayers[i].layerIndex];
        if (layer.pBitmap) {
            // Usa a interpolação apropriada para zoom de pixel art
            pRenderTarget->DrawBitmap(
                layer.pBitmap.Get(),
                D2D1::RectF(0, 0, static_cast<FLOAT>(512), static_cast<FLOAT>(512)), // destino antes da escala
                1.0f,
                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                D2D1::RectF(0, 0, 512 / zoomFactorW, 512 / zoomFactorH));
        }
    }

    for (size_t i = 0; i < sortedLayers.size(); ++i) {
        if (layers[sortedLayers[i].layerIndex].pBitmap) {
            
            pRenderTarget->DrawBitmap(layers[sortedLayers[i].layerIndex].pBitmap.Get());
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

    BITMAP bmp = {};
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    RECT bmpRect = { 0, 0, bmp.bmWidth, bmp.bmHeight };

    HBRUSH whiteBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);
    FillRect(compatibleDC, &bmpRect, whiteBrush);
    
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

                RECT pXY = scalePointsToButton(Actions[i].Line.startPoint.x, Actions[i].Line.startPoint.y, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top));
                RECT pWZ = scalePointsToButton(Actions[i].Line.endPoint.x, Actions[i].Line.endPoint.y, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top));

                RECT rectPoint = { pXY.left - 1, pXY.top - 1, pWZ.left + 1, pWZ.top + 1 };

                MoveToEx(compatibleDC,  rectPoint.left, rectPoint.top, NULL);
                LineTo(compatibleDC, rectPoint.right, rectPoint.bottom);

                DeleteObject(hPen);
            }
            else if (Actions[i].Tool == TPaintBucket) {
                RECT XY = scalePointsToButton(
                    Actions[i].mouseX,
                    Actions[i].mouseY,
                    (WindowRC.right - WindowRC.left),
                    (WindowRC.bottom - WindowRC.top),
                    (rc.right - rc.left),
                    (rc.bottom - rc.top)
                );

                COLORREF clickedColor = GetPixel(compatibleDC, XY.left, XY.top);

                HBRUSH fillBrush = CreateSolidBrush(Actions[i].FillColor);
                SelectObject(compatibleDC, fillBrush);

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

/*extern "C" __declspec(dllexport) void BrushTool(int left, int top, COLORREF hexColor, int brushSize) {
    ComPtr<ID2D1SolidColorBrush> brush;

    currentColor = hexColor;

    pRenderTarget->CreateSolidColorBrush(GetRGBColor(hexColor), &brush);
    
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

            layers[layerIndex].pBitmapRenderTarget->DrawRectangle(rect, brush.Get());

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

    layers[layerIndex].pBitmapRenderTarget->DrawRectangle(rect, brush.Get());

    if (left != prevLeft || top != prevTop) {
        if (left != -1 && top != -1) {
            Vertices.push_back(VERTICE{ static_cast<float>(left), static_cast<float>(top) });
        }
    }

    layers[layerIndex].pBitmapRenderTarget->EndDraw();

    pRenderTarget->EndDraw();

    prevLeft = left;
    prevTop = top;
}*/

extern "C" __declspec(dllexport) void BrushTool(int left, int top, COLORREF hexColor, int brushSize, bool pixelMode = false) {    
    ComPtr<ID2D1SolidColorBrush> brush;

    currentColor = hexColor;
    currentBrushSize = brushSize;
    isDrawingBrush = true;

    pRenderTarget->CreateSolidColorBrush(GetRGBColor(hexColor), &brush);

    pRenderTarget->BeginDraw();
    layers[layerIndex].pBitmapRenderTarget->BeginDraw();

    if (pixelMode) {
        // === PIXEL MODE: 1x1 rectangles only ===
        // Avoid drawing duplicates
        D2D1_RECT_F pixel = D2D1::RectF(
            static_cast<float>(left),
            static_cast<float>(top),
            static_cast<float>(left + 1),
            static_cast<float>(top + 1)
        );

        layers[layerIndex].pBitmapRenderTarget->FillRectangle(pixel, brush.Get());
        Vertices.push_back(VERTICE{ static_cast<float>(left), static_cast<float>(top) });
    }
    else {

        if (prevLeft == -1 && prevTop == -1) {
            prevLeft = left;
            prevTop = top;
        }

        // === NORMAL MODE (smooth brush stroke) ===
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
                    x - brushSize * 0.5f,
                    y - brushSize * 0.5f,
                    x + brushSize * 0.5f,
                    y + brushSize * 0.5f
                );

                layers[layerIndex].pBitmapRenderTarget->DrawRectangle(rect, brush.Get());

                if (x != -1 && y != -1) {
                    Vertices.push_back(VERTICE{ x, y });
                }
            }
        }

        D2D1_RECT_F rect = D2D1::RectF(
            left - brushSize * 0.5f,
            top - brushSize * 0.5f,
            left + brushSize * 0.5f,
            top + brushSize * 0.5f
        );

        layers[layerIndex].pBitmapRenderTarget->DrawRectangle(rect, brush.Get());

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
    ComPtr<ID2D1SolidColorBrush> brush;
    pRenderTarget->CreateSolidColorBrush(GetRGBColor(hexColor), &brush);

    if (!isDrawingRectangle) {
        AddLayer();
    }

    isDrawingRectangle = true;

    currentColor = hexColor;

    layers[LayersCount() - 1].pBitmapRenderTarget->BeginDraw();

    rectangle = D2D1::RectF(left, top, right, bottom);

    if (prevRectangle.left != 0 || prevRectangle.top != 0 || prevRectangle.right != 0 || prevRectangle.bottom != 0) {

        layers[LayersCount() - 1].pBitmapRenderTarget->PushAxisAlignedClip(prevRectangle, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

        layers[LayersCount() - 1].pBitmapRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));

        layers[LayersCount() - 1].pBitmapRenderTarget->PopAxisAlignedClip();
    }

    layers[LayersCount() - 1].pBitmapRenderTarget->PushAxisAlignedClip(rectangle, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    layers[LayersCount() - 1].pBitmapRenderTarget->FillRectangle(rectangle, brush.Get());

    layers[LayersCount() - 1].pBitmapRenderTarget->PopAxisAlignedClip();

    prevRectangle = D2D1::RectF(left, top, right, bottom);

    layers[LayersCount() - 1].pBitmapRenderTarget->EndDraw();
}

extern "C" __declspec(dllexport) void EllipseTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    ComPtr<ID2D1SolidColorBrush> brush;
    pRenderTarget->CreateSolidColorBrush(GetRGBColor(hexColor), &brush);
    
    if (!isDrawingEllipse) {
        AddLayer();
    }

    isDrawingEllipse = true;

    currentColor = hexColor;

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

    layers[LayersCount() - 1].pBitmapRenderTarget->FillEllipse(ellipse, brush.Get());

    prevEllipse = ellipse;

    layers[LayersCount() - 1].pBitmapRenderTarget->EndDraw();
}

extern "C" __declspec(dllexport) void LineTool(int xInitial, int yInitial, int x, int y, unsigned int hexColor, int brushSize) {
    ComPtr<ID2D1SolidColorBrush> brush;
    pRenderTarget->CreateSolidColorBrush(GetRGBColor(hexColor), &brush);
    currentBrushSize = brushSize;

    if (!isDrawingLine) {
        AddLayer();
    }

    isDrawingLine = true;

    currentColor = hexColor;
    
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

    layers[LayersCount() - 1].pBitmapRenderTarget->DrawLine(startPoint, endPoint, brush.Get(), brushSize, nullptr);

    layers[LayersCount() - 1].pBitmapRenderTarget->EndDraw();
}

/*FLOOD FILL*/

#define DX {1, -1, 0, 0}
#define DY {0, 0, 1, -1}

// Função para capturar bitmap da janela
std::vector<COLORREF> CaptureWindowPixels(HWND hWnd, int width, int height) {
    HDC hdcWindow = GetDC(hWnd);
    HDC hdcMemDC = CreateCompatibleDC(hdcWindow);

    HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, width, height);

    SelectObject(hdcMemDC, hBitmap);

    BitBlt(hdcMemDC, 0, 0, width, height, hdcWindow, 0, 0, SRCCOPY);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<COLORREF> pixels(width * height);
    GetDIBits(hdcMemDC, hBitmap, 0, height, pixels.data(), &bmi, DIB_RGB_COLORS);

    DeleteObject(hBitmap);
    DeleteDC(hdcMemDC);
    ReleaseDC(hWnd, hdcWindow);

    return pixels;
}

inline int Index(int x, int y, int width) {
    return y * width + x;
}

extern "C" __declspec(dllexport) void PaintBucketTool(int xInitial, int yInitial, COLORREF fillColor, HWND hWnd) {
    const int maxWidth = width; 
    const int maxHeight = height;

    std::vector<COLORREF> pixels = CaptureWindowPixels(hWnd, maxWidth, maxHeight);
    COLORREF targetColor = pixels[Index(xInitial, yInitial, maxWidth)];
    if (targetColor == fillColor) return;

    std::queue<POINT> q;
    std::set<int> visited;
    std::vector<POINT> pixelsToFill;

    q.push({ xInitial, yInitial });
    visited.insert(Index(xInitial, yInitial, maxWidth));

    const int dx[] = DX;
    const int dy[] = DY;

    while (!q.empty()) {
        POINT p = q.front(); q.pop();
        COLORREF current = pixels[Index(p.x, p.y, maxWidth)];

        if (current != targetColor) continue;

        pixelsToFill.push_back(p);

        for (int i = 0; i < 4; ++i) {
            int nx = p.x + dx[i];
            int ny = p.y + dy[i];

            if (nx >= 0 && nx < maxWidth && ny >= 0 && ny < maxHeight) {
                int idx = Index(nx, ny, maxWidth);
                if (visited.find(idx) == visited.end() && pixels[idx] == targetColor) {
                    q.push({ nx, ny });
                    visited.insert(idx);
                }
            }
        }
    }

    const int threadCount = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    std::mutex drawMutex;

    layers[layerIndex].pBitmapRenderTarget->BeginDraw();
    
    ComPtr<ID2D1SolidColorBrush> brush;
    pRenderTarget->CreateSolidColorBrush(GetRGBColor(fillColor), &brush);

    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t i = t; i < pixelsToFill.size(); i += threadCount) {
                POINT p = pixelsToFill[i];
                D2D1_RECT_F rect = D2D1::RectF((float)p.x, (float)p.y, (float)(p.x + 1), (float)(p.y + 1));

                std::lock_guard<std::mutex> lock(drawMutex);
                layers[layerIndex].pBitmapRenderTarget->FillRectangle(&rect, brush.Get());
            }
        });
    }

    for (auto& th : threads) th.join();

    layers[layerIndex].pBitmapRenderTarget->EndDraw();
    RenderLayers();

    ACTION action;
    action.Tool = TPaintBucket;
    action.Layer = layerIndex;
    action.FillColor = fillColor;
    action.mouseX = xInitial;
    action.mouseY = yInitial;
    action.pixelsToFill = pixelsToFill;
    Actions.push_back(action);

    DrawLayerPreview(layerIndex);
}

/* CLEANUP */

extern "C" __declspec(dllexport) void Cleanup() {
    SafeRelease(&pBrush);
    SafeRelease(&pRenderTarget);
    SafeRelease(pD2DFactory.GetAddressOf());
    SafeRelease(&transparentBrush);

    for (Layer& layer : layers) {
        layer.pBitmap.Reset(); // Automatically releases the bitmap
    }

    layers.clear();
    layersOrder.clear();
    Actions.clear();
    RedoActions.clear();
    LayerButtons.clear();
}

template <class T> void SafeRelease(T** ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}