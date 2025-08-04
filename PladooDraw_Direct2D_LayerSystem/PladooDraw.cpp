// LayersDLL.cpp
#include "pch.h"
#include "PladooDraw.h"
#include "SurfaceDial.h"

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
#include <winrt/base.h>

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
   TPaintBucket,
   TMove
};

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

static float defaultBrushSize = 1.0f;
static float defaultEraserSize = 18.0f;
static float currentBrushSize = 1.0f;
static float currentEraserSize = 18.0f;

static int selectedTool = 1;

static int prevLeft = -1;
static int prevTop = -1;

static float zoomFactor = 1.0f;
static int pixelSizeRatio = -1;

static bool isPixelMode = false;

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

int selectedIndex = -1;
bool selectedAction = false;

int layerIndex = 0;
unsigned int lastHexColor = UINT_MAX;

/* MAIN */

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {  
    HRESULT hr; // Variable to store the return value of RoInitialize  

    switch (fdwReason) {  
        case DLL_PROCESS_ATTACH:  
            hr = RoInitialize(RO_INIT_SINGLETHREADED);  
            if (FAILED(hr)) {  
                // Handle the error, for example, by logging or returning FALSE  
                MessageBox(NULL, L"Failed to initialize Windows Runtime", L"Error", MB_OK | MB_ICONERROR);  
                return FALSE;  
            }  
            break;  

        case DLL_PROCESS_DETACH:  
            CleanupSurfaceDial();  
            Cleanup();  
            break;  

        case DLL_THREAD_ATTACH:  
        case DLL_THREAD_DETACH:  
            break;  
    }  

    return TRUE;  
}

extern "C" __declspec(dllexport) HRESULT Initialize(HWND pmainHWND, HWND pdocHWND, int pWidth, int pHeight, int pPixelSizeRatio) {
    mainHWND = pmainHWND;
    docHWND = pdocHWND;

    D2D1_FACTORY_OPTIONS options = { D2D1_DEBUG_LEVEL_INFORMATION };
    HRESULT factoryResult = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_MULTI_THREADED,
        __uuidof(ID2D1Factory1),
        &options,
        reinterpret_cast<void**>(pD2DFactory.GetAddressOf())
    );

    if (FAILED(factoryResult)) {
        MessageBox(pdocHWND, L"Erro ao criar Factory", L"Erro", MB_OK);

        return factoryResult;
    }

    InitializeDocument(pdocHWND, pWidth, pHeight, pPixelSizeRatio);
    InitializeSurfaceDial(pmainHWND);
        
    return S_OK;
}

HRESULT InitializeDocument(HWND hWnd, int pWidth, int pHeight, int pPixelSizeRatio) {

    RECT rc;
    GetClientRect(hWnd, &rc);

    width = rc.right - rc.left;
    height = rc.bottom - rc.top;

    if (pWidth != -1 && pHeight != -1) {
        width = pWidth;
        height = pHeight;
    }

    if (pPixelSizeRatio != -1) {
        pixelSizeRatio = pPixelSizeRatio;
    }

    D2D1_SIZE_U size = D2D1::SizeU(width, height);
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(hWnd, size);

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

extern "C" __declspec(dllexport) void SetSelectedTool(int pselectedTool) {
    selectedTool = pselectedTool;
}

void CreateLogData(std::string fileName, std::string content) {
    
    std::ofstream outFile(fileName, std::ios::app);

    if (!outFile.is_open()) {
        MessageBox(nullptr, L"Failed to open file for writing", L"Error", MB_OK);
        return;
    }

    outFile << content << std::endl;

    outFile.close();

    return;
}

void SaveBinaryProject(const std::wstring& filename) {
    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open()) {
        CreateLogData("error.log", "Failed to open file for writing: " + std::string(filename.begin(), filename.end()));
        return;
    }

    int magic = 0x30444450; // 'PDD0'
    int version = 2;
    out.write((char*)&magic, sizeof(magic));
    out.write((char*)&version, sizeof(version));
    out.write((char*)&width, sizeof(width));
    out.write((char*)&height, sizeof(height));
    out.write((char*)&pixelSizeRatio, sizeof(pixelSizeRatio));
    if (!out.good()) {
        CreateLogData("error.log", "Error writing header data");
        out.close();
        return;
    }

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

    // Save actions
    int actionCount = static_cast<int>(Actions.size());
    out.write((char*)&actionCount, sizeof(actionCount));
    for (const auto& a : Actions) {
        out.write((char*)&a.Tool, sizeof(a.Tool));
        out.write((char*)&a.Layer, sizeof(a.Layer));
        out.write((char*)&a.Position, sizeof(a.Position));
        out.write((char*)&a.Ellipse, sizeof(a.Ellipse));
        out.write((char*)&a.FillColor, sizeof(a.FillColor));
        out.write((char*)&a.Color, sizeof(a.Color));
        out.write((char*)&a.BrushSize, sizeof(a.BrushSize));
        out.write((char*)&a.IsFilled, sizeof(a.IsFilled));
        out.write((char*)&a.isPixelMode, sizeof(a.isPixelMode));
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

    out.close();
}

void LoadBinaryProject(const std::wstring& filename, HWND hWndLayer, HINSTANCE hLayerInstance, int btnWidth, int btnHeight, HWND* hLayerButtons, int& layerID, const wchar_t* szButtonClass, const wchar_t* msgText) {
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
    if (magic != 0x30444450 || version < 1 || version > 2) {
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
    
    if (version == 1) {
        int zoomFactorW = -1;
        int zoomFactorH = -1;
        in.read((char*)&zoomFactorW, sizeof(zoomFactorW));
        in.read((char*)&zoomFactorH, sizeof(zoomFactorH));

        if (!in.good() || zoomFactorW <= 0 || zoomFactorW > 10000 || zoomFactorH <= 0 || zoomFactorH > 10000) {
            CreateLogData("error.log", "Invalid zoomFactorH in version 1");
            in.close();
            return;
        }

        pixelSizeRatio = zoomFactorW;
    }
    else if (version >= 2) {
        in.read((char*)&pixelSizeRatio, sizeof(pixelSizeRatio));
        if (!in.good()) {
            pixelSizeRatio = -1;
        }
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
        in.read((char*)&a.isPixelMode, sizeof(a.isPixelMode));
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
    HRESULT hr = Initialize(mainHWND, docHWND, width, height, pixelSizeRatio);

    if (FAILED(hr)) {
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
        HRESULT hr = AddLayer(true);
        if (FAILED(hr)) {
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
            btnWidth,                         // nWidth
            btnHeight,                   // nHeight
            hWndLayer,                   // hWndParent
            (HMENU)(intptr_t)layerID,    // hMenu (control ID)
            hLayerInstance,              // hInstance
            NULL                         // lpParam
        );

        if (button == NULL) {
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

    for (int i = 0; i <= maxLayer; ++i) {
        UpdateLayers(layersOrder[i].layerIndex);
    }

   RenderLayers();
}

extern "C" __declspec(dllexport) void __stdcall SaveProjectDll(const char* pathAnsi) {
    int wlen = MultiByteToWideChar(CP_ACP, 0, pathAnsi, -1, nullptr, 0);
    std::wstring wpath(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, pathAnsi, -1, &wpath[0], wlen);

    SaveBinaryProject(wpath);
}

extern "C" __declspec(dllexport) void __stdcall LoadProjectDll(const char* pathAnsi, HWND hWndLayer, HINSTANCE hLayerInstance, int btnWidth, int btnHeight, HWND* hLayerButtons, int* layerID, const wchar_t* szButtonClass, const wchar_t* msgText) {
    int wlen = MultiByteToWideChar(CP_ACP, 0, pathAnsi, -1, nullptr, 0);
    std::wstring wpath(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, pathAnsi, -1, &wpath[0], wlen);

    LoadBinaryProject(wpath, hWndLayer, hLayerInstance, btnWidth, btnHeight, hLayerButtons, *layerID, L"Button", msgText);
}

D2D1_COLOR_F GetRGBColor(COLORREF color) {
    float r = (color & 0xFF) / 255.0f;         
    float g = ((color >> 8) & 0xFF) / 255.0f;  
    float b = ((color >> 16) & 0xFF) / 255.0f; 
    float a = 1.0f;                            

    D2D1_COLOR_F RGBColor = { r, g, b, a };

    return RGBColor;
}

RECT scalePointsToButton(int x, int y, int drawingAreaWidth, int drawingAreaHeight, int buttonWidth, int buttonHeight, bool pixelMode, int pPixelSizeRatio) {
    float scaleX = buttonWidth / static_cast<float>(drawingAreaWidth);
    float scaleY = buttonHeight / static_cast<float>(drawingAreaHeight);

    float scale = min(scaleX, scaleY);

    int offsetX = (buttonWidth - static_cast<int>(drawingAreaWidth * scale * zoomFactor)) / 2;
    int offsetY = (buttonHeight - static_cast<int>(drawingAreaHeight * scale * zoomFactor)) / 2;

    x = static_cast<int>(x * scale * zoomFactor) + offsetX;
    y = static_cast<int>(y * scale * zoomFactor) + offsetY;
    int scaledSize = static_cast<int>(pPixelSizeRatio * scale * zoomFactor);

    RECT rect = { x, y, 0, 0 };

    if (pixelMode) {
        rect = { x, y, x + scaledSize, y + scaledSize };
    }

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
    HDC hdc = GetDC(docHWND);

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

/* SURFACE DIAL */

void OnDialRotation(int menuIndex, int direction, float rotationDegrees) {
    if (direction == 0) return;

    if (menuIndex == 0) {
        if (direction > 0) {
            ZoomIn(rotationDegrees * 0.5f);
        }
        else {
            ZoomOut((rotationDegrees * 0.5f) * -1.0f);
        }
    }
    else if (menuIndex == 1) {
        if (direction > 0) {
            IncreaseBrushSize(rotationDegrees * 0.5f);
        }
        else {
            DecreaseBrushSize((rotationDegrees * 0.5f) * -1.0f);
        }
    }
}
void OnDialButtonClick(int menuIndex) {
    if (menuIndex == 0) {
        zoomFactor = 1.0f;
        Zoom();
    }
    else if (menuIndex == 1) {
        if (selectedTool == TBrush) {
            currentBrushSize = defaultBrushSize;
        }
        else if (selectedTool == TEraser) {
            currentEraserSize = defaultEraserSize;
        }
    }
}

/* ACTIONS */

extern "C" __declspec(dllexport) void handleMouseUp() {

    if (isDrawingRectangle || isDrawingEllipse || isDrawingLine) {
        
        ComPtr<ID2D1SolidColorBrush> brush;
        pRenderTarget->CreateSolidColorBrush(GetRGBColor(currentColor), &brush);

        layers[layerIndex].pBitmapRenderTarget->BeginDraw();              

        ACTION action;

        if (isDrawingRectangle) {
            layers[layerIndex].pBitmapRenderTarget->PushAxisAlignedClip(rectangle, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            layers[layerIndex].pBitmapRenderTarget->FillRectangle(rectangle, brush.Get());
            layers[layerIndex].pBitmapRenderTarget->PopAxisAlignedClip();

            action.Tool = TRectangle;
            action.Layer = layerIndex;
            action.Position = rectangle;
            action.Color = currentColor;

            rectangle = D2D1::RectF(0, 0, 0, 0);
            isDrawingRectangle = false;
        }
        
        if (isDrawingEllipse) {
            layers[layerIndex].pBitmapRenderTarget->FillEllipse(ellipse, brush.Get());

            action.Tool = TEllipse;
            action.Layer = layerIndex;
            action.Position = rectangle;
            action.Ellipse = ellipse;
            action.Color = currentColor;

            ellipse = D2D1::Ellipse(D2D1::Point2F(0, 0), 0, 0);
            isDrawingEllipse = false;
        }

        if (isDrawingLine) {
            layers[layerIndex].pBitmapRenderTarget->DrawLine(startPoint, endPoint, brush.Get(), currentBrushSize, nullptr);

            action.Tool = TLine;
            action.Layer = layerIndex;
            action.Line = { startPoint, endPoint };
            action.Color = currentColor;
            action.BrushSize = currentBrushSize;

            isDrawingLine = false;
        }
   
        layers[layerIndex].pBitmapRenderTarget->EndDraw();
        
        layersOrder.pop_back();
        layers.pop_back();
        Actions.push_back(action);
        action = ACTION();
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
        action.isPixelMode = isPixelMode;
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

    if (selectedAction) {
        UnSelectTool();
    }

    RenderLayers();
    DrawLayerPreview(layerIndex);
}

extern "C" __declspec(dllexport) void Undo() {
    if (Actions.size() > 0) {
        ACTION lastAction = Actions.back();
        RedoActions.push_back(lastAction);
        Actions.pop_back();
 
        UpdateLayers(layerIndex);
        RenderLayers();
        DrawLayerPreview(layerIndex);
    }
}

extern "C" __declspec(dllexport) void Redo() {
    if (RedoActions.size() > 0) {
        ACTION action = RedoActions.back();
        Actions.push_back(action);
        RedoActions.pop_back();

        UpdateLayers(layerIndex);
        RenderLayers();
        DrawLayerPreview(layerIndex);
    }
}

extern "C" __declspec(dllexport) float __stdcall GetZoomFactor() {
    return zoomFactor;
}

extern "C" __declspec(dllexport) void ZoomIn_Default() {
    ZoomIn(0.5f);
}

extern "C" __declspec(dllexport) void ZoomOut_Default() {
    ZoomOut(0.5f);
}

extern "C" __declspec(dllexport) void ZoomIn(float zoomIncrement) {
    if (zoomFactor < 3.0f) {
        zoomFactor = (std::min)(zoomFactor + zoomIncrement, 3.0f);
        Zoom();
    }
}

extern "C" __declspec(dllexport) void ZoomOut(float zoomIncrement) {
    if (zoomFactor > 1.0f) {
        zoomFactor = (std::max)(zoomFactor - zoomIncrement, 1.0f);
        Zoom();
    }
}

extern "C" __declspec(dllexport) void Zoom() {
    int newWidth = static_cast<int>(width * zoomFactor);
    int newHeight = static_cast<int>(height * zoomFactor);

    // Obter o retângulo do cliente da janela pai
    RECT parentRect;
    GetClientRect(GetParent(docHWND), &parentRect); // usa HWND pai

    // Centro da área do cliente do pai
    int parentWidth = parentRect.right - parentRect.left;
    int parentHeight = parentRect.bottom - parentRect.top;

    int centerX = parentWidth / 2;
    int centerY = parentHeight / 2;

    // Novo canto superior esquerdo (relativo ao pai)
    int newLeft = centerX - newWidth / 2;
    int newTop = centerY - newHeight / 2;

    // Redimensiona e reposiciona a janela filha
    SetWindowPos(
        docHWND,
        NULL,
        newLeft,
        newTop,
        newWidth,
        newHeight,
        SWP_NOZORDER | SWP_NOACTIVATE
    );

    // Força repaint da janela filha
    InvalidateRect(docHWND, NULL, TRUE);
    UpdateWindow(docHWND);

    RenderLayers();
}

extern "C" __declspec(dllexport) void IncreaseBrushSize_Default() {
    IncreaseBrushSize(0.5f);
}
extern "C" __declspec(dllexport) void DecreaseBrushSize_Default() {
    DecreaseBrushSize(0.5f);
}

extern "C" __declspec(dllexport) void IncreaseBrushSize(float sizeIncrement) {
    if (selectedTool == TBrush) {
        currentBrushSize = (std::min)(currentBrushSize + sizeIncrement, 100.0f);
    }
    else if (selectedTool == TEraser) {
        currentEraserSize = (std::min)(currentEraserSize + sizeIncrement, 100.0f);
    }
}
extern "C" __declspec(dllexport) void DecreaseBrushSize(float sizeIncrement) {
    if (selectedTool == TBrush) {
        currentBrushSize = (std::max)(currentBrushSize - sizeIncrement, defaultBrushSize);
    }
    else if (selectedTool == TEraser) {
        currentEraserSize = (std::max)(currentEraserSize - sizeIncrement, defaultEraserSize);
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

extern "C" __declspec(dllexport) HRESULT AddLayer(bool fromFile = false) {
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

    pBitmapRenderTarget->BeginDraw();
    pBitmapRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
    pBitmapRenderTarget->EndDraw();

    std::vector<D2D1_COLOR_F> indexedColors;
    D2D1_COLOR_F defaultColor = D2D1::ColorF(255, 255, 255, 0);
    indexedColors.resize(width * height, defaultColor);

    Layer layer = { pBitmapRenderTarget, pBitmap };
    layers.push_back(layer);

    if (fromFile == false) {
        LayerOrder layerOrder = { static_cast<int>(layers.size()) - 1, static_cast<int>(layers.size()) - 1, indexedColors };
        layersOrder.push_back(layerOrder);
    }

    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT RemoveLayer() {
    layers.erase(layers.begin() + layerIndex);

    for (auto it = layersOrder.begin(); it != layersOrder.end(); ++it) {
        if (it->layerIndex == layerIndex) {
            layersOrder.erase(it);
            break;
        }
    }

    for (auto& lo : layersOrder) {
        if (lo.layerIndex > layerIndex) {
            lo.layerIndex--; // shift left
        }

        lo.layerOrder--;
    }

    Actions.erase(
        std::remove_if(Actions.begin(), Actions.end(), [](const ACTION& a) {
            return a.Layer == layerIndex;
        }),
        Actions.end()
    );

    for (auto& a : Actions) {
        if (a.Layer > layerIndex)
            a.Layer--; // shift left
    }

    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT __stdcall RecreateLayers(HWND hWndLayer, HINSTANCE hLayerInstance, int btnWidth, int btnHeight, HWND* hLayerButtons, int& layerID, const wchar_t* szButtonClass, const wchar_t* msgText) {
    for (auto layerButton : LayerButtons) {
        DestroyWindow(layerButton.button);
    }

    LayerButtons.clear();

    int maxLayer = layers.size();

    for (int i = 0; i < maxLayer; ++i) {
        int yPos = btnHeight * i;
        HWND button = CreateWindowEx(
            0,                           // dwExStyle
            L"Button",                   // lpClassName
            msgText,                     // lpWindowName
            WS_CHILD | WS_VISIBLE | BS_BITMAP, // dwStyle
            0,                           // x
            yPos,                        // y
            btnWidth,                          // nWidth
            btnHeight,                   // nHeight
            hWndLayer,                   // hWndParent
            (HMENU)(intptr_t)layerID,    // hMenu (control ID)
            hLayerInstance,              // hInstance
            NULL                         // lpParam
        );

        if (button == NULL) {
            DWORD dwError = GetLastError();
            std::string errorMsg = "Failed to create button for layer " + std::to_string(i) + ": Error " + std::to_string(dwError);
            CreateLogData("error.log", errorMsg);
            return E_FAIL;
        }

        ShowWindow(button, SW_SHOWDEFAULT);
        UpdateWindow(button);

        hLayerButtons[layerID] = button;

        // Add button to LayerButtons
        AddLayerButton(button);

        if (i == maxLayer - 1) {
            layerID = i; // Atualiza layerID para o último índice usado
        }
    }

    return S_OK;
}

extern "C" __declspec(dllexport) int GetLayer() {
    return layerIndex;
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

void RenderActionToTarget(const ACTION& action, ID2D1RenderTarget* target) {
    if (!target) return;

    switch (action.Tool) {
    case TEraser:
        target->PushAxisAlignedClip(action.Position, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        target->Clear(D2D1::ColorF(0, 0, 0, 0));
        target->PopAxisAlignedClip();
        break;

    case TRectangle: {
        ComPtr<ID2D1SolidColorBrush> brush;
        target->CreateSolidColorBrush(GetRGBColor(action.Color), &brush);

        target->PushAxisAlignedClip(action.Position, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        target->FillRectangle(action.Position, brush.Get());
        target->PopAxisAlignedClip();
        break;
    }

    case TEllipse: {
        ComPtr<ID2D1SolidColorBrush> brush;
        target->CreateSolidColorBrush(GetRGBColor(action.Color), &brush);
        target->FillEllipse(action.Ellipse, brush.Get());
        break;
    }

    case TLine: {
        ComPtr<ID2D1SolidColorBrush> brush;
        target->CreateSolidColorBrush(GetRGBColor(action.Color), &brush);
        target->DrawLine(action.Line.startPoint, action.Line.endPoint, brush.Get(), action.BrushSize, nullptr);
        break;
    }

    case TBrush: {
        ID2D1SolidColorBrush* pBrush = nullptr;
        for (const auto& vertex : action.FreeForm.vertices) {
            D2D1_COLOR_F color = GetRGBColor(action.Color);
            target->CreateSolidColorBrush(color, &pBrush);

            if (action.isPixelMode) {
                int snappedLeft = static_cast<int>(vertex.x / pixelSizeRatio) * pixelSizeRatio;
                int snappedTop = static_cast<int>(vertex.y / pixelSizeRatio) * pixelSizeRatio;

                D2D1_RECT_F pixel = D2D1::RectF(
                    static_cast<float>(snappedLeft),
                    static_cast<float>(snappedTop),
                    static_cast<float>(snappedLeft + pixelSizeRatio),
                    static_cast<float>(snappedTop + pixelSizeRatio)
                );

                target->FillRectangle(pixel, pBrush);
            }
            else {
                D2D1_RECT_F rect = D2D1::RectF(
                    vertex.x - action.BrushSize * 0.5f,
                    vertex.y - action.BrushSize * 0.5f,
                    vertex.x + action.BrushSize * 0.5f,
                    vertex.y + action.BrushSize * 0.5f
                );
                target->DrawRectangle(rect, pBrush);
            }

            pBrush->Release();
            pBrush = nullptr;
        }
        break;
    }

    case TPaintBucket: {
        ComPtr<ID2D1SolidColorBrush> brush;
        target->CreateSolidColorBrush(GetRGBColor(action.FillColor), &brush);
        for (const auto& p : action.pixelsToFill) {
            D2D1_RECT_F rect = D2D1::RectF((float)p.x, (float)p.y, (float)(p.x + 1), (float)(p.y + 1));
            target->FillRectangle(&rect, brush.Get());
        }
        break;
    }

    default:
        break;
    }
}

void UpdateLayers(int layerIndexTarget = -1) {

    if (layerIndex < 0 || layerIndex >= layers.size()) return;

    if (layerIndexTarget == -1) {
        layerIndexTarget = layerIndex;
    }

    auto& layer = layers[layerIndexTarget];

    if (!layer.pBitmapRenderTarget) return;

    layer.pBitmapRenderTarget->BeginDraw();
    layer.pBitmapRenderTarget->Clear(D2D1::ColorF(255, 255, 255, 0)); // Transparente

    for (const auto& action : Actions) {
        if (action.Layer == layerIndexTarget) {
            RenderActionToTarget(action, layer.pBitmapRenderTarget.Get());
        }
    }

    layer.pBitmapRenderTarget->EndDraw();
}

extern "C" __declspec(dllexport) void RenderLayers() {
    if (!pRenderTarget) {
        return;
    }

    pRenderTarget->BeginDraw();

    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

    std::vector<LayerOrder> sortedLayers = layersOrder;
    std::sort(sortedLayers.begin(), sortedLayers.end(), [](const LayerOrder& a, const LayerOrder& b) {
        return a.layerOrder < b.layerOrder;
        });

    for (size_t i = 0; i < sortedLayers.size(); ++i) {
        const auto& layer = layers[sortedLayers[i].layerIndex];

        if (layer.pBitmap) {
            pRenderTarget->DrawBitmap(layer.pBitmap.Get());
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
    GetClientRect(docHWND, &WindowRC);

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
                    float scaledLeft = static_cast<float>(Actions[i].FreeForm.vertices[j].x);
                    float scaledTop = static_cast<float>(Actions[i].FreeForm.vertices[j].y);

                    float scaledBrushSize = static_cast<float>(Actions[i].BrushSize) / zoomFactor;
                    float scaledPixelSizeRatio = static_cast<float>(pixelSizeRatio) / zoomFactor;

                    int snappedLeft = static_cast<int>(scaledLeft / scaledPixelSizeRatio) * scaledPixelSizeRatio;
                    int snappedTop = static_cast<int>(scaledTop / scaledPixelSizeRatio) * scaledPixelSizeRatio;

                    HBRUSH hBrush = CreateSolidBrush(Actions[i].Color);
                    HPEN hPen = CreatePen(PS_SOLID, scaledBrushSize, Actions[i].Color);

                    SelectObject(compatibleDC, hBrush);
                    SelectObject(compatibleDC, hPen);

                    if (Actions[i].isPixelMode) {
                        RECT pXY = scalePointsToButton(snappedLeft, snappedTop, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), true, pixelSizeRatio);
                        RECT rectPoint = { pXY.left, pXY.top, pXY.right, pXY.bottom };
                        Rectangle(compatibleDC, rectPoint.left, rectPoint.top, rectPoint.right, rectPoint.bottom);
                    }
                    else
                    {
                        RECT pXY = scalePointsToButton(Actions[i].FreeForm.vertices[j].x, Actions[i].FreeForm.vertices[j].y, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);
                        RECT rectPoint = { pXY.left - 1, pXY.top - 1, pXY.left + 1, pXY.top + 1 };
                        Ellipse(compatibleDC, rectPoint.left, rectPoint.top, rectPoint.right, rectPoint.bottom);
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

                RECT pXY = scalePointsToButton(Actions[i].Position.left, Actions[i].Position.top, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);
                RECT pWZ= scalePointsToButton(Actions[i].Position.right, Actions[i].Position.bottom, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);

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

                RECT pXY = scalePointsToButton(Actions[i].Position.left, Actions[i].Position.top, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);
                RECT pWZ = scalePointsToButton(Actions[i].Position.right, Actions[i].Position.bottom, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);

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

                RECT pXY = scalePointsToButton(Actions[i].Position.left, Actions[i].Position.top, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);
                RECT pWZ = scalePointsToButton(Actions[i].Position.right, Actions[i].Position.bottom, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);

                RECT rectPoint = { pXY.left - 1, pXY.top - 1, pWZ.left + 1, pWZ.top + 1 };

                Ellipse(compatibleDC, rectPoint.left, rectPoint.top, rectPoint.right, rectPoint.bottom);

                DeleteObject(hBrush);
                DeleteObject(hPen);
            }
            else if (Actions[i].Tool == TLine) {
                HPEN hPen = CreatePen(PS_SOLID, 1, Actions[i].Color);

                SelectObject(compatibleDC, hPen);

                RECT pXY = scalePointsToButton(Actions[i].Line.startPoint.x, Actions[i].Line.startPoint.y, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);
                RECT pWZ = scalePointsToButton(Actions[i].Line.endPoint.x, Actions[i].Line.endPoint.y, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);

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
                    (rc.bottom - rc.top),
                    false,
                    1
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

/*FLOOD FILL*/

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

/* MOVE TOOL AUX */

bool IsPointNearSegment(float px, float py, float x1, float y1, float x2, float y2, float tolerance = 5.0f) {
    float dx = x2 - x1;
    float dy = y2 - y1;

    if (dx == 0 && dy == 0) {
        return (std::hypot(px - x1, py - y1) <= tolerance);
    }

    float t = ((px - x1) * dx + (py - y1) * dy) / (dx * dx + dy * dy);
    t = (std::max)(0.0f, (std::min)(1.0f, t));

    float projX = x1 + t * dx;
    float projY = y1 + t * dy;

    float distance = std::hypot(px - projX, py - projY);
    return (distance <= tolerance);
}

bool IsPointNearEdge(const std::vector<VERTICE>& vertices, float px, float py) {
    for (size_t i = 1; i < vertices.size(); ++i) {
        float x1 = vertices[i - 1].x;
        float y1 = vertices[i - 1].y;
        float x2 = vertices[i].x;
        float y2 = vertices[i].y;

        if (IsPointNearSegment(px, py, x1, y1, x2, y2)) {
            return true;
        }
    }
    return false;
}

bool IsPointInsideRect(const D2D1_RECT_F& rect, float x, float y) {
    return (x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom);
}

bool IsPointInsideEllipse(const D2D1_ELLIPSE& ellipse, float x, float y) {
    float dx = x - ellipse.point.x;
    float dy = y - ellipse.point.y;
    return ((dx * dx) / (ellipse.radiusX * ellipse.radiusX) + (dy * dy) / (ellipse.radiusY * ellipse.radiusY)) <= 1.0f;
}

bool IsPointNearLine(const D2D1_RECT_F& lineRect, float x, float y, float tolerance) {
    // lineRect.left/top = ponto A
    // lineRect.right/bottom = ponto B
    float x1 = lineRect.left, y1 = lineRect.top;
    float x2 = lineRect.right, y2 = lineRect.bottom;

    float A = x - x1;
    float B = y - y1;
    float C = x2 - x1;
    float D = y2 - y1;

    float dot = A * C + B * D;
    float len_sq = C * C + D * D;
    float param = len_sq != 0 ? dot / len_sq : -1;

    float xx, yy;
    if (param < 0) {
        xx = x1;
        yy = y1;
    }
    else if (param > 1) {
        xx = x2;
        yy = y2;
    }
    else {
        xx = x1 + param * C;
        yy = y1 + param * D;
    }

    float dx = x - xx;
    float dy = y - yy;
    return (dx * dx + dy * dy) <= (tolerance * tolerance);
}

bool HitTestAction(const ACTION& action, float x, float y) {
    switch (action.Tool) {
    case TBrush:
        return IsPointNearEdge(action.FreeForm.vertices, x, y);
    case TRectangle:
        return IsPointInsideRect(action.Position, x, y);
    case TEllipse:
        return IsPointInsideEllipse(action.Ellipse, x, y);
    case TLine:
        return IsPointNearLine(action.Position, x, y, 5.0f); // 5 px tolerance
    default:
        return false;
    }
}

/* TOOLS */

extern "C" __declspec(dllexport) void EraserTool(int left, int top) {
    if (prevLeft == -1 && prevTop == -1) {
        prevLeft = static_cast<float>(left) / zoomFactor;
        prevTop = static_cast<float>(top) / zoomFactor;
    }

    auto target = layers[layerIndex].pBitmapRenderTarget;

    target->BeginDraw();

    // Scale coordinates and size
    float scaledLeft = static_cast<float>(left) / zoomFactor;
    float scaledTop = static_cast<float>(top) / zoomFactor;
    float scaledBrushSize = static_cast<float>(currentEraserSize) / zoomFactor;

    float dx = scaledLeft - prevLeft;
    float dy = scaledTop - prevTop;
    float distance = sqrt(dx * dx + dy * dy);

    float step = scaledBrushSize * 0.5f;

    if (distance > step) {
        float steps = distance / step;
        float deltaX = dx / steps;
        float deltaY = dy / steps;

        for (float i = 0; i < steps; i++) {
            float x = prevLeft + i * deltaX;
            float y = prevTop + i * deltaY;

            D2D1_RECT_F rect = D2D1::RectF(
                x - scaledBrushSize,
                y - scaledBrushSize,
                x + scaledBrushSize,
                y + scaledBrushSize
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
                    action.BrushSize = scaledBrushSize;
                    action.IsFilled = false;
                    Actions.push_back(action);
                }
            }
        }
    }

    D2D1_RECT_F rect = D2D1::RectF(
        scaledLeft - scaledBrushSize,
        scaledTop - scaledBrushSize,
        scaledLeft + scaledBrushSize,
        scaledTop + scaledBrushSize
    );

    target->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    target->Clear(D2D1::ColorF(0, 0, 0, 0));
    target->PopAxisAlignedClip();

    if (scaledLeft != prevLeft || scaledTop != prevTop) {
        if (scaledLeft != -1 && scaledTop != -1) {
            ACTION action;
            action.Tool = 0;
            action.Layer = layerIndex;
            action.Position = rect;
            action.BrushSize = scaledBrushSize;
            action.IsFilled = false;
            Actions.push_back(action);
        }
    }

    prevLeft = scaledLeft;
    prevTop = scaledTop;

    target->EndDraw();
}

extern "C" __declspec(dllexport) void BrushTool(int left, int top, COLORREF hexColor, bool pixelMode, int pPixelSizeRatio) {
    ComPtr<ID2D1SolidColorBrush> brush;

    if (pixelSizeRatio == -1) {
        pixelSizeRatio = pPixelSizeRatio;
    }

    currentColor = hexColor;
    isDrawingBrush = true;
    isPixelMode = pixelMode;

    pRenderTarget->CreateSolidColorBrush(GetRGBColor(hexColor), &brush);

    pRenderTarget->BeginDraw();
    layers[layerIndex].pBitmapRenderTarget->BeginDraw();

    // Scale coordinates and sizes
    float scaledLeft = static_cast<float>(left) / zoomFactor;
    float scaledTop = static_cast<float>(top) / zoomFactor;
    float scaledBrushSize = static_cast<float>(currentBrushSize) / zoomFactor;
    float scaledPixelSizeRatio = static_cast<float>(pixelSizeRatio) / zoomFactor;

    if (pixelMode) {
        // PIXEL MODE: 1x1 rectangles only
        int snappedLeft = static_cast<int>(scaledLeft / scaledPixelSizeRatio) * scaledPixelSizeRatio;
        int snappedTop = static_cast<int>(scaledTop / scaledPixelSizeRatio) * scaledPixelSizeRatio;

        D2D1_RECT_F pixel = D2D1::RectF(
            static_cast<float>(snappedLeft),
            static_cast<float>(snappedTop),
            static_cast<float>(snappedLeft + scaledPixelSizeRatio),
            static_cast<float>(snappedTop + scaledPixelSizeRatio)
        );

        layers[layerIndex].pBitmapRenderTarget->FillRectangle(pixel, brush.Get());
        Vertices.push_back(VERTICE{ static_cast<float>(snappedLeft), static_cast<float>(snappedTop) });
    }
    else {
        if (prevLeft == -1 && prevTop == -1) {
            prevLeft = scaledLeft;
            prevTop = scaledTop;
        }

        // NORMAL MODE (smooth brush stroke)
        float dx = scaledLeft - prevLeft;
        float dy = scaledTop - prevTop;
        float distance = sqrt(dx * dx + dy * dy);
        float step = scaledBrushSize * 0.5f;

        if (distance > step) {
            float steps = distance / step;
            float deltaX = dx / steps;
            float deltaY = dy / steps;

            for (float i = 0; i < steps; i++) {
                float x = prevLeft + i * deltaX;
                float y = prevTop + i * deltaY;

                D2D1_RECT_F rect = D2D1::RectF(
                    x - scaledBrushSize * 0.5f,
                    y - scaledBrushSize * 0.5f,
                    x + scaledBrushSize * 0.5f,
                    y + scaledBrushSize * 0.5f
                );

                layers[layerIndex].pBitmapRenderTarget->DrawRectangle(rect, brush.Get());
                layers[layerIndex].pBitmapRenderTarget->FillRectangle(rect, brush.Get());

                /*D2D1_ELLIPSE ellipse = D2D1::Ellipse(
                    D2D1::Point2F(x - scaledBrushSize, y - scaledBrushSize),
                    scaledBrushSize / 2.0f,
                    scaledBrushSize / 2.0f
                );

                layers[layerIndex].pBitmapRenderTarget->FillEllipse(ellipse, brush.Get());*/

                if (x != -1 && y != -1) {
                    Vertices.push_back(VERTICE{ x, y });
                }
            }
        }

        D2D1_RECT_F rect = D2D1::RectF(
            scaledLeft - scaledBrushSize * 0.5f,
            scaledTop - scaledBrushSize * 0.5f,
            scaledLeft + scaledBrushSize * 0.5f,
            scaledTop + scaledBrushSize * 0.5f
        );

        layers[layerIndex].pBitmapRenderTarget->DrawRectangle(rect, brush.Get());
        layers[layerIndex].pBitmapRenderTarget->FillRectangle(rect, brush.Get());

        /*D2D1_ELLIPSE ellipse = D2D1::Ellipse(
            D2D1::Point2F(scaledLeft - scaledBrushSize, scaledTop - scaledBrushSize),
            scaledBrushSize / 2.0f,
            scaledBrushSize / 2.0f
        );

        layers[layerIndex].pBitmapRenderTarget->FillEllipse(ellipse, brush.Get());*/

        if (scaledLeft != -1 && scaledTop != -1) {
            Vertices.push_back(VERTICE{ static_cast<float>(scaledLeft), static_cast<float>(scaledTop) });
        }
    }

    layers[layerIndex].pBitmapRenderTarget->EndDraw();
    pRenderTarget->EndDraw();

    prevLeft = scaledLeft;
    prevTop = scaledTop;
}

extern "C" __declspec(dllexport) void RectangleTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    ComPtr<ID2D1SolidColorBrush> brush;
    pRenderTarget->CreateSolidColorBrush(GetRGBColor(hexColor), &brush);

    if (!isDrawingRectangle) {
        AddLayer(false);
    }

    isDrawingRectangle = true;

    currentColor = hexColor;

    layers[LayersCount() - 1].tempLayer = true;

    layers[LayersCount() - 1].pBitmapRenderTarget->BeginDraw();

    // Scale coordinates
    float scaledLeft = static_cast<float>(left) / zoomFactor;
    float scaledTop = static_cast<float>(top) / zoomFactor;
    float scaledRight = static_cast<float>(right) / zoomFactor;
    float scaledBottom = static_cast<float>(bottom) / zoomFactor;

    rectangle = D2D1::RectF(scaledLeft, scaledTop, scaledRight, scaledBottom);

    if (prevRectangle.left != 0 || prevRectangle.top != 0 || prevRectangle.right != 0 || prevRectangle.bottom != 0) {
        layers[LayersCount() - 1].pBitmapRenderTarget->PushAxisAlignedClip(prevRectangle, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        layers[LayersCount() - 1].pBitmapRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
        layers[LayersCount() - 1].pBitmapRenderTarget->PopAxisAlignedClip();
    }

    layers[LayersCount() - 1].pBitmapRenderTarget->PushAxisAlignedClip(rectangle, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    layers[LayersCount() - 1].pBitmapRenderTarget->FillRectangle(rectangle, brush.Get());
    layers[LayersCount() - 1].pBitmapRenderTarget->PopAxisAlignedClip();

    prevRectangle = D2D1::RectF(scaledLeft, scaledTop, scaledRight, scaledBottom);

    layers[LayersCount() - 1].pBitmapRenderTarget->EndDraw();
}

extern "C" __declspec(dllexport) void EllipseTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    ComPtr<ID2D1SolidColorBrush> brush;
    pRenderTarget->CreateSolidColorBrush(GetRGBColor(hexColor), &brush);

    if (!isDrawingEllipse) {
        AddLayer(false);
    }

    isDrawingEllipse = true;

    currentColor = hexColor;

    layers[LayersCount() - 1].tempLayer = true;

    layers[LayersCount() - 1].pBitmapRenderTarget->BeginDraw();

    // Scale coordinates
    float scaledLeft = static_cast<float>(left) / zoomFactor;
    float scaledTop = static_cast<float>(top) / zoomFactor;
    float scaledRight = static_cast<float>(right) / zoomFactor;
    float scaledBottom = static_cast<float>(bottom) / zoomFactor;

    ellipse = D2D1::Ellipse(
        D2D1::Point2F(scaledLeft, scaledTop),
        abs(scaledRight - scaledLeft) / 2.0f,
        abs(scaledBottom - scaledTop) / 2.0f
    );

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

extern "C" __declspec(dllexport) void LineTool(int xInitial, int yInitial, int x, int y, unsigned int hexColor) {
    ComPtr<ID2D1SolidColorBrush> brush;
    pRenderTarget->CreateSolidColorBrush(GetRGBColor(hexColor), &brush);

    if (!isDrawingLine) {
        AddLayer(false);
    }

    isDrawingLine = true;

    currentColor = hexColor;

    layers[LayersCount() - 1].tempLayer = true;

    layers[LayersCount() - 1].pBitmapRenderTarget->BeginDraw();

    // Scale coordinates and size
    float scaledXInitial = static_cast<float>(xInitial) / zoomFactor;
    float scaledYInitial = static_cast<float>(yInitial) / zoomFactor;
    float scaledX = static_cast<float>(x) / zoomFactor;
    float scaledY = static_cast<float>(y) / zoomFactor;
    float scaledBrushSize = static_cast<float>(currentBrushSize) / zoomFactor;

    if (startPoint.x != 0 || startPoint.y != 0 || endPoint.x != 0 || endPoint.y != 0) {
        D2D1_RECT_F lineBounds = D2D1::RectF(
            min(startPoint.x, endPoint.x) - scaledBrushSize,
            min(startPoint.y, endPoint.y) - scaledBrushSize,
            max(startPoint.x, endPoint.x) + scaledBrushSize,
            max(startPoint.y, endPoint.y) + scaledBrushSize
        );

        layers[LayersCount() - 1].pBitmapRenderTarget->PushAxisAlignedClip(lineBounds, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        layers[LayersCount() - 1].pBitmapRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
        layers[LayersCount() - 1].pBitmapRenderTarget->PopAxisAlignedClip();
    }

    startPoint = D2D1::Point2F(scaledXInitial, scaledYInitial);
    endPoint = D2D1::Point2F(scaledX, scaledY);

    layers[LayersCount() - 1].pBitmapRenderTarget->DrawLine(startPoint, endPoint, brush.Get(), scaledBrushSize, nullptr);

    layers[LayersCount() - 1].pBitmapRenderTarget->EndDraw();
}

extern "C" __declspec(dllexport) void PaintBucketTool(int mouseX, int mouseY, COLORREF fillColor, HWND hWnd) {
    // 1) Captura o tamanho real (ampliado) da janela de desenho
    RECT rc;
    GetClientRect(docHWND, &rc);
    int capW = rc.right - rc.left;   // width * zoomFactor
    int capH = rc.bottom - rc.top;   // height * zoomFactor

    // 2) Captura pixels da tela ampliada
    std::vector<COLORREF> pixels = CaptureWindowPixels(docHWND, capW, capH);
    if (pixels.size() < static_cast<size_t>(capW * capH)) {
        CreateLogData("error.log", "PaintBucketTool: falha ao capturar pixels ampliados");
        return;
    }

    // 3) Flood-fill em screen coords
    int startX = mouseX;
    int startY = mouseY;
    int startIdx = startY * capW + startX;
    COLORREF targetColor = pixels[startIdx];
    if (targetColor == fillColor) return;

    std::vector<POINT> rawPixelsToFill;
    rawPixelsToFill.reserve(10000);
    std::queue<POINT> q;
    q.push({ startX, startY });
    std::vector<bool> visited(capW * capH);
    visited[startIdx] = true;
        
    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    while (!q.empty()) {
        POINT p = q.front(); q.pop();
        rawPixelsToFill.push_back(p);
        for (int i = 0; i < 4; ++i) {
            int nx = p.x + dx[i];
            int ny = p.y + dy[i];
            if (nx >= 0 && nx < capW && ny >= 0 && ny < capH) {
                int ni = ny * capW + nx;
                if (!visited[ni] && pixels[ni] == targetColor) {
                    visited[ni] = true;
                    q.push({ nx, ny });
                }
            }
        }
    }

    // 4) Converte para canvas “sem zoom”
    std::vector<POINT> pixelsToFill;
    pixelsToFill.reserve(rawPixelsToFill.size());
    for (auto& p : rawPixelsToFill) {
        pixelsToFill.push_back({
            static_cast<int>(std::floor(p.x / zoomFactor)),
            static_cast<int>(std::floor(p.y / zoomFactor))
            });
    }

    // 5) Desenha em múltiplas threads no BitmapRenderTarget
    auto target = layers[layerIndex].pBitmapRenderTarget.Get();
    ComPtr<ID2D1SolidColorBrush> brush;
    pRenderTarget->CreateSolidColorBrush(GetRGBColor(fillColor), &brush);

    const int threadCount = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    std::mutex drawMutex;

    target->BeginDraw();
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t j = t; j < pixelsToFill.size(); j += threadCount) {
                POINT ip = pixelsToFill[j];
                D2D1_RECT_F r = D2D1::RectF(
                    float(ip.x), float(ip.y),
                    float(ip.x + 1), float(ip.y + 1)
                );
                // serialize chamadas D2D
                std::lock_guard<std::mutex> lk(drawMutex);
                target->FillRectangle(r, brush.Get());
            }
            });
    }
    for (auto& th : threads) th.join();
    target->EndDraw();

    // 6) Salva ACTION
    ACTION action;
    action.Tool = TPaintBucket;
    action.Layer = layerIndex;
    action.FillColor = fillColor;
    action.mouseX = static_cast<int>(std::floor(startX / zoomFactor));
    action.mouseY = static_cast<int>(std::floor(startY / zoomFactor));
    action.pixelsToFill = std::move(pixelsToFill);
    Actions.push_back(std::move(action));

    // 7) Re-renderiza
    RenderLayers();
    DrawLayerPreview(layerIndex);
}

extern "C" __declspec(dllexport) void __stdcall SelectTool(int xInitial, int yInitial) {
    if (selectedAction) return;

    // Scale coordinates
    float scaledXInitial = static_cast<float>(xInitial) / zoomFactor;
    float scaledYInitial = static_cast<float>(yInitial) / zoomFactor;

    for (int i = Actions.size() - 1; i >= 0; --i) {

        const ACTION& Action = Actions[i];
        if (Action.Layer != layerIndex) continue;

        if (HitTestAction(Action, scaledXInitial, scaledYInitial)) {
            selectedIndex = i;
            selectedAction = true;
            break;
        }
    }

    if (selectedAction == false) return;
}

extern "C" __declspec(dllexport) void __stdcall MoveTool(int xInitial, int yInitial, int x, int y) {
    if (layerIndex < 0 || selectedIndex < 0 || selectedIndex >= Actions.size()) {
        return;
    }

    const ACTION& selected = Actions[selectedIndex];

    // Scale coordinates
    float scaledXInitial = static_cast<float>(xInitial) / zoomFactor;
    float scaledYInitial = static_cast<float>(yInitial) / zoomFactor;
    float scaledX = static_cast<float>(x) / zoomFactor;
    float scaledY = static_cast<float>(y) / zoomFactor;

    // Step 1: Calculate bounding box
    float minX = FLT_MAX;
    float minY = FLT_MAX;
    float maxX = -FLT_MAX;
    float maxY = -FLT_MAX;

    if (selected.Tool == TBrush) {
        for (const auto& action : Actions) {
            if (action.Layer != layerIndex || action.Tool != TBrush) continue;

            for (const auto& v : action.FreeForm.vertices) {
                minX = (std::min)(minX, v.x);
                minY = (std::min)(minY, v.y);
                maxX = (std::max)(maxX, v.x);
                maxY = (std::max)(maxY, v.y);
            }
        }
    }
    else {
        switch (selected.Tool) {
        case TRectangle:
            minX = selected.Position.left;
            minY = selected.Position.top;
            maxX = selected.Position.right;
            maxY = selected.Position.bottom;
            break;

        case TEllipse:
            minX = selected.Ellipse.point.x;
            minY = selected.Ellipse.point.y;
            maxX = selected.Ellipse.point.x;
            maxY = selected.Ellipse.point.y;
            break;

        case TLine:
            minX = (std::min)(selected.Line.startPoint.x, selected.Line.endPoint.x);
            minY = (std::min)(selected.Line.startPoint.y, selected.Line.endPoint.y);
            maxX = (std::max)(selected.Line.startPoint.x, selected.Line.endPoint.x);
            maxY = (std::max)(selected.Line.startPoint.y, selected.Line.endPoint.y);
            break;

        default:
            break;
        }
    }

    if (minX == FLT_MAX || minY == FLT_MAX) {
        return;
    }

    // Step 2: Calculate movement delta
    float centerX = (minX + maxX) / 2.0f;
    float centerY = (minY + maxY) / 2.0f;
    float deltaX = scaledX - centerX;
    float deltaY = scaledY - centerY;

    // Step 3: Apply movement
    if (selected.Tool == TBrush) {
        for (auto& action : Actions) {
            if (action.Layer != layerIndex || action.Tool != TBrush) continue;

            for (auto& v : action.FreeForm.vertices) {
                v.x += deltaX;
                v.y += deltaY;
            }
        }
    }
    else {
        ACTION& action = Actions[selectedIndex];

        switch (action.Tool) {
        case TRectangle:
            action.Position.left += deltaX;
            action.Position.top += deltaY;
            action.Position.right += deltaX;
            action.Position.bottom += deltaY;
            break;

        case TEllipse:
            action.Ellipse.point.x += deltaX;
            action.Ellipse.point.y += deltaY;
            break;

        case TLine:
            action.Line.startPoint.x += deltaX;
            action.Line.startPoint.y += deltaY;
            action.Line.endPoint.x += deltaX;
            action.Line.endPoint.y += deltaY;
            break;

        default:
            break;
        }
    }

    UpdateLayers(layerIndex);
}

extern "C" __declspec(dllexport) void __stdcall UnSelectTool() {
    selectedAction = false;
    RenderLayers();
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