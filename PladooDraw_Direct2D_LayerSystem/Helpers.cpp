#include "pch.h"
#include "Constants.h"
#include "Helpers.h"

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

template <class T> void SafeRelease(T** ppT)  
{  
    if (ppT && *ppT)  
    {  
        (*ppT)->Release();  
        *ppT = nullptr;  
    }  
}

void HPrintHResultError(HRESULT hr) {
    _com_error err(hr);
    LPCTSTR errMsg = err.ErrorMessage();
    std::wcout << "Erro HRESULT: 0x" << std::hex << hr << " - " << errMsg << std::endl;
}

void HCleanup() {
    SafeRelease(pBrush.GetAddressOf());
    SafeRelease(&pRenderTarget);
    SafeRelease(hWndLayerRenderTarget.GetAddressOf());
    SafeRelease(&pRenderTargetLayer);
    SafeRelease(pD2DFactory.GetAddressOf());
    SafeRelease(pD2DTargetBitmap.GetAddressOf());
    SafeRelease(pDWriteFactory.GetAddressOf());

    for (auto& layer : layers) {
        layer.pBitmap.Reset();
    }

    layers.clear();
    layersOrder.clear();
    Actions.clear();
    RedoActions.clear();
    LayerButtons.clear();

    // Release D3D resources
    g_pSwapChain.Reset();
    g_pD2DDevice.Reset();
    g_pD3DContext.Reset();
}
