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

	void FMetadataBlender::PrepareForData(PCGExData::FPointIO& InData)
	{
		InternalPrepareForData(InData, InData, true);
	}

	void FMetadataBlender::PrepareForData(
		PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData,
		const bool bSecondaryIn)
	{
		InternalPrepareForData(InPrimaryData, InSecondaryData, bSecondaryIn);
	}

	FMetadataBlender* FMetadataBlender::Copy(PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData) const
	{
		FMetadataBlender* Copy = new FMetadataBlender(this);
		Copy->PrepareForData(InPrimaryData, InSecondaryData, true);
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
		const double Alpha) const
	{
		for (const FDataBlendingOperationBase* Op : Attributes) { Op->DoOperation(A.Index, B.Index, Target.Index, Alpha); }
		if (!bBlendProperties) { return; }
		PropertiesBlender->Blend(*A.Point, *B.Point, Target.MutablePoint(), Alpha);
	}

	void FMetadataBlender::CompleteBlending(
		const PCGEx::FPointRef& Target,
		const double Alpha) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBeCompleted) { Op->FinalizeOperation(Target.Index, Alpha); }
		if (!bBlendProperties || !PropertiesBlender->bRequiresPrepare) { return; }
		PropertiesBlender->CompleteBlending(Target.MutablePoint(), Alpha);
	}

	void FMetadataBlender::PrepareRangeForBlending(
		const int32 StartIndex,
		const int32 Count) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared) { Op->PrepareRangeOperation(StartIndex, Count); }
		if (!bBlendProperties) { return; }
		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Count);
		PropertiesBlender->PrepareRangeBlending(View, (*PrimaryPoints)[StartIndex]);
	}

	void FMetadataBlender::BlendRange(
		const PCGEx::FPointRef& A,
		const PCGEx::FPointRef& B,
		const int32 StartIndex,
		const int32 Count,
		const TArrayView<double>& Alphas) const
	{
		for (const FDataBlendingOperationBase* Op : Attributes) { Op->DoRangeOperation(A.Index, B.Index, StartIndex, Count, Alphas); }
		if (!bBlendProperties) { return; }
		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Count);
		PropertiesBlender->BlendRange(*A.Point, *B.Point, View, Alphas);
	}

	void FMetadataBlender::CompleteRangeBlending(
		const int32 StartIndex,
		const int32 Count,
		const TArrayView<double>& Alphas) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared) { Op->FinalizeRangeOperation(StartIndex, Count, Alphas); }
		if (!bBlendProperties) { return; }
		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Count);
		PropertiesBlender->CompleteRangeBlending(View, 2);
	}

	void FMetadataBlender::BlendRangeOnce(
		const PCGEx::FPointRef& A,
		const PCGEx::FPointRef& B,
		const int32 StartIndex,
		const int32 Count,
		const TArrayView<double>& Alphas) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared)
		{
			Op->PrepareRangeOperation(StartIndex, Count);
			Op->DoRangeOperation(A.Index, B.Index, StartIndex, Count, Alphas);
			Op->FinalizeRangeOperation(StartIndex, Count, Alphas);
		}

		if (!bBlendProperties) { return; }
		const TArrayView<FPCGPoint> View = MakeArrayView(PrimaryPoints->GetData() + StartIndex, Count);
		PropertiesBlender->BlendRangeOnce(*A.Point, *B.Point, View, Alphas);
	}

	void FMetadataBlender::FullBlendToOne(const TArrayView<double>& Alphas) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared) { Op->FullBlendToOne(Alphas); }
		if (!bBlendProperties) { return; }
		for (int i = 0; i < PrimaryPoints->Num(); i++)
		{
			PropertiesBlender->BlendOnce((*PrimaryPoints)[i], (*SecondaryPoints)[i], (*PrimaryPoints)[i], Alphas[i]);
		}
	}

	void FMetadataBlender::Write(const bool bFlush)
	{
		for (FDataBlendingOperationBase* Op : Attributes) { Op->Write(); }
		if (bFlush) { Flush(); }
	}

	void FMetadataBlender::Flush()
	{
		PCGEX_DELETE(PropertiesBlender)

		PCGEX_DELETE_TARRAY(Attributes)

		AttributesToBePrepared.Empty();
		AttributesToBeCompleted.Empty();

		PrimaryPoints = nullptr;
		SecondaryPoints = nullptr;
	}

	void FMetadataBlender::InternalPrepareForData(
		PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData, const bool bSecondaryIn)
	{
		Flush();

		PropertiesBlender = new FPropertiesBlender(*BlendingSettings);

		InPrimaryData.CreateOutKeys();
		const_cast<PCGExData::FPointIO&>(InSecondaryData).CreateInKeys(); //Ugh

		PrimaryPoints = &InPrimaryData.GetOut()->GetMutablePoints();
		SecondaryPoints = bSecondaryIn ?
			                  const_cast<TArray<FPCGPoint>*>(&InSecondaryData.GetIn()->GetPoints()) :
			                  &InSecondaryData.GetOut()->GetMutablePoints();


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
			PCGEx::FAttributeIdentity::Get(InSecondaryData.GetIn()->Metadata, SecondaryNames, SecondaryIdentityMap);

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

			Attributes.Add(Op);
			if (Op->GetRequiresPreparation()) { AttributesToBePrepared.Add(Op); }
			if (Op->GetRequiresFinalization()) { AttributesToBeCompleted.Add(Op); }

			Op->PrepareForData(InPrimaryData, InSecondaryData, bSecondaryIn);
		}
	}
}
