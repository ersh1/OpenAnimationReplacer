#pragma once

#include "Settings.h"

#include "UIWindow.h"

namespace UI
{
    class UIManager
    {
    public:
        class CharEvent : public RE::InputEvent
        {
        public:
            uint32_t keyCode; // 18 (ascii code)
        };

        struct KeyEvent
        {
            KeyEvent(const RE::ButtonEvent* a_event) :
                keyCode(a_event->GetIDCode()),
                device(a_event->GetDevice()),
                eventType(a_event->GetEventType()),
                value(a_event->Value()),
                heldDownSecs(a_event->HeldDuration()) {}

            KeyEvent(const CharEvent* a_event) :
                keyCode(a_event->keyCode),
                device(a_event->GetDevice()),
                eventType(a_event->GetEventType()) {}

            [[nodiscard]] constexpr bool IsPressed() const noexcept { return value > 0.0F; }
            [[nodiscard]] constexpr bool IsRepeating() const noexcept { return heldDownSecs > 0.0F; }
            [[nodiscard]] constexpr bool IsDown() const noexcept { return IsPressed() && (heldDownSecs == 0.0F); }
            [[nodiscard]] constexpr bool IsHeld() const noexcept { return IsPressed() && IsRepeating(); }
            [[nodiscard]] constexpr bool IsUp() const noexcept { return (value == 0.0F) && IsRepeating(); }

            uint32_t keyCode;
            RE::INPUT_DEVICE device;
            RE::INPUT_EVENT_TYPE eventType;
            float value;
            float heldDownSecs;
        };

        static UIManager& GetSingleton()
        {
            static UIManager singleton;
            return singleton;
        }

        void Init();
        void Render();

        void ProcessInputEvents(const RE::InputEvent* const* a_events);

        bool bInitialized = false;

        static RE::TESObjectREFR* GetConsoleRefr();
        RE::TESObjectREFR* GetRefrToEvaluate();
        void SetRefrToEvaluate(RE::TESObjectREFR* a_refr);

        bool ShouldConsumeInput() const;

        void AddInputConsumer();
        void RemoveInputConsumer();
        bool bShowMain = false;
        bool bShowErrorBanner = false;
        uint32_t GetLastKeyPressed() const { return _lastKeyPressed; }

        void AddKeyEvent(KeyEvent& a_keyEvent);
        void OnFocusLost();

        void ProcessInputEventQueue();

        void DisplayWelcomeBanner() const;

    private:
        static ImGuiStyle GetDefaultStyle();
        void UpdateStyle();
        float _prevScale = 1.f;

        RE::TESObjectREFR* _refrToEvaluate = nullptr;
        RE::TESObjectREFR* _consoleRefr = nullptr;
        uint8_t _menusConsumingInput = 0;
        uint32_t _lastKeyPressed = 0;
		bool _bShiftHeld = false;
		bool _bCtrlHeld = false;
		bool _bAltHeld = false;

        std::array<std::unique_ptr<UIWindow>, static_cast<size_t>(WindowID::kMax)> _windows;

        mutable SharedLock _inputEventLock;
        std::vector<KeyEvent> _keyEventQueue{};
        bool _bFocusLost = false;

        UIManager();
        virtual ~UIManager() noexcept = default;

        UIManager(const UIManager&) = delete;
        UIManager(UIManager&&) = delete;
        UIManager& operator=(const UIManager&) = delete;
        UIManager& operator=(UIManager&&) = delete;
    };
}
