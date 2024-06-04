// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/Blending/PCGExMetadataBlender.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExDataBlending.h"

namespace PCGExDataBlending
{
	FMetadataBlender::~FMetadataBlender()
	{
		Flush();
	}

	FMetadataBlender::FMetadataBlender(FPCGExBlendingSettings* InBlendingSettings)
	{
		BlendingSettings = InBlendingSettings;
	}

	FMetadataBlender::FMetadataBlender(const FMetadataBlender* ReferenceBlender)
	{
		BlendingSettings = ReferenceBlender->BlendingSettings;
	}

	void FMetadataBlender::PrepareForData(
		PCGExData::FPointIO& InData,
		const PCGExData::ESource SecondarySource,
		const bool bInitFirstOperation)
	{
		InternalPrepareForData(InData, InData, SecondarySource, bInitFirstOperation);
	}

	void FMetadataBlender::PrepareForData(
		PCGExData::FPointIO& InPrimaryData,
		const PCGExData::FPointIO& InSecondaryData,
		const PCGExData::ESource SecondarySource,
		const bool bInitFirstOperation)
	{
		InternalPrepareForData(InPrimaryData, InSecondaryData, SecondarySource, bInitFirstOperation);
	}

	void FMetadataBlender::PrepareForBlending(const PCGEx::FPointRef& Target, const FPCGPoint* Defaults) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared) { Op->PrepareOperation(Target.Index); }

		if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }

		PropertiesBlender->PrepareBlending(Target.MutablePoint(), Defaults ? *Defaults : *Target.Point);
	}

	void FMetadataBlender::PrepareForBlending(const int32 PrimaryIndex, const FPCGPoint* Defaults) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared) { Op->PrepareOperation(PrimaryIndex); }

		if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }

		PropertiesBlender->PrepareBlending((*PrimaryPoints)[PrimaryIndex], Defaults ? *Defaults : (*PrimaryPoints)[PrimaryIndex]);
	}

	void FMetadataBlender::Blend(
		const PCGEx::FPointRef& A,
		const PCGEx::FPointRef& B,
		const PCGEx::FPointRef& Target,
		const double Weight)
	{
		const bool IsFirstOperation = FirstPointOperation[A.Index];
		for (const FDataBlendingOperationBase* Op : Attributes) { Op->DoOperation(A.Index, B.Index, Target.Index, Weight, IsFirstOperation); }
		FirstPointOperation[A.Index] = false;

		if (bSkipProperties) { return; }

		PropertiesBlender->Blend(*A.Point, *B.Point, Target.MutablePoint(), Weight);
	}

	void FMetadataBlender::Blend(
		const int32 PrimaryIndex,
		const int32 SecondaryIndex,
		const int32 TargetIndex,
		const double Weight)
	{
		const bool IsFirstOperation = FirstPointOperation[PrimaryIndex];
		for (const FDataBlendingOperationBase* Op : Attributes) { Op->DoOperation(PrimaryIndex, SecondaryIndex, TargetIndex, Weight, IsFirstOperation); }
		FirstPointOperation[PrimaryIndex] = false;

		if (bSkipProperties) { return; }

		PropertiesBlender->Blend((*PrimaryPoints)[PrimaryIndex], (*SecondaryPoints)[SecondaryIndex], (*PrimaryPoints)[TargetIndex], Weight);
	}

	void FMetadataBlender::CompleteBlending(
		const PCGEx::FPointRef& Target,
		const int32 Count,
		const double TotalWeight) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBeCompleted) { Op->FinalizeOperation(Target.Index, Count, TotalWeight); }

		if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }

		PropertiesBlender->CompleteBlending(Target.MutablePoint(), Count, TotalWeight);
	}

	void FMetadataBlender::CompleteBlending(const int32 PrimaryIndex, const int32 Count, double TotalWeight) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBeCompleted) { Op->FinalizeOperation(PrimaryIndex, Count, TotalWeight); }

		if (bSkipProperties || !PropertiesBlender->bRequiresPrepare) { return; }

		PropertiesBlender->CompleteBlending((*PrimaryPoints)[PrimaryIndex], Count, TotalWeight);
	}

	void FMetadataBlender::PrepareRangeForBlending(
		const int32 StartIndex,
		const int32 Range) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared) { Op->PrepareRangeOperation(StartIndex, Range); }

		if (bSkipProperties) { return; }

		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->PrepareRangeBlending(View, (*PrimaryPoints)[StartIndex]);
	}

	void FMetadataBlender::BlendRange(
		const PCGEx::FPointRef& A,
		const PCGEx::FPointRef& B,
		const int32 StartIndex,
		const int32 Range,
		const TArrayView<double>& Weights)
	{
		const int32 PrimaryIndex = A.Index;
		const int32 SecondaryIndex = B.Index;

		const bool IsFirstOperation = FirstPointOperation[PrimaryIndex];
		for (const FDataBlendingOperationBase* Op : Attributes) { Op->DoRangeOperation(PrimaryIndex, SecondaryIndex, StartIndex, Weights, IsFirstOperation); }
		FirstPointOperation[PrimaryIndex] = false;

		if (bSkipProperties) { return; }

		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->BlendRange(*A.Point, *B.Point, View, Weights);
	}

	void FMetadataBlender::CompleteRangeBlending(
		const int32 StartIndex,
		const int32 Range,
		const TArrayView<int32>& Counts,
		const TArrayView<double>& TotalWeights) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared) { Op->FinalizeRangeOperation(StartIndex, Counts, TotalWeights); }

		if (bSkipProperties) { return; }

		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->CompleteRangeBlending(View, Counts, TotalWeights);
	}

	void FMetadataBlender::BlendRangeFromTo(
		const PCGEx::FPointRef& From,
		const PCGEx::FPointRef& To,
		const int32 StartIndex,
		const TArrayView<double>& Weights)
	{
		TArray<int32> Counts;
		Counts.SetNumUninitialized(Weights.Num());
		for (int32& C : Counts) { C = 2; }

		const int32 Range = Weights.Num();
		const int32 PrimaryIndex = From.Index;
		const int32 SecondaryIndex = To.Index;

		const bool IsFirstOperation = FirstPointOperation[PrimaryIndex];

		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared)
		{
			Op->PrepareRangeOperation(StartIndex, Range);
			Op->DoRangeOperation(PrimaryIndex, SecondaryIndex, StartIndex, Weights, IsFirstOperation);
			Op->FinalizeRangeOperation(StartIndex, Counts, Weights);
		}

		FirstPointOperation[PrimaryIndex] = false;

		if (bSkipProperties) { return; }

		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->BlendRangeFromTo(*From.Point, *To.Point, View, Weights);
	}

	void FMetadataBlender::Write(const bool bFlush)
	{
		for (FDataBlendingOperationBase* Op : Attributes) { Op->Write(); }
		if (bFlush) { Flush(); }
	}

	void FMetadataBlender::Flush()
	{
		FirstPointOperation.Empty();
		OperationIdMap.Empty();

		PCGEX_DELETE(PropertiesBlender)

		PCGEX_DELETE_TARRAY(Attributes)

		AttributesToBePrepared.Empty();
		AttributesToBeCompleted.Empty();

		PrimaryPoints = nullptr;
		SecondaryPoints = nullptr;
	}

	void FMetadataBlender::InternalPrepareForData(
		PCGExData::FPointIO& InPrimaryData,
		const PCGExData::FPointIO& InSecondaryData,
		const PCGExData::ESource SecondarySource,
		const bool bInitFirstOperation)
	{
		Flush();

		bSkipProperties = !bBlendProperties;
		if (!bSkipProperties)
		{
			PropertiesBlender = new FPropertiesBlender(BlendingSettings->GetPropertiesBlendingSettings());
			if (PropertiesBlender->bHasNoBlending)
			{
				bSkipProperties = true;
				PCGEX_DELETE(PropertiesBlender)
			}
		}

		InPrimaryData.CreateOutKeys();
		const_cast<PCGExData::FPointIO&>(InSecondaryData).CreateKeys(SecondarySource);

		PrimaryPoints = &InPrimaryData.GetOut()->GetMutablePoints();
		SecondaryPoints = const_cast<TArray<FPCGPoint>*>(&InSecondaryData.GetData(SecondarySource)->GetPoints());

		TArray<PCGEx::FAttributeIdentity> Identities;
		PCGEx::FAttributeIdentity::Get(InPrimaryData.GetOut()->Metadata, Identities);
		BlendingSettings->Filter(Identities);

		if (&InSecondaryData != &InPrimaryData)
		{
			TArray<FName> PrimaryNames;
			TArray<FName> SecondaryNames;
			TMap<FName, PCGEx::FAttributeIdentity> PrimaryIdentityMap;
			TMap<FName, PCGEx::FAttributeIdentity> SecondaryIdentityMap;
			PCGEx::FAttributeIdentity::Get(InPrimaryData.GetOut()->Metadata, PrimaryNames, PrimaryIdentityMap);
			PCGEx::FAttributeIdentity::Get(InSecondaryData.GetData(SecondarySource)->Metadata, SecondaryNames, SecondaryIdentityMap);

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
				else if (BlendingSettings->CanBlend(SecondaryIdentity.Name))
				{
					//Operation will handle missing attribute creation.
					Identities.Add(SecondaryIdentity);
				}
			}
		}

		Attributes.Empty(Identities.Num());
		AttributesToBeCompleted.Empty(Identities.Num());
		AttributesToBePrepared.Empty(Identities.Num());

		for (const PCGEx::FAttributeIdentity& Identity : Identities)
		{
			const EPCGExDataBlendingType* TypePtr = BlendingSettings->AttributesOverrides.Find(Identity.Name);
			FDataBlendingOperationBase* Op = CreateOperation(TypePtr ? *TypePtr : BlendingSettings->DefaultBlending, Identity);

			if (!Op) { continue; }

			OperationIdMap.Add(Identity.Name, Op);

			Attributes.Add(Op);
			if (Op->GetRequiresPreparation()) { AttributesToBePrepared.Add(Op); }
			if (Op->GetRequiresFinalization()) { AttributesToBeCompleted.Add(Op); }

			Op->PrepareForData(InPrimaryData, InSecondaryData, SecondarySource);
		}

		FirstPointOperation.SetNum(PrimaryPoints->Num());

		if (bInitFirstOperation) { for (bool& bFirstOp : FirstPointOperation) { bFirstOp = true; } }
		else { for (bool& bFirstOp : FirstPointOperation) { bFirstOp = false; } }
	}
}
