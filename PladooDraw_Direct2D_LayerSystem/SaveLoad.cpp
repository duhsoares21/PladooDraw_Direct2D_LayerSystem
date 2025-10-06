#include "pch.h"
#include "Base.h"
#include "Constants.h"
#include "Helpers.h"
#include "Layers.h"
#include "Main.h"
#include "Tools.h"
#include "SaveLoad.h"

void SaveBinaryProject(const std::wstring& filename) {
    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open()) {
        HCreateLogData("error.log", "Failed to open file for writing: " + std::string(filename.begin(), filename.end()));
        return;
    }

    int magic = 0x30444450; // 'PDD0'
    int version = 1;
    out.write((char*)&magic, sizeof(magic));
    out.write((char*)&version, sizeof(version));
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

    // Save Redo Actions

    int redoActionCount = static_cast<int>(RedoActions.size());
    out.write((char*)&redoActionCount, sizeof(redoActionCount));
    for (const auto& a : RedoActions) {
        out.write((char*)&a.Tool, sizeof(a.Tool));
        out.write((char*)&a.Layer, sizeof(a.Layer));
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

    out.close();
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

    LayerButtons.clear();

    HCleanup();

    std::ifstream in(filename, std::ios::binary);

    int magic = 0, version = 0;
    in.read((char*)&magic, sizeof(magic));
    in.read((char*)&version, sizeof(version));

    in.read((char*)&width, sizeof(width));
    in.read((char*)&height, sizeof(height));

    in.read((char*)&pixelSizeRatio, sizeof(pixelSizeRatio));
    if (!in.good()) {
        pixelSizeRatio = -1;
    }

    if (width > 512) {
        btnWidth = 160;
        btnHeight = 90;
    }
    else {
        btnWidth = 90;
        btnHeight = 90;
    }

    RECT rcMain;
    GetClientRect(mainHWND, &rcMain);

    RECT rc;
    GetClientRect(layersHWND, &rc);

    SetWindowPos(layersHWND, HWND_BOTTOM, (rcMain.right - rcMain.left) - btnWidth, rc.top, btnWidth, rc.bottom, 0);
    
    SetWindowPos(buttonUp, HWND_TOP, 0, rc.bottom - btnHeight, btnWidth / 2, btnHeight / 3, 0);
    SetWindowPos(buttonDown, HWND_TOP, btnWidth / 2, rc.bottom - btnHeight, btnWidth / 2, btnHeight / 3, 0);
    SetWindowPos(buttonPlus, HWND_TOP, 0, rc.bottom - (btnHeight / 1.5), btnWidth, btnHeight / 3, 0);
    SetWindowPos(buttonMinus, HWND_TOP, 0, rc.bottom - (btnHeight / 3), btnWidth, btnHeight / 3, 0);

    int layerCount = 0;
    in.read((char*)&layerCount, sizeof(layerCount));

    layers.clear();
    for (int i = 0; i < layerCount; ++i) {
        bool hasValue = false;
        in.read((char*)&hasValue, sizeof(hasValue));
        if (hasValue) {
            Layer l = {};
            in.read((char*)&l.LayerID, sizeof(l.LayerID));
            in.read((char*)&l.isActive, sizeof(l.isActive));

            layers.push_back(l);

        }
        else {
            layers.push_back(std::nullopt);
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

        a.pixelsToFill.resize(pixelCount);
        Actions.push_back(a);
    }

    in.close();

    HRESULT hr = Initialize(mainHWND, docHWND, width, height, pixelSizeRatio);

    if (SUCCEEDED(hr)) {
        for (size_t i = 0; i < layerCount; i++) {
            if (layers[i].has_value()) {
                layers[i].value().pBitmap = CreateEmptyLayerBitmap();
                if (layers[i].value().isActive){
                    TAddLayerButton(layers[i].value().LayerID, true);
                }
            }
        }
    }

    InitializeLayerRenderPreview();
    for (int i = 0; i < layerCount; ++i) {
        if (layers[i].has_value()) {
            TUpdateLayers(layers[i].value().LayerID);
        }
    }
    TRenderLayers();
}