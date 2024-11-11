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
	class /*PCGEXTENDEDTOOLKIT_API*/ FMultiSourceAttribute : public TSharedFromThis<FMultiSourceAttribute>
	{
	public:
		PCGEx::FAttributeIdentity Identity;
		bool AllowsInterpolation = true;

		const FPCGMetadataAttributeBase* DefaultValue = nullptr;
		TArray<const FPCGMetadataAttributeBase*> Siblings; // Same attribute as it exists from different sources
		TArray<TSharedPtr<FDataBlendingOperationBase>> SubBlendingOps;

		TSharedPtr<FDataBlendingOperationBase> MainBlendingOp;
		TSharedPtr<PCGExData::FBufferBase> Buffer;

		explicit FMultiSourceAttribute(const PCGEx::FAttributeIdentity& InIdentity)
			: Identity(InIdentity)
		{
		}

		~FMultiSourceAttribute()
		{
		}

		template <typename T>
		const FPCGMetadataAttribute<T>* Get(const int32 SourceIndex)
		{
			const FPCGMetadataAttributeBase* Att = Siblings[SourceIndex];
			if (!Att) { return nullptr; }
			return static_cast<const FPCGMetadataAttribute<T>*>(Att);
		}

		template <typename T>
		void PrepareMerge(const TSharedPtr<PCGExData::FFacade> InTargetData, TArray<TSharedPtr<PCGExData::FFacade>>& Sources)
		{
			check(InTargetData);

			Buffer = nullptr;

			if (const FPCGMetadataAttribute<T>* ExistingAttribute = InTargetData->FindConstAttribute<T>(Identity.Name))
			{
				// This attribute exists
				Buffer = InTargetData->GetWritable<T>(ExistingAttribute, PCGExData::EBufferInit::Inherit);
			}
			else
			{
				Buffer = InTargetData->GetWritable<T>(static_cast<const FPCGMetadataAttribute<T>*>(DefaultValue), PCGExData::EBufferInit::New);
			}

			for (int i = 0; i < Sources.Num(); i++)
			{
				if (const TSharedPtr<FDataBlendingOperationBase>& SubOp = SubBlendingOps[i]) { SubOp->PrepareForData(Buffer, Sources[i]); }
			}

			MainBlendingOp->PrepareForData(Buffer, InTargetData, PCGExData::ESource::Out);
		}

		template <typename T>
		void PrepareSoftMerge(const TSharedPtr<PCGExData::FFacade> InTargetData, TArray<TSharedPtr<PCGExData::FFacade>>& Sources)
		{
			check(InTargetData);

			Buffer = nullptr;

			for (int i = 0; i < Sources.Num(); i++)
			{
				if (const TSharedPtr<FDataBlendingOperationBase>& SrcOp = SubBlendingOps[i])
				{
					SrcOp->SoftPrepareForData(InTargetData, Sources[i]);
				}
			}

			MainBlendingOp->SoftPrepareForData(InTargetData, InTargetData, PCGExData::ESource::Out);
		}

		void SetNum(const int32 InNum)
		{
			Siblings.SetNum(InNum);
			SubBlendingOps.SetNum(InNum);
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FUnionBlender final : public TSharedFromThis<FUnionBlender>
	{
	public:
		const FPCGExCarryOverDetails* CarryOverDetails;

		explicit FUnionBlender(const FPCGExBlendingDetails* InBlendingDetails, const FPCGExCarryOverDetails* InCarryOverDetails);
		~FUnionBlender();

		void AddSource(const TSharedPtr<PCGExData::FFacade>& InFacade, const TSet<FName>* IgnoreAttributeSet = nullptr);
		void AddSources(const TArray<TSharedRef<PCGExData::FFacade>>& InFacades, const TSet<FName>* IgnoreAttributeSet = nullptr);

		void PrepareMerge(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& TargetData, const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata);
		void MergeSingle(const int32 UnionIndex, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails);
		void MergeSingle(const int32 WriteIndex, const PCGExData::FUnionData* InUnionData, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails);

		void PrepareSoftMerge(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& TargetData, const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata);
		void SoftMergeSingle(const int32 UnionIndex, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails);
		void SoftMergeSingle(const int32 UnionIndex, const PCGExData::FUnionData* InUnionData, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails);

		void BlendProperties(FPCGPoint& TargetPoint, TArray<int32>& IdxIO, TArray<int32>& IdxPt, TArray<double>& Weights);

		bool Validate(FPCGExContext* InContext, const bool bQuiet) const;

	protected:
		TSet<FString> TypeMismatches;

		bool bPreserveAttributesDefaultValue = false;
		const FPCGExBlendingDetails* BlendingDetails = nullptr;

		TArray<TSharedPtr<FMultiSourceAttribute>> MultiSourceAttributes;

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
