#include "pch.h"
#include "Base.h"
#include "Constants.h"
#include "Helpers.h"
#include "Layers.h"
#include "Main.h"
#include "Tools.h"

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

    // Save actions
    int actionCount = static_cast<int>(Actions.size());
    out.write((char*)&actionCount, sizeof(actionCount));
    for (const auto& a : Actions) {
        out.write((char*)&a.Tool, sizeof(a.Tool));
        out.write((char*)&a.Layer, sizeof(a.Layer));
        out.write((char*)&a.Position, sizeof(a.Position));
        if (a.Tool == TWrite) {
            out.write(reinterpret_cast<const char*>(a.Text), sizeof(a.Text));
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

void LoadBinaryProject(const std::wstring& filename, HWND hWndLayer, HINSTANCE hLayerInstance, int btnWidth, int btnHeight, HWND* hLayerButtons, int layerID, const wchar_t* szButtonClass, const wchar_t* msgText) {

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

    std::ifstream in(filename, std::ios::binary);

    if (!in.is_open()) {
        HCreateLogData("error.log", "Failed to open file for reading: " + std::string(filename.begin(), filename.end()));
        return;
    }

    HCleanup();
    layerID = 0;

    int magic = 0, version = 0;
    in.read((char*)&magic, sizeof(magic));
    in.read((char*)&version, sizeof(version));
    
    if (magic != 0x30444450 || version < 1 || version > 1) {
        HCreateLogData("error.log", "Invalid file format or version");
        in.close();
        return;
    }

    in.read((char*)&width, sizeof(width));
    in.read((char*)&height, sizeof(height));

    if (!in.good() || width <= 0 || height <= 0 || width > 10000 || height > 10000) {
        HCreateLogData("error.log", "Invalid dimensions or read error");
        in.close();
        return;
    }

    in.read((char*)&pixelSizeRatio, sizeof(pixelSizeRatio));
    if (!in.good()) {
        pixelSizeRatio = -1;
    }
    
    int layerOrderCount = 0;
    in.read((char*)&layerOrderCount, sizeof(layerOrderCount));

    if (!in.good() || layerOrderCount < 0 || layerOrderCount > 10000) {
        HCreateLogData("error.log", "Invalid layerOrder count: " + std::to_string(layerOrderCount));
        in.close();
        return;
    }

    layersOrder.clear();
    for (int i = 0; i < layerOrderCount; ++i) {
        LayerOrder lo = {};
        in.read((char*)&lo.layerOrder, sizeof(lo.layerOrder));
        in.read((char*)&lo.layerIndex, sizeof(lo.layerIndex));

        if (!in.good()) {
            HCreateLogData("error.log", "Error reading indexedColors for layerOrder " + std::to_string(i));
            in.close();
            return;
        }
        layersOrder.push_back(lo);
    }

    int actionCount = 0;
    in.read((char*)&actionCount, sizeof(actionCount));

    if (!in.good() || actionCount < 0 || actionCount > 1000000) {
        HCreateLogData("error.log", "Invalid action count: " + std::to_string(actionCount));
        in.close();
        return;
    }

    int maxLayer = -1;

    for (int i = 0; i < actionCount; ++i) {
        ACTION a = {};
        int vertexCount = 0, pixelCount = 0;

        in.read((char*)&a.Tool, sizeof(a.Tool));
        in.read((char*)&a.Layer, sizeof(a.Layer));
        if (!in.good() || a.Layer < 0 || a.Layer > 10000) {
            HCreateLogData("error.log", "Invalid layer index for action " + std::to_string(i) + ": " + std::to_string(a.Layer));
            in.close();
            Actions.clear();
            return;
        }

        in.read((char*)&a.Position, sizeof(a.Position));
        if (a.Tool == TWrite) {
            in.read(reinterpret_cast<char*>(&a.Text), sizeof(a.Text));
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

        if (!in.good() || vertexCount < 0 || vertexCount > 1000000) {
            HCreateLogData("error.log", "Invalid vertex count for action " + std::to_string(i));
            in.close();
            Actions.clear();
            return;
        }

        a.FreeForm.vertices.resize(vertexCount);
        if (vertexCount > 0) {
            in.read((char*)a.FreeForm.vertices.data(), vertexCount * sizeof(VERTICE));

            if (!in.good()) {
                HCreateLogData("error.log", "Error reading vertices for action " + std::to_string(i));
                in.close();
                Actions.clear();
                return;
            }
        }

        in.read((char*)&pixelCount, sizeof(pixelCount));
        if (!in.good() || pixelCount < 0 || pixelCount > width * height) {
            HCreateLogData("error.log", "Invalid pixel count for action " + std::to_string(i));
            in.close();
            Actions.clear();
            return;
        }

        a.pixelsToFill.resize(pixelCount);
        if (pixelCount > 0) {
            in.read((char*)a.pixelsToFill.data(), pixelCount * sizeof(POINT));
            if (!in.good()) {
                HCreateLogData("error.log", "Error reading pixels for action " + std::to_string(i));
                in.close();
                Actions.clear();
                return;
            }
        }
        Actions.push_back(a);
        maxLayer = (std::max)(maxLayer, a.Layer);
    }

    in.close();
    HRESULT hr = Initialize(mainHWND, docHWND, width, height, pixelSizeRatio);

    if (FAILED(hr)) {
        HCreateLogData("error.log", "Failed to initialize after layer setup: HRESULT " + std::to_string(hr));
        layers.clear();
        Actions.clear();
        LayerButtons.clear();
        return;
    }
    
    layers.clear();
    layerIndex = 0;

    for (int i = 0; i <= maxLayer; ++i) {
        HRESULT hr = TAddLayer(true, i);
        if (FAILED(hr)) {
            HCreateLogData("error.log", "Failed to add layer " + std::to_string(i) + ": HRESULT " + std::to_string(hr));
            layers.clear();
            Actions.clear();
            LayerButtons.clear();
            return;
        }

        int yPos = btnHeight * i;
        HWND button = CreateWindowEx(
            0,                            // dwExStyle
            szButtonClass,               // lpClassName
            msgText,                     // lpWindowName
            WS_CHILD | WS_VISIBLE | BS_BITMAP, // dwStyle
            0,                           // x
            yPos,                        // y
            btnWidth,                         // nWidth
            btnHeight,                   // nHeight
            hWndLayer,                   // hWndParent
            (HMENU)(intptr_t)layerID,    // hMenu (control ID)
            hLayerInstance,              // hInstance
            NULL                         // lpParam
        );

        if (button == NULL) {
            DWORD dwError = GetLastError();
            wchar_t buffer[256];
            FormatMessageW(
                FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                dwError,
                0,
                buffer,
                sizeof(buffer) / sizeof(wchar_t),
                NULL
            );

            HCreateLogData("error.log", "Failed to create button for layer " + std::to_string(layerID) + ": " + std::to_string(dwError));

            layers.clear();
            Actions.clear();
            LayerButtons.clear();
            return;
        }

        ShowWindow(button, SW_SHOWDEFAULT);
        UpdateWindow(button);

        hLayerButtons[layerID] = button;

        TAddLayerButton(button);

        if (i < maxLayer) {
            layerID++;
        }
    }

    InitializeLayerRenderPreview();

    for (int i = 0; i <= maxLayer; ++i) {
        TUpdateLayers(layersOrder[i].layerIndex);
        //TDrawLayerPreview(layersOrder[i].layerIndex);
    }

    TRenderLayers();
}