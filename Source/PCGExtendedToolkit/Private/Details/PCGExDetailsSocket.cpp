// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExDetailsSocket.h"

#include "Core/PCGExContext.h"
#include "Math/PCGExMathBounds.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointElements.h"
#include "Details/PCGExSettingsDetails.h"
#include "Engine/EngineTypes.h"
#include "Math/PCGExMathAxis.h"
#include "Sampling/PCGExSampling.h"

FPCGExSocket::FPCGExSocket(const FName& InSocketName, const FVector& InRelativeLocation, const FRotator& InRelativeRotation, const FVector& InRelativeScale, FString InTag)
	: SocketName(InSocketName), RelativeTransform(FTransform(InRelativeRotation.Quaternion(), InRelativeLocation, InRelativeScale)), Tag(InTag)
{
}

FPCGExSocket::FPCGExSocket(const FName& InSocketName, const FTransform& InRelativeTransform, const FString& InTag)
	: SocketName(InSocketName), RelativeTransform(InRelativeTransform), Tag(InTag)
{
}

PCGEX_SETTING_VALUE_IMPL(FPCGExSocketFitDetails, SocketName, FName, SocketNameInput, SocketNameAttribute, SocketName)

bool FPCGExSocketFitDetails::Init(const TSharedPtr<PCGExData::FFacade>& InFacade)
{
	if (!bEnabled || (SocketNameInput == EPCGExInputValueType::Constant && SocketName.IsNone()) || (SocketNameInput == EPCGExInputValueType::Attribute && SocketNameAttribute.IsNone()))
	{
		bMutate = false;
		return true;
	}

	bMutate = true;
	SocketNameBuffer = GetValueSettingSocketName();
	if (!SocketNameBuffer->Init(InFacade)) { return false; }

	return true;
}

void FPCGExSocketFitDetails::Mutate(const int32 Index, const TArray<FPCGExSocket>& InSockets, FTransform& InOutTransform) const
{
	if (!bMutate) { return; }

	const FName SName = SocketNameBuffer->Read(Index);
	for (const FPCGExSocket& Socket : InSockets)
	{
		if (Socket.SocketName == SName)
		{
			InOutTransform = InOutTransform * Socket.RelativeTransform;
			return;
		}
	}
}
