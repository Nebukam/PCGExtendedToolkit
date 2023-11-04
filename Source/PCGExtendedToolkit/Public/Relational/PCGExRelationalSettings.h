// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGExRelationalData.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExRelationalSettings.generated.h"

namespace PCGExRelational
{
	extern const FName SourceLabel;
	extern const FName SourceRelationalLabel;
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExRelationalSettingsBase : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PCGExRelationalSettings")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExRelationalSettings", "NodeTitle", "Relational Settings"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

private:
	friend class FPCGExRelationalProcessingElementBase;
};

class FPCGExRelationalProcessingElementBase : public FPCGPointProcessingElementBase
{
public:
	const UPCGExRelationalData* GetFirstRelationalData(FPCGContext* Context) const;

protected:
	// Prepare an UPCGPointData to be used with the provided RelationalData
	template <RelationalDataStruct T>
	static FPCGMetadataAttribute<T>* PrepareData(const UPCGExRelationalData* RelationalData, UPCGPointData* PointData);
	
	// Attempts to find the attribute allowing access to per-point relational data
	template <RelationalDataStruct T>
	static FPCGMetadataAttribute<T>* FindRelationalAttribute(const UPCGExRelationalData* RelationalData, const UPCGPointData* PointData);

private:
	friend class UPCGExRelationalData;
	
};
