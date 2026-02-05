// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExSocket.generated.h"

USTRUCT(BlueprintType)
struct PCGEXFOUNDATIONS_API FPCGExSocket
{
	GENERATED_BODY()

	FPCGExSocket();
	FPCGExSocket(const FName& InSocketName, const FVector& InRelativeLocation, const FRotator& InRelativeRotation, const FVector& InRelativeScale, FString InTag);
	FPCGExSocket(const FName& InSocketName, const FTransform& InRelativeTransform, const FString& InTag);
	~FPCGExSocket();
	FPCGExSocket(const FPCGExSocket&);
	FPCGExSocket& operator=(const FPCGExSocket&);

	/** Whether this socket is managed by the system. */
	UPROPERTY(meta=(PCG_NotOverridable, EditCondition="false", EditConditionHides))
	bool bManaged = false;

	/** Unique name identifying this socket. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bManaged", HideEditConditionToggle))
	FName SocketName = NAME_None;

	/** Transform relative to the owning point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bManaged", HideEditConditionToggle))
	FTransform RelativeTransform = FTransform::Identity;

	/** Tag associated with this socket for filtering. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bManaged", HideEditConditionToggle))
	FString Tag = TEXT("");
};
