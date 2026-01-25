// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPropertiesEditor.h"

#include "PropertyEditorModule.h"
#include "Details/PCGExPropertyOverridesCustomization.h"
#include "Details/PCGExPropertyOverrideEntryCustomization.h"
#include "Details/PCGExPropertyCompiledCustomization.h"
#include "Details/PCGExPropertySchemaCollectionCustomization.h"
#include "Details/PCGExPropertySchemaCustomization.h"
#include "PCGExPropertyCompiled.h"
#include "PCGExPropertyTypes.h"

#define LOCTEXT_NAMESPACE "FPCGExPropertiesEditorModule"

void FPCGExPropertiesEditorModule::StartupModule()
{
	IPCGExEditorModuleInterface::StartupModule();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Register FPCGExPropertySchemaCollection customization - handles schema array changes
	// Used by Tuple (Composition), Collections (CollectionProperties), Valency (DefaultProperties)
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FPCGExPropertySchemaCollection::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGExPropertySchemaCollectionCustomization::MakeInstance)
	);

	// Register FPCGExPropertySchema customization - handles individual schema entry changes
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FPCGExPropertySchema::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGExPropertySchemaCustomization::MakeInstance)
	);

	// Register FPCGExPropertyOverrides customization - provides toggle-checkbox UI
	// Used by Collections (entry overrides) and Tuple (row values)
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FPCGExPropertyOverrides::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGExPropertyOverridesCustomization::MakeInstance)
	);

	// Register FPCGExPropertyOverrideEntry customization - handles individual entry display
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FPCGExPropertyOverrideEntry::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGExPropertyOverrideEntryCustomization::MakeInstance)
	);

	// Register FPCGExPropertyCompiled customization for all 17 concrete types
	// This hides PropertyName field (shown in entry header) and only shows value fields
	#define REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(TypeName) \
		PropertyModule.RegisterCustomPropertyTypeLayout( \
			FPCGExPropertyCompiled_##TypeName::StaticStruct()->GetFName(), \
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGExPropertyCompiledCustomization::MakeInstance) \
		);

	REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(Bool)
	REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(Int32)
	REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(Int64)
	REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(Float)
	REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(Double)
	REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(String)
	REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(Name)
	REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(Vector2)
	REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(Vector)
	REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(Vector4)
	REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(Color)
	REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(Rotator)
	REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(Quat)
	REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(Transform)
	REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(SoftObjectPath)
	REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(SoftClassPath)
	REGISTER_PROPERTY_COMPILED_CUSTOMIZATION(Enum)

	#undef REGISTER_PROPERTY_COMPILED_CUSTOMIZATION
}

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExPropertiesEditorModule, PCGExPropertiesEditor)
