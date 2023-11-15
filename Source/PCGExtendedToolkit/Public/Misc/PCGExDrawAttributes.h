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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition="Type==EPCGExDebugType::Direction", EditConditionHides, ShowOnlyInnerProperties))
	FPCGExInputSelectorWithDirection Direction;
	PCGEx::FLocalDirectionInput DirectionInput;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition="Type==EPCGExDebugType::Direction", EditConditionHides))
	bool bNormalizeBeforeSizing = true;

	/** Attribute to sample index from */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition="Type==EPCGExDebugType::Connection", EditConditionHides, ShowOnlyInnerProperties))
	FPCGExInputSelectorWithSingleField Index;
	PCGEx::FLocalSingleComponentInput IndexInput;

	/** Draw line thickness. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	float Thickness = 1.0;

	/** Draw size. What it means depends on the selected debug type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size")
	double Size = 100.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size", meta = (InlineEditConditionToggle, EditCondition="Type==EPCGExDebugType::Direction"))
	bool bSizeFromAttribute = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size", meta = (EditCondition="bSizeFromAttribute"))
	FPCGExInputSelectorWithSingleField SizeAttribute;
	PCGEx::FLocalSingleComponentInput SizeAttributeInput;

	/** Draw color. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color")
	FColor Color = FColor(255, 0, 0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color", meta = (InlineEditConditionToggle))
	bool bColorFromAttribute = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color", meta = (EditCondition="bColorFromAttribute"))
	FPCGExInputSelector ColorAttribute;
	PCGEx::FLocalVectorInput ColorAttributeInput;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color", meta = (EditCondition="bColorFromAttribute"))
	bool bColorIsLinear = true;

	bool bValid = false;

	bool Validate(const UPCGPointData* InData)
	{
		bValid = false;

		switch (Type)
		{
		case EPCGExDebugType::Direction:
			DirectionInput.Descriptor = Direction;
			DirectionInput.Direction = Direction.Direction;
			bValid = DirectionInput.Validate(InData);
			break;
		case EPCGExDebugType::Connection:
			IndexInput.Descriptor = Index;
			IndexInput.FieldSelection = Index.FieldSelection;
			bValid = IndexInput.Validate(InData);
			break;
		default: ;
		}

		if (bValid)
		{
			SizeAttributeInput.Descriptor = SizeAttribute;
			SizeAttributeInput.FieldSelection = SizeAttribute.FieldSelection;
			SizeAttributeInput.Validate(InData);

			ColorAttributeInput.Descriptor = ColorAttribute;
			ColorAttributeInput.Validate(InData);
		}

		return bValid;
	}

	double GetSize(const FPCGPoint& Point) const
	{
		double Value = Size;
		if (bSizeFromAttribute && SizeAttributeInput.bValid) { Value = SizeAttributeInput.GetValue(Point) * Size; }
		return Value;
	}

	FColor GetColor(const FPCGPoint& Point) const
	{
		if (bColorFromAttribute && ColorAttributeInput.bValid)
		{
			const FVector Value = ColorAttributeInput.GetValue(Point);
			return bColorIsLinear ? FColor(Value.X * 255.0f, Value.Y * 255.0f, Value.Z * 255.0f) : FColor(Value.X, Value.Y, Value.Z);
		}
		else
		{
			return Color;
		}
	}

	FVector GetDirection(const FPCGPoint& Point) const
	{
		FVector OutDirection = DirectionInput.bValid ? DirectionInput.GetValue(Point) : FVector::Zero();
		if (bNormalizeBeforeSizing) { OutDirection.Normalize(); }
		return OutDirection * GetSize(Point);
	}

	FVector GetTargetPosition(const FPCGPoint& Point, const UPCGPointData* PointData) const
	{
		const int64 OutIndex = IndexInput.bValid ? IndexInput.GetValue(Point) : -1;
		if (OutIndex != -1) { return PointData->GetPoint(OutIndex).Transform.GetLocation(); }
		return Point.Transform.GetLocation();
	}

	void Draw(const UWorld* World, const FVector& Start, const FPCGPoint& Point, const UPCGPointData* PointData) const
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
	bool IsDirectionAndSizeFromAttribute() const
	{
		return Type == EPCGExDebugType::Direction && bSizeFromAttribute;
	}
	
	void DrawDirection(const UWorld* World, const FVector& Start, const FPCGPoint& Point, const UPCGPointData* PointData) const
	{
		FVector Dir = GetDirection(Point);
		DrawDebugDirectionalArrow(World, Start, Start + Dir, Dir.Length() * 0.05f, GetColor(Point), true, -1, 0, Thickness);
	}

	void DrawConnection(const UWorld* World, const FVector& Start, const FPCGPoint& Point, const UPCGPointData* PointData) const
	{
		const FVector End = GetTargetPosition(Point, PointData);
		DrawDebugLine(World, Start, End, GetColor(Point), true, -1, 0, Thickness);
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
	UPCGExDrawAttributesSettings(const FObjectInitializer& ObjectInitializer);

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
	void PrepareForPoints(const UPCGPointData* PointData);
};


class PCGEXTENDEDTOOLKIT_API FPCGExDrawAttributesElement : public FPCGExPointsProcessorElementBase
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
