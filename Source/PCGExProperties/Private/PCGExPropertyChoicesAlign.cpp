// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPropertyChoicesAlign.h"

#if WITH_EDITOR

#include "PCGExProperty.h"
#include "PCGExPropertyRowHost.h"
#include "PCGExPropertySchemaAsset.h"
#include "UObject/UObjectIterator.h"

namespace PCGExPropertyChoicesAlign
{
	FResult AlignHostRows(UObject* Host, const int32 HeaderId, const FName PropertyName)
	{
		FResult Result;
		IPCGExPropertyRowHost* RowHost = Cast<IPCGExPropertyRowHost>(Host);
		const FPCGExPropertySchemaCollection* Schema = RowHost ? RowHost->EDITOR_GetRowSchema() : nullptr;
		if (!Schema)
		{
			return Result;
		}

		TArray<FPCGExPropertyResolved> Resolved;
		Schema->Resolve(Resolved);
		const FPCGExPropertyResolved* Found = nullptr;
		for (const FPCGExPropertyResolved& Entry : Resolved)
		{
			if (Entry.Source && HeaderId != 0 && Entry.Source->HeaderId == HeaderId)
			{
				Found = &Entry;
				break;
			}
		}
		if (!Found && !PropertyName.IsNone())
		{
			for (const FPCGExPropertyResolved& Entry : Resolved)
			{
				if (Entry.Source && Entry.Source->Name == PropertyName)
				{
					Found = &Entry;
					break;
				}
			}
		}
		if (!Found)
		{
			return Result;
		}

		FPCGExPropertyChoices Choices;
		Found->GetEffectiveChoices(Choices);
		if (Choices.Items.IsEmpty())
		{
			return Result;
		}

		// The effective default when it is itself a choice, else the first one.
		int32 TargetIndex = PCGExProperties::FindMatchingChoice(FConstStructView(Found->GetEffectiveProperty()), Choices);
		if (TargetIndex == INDEX_NONE)
		{
			TargetIndex = 0;
		}
		const FPCGExPropertyChoice& Target = Choices.Items[TargetIndex];
		const FName Name = Found->Source->Name;
		const int32 Id = Found->Source->HeaderId;

		bool bModified = false;
		RowHost->EDITOR_ForEachPropertyRows([&](FPCGExPropertyOverrides& Row)
		{
			for (FPCGExPropertyOverrideEntry& Entry : Row.Overrides)
			{
				const bool bMatches = (Id != 0 && Entry.HeaderId == Id) || Entry.GetPropertyName() == Name;
				if (!bMatches || !Entry.bEnabled || !Entry.Value.IsValid())
				{
					continue;
				}
				if (PCGExProperties::ResolveSelectedChoice(FConstStructView(Entry.Value), Choices) != INDEX_NONE)
				{
					continue;
				}
				if (!bModified)
				{
					Host->Modify();
					bModified = true;
				}
				if (PCGExProperties::ApplyChoice(FStructView(Entry.Value), Target))
				{
					++Result.RowsAligned;
				}
			}
		});

		if (bModified)
		{
			RowHost->EDITOR_NotifyRowsAligned();
			Result.HostsTouched = 1;
		}
		return Result;
	}

	FResult AlignImporters(UPCGExPropertySchemaAsset* Asset, const int32 HeaderId, const FName PropertyName)
	{
		FResult Result;
		if (!Asset)
		{
			return Result;
		}
		for (TObjectIterator<UObject> It; It; ++It)
		{
			UObject* Object = *It;
			if (!IsValid(Object) || Object->IsTemplate() || !Object->GetClass()->ImplementsInterface(UPCGExPropertyRowHost::StaticClass()))
			{
				continue;
			}
			const IPCGExPropertyRowHost* RowHost = Cast<IPCGExPropertyRowHost>(Object);
			const FPCGExPropertySchemaCollection* Schema = RowHost ? RowHost->EDITOR_GetRowSchema() : nullptr;
			if (!Schema || !Schema->ImportsAssetTransitive(Asset))
			{
				continue;
			}
			Result += AlignHostRows(Object, HeaderId, PropertyName);
		}
		return Result;
	}
}

#endif
