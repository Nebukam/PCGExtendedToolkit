// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExProperties.h"
#include "PCGExPropertyCompiled.h"
#include "PCGExPropertyTypes.h"

#if WITH_EDITOR
void FPCGExPropertiesModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle);
	// No special editor registration needed for properties module
}
#endif

PCGEX_IMPLEMENT_MODULE(FPCGExPropertiesModule, PCGExProperties)

#pragma region FPCGExPropertySchema

FPCGExPropertySchema::FPCGExPropertySchema()
{
	#if WITH_EDITOR
	HeaderId = GetTypeHash(FGuid::NewGuid());
	Property.InitializeAs<FPCGExPropertyCompiled_Float>();
	#endif
}

#pragma endregion

#pragma region FPCGExPropertySchemaCollection

const FPCGExPropertySchema* FPCGExPropertySchemaCollection::FindByName(FName PropertyName) const
{
	if (PropertyName.IsNone()) { return nullptr; }

	for (const FPCGExPropertySchema& Schema : Schemas)
	{
		if (Schema.Name == PropertyName) { return &Schema; }
	}
	return nullptr;
}

TArray<FInstancedStruct> FPCGExPropertySchemaCollection::BuildSchema() const
{
	TArray<FInstancedStruct> Result;
	Result.Reserve(Schemas.Num());

	for (const FPCGExPropertySchema& Schema : Schemas)
	{
		if (Schema.IsValid())
		{
			Result.Add(Schema.Property);
		}
	}
	return Result;
}

bool FPCGExPropertySchemaCollection::ValidateUniqueNames(TArray<FName>& OutDuplicates) const
{
	OutDuplicates.Empty();
	TSet<FName> Seen;

	for (const FPCGExPropertySchema& Schema : Schemas)
	{
		if (Schema.Name.IsNone()) { continue; }

		bool bAlreadyInSet = false;
		Seen.Add(Schema.Name, &bAlreadyInSet);

		if (bAlreadyInSet)
		{
			OutDuplicates.AddUnique(Schema.Name);
		}
	}

	return OutDuplicates.IsEmpty();
}

#pragma endregion

#pragma region FPCGExPropertyOverrides

void FPCGExPropertyOverrides::SyncToSchema(const TArray<FInstancedStruct>& Schema)
{
	// Save existing overrides
	TArray<FPCGExPropertyOverrideEntry> OldOverrides = MoveTemp(Overrides);

	// Build map by HeaderId (stable identity)
	TMap<int32, FPCGExPropertyOverrideEntry> ExistingById;
	#if WITH_EDITOR
	for (FPCGExPropertyOverrideEntry& Entry : OldOverrides)
	{
		if (const FPCGExPropertyCompiled* Prop = Entry.Value.GetPtr<FPCGExPropertyCompiled>())
		{
			if (Prop->HeaderId != 0)
			{
				ExistingById.Add(Prop->HeaderId, MoveTemp(Entry));
			}
		}
	}
	#endif

	// Rebuild array to match schema exactly (parallel arrays)
	Overrides.Reset(Schema.Num());

	for (const FInstancedStruct& SchemaProp : Schema)
	{
		const FPCGExPropertyCompiled* SchemaData = SchemaProp.GetPtr<FPCGExPropertyCompiled>();
		if (!SchemaData) { continue; }

		FPCGExPropertyOverrideEntry& NewEntry = Overrides.AddDefaulted_GetRef();
		FPCGExPropertyOverrideEntry* Existing = nullptr;

		#if WITH_EDITOR
		// Match by HeaderId (stable across rename/reorder/type change!)
		if (SchemaData->HeaderId != 0)
		{
			Existing = ExistingById.Find(SchemaData->HeaderId);
		}
		#endif

		if (Existing)
		{
			// Found existing by HeaderId - preserve state
			NewEntry.bEnabled = Existing->bEnabled;

			if (Existing->Value.GetScriptStruct() == SchemaProp.GetScriptStruct())
			{
				// Same type - preserve value, update PropertyName from schema
				NewEntry.Value = MoveTemp(Existing->Value);

				if (FPCGExPropertyCompiled* Prop = NewEntry.GetPropertyMutable())
				{
					Prop->PropertyName = SchemaData->PropertyName;
				}
			}
			else
			{
				// Type changed - use schema default, preserve bEnabled
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
