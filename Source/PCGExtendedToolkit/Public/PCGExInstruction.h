// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExInstruction.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew)
class PCGEXTENDEDTOOLKIT_API UPCGExInstruction : public UObject
{
	GENERATED_BODY()

public:
	virtual void UpdateUserFacingInfos();
	
};
