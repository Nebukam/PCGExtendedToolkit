// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "PCGExWriteIndex.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExWriteIndexSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(WriteIndex, "Write Index", "Write the current point index to an attribute.");
#endif

	virtual PCGExPointIO::EInit GetPointOutputInitMode() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** The name of the attribute to write its index to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bOutputNormalizedIndex = false;
	
	/** The name of the attribute to write its index to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName OutputAttributeName = "CurrentIndex";
};

struct PCGEXTENDEDTOOLKIT_API FPCGExWriteIndexContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExWriteIndexElement;

public:
	mutable FRWLock MapLock;
	bool bOutputNormalizedIndex;
	FName OutName = NAME_None;
	TMap<UPCGExPointIO*, FPCGMetadataAttribute<int64>*> AttributeMap;
	TMap<UPCGExPointIO*, FPCGMetadataAttribute<double>*> NormalizedAttributeMap;
};

class PCGEXTENDEDTOOLKIT_API FPCGExWriteIndexElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;
	virtual bool Validate(FPCGContext* InContext) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
