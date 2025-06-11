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
	class PCGEXTENDEDTOOLKIT_API FUnionBlender final : public IUnionBlender
	{
	public:
		const FPCGExCarryOverDetails* CarryOverDetails;

		FUnionBlender(const FPCGExBlendingDetails* InBlendingDetails, const FPCGExCarryOverDetails* InCarryOverDetails, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails);
		virtual ~FUnionBlender() override;

		class FMultiSourceBlender : public TSharedFromThis<FMultiSourceBlender>
		{
			friend FUnionBlender;

		public:
			FBlendingParam Param;
			PCGEx::FAttributeIdentity Identity;
			const FPCGMetadataAttributeBase* DefaultValue = nullptr;

			TSharedPtr<FProxyDataBlender> MainBlender; // Finisher, only used to initialize tracker & complete the multiblend 

			FMultiSourceBlender(const PCGEx::FAttributeIdentity& InIdentity, const TArray<TSharedPtr<PCGExData::FFacade>>& InSources);
			explicit FMultiSourceBlender(const TArray<TSharedPtr<PCGExData::FFacade>>& InSources);

			~FMultiSourceBlender() = default;

			// Used to initialize an attribute of a given type
			bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InTargetData, const bool bWantsDirectAccess = false);

		protected:
			TSet<int32> SupportedSources;
			const TArray<TSharedPtr<PCGExData::FFacade>>& Sources; // Sources (null if attribute is not valid on that source)
			TArray<TSharedPtr<FProxyDataBlender>> SubBlenders;     // One blender per source

			void SetNum(const int32 InNum) { SubBlenders.SetNum(InNum); }
		};

		void AddSources(const TArray<TSharedRef<PCGExData::FFacade>>& InSources, const TSet<FName>* IgnoreAttributeSet = nullptr);

		// bWantsDirectAccess replaces the previous "soft blending" concept
		// Blenders will be initialized with an attribute instead of a buffer if it is enabled
		bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& TargetData, const bool bWantsDirectAccess = false);
		bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& TargetData, const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata, const bool bWantsDirectAccess = false);

		virtual void InitTrackers(TArray<PCGEx::FOpStats>& Trackers) const override
		{
		};
		virtual void MergeSingle(const int32 WriteIndex, const TSharedPtr<PCGExData::FUnionData>& InUnionData, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints, TArray<PCGEx::FOpStats>& Trackers) const override;
		virtual void MergeSingle(const int32 UnionIndex, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints, TArray<PCGEx::FOpStats>& Trackers) const override;

	protected:
		TSet<FString> TypeMismatches;
		bool Validate(FPCGExContext* InContext, const bool bQuiet) const;

		bool bPreserveAttributesDefaultValue = false;
		const FPCGExBlendingDetails* BlendingDetails = nullptr;
		const TSharedPtr<PCGExDetails::FDistances> DistanceDetails = nullptr;

		TArray<FBlendingParam> PropertyParams;
		TArray<TSharedPtr<FMultiSourceBlender>> Blenders;

		TSet<FString> UniqueTags;
		TArray<FString> UniqueTagsList;
		TArray<FPCGMetadataAttribute<bool>*> TagAttributes;
		TSharedPtr<PCGEx::FIndexLookup> IOLookup;

		TArray<TSharedPtr<PCGExData::FFacade>> Sources;
		TArray<const UPCGBasePointData*> SourcesData;

		TSharedPtr<PCGExData::FUnionMetadata> CurrentUnionMetadata;
		TSharedPtr<PCGExData::FFacade> CurrentTargetData;
	};
}
