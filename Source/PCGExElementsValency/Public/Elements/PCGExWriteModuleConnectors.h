// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExValencyProcessor.h"
#include "Core/PCGExValencyConnectorSet.h"
#include "Core/PCGExValencyMap.h"
#include "Data/PCGExPointIO.h"

#include "PCGExWriteModuleConnectors.generated.h"

/**
 * Writes module output connectors as new points for chained solving.
 * After staging resolves modules, this node outputs connector data that can be
 * used as input for a subsequent WriteValencyOrbitals (connector mode) -> Staging chain.
 *
 * Output: New point per output connector, with:
 *   - Transform: point transform * connector offset
 *   - Packed connector reference (int64) for downstream connector mode processing
 *   - Source point index for tracing back to original vertex
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Valency", meta=(Keywords = "valency connectors write output chaining", PCGExNodeLibraryDoc="valency/valency-write-module-connectors"))
class PCGEXELEMENTSVALENCY_API UPCGExWriteModuleConnectorsSettings : public UPCGExValencyProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(WriteModuleConnectors, "Valency : Write Module Connectors", "Outputs module connectors as points for chained solving.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	// This node requires BondingRules (for connector data) but not OrbitalSet
	virtual bool WantsOrbitalSet() const override { return false; }
	virtual bool WantsBondingRules() const override { return true; }

	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	/**
	 * Connector set asset defining connector types.
	 * Required for connector type -> index mapping and compatibility data.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExValencyConnectorSet> ConnectorSet;

	/** Suffix for the ValencyEntry attribute to read (e.g. "Main" -> "PCGEx/V/Entry/Main") */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName EntrySuffix = FName("Main");

	/**
	 * Attribute name for the packed connector reference output.
	 * This attribute is written to output connector points.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	FName ConnectorOutputAttributeName = FName("PCGEx/V/Connector/Main");

	/** Output an attribute containing the source vertex index */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bOutputSourceIndex = true;

	/** Attribute name for source vertex index */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bOutputSourceIndex"))
	FName SourceIndexAttributeName = FName("SourceIndex");

	/** Output an attribute containing the connector identifier */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bOutputConnectorIdentifier = false;

	/** Attribute name for connector identifier */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bOutputConnectorIdentifier"))
	FName ConnectorIdentifierAttributeName = FName("ConnectorIdentifier");

	/** Output an attribute containing the connector type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bOutputConnectorType = false;

	/** Attribute name for connector type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bOutputConnectorType"))
	FName ConnectorTypeAttributeName = FName("ConnectorType");

	/** Quiet mode - suppress missing connector set errors */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable))
	bool bQuietMissingConnectorSet = false;

private:
	friend class FPCGExWriteModuleConnectorsElement;
};

struct PCGEXELEMENTSVALENCY_API FPCGExWriteModuleConnectorsContext final : FPCGExValencyProcessorContext
{
	friend class FPCGExWriteModuleConnectorsElement;

	virtual void RegisterAssetDependencies() override;

	/** Valency unpacker for resolving ValencyEntry hashes */
	TSharedPtr<PCGExValency::FValencyUnpacker> ValencyUnpacker;

	/** Connector set (for type -> index mapping) */
	TObjectPtr<UPCGExValencyConnectorSet> ConnectorSet;

	/** Output point collection for connectors */
	TSharedPtr<PCGExData::FPointIOCollection> ConnectorOutputCollection;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class PCGEXELEMENTSVALENCY_API FPCGExWriteModuleConnectorsElement final : public FPCGExValencyProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(WriteModuleConnectors)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const override;
	virtual bool PostBoot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExWriteModuleConnectors
{
	class FProcessor final : public PCGExValencyMT::TProcessor<FPCGExWriteModuleConnectorsContext, UPCGExWriteModuleConnectorsSettings>
	{
		friend class FBatch;

	protected:
		/** ValencyEntry reader (from Solve output, via Valency Map) */
		TSharedPtr<PCGExData::TBuffer<int64>> ValencyEntryReader;

		/** Output connector points (local collection, merged into context output) */
		TSharedPtr<PCGExData::FPointIO> ConnectorOutput;

		/** Count of connectors written */
		int32 ConnectorCount = 0;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override = default;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;
	};

	class FBatch final : public PCGExValencyMT::TBatch<FProcessor>
	{
		/** ValencyEntry reader (owned here, shared with processors) */
		TSharedPtr<PCGExData::TBuffer<int64>> ValencyEntryReader;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);
		virtual ~FBatch() override;

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void OnProcessingPreparationComplete() override;
		virtual bool PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor) override;
		virtual void CompleteWork() override;
	};
}
