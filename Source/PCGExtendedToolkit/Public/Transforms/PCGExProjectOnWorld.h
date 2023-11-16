// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

/*
 * This is a dummy class to create new simple PCG nodes
 */

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExProjectOnWorld.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExProjectOnWorldSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ProjectOnWorld, "Project on World", "Project points on the world surface.");
#endif

	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** The name of the attribute to write its index to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName OutputAttributeName;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExProjectOnWorldContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExProjectOnWorldElement;

public:
	mutable FRWLock MapLock;
	FName OutName = NAME_None;
	TMap<UPCGExPointIO*, FPCGMetadataAttribute<int64>*> AttributeMap;
};

class PCGEXTENDEDTOOLKIT_API FPCGExProjectOnWorldElement : public FPCGExPointsProcessorElementBase
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
