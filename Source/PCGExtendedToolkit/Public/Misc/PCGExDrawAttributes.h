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
	Label UMETA(DisplayName = "Label", ToolTip="Displays attribute values as string"),
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

	PCGEx::FLocalDirectionGetter VectorGetter;
	PCGEx::FLocalSingleFieldGetter IndexGetter;
	PCGEx::FLocalSingleFieldGetter SizeGetter;
	PCGEx::FLocalVectorInput ColorGetter;
	PCGEx::FLocalToStringGetter TextGetter;

	bool bValid = false;

	bool Validate(const UPCGPointData* InData);

	double GetSize(const FPCGPoint& Point) const;
	FColor GetColor(const FPCGPoint& Point) const;
	FVector GetVector(const FPCGPoint& Point) const;
	FVector GetIndexedPosition(const FPCGPoint& Point, const UPCGPointData* PointData) const;

	void Draw(const UWorld* World, const FVector& Start, const FPCGPoint& Point, const UPCGPointData* PointData) const;

protected:
	void DrawDirection(const UWorld* World, const FVector& Start, const FPCGPoint& Point) const;
	void DrawConnection(const UWorld* World, const FVector& Start, const FPCGPoint& Point, const FVector& End) const;
	void DrawPoint(const UWorld* World, const FVector& Start, const FPCGPoint& Point) const;
	void DrawLabel(const UWorld* World, const FVector& Start, const FPCGPoint& Point) const;

	template <typename T, typename dummy = void>
	FString AsString(const T& InValue) { return InValue.ToString(); }
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
