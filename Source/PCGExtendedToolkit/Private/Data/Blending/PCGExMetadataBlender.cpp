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
		const TSharedRef<PCGExData::FFacade>& InPrimaryFacade,
		const PCGExData::ESource SecondarySource,
		const bool bInitFirstOperation,
		const TSet<FName>* IgnoreAttributeSet,
		const bool bSoftMode)
	{
		InternalPrepareForData(InPrimaryFacade, InPrimaryFacade, SecondarySource, bInitFirstOperation, IgnoreAttributeSet, bSoftMode);
	}

	void FMetadataBlender::PrepareForData(
		const TSharedRef<PCGExData::FFacade>& InPrimaryFacade,
		const TSharedRef<PCGExData::FFacade>& InSecondaryFacade,
		const PCGExData::ESource SecondarySource,
		const bool bInitFirstOperation,
		const TSet<FName>* IgnoreAttributeSet,
		const bool bSoftMode)
	{
		InternalPrepareForData(InPrimaryFacade, InSecondaryFacade, SecondarySource, bInitFirstOperation, IgnoreAttributeSet, bSoftMode);
	}

	void FMetadataBlender::PrepareForBlending(const int32 TargetIndex, const PCGExData::FConstPoint& Defaults) const
	{
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->PrepareOperation(TargetIndex.Index); }
		if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }
		PropertiesBlender->PrepareBlending(TargetIndex.MutablePoint(), Defaults ? *Defaults : *TargetIndex.Point);
	}

	void FMetadataBlender::PrepareForBlending(const int32 PrimaryIndex, const PCGExData::FConstPoint& Defaults) const
	{
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->PrepareOperation(PrimaryIndex); }
		if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }
		PropertiesBlender->PrepareBlending(*(PrimaryPoints->GetData() + PrimaryIndex), &Defaults ? Defaults : *(PrimaryPoints->GetData() + PrimaryIndex));
	}

	void FMetadataBlender::Blend(const PCGExData::FConstPoint& A, const PCGExData::FConstPoint& B, const PCGExData::FMutablePoint& Target, const double Weight)
	{
		const int8 IsFirstOperation = FirstPointOperation[Target.Index];
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->DoOperation(A.Index, B.Index, Target.Index, Weight, IsFirstOperation); }
		FirstPointOperation[Target.Index] = false;
		if (bSkipProperties) { return; }
		PropertiesBlender->Blend(*A.Point, *B.Point, Target.MutablePoint(), Weight);
	}

	void FMetadataBlender::Blend(const int32 PrimaryIndex, const int32 SecondaryIndex, const int32 TargetIndex, const double Weight)
	{
		const int8 IsFirstOperation = FirstPointOperation[TargetIndex];
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->DoOperation(PrimaryIndex, SecondaryIndex, TargetIndex, Weight, IsFirstOperation); }
		FirstPointOperation[TargetIndex] = false;
		if (bSkipProperties) { return; }
		PropertiesBlender->Blend(*(PrimaryPoints->GetData() + PrimaryIndex), *(SecondaryPoints->GetData() + SecondaryIndex), (*PrimaryPoints)[TargetIndex], Weight);
	}

	void FMetadataBlender::Copy(const int32 TargetIndex, const int32 SecondaryIndex)
	{
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->Copy(TargetIndex, SecondaryIndex); }
	}

	void FMetadataBlender::CompleteBlending(const PCGExData::FMutablePoint& Target, const int32 Count, const double TotalWeight) const
	{
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->CompleteOperation(Target.Index, Count, TotalWeight); }
		if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }
		PropertiesBlender->CompleteBlending(Target.MutablePoint(), Count, TotalWeight);
	}

	void FMetadataBlender::CompleteBlending(const int32 PrimaryIndex, const int32 Count, const double TotalWeight) const
	{
		//check(Count > 0) // Ugh, there's a check missing in a blender user...
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->CompleteOperation(PrimaryIndex, Count, TotalWeight); }
		if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }
		PropertiesBlender->CompleteBlending(*(PrimaryPoints->GetData() + PrimaryIndex), Count, TotalWeight);
	}

	void FMetadataBlender::PrepareRangeForBlending(
		const int32 StartIndex,
		const int32 Range) const
	{
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->PrepareRangeOperation(StartIndex, Range); }

		if (bSkipProperties) { return; }

		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->PrepareRangeBlending(View, (*PrimaryPoints)[StartIndex]);
	}

	void FMetadataBlender::BlendRange(
		const PCGExData::FConstPoint& A,
		const PCGExData::FConstPoint& B,
		const int32 StartIndex,
		const int32 Range,
		const TArrayView<double>& Weights)
	{
		const int32 PrimaryIndex = A.Index;
		const int32 SecondaryIndex = B.Index;

		const int8 IsFirstOperation = FirstPointOperation[PrimaryIndex];
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->DoRangeOperation(PrimaryIndex, SecondaryIndex, StartIndex, Weights, IsFirstOperation); }
		FirstPointOperation[PrimaryIndex] = false;

		if (bSkipProperties) { return; }

		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->BlendRange(*A.Point, *B.Point, View, Weights);
	}

	void FMetadataBlender::CompleteRangeBlending(
		const int32 StartIndex,
		const int32 Range,
		const TArrayView<const int32>& Counts,
		const TArrayView<double>& TotalWeights) const
	{
		for (const TSharedPtr<FDataBlendingProcessorBase>& Op : Operations) { Op->CompleteRangeOperation(StartIndex, Counts, TotalWeights); }

		if (bSkipProperties) { return; }

		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->CompleteRangeBlending(View, Counts, TotalWeights);
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
		PropertiesBlender->PrepareBlending(Target, Defaults ? *Defaults : Target);
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

		PrimaryPoints = nullptr;
		SecondaryPoints = nullptr;
	}

	void FMetadataBlender::InternalPrepareForData(
		const TSharedPtr<PCGExData::FFacade>& InPrimaryFacade,
		const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade,
		const PCGExData::ESource SecondarySource,
		const bool bInitFirstOperation,
		const TSet<FName>* IgnoreAttributeSet,
		const bool bSoftMode)
	{
		Cleanup();

		bSkipProperties = !bBlendProperties;
		if (!bSkipProperties)
		{
			PropertiesBlender = MakeUnique<FPropertiesBlender>(BlendingDetails->GetPropertiesBlendingDetails());
			if (PropertiesBlender->bHasNoBlending)
			{
				bSkipProperties = true;
				PropertiesBlender.Reset();
			}
		}

		PrimaryPoints = &InPrimaryFacade->Source->GetOut()->GetMutablePoints();
		SecondaryPoints = &InSecondaryFacade->Source->GetData(SecondarySource)->GetPoints();

		TArray<PCGEx::FAttributeIdentity> Identities;
		PCGEx::FAttributeIdentity::Get(InPrimaryFacade->Source->GetOut()->Metadata, Identities);
		BlendingDetails->Filter(Identities);

		if (InSecondaryFacade != InPrimaryFacade)
		{
			TArray<FName> PrimaryNames;
			TArray<FName> SecondaryNames;
			TMap<FName, PCGEx::FAttributeIdentity> PrimaryIdentityMap;
			TMap<FName, PCGEx::FAttributeIdentity> SecondaryIdentityMap;
			PCGEx::FAttributeIdentity::Get(InPrimaryFacade->Source->GetOut()->Metadata, PrimaryNames, PrimaryIdentityMap);
			PCGEx::FAttributeIdentity::Get(InSecondaryFacade->Source->GetData(SecondarySource)->Metadata, SecondaryNames, SecondaryIdentityMap);

			for (FName PrimaryName : PrimaryNames)
			{
				const PCGEx::FAttributeIdentity& PrimaryIdentity = *PrimaryIdentityMap.Find(PrimaryName);
				if (!SecondaryIdentityMap.Find(PrimaryName))
				{
					//An attribute exists on Primary but not on Secondary -- Simply ignore it.
					Identities.Remove(PrimaryIdentity);
				}
			}

			for (FName SecondaryName : SecondaryNames)
			{
				const PCGEx::FAttributeIdentity& SecondaryIdentity = *SecondaryIdentityMap.Find(SecondaryName);
				if (const PCGEx::FAttributeIdentity* PrimaryIdentityPtr = PrimaryIdentityMap.Find(SecondaryName))
				{
					if (PrimaryIdentityPtr->UnderlyingType != SecondaryIdentity.UnderlyingType)
					{
						// Type mismatch -- Simply ignore it
						Identities.Remove(*PrimaryIdentityPtr);
					}
				}
				else if (BlendingDetails->CanBlend(SecondaryIdentity.Name))
				{
					//Operation will handle missing attribute creation.
					Identities.Add(SecondaryIdentity);
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

			if (bSoftMode) { Op->SoftPrepareForData(InPrimaryFacade, InSecondaryFacade, SecondarySource); }
			else { Op->PrepareForData(InPrimaryFacade, InSecondaryFacade, SecondarySource); }
		}

		FirstPointOperation.Init(bInitFirstOperation, PrimaryPoints->Num());
	}
}
