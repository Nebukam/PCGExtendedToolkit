// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPointData.h"
#include "Relational/PCGExRelationsProcessor.h"
#include "Relational/PCGExRelationsHelpers.h"
#include "PCGExDrawAttributes.generated.h"

UENUM(BlueprintType)
enum class EPCGExDebugType : uint8
{
	Direction UMETA(DisplayName = "Direction", ToolTip="Attribute is treated as a Normal."),
	Connection UMETA(DisplayName = "Connection", ToolTip="Attribute is treated as an lookup index in the same data block.")
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeDebugDraw
{
	GENERATED_BODY()

	FPCGExAttributeDebugDraw()
	{
	}

public:
	/** Draw line thickness. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExDebugType Type = EPCGExDebugType::Direction;

	/** Attribute to sample direction from */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition="Type==EPCGExDebugType::Direction", EditConditionHides))
	FPCGExInputSelectorWithDirection InputDirection;
	PCGEx::FLocalVectorInput LocalInputDirection;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition="Type==EPCGExDebugType::Direction", EditConditionHides))
	bool bNormalizeBeforeSizing = true;

	/** Attribute to sample index from */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition="Type==EPCGExDebugType::Connection", EditConditionHides, ShowOnlyInnerProperties))
	FPCGExInputSelectorWithSingleField InputIndex;
	PCGEx::FLocalInteger64Input LocalInputIndex;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition="Type==EPCGExDebugType::Connection", EditConditionHides))
	bool bValueIsMetadataEntry = true;

	/** Draw line thickness. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	float Thickness = 1.0;

	/** Draw size. What it means depends on the selected debug type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size")
	double Size = 1.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size", meta = (InlineEditConditionToggle))
	bool bSizeFromAttribute = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size", meta = (EditCondition="bSizeFromAttribute"))
	FPCGExInputSelectorWithSingleField DrawSizeAttributeInput;
	PCGEx::FLocalDoubleInput LocalInputSize;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size")
	double DrawSizeMultiplier = 1.0;

	/** Draw color. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color")
	FColor Color = FColor(255, 0, 0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color", meta = (InlineEditConditionToggle))
	bool bColorFromAttribute = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color", meta = (EditCondition="bColorFromAttribute"))
	FPCGExInputSelector ColorAttributeInput;
	PCGEx::FLocalVectorInput LocalInputColor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color", meta = (EditCondition="bColorFromAttribute"))
	bool bColorIsLinear = true;

	bool bValid = false;

	bool Validate(const UPCGPointData* InData)
	{
		bValid = false;

		switch (Type)
		{
		case EPCGExDebugType::Direction:
			LocalInputDirection.Descriptor = InputDirection;
			bValid = LocalInputDirection.Validate(InData);
			break;
		case EPCGExDebugType::Connection:
			LocalInputIndex.Descriptor = InputIndex;
			bValid = LocalInputIndex.Validate(InData);
			break;
		default: ;
		}

		if (bValid)
		{
			DrawSizeAttributeInput.Validate(InData);
			ColorAttributeInput.Validate(InData);
		}

		return bValid;
	}

	double GetSize(const FPCGPoint& Point)
	{
		double Value = Size;
		if (LocalInputSize.bValid) { Value = LocalInputSize.GetValue(Point); }
		return Value * DrawSizeMultiplier;
	}

	FColor GetColor(const FPCGPoint& Point)
	{
		if (LocalInputColor.bValid)
		{
			const FVector Value = LocalInputColor.GetValue(Point);
			return bColorIsLinear ? FColor(Value.X * 255.0f, Value.Y * 255.0f, Value.Z * 255.0f) : FColor(Value.X, Value.Y, Value.Z);
		}
		else
		{
			return Color;
		}
	}

	FVector GetDirection(const FPCGPoint& Point)
	{
		return FVector::Zero();
	}

	FVector GetTargetPosition(const FPCGPoint& Point, UPCGPointData* PointData)
	{
		return Point.Transform.GetLocation();
	}

	void Draw(UWorld* World, const FVector& Start, const FPCGPoint& Point, UPCGPointData* PointData)
	{
		switch (Type)
		{
		case EPCGExDebugType::Direction:
			DrawDirection(World, Start, Point, PointData);
			break;
		case EPCGExDebugType::Connection:
			DrawConnection(World, Start, Point, PointData);
			break;
		default: ;
		}
	}

protected:
	void DrawDirection(UWorld* World, const FVector& Start, const FPCGPoint& Point, UPCGPointData* PointData)
	{
		//TODO
		FVector End = GetDirection(Point);
	}

	void DrawConnection(UWorld* World, const FVector& Start, const FPCGPoint& Point, UPCGPointData* PointData)
	{
		//TODO
		FVector End = GetTargetPosition(Point, PointData);
	}
};


/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Relational")
class PCGEXTENDEDTOOLKIT_API UPCGExDrawAttributesSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DrawAttributes, "Draw Attributes", "Draw debug relations. Toggle debug OFF (D) before disabling this node (E)! Warning: this node will clear persistent debug lines before it!");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Debug; }
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(1.0f, 0.0f, 0.0f, 1.0f); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Attributes to draw.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FPCGExAttributeDebugDraw> DebugList;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;

private:
	friend class FPCGExDrawAttributesElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExDrawAttributesContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExWriteIndexElement;

public:
	TArray<FPCGExAttributeDebugDraw> DebugList;
	void PrepareForPoints(UPCGPointData* PointData);
};


class PCGEXTENDEDTOOLKIT_API FPCGExDrawAttributesElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
