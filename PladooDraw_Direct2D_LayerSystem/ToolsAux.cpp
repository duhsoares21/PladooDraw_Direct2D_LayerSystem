#include "ToolsAux.h"
#include "Actions.h"
/* TOOLS AUX */

void THandleMouseUp() {
    if (!layers[layerIndex].has_value()) return;

    if (isDrawingRectangle || isDrawingEllipse || isDrawingLine || isDrawingWindowText) {

        ComPtr<ID2D1SolidColorBrush> brush;
        pRenderTarget->CreateSolidColorBrush(HGetRGBColor(currentColor), &brush);

        auto it = std::find_if(
            layers.begin(),
            layers.end(),
            [](const std::optional<Layer>& optLayer) {
                return optLayer.has_value() && optLayer->LayerID == layerIndex && optLayer->FrameIndex == CurrentFrameIndex;
            }
        );

        pRenderTarget->SetTarget(it->value().pBitmap.Get());
        pRenderTarget->BeginDraw();

        ACTION action;

        if (isDrawingRectangle) {
            pRenderTarget->PushAxisAlignedClip(rectangle, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            pRenderTarget->FillRectangle(rectangle, brush.Get());
            pRenderTarget->PopAxisAlignedClip();

            actionId++;

			action.Id = actionId;
            action.Tool = TRectangle;
            action.Layer = layerIndex;
            action.FrameIndex = CurrentFrameIndex;
            action.Position = rectangle;
            action.Color = currentColor;
            action.IsFilled = false;

            rectangle = D2D1::RectF(0, 0, 0, 0);
        }

        if (isDrawingEllipse) {
            pRenderTarget->FillEllipse(ellipse, brush.Get());

            actionId++;

			action.Id = actionId;
            action.Tool = TEllipse;
            action.Layer = layerIndex;
            action.FrameIndex = CurrentFrameIndex;
            action.Position = rectangle;
            action.Ellipse = ellipse;
            action.Color = currentColor;
            action.IsFilled = false;

            ellipse = D2D1::Ellipse(D2D1::Point2F(0, 0), 0, 0);
        }

        if (isDrawingLine) {
            pRenderTarget->DrawLine(startPoint, endPoint, brush.Get(), currentBrushSize, nullptr);

            actionId++;

			action.Id = actionId;
            action.Tool = TLine;
            action.Layer = layerIndex;
            action.FrameIndex = CurrentFrameIndex;
            action.Line = { startPoint, endPoint };
            action.Color = currentColor;
            action.BrushSize = currentBrushSize;
            action.IsFilled = false;
        }

        if (isDrawingWindowText) {
            if (!isWritingText) {
                float scaledLeft = static_cast<float>(textArea.left) / zoomFactor;
                float scaledTop = static_cast<float>(textArea.top) / zoomFactor;
                float scaledRight = static_cast<float>(textArea.right) / zoomFactor;
                float scaledBottom = static_cast<float>(textArea.bottom) / zoomFactor;

                int width = static_cast<int>(scaledRight - scaledLeft);
                int height = static_cast<int>(scaledBottom - scaledTop);

                TInitTextTool(scaledLeft, scaledTop, width, height);

                RedrawWindow(docHWND, nullptr, nullptr, RDW_UPDATENOW | RDW_FRAME | RDW_ALLCHILDREN);

                isWritingText = true;
            }
        }

        pRenderTarget->EndDraw();

        if (isDrawingRectangle || isDrawingEllipse || isDrawingLine) {
            layersOrder.pop_back();
            if (layers[TLayersCount()].has_value()) {
                layers[TLayersCount()].reset();
            }
            layers.pop_back();
            Actions.pop_back();

            isDrawingRectangle = false;
            isDrawingEllipse = false;
            isDrawingLine = false;
        }

        Actions.push_back(action);

        ActionsClass actionsClass;
        actionsClass.TCreateMoveAction(action.Id, action);

        action = ACTION();
    }

    if (isDrawingBrush) {
        actionId++;

        ACTION action;
		action.Id = actionId;
        action.Tool = TBrush;
        action.Layer = layerIndex;
        action.FrameIndex = CurrentFrameIndex;
        action.Color = currentColor;
        action.BrushSize = currentBrushSize;
        action.FillColor = RGB(255, 255, 255);
        action.FreeForm.vertices = Vertices;
        action.IsFilled = false;
        action.isPixelMode = isPixelMode;
        Actions.push_back(action);

        ActionsClass actionsClass;
        actionsClass.TCreateMoveAction(action.Id, action);

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

    /*if (selectedAction) {
        //AQUI EU VOU ARMAZENAR A AÇÃO DO TIPO TPosition em UNDO(ACTIONS) contendo SelectedInitialPosition
        TUnSelectTool();
    }*/
    
    TUpdateLayers(layerIndex, CurrentFrameIndex);

    if (isAnimationMode)
    {
        TUpdateAnimation();
    }

    TRenderLayers();
}

void TUpdatePaint(float deltaX, float deltaY) {
    bool hasPaintActive = false;

    for (auto& action : Actions) {
        if (action.PaintTarget == selectedActionId) {
            hasPaintActive = true;
            for (auto& pixel : action.pixelsToFill) {
                pixel.x += deltaX;
                pixel.y += deltaY;
            }
        }
    }

    if (!hasPaintActive) {
        for (auto& action : RedoActions) {
            if (action.PaintTarget == selectedActionId) {
                for (auto& pixel : action.pixelsToFill) {
                    pixel.x += deltaX;
                    pixel.y += deltaY;
                }
            }
        }
    }
}

void TUndo() {

    auto it = std::find_if(
        Actions.rbegin(),
        Actions.rend(),
        [](const ACTION& act) {
            return act.Layer == layerIndex && act.FrameIndex == CurrentFrameIndex;
        }
    );

    if (it != Actions.rend()) {
        if (it->Tool == TMove) {
            //Encontra o LastMovedPosition=true, muda-o para false e move-o para RedoActions
            //Itera o Actions e altera o último LastMovedPosition=false para true

            int targetId = it->TargetID;

            auto originalIt = std::find_if(
                Actions.rbegin(),
                Actions.rend(),
                [targetId](const ACTION& act) {
                    return act.TargetID == targetId && act.LastMovedPosition == true;
                }
            );

            if (originalIt != Actions.rend()) {
                // Move a ação original para RedoActions
				originalIt->LastMovedPosition = false;
                RedoActions.push_back(*originalIt);
                Actions.erase(std::prev(originalIt.base()));
            }

            auto NewIt = std::find_if(
                Actions.rbegin(),
                Actions.rend(),
                [targetId](const ACTION& act) {
                    return act.TargetID == targetId && act.LastMovedPosition == false;
                }
            );

            if (NewIt != Actions.rend()) {
                NewIt->LastMovedPosition = true;
                
                auto TMoveIT = std::find_if(
                    Actions.begin(),
                    Actions.end(),
                    [&](const ACTION& action) {
                        return action.Tool == TMove && action.TargetID == selectedActionId && action.LastMovedPosition == true;
                    }
                );

                //DELTA delta = CalculateMovementDelta(NewIt->Position.left, NewIt->Position.top, &(*NewIt), &(*TMoveIT));
                //TUpdatePaint(delta.deltaX, delta.deltaY);

				bool hasPaintActive = false;

                for (auto& action : Actions) {
                    if (action.PaintTarget == selectedActionId) {
                        hasPaintActive = true;
                        for (auto& pixel : action.pixelsToFill) {

                            float deltaX = TMoveIT->Position.left - pixel.x;
                            float deltaY = TMoveIT->Position.top - pixel.y;

                            pixel.x += deltaX;
                            pixel.y += deltaY;
                        }
                    }
                }

                if (!hasPaintActive) {
                    for (auto& action : RedoActions) {
                        if (action.PaintTarget == selectedActionId) {
                            for (auto& pixel : action.pixelsToFill) {
                                float deltaX = TMoveIT->Position.left - pixel.x;
                                float deltaY = TMoveIT->Position.top - pixel.y;

                                pixel.x += deltaX;
                                pixel.y += deltaY;
                            }
                        }
                    }
                }

                HRenderAction(*NewIt, pRenderTarget, COLOR_UNDEFINED);
            }
        }
        else {
            // converter iterator reverso para iterator normal para erase
            auto normalIt = std::prev(it.base());

            RedoActions.push_back(*normalIt);
            Actions.erase(normalIt);

            // TLayer check
            ACTION& lastAction = RedoActions.back();


            if (lastAction.Tool == TLayer && !isAnimationMode) {
                if (layers[lastAction.Layer].has_value()) {
                    lastAction.isLayerVisible = 0;
                    layers[lastAction.Layer].value().isActive = false;
                    ShowWindow(LayerButtons[lastAction.Layer].value().button, SW_HIDE);
                }
            }
        }        
    }

    for (size_t i = 0; i < layers.size(); i++)
    {
        if (!layers[i].has_value()) continue;
        TUpdateLayers(layers[i].value().LayerID, layers[i].value().FrameIndex);
    }

    TRenderLayers();

    if (isAnimationMode) {
        TUpdateAnimation();
        TRenderAnimation();
    }
}

void TRedo() {
    if (RedoActions.empty()) return; // nada a refazer
                
    // Pega a última ação adicionada ao RedoActions (LIFO)
    ACTION lastAction = RedoActions.back();

    if (lastAction.Tool == TMove) {
        int targetId = lastAction.TargetID;

        // 1. No Actions: encontra o último LastMovedPosition=true e muda para false
        auto oldTrueIt = std::find_if(
            Actions.rbegin(),
            Actions.rend(),
            [targetId](const ACTION& act) {
                return act.TargetID == targetId && act.LastMovedPosition == true;
            }
        );

        if (oldTrueIt != Actions.rend()) {
            oldTrueIt->LastMovedPosition = false;
        }

        // 2. No RedoActions: encontra o último LastMovedPosition=false
        auto redoIt = std::find_if(
            RedoActions.rbegin(),
            RedoActions.rend(),
            [targetId](const ACTION& act) {
                return act.TargetID == targetId && act.LastMovedPosition == false;
            }
        );

        if (redoIt != RedoActions.rend()) {
            // Atualiza para true
            redoIt->LastMovedPosition = true;
			HRenderAction(*redoIt, pRenderTarget, COLOR_UNDEFINED);

            // Move de volta para Actions
            Actions.push_back(*redoIt);

            // Apaga do RedoActions
            RedoActions.erase(std::prev(redoIt.base()));
        }
    }
    else {
        RedoActions.pop_back(); // remove do redo

        // Adiciona novamente ao Actions
        Actions.push_back(lastAction);


        // Se for TLayer, reativa o layer
        if (lastAction.Tool == TLayer && !isAnimationMode) {
            if (layers[lastAction.Layer].has_value()) {
                lastAction.isLayerVisible = 1;
                layers[lastAction.Layer].value().isActive = true;
                ShowWindow(LayerButtons[lastAction.Layer].value().button, SW_SHOW);
            }
        }
    }

    // Atualiza todos os layers (mesma l�gica do TUndo)
    for (size_t i = 0; i < layers.size(); ++i) {
        if (!layers[i].has_value()) continue;
        TUpdateLayers(layers[i].value().LayerID, layers[i].value().FrameIndex);
    }

    TRenderLayers();

    if (isAnimationMode) {
        TUpdateAnimation();
        TRenderAnimation();
    }
}

/* MOVE TOOL AUX */

DELTA CalculateMovementDelta(int x, int y, ACTION *targetAction, ACTION *selected) {
    // Scale coordinates
    float scaledX = static_cast<float>(x) / zoomFactor;
    float scaledY = static_cast<float>(y) / zoomFactor;

    // Step 1: Calculate bounding box
    float minX = FLT_MAX;
    float minY = FLT_MAX;
    float maxX = -FLT_MAX;
    float maxY = -FLT_MAX;

    float deltaX;
    float deltaY;

    switch (targetAction->Tool) {
    case TBrush:
        for (const auto& v : selected->FreeForm.vertices) {
            minX = (std::min)(minX, v.x);
            minY = (std::min)(minY, v.y);
            maxX = (std::max)(maxX, v.x);
            maxY = (std::max)(maxY, v.y);
        }
        break;
    case TRectangle:
    case TWrite:
        minX = selected->Position.left;
        minY = selected->Position.top;
        maxX = selected->Position.right;
        maxY = selected->Position.bottom;
        break;

    case TEllipse:
        minX = selected->Ellipse.point.x;
        minY = selected->Ellipse.point.y;
        maxX = selected->Ellipse.point.x;
        maxY = selected->Ellipse.point.y;
        break;

    case TLine:
        minX = (std::min)(selected->Line.startPoint.x, selected->Line.endPoint.x);
        minY = (std::min)(selected->Line.startPoint.y, selected->Line.endPoint.y);
        maxX = (std::max)(selected->Line.startPoint.x, selected->Line.endPoint.x);
        maxY = (std::max)(selected->Line.startPoint.y, selected->Line.endPoint.y);
        break;

    default:
        break;
    }

    DELTA delta = {-1, -1};

    if (minX == FLT_MAX || minY == FLT_MAX) {
        return delta;
    }

    // Step 2: Calculate movement delta
    float centerX = (minX + maxX) / 2.0f;
    float centerY = (minY + maxY) / 2.0f;
    deltaX = scaledX - centerX;
    deltaY = scaledY - centerY;

	delta = { deltaX, deltaY };

	return delta;
}

bool IsPointNearSegment(float px, float py, float x1, float y1, float x2, float y2, float threshold = 3.0f) {
    // Calcula distância do ponto ao segmento
    float dx = x2 - x1;
    float dy = y2 - y1;
    float lenSq = dx * dx + dy * dy;

    float t = ((px - x1) * dx + (py - y1) * dy) / lenSq;
    t = (std::max)(0.0f, (std::min)(1.0f, t));

    float projX = x1 + t * dx;
    float projY = y1 + t * dy;

    float distSq = (px - projX) * (px - projX) + (py - projY) * (py - projY);
    return distSq <= threshold * threshold;
}

// Teste de ponto dentro de polígono (ray casting)
bool IsPointInsidePolygon(const std::vector<VERTICE>& vertices, float px, float py) {
    bool inside = false;
    size_t n = vertices.size();

    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const auto& vi = vertices[i];
        const auto& vj = vertices[j];

        bool intersect = ((vi.y > py) != (vj.y > py)) &&
            (px < (vj.x - vi.x) * (py - vi.y) / (vj.y - vi.y) + vi.x);
        if (intersect)
            inside = !inside;
    }

    return inside;
}

// Combinação das duas checagens
bool IsPointNearEdgeOrInside(const std::vector<VERTICE>& vertices, float px, float py, float threshold = 3.0f) {
    if (vertices.size() < 2)
        return false;

    // Checa proximidade das arestas
    for (size_t i = 1; i < vertices.size(); ++i) {
        if (IsPointNearSegment(px, py, vertices[i - 1].x, vertices[i - 1].y, vertices[i].x, vertices[i].y, threshold))
            return true;
    }

    // Também verifica o último segmento (fechando o polígono)
    if (IsPointNearSegment(px, py, vertices.back().x, vertices.back().y, vertices.front().x, vertices.front().y, threshold))
        return true;

    // Se não está próximo da borda, verifica se está dentro
    return IsPointInsidePolygon(vertices, px, py);
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

bool IsPointInsideText(const ACTION& action, float x, float y)
{
    auto pos = action.Position; // Point2F
    float w = action.TextWidth;
    float h = action.TextHeight;

    return (x >= pos.left && x <= pos.left + w && y >= pos.top && y <= pos.top + h);
}

bool HitTestAction(const ACTION& action, float x, float y) {
    switch (action.Tool) {
    case TBrush: {
        // Se não houver vértices, não há hit
        if (action.FreeForm.vertices.empty()) return false;

        // Calcula bounding box do stroke
        float minX = FLT_MAX;
        float minY = FLT_MAX;
        float maxX = -FLT_MAX;
        float maxY = -FLT_MAX;

        for (const auto& v : action.FreeForm.vertices) {
            minX = (std::min)(minX, v.x);
            minY = (std::min)(minY, v.y);
            maxX = (std::max)(maxX, v.x);
            maxY = (std::max)(maxY, v.y);
        }

        // Usa BrushSize salvo na action quando disponível, caso contrário usa currentBrushSize
        float brushSize = action.BrushSize > 0 ? static_cast<float>(action.BrushSize) : currentBrushSize;
        float brushRadius = brushSize * 0.5f;

        // Margem extra para tolerância (ajuste conforme necessário)
        const float extraTolerance = 3.0f;
        float padding = brushRadius + extraTolerance;

        // Se o ponto estiver dentro do bounding box expandido, considera hit
        if (x >= minX - padding && x <= maxX + padding && y >= minY - padding && y <= maxY + padding) {
            return true;
        }

        // Fallback: verifica proximidade das arestas (com threshold baseado no raio)
        return IsPointNearEdgeOrInside(action.FreeForm.vertices, x, y, (std::max)(3.0f, brushRadius));
    }

    case TRectangle:
        return IsPointInsideRect(D2D1::RectF(action.Position.left, action.Position.top, action.Position.right, action.Position.bottom), x, y);
    case TWrite:
        return IsPointInsideText(action, x, y);
    case TEllipse:
        return IsPointInsideEllipse(D2D1::Ellipse(D2D1::Point2F(action.Ellipse.point.x, action.Ellipse.point.y), action.Ellipse.radiusX, action.Ellipse.radiusY), x, y);
    case TLine:
        return IsPointNearLine(D2D1::RectF(action.Position.left, action.Position.top, action.Position.right, action.Position.bottom), x, y, 5.0f); // 5 px tolerance
    default:
        return false;
    }
}

std::vector<COLORREF> CaptureCanvasPixels() {
    // Validate render target and dimensions
    if (!pRenderTarget || logicalWidth <= 0 || logicalHeight <= 0) {
        OutputDebugStringW(L"CaptureCanvasPixels: Invalid render target or dimensions\n");
        return {};
    }

    // Create a render-target bitmap (GPU-resident)
    D2D1_SIZE_U size = D2D1::SizeU(static_cast<UINT32>(logicalWidth), static_cast<UINT32>(logicalHeight));
    D2D1_BITMAP_PROPERTIES1 renderBitmapProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f // Standard DPI
    );

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> renderBitmap;
    HRESULT hr = pRenderTarget->CreateBitmap(size, nullptr, 0, &renderBitmapProps, &renderBitmap);
    if (FAILED(hr)) {
        OutputDebugStringW((L"CaptureCanvasPixels: Failed to create render bitmap, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return {};
    }

    // Create a temporary device context for rendering
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> captureContext;
    hr = g_pD2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &captureContext);
    if (FAILED(hr)) {
        OutputDebugStringW((L"CaptureCanvasPixels: Failed to create device context, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return {};
    }

    // Render layers to the render bitmap
    captureContext->SetTarget(renderBitmap.Get());
    captureContext->BeginDraw();
    captureContext->Clear(D2D1::ColorF(D2D1::ColorF::White, 1.0f)); // Match TRenderLayers

    D2D1_RECT_F destRect = D2D1::RectF(0, 0, logicalWidth, logicalHeight);

    auto it = std::find_if(
        layers.begin(),
        layers.end(),
        [](const std::optional<Layer>& optLayer) {
            return optLayer.has_value() && optLayer->LayerID == layerIndex && optLayer->FrameIndex == CurrentFrameIndex;
        }
    );

    if (it->has_value()) {
        captureContext->DrawBitmap(it->value().pBitmap.Get(), destRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    }

    hr = captureContext->EndDraw();
    if (FAILED(hr)) {
        OutputDebugStringW((L"CaptureCanvasPixels: EndDraw failed, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return {};
    }

    // Create a CPU-readable bitmap
    D2D1_BITMAP_PROPERTIES1 cpuBitmapProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f
    );

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> cpuBitmap;
    hr = pRenderTarget->CreateBitmap(size, nullptr, 0, &cpuBitmapProps, &cpuBitmap);
    if (FAILED(hr)) {
        OutputDebugStringW((L"CaptureCanvasPixels: Failed to create CPU-readable bitmap, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return {};
    }

    // Copy render bitmap to CPU-readable bitmap
    hr = cpuBitmap->CopyFromBitmap(nullptr, renderBitmap.Get(), nullptr);
    if (FAILED(hr)) {
        OutputDebugStringW((L"CaptureCanvasPixels: Failed to copy bitmap, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return {};
    }

    // Map the CPU-readable bitmap
    D2D1_MAPPED_RECT mappedRect;
    hr = cpuBitmap->Map(D2D1_MAP_OPTIONS_READ, &mappedRect);
    if (FAILED(hr)) {
        OutputDebugStringW((L"CaptureCanvasPixels: Failed to map CPU-readable bitmap, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return {};
    }

    // Convert to COLORREF (BGRA to RGB)
    std::vector<COLORREF> pixels(static_cast<size_t>(logicalWidth) * logicalHeight);
    BYTE* src = mappedRect.bits;
    for (size_t i = 0; i < pixels.size(); ++i) {
        BYTE b = src[i * 4 + 0];
        BYTE g = src[i * 4 + 1];
        BYTE r = src[i * 4 + 2];
        BYTE a = src[i * 4 + 3];
        pixels[i] = RGB(r, g, b); // Ignore alpha for COLORREF
    }

    cpuBitmap->Unmap();
    OutputDebugStringW((L"CaptureCanvasPixels: Captured " + std::to_wstring(logicalWidth) + L"x" +
        std::to_wstring(logicalHeight) + L" pixels\n").c_str());
    return pixels;
}

void AuxCopyAction() {
    auto it = std::find_if(
        Actions.begin(),
        Actions.end(),
        [](const ACTION& action) {
            return action.Id == selectedActionId;
        }
    );

    if (it != Actions.end()) {
        Clipboard = *it; // cópia real, não referência
    }
}

void AuxPasteAction() {
	Clipboard.Id = ++actionId;
	Clipboard.Layer = layerIndex;
	Clipboard.FrameIndex = CurrentFrameIndex;

    Actions.push_back(Clipboard);

    std::vector<ACTION> paintActions;

    for (auto& action : Actions) {
        if (action.PaintTarget == selectedActionId) {
            paintActions.push_back(action);
        }
    }

    if (paintActions.size() > 0) {
        for (auto& paintAction : paintActions) {
            paintAction.PaintTarget = Actions.size() - 1;

            Actions.push_back(paintAction);
        }
    }

    for (size_t i = 0; i < layers.size(); ++i) {
        if (!layers[i].has_value()) continue;
        TUpdateLayers(layers[i].value().LayerID, layers[i].value().FrameIndex);
    }

    TRenderLayers();
}

void AuxDeleteAction() {
    auto it = std::find_if(
        Actions.begin(),
        Actions.end(),
        [&](const ACTION& act) {
            return act.Id == selectedActionId;
        }
    );

    if (it != Actions.end()) {
        Actions.erase(it);
    }

    std::vector<ACTION> paintActions;

    for (auto& action : Actions) {
        if (action.PaintTarget == selectedActionId) {
            paintActions.push_back(action);
        }
    }

    if (paintActions.size() > 0) {
        for (auto& paintAction : paintActions) {
            Actions.erase(
                std::remove_if(
                    Actions.begin(),
                    Actions.end(),
                    [&](const ACTION& act) {
                        return act.Id == paintAction.Id;
                    }
                ),
                Actions.end()
			);
        }
    }

    selectedIndex = -1;
    selectedActionId = -1;

	TUpdateLayers(layerIndex, CurrentFrameIndex);
	TRenderLayers();
}