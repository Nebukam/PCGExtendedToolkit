// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExValencyConnectorSet.h"

#define LOCTEXT_NAMESPACE "PCGExValencyConnectorSet"

#pragma region UPCGExValencyConnectorSet

int32 UPCGExValencyConnectorSet::FindConnectorTypeIndex(FName ConnectorType) const
{
	for (int32 i = 0; i < ConnectorTypes.Num(); ++i)
	{
		if (ConnectorTypes[i].ConnectorType == ConnectorType)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

bool UPCGExValencyConnectorSet::AreTypesCompatible(int32 TypeIndexA, int32 TypeIndexB) const
{
	if (!CompatibilityMatrix.IsValidIndex(TypeIndexA) || TypeIndexB < 0 || TypeIndexB >= 64)
	{
		return false;
	}

	const int64 Mask = CompatibilityMatrix[TypeIndexA];
	return (Mask & (1LL << TypeIndexB)) != 0;
}

int64 UPCGExValencyConnectorSet::GetCompatibilityMask(int32 TypeIndex) const
{
	return CompatibilityMatrix.IsValidIndex(TypeIndex) ? CompatibilityMatrix[TypeIndex] : 0;
}

void UPCGExValencyConnectorSet::Compile()
{
	// Assign bit indices to connector types
	const int32 NumTypes = FMath::Min(ConnectorTypes.Num(), 64);
	for (int32 i = 0; i < NumTypes; ++i)
	{
		ConnectorTypes[i].BitIndex = i;
	}

	// Warn if we have more than 64 types (excess will be ignored)
	if (ConnectorTypes.Num() > 64)
	{
		UE_LOG(LogTemp, Warning, TEXT("UPCGExValencyConnectorSet '%s': More than 64 connector types defined. Only the first 64 will be usable."), *GetName());
	}

#if WITH_EDITOR
	// Build compatibility matrix from CompatibleTypeIds (editor data)
	BuildCompatibilityMatrixFromTypeIds();
#else
	// Ensure compatibility matrix is sized correctly (runtime - matrix should already be set)
	if (CompatibilityMatrix.Num() != NumTypes)
	{
		const int32 OldNum = CompatibilityMatrix.Num();
		CompatibilityMatrix.SetNum(NumTypes);
		for (int32 i = OldNum; i < NumTypes; ++i)
		{
			CompatibilityMatrix[i] = 0;
		}
	}
#endif
}

bool UPCGExValencyConnectorSet::Validate(TArray<FText>& OutErrors) const
{
	bool bValid = true;

	// Check for duplicate connector type names
	TSet<FName> SeenTypes;
	for (int32 i = 0; i < ConnectorTypes.Num(); ++i)
	{
		const FName& TypeName = ConnectorTypes[i].ConnectorType;
		if (TypeName.IsNone())
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("EmptyConnectorType", "Connector type at index {0} has no name"),
				FText::AsNumber(i)));
			bValid = false;
		}
		else if (SeenTypes.Contains(TypeName))
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("DuplicateConnectorType", "Duplicate connector type '{0}' at index {1}"),
				FText::FromName(TypeName),
				FText::AsNumber(i)));
			bValid = false;
		}
		else
		{
			SeenTypes.Add(TypeName);
		}
	}

	// Check for excessive connector types
	if (ConnectorTypes.Num() > 64)
	{
		OutErrors.Add(FText::Format(
			LOCTEXT("TooManyConnectorTypes", "Too many connector types ({0}). Maximum is 64."),
			FText::AsNumber(ConnectorTypes.Num())));
		bValid = false;
	}

	// Check compatibility matrix size
	if (CompatibilityMatrix.Num() != ConnectorTypes.Num())
	{
		OutErrors.Add(FText::Format(
			LOCTEXT("CompatibilityMatrixSizeMismatch", "Compatibility matrix size ({0}) does not match connector type count ({1})"),
			FText::AsNumber(CompatibilityMatrix.Num()),
			FText::AsNumber(ConnectorTypes.Num())));
		bValid = false;
	}

	return bValid;
}

void UPCGExValencyConnectorSet::SetCompatibility(int32 TypeIndexA, int32 TypeIndexB, bool bBidirectional)
{
	if (!ConnectorTypes.IsValidIndex(TypeIndexA) || !ConnectorTypes.IsValidIndex(TypeIndexB))
	{
		return;
	}

	// Ensure matrix is sized correctly
	if (CompatibilityMatrix.Num() != ConnectorTypes.Num())
	{
		Compile();
	}

	// Set A -> B compatibility
	CompatibilityMatrix[TypeIndexA] |= (1LL << TypeIndexB);

	// Set B -> A compatibility if bidirectional
	if (bBidirectional)
	{
		CompatibilityMatrix[TypeIndexB] |= (1LL << TypeIndexA);
	}
}

void UPCGExValencyConnectorSet::ClearCompatibility()
{
	for (int64& Mask : CompatibilityMatrix)
	{
		Mask = 0;
	}
}

void UPCGExValencyConnectorSet::InitializeSelfCompatible()
{
	Compile(); // Ensure proper sizing

	for (int32 i = 0; i < CompatibilityMatrix.Num(); ++i)
	{
		CompatibilityMatrix[i] = (1LL << i);
	}
}

#if WITH_EDITOR
int32 UPCGExValencyConnectorSet::FindConnectorTypeIndexById(int32 TypeId) const
{
	for (int32 i = 0; i < ConnectorTypes.Num(); ++i)
	{
		if (ConnectorTypes[i].TypeId == TypeId)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

FName UPCGExValencyConnectorSet::GetConnectorTypeNameById(int32 TypeId) const
{
	const int32 Index = FindConnectorTypeIndexById(TypeId);
	return ConnectorTypes.IsValidIndex(Index) ? ConnectorTypes[Index].ConnectorType : NAME_None;
}

void UPCGExValencyConnectorSet::BuildCompatibilityMatrixFromTypeIds()
{
	const int32 NumTypes = FMath::Min(ConnectorTypes.Num(), 64);

	// Reset and resize the matrix
	CompatibilityMatrix.SetNum(NumTypes);
	for (int32 i = 0; i < NumTypes; ++i)
	{
		CompatibilityMatrix[i] = 0;
	}

	// Build bitmask from each type's CompatibleTypeIds
	for (int32 TypeIndexA = 0; TypeIndexA < NumTypes; ++TypeIndexA)
	{
		const FPCGExValencyConnectorEntry& TypeA = ConnectorTypes[TypeIndexA];

		for (const int32 CompatibleTypeId : TypeA.CompatibleTypeIds)
		{
			const int32 TypeIndexB = FindConnectorTypeIndexById(CompatibleTypeId);
			if (TypeIndexB != INDEX_NONE && TypeIndexB < 64)
			{
				// Set bit: TypeA is compatible with TypeB
				CompatibilityMatrix[TypeIndexA] |= (1LL << TypeIndexB);
			}
		}
	}
}

void UPCGExValencyConnectorSet::InitializeSelfCompatibleTypeIds()
{
	for (FPCGExValencyConnectorEntry& TypeDef : ConnectorTypes)
	{
		TypeDef.CompatibleTypeIds.Reset();
		TypeDef.CompatibleTypeIds.Add(TypeDef.TypeId);
	}

	Compile();
}

void UPCGExValencyConnectorSet::InitializeAllCompatibleTypeIds()
{
	// Collect all TypeIds first
	TArray<int32> AllTypeIds;
	AllTypeIds.Reserve(ConnectorTypes.Num());
	for (const FPCGExValencyConnectorEntry& TypeDef : ConnectorTypes)
	{
		AllTypeIds.Add(TypeDef.TypeId);
	}

	// Set all types to be compatible with all other types
	for (FPCGExValencyConnectorEntry& TypeDef : ConnectorTypes)
	{
		TypeDef.CompatibleTypeIds = AllTypeIds;
	}

	Compile();
}
#endif

FName UPCGExValencyConnectorSet::FindMatchingConnectorType(const FName& MeshSocketName, const FString& MeshSocketTag) const
{
	if (ConnectorTypes.Num() == 0)
	{
		return NAME_None;
	}

	// Priority 1: Tag exactly matches a connector type name
	if (!MeshSocketTag.IsEmpty())
	{
		const FName TagAsName = FName(*MeshSocketTag);
		for (const FPCGExValencyConnectorEntry& TypeDef : ConnectorTypes)
		{
			if (TypeDef.ConnectorType == TagAsName)
			{
				return TypeDef.ConnectorType;
			}
		}
	}

	// Priority 2: Socket name exactly matches a connector type name
	for (const FPCGExValencyConnectorEntry& TypeDef : ConnectorTypes)
	{
		if (TypeDef.ConnectorType == MeshSocketName)
		{
			return TypeDef.ConnectorType;
		}
	}

	// Priority 3: Socket name starts with a connector type name (prefix match)
	const FString SocketNameStr = MeshSocketName.ToString();

	FName BestMatch = NAME_None;
	int32 BestMatchLength = 0;

	for (const FPCGExValencyConnectorEntry& TypeDef : ConnectorTypes)
	{
		const FString TypeNameStr = TypeDef.ConnectorType.ToString();
		const int32 TypeLen = TypeNameStr.Len();

		if (TypeLen > BestMatchLength && SocketNameStr.StartsWith(TypeNameStr, ESearchCase::IgnoreCase))
		{
			if (SocketNameStr.Len() == TypeLen ||
				SocketNameStr[TypeLen] == TEXT('_') ||
				FChar::IsDigit(SocketNameStr[TypeLen]))
			{
				BestMatch = TypeDef.ConnectorType;
				BestMatchLength = TypeLen;
			}
		}
	}

	return BestMatch;
}

#if WITH_EDITOR
void UPCGExValencyConnectorSet::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetMemberPropertyName();

	// Auto-compile when connector types or compatibility changes
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UPCGExValencyConnectorSet, ConnectorTypes))
	{
		Compile();
	}
}
#endif

#pragma endregion

#pragma region FPCGExValencyModuleConnector

FTransform FPCGExValencyModuleConnector::GetEffectiveOffset(const UPCGExValencyConnectorSet* ConnectorSet) const
{
	if (bOverrideOffset)
	{
		return LocalOffset;
	}

	if (ConnectorSet)
	{
		const int32 TypeIndex = ConnectorSet->FindConnectorTypeIndex(ConnectorType);
		if (ConnectorSet->ConnectorTypes.IsValidIndex(TypeIndex))
		{
			return ConnectorSet->ConnectorTypes[TypeIndex].DefaultOffset;
		}
	}

	return FTransform::Identity;
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
