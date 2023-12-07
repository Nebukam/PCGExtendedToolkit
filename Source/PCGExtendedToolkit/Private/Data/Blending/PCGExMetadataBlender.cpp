// Fill out your copyright notice in the Description page of Project Settings.


#include "..\..\..\Public\Data\Blending\PCGExMetadataBlender.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Data/Blending/PCGExDataBlending.h"

namespace PCGExDataBlending
{
	FMetadataBlender::~FMetadataBlender()
	{
		Flush();
	}

	void FMetadataBlender::PrepareForData(UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData)
	{
		const TMap<FName, EPCGExDataBlendingType> NoOverrides;
		PrepareForData(InPrimaryData, InSecondaryData ? InSecondaryData : InPrimaryData, NoOverrides);
	}

	void FMetadataBlender::PrepareForData(UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData, const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides)
	{
		InternalPrepareForData(InPrimaryData, InSecondaryData ? InSecondaryData : InPrimaryData, OperationTypeOverrides);
	}

	FMetadataBlender* FMetadataBlender::Copy(UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData) const
	{
		FMetadataBlender* Copy = NewObject<FMetadataBlender>();
		Copy->DefaultOperation = DefaultOperation;
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
			if (PrimaryKeys) { delete PrimaryKeys; }
		}
		else
		{
			if (PrimaryKeys) { delete PrimaryKeys; }
			if (SecondaryKeys) { delete SecondaryKeys; }
		}

		PrimaryKeys = nullptr;
		SecondaryKeys = nullptr;
	}

	void FMetadataBlender::InternalPrepareForData(UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData, const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides)
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

		const TArrayView<FPCGPoint> View(InPrimaryData->GetMutablePoints());
		PrimaryKeys = new FPCGAttributeAccessorKeysPoints(View);

		if (InPrimaryData != InSecondaryData)
		{
			SecondaryKeys = new FPCGAttributeAccessorKeysPoints(InSecondaryData->GetPoints());
		}
		else
		{
			SecondaryKeys = PrimaryKeys;
		}

		for (const PCGEx::FAttributeIdentity& Identity : Identities)
		{
			const EPCGExDataBlendingType* TypePtr = OperationTypeOverrides.Find(Identity.Name);
			FDataBlendingOperationBase* Op = PCGExDataBlending::CreateOperation(TypePtr ? *TypePtr : DefaultOperation, Identity);

			if (!Op) { continue; }

			Attributes.Add(Op);
			if (Op->GetRequiresPreparation()) { AttributesToBePrepared.Add(Op); }
			if (Op->GetRequiresFinalization()) { AttributesToBeCompleted.Add(Op); }

			Op->PrepareForData(InPrimaryData, Mismatch.Contains(Identity.Name) ? InSecondaryData : InPrimaryData, PrimaryKeys, SecondaryKeys);
		}
	}
}
