// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#define PCGEX_REGISTER_CUSTO_START FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
#define PCGEX_REGISTER_CUSTO(_NAME, _CLASS) PropertyModule.RegisterCustomPropertyTypeLayout(_NAME, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&_CLASS::MakeInstance));


#define PCGEX_ASSET_TYPE_ACTION_BASIC(_SHORTNAME, _NAME, _CLASS, _COLOR, _CATEGORIES)\
class FPCGEx##_SHORTNAME##Actions : public FAssetTypeActions_Base{public:\
	virtual FText GetName() const override{return INVTEXT(_NAME);}\
	virtual FString GetObjectDisplayName(UObject* Object) const override{return Object->GetName();}\
	virtual UClass* GetSupportedClass() const override{return _CLASS::StaticClass();}\
	virtual FColor GetTypeColor() const override{return _COLOR;}\
	virtual uint32 GetCategories() override{return _CATEGORIES;}\
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override{return false;}};\
FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(MakeShared<FPCGEx##_SHORTNAME##Actions>());
