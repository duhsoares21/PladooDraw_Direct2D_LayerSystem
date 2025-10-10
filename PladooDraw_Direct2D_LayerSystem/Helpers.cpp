#include "pch.h"
#include "Constants.h"
#include "Helpers.h"
#include "Tools.h"
#include "Layers.h"
#include "SurfaceDial.h"
#include "Replay.h"
#include "ToolsAux.h"

int g_scrollOffset = 0;

int HGetActiveLayersCount() {
    size_t count = std::count_if(layers.begin(), layers.end(), [](auto& l) { return l.has_value(); });
    size_t countHidden = 0;

    for (size_t i = 0; i < layers.size(); i++)
    {
        if (layers[i].has_value()) {
            if (!layers[i].value().isActive) {
                countHidden++;
            }
       }
    }

    count = count > countHidden ? count - countHidden : countHidden - count;

    return count;
}

void HSetReplayMode(int pReplayMode) {
    isReplayMode = pReplayMode;

    if (isReplayMode == 1) {

        ReplayActions = Actions;
        ReplayRedoActions = RedoActions;

        std::vector<ACTION> filteredActions;

        for (int i = 0; i < Actions.size(); i++) {
            if (Actions[i].Tool == TLayer) continue;
            filteredActions.push_back(Actions[i]);
        }

        std::reverse(filteredActions.begin(), filteredActions.end());

        RedoActions = filteredActions;
        Actions.clear();

        if (ReplayFrameButtons.size() > 0) {
            
            if (ReplayFrameButtons.size() == RedoActions.size()) {
                for (int i = 0; i < lastActiveReplayFrame; i++) {
                    TReplayForward();
                }
            }
            else {
                for (int i = 0; i < ReplayFrameButtons.size(); i++) {
                    DestroyWindow(ReplayFrameButtons[i].value().frame);
                    ReplayFrameButtons[i].reset();
                }

                ReplayFrameButtons.clear();

                int totalFrames = static_cast<int>(RedoActions.size()) + 1;  // N+1 frames (0 vazio)

                for (int i = 0; i < totalFrames; i++) {
                    TCreateReplayFrame(i);
                }
            }

        }
        else {
            int totalFrames = static_cast<int>(RedoActions.size()) + 1;  // N+1 frames (0 vazio)

            for (int i = 0; i < totalFrames; i++) {
                TCreateReplayFrame(i);
            }
        }
        
        HCreateHighlightFrame();
        TSetReplayHighlight();
        TReplayRender();
    }
    else {
        Actions.clear();
        RedoActions.clear();

        Actions = ReplayActions;
        RedoActions = ReplayRedoActions;
    }

    CleanupSurfaceDial();
    InitializeSurfaceDial(mainHWND);
}

void HCreateHighlightFrame() {
    HINSTANCE hReplayInstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtr(replayHWND, GWLP_HINSTANCE));

    DWORD style = WS_CHILD | WS_VISIBLE | BS_BITMAP;

    highlightFrame = CreateWindowEx(
        0,
        L"Button",
        L"",
        style,
        btnWidth * 0,
        0,
        btnWidth,
        10,
        replayHWND,
        (HMENU)(intptr_t)-1,
        hReplayInstance,
        NULL
    );

    RECT rc;
    GetClientRect(highlightFrame, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    Microsoft::WRL::ComPtr<ID2D1DeviceContext> replayDeviceContext;
    HRESULT hr = g_pD2DDevice->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        replayDeviceContext.GetAddressOf()
    );

    Microsoft::WRL::ComPtr<IDXGIDevice1> dxgiDevice;
    hr = g_pD3DDevice.As(&dxgiDevice);

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);

    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory;
    hr = adapter->GetParent(__uuidof(IDXGIFactory2), &dxgiFactory);

    DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
    swapDesc.Width = size.width;
    swapDesc.Height = size.height;
    swapDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = 2;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> replaySwapChain;
    hr = dxgiFactory->CreateSwapChainForHwnd(
        g_pD3DDevice.Get(),
        highlightFrame,
        &swapDesc,
        nullptr, nullptr,
        &replaySwapChain
    );

    Microsoft::WRL::ComPtr<IDXGISurface> backBuffer;
    replaySwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap;
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );

    replayDeviceContext->CreateBitmapFromDxgiSurface(
        backBuffer.Get(),
        &bitmapProperties,
        &targetBitmap
    );

    replayDeviceContext->SetTarget(targetBitmap.Get());

    replayDeviceContext->BeginDraw();
    replayDeviceContext->Clear(D2D1::ColorF(D2D1::ColorF::White, 1));

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> highlightReplayBrush;

    D2D1_COLOR_F color = D2D1::ColorF(0, 0, 1, 1);

    if (highlightReplayBrush == nullptr) {
        replayDeviceContext->CreateSolidColorBrush(color, &highlightReplayBrush);
    }
    else {
        highlightReplayBrush->SetColor(color);
    }

    replayDeviceContext->FillRectangle(D2D1::RectF(rc.left, rc.top, rc.right, rc.bottom), highlightReplayBrush.Get());
    replayDeviceContext->EndDraw();

    replaySwapChain->Present(1, 0);
}

void HSetSelectedTool(int pselectedTool) {
    selectedTool = pselectedTool;
}

void HCreateLogData(std::string fileName, std::string content) {

    std::ofstream outFile(fileName, std::ios::app);

    if (!outFile.is_open()) {
        MessageBox(nullptr, L"Failed to open file for writing", L"Error", MB_OK);
        return;
    }

    outFile << content << std::endl;

    outFile.close();

    return;
}

D2D1_COLOR_F HGetRGBColor(COLORREF color) {
    float r = (color & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = ((color >> 16) & 0xFF) / 255.0f;
    float a = 1.0f;

    D2D1_COLOR_F RGBColor = { r, g, b, a };

    return RGBColor;
}

RECT HScalePointsToButton(int x, int y, int drawingAreaWidth, int drawingAreaHeight, int buttonWidth, int buttonHeight, bool pixelMode, int pPixelSizeRatio) {
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

std::vector<std::string> HSplit(std::string s, std::string delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }

    res.push_back(s.substr(pos_start));
    return res;
}

void HPrintHResultError(HRESULT hr) {
    _com_error err(hr);
    LPCTSTR errMsg = err.ErrorMessage();
    std::wcout << "Erro HRESULT: 0x" << std::hex << hr << " - " << errMsg << std::endl;
}

void HCleanup() {
    pBrush.Reset();
    pRenderTarget.Reset();
    hWndLayerRenderTarget.Reset();
    pRenderTargetLayer.Reset();
    pD2DFactory.Reset();
    pD2DTargetBitmap.Reset();
    pDWriteFactory.Reset();

    for (auto& layer : layers) {
        if (layer.has_value()) {
            layer.value().pBitmap.Reset();
        }
    }

    for (auto& layerbuttons : LayerButtons) {
        if (layerbuttons.has_value()) {
            layerbuttons.reset();
        }
    }

    for (auto& replayFrames : ReplayFrameButtons) {
        if (replayFrames.has_value()) {
            replayFrames.reset();
        }
    }

    layers.clear();
    layersOrder.clear();
    Actions.clear();
    RedoActions.clear();
    LayerButtons.clear();
    ReplayFrameButtons.clear();

    // Release D3D resources
    g_pSwapChain.Reset();
    g_pD2DDevice.Reset();
    g_pD3DContext.Reset();
}

void HOnScrollWheelLayers(int wParam) {
    int direction = (wParam > 0) ? -1 : 1;

    int delta = btnHeight;

    std::vector<LayerOrder> result;
    result.reserve(layers.size());
    for (auto& sortedLayer : layersOrder) {
        if (layers[sortedLayer.layerIndex].has_value() &&
            sortedLayer.layerIndex == layers[sortedLayer.layerIndex].value().LayerID) {
            result.push_back(sortedLayer);
        }
    }

    std::sort(result.begin(), result.end(),
        [](const LayerOrder& a, const LayerOrder& b) {
            return a.layerOrder > b.layerOrder;
        });

    int itemHeight = btnHeight;
    int contentHeight = (int)result.size() * itemHeight;

    RECT parentRc;
    GetClientRect(layersHWND, &parentRc);
    int viewHeight = parentRc.bottom - parentRc.top - itemHeight;

    g_scrollOffset += direction * delta;

    if (g_scrollOffset < 0)
        g_scrollOffset = 0;

    if (g_scrollOffset > contentHeight - viewHeight)
        g_scrollOffset = contentHeight - viewHeight;

    if (contentHeight <= viewHeight)
        g_scrollOffset = 0;

    for (size_t i = 0; i < result.size(); i++) {
        int currentLayerIndex = result[i].layerIndex;

        int x = 0;
        int y = (int)(i * itemHeight) - g_scrollOffset; // posição base - offset

        MoveWindow(LayerButtons[currentLayerIndex].value().button,
            x, y, itemHeight, itemHeight, TRUE);
    }
}

void HOnScrollWheelReplay(int wParam) {
    int direction = (wParam > 0) ? -1 : 1;

    int delta = btnWidth;
   
    int itemWidth = btnWidth;
    int contentWidth = (int)ReplayFrameButtons.size() * itemWidth;

    RECT parentRc;
    GetClientRect(replayHWND, &parentRc);
    int viewWidth = parentRc.right - parentRc.left;

    g_scrollOffset += direction * delta;

    RECT rcParent;
    GetClientRect(replayHWND, &rcParent);

    int replayParentWidth = rcParent.right - rcParent.left;

    for (size_t i = 0; i < ReplayFrameButtons.size(); i++) {
        int currentFrameIndex = ReplayFrameButtons[i].value().FrameIndex;

        int x = ((replayParentWidth / 2) - (itemWidth / 2) + (itemWidth * i)) - g_scrollOffset;
        int y = 10;

        MoveWindow(ReplayFrameButtons[currentFrameIndex].value().frame,
            x, y, itemWidth, itemWidth, TRUE);
    }

    TReplayRender();
}

template <class T> void SafeRelease(T** ppT)  
{  
    if (ppT && *ppT)  
    {  
        (*ppT)->Release();  
        *ppT = nullptr;  
    }  
}