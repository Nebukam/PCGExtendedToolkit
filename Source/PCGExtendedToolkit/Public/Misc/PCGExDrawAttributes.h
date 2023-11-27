// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "IPCGExDebug.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExDrawAttributes.generated.h"

UENUM(BlueprintType)
enum class EPCGExDebugExpression : uint8
{
	Direction UMETA(DisplayName = "Direction", ToolTip="Attribute is treated as a Normal."),
	ConnectionToIndex UMETA(DisplayName = "Connection (Point Index)", ToolTip="Attribute is treated as a lookup index in the same data block."),
	ConnectionToPosition UMETA(DisplayName = "Connection (Position)", ToolTip="Attribute is treated as world space position in the same data block."),
	Point UMETA(DisplayName = "Point", ToolTip="Attribute is treated as a world space position."),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeDebugDrawDescriptor : public FPCGExInputDescriptor
{
	GENERATED_BODY()

	FPCGExAttributeDebugDrawDescriptor(): FPCGExInputDescriptor()
	{
	}

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
	
	/** Enable or disable this debug group. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayPriority=-1))
	bool bEnabled = true;

	/** Draw line thickness. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExDebugExpression ExpressedAs = EPCGExDebugExpression::Direction;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName=" └─ Axis", EditCondition="ExpressedAs==EPCGExDebugExpression::Direction", EditConditionHides))
	EPCGExAxis Axis = EPCGExAxis::Forward;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition="ExpressedAs==EPCGExDebugExpression::Direction", EditConditionHides))
	bool bNormalizeBeforeSizing = true;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName=" └─ Field", EditCondition="ExpressedAs==EPCGExDebugExpression::ConnectionToIndex", EditConditionHides))
	EPCGExSingleField Field = EPCGExSingleField::X;

	/** Draw line thickness. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	float Thickness = 1.0;

	/** Draw size. What it means depends on the selected debug type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size")
	double Size = 100.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size", meta = (InlineEditConditionToggle))
	bool bSizeFromAttribute = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size", meta = (EditCondition="bSizeFromAttribute"))
	FPCGExInputDescriptorWithSingleField SizeAttribute;

	/** Draw color. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color")
	FColor Color = FColor(255, 0, 0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color", meta = (EditCondition="bEnabled", InlineEditConditionToggle))
	bool bColorFromAttribute = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color", meta = (EditCondition="bColorFromAttribute"))
	FPCGExInputDescriptor ColorAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color", meta = (EditCondition="bColorFromAttribute"))
	bool bColorIsLinear = true;

	FString GetNestedStructDisplayText() const
	{
		return Selector.GetName().ToString();
	}
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

		switch (Descriptor->ExpressedAs)
		{
		case EPCGExDebugExpression::Direction:
		case EPCGExDebugExpression::Point:
		case EPCGExDebugExpression::ConnectionToPosition:
			VectorInput.Descriptor = static_cast<FPCGExInputDescriptor>(*Descriptor);
			VectorInput.Axis = Descriptor->Axis;
			bValid = VectorInput.Validate(InData);
			break;
		case EPCGExDebugExpression::ConnectionToIndex:
			IndexInput.Descriptor = static_cast<FPCGExInputDescriptor>(*Descriptor);
			IndexInput.Axis = Descriptor->Axis;
			IndexInput.Field = Descriptor->Field;
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
		else
		{
			SizeAttributeInput.bValid = false;
			ColorAttributeInput.bValid = false;
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

	FVector GetVector(const FPCGPoint& Point) const
	{
		FVector OutVector = VectorInput.GetValueSafe(Point, FVector::ZeroVector);
		if (Descriptor->ExpressedAs == EPCGExDebugExpression::Direction && Descriptor->bNormalizeBeforeSizing) { OutVector.Normalize(); }
		return OutVector;
	}

	FVector GetIndexedPosition(const FPCGPoint& Point, const UPCGPointData* PointData) const
	{
		const int64 OutIndex = IndexInput.GetValueSafe(Point, -1);
		if (OutIndex != -1) { return PointData->GetPoints()[OutIndex].Transform.GetLocation(); }
		return Point.Transform.GetLocation();
	}

	void Draw(const UWorld* World, const FVector& Start, const FPCGPoint& Point, const UPCGPointData* PointData) const
	{
		switch (Descriptor->ExpressedAs)
		{
		case EPCGExDebugExpression::Direction:
			DrawDirection(World, Start, Point);
			break;
		case EPCGExDebugExpression::ConnectionToIndex:
			DrawConnection(World, Start, Point, GetIndexedPosition(Point, PointData));
			break;
		case EPCGExDebugExpression::ConnectionToPosition:
			DrawConnection(World, Start, Point, GetVector(Point));
			break;
		case EPCGExDebugExpression::Point:
			DrawPoint(World, Start, Point);
			break;
		default: ;
		}
	}

protected:
	void DrawDirection(const UWorld* World, const FVector& Start, const FPCGPoint& Point) const
	{
		const FVector Dir = GetVector(Point) * GetSize(Point);
		DrawDebugDirectionalArrow(World, Start, Start + Dir, Dir.Length() * 0.05f, GetColor(Point), true, -1, 0, Descriptor->Thickness);
	}

	void DrawConnection(const UWorld* World, const FVector& Start, const FPCGPoint& Point, const FVector& End) const
	{
		DrawDebugLine(World, Start, End, GetColor(Point), true, -1, 0, Descriptor->Thickness);
	}

	void DrawPoint(const UWorld* World, const FVector& Start, const FPCGPoint& Point) const
	{
		const FVector End = GetVector(Point);
		DrawDebugPoint(World, End, GetSize(Point), GetColor(Point), true, -1, 0);
	}
};


/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExDrawAttributesSettings : public UPCGExPointsProcessorSettings, public IPCGExDebug
{
	GENERATED_BODY()

public:
	UPCGExDrawAttributesSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DrawAttributes, "Draw Attributes", "Draw debug attributes. Toggle debug OFF (D) before disabling this node (E)! Warning: this node will clear persistent debug lines before it!");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Debug; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorDebug; }
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	
	/** Attributes to draw.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(TitleProperty="{HiddenDisplayName} as {ExpressedAs}"))
	TArray<FPCGExAttributeDebugDrawDescriptor> DebugList;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin IPCGExDebug interface
public:
	virtual bool IsDebugEnabled() const override { return bEnabled && bDebug; }
	//~End IPCGExDebug interface

protected:
	virtual PCGExIO::EInitMode GetPointOutputInitMode() const override;

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
