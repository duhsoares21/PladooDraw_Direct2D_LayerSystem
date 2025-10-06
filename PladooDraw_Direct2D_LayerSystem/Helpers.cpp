#include "pch.h"
#include "Constants.h"
#include "Helpers.h"
#include "Tools.h"

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

    for (auto& layerItems : LayerItems) {
        if (layerItems.has_value()) {
            layerItems.reset();
        }
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

template <class T> void SafeRelease(T** ppT)  
{  
    if (ppT && *ppT)  
    {  
        (*ppT)->Release();  
        *ppT = nullptr;  
    }  
}