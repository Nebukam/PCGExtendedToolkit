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

	void FMetadataBlender::PrepareForData(PCGExData::FPointIO& InData, const PCGExData::ESource SecondarySource)
	{
		InternalPrepareForData(InData, InData, SecondarySource);
	}

	void FMetadataBlender::PrepareForData(
		PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData,
		const PCGExData::ESource SecondarySource)
	{
		InternalPrepareForData(InPrimaryData, InSecondaryData, SecondarySource);
	}

	FMetadataBlender* FMetadataBlender::Copy(PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData) const
	{
		FMetadataBlender* Copy = new FMetadataBlender(this);
		Copy->PrepareForData(InPrimaryData, InSecondaryData, PCGExData::ESource::Out);
		return Copy;
	}

	void FMetadataBlender::PrepareForBlending(const PCGEx::FPointRef& Target, const FPCGPoint* Defaults) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared) { Op->PrepareOperation(Target.Index); }
		if (!bBlendProperties || !PropertiesBlender->bRequiresPrepare) { return; }
		PropertiesBlender->PrepareBlending(Target.MutablePoint(), Defaults ? *Defaults : *Target.Point);
	}

	void FMetadataBlender::Blend(
		const PCGEx::FPointRef& A,
		const PCGEx::FPointRef& B,
		const PCGEx::FPointRef& Target,
		const double Weight) const
	{
		for (const FDataBlendingOperationBase* Op : Attributes) { Op->DoOperation(A.Index, B.Index, Target.Index, Weight); }
		if (!bBlendProperties) { return; }
		PropertiesBlender->Blend(*A.Point, *B.Point, Target.MutablePoint(), Weight);
	}

	void FMetadataBlender::CompleteBlending(
		const PCGEx::FPointRef& Target,
		const int32 Count,
		const double TotalWeight) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBeCompleted) { Op->FinalizeOperation(Target.Index, Count, TotalWeight); }
		if (!bBlendProperties || !PropertiesBlender->bRequiresPrepare) { return; }
		PropertiesBlender->CompleteBlending(Target.MutablePoint(), Count, TotalWeight);
	}

	void FMetadataBlender::PrepareRangeForBlending(
		const int32 StartIndex,
		const int32 Range) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared) { Op->PrepareRangeOperation(StartIndex, Range); }
		if (!bBlendProperties) { return; }
		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->PrepareRangeBlending(View, (*PrimaryPoints)[StartIndex]);
	}

	void FMetadataBlender::BlendRange(
		const PCGEx::FPointRef& A,
		const PCGEx::FPointRef& B,
		const int32 StartIndex,
		const int32 Range,
		const TArrayView<double>& Weights) const
	{
		for (const FDataBlendingOperationBase* Op : Attributes) { Op->DoRangeOperation(A.Index, B.Index, StartIndex, Weights); }
		if (!bBlendProperties) { return; }
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
		if (!bBlendProperties) { return; }
		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->CompleteRangeBlending(View, Counts, TotalWeights);
	}

	void FMetadataBlender::BlendRangeFromTo(
		const PCGEx::FPointRef& From,
		const PCGEx::FPointRef& To,
		const int32 StartIndex,
		const TArrayView<double>& Weights) const
	{
		TArray<int32> Counts;
		Counts.SetNumUninitialized(Weights.Num());
		for (int32& C : Counts) { C = 2; }

		const int32 Range = Weights.Num();

		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared)
		{
			Op->PrepareRangeOperation(StartIndex, Range);
			Op->DoRangeOperation(From.Index, To.Index, StartIndex, Weights);
			Op->FinalizeRangeOperation(StartIndex, Counts, Weights);
		}

		if (!bBlendProperties) { return; }
		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Range);
		PropertiesBlender->BlendRangeFromTo(*From.Point, *To.Point, View, Weights);
	}

	void FMetadataBlender::BlendEachPrimaryToSecondary(const TArrayView<double>& Weights) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared) { Op->BlendEachPrimaryToSecondary(Weights); }
		if (!bBlendProperties) { return; }
		for (int i = 0; i < PrimaryPoints->Num(); i++)
		{
			PropertiesBlender->BlendOnce((*PrimaryPoints)[i], (*SecondaryPoints)[i], (*PrimaryPoints)[i], Weights[i]);
		}
	}

	void FMetadataBlender::Write(const bool bFlush)
	{
		for (FDataBlendingOperationBase* Op : Attributes) { Op->Write(); }
		if (bFlush) { Flush(); }
	}

	void FMetadataBlender::Flush()
	{
		OperationIdMap.Empty();
		
		PCGEX_DELETE(PropertiesBlender)
		
		PCGEX_DELETE_TARRAY(Attributes)

		AttributesToBePrepared.Empty();
		AttributesToBeCompleted.Empty();

		PrimaryPoints = nullptr;
		SecondaryPoints = nullptr;
	}

	void FMetadataBlender::InitializeFromScratch()
	{
		for (FDataBlendingOperationBase* Op : Attributes) { Op->InitializeFromScratch(); }
	}

	void FMetadataBlender::InternalPrepareForData(
		PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData, const PCGExData::ESource SecondarySource)
	{
		Flush();

		PropertiesBlender = new FPropertiesBlender(*BlendingSettings);

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
				else
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
	}
}
