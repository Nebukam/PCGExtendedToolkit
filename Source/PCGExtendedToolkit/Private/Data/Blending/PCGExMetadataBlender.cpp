// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/Blending/PCGExMetadataBlender.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Data/Blending/PCGExSAO.h"

void UPCGExMetadataBlender::PrepareForData(const UPCGPointData* InData, const TMap<FName, EPCGExMetadataBlendingOperationType>& OperationTypeOverrides)
{
	for (UPCGExMetadataOperation* Op : Attributes) { Op->ConditionalBeginDestroy(); }

	TArray<PCGEx::FAttributeIdentity> Identities;
	PCGEx::GetAttributeIdentities(InData, Identities);

	Attributes.Empty(Identities.Num());
	AttributesFinalizers.Empty(Identities.Num());
	AttributesPrep.Empty(Identities.Num());

	for (const PCGEx::FAttributeIdentity& Identity : Identities)
	{
		const EPCGExMetadataBlendingOperationType* TypePtr = OperationTypeOverrides.Find(Identity.Name);
		UPCGExMetadataOperation* Op = PCGExSAO::CreateOperation(TypePtr ? *TypePtr : DefaultOperation, Identity);

		if (!Op) { continue; }

		Attributes.Add(Op);
		if (Op->UsePreparation()) { AttributesPrep.Add(Op); }
		if (Op->UseFinalize()) { AttributesFinalizers.Add(Op); }
		Op->PrepareForData(InData);
	}
}

void UPCGExMetadataBlender::PrepareForOperations(const PCGMetadataEntryKey OutputKey) const
{
	for (const UPCGExMetadataOperation* Op : AttributesPrep) { Op->PrepareOperation(OutputKey); }
}

void UPCGExMetadataBlender::DoOperations(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha) const
{
	for (const UPCGExMetadataOperation* Op : Attributes) { Op->DoOperation(OperandAKey, OperandBKey, OutputKey, Alpha); }
}

void UPCGExMetadataBlender::FinalizeOperations(const PCGMetadataEntryKey OutputKey, const double Alpha) const
{
	for (const UPCGExMetadataOperation* Op : AttributesFinalizers) { Op->FinalizeOperation(OutputKey, Alpha); }
}

void UPCGExMetadataBlender::ResetToDefaults(const PCGMetadataEntryKey OutputKey) const
{
	for (const UPCGExMetadataOperation* Op : Attributes) { Op->ResetToDefault(OutputKey); }
}
