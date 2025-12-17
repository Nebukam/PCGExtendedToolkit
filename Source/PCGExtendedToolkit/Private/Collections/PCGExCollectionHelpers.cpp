// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Collections/PCGExCollectionHelpers.h"

#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Collections/PCGExActorCollection.h"
#include "Collections/PCGExAssetCollection.h"
#include "Collections/PCGExMeshCollection.h"
#include "Collections/PCGExPCGDataAssetCollection.h"
#include "Engine/World.h"

#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Types/PCGExAttributeIdentity.h"

namespace PCGExCollectionHelpers
{
	bool BuildFromAttributeSet(UPCGExAssetCollection* InCollection, FPCGExContext* InContext, const UPCGParamData* InAttributeSet, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging)
	{
		const UPCGMetadata* Metadata = InAttributeSet->Metadata;

		TUniquePtr<FPCGAttributeAccessorKeysEntries> Keys = MakeUnique<FPCGAttributeAccessorKeysEntries>(Metadata);

		const int32 NumEntries = Keys->GetNum();
		if (NumEntries <= 0)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Attribute set is empty."));
			return false;
		}

		const TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(Metadata);
		if (Infos->Attributes.IsEmpty()) { return false; }

#define PCGEX_PROCESS_ATTRIBUTE(_TYPE, _NAME) \
	TArray<_TYPE> _NAME##Values;\
	bool b##_NAME = false;\
	if (const PCGEx::FAttributeIdentity* _NAME = Infos->Find(Details._NAME)){ \
		if (TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(Infos->Attributes[Infos->Map[_NAME->Identifier]], Metadata)){ \
			PCGEx::InitArray(_NAME##Values, NumEntries); \
			b##_NAME = Accessor->GetRange<_TYPE>(_NAME##Values, 0, *Keys.Get(), EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible);\
		}\
	}

		InCollection->InitNumEntries(NumEntries);

		PCGEX_PROCESS_ATTRIBUTE(FSoftObjectPath, AssetPathSourceAttribute)
		PCGEX_PROCESS_ATTRIBUTE(double, WeightSourceAttribute)
		PCGEX_PROCESS_ATTRIBUTE(FName, CategorySourceAttribute)

		InCollection->ForEachEntry([&](FPCGExAssetCollectionEntry* InEntry, const int32 i)
		{
			if (bAssetPathSourceAttribute) { InEntry->SetAssetPath(AssetPathSourceAttributeValues[i]); }
			if (bWeightSourceAttribute) { InEntry->Weight = WeightSourceAttributeValues[i]; }
			if (bCategorySourceAttribute) { InEntry->Category = CategorySourceAttributeValues[i]; }
		});

		if (bBuildStaging) { InCollection->RebuildStagingData(false); }

		return true;
	}

	bool BuildFromAttributeSet(UPCGExAssetCollection* InCollection, FPCGExContext* InContext, const FName InputPin, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging)
	{
		const TArray<FPCGTaggedData> Inputs = InContext->InputData.GetInputsByPin(InputPin);
		if (Inputs.IsEmpty()) { return false; }
		for (const FPCGTaggedData& InData : Inputs)
		{
			if (const UPCGParamData* ParamData = Cast<UPCGParamData>(InData.Data))
			{
				return BuildFromAttributeSet(InCollection, InContext, ParamData, Details, bBuildStaging);
			}
		}
		return false;
	}
}
