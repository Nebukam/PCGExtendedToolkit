// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExValencyMap.h"

#include "PCGParamData.h"
#include "Core/PCGExValencyBondingRules.h"
#include "Core/PCGExValencyCommon.h"
#include "Collections/PCGExMeshCollection.h"
#include "Collections/PCGExActorCollection.h"
#include "Helpers/PCGExStreamingHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

namespace PCGExValency
{
#pragma region FValencyPacker

	FValencyPacker::FValencyPacker(FPCGContext* InContext)
	{
		BaseHash = static_cast<uint16>(InContext->GetInputSettings<UPCGSettings>()->UID);
	}

	uint64 FValencyPacker::GetEntryIdx(const UPCGExValencyBondingRules* InBondingRules, uint16 InModuleIndex, uint16 InPatternFlags)
	{
		const uint32 ItemHash = PCGEx::H32(InModuleIndex, InPatternFlags);

		{
			FReadScopeLock ReadScopeLock(BondingRulesLock);
			if (const uint32* RulesIdxPtr = BondingRulesMap.Find(InBondingRules))
			{
				return PCGEx::H64(*RulesIdxPtr, ItemHash);
			}
		}

		{
			FWriteScopeLock WriteScopeLock(BondingRulesLock);
			if (const uint32* RulesIdxPtr = BondingRulesMap.Find(InBondingRules))
			{
				return PCGEx::H64(*RulesIdxPtr, ItemHash);
			}

			uint32 RulesIndex = PCGEx::H32(BaseHash, BondingRulesArray.Add(InBondingRules));
			BondingRulesMap.Add(InBondingRules, RulesIndex);
			return PCGEx::H64(RulesIndex, ItemHash);
		}
	}

	void FValencyPacker::PackToDataset(const UPCGParamData* InAttributeSet)
	{
		FPCGMetadataAttribute<int32>* RulesIdx = InAttributeSet->Metadata->FindOrCreateAttribute<int32>(
			Labels::Tag_ValencyMapIdx, 0,
			false, true, true);
		FPCGMetadataAttribute<FSoftObjectPath>* RulesPath = InAttributeSet->Metadata->FindOrCreateAttribute<FSoftObjectPath>(
			Labels::Tag_ValencyRulesPath, FSoftObjectPath(),
			false, true, true);

		for (const TPair<const UPCGExValencyBondingRules*, uint32>& Pair : BondingRulesMap)
		{
			const int64 Key = InAttributeSet->Metadata->AddEntry();
			RulesIdx->SetValue(Key, Pair.Value);
			RulesPath->SetValue(Key, FSoftObjectPath(Pair.Key));
		}
	}

#pragma endregion

#pragma region FValencyUnpacker

	FValencyUnpacker::~FValencyUnpacker()
	{
		PCGExHelpers::SafeReleaseHandle(BondingRulesHandle);
	}

	bool FValencyUnpacker::UnpackDataset(FPCGContext* InContext, const UPCGParamData* InAttributeSet)
	{
		const UPCGMetadata* Metadata = InAttributeSet->Metadata;
		TUniquePtr<FPCGAttributeAccessorKeysEntries> Keys = MakeUnique<FPCGAttributeAccessorKeysEntries>(Metadata);

		const int32 NumEntries = Keys->GetNum();
		if (NumEntries == 0)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Valency map is empty."));
			return false;
		}

		BondingRulesMap.Reserve(BondingRulesMap.Num() + NumEntries);

		const FPCGMetadataAttribute<int32>* RulesIdx = InAttributeSet->Metadata->GetConstTypedAttribute<int32>(Labels::Tag_ValencyMapIdx);
		const FPCGMetadataAttribute<FSoftObjectPath>* RulesPath = InAttributeSet->Metadata->GetConstTypedAttribute<FSoftObjectPath>(Labels::Tag_ValencyRulesPath);

		if (!RulesIdx || !RulesPath)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Valency map missing required attributes."));
			return false;
		}

		// Load all referenced bonding rules
		{
			TSharedPtr<TSet<FSoftObjectPath>> RulesPaths = MakeShared<TSet<FSoftObjectPath>>();
			for (int32 i = 0; i < NumEntries; i++)
			{
				RulesPaths->Add(RulesPath->GetValueFromItemKey(i));
			}
			BondingRulesHandle = PCGExHelpers::LoadBlocking_AnyThread(RulesPaths);
		}

		for (int32 i = 0; i < NumEntries; i++)
		{
			int32 Idx = RulesIdx->GetValueFromItemKey(i);

			TSoftObjectPtr<UPCGExValencyBondingRules> RulesSoftPtr(RulesPath->GetValueFromItemKey(i));
			UPCGExValencyBondingRules* Rules = RulesSoftPtr.Get();

			if (!Rules)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Some bonding rules could not be loaded."));
				return false;
			}

			if (BondingRulesMap.Contains(Idx))
			{
				if (BondingRulesMap[Idx] == Rules) { continue; }

				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Valency map index collision."));
				return false;
			}

			// Ensure compiled
			if (!Rules->IsCompiled())
			{
				if (!Rules->Compile())
				{
					PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Failed to compile bonding rules from Valency Map."));
					return false;
				}
			}

			BondingRulesMap.Add(Idx, Rules);
		}

		return true;
	}

	void FValencyUnpacker::UnpackPin(FPCGContext* InContext, const FName InPinLabel)
	{
		for (TArray<FPCGTaggedData> Params = InContext->InputData.GetParamsByPin(InPinLabel.IsNone() ? Labels::SourceValencyMapLabel : InPinLabel);
			const FPCGTaggedData& InTaggedData : Params)
		{
			const UPCGParamData* ParamData = Cast<UPCGParamData>(InTaggedData.Data);

			if (!ParamData) { continue; }

			if (!ParamData->Metadata->HasAttribute(Labels::Tag_ValencyMapIdx) || !ParamData->Metadata->HasAttribute(Labels::Tag_ValencyRulesPath))
			{
				continue;
			}

			UnpackDataset(InContext, ParamData);
		}
	}

	UPCGExValencyBondingRules* FValencyUnpacker::ResolveEntry(uint64 EntryHash, uint16& OutModuleIndex, uint16& OutPatternFlags)
	{
		if (!EntryData::IsValid(EntryHash))
		{
			OutModuleIndex = MAX_uint16;
			OutPatternFlags = EntryData::Flags::None;
			return nullptr;
		}

		const uint32 RulesMapId = EntryData::GetBondingRulesMapId(EntryHash);
		OutModuleIndex = static_cast<uint16>(EntryData::GetModuleIndex(EntryHash));
		OutPatternFlags = EntryData::GetPatternFlags(EntryHash);

		UPCGExValencyBondingRules** Rules = BondingRulesMap.Find(RulesMapId);
		if (!Rules) { return nullptr; }

		return *Rules;
	}

	UPCGExMeshCollection* FValencyUnpacker::GetMeshCollection(const UPCGExValencyBondingRules* InBondingRules)
	{
		if (!InBondingRules) { return nullptr; }
		UPCGExMeshCollection* Collection = InBondingRules->GetMeshCollection();
		if (Collection) { Collection->BuildCache(); }
		return Collection;
	}

	UPCGExActorCollection* FValencyUnpacker::GetActorCollection(const UPCGExValencyBondingRules* InBondingRules)
	{
		if (!InBondingRules) { return nullptr; }
		UPCGExActorCollection* Collection = InBondingRules->GetActorCollection();
		if (Collection) { Collection->BuildCache(); }
		return Collection;
	}

#pragma endregion
}
