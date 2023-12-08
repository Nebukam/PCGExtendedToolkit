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

	void FMetadataBlender::PrepareForData(UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData)
	{
		const TMap<FName, EPCGExDataBlendingType> NoOverrides;
		PrepareForData(InPrimaryData, InSecondaryData ? InSecondaryData : InPrimaryData, NoOverrides);
	}

	void FMetadataBlender::PrepareForData(UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData, const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides)
	{
		PrepareForData(InPrimaryData, InSecondaryData ? InSecondaryData : InPrimaryData, nullptr, nullptr, OperationTypeOverrides);
	}

	void FMetadataBlender::PrepareForData(
		UPCGPointData* InPrimaryData,
		const UPCGPointData* InSecondaryData,
		FPCGAttributeAccessorKeysPoints* InPrimaryKeys,
		FPCGAttributeAccessorKeysPoints* InSecondaryKeys,
		const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides)
	{
		InternalPrepareForData(InPrimaryData, InSecondaryData ? InSecondaryData : InPrimaryData, InPrimaryKeys, InSecondaryKeys, OperationTypeOverrides);
	}

	FMetadataBlender* FMetadataBlender::Copy(UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData) const
	{
		FMetadataBlender* Copy = new FMetadataBlender(this);
		Copy->PrepareForData(InPrimaryData, InSecondaryData, BlendingOverrides);
		return Copy;
	}

	void FMetadataBlender::PrepareForBlending(const int32 WriteKey) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared) { Op->PrepareOperation(WriteKey); }
	}

	void FMetadataBlender::Blend(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 WriteIndex, const double Alpha) const
	{
		for (const FDataBlendingOperationBase* Op : Attributes) { Op->DoOperation(PrimaryReadIndex, SecondaryReadIndex, WriteIndex, Alpha); }
	}

	void FMetadataBlender::CompleteBlending(const int32 WriteIndex, const double Alpha) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBeCompleted) { Op->FinalizeOperation(WriteIndex, Alpha); }
	}

	void FMetadataBlender::PrepareRangeForBlending(const int32 StartIndex, const int32 Count) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared) { Op->PrepareRangeOperation(StartIndex, Count); }
	}

	void FMetadataBlender::BlendRange(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 StartIndex, const int32 Count, const TArrayView<double>& Alphas) const
	{
		for (const FDataBlendingOperationBase* Op : Attributes) { Op->DoRangeOperation(PrimaryReadIndex, SecondaryReadIndex, StartIndex, Count, Alphas); }
	}

	void FMetadataBlender::CompleteRangeBlending(const int32 StartIndex, const int32 Count, const TArrayView<double>& Alphas) const
	{
		for (const FDataBlendingOperationBase* Op : AttributesToBePrepared) { Op->FinalizeRangeOperation(StartIndex, Count, Alphas); }
	}

	void FMetadataBlender::BlendRangeOnce(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 StartIndex, const int32 Count, const TArrayView<double>& Alphas) const
	{
		PrepareRangeForBlending(StartIndex, Count);
		BlendRange(PrimaryReadIndex, SecondaryReadIndex, StartIndex, Count, Alphas);
		CompleteRangeBlending(StartIndex, Count, Alphas);
	}

	void FMetadataBlender::ResetToDefaults(const int32 WriteIndex) const
	{
		for (const FDataBlendingOperationBase* Op : Attributes) { Op->ResetToDefault(WriteIndex); }
	}

	void FMetadataBlender::Flush()
	{
		for (FDataBlendingOperationBase* Op : Attributes) { delete Op; }

		BlendingOverrides.Empty();
		Attributes.Empty();
		AttributesToBePrepared.Empty();
		AttributesToBeCompleted.Empty();

		if (PrimaryKeys == SecondaryKeys)
		{
			if (PrimaryKeys && bOwnsPrimaryKeys) { delete PrimaryKeys; }
		}
		else
		{
			if (PrimaryKeys && bOwnsPrimaryKeys) { delete PrimaryKeys; }
			if (SecondaryKeys && bOwnsSecondaryKeys) { delete SecondaryKeys; }
		}

		PrimaryKeys = nullptr;
		SecondaryKeys = nullptr;
	}

	void FMetadataBlender::InternalPrepareForData(
		UPCGPointData* InPrimaryData,
		const UPCGPointData* InSecondaryData,
		FPCGAttributeAccessorKeysPoints* InPrimaryKeys,
		FPCGAttributeAccessorKeysPoints* InSecondaryKeys,
		const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides)
	{
		Flush();
		BlendingOverrides = OperationTypeOverrides;

		TArray<PCGEx::FAttributeIdentity> Identities;
		TSet<FName> Mismatch;

		GetAttributeIdentities(InPrimaryData, Identities);

		if (InSecondaryData != InPrimaryData)
		{
			TArray<FName> PrimaryNames;
			TArray<FName> SecondaryNames;
			TMap<FName, PCGEx::FAttributeIdentity> PrimaryIdentityMap;
			TMap<FName, PCGEx::FAttributeIdentity> SecondaryIdentityMap;
			GetAttributeIdentities(InPrimaryData, PrimaryNames, PrimaryIdentityMap);
			GetAttributeIdentities(InSecondaryData, SecondaryNames, SecondaryIdentityMap);

			for (FName SecondaryName : SecondaryNames)
			{
				const PCGEx::FAttributeIdentity& SecondaryIdentity = *SecondaryIdentityMap.Find(SecondaryName);
				if (const PCGEx::FAttributeIdentity* PrimaryIdentityPtr = PrimaryIdentityMap.Find(SecondaryName))
				{
					if (PrimaryIdentityPtr->UnderlyingType != SecondaryIdentity.UnderlyingType)
					{
						Mismatch.Add(SecondaryName);
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

		if (!InPrimaryKeys)
		{
			const TArrayView<FPCGPoint> View(InPrimaryData->GetMutablePoints());
			PrimaryKeys = new FPCGAttributeAccessorKeysPoints(View);
			bOwnsPrimaryKeys = true;
		}
		else
		{
			PrimaryKeys = InPrimaryKeys;
			bOwnsPrimaryKeys = false;
		}

		if (InPrimaryData != InSecondaryData)
		{
			if (!InSecondaryKeys)
			{
				SecondaryKeys = new FPCGAttributeAccessorKeysPoints(InSecondaryData->GetPoints());
				bOwnsSecondaryKeys = true;
			}
			else
			{
				SecondaryKeys = InSecondaryKeys;
				bOwnsSecondaryKeys = false;
			}
		}
		else
		{
			SecondaryKeys = PrimaryKeys;
			bOwnsSecondaryKeys = bOwnsPrimaryKeys;
		}

		for (const PCGEx::FAttributeIdentity& Identity : Identities)
		{
			const EPCGExDataBlendingType* TypePtr = OperationTypeOverrides.Find(Identity.Name);
			FDataBlendingOperationBase* Op = CreateOperation(TypePtr ? *TypePtr : DefaultOperation, Identity);

			if (!Op) { continue; }

			Attributes.Add(Op);
			if (Op->GetRequiresPreparation()) { AttributesToBePrepared.Add(Op); }
			if (Op->GetRequiresFinalization()) { AttributesToBeCompleted.Add(Op); }

			Op->PrepareForData(InPrimaryData, Mismatch.Contains(Identity.Name) ? InSecondaryData : InPrimaryData, PrimaryKeys, SecondaryKeys);
		}
	}
}
