// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExSocket.h"

FPCGExSocket::FPCGExSocket() = default;
FPCGExSocket::~FPCGExSocket() = default;
FPCGExSocket::FPCGExSocket(const FPCGExSocket&) = default;
FPCGExSocket& FPCGExSocket::operator=(const FPCGExSocket&) = default;

FPCGExSocket::FPCGExSocket(const FName& InSocketName, const FVector& InRelativeLocation, const FRotator& InRelativeRotation, const FVector& InRelativeScale, FString InTag)
	: SocketName(InSocketName), RelativeTransform(FTransform(InRelativeRotation.Quaternion(), InRelativeLocation, InRelativeScale)), Tag(InTag)
{
}

FPCGExSocket::FPCGExSocket(const FName& InSocketName, const FTransform& InRelativeTransform, const FString& InTag)
	: SocketName(InSocketName), RelativeTransform(InRelativeTransform), Tag(InTag)
{
}
