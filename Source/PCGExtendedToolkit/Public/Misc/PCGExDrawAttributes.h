// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExDrawAttributes.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Debug Expression"))
enum class EPCGExDebugExpression : uint8
{
	Direction            = 0 UMETA(DisplayName = "Direction", ToolTip="Attribute is treated as a Normal."),
	ConnectionToIndex    = 1 UMETA(DisplayName = "Connection (Point Index)", ToolTip="Attribute is treated as a lookup index in the same data block."),
	ConnectionToPosition = 2 UMETA(DisplayName = "Connection (Position)", ToolTip="Attribute is treated as world space position in the same data block."),
	Point                = 3 UMETA(DisplayName = "Point", ToolTip="Attribute is treated as a world space position."),
	Boolean              = 4 UMETA(DisplayName = "Boolean", ToolTip="Attribute is treated as a boolean switch between two colors.")
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAttributeDebugDrawConfig : public FPCGExInputConfig
{
	GENERATED_BODY()

	FPCGExAttributeDebugDrawConfig()
	{
		LocalColorAttribute.Update(TEXT("$Color"));
	}

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Enable or disable this debug group. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	bool bEnabled = true;

	/** Draw line thickness. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDebugExpression ExpressedAs = EPCGExDebugExpression::Direction;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ExpressedAs==EPCGExDebugExpression::ConnectionToIndex||ExpressedAs==EPCGExDebugExpression::ConnectionToPosition||ExpressedAs==EPCGExDebugExpression::Point", EditConditionHides))
	bool bAsOffset = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ExpressedAs==EPCGExDebugExpression::Direction", EditConditionHides))
	bool bNormalizeBeforeSizing = true;

	/** Draw line thickness. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0.001, ClampMax=10))
	float Thickness = 1.0;

	/** Draw size. What it means depends on the selected debug type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Size, meta=(PCG_Overridable, ClampMin=0.000001))
	double Size = 100.0;

	/** Fetch the size from a local attribute. The regular Size parameter then act as a scale.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Size, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bSizeFromAttribute = false;

	/** Fetch the size from a local attribute. The regular Size parameter then act as a scale.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Size, meta = (PCG_Overridable, EditCondition="bSizeFromAttribute"))
	FPCGAttributePropertyInputSelector LocalSizeAttribute;

	/** Draw color. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Color, meta=(PCG_Overridable))
	FColor Color = FColor(255, 0, 0); /** Draw color. */

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Color, meta=(PCG_Overridable, EditConditionHides, EditCondition="ExpressedAs==EPCGExDebugExpression::Boolean"))
	FColor SecondaryColor = FColor(0, 255, 0);

	/** Fetch the color from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Color, meta = (PCG_Overridable, EditCondition="bEnabled", InlineEditConditionToggle))
	bool bColorFromAttribute = false;

	/** Fetch the color from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Color, meta = (PCG_Overridable, EditCondition="bColorFromAttribute"))
	FPCGAttributePropertyInputSelector LocalColorAttribute;

	/** Basically divides input values by 255*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Color, meta = (PCG_Overridable, EditCondition="bColorFromAttribute"))
	bool bColorIsLinear = true;

	/** Depth priority. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 DepthPriority = 0;

	FString GetNestedStructDisplayText() const
	{
		return Selector.GetName().ToString();
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAttributeDebugDraw
{
	GENERATED_BODY()

	FPCGExAttributeDebugDraw()
	{
		Config = nullptr;
	}

	FPCGExAttributeDebugDrawConfig* Config;

	TSharedPtr<PCGEx::TAttributeBroadcaster<FVector>> VectorGetter;
	TSharedPtr<PCGEx::TAttributeBroadcaster<int32>> IndexGetter;
	TSharedPtr<PCGEx::TAttributeBroadcaster<double>> SingleGetter;
	TSharedPtr<PCGEx::TAttributeBroadcaster<double>> SizeGetter;
	TSharedPtr<PCGEx::TAttributeBroadcaster<FVector>> ColorGetter;
	TSharedPtr<PCGEx::TAttributeBroadcaster<FString>> TextGetter;

	bool bValid = false;

	bool Bind(const TSharedRef<PCGExData::FPointIO>& PointIO);

	double GetSize(const PCGExData::FPointRef& Point) const;
	FColor GetColor(const PCGExData::FPointRef& Point) const;
	double GetSingle(const PCGExData::FPointRef& Point) const;
	FVector GetVector(const PCGExData::FPointRef& Point) const;
	FVector GetIndexedPosition(const PCGExData::FPointRef& Point, const UPCGPointData* PointData) const;

	void Draw(const UWorld* World, const FVector& Start, const PCGExData::FPointRef& Point, const UPCGPointData* PointData) const;

protected:
	void DrawDirection(const UWorld* World, const FVector& Start, const PCGExData::FPointRef& Point) const;
	void DrawConnection(const UWorld* World, const FVector& Start, const PCGExData::FPointRef& Point, const FVector& End) const;
	void DrawPoint(const UWorld* World, const FVector& Start, const PCGExData::FPointRef& Point) const;
	void DrawSingle(const UWorld* World, const FVector& Start, const PCGExData::FPointRef& Point) const;
};


/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExDrawAttributesSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExDrawAttributesSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DrawAttributes, "Draw Attributes", "Draw debug attributes. Toggle debug OFF (D) before disabling this node (E)! Warning: this node will clear persistent debug lines before it!");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Debug; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorDebug; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	/** Attributes to draw.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(TitleProperty="{TitlePropertyName} as {ExpressedAs}"))
	TArray<FPCGExAttributeDebugDrawConfig> DebugList;

	/** Debug drawing toggle. Exposed to have more control on debug draw in sub-graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Debug, meta=(PCG_Overridable))
	bool bPCGExDebug = true;

protected:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;

private:
	friend class FPCGExDrawAttributesElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDrawAttributesContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExWriteIndexElement;
	TArray<FPCGExAttributeDebugDraw> DebugList;
};


class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDrawAttributesElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return true; }
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
