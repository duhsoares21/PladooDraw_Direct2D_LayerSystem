#include "pch.h"
#include "CoreBase.h"
#include "Structs.h"
#include "Constants.h"
#include "Helpers.h"
#include "Tools.h"
#include "ToolsAux.h"
#include "Layers.h"
#include "Main.h"
#include "Actions.h"

/* TOOLS */

namespace {
    std::optional<std::reference_wrapper<Layer>> FindLayerRef(int targetLayerIndex, int targetFrameIndex) {
        auto it = std::find_if(
            layers.begin(),
            layers.end(),
            [targetLayerIndex, targetFrameIndex](const std::optional<Layer>& optLayer) {
                return optLayer.has_value() && optLayer->LayerID == targetLayerIndex && optLayer->FrameIndex == targetFrameIndex;
            }
        );

        if (it == layers.end() || !it->has_value()) {
            return std::nullopt;
        }

        return std::ref(it->value());
    }
}

void TEraserTool(int left, int top) {

	RedoActions.clear();

    auto layerRef = FindLayerRef(layerIndex, CurrentFrameIndex);
    if (!layerRef.has_value()) return;

    Layer& currentLayerRef = layerRef->get();
    if (!currentLayerRef.surfaceHandle) return;

    if (prevLeft == -1 && prevTop == -1) {
        prevLeft = static_cast<float>(left) / zoomFactor;
        prevTop = static_cast<float>(top) / zoomFactor;
    }

    currentLayerRef.surfaceHandle->BeginDraw();
    currentLayerRef.surfaceHandle->SetTransform(MakeIdentityMatrix3x2());

    // Scale coordinates and size
    float scaledLeft = static_cast<float>(left) / zoomFactor;
    float scaledTop = static_cast<float>(top) / zoomFactor;
    float scaledBrushSize = static_cast<float>(currentEraserSize) / zoomFactor;

    float dx = scaledLeft - prevLeft;
    float dy = scaledTop - prevTop;
    float distance = sqrt(dx * dx + dy * dy);

    float step = scaledBrushSize * 0.5f;

    if (distance > step) {
        float steps = distance / step;
        float deltaX = dx / steps;
        float deltaY = dy / steps;

        for (float i = 0; i < steps; i++) {
            float x = prevLeft + i * deltaX;
            float y = prevTop + i * deltaY;

            RectF rect = MakeRectF(
                x - scaledBrushSize,
                y - scaledBrushSize,
                x + scaledBrushSize,
                y + scaledBrushSize
            );

            currentLayerRef.surfaceHandle->PushClip(rect);
            currentLayerRef.surfaceHandle->Clear(ColorRGBA{ 1.0f, 1.0f, 1.0f, 0.0f });
            currentLayerRef.surfaceHandle->PopClip();

            if (x != prevLeft || y != prevTop) {
                if (x != -1 && y != -1) {
                    ACTION action;
                    action.Tool = 0;
                    action.Layer = layerIndex;
                    action.Position = rect;
                    action.BrushSize = static_cast<int>(scaledBrushSize);
                    action.IsFilled = false;
                    Actions.emplace_back(action);
                }
            }
        }
    }

    RectF rect = MakeRectF(
        scaledLeft - scaledBrushSize,
        scaledTop - scaledBrushSize,
        scaledLeft + scaledBrushSize,
        scaledTop + scaledBrushSize
    );

    currentLayerRef.surfaceHandle->PushClip(rect);
    currentLayerRef.surfaceHandle->Clear(ColorRGBA{ 1.0f, 1.0f, 1.0f, 0.0f });
    currentLayerRef.surfaceHandle->PopClip();

    if (scaledLeft != prevLeft || scaledTop != prevTop) {
        if (scaledLeft != -1 && scaledTop != -1) {
            actionId++;

            ACTION action;
            action.Id = actionId;
            action.Tool = TEraser;
            action.Layer = layerIndex;
            action.FrameIndex = CurrentFrameIndex;
            action.Position = rect;
            action.BrushSize = static_cast<int>(scaledBrushSize);
            action.IsFilled = false;
            Actions.emplace_back(action);
        }
    }

    prevLeft = scaledLeft;
    prevTop = scaledTop;

    currentLayerRef.surfaceHandle->EndDraw();
}

void TBrushTool(int left, int top, COLORREF hexColor, bool pixelMode, int pPixelSizeRatio) {
    RedoActions.clear();

    auto layerRef = FindLayerRef(layerIndex, CurrentFrameIndex);
    if (!layerRef.has_value()) return;

    Layer& currentLayerRef = layerRef->get();
    if (!currentLayerRef.surfaceHandle) return;

    if (pixelSizeRatio == -1) {
        pixelSizeRatio = pPixelSizeRatio;
    }

    currentColor = hexColor;
    isDrawingBrush = true;
    isPixelMode = pixelMode;

    currentLayerRef.surfaceHandle->BeginDraw();
    currentLayerRef.surfaceHandle->SetTransform(MakeIdentityMatrix3x2());

    // Scale coordinates and sizes
    float scaledLeft = static_cast<float>(left) / zoomFactor;
    float scaledTop = static_cast<float>(top) / zoomFactor;
    float scaledBrushSize = static_cast<float>(currentBrushSize) / zoomFactor;

    if (pixelMode) {
        RECT rect;

        GetWindowRect(docHWND, &rect);

        int DocumentWidth = rect.right - rect.left;
        int DocumentHeight = rect.bottom - rect.top;

        int CanvasWidth = DocumentWidth / 16;
        int CanvasHeight = DocumentHeight / 16;

        pixelSizeRatio = DocumentWidth / CanvasWidth;

        // PIXEL MODE: 1x1 rectangles only
        int snappedLeft = static_cast<int>(scaledLeft / pixelSizeRatio) * pixelSizeRatio;
        int snappedTop = static_cast<int>(scaledTop / pixelSizeRatio) * pixelSizeRatio;

        RectF pixel = MakeRectF(
            static_cast<float>(snappedLeft),
            static_cast<float>(snappedTop),
            static_cast<float>(snappedLeft + pixelSizeRatio),
            static_cast<float>(snappedTop + pixelSizeRatio)
        );

        currentLayerRef.surfaceHandle->FillRect(pixel, HGetRGBAColor(hexColor));

        Vertices.emplace_back(VERTICE{ static_cast<float>(snappedLeft), static_cast<float>(snappedTop), static_cast<int>(currentBrushSize) });
    }
    else {
        if (prevLeft == -1 && prevTop == -1) {
            prevLeft = scaledLeft;
            prevTop = scaledTop;
        }

        // NORMAL MODE (smooth brush stroke)
        float dx = scaledLeft - prevLeft;
        float dy = scaledTop - prevTop;
        float distance = sqrt(dx * dx + dy * dy);
        float step = scaledBrushSize * 0.5f;

        if (distance > step) {
            float steps = distance / step;
            float deltaX = dx / steps;
            float deltaY = dy / steps;

            for (float i = 0; i < steps; i++) {
                float x = prevLeft + i * deltaX;
                float y = prevTop + i * deltaY;

                RectF rect = MakeRectF(
                    x - scaledBrushSize * 0.5f,
                    y - scaledBrushSize * 0.5f,
                    x + scaledBrushSize * 0.5f,
                    y + scaledBrushSize * 0.5f
                );

                currentLayerRef.surfaceHandle->StrokeRect(rect, HGetRGBAColor(hexColor), 1.0f);
                currentLayerRef.surfaceHandle->FillRect(rect, HGetRGBAColor(hexColor));

                if (x != -1 && y != -1) {
                    Vertices.emplace_back(VERTICE{ x, y });
                }
            }
        }

        RectF rect = MakeRectF(
            scaledLeft - scaledBrushSize * 0.5f,
            scaledTop - scaledBrushSize * 0.5f,
            scaledLeft + scaledBrushSize * 0.5f,
            scaledTop + scaledBrushSize * 0.5f
        );

        currentLayerRef.surfaceHandle->StrokeRect(rect, HGetRGBAColor(hexColor), 1.0f);
        currentLayerRef.surfaceHandle->FillRect(rect, HGetRGBAColor(hexColor));

        if (scaledLeft != -1 && scaledTop != -1) {
            Vertices.emplace_back(VERTICE{ static_cast<float>(scaledLeft), static_cast<float>(scaledTop), static_cast<int>(currentBrushSize) });
        }
    }

    currentLayerRef.surfaceHandle->EndDraw();

    prevLeft = scaledLeft;
    prevTop = scaledTop;
}

void TRectangleTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    RedoActions.clear();

    if (!layers[layerIndex].has_value()) return;

    if (!isDrawingRectangle) {
        TAddLayer(false, -1, CurrentFrameIndex);
    }

    isDrawingRectangle = true;

    currentColor = hexColor;

    auto& previewLayer = layers[TLayersCount() - 1].value();
    if (!previewLayer.surfaceHandle) return;

    previewLayer.surfaceHandle->BeginDraw();
    previewLayer.surfaceHandle->SetTransform(MakeIdentityMatrix3x2());

    // Scale coordinates
    float scaledLeft = static_cast<float>(left) / zoomFactor;
    float scaledTop = static_cast<float>(top) / zoomFactor;
    float scaledRight = static_cast<float>(right) / zoomFactor;
    float scaledBottom = static_cast<float>(bottom) / zoomFactor;

    rectangle = MakeRectF(scaledLeft, scaledTop, scaledRight, scaledBottom);

    if (prevRectangle.left != 0 || prevRectangle.top != 0 || prevRectangle.right != 0 || prevRectangle.bottom != 0) {
        previewLayer.surfaceHandle->PushClip(prevRectangle);
        previewLayer.surfaceHandle->Clear(ColorRGBA{ 1.0f, 1.0f, 1.0f, 0.0f });
        previewLayer.surfaceHandle->PopClip();
    }

    previewLayer.surfaceHandle->PushClip(rectangle);
    previewLayer.surfaceHandle->FillRect(rectangle, HGetRGBAColor(hexColor));
    previewLayer.surfaceHandle->PopClip();

    prevRectangle = MakeRectF(scaledLeft, scaledTop, scaledRight, scaledBottom);

    previewLayer.surfaceHandle->EndDraw();
}

void TEllipseTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    RedoActions.clear();

    if (!layers[layerIndex].has_value()) return;

    if (!isDrawingEllipse) {
        TAddLayer(false, -1, -1);
    }

    isDrawingEllipse = true;

    currentColor = hexColor;

    auto& previewLayer = layers[TLayersCount() - 1].value();
    if (!previewLayer.surfaceHandle) return;

    previewLayer.surfaceHandle->BeginDraw();
    previewLayer.surfaceHandle->SetTransform(MakeIdentityMatrix3x2());

    // Scale coordinates
    float scaledLeft = static_cast<float>(left) / zoomFactor;
    float scaledTop = static_cast<float>(top) / zoomFactor;
    float scaledRight = static_cast<float>(right) / zoomFactor;
    float scaledBottom = static_cast<float>(bottom) / zoomFactor;

    ellipse = MakeEllipseF(
        MakePointF(scaledLeft, scaledTop),
        abs(scaledRight - scaledLeft) / 2.0f,
        abs(scaledBottom - scaledTop) / 2.0f
    );

    if (prevEllipse.point.x != 0 || prevEllipse.point.y != 0 || prevEllipse.radiusX != 0 || prevEllipse.radiusY != 0) {
        RectF prevRect = MakeRectF(
            prevEllipse.point.x - prevEllipse.radiusX,
            prevEllipse.point.y - prevEllipse.radiusY,
            prevEllipse.point.x + prevEllipse.radiusX,
            prevEllipse.point.y + prevEllipse.radiusY
        );

        previewLayer.surfaceHandle->PushClip(prevRect);
        previewLayer.surfaceHandle->Clear(ColorRGBA{ 1.0f, 1.0f, 1.0f, 0.0f });
        previewLayer.surfaceHandle->PopClip();
    }

    previewLayer.surfaceHandle->FillEllipse(ellipse, HGetRGBAColor(hexColor));

    prevEllipse = ellipse;

    previewLayer.surfaceHandle->EndDraw();
}

void TLineTool(int xInitial, int yInitial, int x, int y, unsigned int hexColor) {
    RedoActions.clear();

    if (!layers[layerIndex].has_value()) return;

    if (!isDrawingLine) {
        TAddLayer(false, -1, -1);
    }

    isDrawingLine = true;

    currentColor = hexColor;

    auto& previewLayer = layers[TLayersCount() - 1].value();
    if (!previewLayer.surfaceHandle) return;

    previewLayer.surfaceHandle->BeginDraw();
    previewLayer.surfaceHandle->SetTransform(MakeIdentityMatrix3x2());

    // Scale coordinates and size
    float scaledXInitial = static_cast<float>(xInitial) / zoomFactor;
    float scaledYInitial = static_cast<float>(yInitial) / zoomFactor;
    float scaledX = static_cast<float>(x) / zoomFactor;
    float scaledY = static_cast<float>(y) / zoomFactor;
    float scaledBrushSize = static_cast<float>(currentBrushSize) / zoomFactor;

    if (startPoint.x != 0 || startPoint.y != 0 || endPoint.x != 0 || endPoint.y != 0) {
        RectF lineBounds = MakeRectF(
            min(startPoint.x, endPoint.x) - scaledBrushSize,
            min(startPoint.y, endPoint.y) - scaledBrushSize,
            max(startPoint.x, endPoint.x) + scaledBrushSize,
            max(startPoint.y, endPoint.y) + scaledBrushSize
        );

        previewLayer.surfaceHandle->PushClip(lineBounds);
        previewLayer.surfaceHandle->Clear(ColorRGBA{ 1.0f, 1.0f, 1.0f, 0.0f });
        previewLayer.surfaceHandle->PopClip();
    }

    startPoint = MakePointF(scaledXInitial, scaledYInitial);
    endPoint = MakePointF(scaledX, scaledY);

    previewLayer.surfaceHandle->DrawLine(startPoint, endPoint, HGetRGBAColor(hexColor), scaledBrushSize);

    previewLayer.surfaceHandle->EndDraw();
}

void TPaintBucketTool(int mouseX, int mouseY, COLORREF fillColor, HWND hWnd) {
    
    RedoActions.clear();

    auto layerRef = FindLayerRef(layerIndex, CurrentFrameIndex);
    if (!layerRef.has_value()) return;
    Layer& currentLayerRef = layerRef->get();
    if (!currentLayerRef.surfaceHandle) return;

    RECT rc;
    GetClientRect(docHWND, &rc);
    
    int capW = static_cast<int>(logicalWidth);
    int capH = static_cast<int>(logicalHeight);

    std::vector<COLORREF> pixels = CaptureCanvasPixels();
    if (pixels.size() < static_cast<size_t>(capW * capH)) {
        HCreateLogData("error.log", "PaintBucketTool: falha ao capturar pixels ampliados");
        return;
    }

    float startX = mouseX / zoomFactor;
    float startY = mouseY / zoomFactor;
    int startIdx = startY * capW + startX;

    if (startX < 0 || startX >= capW || startY < 0 || startY >= capH) return;

    COLORREF targetColor = pixels[startIdx];
    if (targetColor == fillColor) return;

    // --- OPTIMIZED BFS ---
    std::vector<FLOATPOINT> pixelsToFill;
    pixelsToFill.reserve(10000);

    std::vector<FLOATPOINT> q;  // flat vector replaces queue
    q.reserve(capW * capH);
    q.push_back({ startX, startY });

    std::vector<uint8_t> visited(capW * capH, 0); // faster visited map
    visited[startIdx] = 1;

    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    size_t idx = 0;
    while (idx < q.size()) {
        FLOATPOINT p = q[idx++];
        pixelsToFill.push_back(p);

        for (int i = 0; i < 4; ++i) {
            float nx = p.x + dx[i];
            float ny = p.y + dy[i];
            if (nx >= 0 && nx < capW && ny >= 0 && ny < capH) {
                int ni = ny * capW + nx;
                if (!visited[ni] && pixels[ni] == targetColor) {
                    visited[ni] = 1;
                    q.push_back({ nx, ny });
                }
            }
        }
    }
    // --- END OPTIMIZED BFS ---

    std::vector<RectF> rects(pixelsToFill.size());
    
    for (size_t j = 0; j < pixelsToFill.size(); j++) {
        auto [px, py] = pixelsToFill[j];
        rects[j] = MakeRectF(
            (FLOAT)px, (FLOAT)py,
            (FLOAT)px + 1, (FLOAT)py + 1
        );
    }
    
    currentLayerRef.surfaceHandle->BeginDraw();
    currentLayerRef.surfaceHandle->SetTransform(MakeIdentityMatrix3x2());
    for (auto& r : rects) {
        currentLayerRef.surfaceHandle->FillRect(r, HGetRGBAColor(fillColor));
    }
    currentLayerRef.surfaceHandle->EndDraw();

    // 6) Salva ACTION

    int paintTargetID = -1;

    for (size_t i = 0; i < Actions.size(); ++i) {
        if (HitTestAction(Actions[i], mouseX, mouseY)) {
            paintTargetID = Actions[i].Id;
        }
    }

    ACTION action;
    actionId++;

	action.Id = actionId;
    action.Tool = TPaintBucket;
    action.Layer = layerIndex;
    action.FrameIndex = CurrentFrameIndex;
    action.FillColor = fillColor;
    action.mouseX = startX;
    action.mouseY = startY;
    action.pixelsToFill = std::move(pixelsToFill);
    action.PaintTarget = paintTargetID;
    Actions.emplace_back(std::move(action));

    TRenderLayers();
}

void TInitTextTool(float scaledLeft, float scaledTop, float width, float height) {
    RedoActions.clear();

    if (isWritingText) return;
    HINSTANCE hInstance = GetModuleHandle(NULL);

    hTextInput = CreateWindowEx(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        scaledLeft, scaledTop, width, height,        // temporary size, will update per textArea
        docHWND, nullptr, hInstance , nullptr
    );

    oldEditProc = (WNDPROC)SetWindowLongPtr(
        hTextInput,
        GWLP_WNDPROC,
        (LONG_PTR)TextEditProc
    );

    SetFocus(hTextInput);
    
}

void TWriteTool(int x, int y) {
    if (fontSize == 0) {
        SetFont();
        return;
    }

    float scaledX = static_cast<float>(x) / zoomFactor;
    float scaledY = static_cast<float>(y) / zoomFactor;
    float scaledW = 200 / zoomFactor;
    float scaledH = 30 / zoomFactor;

    textArea = MakeRectF(scaledX, scaledY, scaledX + scaledW, scaledY + scaledH);

    HINSTANCE hInstance = GetModuleHandle(NULL);

    hTextInput = CreateWindowEx(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        scaledX, scaledY, scaledW, scaledH,
        docHWND, nullptr, hInstance, nullptr
    );

    SetFocus(hTextInput);

    oldEditProc = (WNDPROC)SetWindowLongPtr(
        hTextInput,
        GWLP_WNDPROC,
        (LONG_PTR)TextEditProc
    );
}

void TWriteToolCommitText() {
    auto layerRef = FindLayerRef(layerIndex, CurrentFrameIndex);
    if (!layerRef.has_value()) return;
    Layer& currentLayerRef = layerRef->get();
    if (!currentLayerRef.surfaceHandle || !gGraphicsBackend) return;

    wchar_t buffer[1024] = {};
    GetWindowText(hTextInput, buffer, 1024);
    std::wstring text(buffer);

    FontDesc fontDesc{
        fontFace,
        fontSize,
        static_cast<int>(fontWeight),
        fontItalic != 0,
        fontUnderline != 0,
        fontStrike != 0
    };

    currentLayerRef.surfaceHandle->BeginDraw();
    currentLayerRef.surfaceHandle->SetTransform(MakeIdentityMatrix3x2());
    currentLayerRef.surfaceHandle->DrawText(text, fontDesc, textArea, HGetRGBAColor(fontColor));
    currentLayerRef.surfaceHandle->EndDraw();

    SizeF metrics = gGraphicsBackend->MeasureText(text, fontDesc, textArea);

    isWritingText = false;

    DestroyWindow(hTextInput);

    TRenderLayers();

    InvalidateRect(docHWND, nullptr, TRUE);

    SetFocus(docHWND);

    actionId++;

    ACTION action;

	action.Id = actionId;
    action.Tool = TWrite;
    action.Text = text;
    action.TextWidth = metrics.width;
    action.TextHeight = metrics.height;
    action.Layer = layerIndex;
    action.FrameIndex = CurrentFrameIndex;
    action.Position = textArea;
    action.Color = fontColor;

    action.FontFamily = fontFace;
    action.FontSize = fontSize;
    action.FontWeight = fontWeight;
    action.FontItalic = (fontItalic != 0);
    action.FontUnderline = (fontUnderline != 0);
    action.FontStrike = (fontStrike != 0);

    Actions.push_back(action);

    ActionsClass actionsClass;
    actionsClass.TCreateMoveAction(action.Id, action);
}

void __stdcall TSelectTool(int xInitial, int yInitial) {
    // Scale coordinates
    float scaledXInitial = static_cast<float>(xInitial) / zoomFactor;
    float scaledYInitial = static_cast<float>(yInitial) / zoomFactor;

    int foundIndex = -1;

    for (int i = (int)Actions.size() - 1; i >= 0; --i) {
        const ACTION& Action = Actions[i];

        if (Action.Layer != layerIndex) continue;

        if (HitTestAction(Action, scaledXInitial, scaledYInitial)) {
            foundIndex = i;
            break;
        }
    }

    if (foundIndex == -1) {
        // Clique em área vazia -> desseleciona
        if (selectedAction) {
            TUnSelectTool();
        }
        return;
    }

    // Se já está selecionada a mesma ação, não faz nada
    if (selectedAction && selectedIndex == foundIndex) return;

    // Seleciona a ação encontrada
    selectedIndex = foundIndex;
    selectedActionId = Actions[foundIndex].Id;
    selectedAction = true;

    //TODO: Recurso de edição de texto
	//Se eu clicar novamente em uma ação já selecionada, e ela for do tipo TWrite, eu quero permitir editar o texto novamente, 
    //então reseto selectedAction para false, e chamo TWriteToolCommitText para finalizar a edição anterior (caso exista) 
    //e permitir uma nova edição

	//TODO: Recurso de transformação (redimensionar e rotacionar)
	//Detectar clique no icone das bordas da seleção, para iniciar redimensionamento.
	//Para rotação detectar clique no ícone de rotação (girar em volta do próprio eixo).

    TRenderLayers();
}

void __stdcall TMoveTool(int xInitial, int yInitial, int x, int y) {
    if (layerIndex < 0 || selectedIndex < 0 || selectedIndex >= (int)Actions.size()) {
        return;
    }

    const ACTION& selected = Actions[selectedIndex];

    // Scale coordinates
    float scaledXInitial = static_cast<float>(xInitial) / zoomFactor;
    float scaledYInitial = static_cast<float>(yInitial) / zoomFactor;
    float scaledX = static_cast<float>(x) / zoomFactor;
    float scaledY = static_cast<float>(y) / zoomFactor;

    // Step 1: Calculate bounding box
    float minX = FLT_MAX;
    float minY = FLT_MAX;
    float maxX = -FLT_MAX;
    float maxY = -FLT_MAX;

    if (selected.Tool == TBrush) {
        for (auto action = Actions.begin(); action != Actions.end(); ++action) {
            if (action->Layer != layerIndex || action->Tool != TBrush) continue;

            size_t index = std::distance(Actions.begin(), action);

            if ((int)index != selectedIndex) continue;

            for (const auto& v : action->FreeForm.vertices) {
                minX = (std::min)(minX, v.x);
                minY = (std::min)(minY, v.y);
                maxX = (std::max)(maxX, v.x);
                maxY = (std::max)(maxY, v.y);
            }
        }
    }
    else {
        switch (selected.Tool) {
        case TRectangle:
        case TWrite:
            minX = selected.Position.left;
            minY = selected.Position.top;
            maxX = selected.Position.right;
            maxY = selected.Position.bottom;
            break;

        case TEllipse:
            minX = selected.Ellipse.point.x;
            minY = selected.Ellipse.point.y;
            maxX = selected.Ellipse.point.x;
            maxY = selected.Ellipse.point.y;
            break;

        case TLine:
            minX = (std::min)(selected.Line.startPoint.x, selected.Line.endPoint.x);
            minY = (std::min)(selected.Line.startPoint.y, selected.Line.endPoint.y);
            maxX = (std::max)(selected.Line.startPoint.x, selected.Line.endPoint.x);
            maxY = (std::max)(selected.Line.startPoint.y, selected.Line.endPoint.y);
            break;

        default:
            break;
        }
    }

    if (minX == FLT_MAX || minY == FLT_MAX) {
        return;
    }

    // Step 2: Calculate movement delta (center-based, como antes)
    float centerX = (minX + maxX) / 2.0f;
    float centerY = (minY + maxY) / 2.0f;
    float deltaX = scaledX - centerX;
    float deltaY = scaledY - centerY;

    // --- Nova parte: criar ACTION do tipo TMove (não alterar a ação original) ---
    ACTION moveAction;
    actionId++;

    moveAction.Id = actionId;
    moveAction.Tool = TMove;
    moveAction.TargetID = selected.Id;
    moveAction.Layer = layerIndex;
    moveAction.FrameIndex = CurrentFrameIndex;
    moveAction.LastMovedPosition = true;

    switch (selected.Tool) {
    case TBrush:
        moveAction.FreeForm = selected.FreeForm;
        for (auto& v : moveAction.FreeForm.vertices) {
            v.x += deltaX;
            v.y += deltaY;
        }
        break;

    case TRectangle:
    case TWrite:
        moveAction.Position = selected.Position;
        moveAction.Position.left += deltaX;
        moveAction.Position.right += deltaX;
        moveAction.Position.top += deltaY;
        moveAction.Position.bottom += deltaY;
        break;

    case TEllipse:
        moveAction.Ellipse = selected.Ellipse;
        moveAction.Ellipse.point.x += deltaX;
        moveAction.Ellipse.point.y += deltaY;
        break;

    case TLine:
        moveAction.Line = selected.Line;
        moveAction.Line.startPoint.x += deltaX;
        moveAction.Line.startPoint.y += deltaY;
        moveAction.Line.endPoint.x += deltaX;
        moveAction.Line.endPoint.y += deltaY;
        break;

    default:
        // tipo não suportado para mover
        return;
    }

    // Marca como LastMovedPosition=false quaisquer TMove existentes para este TargetID
    for (auto& a : Actions) {
        if (a.Tool == TMove && a.TargetID == selected.Id && a.LastMovedPosition) {
            a.LastMovedPosition = false;
        }
    }

    // Insere a nova ação TMove (para o mecanismo de undo/redo funcionar)
    Actions.push_back(moveAction);

    // Atualiza ações de paint vinculadas (se houver)
    selectedActionId = selected.Id; // garantir que TUpdatePaint use o id correto
    TUpdatePaint(deltaX, deltaY);

    // Mantém comportamento visual imediato: atualiza layers/render
    TUpdateLayers(layerIndex, CurrentFrameIndex);
    TRenderLayers();
}

void __stdcall TResizeTool(int x, int y) {}

void __stdcall TRotateTool(float angle) {}

void __stdcall TUnSelectTool() {
    selectedActionId = -1;
    selectedIndex = -1;
    selectedAction = false;
    isMovingAction = false;
    TRenderLayers();
}

/* SUBCLASS */

LRESULT CALLBACK TextEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            TWriteToolCommitText(); // bake text into bitmap
            return 0; // swallow Enter
        }
        break;

    case WM_KILLFOCUS:
        //TWriteToolCommitText(); // user clicked away
        break;
    }

    return CallWindowProc(oldEditProc, hwnd, msg, wParam, lParam);
}

