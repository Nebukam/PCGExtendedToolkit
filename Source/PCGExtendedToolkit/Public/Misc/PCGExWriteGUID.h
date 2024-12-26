// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
#include "PCGExPointsProcessor.h"
#include "Misc/Guid.h"

#include "PCGExWriteGUID.generated.h"

UENUM()
enum class EPCGExGUIDOutputType : uint8
{
	Integer = 0 UMETA(DisplayName = "Integer", ToolTip="..."),
	String  = 1 UMETA(DisplayName = "String", ToolTip="..."),
};

UENUM()
enum class EPCGExGUIDFormat : uint8
{
	Digits                         = 0 UMETA(DisplayName = "Digits", ToolTip="32 digits. For example: `00000000000000000000000000000000`"),
	DigitsLower                    = 1 UMETA(DisplayName = "Digits (Lowercase)", ToolTip="32 digits in lowercase. For example: `0123abc456def789abcd123ef4a5b6c7`"),
	DigitsWithHyphens              = 2 UMETA(DisplayName = "Digits with hyphens", ToolTip="32 digits separated by hyphens. For example: 00000000-0000-0000-0000-000000000000"),
	DigitsWithHyphensLower         = 3 UMETA(DisplayName = "Digits (Lowercase, RFC 4122)", ToolTip="32 digits separated by hyphens, in lowercase as described by RFC 4122. For example: bd048ce3-358b-46c5-8cee-627c719418f8"),
	DigitsWithHyphensInBraces      = 4 UMETA(DisplayName = "{Digits} with hypens", ToolTip="32 digits separated by hyphens and enclosed in braces. For example: {00000000-0000-0000-0000-000000000000}"),
	DigitsWithHyphensInParentheses = 5 UMETA(DisplayName = "(Digits)", ToolTip="32 digits separated by hyphens and enclosed in parentheses. For example: (00000000-0000-0000-0000-000000000000)"),
	HexValuesInBraces              = 6 UMETA(DisplayName = "{Hex}", ToolTip="Comma-separated hexadecimal values enclosed in braces. For example: {0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}"),
	UniqueObjectGuid               = 7 UMETA(DisplayName = "Unique Object GUID", ToolTip="This format is currently used by the FUniqueObjectGuid class. For example: 00000000-00000000-00000000-00000000"),
	Short                          = 8 UMETA(DisplayName = "Short (Base64)", ToolTip="Base64 characters with dashes and underscores instead of pluses and slashes (respectively), For example: AQsMCQ0PAAUKCgQEBAgADQ"),
	Base36Encoded                  = 9 UMETA(DisplayName = "Short (Base36)", ToolTip="Base-36 encoded, compatible with case-insensitive OS file systems (such as Windows). For example: 1DPF6ARFCM4XH5RMWPU8TGR0J"),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExWriteGUIDSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		WriteGUID, "Write GUID", "Write a GUID on the point.",
		OutputAttributeName);
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** The name of the attribute to write its index to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName OutputAttributeName = "GUID";

	/** A Key value used to make the GUID more unique when dealing with points that have the exact same location, index & parent actor. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	int32 UniqueKey = 42;

	/** Output type.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExGUIDOutputType OutputType = EPCGExGUIDOutputType::Integer;

	/** Output type.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExGUIDFormat Format = EPCGExGUIDFormat::Digits;

	/** Whether the created attribute allows interpolation or not.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAllowInterpolation = true;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExWriteGUIDContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExWriteGUIDElement;
	EGuidFormats Format = EGuidFormats::Digits;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExWriteGUIDElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExWriteGUID
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExWriteGUIDContext, UPCGExWriteGUIDSettings>
	{
		double NumPoints = 0;

		uint32 UIDComponent = 0;
		FVector CWTolerance = FVector(1 / 0.001);

		FGuid DefaultGUID = FGuid(0, 0, 0, 0);

		TSharedPtr<PCGExData::TBuffer<FString>> StringGUIDWriter;
		TSharedPtr<PCGExData::TBuffer<int32>> IntegerGUIDWriter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
