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
	// Save existing overrides (by index and by name for fallback)
	TArray<FPCGExPropertyOverrideEntry> OldOverrides = MoveTemp(Overrides);

	// Build map by name for fallback matching
	TMap<FName, int32> OldIndexByName;
	for (int32 i = 0; i < OldOverrides.Num(); ++i)
	{
		FName PropName = OldOverrides[i].GetPropertyName();
		if (!PropName.IsNone())
		{
			OldIndexByName.Add(PropName, i);
		}
	}

	// Rebuild array to match schema exactly (parallel arrays)
	Overrides.Reset(Schema.Num());

	for (int32 SchemaIndex = 0; SchemaIndex < Schema.Num(); ++SchemaIndex)
	{
		const FInstancedStruct& SchemaProp = Schema[SchemaIndex];
		const FPCGExPropertyCompiled* SchemaData = SchemaProp.GetPtr<FPCGExPropertyCompiled>();
		if (!SchemaData) { continue; }

		FPCGExPropertyOverrideEntry& NewEntry = Overrides.AddDefaulted_GetRef();

		FPCGExPropertyOverrideEntry* Existing = nullptr;

		// Strategy 1: Match by index (handles renames - same position = same logical property)
		if (OldOverrides.IsValidIndex(SchemaIndex))
		{
			FPCGExPropertyOverrideEntry& OldEntry = OldOverrides[SchemaIndex];
			// Only use index match if type matches (otherwise it's a different property now)
			if (OldEntry.Value.GetScriptStruct() == SchemaProp.GetScriptStruct())
			{
				Existing = &OldEntry;
			}
		}

		// Strategy 2: Fallback to match by name (handles reordering/insertion)
		if (!Existing)
		{
			if (int32* OldIndex = OldIndexByName.Find(SchemaData->PropertyName))
			{
				Existing = &OldOverrides[*OldIndex];
			}
		}

		if (Existing)
		{
			// Preserve enabled state
			NewEntry.bEnabled = Existing->bEnabled;

			// Check if type matches
			if (Existing->Value.GetScriptStruct() == SchemaProp.GetScriptStruct())
			{
				// Same type - keep existing value
				NewEntry.Value = MoveTemp(Existing->Value);

				// CRITICAL: Ensure PropertyName stays synced with schema (handles renames)
				if (FPCGExPropertyCompiled* Prop = NewEntry.GetPropertyMutable())
				{
					Prop->PropertyName = SchemaData->PropertyName;
				}
			}
			else
			{
				// Type changed - use schema default (keep enabled state)
				NewEntry.Value = SchemaProp;
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
