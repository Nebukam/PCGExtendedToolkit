// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/Blending/PCGExMetadataBlender.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Data/Blending/PCGExDataBlending.h"

namespace PCGExDataBlending
{
	FMetadataBlender::~FMetadataBlender()
	{
		Flush();
	}

	FMetadataBlender::FMetadataBlender()
		: FMetadataBlender(EPCGExDataBlendingType::Copy)
	{
	}

	FMetadataBlender::FMetadataBlender(const EPCGExDataBlendingType InDefaultBlending)
	{
		DefaultOperation = InDefaultBlending;
	}

	FMetadataBlender::FMetadataBlender(const FMetadataBlender* ReferenceBlender)
	{
		DefaultOperation = ReferenceBlender->DefaultOperation;
	}

	void FMetadataBlender::PrepareForData(PCGExData::FPointIO& InData, const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides)
	{
		InternalPrepareForData(InData, InData, OperationTypeOverrides, true);
	}

	void FMetadataBlender::PrepareForData(
		PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData,
		const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides, bool bSecondaryIn)
	{
		InternalPrepareForData(InPrimaryData, InSecondaryData, OperationTypeOverrides, bSecondaryIn);
	}

	FMetadataBlender* FMetadataBlender::Copy(PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData) const
	{
		FMetadataBlender* Copy = new FMetadataBlender(this);
		Copy->PrepareForData(InPrimaryData, InSecondaryData, BlendingOverrides, true);
		return Copy;
	}

	void FMetadataBlender::PrepareForBlending(const int32 WriteKey) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared) { Op->PrepareOperation(WriteKey); }
	}

	void FMetadataBlender::Blend(
		const int32 PrimaryReadIndex,
		const int32 SecondaryReadIndex,
		const int32 WriteIndex,
		const double Alpha) const
	{
		for (const FDataBlendingOperationBase* Op : Attributes) { Op->DoOperation(PrimaryReadIndex, SecondaryReadIndex, WriteIndex, Alpha); }
	}

	void FMetadataBlender::CompleteBlending(
		const int32 WriteIndex,
		const double Alpha) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBeCompleted) { Op->FinalizeOperation(WriteIndex, Alpha); }
	}

	void FMetadataBlender::PrepareRangeForBlending(
		const int32 StartIndex,
		const int32 Count) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared) { Op->PrepareRangeOperation(StartIndex, Count); }
	}

	void FMetadataBlender::BlendRange(
		const int32 PrimaryReadIndex,
		const int32 SecondaryReadIndex,
		const int32 StartIndex,
		const int32 Count,
		const TArrayView<double>& Alphas) const
	{
		for (const FDataBlendingOperationBase* Op : Attributes) { Op->DoRangeOperation(PrimaryReadIndex, SecondaryReadIndex, StartIndex, Count, Alphas); }
	}

	void FMetadataBlender::CompleteRangeBlending(
		const int32 StartIndex,
		const int32 Count,
		const TArrayView<double>& Alphas) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared) { Op->FinalizeRangeOperation(StartIndex, Count, Alphas); }
	}

	void FMetadataBlender::BlendRangeOnce(
		const int32 PrimaryReadIndex,
		const int32 SecondaryReadIndex,
		const int32 StartIndex,
		const int32 Count,
		const TArrayView<double>& Alphas) const
	{
		PrepareRangeForBlending(StartIndex, Count);
		BlendRange(PrimaryReadIndex, SecondaryReadIndex, StartIndex, Count, Alphas);
		CompleteRangeBlending(StartIndex, Count, Alphas);
	}

	void FMetadataBlender::FullBlendToOne(const TArrayView<double>& Alphas) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared) { Op->FullBlendToOne(Alphas); }
	}

	void FMetadataBlender::ResetToDefaults(const int32 WriteIndex) const
	{
		for (const FDataBlendingOperationBase* Op : Attributes) { Op->ResetToDefault(WriteIndex); }
	}

	void FMetadataBlender::Write()
	{
		for (FDataBlendingOperationBase* Op : Attributes) { Op->Write(); }
	}

	void FMetadataBlender::Flush()
	{
		PCGEX_DELETE_TARRAY(Attributes)

		BlendingOverrides.Empty();
		AttributesToBePrepared.Empty();
		AttributesToBeCompleted.Empty();
	}

	void FMetadataBlender::InternalPrepareForData(
		PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData,
		const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides, bool bSecondaryIn)
	{
		Flush();

		InPrimaryData.CreateOutKeys();
		const_cast<PCGExData::FPointIO&>(InSecondaryData).CreateInKeys(); //Ugh
		
		BlendingOverrides = OperationTypeOverrides;

		TArray<PCGEx::FAttributeIdentity> Identities;
		PCGEx::FAttributeIdentity::Get(InPrimaryData.GetOut(), Identities);

		if (&InSecondaryData != &InPrimaryData)
		{
			TArray<FName> PrimaryNames;
			TArray<FName> SecondaryNames;
			TMap<FName, PCGEx::FAttributeIdentity> PrimaryIdentityMap;
			TMap<FName, PCGEx::FAttributeIdentity> SecondaryIdentityMap;
			PCGEx::FAttributeIdentity::Get(InPrimaryData.GetOut(), PrimaryNames, PrimaryIdentityMap);
			PCGEx::FAttributeIdentity::Get(InSecondaryData.GetIn(), SecondaryNames, SecondaryIdentityMap);

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
			const EPCGExDataBlendingType* TypePtr = OperationTypeOverrides.Find(Identity.Name);
			FDataBlendingOperationBase* Op = CreateOperation(TypePtr ? *TypePtr : DefaultOperation, Identity);

			if (!Op) { continue; }

			Attributes.Add(Op);
			if (Op->GetRequiresPreparation()) { AttributesToBePrepared.Add(Op); }
			if (Op->GetRequiresFinalization()) { AttributesToBeCompleted.Add(Op); }

			Op->PrepareForData(InPrimaryData, InSecondaryData, bSecondaryIn);
		}
	}
}
