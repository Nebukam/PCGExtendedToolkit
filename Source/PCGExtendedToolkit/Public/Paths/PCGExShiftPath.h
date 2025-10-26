﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFactoryProvider.h"
#include "PCGExMath.h"
#include "PCGExPathProcessor.h"
#include "PCGExPaths.h"
#include "Data/Blending/PCGExDataBlending.h"

#include "PCGExShiftPath.generated.h"

namespace PCGExData
{
	class IBuffer;
}

UENUM()
enum class EPCGExShiftType : uint8
{
	Index                 = 0 UMETA(DisplayName = "Index", ToolTip="..."),
	Metadata              = 1 UMETA(DisplayName = "Metadata", ToolTip="..."),
	Properties            = 2 UMETA(DisplayName = "Properties", ToolTip="..."),
	MetadataAndProperties = 3 UMETA(DisplayName = "Metadata and Properties", ToolTip="..."),
	CherryPick            = 4 UMETA(DisplayName = "CherryPick", ToolTip="...")
};

UENUM()
enum class EPCGExShiftPathMode : uint8
{
	Discrete = 0 UMETA(DisplayName = "Discrete", ToolTip="Shift point is selected using a discrete value"),
	Relative = 1 UMETA(DisplayName = "Relative", ToolTip="Shift point is selected using a value relative to the input size"),
	Filter   = 2 UMETA(DisplayName = "Filter", ToolTip="Shift point using the first point that passes the provided filters"),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/shift"))
class UPCGExShiftPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExShiftPathSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UObject interface
#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathShift, "Path : Shift", "Shift path points");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	virtual PCGExData::EIOInit GetMainDataInitializationPolicy() const override;
	
	//~Begin UPCGExPointsProcessorSettings
public:
	PCGEX_NODE_POINT_FILTER(InputMode == EPCGExShiftPathMode::Filter ? PCGExPaths::SourceShiftFilters : NAME_None, "Filters used to find the shift starting point.", PCGExFactories::PointFilters, InputMode == EPCGExShiftPathMode::Filter)
	//~End UPCGExPointsProcessorSettings

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExShiftType ShiftType = EPCGExShiftType::MetadataAndProperties;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExShiftPathMode InputMode = EPCGExShiftPathMode::Relative;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="InputMode == EPCGExShiftPathMode::Relative", EditConditionHides))
	double RelativeConstant = 0.5;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="InputMode == EPCGExShiftPathMode::Relative", EditConditionHides))
	EPCGExTruncateMode Truncate = EPCGExTruncateMode::Round;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="InputMode == EPCGExShiftPathMode::Discrete", EditConditionHides))
	int32 DiscreteConstant = 0;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="InputMode != EPCGExShiftPathMode::Filter", EditConditionHides))
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Tile;

	/** Reverse shift order */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bReverseShift = false;

	/** Point properties to be shifted */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="ShiftType == EPCGExShiftType::CherryPick", EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExPointNativeProperties"))
	uint8 CherryPickedProperties = 0;

	/** Attributes to be shifted */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="ShiftType == EPCGExShiftType::CherryPick", EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExPointNativeProperties"))
	TArray<FName> CherryPickedAttributes;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietDoubleShiftWarning = false;
};

struct FPCGExShiftPathContext final : FPCGExPathProcessorContext
{
	friend class FPCGExShiftPathElement;
	FPCGExBlendingDetails BlendingSettings;
	EPCGPointNativeProperties ShiftedProperties;
	TArray<FPCGAttributeIdentifier> ShiftedAttributes;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExShiftPathElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ShiftPath)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExShiftPath
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExShiftPathContext, UPCGExShiftPathSettings>
	{
		int32 MaxIndex = 0;
		int32 PivotIndex = -1;

		TArray<int32> Indices;

		TArray<TSharedPtr<PCGExData::IBuffer>> Buffers;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;

		virtual void CompleteWork() override;
	};
}
