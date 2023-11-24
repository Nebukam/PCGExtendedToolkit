// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Graph/PCGExGraphProcessor.h"

#include "PCGExOperations.generated.h"

UENUM(BlueprintType)
enum class EPCGExOperationOutput : uint8
{
	OperandA UMETA(DisplayName = "Operand A", ToolTip="Overwrite Operand A"),
	OperandB UMETA(DisplayName = "Operand B", ToolTip="Overwrite Operand B"),
	OutputToCustom UMETA(DisplayName = "Other", ToolTip="Create or overwrite another attribute"),
};

UENUM(BlueprintType)
enum class EPCGExApplyOperation : uint8
{
	Copy UMETA(DisplayName = "A=B", ToolTip="Copy A to B"),
	Lerp UMETA(DisplayName = "Lerp(A,B)", ToolTip="Lerp A toward B"),
	Add UMETA(DisplayName = "A+=B", ToolTip="Add B to A"),
	Sub UMETA(DisplayName = "A-=B", ToolTip="Substract B from A"),
	Mult UMETA(DisplayName = "A*=B", ToolTip="Multiply A by B"),
	Div UMETA(DisplayName = "A/=B", ToolTip="Divide A by B"),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExOperationDescriptor
{
	GENERATED_BODY()

	FPCGExOperationDescriptor()
	{
	}

public:
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bEnabled = true;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExApplyOperation Operation = EPCGExApplyOperation::Copy;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="A", ShowOnlyInnerProperties, FullyExpand=true))
	FPCGExInputDescriptorGeneric OperandA;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="B", ShowOnlyInnerProperties, FullyExpand=true))
	FPCGExInputDescriptorGeneric OperandB;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Alpha", EditCondition="Operation==EPCGExApplyOperation::Lerp", ShowOnlyInnerProperties, FullyExpand=true))
	FPCGExInputDescriptorWithSingleField Alpha;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExOperationOutput OutputTo = EPCGExOperationOutput::OutputToCustom;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Output", EditCondition="OutputTo==EPCGExOperationOutput::OutputToCustom", ShowOnlyInnerProperties, FullyExpand=true))
	FPCGExInputDescriptorGeneric CustomOutput;
	
		
};

namespace PCGEx
{
	struct PCGEXTENDEDTOOLKIT_API FOperation
	{

		FOperation()
		{
			Descriptor = nullptr;
		}

	public:
		FPCGExOperationDescriptor* Descriptor;

		PCGEx::FLocalDirectionInput VectorInput;

		bool bValid = false;

		bool Validate(const UPCGPointData* InData)
		{
			bValid = false;
			return bValid;
		}
	};
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExOperationsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExOperationsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Operations, "Operations", "Perform an ordered list of in-place operations on local attributes.");
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Attributes to draw.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FPCGExOperationDescriptor> Applications;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	virtual PCGExIO::EInitMode GetPointOutputInitMode() const override;

private:
	friend class FPCGExOperationsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExOperationsContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExWriteIndexElement;

public:
	TArray<PCGEx::FOperation> Operations;
	void PrepareForPoints(const UPCGPointData* PointData);
};


class PCGEXTENDEDTOOLKIT_API FPCGExOperationsElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;
	virtual bool Validate(FPCGContext* InContext) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
