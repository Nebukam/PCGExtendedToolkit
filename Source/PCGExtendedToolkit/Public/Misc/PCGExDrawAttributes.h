// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPointData.h"
#include "Graph/PCGExGraphProcessor.h"
#include "Graph/PCGExGraphHelpers.h"
#include "PCGExDrawAttributes.generated.h"

UENUM(BlueprintType)
enum class EPCGExDebugType : uint8
{
	Direction UMETA(DisplayName = "Direction", ToolTip="Attribute is treated as a Normal."),
	Connection UMETA(DisplayName = "Connection", ToolTip="Attribute is treated as an lookup index in the same data block."),
	Point UMETA(DisplayName = "Point", ToolTip="Attribute is treated as a world space position."),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeDebugDrawDescriptor
{
	GENERATED_BODY()

	FPCGExAttributeDebugDrawDescriptor()
	{
	}

public:
	/** Draw line thickness. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bEnabled = true;

	/** Draw line thickness. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition="bEnabled"))
	EPCGExDebugType Type = EPCGExDebugType::Direction;

	/** Attribute to sample direction from */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition="Type==EPCGExDebugType::Direction", EditConditionHides, ShowOnlyInnerProperties, FullyExpand=true))
	FPCGExInputDescriptorWithDirection Direction;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition="Type==EPCGExDebugType::Direction", EditConditionHides))
	bool bNormalizeBeforeSizing = true;
	
	/** Attribute to sample position from */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition="Type==EPCGExDebugType::Point", EditConditionHides, ShowOnlyInnerProperties, FullyExpand=true))
	FPCGExInputDescriptorWithDirection Point;

	/** Attribute to sample index from */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition="Type==EPCGExDebugType::Connection", EditConditionHides, ShowOnlyInnerProperties, FullyExpand=true))
	FPCGExInputDescriptorWithSingleField Index;

	/** Draw line thickness. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	float Thickness = 1.0;

	/** Draw size. What it means depends on the selected debug type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size")
	double Size = 100.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size", meta = (InlineEditConditionToggle, EditCondition="Type==EPCGExDebugType::Direction"))
	bool bSizeFromAttribute = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size", meta = (EditCondition="bSizeFromAttribute"))
	FPCGExInputDescriptorWithSingleField SizeAttribute;

	/** Draw color. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color")
	FColor Color = FColor(255, 0, 0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color", meta = (InlineEditConditionToggle))
	bool bColorFromAttribute = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color", meta = (EditCondition="bColorFromAttribute"))
	FPCGExInputDescriptor ColorAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color", meta = (EditCondition="bColorFromAttribute"))
	bool bColorIsLinear = true;
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeDebugDraw
{
	GENERATED_BODY()

	FPCGExAttributeDebugDraw()
	{
		Descriptor = nullptr;
	}

public:
	FPCGExAttributeDebugDrawDescriptor* Descriptor;

	PCGEx::FLocalDirectionInput VectorInput;
	PCGEx::FLocalSingleComponentInput IndexInput;
	PCGEx::FLocalSingleComponentInput SizeAttributeInput;
	PCGEx::FLocalVectorInput ColorAttributeInput;

	bool bValid = false;

	bool Validate(const UPCGPointData* InData)
	{
		bValid = false;

		switch (Descriptor->Type)
		{
		case EPCGExDebugType::Direction:
			VectorInput.Capture(Descriptor->Direction);
			bValid = VectorInput.Validate(InData);
			break;
		case EPCGExDebugType::Point:
			VectorInput.Capture(Descriptor->Point);
			bValid = VectorInput.Validate(InData);
			break;
		case EPCGExDebugType::Connection:
			IndexInput.Capture(Descriptor->Index);
			bValid = IndexInput.Validate(InData);
			break;
		default: ;
		}

		if (bValid)
		{
			SizeAttributeInput.Capture(Descriptor->SizeAttribute);
			SizeAttributeInput.Validate(InData);

			ColorAttributeInput.Descriptor = Descriptor->ColorAttribute;
			ColorAttributeInput.Validate(InData);
		}

		return bValid;
	}

	double GetSize(const FPCGPoint& Point) const
	{
		double Value = Descriptor->Size;
		if (Descriptor->bSizeFromAttribute && SizeAttributeInput.bValid) { Value = SizeAttributeInput.GetValue(Point) * Descriptor->Size; }
		return Value;
	}

	FColor GetColor(const FPCGPoint& Point) const
	{
		if (Descriptor->bColorFromAttribute && ColorAttributeInput.bValid)
		{
			const FVector Value = ColorAttributeInput.GetValue(Point);
			return Descriptor->bColorIsLinear ? FColor(Value.X * 255.0f, Value.Y * 255.0f, Value.Z * 255.0f) : FColor(Value.X, Value.Y, Value.Z);
		}
		else
		{
			return Descriptor->Color;
		}
	}

	FVector GetDirection(const FPCGPoint& Point) const
	{
		FVector OutDirection = VectorInput.bValid ? VectorInput.GetValue(Point) : FVector::Zero();
		if (Descriptor->bNormalizeBeforeSizing) { OutDirection.Normalize(); }
		return OutDirection * GetSize(Point);
	}

	FVector GetPosition(const FPCGPoint& Point) const
	{
		return VectorInput.bValid ? VectorInput.GetValue(Point) : Point.Transform.GetLocation();
	}

	FVector GetTargetPosition(const FPCGPoint& Point, const UPCGPointData* PointData) const
	{
		const int64 OutIndex = IndexInput.bValid ? IndexInput.GetValue(Point) : -1;
		if (OutIndex != -1) { return PointData->GetPoint(OutIndex).Transform.GetLocation(); }
		return Point.Transform.GetLocation();
	}

	void Draw(const UWorld* World, const FVector& Start, const FPCGPoint& Point, const UPCGPointData* PointData) const
	{
		switch (Descriptor->Type)
		{
		case EPCGExDebugType::Direction:
			DrawDirection(World, Start, Point, PointData);
			break;
		case EPCGExDebugType::Connection:
			DrawConnection(World, Start, Point, PointData);
			break;
		case EPCGExDebugType::Point:
			DrawPoint(World, Start, Point, PointData);
			break;
		default: ;
		}
	}

protected:
	bool IsDirectionAndSizeFromAttribute() const
	{
		return Descriptor->Type == EPCGExDebugType::Direction && Descriptor->bSizeFromAttribute;
	}

	void DrawDirection(const UWorld* World, const FVector& Start, const FPCGPoint& Point, const UPCGPointData* PointData) const
	{
		FVector Dir = GetDirection(Point);
		DrawDebugDirectionalArrow(World, Start, Start + Dir, Dir.Length() * 0.05f, GetColor(Point), true, -1, 0, Descriptor->Thickness);
	}

	void DrawConnection(const UWorld* World, const FVector& Start, const FPCGPoint& Point, const UPCGPointData* PointData) const
	{
		const FVector End = GetTargetPosition(Point, PointData);
		DrawDebugLine(World, Start, End, GetColor(Point), true, -1, 0, Descriptor->Thickness);
	}

	void DrawPoint(const UWorld* World, const FVector& Start, const FPCGPoint& Point, const UPCGPointData* PointData) const
	{
		const FVector End = GetPosition(Point);
		DrawDebugPoint(World, End, GetSize(Point), GetColor(Point), true, -1, 0);
	}
};


/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExDrawAttributesSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExDrawAttributesSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DrawAttributes, "Draw Attributes", "Draw debug attributes. Toggle debug OFF (D) before disabling this node (E)! Warning: this node will clear persistent debug lines before it!");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Debug; }
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(1.0f, 0.0f, 0.0f, 1.0f); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Attributes to draw.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FPCGExAttributeDebugDrawDescriptor> DebugList;

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
