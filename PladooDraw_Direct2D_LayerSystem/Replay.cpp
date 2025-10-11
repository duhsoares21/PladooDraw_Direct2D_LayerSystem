#include "pch.h"
#include "Replay.h"
#include "Layers.h"
#include "Helpers.h"
#include "Tools.h"

void TCreateReplayFrame(int FrameIndex) {

    RECT rcParent;
    GetClientRect(replayHWND, &rcParent);

    int replayParentWidth = rcParent.right - rcParent.left;

    HINSTANCE hReplayInstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtr(replayHWND, GWLP_HINSTANCE));

    DWORD style = WS_CHILD | WS_VISIBLE | BS_BITMAP;

    HWND replayFrame = CreateWindowEx(
        0,
        L"Button",
        L"",
        style,
        (replayParentWidth / 2) - (btnWidth / 2) + (btnWidth * FrameIndex),
        10,
        btnWidth,
        btnHeight,
        replayHWND,
        (HMENU)(intptr_t)FrameIndex,
        hReplayInstance,
        NULL
    );

    RECT rc;
    GetClientRect(replayFrame, &rc);
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
        replayFrame,
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
    
    ReplayFrameButtons.push_back(ReplayFrameButton{
        FrameIndex,
        replayFrame,
        replayDeviceContext,
        replaySwapChain
    });

    replayDeviceContext->BeginDraw(); 
    replayDeviceContext->Clear(D2D1::ColorF(D2D1::ColorF::White, 1.0f));

    float canvasW = 512.0f;
    float canvasH = 512.0f;
    float thumbW = size.width;
    float thumbH = size.height;

    float scale = min(thumbW / canvasW, thumbH / canvasH);

    replayDeviceContext->SetTransform(
        D2D1::Matrix3x2F::Scale(scale, scale)
    );

    std::vector<ACTION> FilteredRedoActions;

    for (int i = 0; i < RedoActions.size(); i++) {
        if (RedoActions[i].Tool == TLayer) continue;
        FilteredRedoActions.push_back(RedoActions[i]);
    }

    int total = static_cast<int>(FilteredRedoActions.size());

    for (int i = 0; i < FrameIndex && i < total; i++) {
        int idx = total - 1 - i; // lê de trás pra frente
        TRenderReplayAction(FilteredRedoActions[idx], replayDeviceContext);
    }
        
    replayDeviceContext->EndDraw();

    replaySwapChain->Present(1, 0);
}

void TRenderReplayAction(const ACTION& action, Microsoft::WRL::ComPtr<ID2D1DeviceContext> replayDeviceContext) {

    D2D1_COLOR_F color = HGetRGBColor(action.Color);
    if (action.Tool == TPaintBucket) {
        color = HGetRGBColor(action.FillColor);
    }

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> replayBrush;

    if (replayBrush == nullptr) {
        replayDeviceContext->CreateSolidColorBrush(color, &replayBrush);
    }
    else {
        replayBrush->SetColor(color);
    }

    if (replayDeviceContext == pRenderTarget) {
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
        replayDeviceContext->PushAxisAlignedClip(action.Position, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        replayDeviceContext->Clear(D2D1::ColorF(0, 0, 0, 0));
        replayDeviceContext->PopAxisAlignedClip();
        break;

    case TRectangle: {
        replayDeviceContext->PushAxisAlignedClip(action.Position, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        replayDeviceContext->FillRectangle(action.Position, replayBrush.Get());
        replayDeviceContext->PopAxisAlignedClip();
        break;
    }

    case TEllipse: {
        replayDeviceContext->FillEllipse(action.Ellipse, replayBrush.Get());
        break;
    }

    case TLine: {
        replayDeviceContext->DrawLine(action.Line.startPoint, action.Line.endPoint, replayBrush.Get(), action.BrushSize, nullptr);
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

                replayDeviceContext->FillRectangle(pixel, replayBrush.Get());
            }
            else {

                float scaledLeft = static_cast<float>(vertex.x) / zoomFactor;
                float scaledTop = static_cast<float>(vertex.y) / zoomFactor;
                float scaledBrushSize = static_cast<float>(currentBrushSize) / zoomFactor;
                float scaledPixelSizeRatio = static_cast<float>(pixelSizeRatio) / zoomFactor;

                D2D1_RECT_F rect = D2D1::RectF(
                    scaledLeft - scaledBrushSize * 0.5f,
                    scaledTop - scaledBrushSize * 0.5f,
                    scaledLeft + scaledBrushSize * 0.5f,
                    scaledTop + scaledBrushSize * 0.5f
                );

                replayDeviceContext->DrawRectangle(rect, replayBrush.Get());
                replayDeviceContext->FillRectangle(rect, replayBrush.Get());
            }
        }
        break;
    }

    case TPaintBucket: {
        for (const auto& p : action.pixelsToFill) {
            D2D1_RECT_F rect = D2D1::RectF((float)p.x * zoomFactor, (float)p.y * zoomFactor, (float)(p.x * zoomFactor + 1), (float)(p.y * zoomFactor + 1));
            replayDeviceContext->FillRectangle(&rect, replayBrush.Get());
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
        replayDeviceContext->DrawTextLayout(
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

int counter = 0;
void TSetReplayHighlight() {

    RECT rc;
    GetClientRect(replayHWND, &rc);

    int replayParentWidth = rc.right - rc.left;
    
    SetWindowPos(highlightFrame, HWND_BOTTOM, (replayParentWidth / 2) - (btnWidth / 2), 0, btnWidth, 10, 0);    
}

void TReplayRender() {
    Microsoft::WRL::ComPtr<IDXGISurface> backBuffer;
    HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(IDXGISurface), &backBuffer);
    if (FAILED(hr)) {
        OutputDebugStringW((L"TRenderLayers: Failed to get backbuffer, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return;
    }

    // Get DPI from pRenderTarget
    FLOAT dpiX, dpiY;
    pRenderTarget->GetDpi(&dpiX, &dpiY);

    // Create bitmap from backbuffer
    D2D1_BITMAP_PROPERTIES1 bitmapProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpiX, dpiY
    );

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap;
    hr = pRenderTarget->CreateBitmapFromDxgiSurface(backBuffer.Get(), &bitmapProps, pD2DTargetBitmap.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        OutputDebugStringW((L"TRenderLayers: Failed to create bitmap from DXGI surface, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return;
    }

    pRenderTarget->SetTarget(pD2DTargetBitmap.Get());

    pRenderTarget->BeginDraw();
    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
    pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White, 1.0f));

    for (int j = 0; j <= Actions.size() && j < Actions.size(); j++) {
        TRenderReplayAction(Actions[j], pRenderTarget);
    }
    
    pRenderTarget->EndDraw();

    g_pSwapChain->Present(0, 0);
}

void TEditFromThisPoint() {

    std::string text = "Essa é uma ação destrutiva. Todas as ações após esse ponto serão excluídas. Tem certeza que quer continuar?";

    LPCSTR message = text.c_str();
    LPCSTR title = "Editar a partir daqui";

    int clicked = MessageBoxA(mainHWND, message, title, MB_YESNO);

    if (clicked == IDYES) {
        int ActionIndex = lastActiveReplayFrame;

        if (ActionIndex < Actions.size()) {
            Actions.erase(Actions.begin() + ActionIndex + 1, Actions.end());
        }

        RedoActions.clear();
    }
    else
    {
        std::string text = "Ação cancelada.";

        LPCSTR message = text.c_str();
        LPCSTR title = "Editar a partir daqui";

        int clicked = MessageBoxA(mainHWND, message, title, MB_OK);
    }
}