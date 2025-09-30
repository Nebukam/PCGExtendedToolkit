// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "UObject/Object.h"

#include "PCGExProbeFactoryProvider.h"
#include "PCGExProbeOperation.h"
#include "Details/PCGExDetailsSettings.h"


#include "PCGExProbeNumericCompare.generated.h"

namespace PCGExProbing
{
	struct FCandidate;
}

USTRUCT(BlueprintType)
struct FPCGExProbeConfigNumericCompare : public FPCGExProbeConfigBase
{
	GENERATED_BODY()

	FPCGExProbeConfigNumericCompare() :
		FPCGExProbeConfigBase()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType MaxConnectionsInput = EPCGExInputValueType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Max Connections (Attr)", EditCondition="MaxConnectionsInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector MaxConnectionsAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Max Connections", ClampMin=0, EditCondition="MaxConnectionsInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0))
	int32 MaxConnectionsConstant = 1;

	PCGEX_SETTING_VALUE_GET(MaxConnections, int32, MaxConnectionsInput, MaxConnectionsAttribute, MaxConnectionsConstant)


	/** Attribute to compare */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector Attribute;

	/** Comparison check */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Comparison"))
	EPCGExComparison Comparison = EPCGExComparison::StrictlyGreater;

	/** Rounding mode for approx. comparison modes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison == EPCGExComparison::NearlyEqual || Comparison == EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = DBL_COMPARE_TOLERANCE;

	/** Attempts to prevent connections that are roughly in the same direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bPreventCoincidence = true;

	/** Attempts to prevent connections that are roughly in the same direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bPreventCoincidence", ClampMin=0.00001))
	double CoincidencePreventionTolerance = 0.001;
};

/**
 * 
 */
class FPCGExProbeNumericCompare : public FPCGExProbeOperation
{
public:
	virtual bool PrepareForPoints(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIO>& InPointIO) override;
	virtual void ProcessCandidates(const int32 Index, const FTransform& WorkingTransform, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges) override;
	virtual void ProcessNode(const int32 Index, const FTransform& WorkingTransform, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, const TArray<int8>& AcceptConnections) override;

	FPCGExProbeConfigNumericCompare Config;

	TSharedPtr<PCGExDetails::TSettingValue<int32>> MaxConnections;
	TSharedPtr<PCGExData::TBuffer<double>> ValuesBuffer;

protected:
	FVector CWCoincidenceTolerance = FVector::ZeroVector;
};

////

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExProbeFactoryNumericCompare : public UPCGExProbeFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExProbeConfigNumericCompare Config;

	virtual TSharedPtr<FPCGExProbeOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params", meta=(PCGExNodeLibraryDoc="clusters/connect-points/probe-compare"))
class UPCGExProbeNumericCompareProviderSettings : public UPCGExProbeFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		ProbeNumericCompare, "Probe : Numeric Compare", "Connect points that pass the value comparison between the probing point and the candidate point.",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExProbeConfigNumericCompare Config;


#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
