#include "pch.h"
#include "SvgExporter.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <map>
#include "Constants.h"
#include "Tools.h"

// Função para converter COLORREF para string hexadecimal SVG
std::string ColorRefToSvgHex(COLORREF color) {
    unsigned int r = GetRValue(color);
    unsigned int g = GetGValue(color);
    unsigned int b = GetBValue(color);

    char buf[8];
    sprintf_s(buf, "#%02X%02X%02X", r, g, b);
    return std::string(buf);
}

// Função para exportar Actions para SVG com upscaling
void ExportActionsToSvg(const std::vector<ACTION>& actions, const std::string& filename, float scaleFactor, int originalWidth, int originalHeight) {
    std::ofstream svgFile(filename);

    if (!svgFile.is_open()) {
        std::cerr << "Erro ao abrir o arquivo SVG: " << filename << std::endl;
        return;
    }

    // Cabeçalho SVG
    svgFile << "<?xml version=\"1.0\" standalone=\"no\"?>\n";
    svgFile << "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";
    svgFile << "<svg width=\"" << originalWidth * scaleFactor << "\" height=\"" << originalHeight * scaleFactor << "\" ";
    svgFile << "viewBox=\"0 0 " << originalWidth * scaleFactor << " " << originalHeight * scaleFactor << "\" ";
    svgFile << "xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n";

    // Para o Paint Bucket, vamos consolidar todos os pixels preenchidos por cor e gerar um único path para cada cor.
    // Isso evita a animação de preenchimento e resulta em um SVG mais otimizado.
    std::map<COLORREF, std::vector<POINT>> consolidatedPaintBucketPixels;

    for (const auto& action : actions) {
        std::cout << "Export Action Tool=" << action.Tool << " Color=0x" << std::hex << action.Color << std::dec << "\n";

        switch (action.Tool) {
        case TEraser: // Eraser (não exportado para SVG, pois é uma operação de limpeza)
            break;
        case TBrush: // Brush (FreeForm)
        {
            if (!action.FreeForm.vertices.empty()) {
                if (action.isPixelMode) {
                    // Se for pixelMode, cada vertice representa um pixel 1x1
                    for (const auto& vertice : action.FreeForm.vertices) {
                        int x = static_cast<int>(vertice.x / pixelSizeRatio) * pixelSizeRatio * scaleFactor;
                        int y = static_cast<int>(vertice.y / pixelSizeRatio) * pixelSizeRatio * scaleFactor;                        

                        D2D1_RECT_F pixel = D2D1::RectF(
                            static_cast<float>(x),
                            static_cast<float>(y),
                            static_cast<float>(x + pixelSizeRatio * scaleFactor),
                            static_cast<float>(y + pixelSizeRatio * scaleFactor)
                        );

                        // Assumimos que o BrushSize em pixelMode é o tamanho do pixel
                        svgFile << "  <rect x=\"" << pixel.left << "\" y=\"" << pixel.top << "\" width=\"" << pixel.right - pixel.left << "\" height=\"" << pixel.bottom - pixel.top << "\" fill=\"" << ColorRefToSvgHex(action.Color) << "\" />\n";
                    }
                }
                else {
                    // Modo normal de pincel, usa polyline
                    svgFile << "  <polyline points=\"";
                    for (const auto& vertice : action.FreeForm.vertices) {
                        svgFile << (vertice.x * scaleFactor) << "," << (vertice.y * scaleFactor) << " ";
                    }
                    svgFile << "\" fill=\"none\" stroke=\"" << ColorRefToSvgHex(action.Color) << "\" stroke-width=\"" << (action.BrushSize * scaleFactor) << "\" stroke-linecap=\"round\" stroke-linejoin=\"round\" />\n";
                }
            }
            break;
        }
        case TRectangle: // Rectangle
        {
            float x = action.Position.left * scaleFactor;
            float y = action.Position.top * scaleFactor;
            float width = (action.Position.right - action.Position.left) * scaleFactor;
            float height = (action.Position.bottom - action.Position.top) * scaleFactor;
            svgFile << "  <rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << width << "\" height=\"" << height << "\" fill=\"" << ColorRefToSvgHex(action.Color) << "\" />\n";
            break;
        }
        case TEllipse: // Ellipse
        {
            float cx = action.Ellipse.point.x * scaleFactor;
            float cy = action.Ellipse.point.y * scaleFactor;
            float rx = action.Ellipse.radiusX * scaleFactor;
            float ry = action.Ellipse.radiusY * scaleFactor;
            svgFile << "  <ellipse cx=\"" << cx << "\" cy=\"" << cy << "\" rx=\"" << rx << "\" ry=\"" << ry << "\" fill=\"" << ColorRefToSvgHex(action.Color) << "\" />\n";
            break;
        }
        case TLine: // Line
        {
            float x1 = action.Line.startPoint.x * scaleFactor;
            float y1 = action.Line.startPoint.y * scaleFactor;
            float x2 = action.Line.endPoint.x * scaleFactor;
            float y2 = action.Line.endPoint.y * scaleFactor;
            svgFile << "  <line x1=\"" << x1 << "\" y1=\"" << y1 << "\" x2=\"" << x2 << "\" y2=\"" << y2 << "\" stroke=\"" << ColorRefToSvgHex(action.Color) << "\" stroke-width=\"" << (action.BrushSize * scaleFactor) << "\" stroke-linecap=\"round\" />\n";
            break;
        }
        case TPaintBucket: // Paint Bucket
        {
            auto pixels = action.pixelsToFill; // cria cópia pra ordenar
            std::sort(pixels.begin(), pixels.end(), [](const FLOATPOINT& a, const FLOATPOINT& b) {
                return (a.y < b.y) || (a.y == b.y && a.x < b.x);
                });

            size_t i = 0;
            while (i < pixels.size()) {
                LONG startX = pixels[i].x;
                LONG y = pixels[i].y;
                size_t j = i + 1;

                // estende enquanto pixels forem contíguos horizontalmente
                while (j < pixels.size() && pixels[j].y == y && pixels[j].x == startX + (j - i)) {
                    ++j;
                }

                // Output merged rect para o run
                float x = startX * zoomFactor * scaleFactor;
                float rectY = y * zoomFactor * scaleFactor;
                float width = (j - i) * zoomFactor * scaleFactor;
                float height = zoomFactor * scaleFactor;
                svgFile << "  <rect x=\"" << x << "\" y=\"" << rectY << "\" width=\"" << width
                    << "\" height=\"" << height << "\" fill=\"" << ColorRefToSvgHex(action.FillColor) << "\" />\n";

                i = j;
            }
            break;
        }
        case TWrite: // Text
        {
            // O 'y' no SVG é a linha de base do texto, então ajustamos a partir do 'top' da Position
            // FontSize é em décimos de ponto, converter para pixels e escalar
            float fontSizeScaled = (action.FontSize / 10.0f) * scaleFactor;
            float x = action.Position.left * scaleFactor;
            // Ajuste para a linha de base. A posição Y da ACTION é o topo da caixa de texto.
            // No SVG, o 'y' do texto é a linha de base, então adicionamos o tamanho da fonte.
            float y = (action.Position.top * scaleFactor) + fontSizeScaled;

            std::string style = "font-family:\'" + std::string(action.FontFamily.begin(), action.FontFamily.end()) + "\';";
            style += "font-size:" + std::to_string(fontSizeScaled) + "px;";
            style += "fill:" + ColorRefToSvgHex(action.Color) + ";";
            if (action.FontWeight > 400) { // DWRITE_FONT_WEIGHT_NORMAL é 400
                style += "font-weight:bold;";
            }
            if (action.FontItalic) {
                style += "font-style:italic;";
            }
            std::string textDecoration = "";
            if (action.FontUnderline) {
                textDecoration += "underline ";
            }
            if (action.FontStrike) {
                textDecoration += "line-through ";
            }
            if (!textDecoration.empty()) {
                style += "text-decoration:" + textDecoration + ";";
            }

            svgFile << "  <text x=\"" << x << "\" y=\"" << y << "\" style=\"" << style << "\">"
                << std::string(action.Text.begin(), action.Text.end()) << "</text>\n";
            break;
        }
        default:
            std::cerr << "Ferramenta desconhecida: " << action.Tool << std::endl;
            break;
        }
    }

    svgFile << "</svg>\n";
    svgFile.close();
    std::cout << "SVG exportado com sucesso para: " << filename << std::endl;

    std::string fileName = "imagem.svg";
    std::string text = "SVG Exportado com sucesso para " + fileName;

    LPCSTR message = text.c_str();
    LPCSTR title = "SVG Exporter";

    MessageBoxA(mainHWND, message, title, MB_OK);
}


