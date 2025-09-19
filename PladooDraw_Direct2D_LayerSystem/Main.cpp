// LayersDLL.cpp
#include "pch.h"
#include "Constants.h"
#include "Helpers.h"
#include "Main.h"
#include "Layers.h"
#include "Render.h"
#include "SaveLoad.h"
#include "SurfaceDial.h"
#include "Tools.h"
#include "ToolsAux.h"
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

void __stdcall Resize() {
    if (!g_pSwapChain || !pRenderTarget) return;

    // Release current target
    pRenderTarget->SetTarget(nullptr);
    pD2DTargetBitmap.Reset();  // Assuming pD2DTargetBitmap is your ComPtr<ID2D1Bitmap1>

    // Resize swap chain
    HRESULT hr = g_pSwapChain->ResizeBuffers(0, width * zoomFactor, height * zoomFactor, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
    if (FAILED(hr)) {
        // Log error (use HCreateLogData)
        return;
    }

    // Get new backbuffer
    Microsoft::WRL::ComPtr<IDXGISurface> backBuffer;
    hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) return;

    // Get current DPI
    FLOAT dpiX, dpiY;
    pRenderTarget->GetDpi(&dpiX, &dpiY);

    // Create new bitmap from surface
    D2D1_BITMAP_PROPERTIES1 bitmapProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        dpiX, dpiY
    );
    hr = pRenderTarget->CreateBitmapFromDxgiSurface(backBuffer.Get(), &bitmapProps, pD2DTargetBitmap.GetAddressOf());
    if (FAILED(hr)) return;

    // Set new target
    pRenderTarget->SetTarget(pD2DTargetBitmap.Get());

    // Update any transforms/DPI if needed
    pRenderTarget->SetDpi(dpiX, dpiY);
}

HRESULT Initialize(HWND pmainHWND, HWND pdocHWND, int pWidth, int pHeight, int pPixelSizeRatio) {
    return TInitialize(pmainHWND, pdocHWND, pWidth, pHeight, pPixelSizeRatio);
}

HRESULT InitializeDocument(HWND hWnd, int pWidth, int pHeight, int pPixelSizeRatio) {
    return TInitializeDocument(hWnd, pWidth, pHeight, pPixelSizeRatio);
}

HRESULT InitializeLayerRenderPreview() {
    return TInitializeLayerRenderPreview();
}

HRESULT InitializeLayers(HWND hWnd) {
    return TInitializeLayers(hWnd);
}

/* HELPERS */

void CreateLogData(std::string fileName, std::string content) {
    HCreateLogData(fileName, content);
}

void __stdcall SaveProjectDll(const char* pathAnsi) {
    int wlen = MultiByteToWideChar(CP_ACP, 0, pathAnsi, -1, nullptr, 0);
    std::wstring wpath(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, pathAnsi, -1, &wpath[0], wlen);

    SaveBinaryProject(wpath);
}

void __stdcall LoadProjectDll(LPCSTR apath, HWND hWndLayer, HINSTANCE hLayerInstance, int btnWidth, int btnHeight, HWND* hLayerButtons, int layerID, const wchar_t* szButtonClass, const wchar_t* msgText) {
    std::wstring wpath;
    int size_needed = MultiByteToWideChar(CP_ACP, 0, apath, -1, nullptr, 0);
    if (size_needed > 0) {
        wpath.resize(size_needed);
        MultiByteToWideChar(CP_ACP, 0, apath, -1, &wpath[0], size_needed);
        if (!wpath.empty() && wpath.back() == L'\0') {
            wpath.pop_back(); // Remove extra null terminator
        }
    }

    // Print parameters
    std::cout << "\n\n[LoadProjectDll]\n";
    std::cout << "apath: " << (apath ? apath : "(null)") << "\n";
    std::cout << "hWndLayer: " << hWndLayer << "\n";
    std::cout << "hLayerInstance: " << hLayerInstance << "\n";
    std::cout << "btnWidth: " << btnWidth << "\n";
    std::cout << "btnHeight: " << btnHeight << "\n";
    std::cout << "hLayerButtons: " << hLayerButtons << " (value: "
        << (hLayerButtons ? *hLayerButtons : 0) << ")\n";
    std::cout << "layerID: " << layerID << " (value: "
        << (layerID ? layerID : 0) << ")\n";

    std::wcout << L"szButtonClass: "
        << (szButtonClass ? szButtonClass : L"(null)") << L"\n";

    std::wcout << L"msgText: "
        << (msgText ? msgText : L"(null)") << L"\n";
    std::wcout << L"Converted wpath: " << wpath << L"\n\n";

    LoadBinaryProject(wpath, hWndLayer, hLayerInstance, btnWidth, btnHeight, hLayerButtons, layerID, L"Button", msgText);
}

void __stdcall LoadProjectDllW(LPWSTR wpath, HWND hWndLayer, HINSTANCE hLayerInstance, int btnWidth, int btnHeight, HWND* hLayerButtons, int layerID, const wchar_t* szButtonClass, const wchar_t* msgText) {

    std::wstring widePath;
    if (wpath) {
        // Safely compute length (up to a max, e.g., MAX_PATH or 1024)
        size_t maxLen = MAX_PATH;
        size_t len = 0;
        while (len < maxLen && wpath[len] != L'\0') {
            len++;
        }
        widePath.assign(wpath, len); // Copy exactly 'len' characters
    }

    // Print parameters
    std::wcout << L"\n\n[LoadProjectDllW]\n";
    std::wcout << L"wpath: " << (wpath ? wpath : L"(null)") << L"\n";
    std::cout << "hWndLayer: " << hWndLayer << "\n";
    std::cout << "hLayerInstance: " << hLayerInstance << "\n";
    std::cout << "btnWidth: " << btnWidth << "\n";
    std::cout << "btnHeight: " << btnHeight << "\n";
    std::cout << "hLayerButtons: " << hLayerButtons << " (value: "
        << (hLayerButtons ? *hLayerButtons : 0) << ")\n";
    std::cout << "layerID: " << layerID << " (value: "
        << (layerID ? layerID : 0) << ")\n";

    std::wcout << L"szButtonClass: "
        << (szButtonClass ? szButtonClass : L"(null)") << L"\n";
    std::wcout << L"msgText: "
        << (msgText ? msgText : L"(null)") << L"\n\n";

    LoadBinaryProject(widePath, hWndLayer, hLayerInstance, btnWidth, btnHeight, hLayerButtons, layerID, L"Button", msgText);
}

/* ACTIONS */

void handleMouseUp() {
    THandleMouseUp();
}

void Undo() {
    TUndo();
}

void Redo() {
    TRedo();
}

/* TRANSFORM */

float __stdcall GetZoomFactor() {
    return TGetZoomFactor();
}

void ZoomIn_Default() {
    TZoomIn(0.5f);
}

void ZoomOut_Default() {
    TZoomOut(0.5f);
}

void ZoomIn(float zoomRatio) {
    TZoomIn(zoomRatio);
}

void ZoomOut(float zoomRatio) {
    TZoomOut(zoomRatio);
}

void Zoom() {
    TZoom();
}

void IncreaseBrushSize_Default() {
    TIncreaseBrushSize(0.5f);
}
void DecreaseBrushSize_Default() {
    TDecreaseBrushSize(0.5f);
}

void IncreaseBrushSize(float sizeRatio) {
    TIncreaseBrushSize(sizeRatio);
}
void DecreaseBrushSize(float sizeRatio) {
    TDecreaseBrushSize(sizeRatio);
}

/* LAYERS */

HRESULT AddLayer(bool fromFile=false, int currentLayer = -1) {
    std::cout << "\n\nADD LAYER ASM\n\n" << currentLayer << "\n\n";
    return TAddLayer(fromFile, currentLayer);
}

int LayersCount() {
    return TLayersCount();
}

void AddLayerButton(HWND layerButton) {
    TAddLayerButton(layerButton);
}

void RemoveLayerButton() {
    TRemoveLayerButton();
}

HRESULT RemoveLayer() {
    return TRemoveLayer();
}

int GetLayer() {
    return TGetLayer();
}

void SetLayer(int index) {
    TSetLayer(index);
}

int GetActiveLayersCount() {
    return HGetActiveLayersCount();
}

void ReorderLayerUp() {
    TReorderLayerUp();
}

void ReorderLayerDown() {
    TReorderLayerDown();
}

void RenderActionToTarget(const ACTION& action) {
    TRenderActionToTarget(action);
}

void UpdateLayers(int layerIndexTarget = -1) {
    TUpdateLayers(layerIndexTarget);
}

void RenderLayers() {
    TRenderLayers();
}

void DrawLayerPreview(int currentLayer) {
    std::cout << "ASSEMBLY LAYER PREVIEW - " << currentLayer;
    TDrawLayerPreview(currentLayer);
}

/* TOOLS */

void SetSelectedTool(int pselectedTool) {
    HSetSelectedTool(pselectedTool);
}

void __stdcall SetFont() {

    CHOOSEFONT cf = { 0 };
    LOGFONT lf = { 0 };

    cf.lStructSize = sizeof(CHOOSEFONT);
    cf.hwndOwner = docHWND;
    cf.lpLogFont = &lf;
    cf.Flags = CF_SCREENFONTS | CF_EFFECTS | CF_INITTOLOGFONTSTRUCT;

    if (ChooseFont(&cf))
    {
        fontFace = lf.lfFaceName;
        fontWeight = lf.lfWeight;
        fontItalic = lf.lfItalic;
        fontStrike = lf.lfStrikeOut;
        fontUnderline = lf.lfUnderline;
        fontSize = cf.iPointSize;
        fontColor = cf.rgbColors;
    }
}

void EraserTool(int left, int top) {
    TEraserTool(left, top);
}

void BrushTool(int left, int top, COLORREF hexColor, bool pixelMode, int pPixelSizeRatio) {
    TBrushTool(left, top, hexColor, pixelMode, pPixelSizeRatio);
}

void RectangleTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    TRectangleTool(left, top, right, bottom, hexColor);
}

void EllipseTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    TEllipseTool(left, top, right, bottom, hexColor);
}

void LineTool(int xInitial, int yInitial, int x, int y, unsigned int hexColor) {
    TLineTool(xInitial, yInitial, x, y, hexColor);
}

void PaintBucketTool(int mouseX, int mouseY, COLORREF fillColor, HWND hWnd) {
    TPaintBucketTool(mouseX, mouseY, fillColor, hWnd);
}

void WriteTool(int left, int top, int right, int bottom) {
    TWriteTool(left, top, right, bottom);
}

void __stdcall SelectTool(int xInitial, int yInitial) {
    TSelectTool(xInitial, yInitial);
}

void __stdcall MoveTool(int xInitial, int yInitial, int x, int y) {
    TMoveTool(xInitial, yInitial, x, y);
}

void __stdcall UnSelectTool() {
    TUnSelectTool();
}

void Cleanup() {
    HCleanup();
}