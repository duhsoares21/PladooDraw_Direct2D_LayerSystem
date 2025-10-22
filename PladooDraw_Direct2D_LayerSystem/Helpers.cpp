#include "pch.h"
#include "Constants.h"
#include "Helpers.h"
#include "Tools.h"
#include "Layers.h"
#include "SurfaceDial.h"
#include "Replay.h"
#include "ToolsAux.h"
#include "Animation.h"

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

int HGetMaxFrameIndex() {
    int maxFrameIndex = 0;

    for (const auto& action : Actions) {
        if (action.FrameIndex > maxFrameIndex)
            maxFrameIndex = action.FrameIndex;
    }

    return maxFrameIndex;
}

/*RENDER*/

void HRenderAction(const ACTION& action, Microsoft::WRL::ComPtr<ID2D1DeviceContext> deviceContext, D2D1_COLOR_F customColor = COLOR_UNDEFINED) {

    D2D1_COLOR_F color = HGetRGBColor(action.Color);

    if (customColor.r != COLOR_UNDEFINED.r ||
        customColor.g != COLOR_UNDEFINED.g ||
        customColor.b != COLOR_UNDEFINED.b ||
        customColor.a != COLOR_UNDEFINED.a){
        color = customColor;

        std::cout << "CUSTOM COLOR" << "\n";
    }

    if (action.Tool == TPaintBucket) {
        color = HGetRGBColor(action.FillColor);
    }

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> replayBrush;

    if (replayBrush == nullptr) {
        deviceContext->CreateSolidColorBrush(color, &replayBrush);
    }
    else {
        replayBrush->SetColor(color);
    }

    if (deviceContext == pRenderTarget) {
        if (pBrush == nullptr) {
            pRenderTarget->CreateSolidColorBrush(color, &replayBrush);
        }
        else {
            pBrush->SetColor(color);
        }

        replayBrush = pBrush;
    }

    switch (action.Tool) {
    case TEraser:
        deviceContext->PushAxisAlignedClip(action.Position, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        deviceContext->Clear(D2D1::ColorF(1, 1, 1, 0));
        deviceContext->PopAxisAlignedClip();
        break;

    case TRectangle: {
        deviceContext->PushAxisAlignedClip(action.Position, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        deviceContext->FillRectangle(action.Position, replayBrush.Get());
        deviceContext->PopAxisAlignedClip();
        break;
    }

    case TEllipse: {
        deviceContext->FillEllipse(action.Ellipse, replayBrush.Get());
        break;
    }

    case TLine: {
        deviceContext->DrawLine(action.Line.startPoint, action.Line.endPoint, replayBrush.Get(), action.BrushSize, nullptr);
        break;
    }

    case TBrush: {
        for (const auto& vertex : action.FreeForm.vertices) {
            if (action.isPixelMode) {
                int snappedLeft = static_cast<int>(vertex.x / pixelSizeRatio) * pixelSizeRatio;
                int snappedTop = static_cast<int>(vertex.y / pixelSizeRatio) * pixelSizeRatio;

                D2D1_RECT_F pixel = D2D1::RectF(
                    static_cast<float>(snappedLeft),
                    static_cast<float>(snappedTop),
                    static_cast<float>(snappedLeft + pixelSizeRatio),
                    static_cast<float>(snappedTop + pixelSizeRatio)
                );

                deviceContext->FillRectangle(pixel, replayBrush.Get());
            }
            else {

                float scaledLeft = static_cast<float>(vertex.x);
                float scaledTop = static_cast<float>(vertex.y);
                float scaledBrushSize = static_cast<float>(currentBrushSize);
                float scaledPixelSizeRatio = static_cast<float>(pixelSizeRatio);

                D2D1_RECT_F rect = D2D1::RectF(
                    scaledLeft - scaledBrushSize * 0.5f,
                    scaledTop - scaledBrushSize * 0.5f,
                    scaledLeft + scaledBrushSize * 0.5f,
                    scaledTop + scaledBrushSize * 0.5f
                );

                deviceContext->DrawRectangle(rect, replayBrush.Get());
                deviceContext->FillRectangle(rect, replayBrush.Get());
            }
        }
        break;
    }

    case TPaintBucket: {
        for (const auto& p : action.pixelsToFill) {
            D2D1_RECT_F rect = D2D1::RectF((float)p.x, (float)p.y, (float)(p.x + 1), (float)(p.y + 1));
            deviceContext->FillRectangle(&rect, replayBrush.Get());
        }
        break;
    }

    case TWrite: {
        if (action.Text.empty()) break;

        Microsoft::WRL::ComPtr<IDWriteTextFormat> localTextFormat;

        pDWriteFactory->CreateTextFormat(
            action.FontFamily.c_str(),
            nullptr,
            static_cast<DWRITE_FONT_WEIGHT>(action.FontWeight),
            action.FontItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            static_cast<FLOAT>(action.FontSize) / 10.0f,   // if stored in tenths
            L"en-us",
            &localTextFormat
        );

        // Apply underline/strikethrough with a TextLayout if needed
        Microsoft::WRL::ComPtr<IDWriteTextLayout> textLayout;
        pDWriteFactory->CreateTextLayout(
            action.Text.c_str(),
            static_cast<UINT32>(action.Text.size()),
            localTextFormat.Get(),
            action.Position.right - action.Position.left,
            action.Position.bottom - action.Position.top,
            &textLayout
        );

        if (action.FontUnderline)
            textLayout->SetUnderline(TRUE, DWRITE_TEXT_RANGE{ 0, (UINT32)action.Text.size() });
        if (action.FontStrike)
            textLayout->SetStrikethrough(TRUE, DWRITE_TEXT_RANGE{ 0, (UINT32)action.Text.size() });

        // Draw it
        deviceContext->DrawTextLayout(
            D2D1::Point2F(action.Position.left, action.Position.top),
            textLayout.Get(),
            replayBrush.Get()
        );
        break;
    }

    default:
        break;
    }
}

RenderData HCreateRenderData(D2D1_SIZE_U size) {
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> deviceContext;
    HRESULT hr = g_pD2DDevice->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        deviceContext.GetAddressOf()
    );

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap;
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );

    deviceContext->CreateBitmap(size, nullptr, 0, &bitmapProperties, &targetBitmap);

    RenderData renderData = RenderData{
        size,
        deviceContext,
        nullptr,
        targetBitmap
    };

    return renderData;
}

RenderData HCreateRenderDataHWND(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    Microsoft::WRL::ComPtr<ID2D1DeviceContext> deviceContext;
    HRESULT hr = g_pD2DDevice->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        deviceContext.GetAddressOf()
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
    swapDesc.SampleDesc.Quality = 0;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = 2;
    swapDesc.Scaling = DXGI_SCALING_STRETCH;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapDesc.Flags = 0;
   

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    hr = dxgiFactory->CreateSwapChainForHwnd(
        g_pD3DDevice.Get(),
        hWnd,
        &swapDesc,
        nullptr, nullptr,
        &swapChain
    );

    Microsoft::WRL::ComPtr<IDXGISurface> backBuffer;
    swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap;
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)
    );

    deviceContext->CreateBitmapFromDxgiSurface(
        backBuffer.Get(),
        &bitmapProperties,
        &targetBitmap
    );

    RenderData renderData = RenderData{
        size,
        deviceContext,
        swapChain,
        targetBitmap
    };
    
    return renderData;
}

void CreateGlobalRenderBitmap() {
    ComPtr<IDXGISurface> backBuffer;
    HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(IDXGISurface), &backBuffer);

    ComPtr<ID3D11Resource> backBufferResource;
    // Converte o IDXGISurface para um recurso D3D11 (ID3D11Resource ou ID3D11Texture2D)
    hr = backBuffer.As(&backBufferResource);
    if (FAILED(hr)) return;

    // Cria a visão que permite ao D3D11 limpar/renderizar no back buffer
    // Usa ReleaseAndGetAddressOf() para liberar a visão anterior (se houver) e armazenar a nova
    hr = g_pD3DDevice->CreateRenderTargetView(
        backBufferResource.Get(),
        nullptr, // Usa as configurações padrão
        g_pRenderTargetView.ReleaseAndGetAddressOf() // Armazena a nova visão
    );
    if (FAILED(hr)) return;

    FLOAT dpiX, dpiY;
    pRenderTarget->GetDpi(&dpiX, &dpiY);

    D2D1_BITMAP_PROPERTIES1 bitmapProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpiX, dpiY
    );

    hr = pRenderTarget->CreateBitmapFromDxgiSurface(backBuffer.Get(), &bitmapProps, pD2DTargetBitmap.ReleaseAndGetAddressOf());
}

/*TIMELINE*/

HWND HCreateTimelineFrameButton(int FrameIndex) {
    RECT rcParent;
    GetClientRect(timelineHWND, &rcParent);

    int timelineParentWidth = rcParent.right - rcParent.left;

    HINSTANCE hTimelineInstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtr(timelineHWND, GWLP_HINSTANCE));

    DWORD style = WS_CHILD | WS_VISIBLE | BS_BITMAP;

    HWND timelineFrame = CreateWindowEx(
        0,
        L"Button",
        L"",
        style,
        (timelineParentWidth / 2) - (btnWidth / 2) + (btnWidth * FrameIndex),
        10,
        btnWidth,
        btnHeight,
        timelineHWND,
        (HMENU)(intptr_t)FrameIndex,
        hTimelineInstance,
        NULL
    );
    
    return timelineFrame;
}
void HCreateHighlightFrame() {
    HINSTANCE hReplayInstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtr(timelineHWND, GWLP_HINSTANCE));

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
        timelineHWND,
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
    swapDesc.SampleDesc.Quality = 0;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = 2;
    swapDesc.Scaling = DXGI_SCALING_STRETCH;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapDesc.Flags = 0;

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
void HSetTimelineHighlight() {

    RECT rc;
    GetClientRect(timelineHWND, &rc);

    int timelineParentWidth = rc.right - rc.left;

    SetWindowPos(highlightFrame, HWND_BOTTOM, (timelineParentWidth / 2) - (btnWidth / 2), 0, btnWidth, 10, 0);
}

/*REPLAY MODE*/
void HSetReplayMode(int pReplayMode) {
    isReplayMode = pReplayMode;

    if (isReplayMode == 1) {

        lastActiveReplayFrame = 0;
        g_scrollOffsetTimeline = 0;

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

        for (int i = 0; i < TimelineFrameButtons.size(); i++) {
            DestroyWindow(TimelineFrameButtons[i].value().frame);
            TimelineFrameButtons[i].reset();
        }

        TimelineFrameButtons.clear();

        int totalFrames = static_cast<int>(RedoActions.size()) + 1;  // N+1 frames (0 vazio)

        for (int i = 0; i < totalFrames; i++) {
            TCreateReplayFrame(i);
        }
        
        HCreateHighlightFrame();
        HSetTimelineHighlight();
        TReplayClearLayers();
        TReplayRender();
    }
    else {
        Actions.clear();
        RedoActions.clear();

        Actions = ReplayActions;
        RedoActions = ReplayRedoActions;
    }

    CleanupSurfaceDial();
    TInitializeSurfaceDial(mainHWND);
}

/*ANIMATION MODE*/
void HSetAnimationMode(int pAnimationMode) {
    isAnimationMode = pAnimationMode;

    if (isAnimationMode == 1) {
        HCreateHighlightFrame();
        HSetTimelineHighlight();
        TRenderAnimation();
    }

    CleanupSurfaceDial();
    TInitializeSurfaceDial(mainHWND);
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
    g_pRenderTargetView.Reset();

    for (auto& layer : layers) {
        if (layer.has_value()) {
            layer.value().pBitmap.Reset();
        }
    }

    for (auto& layer : layerBitmaps) {
        layer.pBitmap.Reset();
    }

    for (auto& layerbuttons : LayerButtons) {
        if (layerbuttons.has_value()) {
            layerbuttons.reset();
        }
    }

    for (auto& timelineFrameButton : TimelineFrameButtons) {
        if (timelineFrameButton.has_value()) {
            timelineFrameButton.reset();
        }
    }

    layers.clear();
    layersOrder.clear();
    Actions.clear();
    RedoActions.clear();
    LayerButtons.clear();
    TimelineFrameButtons.clear();

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

    int itemWidth = btnWidth;
    int itemHeight = btnHeight;
    int contentHeight = ((int)result.size() - 1) * itemHeight;

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
            x, y, itemWidth, itemHeight, TRUE);
    }
}

void HOnScrollWheelTimeline(int wParam) {
    int direction = (wParam > 0) ? -1 : 1;

    int delta = btnWidth;
   
    int itemWidth = btnWidth;
    int itemHeight = btnHeight;

    g_scrollOffsetTimeline += direction * delta;

    RECT rcParent;
    GetClientRect(timelineHWND, &rcParent);

    int timelineParentWidth = rcParent.right - rcParent.left;

    for (size_t i = 0; i < TimelineFrameButtons.size(); i++) {
        int currentFrameIndex = TimelineFrameButtons[i].value().FrameIndex;

        int x = ((timelineParentWidth / 2) - (itemWidth / 2) + (itemWidth * i)) - g_scrollOffsetTimeline;
        int y = 10;

        MoveWindow(TimelineFrameButtons[currentFrameIndex].value().frame,
            x, y, itemWidth, itemHeight, TRUE);
    }
}

void HSetScrollPosition(int index) {
    
    int delta = btnWidth;

    int itemWidth = btnWidth;
    int itemHeight = btnHeight;

    g_scrollOffsetTimeline = index * delta;

    RECT rcParent;
    GetClientRect(timelineHWND, &rcParent);

    int timelineParentWidth = rcParent.right - rcParent.left;

    for (size_t i = 0; i < TimelineFrameButtons.size(); i++) {
        int currentFrameIndex = TimelineFrameButtons[i].value().FrameIndex;

        int x = ((timelineParentWidth / 2) - (itemWidth / 2) + (itemWidth * i)) - g_scrollOffsetTimeline;
        int y = 10;

        MoveWindow(TimelineFrameButtons[currentFrameIndex].value().frame,
            x, y, itemWidth, itemHeight, TRUE);
    }
}

template <class T> void SafeRelease(T** ppT)  
{  
    if (ppT && *ppT)  
    {  
        (*ppT)->Release();  
        *ppT = nullptr;  
    }  
}