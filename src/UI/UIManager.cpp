#include "UIManager.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <dxgi.h>
#include <dinput.h>
#include "UIMain.h"
#include "UIAnimationLog.h"
#include "UIAnimationQueue.h"
#include "UIErrorBanner.h"
#include "UIWelcomeBanner.h"

namespace UI
{
    void UIManager::Init()
    {
        if (bInitialized) {
            return;
        }

        const auto renderManager = RE::BSGraphics::Renderer::GetSingleton();

		const auto device = renderManager->data.forwarder;
		const auto context = renderManager->data.context;
		const auto swapChain = renderManager->data.renderWindows[0].swapChain;

        DXGI_SWAP_CHAIN_DESC sd{};
        swapChain->GetDesc(&sd);

        ImGui::CreateContext();

        auto& io = ImGui::GetIO();

        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.IniFilename = Settings::imguiIni.data();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

        ImGui_ImplWin32_Init(sd.OutputWindow);
        ImGui_ImplDX11_Init(device, context);

        ImGui::StyleColorsDark();
        auto& style = ImGui::GetStyle();
        style = GetDefaultStyle();

        bInitialized = true;

        //bShowMain = true;
    }

    void UIManager::Render()
    {
        if (!bInitialized) {
            return;
        }

        ProcessInputEventQueue();

        bool bShouldDraw = false;
        for (const auto& window : _windows) {
            if (window->ShouldDraw()) {
                bShouldDraw = true;
            }
        }

        if (!bShouldDraw) {
            // early out
            return;
        }

        UpdateStyle();
        ImGui_ImplWin32_NewFrame();
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();

        for (const auto& window : _windows) {
            window->TryDraw();
        }

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

#define IM_VK_KEYPAD_ENTER (VK_RETURN + 256)

    static ImGuiKey ImGui_ImplWin32_VirtualKeyToImGuiKey(WPARAM wParam)
    {
        switch (wParam) {
        case VK_TAB:
            return ImGuiKey_Tab;
        case VK_LEFT:
            return ImGuiKey_LeftArrow;
        case VK_RIGHT:
            return ImGuiKey_RightArrow;
        case VK_UP:
            return ImGuiKey_UpArrow;
        case VK_DOWN:
            return ImGuiKey_DownArrow;
        case VK_PRIOR:
            return ImGuiKey_PageUp;
        case VK_NEXT:
            return ImGuiKey_PageDown;
        case VK_HOME:
            return ImGuiKey_Home;
        case VK_END:
            return ImGuiKey_End;
        case VK_INSERT:
            return ImGuiKey_Insert;
        case VK_DELETE:
            return ImGuiKey_Delete;
        case VK_BACK:
            return ImGuiKey_Backspace;
        case VK_SPACE:
            return ImGuiKey_Space;
        case VK_RETURN:
            return ImGuiKey_Enter;
        case VK_ESCAPE:
            return ImGuiKey_Escape;
        case VK_OEM_7:
            return ImGuiKey_Apostrophe;
        case VK_OEM_COMMA:
            return ImGuiKey_Comma;
        case VK_OEM_MINUS:
            return ImGuiKey_Minus;
        case VK_OEM_PERIOD:
            return ImGuiKey_Period;
        case VK_OEM_2:
            return ImGuiKey_Slash;
        case VK_OEM_1:
            return ImGuiKey_Semicolon;
        case VK_OEM_PLUS:
            return ImGuiKey_Equal;
        case VK_OEM_4:
            return ImGuiKey_LeftBracket;
        case VK_OEM_5:
            return ImGuiKey_Backslash;
        case VK_OEM_6:
            return ImGuiKey_RightBracket;
        case VK_OEM_3:
            return ImGuiKey_GraveAccent;
        case VK_CAPITAL:
            return ImGuiKey_CapsLock;
        case VK_SCROLL:
            return ImGuiKey_ScrollLock;
        case VK_NUMLOCK:
            return ImGuiKey_NumLock;
        case VK_SNAPSHOT:
            return ImGuiKey_PrintScreen;
        case VK_PAUSE:
            return ImGuiKey_Pause;
        case VK_NUMPAD0:
            return ImGuiKey_Keypad0;
        case VK_NUMPAD1:
            return ImGuiKey_Keypad1;
        case VK_NUMPAD2:
            return ImGuiKey_Keypad2;
        case VK_NUMPAD3:
            return ImGuiKey_Keypad3;
        case VK_NUMPAD4:
            return ImGuiKey_Keypad4;
        case VK_NUMPAD5:
            return ImGuiKey_Keypad5;
        case VK_NUMPAD6:
            return ImGuiKey_Keypad6;
        case VK_NUMPAD7:
            return ImGuiKey_Keypad7;
        case VK_NUMPAD8:
            return ImGuiKey_Keypad8;
        case VK_NUMPAD9:
            return ImGuiKey_Keypad9;
        case VK_DECIMAL:
            return ImGuiKey_KeypadDecimal;
        case VK_DIVIDE:
            return ImGuiKey_KeypadDivide;
        case VK_MULTIPLY:
            return ImGuiKey_KeypadMultiply;
        case VK_SUBTRACT:
            return ImGuiKey_KeypadSubtract;
        case VK_ADD:
            return ImGuiKey_KeypadAdd;
        case IM_VK_KEYPAD_ENTER:
            return ImGuiKey_KeypadEnter;
        case VK_LSHIFT:
            return ImGuiKey_LeftShift;
        case VK_LCONTROL:
            return ImGuiKey_LeftCtrl;
        case VK_LMENU:
            return ImGuiKey_LeftAlt;
        case VK_LWIN:
            return ImGuiKey_LeftSuper;
        case VK_RSHIFT:
            return ImGuiKey_RightShift;
        case VK_RCONTROL:
            return ImGuiKey_RightCtrl;
        case VK_RMENU:
            return ImGuiKey_RightAlt;
        case VK_RWIN:
            return ImGuiKey_RightSuper;
        case VK_APPS:
            return ImGuiKey_Menu;
        case '0':
            return ImGuiKey_0;
        case '1':
            return ImGuiKey_1;
        case '2':
            return ImGuiKey_2;
        case '3':
            return ImGuiKey_3;
        case '4':
            return ImGuiKey_4;
        case '5':
            return ImGuiKey_5;
        case '6':
            return ImGuiKey_6;
        case '7':
            return ImGuiKey_7;
        case '8':
            return ImGuiKey_8;
        case '9':
            return ImGuiKey_9;
        case 'A':
            return ImGuiKey_A;
        case 'B':
            return ImGuiKey_B;
        case 'C':
            return ImGuiKey_C;
        case 'D':
            return ImGuiKey_D;
        case 'E':
            return ImGuiKey_E;
        case 'F':
            return ImGuiKey_F;
        case 'G':
            return ImGuiKey_G;
        case 'H':
            return ImGuiKey_H;
        case 'I':
            return ImGuiKey_I;
        case 'J':
            return ImGuiKey_J;
        case 'K':
            return ImGuiKey_K;
        case 'L':
            return ImGuiKey_L;
        case 'M':
            return ImGuiKey_M;
        case 'N':
            return ImGuiKey_N;
        case 'O':
            return ImGuiKey_O;
        case 'P':
            return ImGuiKey_P;
        case 'Q':
            return ImGuiKey_Q;
        case 'R':
            return ImGuiKey_R;
        case 'S':
            return ImGuiKey_S;
        case 'T':
            return ImGuiKey_T;
        case 'U':
            return ImGuiKey_U;
        case 'V':
            return ImGuiKey_V;
        case 'W':
            return ImGuiKey_W;
        case 'X':
            return ImGuiKey_X;
        case 'Y':
            return ImGuiKey_Y;
        case 'Z':
            return ImGuiKey_Z;
        case VK_F1:
            return ImGuiKey_F1;
        case VK_F2:
            return ImGuiKey_F2;
        case VK_F3:
            return ImGuiKey_F3;
        case VK_F4:
            return ImGuiKey_F4;
        case VK_F5:
            return ImGuiKey_F5;
        case VK_F6:
            return ImGuiKey_F6;
        case VK_F7:
            return ImGuiKey_F7;
        case VK_F8:
            return ImGuiKey_F8;
        case VK_F9:
            return ImGuiKey_F9;
        case VK_F10:
            return ImGuiKey_F10;
        case VK_F11:
            return ImGuiKey_F11;
        case VK_F12:
            return ImGuiKey_F12;
        default:
            return ImGuiKey_None;
        }
    }

    void UIManager::ProcessInputEvents(const RE::InputEvent* const* a_events)
    {
        if (a_events) {
            for (auto it = *a_events; it; it = it->next) {
                switch (it->GetEventType()) {
                case RE::INPUT_EVENT_TYPE::kButton:
                {
                    const auto buttonEvent = static_cast<const RE::ButtonEvent*>(it);
                    KeyEvent keyEvent(buttonEvent);

                    AddKeyEvent(keyEvent);
                    break;
                }
                case RE::INPUT_EVENT_TYPE::kChar:
                {
                    const auto charEvent = static_cast<const CharEvent*>(it);
                    KeyEvent keyEvent(charEvent);

                    AddKeyEvent(keyEvent);

                    break;
                }
                }
            }
        }
    }

    RE::TESObjectREFR* UIManager::GetConsoleRefr()
    {
        if (const auto ui = RE::UI::GetSingleton()) {
            if (const auto console = ui->GetMenu<RE::Console>(RE::Console::MENU_NAME)) {
                if (const auto ref = console->GetSelectedRef()) {
                    return ref.get();
                }
            }
        }

        return nullptr;
    }

    RE::TESObjectREFR* UIManager::GetRefrToEvaluate()
    {
        const auto newConsoleRefr = GetConsoleRefr();
        if (newConsoleRefr != _consoleRefr) {
            _consoleRefr = newConsoleRefr;
            AnimationLog::GetSingleton().ClearAnimationLog();
        }

        if (_consoleRefr) {
            return _consoleRefr;
        }

        return _refrToEvaluate;
    }

    void UIManager::SetRefrToEvaluate(RE::TESObjectREFR* a_refr)
    {
        _refrToEvaluate = a_refr;
        AnimationLog::GetSingleton().ClearAnimationLog();
    }

    bool UIManager::ShouldConsumeInput() const
    {
        return _menusConsumingInput > 0;
    }

    void UIManager::AddInputConsumer()
    {
        if (++_menusConsumingInput > 0) {
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            io.MouseDrawCursor = true;
        }
    }

    void UIManager::RemoveInputConsumer()
    {
        if (--_menusConsumingInput == 0) {
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
            io.MouseDrawCursor = false;
        }
    }

    void UIManager::AddKeyEvent(KeyEvent& a_keyEvent)
    {
        WriteLocker locker(_inputEventLock);

        _keyEventQueue.emplace_back(a_keyEvent);
    }

    void UIManager::OnFocusLost()
    {
        _bFocusLost = true;
    }

    void UIManager::ProcessInputEventQueue()
    {
        _lastKeyPressed = 0;

        auto isVkDown = [&](const int a_vk) {
            return (GetKeyState(a_vk) & 0x8000) != 0;
        };

        WriteLocker locker(_inputEventLock);

        ImGuiIO& io = ImGui::GetIO();

        if (_bFocusLost) {
            io.AddFocusEvent(false);
            _keyEventQueue.clear();

            _bFocusLost = false;
        }

        for (auto& event : _keyEventQueue) {
            const bool bIsCtrlDown = isVkDown(VK_CONTROL);
            const bool bIsShiftDown = isVkDown(VK_SHIFT);
            const bool bIsAltDown = isVkDown(VK_APPS);

            io.AddKeyEvent(ImGuiMod_Ctrl, bIsCtrlDown);
            io.AddKeyEvent(ImGuiMod_Shift, bIsShiftDown);
            io.AddKeyEvent(ImGuiMod_Alt, bIsAltDown);
            io.AddKeyEvent(ImGuiMod_Super, isVkDown(VK_APPS));

            switch (event.eventType) {
            case RE::INPUT_EVENT_TYPE::kChar:
            {
                io.AddInputCharacter(event.keyCode);
                break;
            }
            case RE::INPUT_EVENT_TYPE::kButton:
            {
                uint32_t key = MapVirtualKeyEx(event.keyCode, MAPVK_VSC_TO_VK_EX, GetKeyboardLayout(0));
                switch (event.keyCode) {
                case DIK_LEFTARROW:
                    key = VK_LEFT;
                    break;
                case DIK_RIGHTARROW:
                    key = VK_RIGHT;
                    break;
                case DIK_UPARROW:
                    key = VK_UP;
                    break;
                case DIK_DOWNARROW:
                    key = VK_DOWN;
                    break;
                case DIK_DELETE:
                    key = VK_DELETE;
                    break;
                case DIK_END:
                    key = VK_END;
                    break;
                case DIK_HOME:
                    key = VK_HOME;
                    break; // pos1
                case DIK_PRIOR:
                    key = VK_PRIOR;
                    break; // page up
                case DIK_NEXT:
                    key = VK_NEXT;
                    break; // page down
                case DIK_INSERT:
                    key = VK_INSERT;
                    break;
                case DIK_NUMPAD0:
                    key = VK_NUMPAD0;
                    break;
                case DIK_NUMPAD1:
                    key = VK_NUMPAD1;
                    break;
                case DIK_NUMPAD2:
                    key = VK_NUMPAD2;
                    break;
                case DIK_NUMPAD3:
                    key = VK_NUMPAD3;
                    break;
                case DIK_NUMPAD4:
                    key = VK_NUMPAD4;
                    break;
                case DIK_NUMPAD5:
                    key = VK_NUMPAD5;
                    break;
                case DIK_NUMPAD6:
                    key = VK_NUMPAD6;
                    break;
                case DIK_NUMPAD7:
                    key = VK_NUMPAD7;
                    break;
                case DIK_NUMPAD8:
                    key = VK_NUMPAD8;
                    break;
                case DIK_NUMPAD9:
                    key = VK_NUMPAD9;
                    break;
                case DIK_DECIMAL:
                    key = VK_DECIMAL;
                    break;
                case DIK_NUMPADENTER:
                    key = IM_VK_KEYPAD_ENTER;
                    break;
                case DIK_RMENU:
                    key = VK_RMENU;
                    break; // right alt
                case DIK_RCONTROL:
                    key = VK_RCONTROL;
                    break; // right control
                case DIK_LWIN:
                    key = VK_LWIN;
                    break; // left win
                case DIK_RWIN:
                    key = VK_RWIN;
                    break; // right win
                case DIK_APPS:
                    key = VK_APPS;
                    break;
                default:
                    break;
                }

                if (!bShowMain) {
                    if (event.keyCode == Settings::uToggleUIKeyData[0] && event.IsDown()) {
                        if (bIsCtrlDown == static_cast<bool>(Settings::uToggleUIKeyData[1]) && bIsShiftDown == static_cast<bool>(Settings::uToggleUIKeyData[2]) && bIsAltDown == static_cast<bool>(Settings::uToggleUIKeyData[3])) {
                            bShowMain = true;
                        }
                    }
                } else {
                    switch (event.device) {
                    case RE::INPUT_DEVICE::kMouse:
                        if (event.keyCode > 7) {
                            io.AddMouseWheelEvent(0, event.value * (event.keyCode == 8 ? 1 : -1));
                        } else {
                            if (event.keyCode > 5)
                                event.keyCode = 5;
                            io.AddMouseButtonEvent(event.keyCode, event.IsPressed());
                        }
                        break;
                    case RE::INPUT_DEVICE::kKeyboard:
                        if (event.keyCode == 0x1 && event.IsDown()) {
                            // 0x1 = escape
                            bShowMain = false;
                        } else if (event.keyCode == Settings::uToggleUIKeyData[0] && event.IsDown() && bIsCtrlDown == static_cast<bool>(Settings::uToggleUIKeyData[1]) && bIsShiftDown == static_cast<bool>(Settings::uToggleUIKeyData[2]) && bIsAltDown == static_cast<bool>(Settings::uToggleUIKeyData[3])) {
                            bShowMain = false;
                        } else {
                            io.AddKeyEvent(ImGui_ImplWin32_VirtualKeyToImGuiKey(key), event.IsPressed());

                            if (key < 0xA0 || key > 0xA5) {
                                // exclude modifier keys
                                _lastKeyPressed = event.keyCode;
                            }
                        }
                        break;
                    }
                    break;
                }
            }
            }
        }

        _keyEventQueue.clear();
    }

    void UIManager::DisplayWelcomeBanner() const
    {
        static_cast<UIWelcomeBanner*>(_windows[static_cast<size_t>(WindowID::kWelcomeBanner)].get())->Display();
    }

    ImGuiStyle UIManager::GetDefaultStyle()
    {
        ImGuiStyle style;
        ImGui::StyleColorsDark(&style);
        style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.65f);

        return style;
    }

    void UIManager::UpdateStyle()
    {
        const auto scale = Settings::fUIScale;
        if (scale != _prevScale) {
            auto& style = ImGui::GetStyle();
            style = GetDefaultStyle();
            style.ScaleAllSizes(scale);
            _prevScale = scale;

            auto& io = ImGui::GetIO();
            io.FontGlobalScale = scale;
        }
    }

    UIManager::UIManager() :
        _windows{
            std::make_unique<UIMain>(),
            std::make_unique<UIAnimationLog>(),
            std::make_unique<UIAnimationQueue>(),
            std::make_unique<UIErrorBanner>(),
            std::make_unique<UIWelcomeBanner>()
        } {}
}
