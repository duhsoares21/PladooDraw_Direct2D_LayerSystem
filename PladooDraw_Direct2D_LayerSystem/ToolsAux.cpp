#include "pch.h"
#include "Base.h"
#include "Constants.h"
#include "Helpers.h"
#include "Layers.h"
#include "Tools.h"

/* TOOLS AUX */

void THandleMouseUp() {

    if (isDrawingRectangle || isDrawingEllipse || isDrawingLine) {

        ComPtr<ID2D1SolidColorBrush> brush;
        pRenderTarget->CreateSolidColorBrush(HGetRGBColor(currentColor), &brush);

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
        TUnSelectTool();
    }

    TRenderLayers();
    TDrawLayerPreview(layerIndex);
}

void TUndo() {
    if (Actions.size() > 0) {
        ACTION lastAction = Actions.back();
        RedoActions.push_back(lastAction);
        Actions.pop_back();

        TUpdateLayers(layerIndex);
        TRenderLayers();
        TDrawLayerPreview(layerIndex);
    }
}

void TRedo() {
    if (RedoActions.size() > 0) {
        ACTION action = RedoActions.back();
        Actions.push_back(action);
        RedoActions.pop_back();

        TUpdateLayers(layerIndex);
        TRenderLayers();
        TDrawLayerPreview(layerIndex);
    }
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

bool IsPointNearEdge(const std::vector<VERTICE> vertices, float px, float py) {
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

/*FLOODFILL AUX*/

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