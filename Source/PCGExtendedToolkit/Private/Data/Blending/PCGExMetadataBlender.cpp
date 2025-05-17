// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Blending/PCGExMetadataBlender.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExDataBlendingProcessors.h"
#include "Data/Blending/PCGExDataBlending.h"


namespace PCGExDataBlending
{
	FMetadataBlender::FMetadataBlender(const FPCGExBlendingDetails* InBlendingDetails)
	{
		BlendingDetails = InBlendingDetails;
	}

	FMetadataBlender::FMetadataBlender(const FMetadataBlender* ReferenceBlender)
	{
		BlendingDetails = ReferenceBlender->BlendingDetails;
	}

	void FMetadataBlender::PrepareForData(
		const TSharedRef<PCGExData::FFacade>& InTargetFacade,
		const PCGExData::EIOSide InSourceSide,
		const bool bInitFirstOperation,
		const TSet<FName>* IgnoreAttributeSet,
		const bool bSoftMode)
	{
		InternalPrepareForData(InTargetFacade, InTargetFacade, InSourceSide, bInitFirstOperation, IgnoreAttributeSet, bSoftMode);
	}

	void FMetadataBlender::PrepareForData(
		const TSharedRef<PCGExData::FFacade>& InTargetFacade,
		const TSharedRef<PCGExData::FFacade>& InSourceFacade,
		const PCGExData::EIOSide InSourceSide,
		const bool bInitFirstOperation,
		const TSet<FName>* IgnoreAttributeSet,
		const bool bSoftMode)
	{
		InternalPrepareForData(InTargetFacade, InSourceFacade, InSourceSide, bInitFirstOperation, IgnoreAttributeSet, bSoftMode);
	}

	void FMetadataBlender::PrepareForBlending(const int32 TargetWriteIndex, const PCGExData::FConstPoint& Defaults) const
	{
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations)
		{
			Op->PrepareOperation(TargetWriteIndex);
		}

		if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }

		// TODO : Proper defaults handling 
		PropertiesBlender->PrepareBlending(TargetWriteIndex, Defaults.IsValid() ? Defaults.Index : TargetWriteIndex);
	}

	void FMetadataBlender::Blend(const int32 TargetReadIndex, const int32 SourceReadIndex, const int32 TargetWriteIndex, const double Weight)
	{
		const int8 IsFirstOperation = FirstPointOperation[TargetWriteIndex];

		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations)
		{
			Op->DoOperation(TargetReadIndex, SourceReadIndex, TargetWriteIndex, Weight, IsFirstOperation);
		}

		FirstPointOperation[TargetWriteIndex] = false;

		if (bSkipProperties) { return; }

		PropertiesBlender->Blend(TargetReadIndex, SourceReadIndex, TargetReadIndex, Weight);
	}

	void FMetadataBlender::Copy(const int32 TargetWriteIndex, const int32 SourceReadIndex)
	{
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations)
		{
			Op->Copy(TargetWriteIndex, SourceReadIndex);
		}
	}

	void FMetadataBlender::CompleteBlending(const int32 TargetWriteIndex, const int32 Count, const double TotalWeight) const
	{
		check(Count > 0) // Ugh, there's a check missing in a blender user...

		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations)
		{
			Op->CompleteOperation(TargetWriteIndex, Count, TotalWeight);
		}

		if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }

		// TODO : Proper defaults handling
		PropertiesBlender->CompleteBlending(TargetWriteIndex, Count, TotalWeight);
	}

	void FMetadataBlender::PrepareRangeForBlending(
		const int32 StartIndex,
		const int32 Range) const
	{
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->PrepareRangeOperation(StartIndex, Range); }

		if (bSkipProperties) { return; }

		TArray<int32> RangeIndices;
		PCGExMT::FScope(StartIndex, Range).GetIndices(RangeIndices);
		PropertiesBlender->PrepareRangeBlending(RangeIndices, StartIndex);
	}

	void FMetadataBlender::BlendRange(
		const int32 A, const int32 B,
		const int32 StartIndex, const int32 Range, const TArrayView<double>& Weights)
	{
		const int8 IsFirstOperation = FirstPointOperation[A];
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->DoRangeOperation(A, B, StartIndex, Weights, IsFirstOperation); }
		FirstPointOperation[A] = false;

		if (bSkipProperties) { return; }

		TArray<int32> RangeIndices;
		PCGExMT::FScope(StartIndex, Range).GetIndices(RangeIndices);
		PropertiesBlender->BlendRange(A, B, RangeIndices, Weights);
	}

	void FMetadataBlender::CompleteRangeBlending(
		const int32 StartIndex, const int32 Range,
		const TArrayView<const int32>& Counts,
		const TArrayView<double>& TotalWeights) const
	{
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->CompleteRangeOperation(StartIndex, Counts, TotalWeights); }

		if (bSkipProperties) { return; }

		TArray<int32> RangeIndices;
		PCGExMT::FScope(StartIndex, Range).GetIndices(RangeIndices);
		PropertiesBlender->CompleteRangeBlending(RangeIndices, Counts, TotalWeights);
	}

	void FMetadataBlender::BlendRangeFromTo(
		const PCGExData::FConstPoint& From,
		const PCGExData::FConstPoint& To,
		const int32 StartIndex,
		const TArrayView<double>& Weights)
	{
		TArray<int32> Counts;
		Counts.SetNumUninitialized(Weights.Num());
		for (int32& C : Counts) { C = 2; }

		const int32 Range = Weights.Num();
		const int32 PrimaryIndex = From.Index;
		const int32 SecondaryIndex = To.Index;

		const int8 IsFirstOperation = FirstPointOperation[PrimaryIndex];

		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->DoRangeOperation(PrimaryIndex, SecondaryIndex, StartIndex, Weights, IsFirstOperation); }
		for (int i = 0; i < Weights.Num(); i++) { FirstPointOperation[StartIndex + i] = false; }

		if (bSkipProperties) { return; }

		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->BlendRangeFromTo(*From.Point, *To.Point, View, Weights);
	}

	void FMetadataBlender::PrepareForBlending(PCGExData::FMutablePoint& Target, const PCGExData::FConstPoint& Defaults) const
	{
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->PrepareOperation(Target.MetadataEntry); }
		if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }
		PropertiesBlender->PrepareBlending(Target, Defaults.IsValid() ? *Defaults : Target);
	}

	void FMetadataBlender::Copy(const PCGExData::FMutablePoint& Target, const PCGExData::FConstPoint& Source)
	{
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->Copy(Target.MetadataEntry, Source.MetadataEntry); }
	}

	void FMetadataBlender::Blend(const PCGExData::FConstPoint& A, const PCGExData::FConstPoint& B, const PCGExData::FMutablePoint& Target, const double Weight, const bool bIsFirstOperation)
	{
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->DoOperation(A.MetadataEntry, B.MetadataEntry, Target.MetadataEntry, Weight, bIsFirstOperation); }
		if (bSkipProperties) { return; }
		PropertiesBlender->Blend(A, B, Target, Weight);
	}

	void FMetadataBlender::CompleteBlending(const PCGExData::FMutablePoint& Target, const int32 Count, const double TotalWeight) const
	{
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->CompleteOperation(Target.MetadataEntry, Count, TotalWeight); }
		if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }
		PropertiesBlender->CompleteBlending(Target, Count, TotalWeight);
	}

	void FMetadataBlender::Cleanup()
	{
		FirstPointOperation.Empty();
		OperationIdMap.Empty();
	}

	void FMetadataBlender::InternalPrepareForData(
		const TSharedPtr<PCGExData::FFacade>& InTargetFacade,
		const TSharedPtr<PCGExData::FFacade>& InSourceFacade,
		const PCGExData::EIOSide InSourceSide,
		const bool bInitFirstOperation,
		const TSet<FName>* IgnoreAttributeSet,
		const bool bSoftMode)
	{
		Cleanup();

		bSkipProperties = !bBlendProperties;
		if (!bSkipProperties)
		{
			PropertiesBlender = MakeUnique<FPropertiesBlender>();
			// TODO : Add source for blenders
			PropertiesBlender->Init(BlendingDetails->GetPropertiesBlendingDetails());
			if (PropertiesBlender->bHasNoBlending)
			{
				bSkipProperties = true;
				PropertiesBlender.Reset();
			}
		}

		TargetData = InTargetFacade->Source->GetOut();
		SourceData = InSourceFacade->Source->GetData(InSourceSide);

		TArray<PCGEx::FAttributeIdentity> Identities;
		PCGEx::FAttributeIdentity::Get(InTargetFacade->Source->GetOut()->Metadata, Identities);
		BlendingDetails->Filter(Identities);

		if (InSourceFacade != InTargetFacade)
		{
			TArray<FName> TargetNames;
			TArray<FName> SourceNames;
			TMap<FName, PCGEx::FAttributeIdentity> TargetIdentityMap;
			TMap<FName, PCGEx::FAttributeIdentity> SourceIdentityMap;
			PCGEx::FAttributeIdentity::Get(TargetData->Metadata, TargetNames, TargetIdentityMap);
			PCGEx::FAttributeIdentity::Get(SourceData->Metadata, SourceNames, SourceIdentityMap);

			for (FName TargetName : TargetNames)
			{
				const PCGEx::FAttributeIdentity& TargetIdentity = *TargetIdentityMap.Find(TargetName);
				if (!SourceIdentityMap.Find(TargetName))
				{
					//An attribute exists on Primary but not on Secondary -- Simply ignore it.
					Identities.Remove(TargetIdentity);
				}
			}

			for (FName SourceName : SourceNames)
			{
				const PCGEx::FAttributeIdentity& SourceIdentity = *SourceIdentityMap.Find(SourceName);
				if (const PCGEx::FAttributeIdentity* TargetIdentityPtr = TargetIdentityMap.Find(SourceName))
				{
					if (TargetIdentityPtr->UnderlyingType != SourceIdentity.UnderlyingType)
					{
						// Type mismatch -- Simply ignore it
						Identities.Remove(*TargetIdentityPtr);
					}
				}
				else if (BlendingDetails->CanBlend(SourceIdentity.Name))
				{
					//Operation will handle missing attribute creation.
					Identities.Add(SourceIdentity);
				}
			}
		}

		Operations.Empty(Identities.Num());

		for (const PCGEx::FAttributeIdentity& Identity : Identities)
		{
			if (IgnoreAttributeSet && IgnoreAttributeSet->Contains(Identity.Name)) { continue; }

			const EPCGExDataBlendingType* TypePtr = BlendingDetails->AttributesOverrides.Find(Identity.Name);

			TSharedPtr<FDataBlendingProcessorBase> Op;
			if (PCGEx::IsPCGExAttribute(Identity.Name)) { Op = CreateProcessor(EPCGExDataBlendingType::Copy, Identity); }
			else { Op = CreateProcessor(TypePtr, BlendingDetails->DefaultBlending, Identity); }

			if (!Op) { continue; }

			OperationIdMap.Add(Identity.Name, Op.Get());
			Operations.Add(Op);

			if (bSoftMode) { Op->SoftPrepareForData(InTargetFacade, InSourceFacade, InSourceSide); }
			else { Op->PrepareForData(InTargetFacade, InSourceFacade, InSourceSide); }
		}

		FirstPointOperation.Init(bInitFirstOperation, TargetData->GetNumPoints());
	}
}
