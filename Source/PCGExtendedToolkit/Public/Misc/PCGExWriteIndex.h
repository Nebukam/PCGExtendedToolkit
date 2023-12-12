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

	virtual PCGExData::EInit GetPointOutputInitMode() const override;

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

	virtual ~FPCGExWriteIndexContext() override;

	mutable FRWLock MapLock;
	bool bOutputNormalizedIndex;
	FName OutputAttributeName = NAME_None;

	TArray<int32> IndicesBuffer;
	PCGEx::FAttributeAccessor<int32>* IndexAccessor = nullptr;

	TArray<double> NormalizedIndicesBuffer;
	PCGEx::FAttributeAccessor<double>* NormalizedIndexAccessor = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExWriteIndexElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Validate(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
