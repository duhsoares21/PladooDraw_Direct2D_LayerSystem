#include "pch.h"
#include "Constants.h"
#include "Layers.h"
#include "Tools.h"
#include "Transforms.h"

float TGetZoomFactor() {
    return zoomFactor;
}

void TZoomIn_Default() {
    TZoomIn(0.5f);
}

void TZoomOut_Default() {
    TZoomOut(0.5f);
}

void TZoomIn(float zoomRatio) {
    if (zoomFactor < 3.0f) {
        zoomFactor = (std::min)(zoomFactor + zoomRatio, 3.0f);
        TZoom();
    }
}

void TZoomOut(float zoomRatio) {
    if (zoomFactor > 1.0f) {
        zoomFactor = (std::max)(zoomFactor - zoomRatio, 1.0f);
        TZoom();
    }
}

void TZoom() {
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

    TRenderLayers();
}

void TIncreaseBrushSize_Default() {
    TIncreaseBrushSize(0.5f);
}

void TDecreaseBrushSize_Default() {
    TDecreaseBrushSize(0.5f);
}

void TIncreaseBrushSize(float sizeIncrement) {
    if (selectedTool == TBrush) {
        currentBrushSize = (std::min)(currentBrushSize + sizeIncrement, 100.0f);
    }
    else if (selectedTool == TEraser) {
        currentEraserSize = (std::min)(currentEraserSize + sizeIncrement, 100.0f);
    }
}

void TDecreaseBrushSize(float sizeIncrement) {
    if (selectedTool == TBrush) {
        currentBrushSize = (std::max)(currentBrushSize - sizeIncrement, defaultBrushSize);
    }
    else if (selectedTool == TEraser) {
        currentEraserSize = (std::max)(currentEraserSize - sizeIncrement, defaultEraserSize);
    }
}