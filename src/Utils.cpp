#include "Utils.h"

#include "BaseConditions.h"
#include <ranges>

namespace Utils
{
    std::string_view TrimWhitespace(const std::string_view a_s)
    {
        const auto startPos = a_s.find_first_not_of(" "sv);
        const auto endPos = a_s.find_last_not_of(" "sv);

        if (startPos != std::string_view::npos && endPos != std::string_view::npos) {
            return a_s.substr(startPos, endPos - startPos + 1);
        }
        return ""sv;
    }

    std::string_view TrimQuotes(const std::string_view a_s)
    {
        const auto startPos = a_s.find_first_not_of("\""sv);
        const auto endPos = a_s.find_last_not_of("\""sv);

        if (startPos != std::string_view::npos && endPos != std::string_view::npos) {
            return a_s.substr(startPos, endPos - startPos + 1);
        }
        return a_s;
    }

    std::string_view TrimSquareBrackets(const std::string_view a_s)
    {
        const auto startPos = a_s.find_first_not_of("["sv);
        const auto endPos = a_s.find_last_not_of("]"sv);

        if (startPos != std::string_view::npos && endPos != std::string_view::npos) {
            return a_s.substr(startPos, endPos - startPos + 1);
        }
        return a_s;
    }

    std::string_view TrimHexPrefix(const std::string_view a_s)
    {
        if (a_s.starts_with("0x"sv) || a_s.starts_with("0X"sv)) {
            return a_s.substr(2);
        }
        return a_s;
    }

    std::string_view GetFileNameWithoutExtension(std::string_view a_s)
    {
        const size_t slashIndex = a_s.find_last_of('\\');
        const size_t extIndex = a_s.find_last_of('.');
        if (slashIndex != std::string::npos && extIndex != std::string::npos && slashIndex < extIndex) {
            return a_s.substr(slashIndex + 1, extIndex - slashIndex - 1);
        }
        return a_s;
    }

    bool CompareStringsIgnoreCase(const std::string_view a_lhs, const std::string_view a_rhs)
    {
        if (a_lhs.size() != a_rhs.size()) {
            return false;
        }

        return std::equal(a_lhs.begin(), a_lhs.end(), a_rhs.begin(), [](const char a, const char b) {
            return std::tolower(a) == std::tolower(b);
        });
    }

    bool ContainsStringIgnoreCase(const std::string_view a_string, const std::string_view a_substring)
    {
        const auto it = std::ranges::search(a_string, a_substring, [](const char a_a, const char a_b) {
            return std::tolower(a_a) == std::tolower(a_b);
        }).begin();
        return it != a_string.end();
    }

    std::string GetFormNameString(RE::TESForm* a_form)
    {
        if (a_form) {
            std::string name = a_form->GetName();
            std::string editorID = a_form->GetFormEditorID();
            if (!editorID.empty()) {
                if (!name.empty()) {
                    return std::format("{} ({})", name, editorID);
                }
                return std::format("({})", editorID);
            }
            if (!name.empty()) {
                return name;
            }
            return std::format("[{:08X}]", a_form->GetFormID());
        }

        return "";
    }

    std::string GetFormKeywords(RE::TESForm* a_form)
    {
        if (a_form) {
            if (const auto bgsKeywordForm = a_form->As<RE::BGSKeywordForm>()) {
                return GetFormKeywords(bgsKeywordForm);
            }
        }

        return "";
    }

    std::string GetFormKeywords(RE::BGSKeywordForm* a_keywordForm)
    {
        std::string keywords;

        if (a_keywordForm) {
            bool bIsFirstKeyword = true;
            for (const auto& keyword : a_keywordForm->GetKeywords()) {
                if (keyword) {
                    if (bIsFirstKeyword) {
                        bIsFirstKeyword = false;
                    } else {
                        keywords += ", ";
                    }
                    keywords += keyword->GetFormEditorID();
                }
            }
        }

        return keywords;
    }

    std::string_view GetActorValueName(RE::ActorValue a_actorValue)
    {
        const auto actorValueList = RE::ActorValueList::GetSingleton();
        if (const auto actorValueInfo = actorValueList->GetActorValue(a_actorValue)) {
            const std::string_view actorValueName = actorValueInfo->enumName;
            return actorValueName;
        }

        return "(Not found)"sv;
    }

    bool ConditionHasRandomResult(Conditions::ICondition* a_condition)
    {
        for (uint32_t i = 0; i < a_condition->GetNumComponents(); ++i) {
            if (!a_condition->IsDisabled()) {
                if (const auto component = a_condition->GetComponent(i)) {
                    if (component->GetType() == Conditions::ConditionComponentType::kRandom) {
                        return true;
                    }
                    if (component->GetType() == Conditions::ConditionComponentType::kMulti) {
                        const auto multiComponent = static_cast<Conditions::IMultiConditionComponent*>(component);
                        if (const auto conditionSet = multiComponent->GetConditions()) {
                            if (ConditionSetHasRandomResult(conditionSet)) {
                                return true;
                            }
                        }
                    }
                }
            }
        }

        return false;
    }

    bool ConditionSetHasRandomResult(Conditions::ConditionSet* a_conditionSet)
    {
        if (a_conditionSet) {
            const auto result = a_conditionSet->ForEachCondition([&](auto& a_childCondition) {
                if (ConditionHasRandomResult(a_childCondition.get())) {
                    return RE::BSVisit::BSVisitControl::kStop;
                }
                return RE::BSVisit::BSVisitControl::kContinue;
            });

            if (result == RE::BSVisit::BSVisitControl::kStop) {
                return true;
            }
        }

        return false;
    }

    RE::TESForm* LookupForm(RE::FormID a_localFormID, std::string_view a_modName)
    {
        RE::FormID formID;
        if (g_mergeMapperInterface) {
            const auto [newModName, newFormID] = g_mergeMapperInterface->GetNewFormID(a_modName.data(), a_localFormID);
            formID = RE::TESDataHandler::GetSingleton()->LookupFormID(newFormID, newModName);
        } else {
            formID = RE::TESDataHandler::GetSingleton()->LookupFormID(a_localFormID, a_modName);
        }

        return formID ? RE::TESForm::LookupByID(formID) : nullptr;
    }
}
