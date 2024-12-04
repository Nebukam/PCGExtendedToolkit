// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Blending/PCGExMetadataBlender.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExDataBlendingOperations.h"
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

	void FMetadataBlender::PrepareRangeForBlending(
		const int32 StartIndex,
		const int32 Range) const
	{
		for (const TSharedPtr<FDataBlendingOperationBase>& Op : Operations) { Op->PrepareRangeOperation(StartIndex, Range); }

		if (bSkipProperties) { return; }

		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->PrepareRangeBlending(View, (*PrimaryPoints)[StartIndex]);
	}

	void FMetadataBlender::BlendRange(
		const PCGExData::FPointRef& A,
		const PCGExData::FPointRef& B,
		const int32 StartIndex,
		const int32 Range,
		const TArrayView<double>& Weights)
	{
		const int32 PrimaryIndex = A.Index;
		const int32 SecondaryIndex = B.Index;

		const int8 IsFirstOperation = FirstPointOperation[PrimaryIndex];
		for (const TSharedPtr<FDataBlendingOperationBase>& Op : Operations) { Op->DoRangeOperation(PrimaryIndex, SecondaryIndex, StartIndex, Weights, IsFirstOperation); }
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
		for (const TSharedPtr<FDataBlendingOperationBase>& Op : Operations) { Op->CompleteRangeOperation(StartIndex, Counts, TotalWeights); }

		if (bSkipProperties) { return; }

		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->CompleteRangeBlending(View, Counts, TotalWeights);
	}

	void FMetadataBlender::BlendRangeFromTo(
		const PCGExData::FPointRef& From,
		const PCGExData::FPointRef& To,
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

		for (const TSharedPtr<FDataBlendingOperationBase>& Op : Operations) { Op->DoRangeOperation(PrimaryIndex, SecondaryIndex, StartIndex, Weights, IsFirstOperation); }
		for (int i = 0; i < Weights.Num(); i++) { FirstPointOperation[StartIndex + i] = false; }

		if (bSkipProperties) { return; }

		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->BlendRangeFromTo(*From.Point, *To.Point, View, Weights);
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

			TSharedPtr<FDataBlendingOperationBase> Op;
			if (PCGEx::IsPCGExAttribute(Identity.Name)) { Op = CreateOperation(EPCGExDataBlendingType::Copy, Identity); }
			else { Op = CreateOperation(TypePtr, BlendingDetails->DefaultBlending, Identity); }

			if (!Op) { continue; }

			OperationIdMap.Add(Identity.Name, Op.Get());
			Operations.Add(Op);

			if (bSoftMode) { Op->SoftPrepareForData(InPrimaryFacade, InSecondaryFacade, SecondarySource); }
			else { Op->PrepareForData(InPrimaryFacade, InSecondaryFacade, SecondarySource); }
		}

		FirstPointOperation.Init(bInitFirstOperation, PrimaryPoints->Num());
	}
}
