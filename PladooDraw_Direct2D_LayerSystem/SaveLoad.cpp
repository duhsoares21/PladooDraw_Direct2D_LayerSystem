#include "pch.h"
#include "Base.h"
#include "Constants.h"
#include "Helpers.h"
#include "Layers.h"
#include "Main.h"
#include "Tools.h"
#include "SaveLoad.h"
#include "Animation.h"

void SaveBinaryProject(const std::wstring& filename) {
    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open()) {
        HCreateLogData("error.log", "Failed to open file for writing: " + std::string(filename.begin(), filename.end()));
        return;
    }

    int magic = 0x30444450; // 'PDD0'
    int version = 2;
    out.write((char*)&magic, sizeof(magic));
    out.write((char*)&version, sizeof(version));
    out.write((char*)&isAnimationMode, sizeof(isAnimationMode));
    out.write((char*)&width, sizeof(width));
    out.write((char*)&height, sizeof(height));
    out.write((char*)&pixelSizeRatio, sizeof(pixelSizeRatio));
    if (!out.good()) {
        HCreateLogData("error.log", "Error writing header data");
        out.close();
        return;
    }

    // Save layers
    std::vector<std::optional<Layer>> layersToSave;
    layersToSave.reserve(layers.size());
    for (auto& l : layers) {
        if (l.has_value() && l->isActive)
            layersToSave.push_back(l);
    }

    // Salvar quantidade
    int layerCount = static_cast<int>(layersToSave.size());
    out.write((char*)&layerCount, sizeof(layerCount));

    // Salvar cada layer ativo
    for (auto& l : layersToSave) {
        bool hasValue = l.has_value();
        out.write((char*)&hasValue, sizeof(hasValue));
        if (hasValue) {
            out.write((char*)&l->LayerID, sizeof(l->LayerID));
            out.write((char*)&l->FrameIndex, sizeof(l->FrameIndex));
            out.write((char*)&l->isActive, sizeof(l->isActive));
        }
    }

    // Save layersOrder
    int layerOrderCount = static_cast<int>(layersOrder.size());
    out.write((char*)&layerOrderCount, sizeof(layerOrderCount));
    for (const auto& lo : layersOrder) {
        out.write((char*)&lo.layerOrder, sizeof(lo.layerOrder));
        out.write((char*)&lo.layerIndex, sizeof(lo.layerIndex));
        if (!out.good()) {
            HCreateLogData("error.log", "Error writing layerOrder data");
            out.close();
            return;
        }
    }

    // Save Actions

    int actionCount = static_cast<int>(Actions.size());
    out.write((char*)&actionCount, sizeof(actionCount));
    for (const auto& a : Actions) {
        out.write((char*)&a.Tool, sizeof(a.Tool));
        out.write((char*)&a.Layer, sizeof(a.Layer));
        out.write((char*)&a.FrameIndex, sizeof(a.FrameIndex));
        out.write((char*)&a.Position, sizeof(a.Position));
        if (a.Tool == TWrite) {
            uint32_t textLen = static_cast<uint32_t>(a.Text.size());
            out.write(reinterpret_cast<const char*>(&textLen), sizeof(textLen));
            if (textLen > 0) {
                out.write(reinterpret_cast<const char*>(a.Text.data()),
                    textLen * sizeof(wchar_t));
            }

            uint32_t ffLen = static_cast<uint32_t>(a.FontFamily.size());
            out.write(reinterpret_cast<const char*>(&ffLen), sizeof(ffLen));
            if (ffLen > 0) {
                out.write(reinterpret_cast<const char*>(a.FontFamily.data()),
                    ffLen * sizeof(wchar_t));
            }

            out.write(reinterpret_cast<const char*>(&a.FontSize), sizeof(a.FontSize));
            out.write(reinterpret_cast<const char*>(&a.FontWeight), sizeof(a.FontWeight));
            out.write(reinterpret_cast<const char*>(&a.FontItalic), sizeof(a.FontItalic));
            out.write(reinterpret_cast<const char*>(&a.FontUnderline), sizeof(a.FontUnderline));
            out.write(reinterpret_cast<const char*>(&a.FontStrike), sizeof(a.FontStrike));
        }
        if (a.Tool == TLayer) {
            out.write((char*)&a.isLayerVisible, sizeof(a.isLayerVisible));
            if (!out.good()) {
                std::cout << "Error Is Layer Visible" << "\n";
                out.close();
                return;
            }
        }
        out.write((char*)&a.Ellipse, sizeof(a.Ellipse));
        out.write((char*)&a.FillColor, sizeof(a.FillColor));
        out.write((char*)&a.Color, sizeof(a.Color));
        out.write((char*)&a.BrushSize, sizeof(a.BrushSize));
        out.write((char*)&a.IsFilled, sizeof(a.IsFilled));
        out.write((char*)&a.isPixelMode, sizeof(a.isPixelMode));
        out.write((char*)&a.mouseX, sizeof(a.mouseX));
        out.write((char*)&a.mouseY, sizeof(a.mouseY));

        int vertexCount = static_cast<int>(a.FreeForm.vertices.size());
        out.write((char*)&vertexCount, sizeof(vertexCount));
        if (vertexCount > 0) {
            out.write((char*)a.FreeForm.vertices.data(), vertexCount * sizeof(VERTICE));
        }

        int pixelCount = static_cast<int>(a.pixelsToFill.size());
        out.write((char*)&pixelCount, sizeof(pixelCount));
        if (pixelCount > 0) {
            out.write((char*)a.pixelsToFill.data(), pixelCount * sizeof(POINT));
        }

        if (!out.good()) {
            HCreateLogData("error.log", "Error writing action data");
            out.close();
            return;
        }
    }
}

void LoadBinaryProject(const std::wstring& filename) {
    layers.clear();
    layersOrder.clear();
    Actions.clear();
    RedoActions.clear();

    for (auto layerButton : LayerButtons) {
        if (layerButton.has_value()) {
            DestroyWindow(layerButton.value().button);
        }
    }

    for (auto frameButton : TimelineFrameButtons) {
        if (frameButton.has_value()) {
            DestroyWindow(frameButton.value().frame);
        }
    }

    TimelineFrameButtons.clear();

    HCleanup();

    std::ifstream in(filename, std::ios::binary);

    int magic = 0, version = 0, _isAnimationMode = 0;

    in.read((char*)&magic, sizeof(magic));
    in.read((char*)&version, sizeof(version));
    if (version == 2) {
        in.read((char*)&isAnimationMode, sizeof(isAnimationMode));
    }
    in.read((char*)&width, sizeof(width));
    in.read((char*)&height, sizeof(height));

    in.read((char*)&pixelSizeRatio, sizeof(pixelSizeRatio));
    if (!in.good()) {
        pixelSizeRatio = -1;
    }

    int layerCount = 0;
    in.read((char*)&layerCount, sizeof(layerCount));

    layers.clear();
    
    vector<optional<Layer>> tempLayers;
    for (int i = 0; i < layerCount; ++i) {
        bool hasValue = false;
        in.read((char*)&hasValue, sizeof(hasValue));
        if (hasValue) {
            Layer l = {};
            in.read((char*)&l.LayerID, sizeof(l.LayerID));
            if (version == 2) {
                in.read((char*)&l.FrameIndex, sizeof(l.FrameIndex));
            }
            in.read((char*)&l.isActive, sizeof(l.isActive));

            tempLayers.push_back(l);
        }
        else {
            tempLayers.push_back(nullopt);
        }
    }

    int layerOrderCount = 0;
    in.read((char*)&layerOrderCount, sizeof(layerOrderCount));

    layersOrder.clear();
    for (int i = 0; i < layerOrderCount; ++i) {
        LayerOrder lo = {};
        in.read((char*)&lo.layerOrder, sizeof(lo.layerOrder));
        in.read((char*)&lo.layerIndex, sizeof(lo.layerIndex));
        layersOrder.push_back(lo);
    }
    
    int actionCount = 0;
    in.read((char*)&actionCount, sizeof(actionCount));

    for (int i = 0; i < actionCount; ++i) {
        ACTION a = {};
        int vertexCount = 0, pixelCount = 0;

        in.read((char*)&a.Tool, sizeof(a.Tool));
        in.read((char*)&a.Layer, sizeof(a.Layer));
        if (version == 2) {
            in.read((char*)&a.FrameIndex, sizeof(a.FrameIndex));
        }
        in.read((char*)&a.Position, sizeof(a.Position));
        if (a.Tool == TWrite) {
            uint32_t length = 0;
            in.read(reinterpret_cast<char*>(&length), sizeof(length));
            a.Text.resize(length);
            if (length > 0) {
                in.read(reinterpret_cast<char*>(a.Text.data()), length * sizeof(wchar_t));
            }
            else {
                a.Text.clear();
            }

            length = 0;
            in.read(reinterpret_cast<char*>(&length), sizeof(length));
            a.FontFamily.resize(length);
            if (length > 0) {
                in.read(reinterpret_cast<char*>(a.FontFamily.data()), length * sizeof(wchar_t));
            }
            else {
                a.FontFamily.clear();
            }

            in.read(reinterpret_cast<char*>(&a.FontSize), sizeof(a.FontSize));
            in.read(reinterpret_cast<char*>(&a.FontWeight), sizeof(a.FontWeight));
            in.read(reinterpret_cast<char*>(&a.FontItalic), sizeof(a.FontItalic));
            in.read(reinterpret_cast<char*>(&a.FontUnderline), sizeof(a.FontUnderline));
            in.read(reinterpret_cast<char*>(&a.FontStrike), sizeof(a.FontStrike));
        }
        if (a.Tool == TLayer) {
            in.read((char*)&a.isLayerVisible, sizeof(a.isLayerVisible));
        }
        in.read((char*)&a.Ellipse, sizeof(a.Ellipse));
        in.read((char*)&a.FillColor, sizeof(a.FillColor));
        in.read((char*)&a.Color, sizeof(a.Color));
        in.read((char*)&a.BrushSize, sizeof(a.BrushSize));
        in.read((char*)&a.IsFilled, sizeof(a.IsFilled));
        in.read((char*)&a.isPixelMode, sizeof(a.isPixelMode));
        in.read((char*)&a.mouseX, sizeof(a.mouseX));
        in.read((char*)&a.mouseY, sizeof(a.mouseY));

        in.read((char*)&vertexCount, sizeof(vertexCount));

        a.FreeForm.vertices.resize(vertexCount);
        if (vertexCount > 0) {
            in.read((char*)a.FreeForm.vertices.data(), vertexCount * sizeof(VERTICE));
        }

        in.read((char*)&pixelCount, sizeof(pixelCount));
        if (pixelCount > 0) {
            a.pixelsToFill.resize(pixelCount);
            in.read((char*)a.pixelsToFill.data(), pixelCount * sizeof(POINT));
        }

        Actions.push_back(a);
    }

    in.close();

    if (width > 512) {
        btnWidth = BTN_WIDTH_WIDE_DEFAULT;
        btnHeight = BTN_HEIGHT_WIDE_DEFAULT;
    }
    else {
        btnWidth = BTN_WIDTH_DEFAULT;
        btnHeight = BTN_HEIGHT_DEFAULT;
    }

    HRESULT hr = Initialize(mainHWND);

    if (SUCCEEDED(hr)) {

        InitializeDocument(docHWND, width, height, pixelSizeRatio, btnWidth, btnHeight);

        RECT rcMain;
        GetClientRect(mainHWND, &rcMain);

        SetWindowPos(layerWindowHWND, HWND_TOP, (rcMain.right - rcMain.left) - btnWidth, rcMain.bottom - rcMain.top, btnWidth, rcMain.bottom - rcMain.top, SWP_NOMOVE);

        RECT rcLayerWindow;
        GetClientRect(layerWindowHWND, &rcLayerWindow);

        SetWindowPos(layersHWND, layerWindowHWND, 0, 0, (rcLayerWindow.right - rcLayerWindow.left), (rcLayerWindow.bottom - rcLayerWindow.top) - btnHeight, SWP_NOMOVE);

        RECT rcLayers;
        GetClientRect(layersHWND, &rcLayers);

        SetWindowPos(layersControlButtonsGroupHWND, layerWindowHWND, 0, (rcLayerWindow.bottom - rcLayerWindow.top), (rcLayerWindow.right - rcLayerWindow.left), btnHeight, 0);

        SetWindowPos(buttonUp, HWND_TOP, 0, 0, (rcLayerWindow.right - rcLayerWindow.left) / 2, 30, 0);
        SetWindowPos(buttonDown, HWND_TOP, (rcLayerWindow.right - rcLayerWindow.left) / 2, 0, (rcLayerWindow.right - rcLayerWindow.left) / 2, 30, 0);
        SetWindowPos(buttonPlus, HWND_TOP, 0, 30, (rcLayerWindow.right - rcLayerWindow.left), 30, 0);
        SetWindowPos(buttonMinus, HWND_TOP, 0, 60, (rcLayerWindow.right - rcLayerWindow.left), 30, SWP_NOMOVE);

        for (size_t i = 0; i < layerCount; i++) {
            if (tempLayers[i].has_value()) {
                tempLayers[i].value().pBitmap = CreateEmptyLayerBitmap();
                if (tempLayers[i].value().isActive){
                    if (tempLayers[i].value().FrameIndex == 0) {
                        TAddLayerButton(tempLayers[i].value().LayerID, true);
                    }
                    TCreateAnimationFrame(tempLayers[i].value().LayerID, tempLayers[i].value().FrameIndex);
                }
            }
        }
    }

    for (size_t i = 0; i < tempLayers.size(); ++i) {
        if (tempLayers[i].has_value()) {
            Microsoft::WRL::ComPtr<ID2D1Bitmap1> pBitmap = CreateEmptyLayerBitmap();

            pRenderTarget->SetTarget(pBitmap.Get());
            pRenderTarget->BeginDraw();
            pRenderTarget->Clear(D2D1::ColorF(1, 1, 1, 0));
            pRenderTarget->EndDraw();

            Layer layer = { tempLayers[i].value().LayerID, tempLayers[i].value().FrameIndex, true, true, pBitmap };
            layers.emplace_back(layer);
        }
        else
        {
            layers.emplace_back(nullopt);
        }
    }

    InitializeLayerRenderPreview();

    for (int i = 0; i < layerCount; ++i) {
        if (layers[i].has_value()) {
            TUpdateLayers(layers[i].value().LayerID, layers[i].value().FrameIndex);
        }
    }
    
    TRenderLayers();

    if (isAnimationMode == 1) {
        HWND hCheck = GetDlgItem(mainHWND, 110);
        LRESULT state = SendMessage(hCheck, BM_GETCHECK, 0, 0);
        if (state == BST_UNCHECKED) {
            SendMessage(hCheck, BM_CLICK, 0, 0);
        }
    
        TUpdateAnimation();
        TRenderAnimation();
    }

}