// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExProperties.h"
#include "PCGExPropertyCompiled.h"

#if WITH_EDITOR
void FPCGExPropertiesModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle);
	// No special editor registration needed for properties module
}
#endif

PCGEX_IMPLEMENT_MODULE(FPCGExPropertiesModule, PCGExProperties)

#pragma region FPCGExPropertyOverrides

void FPCGExPropertyOverrides::SyncToSchema(const TArray<FInstancedStruct>& Schema)
{
	// Build map of existing overrides by name (to preserve enabled state)
	TMap<FName, FPCGExPropertyOverrideEntry> ExistingByName;
	for (FPCGExPropertyOverrideEntry& Entry : Overrides)
	{
		FName PropName = Entry.GetPropertyName();
		if (!PropName.IsNone())
		{
			ExistingByName.Add(PropName, MoveTemp(Entry));
		}
	}

	// Rebuild array to match schema exactly (parallel arrays)
	Overrides.Reset(Schema.Num());

	for (const FInstancedStruct& SchemaProp : Schema)
	{
		const FPCGExPropertyCompiled* SchemaData = SchemaProp.GetPtr<FPCGExPropertyCompiled>();
		if (!SchemaData) { continue; }

		FPCGExPropertyOverrideEntry& NewEntry = Overrides.AddDefaulted_GetRef();

		// Check if we had an existing override for this property
		if (FPCGExPropertyOverrideEntry* Existing = ExistingByName.Find(SchemaData->PropertyName))
		{
			// Preserve enabled state
			NewEntry.bEnabled = Existing->bEnabled;

			// Check if type matches
			if (Existing->Value.GetScriptStruct() == SchemaProp.GetScriptStruct())
			{
				// Same type - keep existing value
				NewEntry.Value = MoveTemp(Existing->Value);

				// CRITICAL: Ensure PropertyName stays synced with schema (user may have edited it in UI)
				if (FPCGExPropertyCompiled* Prop = NewEntry.GetPropertyMutable())
				{
					Prop->PropertyName = SchemaData->PropertyName;
				}
			}
			else
			{
				// Type changed - use schema default, disable override
				NewEntry.Value = SchemaProp;
				NewEntry.bEnabled = false;
			}
		}
		else
		{
			// New property - use schema default, disabled
			NewEntry.Value = SchemaProp;
			NewEntry.bEnabled = false;
		}
	}
}

const FInstancedStruct* FPCGExPropertyOverrides::GetOverride(FName PropertyName) const
{
	for (const FPCGExPropertyOverrideEntry& Entry : Overrides)
	{
		if (Entry.bEnabled && Entry.GetPropertyName() == PropertyName)
		{
			return &Entry.Value;
		}
	}
	return nullptr;
}

#pragma endregion
