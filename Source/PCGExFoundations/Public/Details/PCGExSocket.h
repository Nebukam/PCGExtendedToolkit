// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExSocket.generated.h"

USTRUCT(BlueprintType)
struct PCGEXFOUNDATIONS_API FPCGExSocket
{
	GENERATED_BODY()

	FPCGExSocket() = default;
	FPCGExSocket(const FName& InSocketName, const FVector& InRelativeLocation, const FRotator& InRelativeRotation, const FVector& InRelativeScale, FString InTag);
	FPCGExSocket(const FName& InSocketName, const FTransform& InRelativeTransform, const FString& InTag);
	~FPCGExSocket() = default;

	UPROPERTY(meta=(PCG_NotOverridable, EditCondition="false", EditConditionHides))
	bool bManaged = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bManaged"))
	FName SocketName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bManaged"))
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bManaged"))
	FString Tag = TEXT("");
};
