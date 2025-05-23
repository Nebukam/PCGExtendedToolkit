// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
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
	class PCGEXTENDEDTOOLKIT_API FUnionBlender final : public TSharedFromThis<FUnionBlender>
	{
	public:
		const FPCGExCarryOverDetails* CarryOverDetails;

		explicit FUnionBlender(const FPCGExBlendingDetails* InBlendingDetails, const FPCGExCarryOverDetails* InCarryOverDetails, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails);
		~FUnionBlender();

		class FMultiSourceBlender : public TSharedFromThis<FMultiSourceBlender>
		{
			friend FUnionBlender;
			
		public:
			FBlendingHeader Header;
			PCGEx::FAttributeIdentity Identity;
			const FPCGMetadataAttributeBase* DefaultValue = nullptr;

			TSharedPtr<FProxyDataBlender> MainBlender; // Finisher, only used to initialize tracker & complete the multiblend 

			explicit FMultiSourceBlender(const PCGEx::FAttributeIdentity& InIdentity);

			~FMultiSourceBlender() = default;

			// Used to initialize an attribute of a given type
			bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InTargetData, const bool bWantsDirectAccess = false);

		protected:
			TArray<const FPCGMetadataAttributeBase*> Siblings; // Same attribute as it exists from different sources
			TArray<TSharedPtr<PCGExData::FFacade>> Sources;    // Sources (null if attribute is not valid on that source)
			TArray<TSharedPtr<FProxyDataBlender>> SubBlenders; // One blender per source
			
			void SetNum(const int32 InNum)
			{
				Siblings.SetNum(InNum);
				Sources.SetNum(InNum);
				SubBlenders.SetNum(InNum);
			}
		};

		void AddSource(const TSharedPtr<PCGExData::FFacade>& InFacade, const TSet<FName>* IgnoreAttributeSet = nullptr);
		void AddSources(const TArray<TSharedRef<PCGExData::FFacade>>& InFacades, const TSet<FName>* IgnoreAttributeSet = nullptr);

		// bWantsDirectAccess replaces the previous "soft blending" concept
		// Blenders will be initialized with an attribute instead of a buffer if it is enabled
		bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& TargetData, const bool bWantsDirectAccess = false);
		bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& TargetData, const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata, const bool bWantsDirectAccess = false);

		void MergeSingle(const int32 WriteIndex, const TSharedPtr<PCGExData::FUnionData>& InUnionData, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints);
		void MergeSingle(const int32 UnionIndex, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints);

	protected:
		TSet<FString> TypeMismatches;
		bool Validate(FPCGExContext* InContext, const bool bQuiet) const;

		bool bPreserveAttributesDefaultValue = false;
		const FPCGExBlendingDetails* BlendingDetails = nullptr;
		const TSharedPtr<PCGExDetails::FDistances> DistanceDetails = nullptr;

		TArray<FBlendingHeader> PropertyHeaders;
		TArray<TSharedPtr<FMultiSourceBlender>> Blenders;

		TSet<FString> UniqueTags;
		TArray<FString> UniqueTagsList;
		TArray<FPCGMetadataAttribute<bool>*> TagAttributes;
		TMap<uint32, int32> IOIndices;

		TArray<TSharedPtr<PCGExData::FFacade>> Sources;
		TArray<const UPCGBasePointData*> SourcesData;

		TSharedPtr<PCGExData::FUnionMetadata> CurrentUnionMetadata;
		TSharedPtr<PCGExData::FFacade> CurrentTargetData;
	};
}
