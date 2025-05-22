// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExDetails.h"
#include "PCGExDetailsData.h"
#include "PCGExProxyDataBlending.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExDataFilter.h"
#include "Data/PCGExUnionData.h"









namespace PCGExDataBlending
{
	struct FPropertiesBlender;
}

namespace PCGExDataBlending
{
	class PCGEXTENDEDTOOLKIT_API FMultiSourceBlender : public TSharedFromThis<FMultiSourceBlender>
	{
		// Cheap trick :
		// For each attribute that requires blending, we create a blender
		// Source A : Unique Source (In), Source B : Target (Out), Target : Target (Out)
		// When comes the time to blend
		// We initialize a single MultiBlend on the main blender
		// then loop over each sub-blender using the FOpStats from the main blender
		// and finally end blend on the main blender using the updated FOpStats
		
	public:
		FBlendingHeader Header;
		PCGEx::FAttributeIdentity Identity;
		const FPCGMetadataAttributeBase* DefaultValue = nullptr;

		TArray<const FPCGMetadataAttributeBase*> Siblings; // Same attribute as it exists from different sources
		TArray<TSharedPtr<PCGExData::FFacade>> Sources; // Sources (null if attribute is not valid on that source)
		TArray<TSharedPtr<FProxyDataBlender>> SubBlenders; // One blender per source
		
		TSharedPtr<FProxyDataBlender> MainBlender; // Finisher, only used to initialize tracker & complete the multiblend 

		explicit FMultiSourceBlender(const PCGEx::FAttributeIdentity& InIdentity);

		~FMultiSourceBlender() = default;
		
		// Used to initialize an attribute of a given type
		bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InTargetData, TArray<TSharedPtr<PCGExData::FFacade>>& Sources);
		
		void SoftInit(const TSharedPtr<PCGExData::FFacade>& InTargetData, TArray<TSharedPtr<PCGExData::FFacade>>& Sources);

		void SetNum(const int32 InNum)
		{
			Siblings.SetNum(InNum);
			Sources.SetNum(InNum);
			SubBlenders.SetNum(InNum);
		}
	};

	class PCGEXTENDEDTOOLKIT_API FUnionBlender final : public TSharedFromThis<FUnionBlender>
	{
	public:
		const FPCGExCarryOverDetails* CarryOverDetails;

		explicit FUnionBlender(const FPCGExBlendingDetails* InBlendingDetails, const FPCGExCarryOverDetails* InCarryOverDetails);
		~FUnionBlender();

		void AddSource(const TSharedPtr<PCGExData::FFacade>& InFacade, const TSet<FName>* IgnoreAttributeSet = nullptr);
		void AddSources(const TArray<TSharedRef<PCGExData::FFacade>>& InFacades, const TSet<FName>* IgnoreAttributeSet = nullptr);

		void Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& TargetData, const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata);

		void InitTrackers(TArray<PCGEx::FOpStats>& Trackers);

		void MergeSingle(const int32 UnionIndex, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails, TArray<PCGEx::FOpStats>& Trackers);
		void MergeSingle(const int32 WriteIndex, const TSharedPtr<PCGExData::FUnionData>& InUnionData, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails, TArray<PCGEx::FOpStats>& Trackers);

		void PrepareSoftMerge(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& TargetData, const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata);
		void SoftMergeSingle(const int32 UnionIndex, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails);
		void SoftMergeSingle(const int32 UnionIndex, const TSharedPtr<PCGExData::FUnionData>& InUnionData, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails);

		bool Validate(FPCGExContext* InContext, const bool bQuiet) const;

	protected:
		TSet<FString> TypeMismatches;

		bool bPreserveAttributesDefaultValue = false;
		const FPCGExBlendingDetails* BlendingDetails = nullptr;
		TArray<FBlendingHeader> PropertyHeaders;

		TArray<TSharedPtr<FMultiSourceBlender>> MultiSourceBlender;

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
