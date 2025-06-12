// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExBlendOpFactoryProvider.h"
#include "PCGExBlendOpsManager.h"
#include "UObject/Object.h"
#include "PCGExDetailsData.h"
#include "PCGExProxyDataBlending.h"
#include "Data/PCGExData.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExUnionData.h"

namespace PCGExDataBlending
{
	struct FPropertiesBlender;
}

namespace PCGExDataBlending
{
	class PCGEXTENDEDTOOLKIT_API FUnionOpsManager final : public IUnionBlender
	{
	public:
		FUnionOpsManager(const TArray<TObjectPtr<const UPCGExBlendOpFactory>>* InBlendingFactories, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails);
		virtual ~FUnionOpsManager() override;

		bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& TargetData, const TArray<TSharedRef<PCGExData::FFacade>>& InSources);
		bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& TargetData, const TArray<TSharedRef<PCGExData::FFacade>>& InSources, const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata);

		virtual void InitTrackers(TArray<PCGEx::FOpStats>& Trackers) const override;
		virtual void MergeSingle(const int32 WriteIndex, const TSharedPtr<PCGExData::FUnionData>& InUnionData, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints, TArray<PCGEx::FOpStats>& Trackers) const override;
		virtual void MergeSingle(const int32 UnionIndex, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints, TArray<PCGEx::FOpStats>& Trackers) const override;

		void Cleanup(FPCGExContext* InContext);
		
	protected:
		const TArray<TObjectPtr<const UPCGExBlendOpFactory>>* BlendingFactories = nullptr;

		const TSharedPtr<PCGExDetails::FDistances> DistanceDetails = nullptr;

		TArray<TSharedPtr<FBlendOpsManager>> Blenders;
		TSharedPtr<PCGEx::FIndexLookup> IOLookup;

		TArray<const UPCGBasePointData*> SourcesData;

		TSharedPtr<PCGExData::FUnionMetadata> CurrentUnionMetadata;
		TSharedPtr<PCGExData::FFacade> CurrentTargetData;
	};
}
