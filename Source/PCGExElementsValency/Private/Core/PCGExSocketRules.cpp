// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExSocketRules.h"

#define LOCTEXT_NAMESPACE "PCGExSocketRules"

#pragma region UPCGExSocketRules

int32 UPCGExSocketRules::FindSocketTypeIndex(FName SocketType) const
{
	for (int32 i = 0; i < SocketTypes.Num(); ++i)
	{
		if (SocketTypes[i].SocketType == SocketType)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

bool UPCGExSocketRules::AreTypesCompatible(int32 TypeIndexA, int32 TypeIndexB) const
{
	if (!CompatibilityMatrix.IsValidIndex(TypeIndexA) || TypeIndexB < 0 || TypeIndexB >= 64)
	{
		return false;
	}

	const int64 Mask = CompatibilityMatrix[TypeIndexA];
	return (Mask & (1LL << TypeIndexB)) != 0;
}

int64 UPCGExSocketRules::GetCompatibilityMask(int32 TypeIndex) const
{
	return CompatibilityMatrix.IsValidIndex(TypeIndex) ? CompatibilityMatrix[TypeIndex] : 0;
}

void UPCGExSocketRules::Compile()
{
	// Assign bit indices to socket types
	const int32 NumTypes = FMath::Min(SocketTypes.Num(), 64);
	for (int32 i = 0; i < NumTypes; ++i)
	{
		SocketTypes[i].BitIndex = i;
	}

	// Warn if we have more than 64 types (excess will be ignored)
	if (SocketTypes.Num() > 64)
	{
		UE_LOG(LogTemp, Warning, TEXT("UPCGExSocketRules '%s': More than 64 socket types defined. Only the first 64 will be usable."), *GetName());
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

bool UPCGExSocketRules::Validate(TArray<FText>& OutErrors) const
{
	bool bValid = true;

	// Check for duplicate socket type names
	TSet<FName> SeenTypes;
	for (int32 i = 0; i < SocketTypes.Num(); ++i)
	{
		const FName& TypeName = SocketTypes[i].SocketType;
		if (TypeName.IsNone())
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("EmptySocketType", "Socket type at index {0} has no name"),
				FText::AsNumber(i)));
			bValid = false;
		}
		else if (SeenTypes.Contains(TypeName))
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("DuplicateSocketType", "Duplicate socket type '{0}' at index {1}"),
				FText::FromName(TypeName),
				FText::AsNumber(i)));
			bValid = false;
		}
		else
		{
			SeenTypes.Add(TypeName);
		}
	}

	// Check for excessive socket types
	if (SocketTypes.Num() > 64)
	{
		OutErrors.Add(FText::Format(
			LOCTEXT("TooManySocketTypes", "Too many socket types ({0}). Maximum is 64."),
			FText::AsNumber(SocketTypes.Num())));
		bValid = false;
	}

	// Check compatibility matrix size
	if (CompatibilityMatrix.Num() != SocketTypes.Num())
	{
		OutErrors.Add(FText::Format(
			LOCTEXT("CompatibilityMatrixSizeMismatch", "Compatibility matrix size ({0}) does not match socket type count ({1})"),
			FText::AsNumber(CompatibilityMatrix.Num()),
			FText::AsNumber(SocketTypes.Num())));
		bValid = false;
	}

	return bValid;
}

void UPCGExSocketRules::SetCompatibility(int32 TypeIndexA, int32 TypeIndexB, bool bBidirectional)
{
	if (!SocketTypes.IsValidIndex(TypeIndexA) || !SocketTypes.IsValidIndex(TypeIndexB))
	{
		return;
	}

	// Ensure matrix is sized correctly
	if (CompatibilityMatrix.Num() != SocketTypes.Num())
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

void UPCGExSocketRules::ClearCompatibility()
{
	for (int64& Mask : CompatibilityMatrix)
	{
		Mask = 0;
	}
}

void UPCGExSocketRules::InitializeSelfCompatible()
{
	Compile(); // Ensure proper sizing

	for (int32 i = 0; i < CompatibilityMatrix.Num(); ++i)
	{
		CompatibilityMatrix[i] = (1LL << i);
	}
}

#if WITH_EDITOR
int32 UPCGExSocketRules::FindSocketTypeIndexById(int32 TypeId) const
{
	for (int32 i = 0; i < SocketTypes.Num(); ++i)
	{
		if (SocketTypes[i].TypeId == TypeId)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

FName UPCGExSocketRules::GetSocketTypeNameById(int32 TypeId) const
{
	const int32 Index = FindSocketTypeIndexById(TypeId);
	return SocketTypes.IsValidIndex(Index) ? SocketTypes[Index].SocketType : NAME_None;
}

FText UPCGExSocketRules::GetSocketTypeDisplayNameById(int32 TypeId) const
{
	const int32 Index = FindSocketTypeIndexById(TypeId);
	return SocketTypes.IsValidIndex(Index) ? SocketTypes[Index].GetDisplayName() : FText::GetEmpty();
}

void UPCGExSocketRules::BuildCompatibilityMatrixFromTypeIds()
{
	const int32 NumTypes = FMath::Min(SocketTypes.Num(), 64);

	// Reset and resize the matrix
	CompatibilityMatrix.SetNum(NumTypes);
	for (int32 i = 0; i < NumTypes; ++i)
	{
		CompatibilityMatrix[i] = 0;
	}

	// Build bitmask from each type's CompatibleTypeIds
	for (int32 TypeIndexA = 0; TypeIndexA < NumTypes; ++TypeIndexA)
	{
		const FPCGExSocketDefinition& TypeA = SocketTypes[TypeIndexA];

		for (const int32 CompatibleTypeId : TypeA.CompatibleTypeIds)
		{
			const int32 TypeIndexB = FindSocketTypeIndexById(CompatibleTypeId);
			if (TypeIndexB != INDEX_NONE && TypeIndexB < 64)
			{
				// Set bit: TypeA is compatible with TypeB
				CompatibilityMatrix[TypeIndexA] |= (1LL << TypeIndexB);
			}
		}
	}
}

void UPCGExSocketRules::InitializeSelfCompatibleTypeIds()
{
	for (FPCGExSocketDefinition& TypeDef : SocketTypes)
	{
		TypeDef.CompatibleTypeIds.Reset();
		TypeDef.CompatibleTypeIds.Add(TypeDef.TypeId);
	}

	Compile();
}

void UPCGExSocketRules::InitializeAllCompatibleTypeIds()
{
	// Collect all TypeIds first
	TArray<int32> AllTypeIds;
	AllTypeIds.Reserve(SocketTypes.Num());
	for (const FPCGExSocketDefinition& TypeDef : SocketTypes)
	{
		AllTypeIds.Add(TypeDef.TypeId);
	}

	// Set all types to be compatible with all other types
	for (FPCGExSocketDefinition& TypeDef : SocketTypes)
	{
		TypeDef.CompatibleTypeIds = AllTypeIds;
	}

	Compile();
}
#endif

FName UPCGExSocketRules::FindMatchingSocketType(const FName& MeshSocketName, const FString& MeshSocketTag) const
{
	if (SocketTypes.Num() == 0)
	{
		return NAME_None;
	}

	// Priority 1: Tag exactly matches a socket type name
	if (!MeshSocketTag.IsEmpty())
	{
		const FName TagAsName = FName(*MeshSocketTag);
		for (const FPCGExSocketDefinition& TypeDef : SocketTypes)
		{
			if (TypeDef.SocketType == TagAsName)
			{
				return TypeDef.SocketType;
			}
		}
	}

	// Priority 2: Socket name exactly matches a socket type name
	for (const FPCGExSocketDefinition& TypeDef : SocketTypes)
	{
		if (TypeDef.SocketType == MeshSocketName)
		{
			return TypeDef.SocketType;
		}
	}

	// Priority 3: Socket name starts with a socket type name (prefix match)
	// Sort by length descending to match longest prefix first (e.g., "DoorLarge" before "Door")
	const FString SocketNameStr = MeshSocketName.ToString();

	FName BestMatch = NAME_None;
	int32 BestMatchLength = 0;

	for (const FPCGExSocketDefinition& TypeDef : SocketTypes)
	{
		const FString TypeNameStr = TypeDef.SocketType.ToString();
		const int32 TypeLen = TypeNameStr.Len();

		// Check if socket name starts with this type name
		if (TypeLen > BestMatchLength && SocketNameStr.StartsWith(TypeNameStr, ESearchCase::IgnoreCase))
		{
			// Verify it's a proper prefix (followed by underscore, number, or end)
			// This prevents "Door" from matching "Doorway" but allows "Door_Left" or "Door1"
			if (SocketNameStr.Len() == TypeLen ||
				SocketNameStr[TypeLen] == TEXT('_') ||
				FChar::IsDigit(SocketNameStr[TypeLen]))
			{
				BestMatch = TypeDef.SocketType;
				BestMatchLength = TypeLen;
			}
		}
	}

	return BestMatch;
}

#if WITH_EDITOR
void UPCGExSocketRules::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetMemberPropertyName();

	// Auto-compile when socket types or compatibility changes
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UPCGExSocketRules, SocketTypes))
	{
		Compile();
	}
}
#endif

#pragma endregion

#pragma region FPCGExModuleSocket

FTransform FPCGExModuleSocket::GetEffectiveOffset(const UPCGExSocketRules* SocketRules) const
{
	if (bOverrideOffset)
	{
		return LocalOffset;
	}

	if (SocketRules)
	{
		const int32 TypeIndex = SocketRules->FindSocketTypeIndex(SocketType);
		if (SocketRules->SocketTypes.IsValidIndex(TypeIndex))
		{
			return SocketRules->SocketTypes[TypeIndex].DefaultOffset;
		}
	}

	return FTransform::Identity;
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
