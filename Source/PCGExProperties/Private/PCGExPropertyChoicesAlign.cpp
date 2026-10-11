// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPropertyChoicesAlign.h"

#if WITH_EDITOR

#include "PCGExProperty.h"
#include "PCGExPropertyRowHost.h"
#include "PCGExPropertyRowMatching.h"
#include "PCGExPropertySchemaAsset.h"

namespace PCGExPropertyChoicesAlign
{
	const FPCGExPropertyResolved* FindByName(const TConstArrayView<FPCGExPropertyResolved> Resolved, const FName PropertyName)
	{
		return Resolved.FindByPredicate([PropertyName](const FPCGExPropertyResolved& Entry) { return Entry.Source->Name == PropertyName; });
	}

	/** The choice rows should land on: the one Default stands on, else the first that fits Declaration. */
	const FPCGExPropertyChoice* PickTarget(const FConstStructView Default, const FConstStructView Declaration, const FPCGExPropertyChoices& Choices)
	{
		const int32 OnDefault = PCGExProperties::ResolveSelectedChoice(Default, Choices);
		if (OnDefault != INDEX_NONE)
		{
			return &Choices.Items[OnDefault];
		}
		return Choices.Items.FindByPredicate([&Declaration](const FPCGExPropertyChoice& Choice)
		{
			return PCGExProperties::IsChoiceCompatible(Declaration, Choice);
		});
	}

	FResult AlignHostRows(UObject* Host, const FName PropertyName, const UPCGExPropertySchemaAsset* DeclaringAsset)
	{
		FResult Result;
		IPCGExPropertyRowHost* RowHost = Cast<IPCGExPropertyRowHost>(Host);
		const FPCGExPropertySchemaCollection* Schema = RowHost ? RowHost->EDITOR_GetRowSchema() : nullptr;
		if (!Schema || PropertyName.IsNone())
		{
			return Result;
		}

		IPCGExPropertyRowHost::FLayerChain Chain;
		RowHost->EDITOR_GetRowSchemaFallbackChain(Chain);

		// Names are unique in a resolved list, so a host that shadows the declaration resolves the name to its own.
		TArray<FPCGExPropertyResolved> Resolved;
		Schema->Resolve(Resolved, MakeArrayView(Chain));
		const FPCGExPropertyResolved* Found = FindByName(Resolved, PropertyName);
		if (!Found || (DeclaringAsset && Found->OwningAsset != DeclaringAsset))
		{
			return Result;
		}

		FPCGExPropertyChoices Choices;
		Found->GetEffectiveChoices(Choices);
		if (Choices.Items.IsEmpty())
		{
			return Result;
		}
		Result.HostsVisited = 1;

		const FInstancedStruct& Declaration = Found->Source->Property;
		const int32 HeaderId = Found->Source->HeaderId;

		// Enabled rows of this property that stand on none of its choices and that Target can replace.
		TArray<FPCGExPropertyOverrideEntry*, TInlineAllocator<16>> Pending;
		auto Gather = [&](FPCGExPropertyOverrides& Rows, const FPCGExPropertyChoice& Target)
		{
			for (int32 i = 0; i < Rows.Overrides.Num(); ++i)
			{
				FPCGExPropertyOverrideEntry& Row = Rows.Overrides[i];
				if (!Row.bEnabled || Row.Value.GetScriptStruct() != Declaration.GetScriptStruct())
				{
					continue;
				}
				// Cheap filter first; the shared matcher then decides, as it does for every row sync.
				const bool bCandidate = Row.GetPropertyName() == PropertyName || (HeaderId != 0 && Row.HeaderId == HeaderId);
				if (!bCandidate || PCGExPropertyOverridesSync::FindProperty(Rows.Overrides, i, Resolved) != Found)
				{
					continue;
				}
				if (PCGExProperties::ResolveSelectedChoice(Row.Value, Choices) == INDEX_NONE && PCGExProperties::IsChoiceCompatible(Row.Value, Target))
				{
					Pending.Add(&Row);
				}
			}
		};

		auto Apply = [&](const FPCGExPropertyChoice& Target)
		{
			if (Pending.IsEmpty())
			{
				return;
			}
			if (Result.RowsAligned == 0)
			{
				Host->Modify();
			}
			for (FPCGExPropertyOverrideEntry* Row : Pending)
			{
				Result.RowsAligned += PCGExProperties::ApplyChoice(Row->Value, Target) ? 1 : 0;
			}
			Pending.Reset();
		};

		// The schema's own import layer first: its row is the default every other row falls back to, and it falls
		// back itself to what the schema resolves to without it.
		if (Found->OwningAsset)
		{
			TArray<FPCGExPropertyResolved> Inherited;
			Schema->Resolve(Inherited, MakeArrayView(Chain), /*bIncludeOwnOverrides=*/false);
			const FPCGExPropertyResolved* InheritedEntry = FindByName(Inherited, PropertyName);
			const FPCGExPropertyChoice* Target = InheritedEntry && InheritedEntry->OwningAsset == Found->OwningAsset
				? PickTarget(InheritedEntry->GetEffectiveProperty(), Declaration, Choices)
				: nullptr;
			if (Target)
			{
				RowHost->EDITOR_ForEachPropertyRows([&](FPCGExPropertyOverrides& Rows, const bool bIsImportLayer)
				{
					if (bIsImportLayer)
					{
						Gather(Rows, *Target);
					}
				});
				Apply(*Target);
			}
		}

		// Found points into the layer just written, so this default is the aligned one.
		if (const FPCGExPropertyChoice* Target = PickTarget(Found->GetEffectiveProperty(), Declaration, Choices))
		{
			RowHost->EDITOR_ForEachPropertyRows([&](FPCGExPropertyOverrides& Rows, const bool bIsImportLayer)
			{
				if (!bIsImportLayer)
				{
					Gather(Rows, *Target);
				}
			});
			Apply(*Target);
		}

		if (Result.RowsAligned > 0)
		{
			RowHost->EDITOR_NotifyRowsChanged();
			Result.HostsTouched = 1;
		}
		return Result;
	}

	FResult AlignImporters(UPCGExPropertySchemaAsset* Through, const FName PropertyName, const UPCGExPropertySchemaAsset* DeclaringAsset)
	{
		FResult Result;
		if (!Through)
		{
			return Result;
		}

		TArray<TWeakObjectPtr<UObject>> Hosts;
		PCGExPropertyRowHost::GetLoadedImporters(Through, Hosts);
		for (const TWeakObjectPtr<UObject>& Host : Hosts)
		{
			// Weak: a host's notify can run anything, including what destroys a later host.
			if (UObject* Live = Host.Get())
			{
				Result += AlignHostRows(Live, PropertyName, DeclaringAsset ? DeclaringAsset : Through);
			}
		}
		return Result;
	}
}

#endif
