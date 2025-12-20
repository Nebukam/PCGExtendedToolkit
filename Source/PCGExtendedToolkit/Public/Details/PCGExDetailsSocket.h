// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "Engine/EngineTypes.h"

#include "Data/PCGExDataCommon.h"
#include "Details/PCGExSettingsMacros.h"
#include "Sampling/PCGExSampling.h"

#include "PCGExDetailsSocket.generated.h"

namespace PCGExDetails
{
	template <typename T>
	class TSettingValue;
}

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExSocket
{
	GENERATED_BODY()

	FPCGExSocket() = default;
	FPCGExSocket(const FName& InSocketName, const FVector& InRelativeLocation, const FRotator& InRelativeRotation, const FVector& InRelativeScale, FString InTag);
	FPCGExSocket(const FName& InSocketName, const FTransform& InRelativeTransform, const FString& InTag);
	~FPCGExSocket() = default;

	UPROPERTY(meta=(PCG_NotOverridable, EditCondition="false", EditConditionHides))
	bool bManaged = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bManaged"))
	FName SocketName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bManaged"))
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bManaged"))
	FString Tag = TEXT("");
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExSocketFitDetails
{
	GENERATED_BODY()

	FPCGExSocketFitDetails() = default;

	/** Whether socket fit is enabled or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bEnabled = false;

	/** Type of Socket name input */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bEnabled"))
	EPCGExInputValueType SocketNameInput = EPCGExInputValueType::Attribute;

	/** Attribute to read socket name from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Socket Name (Attr)", EditCondition="bEnabled && SocketNameInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName SocketNameAttribute = NAME_None;

	/** Socket name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Socket Name", EditCondition="bEnabled && SocketNameInput == EPCGExInputValueType::Constant", EditConditionHides))
	FName SocketName = NAME_None;

	PCGEX_SETTING_VALUE_DECL(SocketName, FName)

	/** Type of Socket name input */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	//EPCGExInputValueType SocketTagInput = EPCGExInputValueType::Attribute;

	/** Attribute to read socket name from. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Socket Tag (Attr)", EditCondition="SocketTagInput != EPCGExInputValueType::Constant", EditConditionHides))
	//FName SocketTagttribute = NAME_None;

	/** Socket name */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Socket Tag", EditCondition="SocketTagInput == EPCGExInputValueType::Constant", EditConditionHides))
	//FString SocketTag = TEXT("");

	//PCGEX_SETTING_VALUE_GET(SocketTag, FString, SocketTagInput, SocketTagAttribute, SocketTag)

	bool Init(const TSharedPtr<PCGExData::FFacade>& InFacade);
	void Mutate(const int32 Index, const TArray<FPCGExSocket>& InSockets, FTransform& InOutTransform) const;

protected:
	bool bMutate = false;
	TSharedPtr<PCGExDetails::TSettingValue<FName>> SocketNameBuffer;
};
