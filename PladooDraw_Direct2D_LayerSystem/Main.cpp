// LayersDLL.cpp
#include "pch.h"
#include "Constants.h"
#include "Helpers.h"
#include "Main.h"
#include "Layers.h"
#include "SurfaceDial.h"
#include "Tools.h"
#include "Transforms.h"

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
            HCleanup();  
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

void CreateLogData(std::string fileName, std::string content) {
    HCreateLogData(fileName, content);
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

    HCleanup();
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
        in.read((char*)&lo.layerOrder, sizeof(lo.layerOrder));
        in.read((char*)&lo.layerIndex, sizeof(lo.layerIndex));

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

        ShowWindow(button, SW_SHOWDEFAULT);
        UpdateWindow(button);

        hLayerButtons[layerID] = button;

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

/* ACTIONS */

extern "C" __declspec(dllexport) void handleMouseUp() {
    THandleMouseUp();
}

extern "C" __declspec(dllexport) void Undo() {
    TUndo();
}

extern "C" __declspec(dllexport) void Redo() {
    TRedo();
}

/* TRANSFORM */

extern "C" __declspec(dllexport) float __stdcall GetZoomFactor() {
    return TGetZoomFactor();
}

extern "C" __declspec(dllexport) void ZoomIn_Default() {
    TZoomIn(0.5f);
}

extern "C" __declspec(dllexport) void ZoomOut_Default() {
    TZoomOut(0.5f);
}

extern "C" __declspec(dllexport) void ZoomIn(float zoomRatio) {
    TZoomIn(zoomRatio);
}

extern "C" __declspec(dllexport) void ZoomOut(float zoomRatio) {
    TZoomOut(zoomRatio);
}

extern "C" __declspec(dllexport) void Zoom() {
    TZoom();
}

extern "C" __declspec(dllexport) void IncreaseBrushSize_Default() {
    TIncreaseBrushSize(0.5f);
}
extern "C" __declspec(dllexport) void DecreaseBrushSize_Default() {
    TDecreaseBrushSize(0.5f);
}

extern "C" __declspec(dllexport) void IncreaseBrushSize(float sizeRatio) {
    TIncreaseBrushSize(sizeRatio);
}
extern "C" __declspec(dllexport) void DecreaseBrushSize(float sizeRatio) {
    TDecreaseBrushSize(sizeRatio);
}

/* LAYERS */

extern "C" __declspec(dllexport) HRESULT AddLayer(bool fromFile=false) {
    return TAddLayer(fromFile);
}

extern "C" __declspec(dllexport) int LayersCount() {
    return TLayersCount();
}

extern "C" __declspec(dllexport) void AddLayerButton(HWND layerButton) {
    TAddLayerButton(layerButton);
}

extern "C" __declspec(dllexport) HRESULT RemoveLayer() {
    return TRemoveLayer();
}

extern "C" __declspec(dllexport) HRESULT __stdcall RecreateLayers(HWND hWndLayer, HINSTANCE hLayerInstance, int btnWidth, int btnHeight, HWND* hLayerButtons, int& layerID, const wchar_t* szButtonClass, const wchar_t* msgText) {
    return TRecreateLayers(hWndLayer, hLayerInstance, btnWidth, btnHeight, hLayerButtons, layerID, szButtonClass, msgText);
}

extern "C" __declspec(dllexport) int GetLayer() {
    return TGetLayer();
}

extern "C" __declspec(dllexport) void SetLayer(int index) {
    TSetLayer(index);
}

extern "C" __declspec(dllexport) void ReorderLayerUp() {
    TReorderLayerUp();
}

extern "C" __declspec(dllexport) void ReorderLayerDown() {
    TReorderLayerDown();
}

void RenderActionToTarget(const ACTION& action, ID2D1RenderTarget* target) {
    TRenderActionToTarget(action, target);
}

void UpdateLayers(int layerIndexTarget = -1) {
    TUpdateLayers(layerIndexTarget);
}

extern "C" __declspec(dllexport) void RenderLayers() {
    TRenderLayers();
}

extern "C" __declspec(dllexport) void DrawLayerPreview(int currentLayer) {
    TDrawLayerPreview(currentLayer);
}

/* TOOLS */

extern "C" __declspec(dllexport) void SetSelectedTool(int pselectedTool) {
    HSetSelectedTool(pselectedTool);
}

extern "C" __declspec(dllexport) void EraserTool(int left, int top) {
    TEraserTool(left, top);
}

extern "C" __declspec(dllexport) void BrushTool(int left, int top, COLORREF hexColor, bool pixelMode, int pPixelSizeRatio) {
    TBrushTool(left, top, hexColor, pixelMode, pPixelSizeRatio);
}

extern "C" __declspec(dllexport) void RectangleTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    TRectangleTool(left, top, right, bottom, hexColor);
}

extern "C" __declspec(dllexport) void EllipseTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    TEllipseTool(left, top, right, bottom, hexColor);
}

extern "C" __declspec(dllexport) void LineTool(int xInitial, int yInitial, int x, int y, unsigned int hexColor) {
    TLineTool(xInitial, yInitial, x, y, hexColor);
}

extern "C" __declspec(dllexport) void PaintBucketTool(int mouseX, int mouseY, COLORREF fillColor, HWND hWnd) {
    TPaintBucketTool(mouseX, mouseY, fillColor, hWnd);
}

extern "C" __declspec(dllexport) void __stdcall SelectTool(int xInitial, int yInitial) {
    TSelectTool(xInitial, yInitial);
}

extern "C" __declspec(dllexport) void __stdcall MoveTool(int xInitial, int yInitial, int x, int y) {
    TMoveTool(xInitial, yInitial, x, y);
}

extern "C" __declspec(dllexport) void __stdcall UnSelectTool() {
    TUnSelectTool();
}

extern "C" __declspec(dllexport) void Cleanup() {
    HCleanup();
}