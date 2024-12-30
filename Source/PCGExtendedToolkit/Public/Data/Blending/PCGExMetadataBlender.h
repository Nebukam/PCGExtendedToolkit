// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExDataBlending.h"
#include "PCGExPropertiesBlender.h"


namespace PCGExDataBlending
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FMetadataBlender final : public TSharedFromThis<FMetadataBlender>
	{
	public:
		bool bBlendProperties = true;

		TMap<FName, FDataBlendingProcessorBase*> OperationIdMap;

		~FMetadataBlender() = default;

		explicit FMetadataBlender(const FPCGExBlendingDetails* InBlendingDetails);
		explicit FMetadataBlender(const FMetadataBlender* ReferenceBlender);

		void PrepareForData(
			const TSharedRef<PCGExData::FFacade>& InPrimaryFacade,
			const PCGExData::ESource SecondarySource = PCGExData::ESource::In,
			const bool bInitFirstOperation = true,
			const TSet<FName>* IgnoreAttributeSet = nullptr,
			const bool bSoftMode = false);

		void PrepareForData(
			const TSharedRef<PCGExData::FFacade>& InPrimaryFacade,
			const TSharedRef<PCGExData::FFacade>& InSecondaryFacade,
			const PCGExData::ESource SecondarySource = PCGExData::ESource::In,
			const bool bInitFirstOperation = true,
			const TSet<FName>* IgnoreAttributeSet = nullptr,
			const bool bSoftMode = false);


		FORCEINLINE void PrepareForBlending(const PCGExData::FPointRef& Target, const FPCGPoint* Defaults = nullptr) const
		{
			for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->PrepareOperation(Target.Index); }
			if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }
			PropertiesBlender->PrepareBlending(Target.MutablePoint(), Defaults ? *Defaults : *Target.Point);
		}

		FORCEINLINE void PrepareForBlending(const int32 PrimaryIndex, const FPCGPoint* Defaults = nullptr) const
		{
			for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->PrepareOperation(PrimaryIndex); }
			if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }
			PropertiesBlender->PrepareBlending(*(PrimaryPoints->GetData() + PrimaryIndex), Defaults ? *Defaults : *(PrimaryPoints->GetData() + PrimaryIndex));
		}

		FORCEINLINE void Blend(const PCGExData::FPointRef& A, const PCGExData::FPointRef& B, const PCGExData::FPointRef& Target, const double Weight)
		{
			const int8 IsFirstOperation = FirstPointOperation[Target.Index];
			for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->DoOperation(A.Index, B.Index, Target.Index, Weight, IsFirstOperation); }
			FirstPointOperation[Target.Index] = false;
			if (bSkipProperties) { return; }
			PropertiesBlender->Blend(*A.Point, *B.Point, Target.MutablePoint(), Weight);
		}

		FORCEINLINE void Blend(const int32 PrimaryIndex, const int32 SecondaryIndex, const int32 TargetIndex, const double Weight)
		{
			const int8 IsFirstOperation = FirstPointOperation[TargetIndex];
			for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->DoOperation(PrimaryIndex, SecondaryIndex, TargetIndex, Weight, IsFirstOperation); }
			FirstPointOperation[TargetIndex] = false;
			if (bSkipProperties) { return; }
			PropertiesBlender->Blend(*(PrimaryPoints->GetData() + PrimaryIndex), *(SecondaryPoints->GetData() + SecondaryIndex), (*PrimaryPoints)[TargetIndex], Weight);
		}

		FORCEINLINE void Copy(const int32 TargetIndex, const int32 SecondaryIndex)
		{
			for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->Copy(TargetIndex, SecondaryIndex); }
		}

		FORCEINLINE void CompleteBlending(const PCGExData::FPointRef& Target, const int32 Count, const double TotalWeight) const
		{
			for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->CompleteOperation(Target.Index, Count, TotalWeight); }
			if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }
			PropertiesBlender->CompleteBlending(Target.MutablePoint(), Count, TotalWeight);
		}

		FORCEINLINE void CompleteBlending(const int32 PrimaryIndex, const int32 Count, const double TotalWeight) const
		{
			//check(Count > 0) // Ugh, there's a check missing in a blender user...
			for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->CompleteOperation(PrimaryIndex, Count, TotalWeight); }
			if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }
			PropertiesBlender->CompleteBlending(*(PrimaryPoints->GetData() + PrimaryIndex), Count, TotalWeight);
		}

		void PrepareRangeForBlending(const int32 StartIndex, const int32 Range) const;
		void BlendRange(const PCGExData::FPointRef& A, const PCGExData::FPointRef& B, const int32 StartIndex, const int32 Range, const TArrayView<double>& Weights);
		void CompleteRangeBlending(const int32 StartIndex, const int32 Range, const TArrayView<const int32>& Counts, const TArrayView<double>& TotalWeights) const;

		void BlendRangeFromTo(const PCGExData::FPointRef& From, const PCGExData::FPointRef& To, const int32 StartIndex, const TArrayView<double>& Weights);

		// Soft ops

		FORCEINLINE void PrepareForBlending(FPCGPoint& Target, const FPCGPoint* Defaults = nullptr) const
		{
			for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->PrepareOperation(Target.MetadataEntry); }
			if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }
			PropertiesBlender->PrepareBlending(Target, Defaults ? *Defaults : Target);
		}

		FORCEINLINE void Copy(const FPCGPoint& Target, const FPCGPoint& Source)
		{
			for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->Copy(Target.MetadataEntry, Source.MetadataEntry); }
		}

		FORCEINLINE void Blend(const FPCGPoint& A, const FPCGPoint& B, FPCGPoint& Target, const double Weight, const bool bIsFirstOperation = false)
		{
			for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->DoOperation(A.MetadataEntry, B.MetadataEntry, Target.MetadataEntry, Weight, bIsFirstOperation); }
			if (bSkipProperties) { return; }
			PropertiesBlender->Blend(A, B, Target, Weight);
		}

		FORCEINLINE void CompleteBlending(FPCGPoint& Target, const int32 Count, const double TotalWeight) const
		{
			for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->CompleteOperation(Target.MetadataEntry, Count, TotalWeight); }
			if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }
			PropertiesBlender->CompleteBlending(Target, Count, TotalWeight);
		}

		void Cleanup();

	protected:
		const FPCGExBlendingDetails* BlendingDetails = nullptr;
		TUniquePtr<FPropertiesBlender> PropertiesBlender;
		bool bSkipProperties = false;
		TArray<TSharedPtr<FDataBlendingProcessorBase>> Operations;

		TArray<FPCGPoint>* PrimaryPoints = nullptr;
		const TArray<FPCGPoint>* SecondaryPoints = nullptr;

		TArray<int8> FirstPointOperation;

		void InternalPrepareForData(
			const TSharedPtr<PCGExData::FFacade>& InPrimaryFacade,
			const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade,
			const PCGExData::ESource SecondarySource,
			const bool bInitFirstOperation,
			const TSet<FName>* IgnoreAttributeSet,
			const bool bSoftMode = false);
	};
}
