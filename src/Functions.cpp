#include "Functions.h"

#include "OpenAnimationReplacer.h"

namespace Functions
{
	std::unique_ptr<IFunction> CreateFunctionFromJson(rapidjson::Value& a_value, FunctionSet* a_parentFunctionSet)
	{
		if (!a_value.IsObject()) {
			logger::error("Missing function value!");
			return std::make_unique<InvalidFunction>("Missing function value");
		}

		const auto object = a_value.GetObj();

		if (const auto functionNameIt = object.FindMember("function"); functionNameIt != object.MemberEnd() && functionNameIt->value.IsString()) {
			bool bHasRequiredPlugin = false;
			std::string requiredPluginName;
			if (const auto requiredPluginIt = object.FindMember("requiredPlugin"); requiredPluginIt != object.MemberEnd() && requiredPluginIt->value.IsString()) {
				bHasRequiredPlugin = true;
				requiredPluginName = requiredPluginIt->value.GetString();
			}

			REL::Version requiredVersion;
			if (const auto requiredVersionIt = object.FindMember("requiredVersion"); requiredVersionIt != object.MemberEnd() && requiredVersionIt->value.IsString()) {
				requiredVersion = REL::Version(requiredVersionIt->value.GetString());
			}

			EssentialState essentialState = EssentialState::kEssential;
			if (const auto essentialIt = object.FindMember("essential"); essentialIt != object.MemberEnd() && essentialIt->value.IsInt()) {
				essentialState = static_cast<EssentialState>(essentialIt->value.GetInt());
			}
			bool bEssential = essentialState == EssentialState::kEssential;

			std::string functionName = functionNameIt->value.GetString();

			if (bHasRequiredPlugin && !requiredPluginName.empty()) {
				// check if required plugin is loaded and is the required version or higher
				if (!OpenAnimationReplacer::GetSingleton().IsPluginLoaded(requiredPluginName, requiredVersion)) {
					auto errorStr = std::format("Missing required plugin {} version {}!", requiredPluginName, requiredVersion.string("."));
					if (bEssential) {
						DetectedProblems::GetSingleton().AddMissingPluginName(requiredPluginName, requiredVersion);
						logger::error("{}", errorStr);
						return std::make_unique<InvalidFunction>(errorStr);
					} else {
						auto function = std::make_unique<InvalidNonEssentialFunction>(errorStr, functionName, a_value);
						function->Initialize(&a_value);
						return std::move(function);
					}
				}
			} else {
				// no required plugin, compare required version with OAR function version
				if (requiredVersion > Plugin::VERSION) {
					auto errorStr = std::format("Function {} requires a newer version of OAR! ({})", functionName, requiredVersion.string("."));
					if (bEssential) {
						DetectedProblems::GetSingleton().MarkOutdatedVersion();
						logger::error("{}", errorStr);
						return std::make_unique<InvalidFunction>(errorStr);
					} else {
						auto function = std::make_unique<InvalidNonEssentialFunction>(errorStr, functionName, a_value);
						function->Initialize(&a_value);
						return std::move(function);
					}
				}
			}

			if (auto function = OpenAnimationReplacer::GetSingleton().CreateFunction(functionName)) {
				if (function->IsDeprecated()) {
					//return ConvertDeprecatedFunction(function, functionName, a_value);
				}
				function->PreInitialize();
				if (a_parentFunctionSet) {  // set the parent function set early if possible
					function->SetParentSet(a_parentFunctionSet);
				}
				function->Initialize(&a_value);
				function->PostInitialize();
				return std::move(function);
			}

			// at this point we failed to create a new function even though the plugin is present and not outdated. This means that the plugin failed to initialize factories for whatever reason.
			if (bHasRequiredPlugin) {
				auto errorStr = std::format("Function {} not found in plugin {}!", functionName, requiredPluginName);
				if (bEssential) {
					DetectedProblems::GetSingleton().AddInvalidPluginName(requiredPluginName, requiredVersion);
					logger::error("{}", errorStr);
					return std::make_unique<InvalidFunction>(errorStr);
				} else {
					auto function = std::make_unique<InvalidNonEssentialFunction>(errorStr, functionName, a_value);
					function->Initialize(&a_value);
					return std::move(function);
				}
			}

			auto errorStr = std::format("Function {} not found!", functionName);
			if (bEssential) {
				logger::error("{}", errorStr);
				return std::make_unique<InvalidFunction>(errorStr);
			} else {
				auto function = std::make_unique<InvalidNonEssentialFunction>(errorStr, functionName, a_value);
				function->Initialize(&a_value);
				return std::move(function);
			}
		}

		auto errorStr = "Function name not found!";
		logger::error("{}", errorStr);

		return std::make_unique<InvalidFunction>(errorStr);
	}

	std::unique_ptr<IFunction> CreateFunction(std::string_view a_functionName)
	{
		if (auto function = OpenAnimationReplacer::GetSingleton().CreateFunction(a_functionName)) {
			function->PreInitialize();
			function->PostInitialize();
			return std::move(function);
		}

		return std::make_unique<InvalidFunction>(a_functionName);
	}

	std::unique_ptr<IFunction> DuplicateFunction(const std::unique_ptr<IFunction>& a_functionToDuplicate)
	{
		// serialize function to json
		rapidjson::Document doc(rapidjson::kObjectType);

		rapidjson::Value serializedFunction(rapidjson::kObjectType);
		a_functionToDuplicate->Serialize(&serializedFunction, &doc.GetAllocator());

		// create new function from the serialized json
		return CreateFunctionFromJson(serializedFunction);
	}

	std::unique_ptr<FunctionSet> DuplicateFunctionSet(FunctionSet* a_functionSetToDuplicate)
	{
		// serialize the function set to json
		rapidjson::Document doc(rapidjson::kObjectType);

		rapidjson::Value serializedFunctionSet = a_functionSetToDuplicate->Serialize(doc.GetAllocator());

		// create new functions from the serialized json
		auto newFunctionSet = std::make_unique<FunctionSet>();

		for (auto& functionValue : serializedFunctionSet.GetArray()) {
			if (auto function = CreateFunctionFromJson(functionValue)) {
				newFunctionSet->Add(function);
			}
		}

		return newFunctionSet;
	}

	void InvalidNonEssentialFunction::Serialize(void* a_value, void* a_allocator, IFunction*)
	{
		auto& value = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		// just serialize back the json that was read
		value.CopyFrom(_json, allocator);
	}

	RE::BSString InvalidNonEssentialFunction::GetName() const
	{
		std::string ret = "[Missing] " + _name;
		return ret.data();
	}

	RE::BSString InvalidNonEssentialFunction::GetDescription() const
	{
		switch (GetEssential()) {
		case EssentialState::kNonEssential_True:
			return "The function was not found, however it was marked as non-essential by the replacer mod author. It will be treated as if it ran successfully."sv.data();
		case EssentialState::kNonEssential_False:
			return "The function was not found, however it was marked as non-essential by the replacer mod author. It will be treated as if it failed to run."sv.data();
		}
		return "The function was not found!"sv.data();
	}

	bool InvalidNonEssentialFunction::RunImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, void*, Trigger*) const
	{
		return GetEssential() == EssentialState::kNonEssential_True;
	}

	bool CONDITIONFunction::RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger) const
	{
		return conditionComponent->TryRun(a_refr, a_clipGenerator, a_parentSubMod, a_trigger);
	}

	void RANDOMFunctionComponent::InitializeComponent(void* a_value)
	{
		MultiFunctionComponent::InitializeComponent(a_value);

		WriteLocker locker(_lock);

		UpdateWeightCache();

		auto& value = *static_cast<rapidjson::Value*>(a_value);
		const auto object = value.GetObj();
		if (const auto weightsIt = object.FindMember("weights"); weightsIt != object.MemberEnd() && weightsIt->value.IsArray()) {
			size_t i = 0;
			for (const auto weightsArray = weightsIt->value.GetArray(); auto& weightValue : weightsArray) {
				if (i < weights.size()) {
					weights[i] = weightValue.GetFloat();
				}
				++i;
			}
		}
	}

	void RANDOMFunctionComponent::SerializeComponent(void* a_value, void* a_allocator)
	{
		MultiFunctionComponent::SerializeComponent(a_value, a_allocator);

		auto& value = *static_cast<rapidjson::Value*>(a_value);
		auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(a_allocator);

		rapidjson::Value weightArrayValue(rapidjson::kArrayType);

		ReadLocker locker(_lock);
		for (auto& weight : weights) {
			weightArrayValue.PushBack(weight, allocator);
		}

		value.AddMember("weights", weightArrayValue, allocator);
	}

	bool RANDOMFunctionComponent::DisplayInUI(bool a_bEditable, float a_firstColumnWidthPercent)
	{
		bool bSetDirty = false;

		{
			ReadLocker locker(_lock);

			size_t i = 0;
			functionSet->ForEach([&](const std::unique_ptr<Functions::IFunction>& a_function) {
				ImGui::TextUnformatted(a_function->GetName().c_str());
				UI::UICommon::SecondColumn(a_firstColumnWidthPercent);
				if (a_bEditable) {
					std::string label = std::format("Weight##{}", reinterpret_cast<uintptr_t>(a_function.get()));
					ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
					if (ImGui::InputFloat(label.c_str(), &weights[i], .01f, 1.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue)) {
						bSetDirty = true;
					}
				} else {
					std::string text = std::format("Weight: {}", weights[i]);
					ImGui::TextUnformatted(text.data());
				}

				ImGui::SameLine();
				UI::UICommon::HelpMarker("The weight of this function used for the weighted random selection (e.g. a function with a weight of 2 will be twice as likely to be picked than a function with a weight of 1)");
				
				++i;
				return RE::BSVisit::BSVisitControl::kContinue;
			});
		}
		
		return bSetDirty;
	}

	bool RANDOMFunctionComponent::Run(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, void* a_trigger) const
	{
		ReadLocker locker(_lock);

		bool bRanFunction = false;

		float random = Utils::GetRandomFloat(0.f, _cumulativeWeights.back());
		size_t index = std::distance(_cumulativeWeights.begin(), std::lower_bound(_cumulativeWeights.begin(), _cumulativeWeights.end(), random));

		size_t i = 0;
		functionSet->ForEach([&](const std::unique_ptr<Functions::IFunction>& a_function) {
			if (i == index) {
				if (a_function->Run(a_refr, a_clipGenerator, a_parentSubMod, static_cast<Trigger*>(a_trigger))) {
					bRanFunction = true;
				}
				return RE::BSVisit::BSVisitControl::kStop;
			}
			++i;
			return RE::BSVisit::BSVisitControl::kContinue;
		});

		return bRanFunction;
	}

	void RANDOMFunctionComponent::UpdateWeightCache()
	{
		WriteLocker locker(_lock);

		// For each function in the set (in its current order), we check if its pointer is
		// already in _functions. If it is, we keep its associated weight.
		// If not, we assign a default weight.
		// Any entries for objects no longer in the container are discarded.
		std::vector<IFunction*> newPointers;
		std::vector<float> newWeights;

		functionSet->ForEach([&](const std::unique_ptr<Functions::IFunction>& a_function) {
			IFunction* function = a_function.get();
			auto it = std::find(_functions.begin(), _functions.end(), function);
			if (it != _functions.end()) {
				size_t idx = std::distance(_functions.begin(), it);
				newPointers.push_back(function);
				newWeights.push_back(weights[idx]);
			} else {
				// new - assign default weight
				newPointers.push_back(function);
				newWeights.push_back(1.f);
			}
			return RE::BSVisit::BSVisitControl::kContinue;
		});

		_functions = std::move(newPointers);
		weights = std::move(newWeights);

		// rebuild cumulative weights
		_cumulativeWeights.clear();
		_cumulativeWeights.reserve(weights.size());

		float sum = 0.f;
		for (float weight : weights) {
			sum += weight;
			_cumulativeWeights.push_back(sum);
		}
	}

	bool RANDOMFunction::RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger) const
	{
		return randomFunctionComponent->Run(a_refr, a_clipGenerator, a_parentSubMod, a_trigger);
	}

	bool ONEFunction::RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator* a_clipGenerator, void* a_parentSubMod, Trigger* a_trigger) const
	{
		bool bRanFunction = false;

		multiFunctionComponent->ForEachFunction([&](const std::unique_ptr<Functions::IFunction>& a_function) {
			if (a_function->Run(a_refr, a_clipGenerator, a_parentSubMod, static_cast<Trigger*>(a_trigger))) {
				bRanFunction = true;
				return RE::BSVisit::BSVisitControl::kStop;
			}
			return RE::BSVisit::BSVisitControl::kContinue;
		});

		return bRanFunction;
	}

	bool PlaySoundFunction::RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, void*, Trigger*) const
	{
		if (const auto form = formComponent->GetTESFormValue()) {
			if (const auto descriptor = form->As<RE::BGSSoundDescriptorForm>()) {
				RE::BSSoundHandle handle;
				const auto audioManager = RE::BSAudioManager::GetSingleton();
				audioManager->BuildSoundDataFromDescriptor(handle, descriptor);
				handle.SetObjectToFollow(a_refr->Get3D());
				handle.Play();

				return true;
			}
		}

		return false;
	}

	bool ModActorValueFunction::RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, void*, Trigger*) const
	{
		if (a_refr) {
			if (const auto actor = a_refr->As<RE::Actor>()) {
				const auto actorValueOwner = actor->AsActorValueOwner();
				const auto actorValue = actorValueComponent->GetActorValue();

				if (actorValue > RE::ActorValue::kNone && actorValue < RE::ActorValue::kTotal) {
					actorValueOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, actorValue, valueToWriteComponent->GetNumericValue(a_refr));
					return true;
				}
			}
		}

		return false;
	}

	bool SetGraphVariableFunction::RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, void*, Trigger*) const
	{
		if (a_refr) {
			switch (graphVariableComponent->GetGraphVariableType()) {
			case Components::GraphVariableType::kFloat:
				a_refr->SetGraphVariableFloat(graphVariableComponent->GetGraphVariableName(), valueToWriteComponent->GetNumericValue(a_refr));
				return true;

			case Components::GraphVariableType::kInt:
				a_refr->SetGraphVariableInt(graphVariableComponent->GetGraphVariableName(), static_cast<int32_t>(valueToWriteComponent->GetNumericValue(a_refr)));
				return true;

			case Components::GraphVariableType::kBool:
				a_refr->SetGraphVariableBool(graphVariableComponent->GetGraphVariableName(), static_cast<bool>(valueToWriteComponent->GetNumericValue(a_refr)));
				return true;
			}
		}

		return false;
	}

	RE::BSString SendAnimEventFunction::GetArgument() const
	{
		return eventNameComponent->GetTextValue();
	}

	bool SendAnimEventFunction::RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, void*, Trigger*) const
	{
		OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::NotifyAnimationGraphJob>(a_refr, eventNameComponent->GetTextValue());

		return true;
	}

	bool CastSpellFunction::RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, void*, Trigger*) const
	{
		if (a_refr) {
			if (auto actor = a_refr->As<RE::Actor>()) {
				if (auto magicCaster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
					if (const auto form = formComponent->GetTESFormValue()) {
						if (const auto spellItem = form->As<RE::SpellItem>()) {
							magicCaster->CastSpellImmediate(spellItem, false, selfTarget->GetBoolValue() ? actor : nullptr, effectiveness->GetNumericValue(actor), false, magnitude->GetNumericValue(actor), actor);

							return true;
						}
					}
				}
			}
		}

		return false;
	}

	bool DispelSpellFunction::RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, void*, Trigger*) const
	{
		if (a_refr) {
			if (auto actor = a_refr->As<RE::Actor>()) {
				if (const auto form = formComponent->GetTESFormValue()) {
					if (const auto magicItem = form->As<RE::MagicItem>()) {
						auto magicTarget = actor->AsMagicTarget();
						RE::BSPointerHandle<RE::Actor> ptr;
						magicTarget->DispelEffect(magicItem, ptr);

						return true;
					}
				}

			}
		}

		return false;
	}

	bool SpawnParticleFunction::RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, void*, Trigger*) const
	{
		if (a_refr) {
			if (auto cell = a_refr->GetParentCell()) {
				RE::NiAVObject* actorNode = nullptr;
				if (auto actor = a_refr->As<RE::Actor>()) {
					const auto actorNodeIndexValue = static_cast<RE::BIPED_OBJECT>(actorNodeIndex->GetNumericValue(a_refr));
					if (actorNodeIndexValue < RE::BIPED_OBJECT::kTotal) {
						actorNode = actor->GetCurrentBiped()->objects[actorNodeIndexValue].partClone.get();
					}
				}

				RE::NiMatrix3 rotation;
				RE::NiPoint3 position;
				if (actorNode) {
					rotation = actorNode->world.rotate;
					position = actorNode->worldBound.center + Utils::TransformVectorByMatrix(offset->GetNiPoint3Value(), rotation);
				} else {
					rotation = a_refr->data.angle;
					position = a_refr->data.location + Utils::TransformVectorByMatrix(offset->GetNiPoint3Value(), rotation);
				}

				constexpr uint32_t flags = 7;

				RE::BSTempEffectParticle::Spawn(cell, playTime->GetNumericValue(a_refr), path->GetTextValue().c_str(), rotation, position, scale->GetNumericValue(a_refr), flags, actorNode);
				return true;
			}
		}

		return false;
	}

	void UnequipSlotFunction::PostInitialize()
	{
		FunctionBase::PostInitialize();
		slotComponent->value.getEnumMap = &UnequipSlotFunction::GetEnumMap;
	}

	RE::BSString UnequipSlotFunction::GetArgument() const
	{
		std::string slotName = "(Invalid)";
		const auto slot = static_cast<uint32_t>(slotComponent->GetNumericValue(nullptr));

		static auto map = GetEnumMap();
		if (const auto it = map.find(slot); it != map.end()) {
			slotName = it->second;
		}

		return slotName.data();
	}

	bool UnequipSlotFunction::RunImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, void*, Trigger*) const
	{
		if (a_refr) {
			if (const auto actor = a_refr->As<RE::Actor>()) {
				const auto slot = static_cast<RE::BIPED_MODEL::BipedObjectSlot>(1 << static_cast<uint32_t>(slotComponent->GetNumericValue(a_refr)));
				if (const auto armor = actor->GetWornArmor(slot, true)) {
					auto actorEquipManager = RE::ActorEquipManager::GetSingleton();
					return actorEquipManager->UnequipObject(actor, armor);
				}
			}
		}

		return false;
	}

	std::map<int32_t, std::string_view> UnequipSlotFunction::GetEnumMap()
	{
		std::map<int32_t, std::string_view> enumMap;

		enumMap[0] = "Head"sv;
		enumMap[1] = "Hair"sv;
		enumMap[2] = "Body"sv;
		enumMap[3] = "Hands"sv;
		enumMap[4] = "Forearms"sv;
		enumMap[5] = "Amulet"sv;
		enumMap[6] = "Ring"sv;
		enumMap[7] = "Feet"sv;
		enumMap[8] = "Calves"sv;
		enumMap[9] = "Shield"sv;
		enumMap[10] = "Tail"sv;
		enumMap[11] = "LongHair"sv;
		enumMap[12] = "Circlet"sv;
		enumMap[13] = "Ears"sv;
		enumMap[14] = "ModMouth"sv;
		enumMap[15] = "ModNeck"sv;
		enumMap[16] = "ModChestPrimary"sv;
		enumMap[17] = "ModBack"sv;
		enumMap[18] = "ModMisc1"sv;
		enumMap[19] = "ModPelvisPrimary"sv;
		enumMap[20] = "DecapitateHead"sv;
		enumMap[21] = "Decapitate"sv;
		enumMap[22] = "ModPelvisSecondary"sv;
		enumMap[23] = "ModLegRight"sv;
		enumMap[24] = "ModLegLeft"sv;
		enumMap[25] = "ModFaceJewelry"sv;
		enumMap[26] = "ModChestSecondary"sv;
		enumMap[27] = "ModShoulder"sv;
		enumMap[28] = "ModArmLeft"sv;
		enumMap[29] = "ModArmRight"sv;
		enumMap[30] = "ModMisc2"sv;
		enumMap[31] = "FX01"sv;

		return enumMap;
	}
}
