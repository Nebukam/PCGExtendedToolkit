// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/Blending/PCGExMetadataBlender.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Data/Blending/PCGExDataBlending.h"

void UPCGExMetadataBlender::PrepareForData(const UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData)
{
	const TMap<FName, EPCGExDataBlendingType> NoOverrides;
	PrepareForData(InPrimaryData, InSecondaryData ? InSecondaryData : InPrimaryData, NoOverrides);
}

void UPCGExMetadataBlender::PrepareForData(const UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData, const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides)
{
	InternalPrepareForData(InPrimaryData, InSecondaryData ? InSecondaryData : InPrimaryData, OperationTypeOverrides);
}

UPCGExMetadataBlender* UPCGExMetadataBlender::Copy(const UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData) const
{
	UPCGExMetadataBlender* Copy = NewObject<UPCGExMetadataBlender>();
	Copy->DefaultOperation = DefaultOperation;
	Copy->PrepareForData(InPrimaryData, InSecondaryData, BlendingOverrides);
	return Copy;
}

void UPCGExMetadataBlender::PrepareForBlending(const PCGMetadataEntryKey InPrimaryOutputKey) const
{
	for (const UPCGExDataBlendingOperation* Op : AttributesToBePrepared) { Op->PrepareOperation(InPrimaryOutputKey); }
}

void UPCGExMetadataBlender::Blend(const PCGMetadataEntryKey InPrimaryKey, const PCGMetadataEntryKey InSecondaryKey, const PCGMetadataEntryKey InPrimaryOutputKey, const double Alpha) const
{
	for (const UPCGExDataBlendingOperation* Op : Attributes) { Op->DoOperation(InPrimaryKey, InSecondaryKey, InPrimaryOutputKey, Alpha); }
}

void UPCGExMetadataBlender::CompleteBlending(const PCGMetadataEntryKey InPrimaryOutputKey, const double Alpha) const
{
	for (const UPCGExDataBlendingOperation* Op : AttributesToBeCompleted) { Op->FinalizeOperation(InPrimaryOutputKey, Alpha); }
}

void UPCGExMetadataBlender::ResetToDefaults(const PCGMetadataEntryKey InPrimaryOutputKey) const
{
	for (const UPCGExDataBlendingOperation* Op : Attributes) { Op->ResetToDefault(InPrimaryOutputKey); }
}

void UPCGExMetadataBlender::Flush()
{
	for (UPCGExDataBlendingOperation* Op : Attributes)
	{
		Op->Flush();
		Op->ConditionalBeginDestroy();
	}
	
	BlendingOverrides.Empty();
	Attributes.Empty();
	AttributesToBePrepared.Empty();
	AttributesToBeCompleted.Empty();
}

void UPCGExMetadataBlender::BeginDestroy()
{
	Flush();
	UObject::BeginDestroy();
}

void UPCGExMetadataBlender::InternalPrepareForData(const UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData, const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides)
{
	
	Flush();
	BlendingOverrides = OperationTypeOverrides;

	TArray<PCGEx::FAttributeIdentity> Identities;
	TSet<FName> Mismatch;

	PCGEx::GetAttributeIdentities(InPrimaryData, Identities);

	if (InSecondaryData != InPrimaryData)
	{
		TArray<FName> PrimaryNames;
		TArray<FName> SecondaryNames;
		TMap<FName, PCGEx::FAttributeIdentity> PrimaryIdentityMap;
		TMap<FName, PCGEx::FAttributeIdentity> SecondaryIdentityMap;
		PCGEx::GetAttributeIdentities(InPrimaryData, PrimaryNames, PrimaryIdentityMap);
		PCGEx::GetAttributeIdentities(InSecondaryData, SecondaryNames, SecondaryIdentityMap);

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

	for (const PCGEx::FAttributeIdentity& Identity : Identities)
	{
		const EPCGExDataBlendingType* TypePtr = OperationTypeOverrides.Find(Identity.Name);
		UPCGExDataBlendingOperation* Op = PCGExDataBlending::CreateOperation(TypePtr ? *TypePtr : DefaultOperation, Identity);

		if (!Op) { continue; }

		Attributes.Add(Op);
		if (Op->GetRequiresPreparation()) { AttributesToBePrepared.Add(Op); }
		if (Op->GetRequiresFinalization()) { AttributesToBeCompleted.Add(Op); }

		Op->PrepareForData(InPrimaryData, Mismatch.Contains(Identity.Name) ? InSecondaryData : InPrimaryData);
	}
}
