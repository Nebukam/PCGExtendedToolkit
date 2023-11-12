// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "PCGPin.h"
#include "Data/PCGExRelationsParamsData.h"
#include "PCGExCreateRelationsParams.generated.h"

/** Outputs a single RelationalParam to be consumed by other nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Relational|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExCreateRelationsParamsSettings : public UPCGSettings
{
	GENERATED_BODY()

	UPCGExCreateRelationsParamsSettings(const FObjectInitializer& ObjectInitializer);

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(RelationalParams, "Relational Params", "Builds a collection of PCG-compatible data from the selected actors.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Attribute name to store relation data to. Used as prefix.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName RelationIdentifier = "RelationIdentifier";

	/** Relation sockets model.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(TitleProperty="{SocketName}"))
	TArray<FPCGExSocketDescriptor> Sockets;

	/** Overrides individual socket values with a global one.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bApplyGlobalOverrides = false;

	/** Individual socket properties overrides */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bApplyGlobalOverrides"))
	FPCGExSocketGlobalOverrides GlobalOverrides;

	const TArray<FPCGExSocketDescriptor>& GetSockets() const;
	
protected:
	virtual void InitDefaultSockets();
};

class PCGEXTENDEDTOOLKIT_API FPCGExCreateRelationsParamsElement : public FSimplePCGElement
{
protected:
	template <typename T>
	T* BuildParams(FPCGContext* Context) const;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
