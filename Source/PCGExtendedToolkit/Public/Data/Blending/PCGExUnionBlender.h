// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExDetails.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExDataFilter.h"


namespace PCGExDataBlending
{
	struct FPropertiesBlender;
}

namespace PCGExDataBlending
{
	struct /*PCGEXTENDEDTOOLKIT_API*/ FAttributeSourceMap
	{
		FPCGMetadataAttributeBase* DefaultValuesSource = nullptr;
		TArray<FPCGMetadataAttributeBase*> Attributes;
		TArray<TSharedPtr<FDataBlendingOperationBase>> BlendOps;
		PCGEx::FAttributeIdentity Identity;
		bool AllowsInterpolation = true;

		TSharedPtr<FDataBlendingOperationBase> TargetBlendOp;
		TSharedPtr<PCGExData::FBufferBase> Writer;

		explicit FAttributeSourceMap(const PCGEx::FAttributeIdentity& InIdentity)
			: Identity(InIdentity)
		{
		}

		~FAttributeSourceMap()
		{
		}

		template <typename T>
		FPCGMetadataAttribute<T>* Get(const int32 SourceIndex)
		{
			FPCGMetadataAttributeBase* Att = Attributes[SourceIndex];
			if (!Att) { return nullptr; }
			return static_cast<FPCGMetadataAttribute<T>*>(Att);
		}

		void SetNum(const int32 InNum)
		{
			Attributes.SetNum(InNum);
			BlendOps.SetNum(InNum);
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FUnionBlender final : public TSharedFromThis<FUnionBlender>
	{
	public:
		const FPCGExCarryOverDetails* CarryOverDetails;

		explicit FUnionBlender(const FPCGExBlendingDetails* InBlendingDetails, const FPCGExCarryOverDetails* InCarryOverDetails);
		~FUnionBlender();

		void AddSource(const TSharedPtr<PCGExData::FFacade>& InFacade);
		void AddSources(const TArray<TSharedRef<PCGExData::FFacade>>& InFacades);

		void PrepareMerge(const TSharedPtr<PCGExData::FFacade>& TargetData, const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata, const TSet<FName>* IgnoreAttributeSet = nullptr);
		void MergeSingle(const int32 UnionIndex, const FPCGExDistanceDetails& InDistanceDetails);
		void MergeSingle(const int32 WriteIndex, const PCGExData::FUnionData* InUnionData, const FPCGExDistanceDetails& InDistanceDetails);

		void PrepareSoftMerge(const TSharedPtr<PCGExData::FFacade>& TargetData, const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata, const TSet<FName>* IgnoreAttributeSet = nullptr);
		void SoftMergeSingle(const int32 UnionIndex, const FPCGExDistanceDetails& InDistanceDetails);
		void SoftMergeSingle(const int32 UnionIndex, const PCGExData::FUnionData* InUnionData, const FPCGExDistanceDetails& InDistanceDetails);

		void BlendProperties(FPCGPoint& TargetPoint, TArray<int32>& IdxIO, TArray<int32>& IdxPt, TArray<double>& Weights);

	protected:
		bool bPreserveAttributesDefaultValue = false;
		const FPCGExBlendingDetails* BlendingDetails = nullptr;

		TArray<TSharedPtr<FAttributeSourceMap>> AttributeSourceMaps;
		TSet<FString> UniqueTags;
		TArray<FString> UniqueTagsList;
		TArray<FPCGMetadataAttribute<bool>*> TagAttributes;
		TMap<uint32, int32> IOIndices;
		TArray<TSharedPtr<PCGExData::FFacade>> Sources;

		TSharedPtr<PCGExData::FUnionMetadata> CurrentUnionMetadata;
		TSharedPtr<PCGExData::FFacade> CurrentTargetData;
		TUniquePtr<FPropertiesBlender> PropertiesBlender;
	};
}
