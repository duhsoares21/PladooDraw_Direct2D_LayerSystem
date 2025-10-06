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
#include "SvgExporter.h"

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
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
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

HRESULT InitializeLayersButtons(HWND* buttonsHwnd) {
    return TInitializeLayersButtons(buttonsHwnd);
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

void __stdcall LoadProjectDll(LPCSTR apath) {

    vector<std::string> splitString = HSplit(apath,"\\");
    std::string filePDD = splitString[splitString.size() - 1];

    loadedFileName = HSplit(filePDD, ".")[0];

    std::wstring wpath;
    int size_needed = MultiByteToWideChar(CP_ACP, 0, apath, -1, nullptr, 0);
    if (size_needed > 0) {
        wpath.resize(size_needed);
        MultiByteToWideChar(CP_ACP, 0, apath, -1, &wpath[0], size_needed);
        if (!wpath.empty() && wpath.back() == L'\0') {
            wpath.pop_back(); // Remove extra null terminator
        }
    }

    LoadBinaryProject(wpath);
}

void __stdcall LoadProjectDllW(LPWSTR wpath) {

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

    int bufferSize = WideCharToMultiByte(CP_ACP, 0, wpath, -1, NULL, 0, NULL, NULL);

    // Allocate memory for the converted string
    char* narrowString = new char[bufferSize];

    // Perform the conversion
    WideCharToMultiByte(CP_ACP, 0, wpath, -1, narrowString, bufferSize, NULL, NULL);

    vector<std::string> splitString = HSplit(narrowString, "\\");
    std::string filePDD = splitString[splitString.size() - 1];

    loadedFileName = HSplit(filePDD, ".")[0];

    LoadBinaryProject(widePath);
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
    TZoomIn(0.1f);
}

void ZoomOut_Default() {
    TZoomOut(0.1f);
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

void AddLayer(bool fromFile = false, int currentLayer = -1) {
    TAddLayer(fromFile, currentLayer);
}

int LayersCount() {
    return TLayersCount();
}

void AddLayerButton(int LayerButtonID) {
    TAddLayerButton(LayerButtonID, true);
}

void RemoveLayerButton() {
    TRemoveLayerButton(-1);
}

HRESULT RemoveLayer() {
    return TRemoveLayer(-1);
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

int __stdcall IsLayerActive(int layer, int* isActive) {
    if (layers[layer].has_value()) {
        *isActive = layers[layer].value().isActive ? 1 : 0;
    }
    else {
        *isActive = 0;
    }
    return layers[layer].has_value();
}

void ReorderLayers(int isAddingLayer) {
    TReorderLayers(isAddingLayer && 1);
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
    TDrawLayerPreview(currentLayer);
}

/* TOOLS */

void SetSelectedTool(int pselectedTool) {
    HSetSelectedTool(pselectedTool);
}

void __stdcall ExportSVG() {
    float scaleFactor = 1.0f; // Duplicar o tamanho
    std::string outputFilename;
    
    if (loadedFileName.empty()) {
        srand(time(0));
        int randomNumber = rand();
        loadedFileName = std::to_string(randomNumber);
    }

    TCHAR path[MAX_PATH];

    HRESULT hr = SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, 0, path);

    if (SUCCEEDED(hr)) {
        std::wstring profilePath(path);

        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, profilePath.c_str(), (int)profilePath.size(), NULL, 0, NULL, NULL);
        std::string path(sizeNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, profilePath.c_str(), (int)profilePath.size(), &path[0], sizeNeeded, NULL, NULL);
        
        outputFilename = path + "\\documents\\pladoo-draw\\exports\\" + loadedFileName + ".svg";
    }

    ExportActionsToSvg(Actions, outputFilename, scaleFactor, width, height);
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

void WriteTool(int x, int y) {
    TWriteTool(x, y);
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

void OnScrollWheelLayers(int wParam) {
    HOnScrollWheelLayers(wParam);
}