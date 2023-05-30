#include "UIWindow.h"

namespace UI
{
    void UIWindow::TryDraw()
    {
        if (ShouldDraw()) {
            DrawImpl();
        }
    }

    bool UIWindow::ShouldDraw()
    {
        const bool bShouldDraw = ShouldDrawImpl();
        OnShouldDraw(bShouldDraw);
        return bShouldDraw;
    }

    void UIWindow::OnShouldDraw(bool a_bShouldDraw)
    {
        if (a_bShouldDraw != _bLastShouldDraw) {
            if (a_bShouldDraw) {
                OnOpen();
            } else {
                OnClose();
            }
        }

        _bLastShouldDraw = a_bShouldDraw;
    }

    void UIWindow::SetWindowDimensions(float a_offsetX /*= 0.f*/, float a_offsetY /*= 0.f*/, float a_sizeX /*= -1.f*/, float a_sizeY /*= -1.f*/, WindowAlignment a_alignment /*= WindowAlignment::kTopLeft*/, ImVec2 a_sizeMin /*= ImVec2(0, 0)*/, ImVec2 a_sizeMax /*= ImVec2(0, 0)*/, ImGuiCond_ a_cond /*= ImGuiCond_FirstUseEver*/)
    {
        if (!_sizeData.bInitialized) {
            auto& io = ImGui::GetIO();

            _sizeData.sizeMin = {
                std::fmin(300.f, io.DisplaySize.x - 40.f),
                std::fmin(200.f, io.DisplaySize.y - 40.f)
            };

            if (a_sizeMin.x != 0) {
                _sizeData.sizeMin.x = a_sizeMin.x;
            }
            if (a_sizeMin.y != 0) {
                _sizeData.sizeMin.y = a_sizeMin.y;
            }

            _sizeData.sizeMax = {
                io.DisplaySize.x,
                std::fmax(io.DisplaySize.y - 40.f, _sizeData.sizeMin.y)
            };

            if (a_sizeMax.x != 0) {
                _sizeData.sizeMax.x = a_sizeMax.x;
            }
            if (a_sizeMax.y != 0) {
                _sizeData.sizeMax.y = a_sizeMax.y;
            }

            switch (a_alignment) {
            case WindowAlignment::kTopLeft:
                _sizeData.pivot = { 0.f, 0.f };
                _sizeData.pos = {
                    std::fmin(20.f + a_offsetX, io.DisplaySize.x - 40.f),
                    20.f + a_offsetY
                };
                break;
            case WindowAlignment::kTopCenter:
                _sizeData.pivot = { 0.5f, 0.f };
                _sizeData.pos = {
                    io.DisplaySize.x * 0.5f + a_offsetX,
                    20.f + a_offsetY
                };
                break;
            case WindowAlignment::kTopRight:
                _sizeData.pivot = { 1.f, 0.f };
                _sizeData.pos = {
                    std::fmax(io.DisplaySize.x - 20.f - a_offsetX, 40.f),
                    20.f + a_offsetY
                };
                break;
            case WindowAlignment::kCenterLeft:
                _sizeData.pivot = { 0.f, 0.5f };
                _sizeData.pos = {
                    20.f + a_offsetX,
                    io.DisplaySize.y * 0.5f + a_offsetY
                };
                break;
            case WindowAlignment::kCenter:
                _sizeData.pivot = { 0.5f, 0.5f };
                _sizeData.pos = {
                    io.DisplaySize.x * 0.5f + a_offsetX,
                    io.DisplaySize.y * 0.5f + a_offsetY
                };
                break;
            case WindowAlignment::kCenterRight:
                _sizeData.pivot = { 1.f, 0.5f };
                _sizeData.pos = {
                    std::fmax(io.DisplaySize.x - 20.f - a_offsetX, 40.f),
                    io.DisplaySize.y * 0.5f + a_offsetY
                };
                break;
            case WindowAlignment::kBottomLeft:
                _sizeData.pivot = { 0.f, 1.f };
                _sizeData.pos = {
                    std::fmin(20.f + a_offsetX, io.DisplaySize.x - 40.f),
                    io.DisplaySize.y - 20.f - a_offsetY
                };
                break;
            case WindowAlignment::kBottomCenter:
                _sizeData.pivot = { 0.5f, 1.f };
                _sizeData.pos = {
                    io.DisplaySize.x * 0.5f + a_offsetX,
                    io.DisplaySize.y - 20.f - a_offsetY
                };
                break;
            case WindowAlignment::kBottomRight:
                _sizeData.pivot = { 1.f, 1.f };
                _sizeData.pos = {
                    std::fmax(io.DisplaySize.x - 20.f - a_offsetX, 40.f),
                    io.DisplaySize.y - 20.f - a_offsetY
                };
                break;
            }

            _sizeData.size = {
                a_sizeX < 0.f ? io.DisplaySize.x : a_sizeX,
                a_sizeY < 0.f ? io.DisplaySize.y : a_sizeY
            };

            _sizeData.bInitialized = true;
        }

        ImGui::SetNextWindowPos(_sizeData.pos, a_cond, _sizeData.pivot);
        if (a_sizeX != 0.f && a_sizeY != 0.f) {
            ImGui::SetNextWindowSize(_sizeData.size, a_cond);
            ImGui::SetNextWindowSizeConstraints(_sizeData.sizeMin, _sizeData.sizeMax);
        }
    }
}
