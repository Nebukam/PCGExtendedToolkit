// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExValencyProcessor.h"
#include "Core/PCGExValencySocketRules.h"
#include "Data/PCGExPointIO.h"

#include "PCGExWriteModuleSockets.generated.h"

/**
 * Writes module output sockets as new points for chained solving.
 * After staging resolves modules, this node outputs socket data that can be
 * used as input for a subsequent WriteValencyOrbitals (socket mode) → Staging chain.
 *
 * Output: New point per output socket, with:
 *   - Transform: point transform * socket offset
 *   - Packed socket reference (int64) for downstream socket mode processing
 *   - Source point index for tracing back to original vertex
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Valency", meta=(Keywords = "valency sockets write output chaining", PCGExNodeLibraryDoc="valency/write-module-sockets"))
class PCGEXELEMENTSVALENCY_API UPCGExWriteModuleSocketsSettings : public UPCGExValencyProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(WriteModuleSockets, "Valency : Write Module Sockets", "Outputs module sockets as points for chained solving.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	// This node requires BondingRules (for socket data) but not OrbitalSet
	virtual bool WantsOrbitalSet() const override { return false; }
	virtual bool WantsBondingRules() const override { return true; }

	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	/**
	 * Socket rules asset defining socket types.
	 * Required for socket type → index mapping and compatibility data.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExValencySocketRules> SocketRules;

	/**
	 * Attribute name for the module data (from staging output).
	 * Default matches the output from ValencyStaging with layer "Main".
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName ModuleDataAttributeName = FName("PCGEx/V/Module/Main");

	/**
	 * Attribute name for the packed socket reference output.
	 * This attribute is written to output socket points.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	FName SocketOutputAttributeName = FName("PCGEx/V/Socket/Main");

	/** Output an attribute containing the source vertex index */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bOutputSourceIndex = true;

	/** Attribute name for source vertex index */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bOutputSourceIndex"))
	FName SourceIndexAttributeName = FName("SourceIndex");

	/** Output an attribute containing the socket name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bOutputSocketName = false;

	/** Attribute name for socket name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bOutputSocketName"))
	FName SocketNameAttributeName = FName("SocketName");

	/** Output an attribute containing the socket type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bOutputSocketType = false;

	/** Attribute name for socket type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bOutputSocketType"))
	FName SocketTypeAttributeName = FName("SocketType");

	/** Quiet mode - suppress missing socket rules errors */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable))
	bool bQuietMissingSocketRules = false;

private:
	friend class FPCGExWriteModuleSocketsElement;
};

struct PCGEXELEMENTSVALENCY_API FPCGExWriteModuleSocketsContext final : FPCGExValencyProcessorContext
{
	friend class FPCGExWriteModuleSocketsElement;

	virtual void RegisterAssetDependencies() override;

	/** Socket rules (for type → index mapping) */
	TObjectPtr<UPCGExValencySocketRules> SocketRules;

	/** Output point collection for sockets */
	TSharedPtr<PCGExData::FPointIOCollection> SocketOutputCollection;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class PCGEXELEMENTSVALENCY_API FPCGExWriteModuleSocketsElement final : public FPCGExValencyProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(WriteModuleSockets)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const override;
	virtual bool PostBoot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExWriteModuleSockets
{
	class FProcessor final : public PCGExValencyMT::TProcessor<FPCGExWriteModuleSocketsContext, UPCGExWriteModuleSocketsSettings>
	{
		friend class FBatch;

	protected:
		/** Module data reader (from staging output) */
		TSharedPtr<PCGExData::TBuffer<int64>> ModuleDataReader;

		/** Output socket points (local collection, merged into context output) */
		TSharedPtr<PCGExData::FPointIO> SocketOutput;

		/** Count of sockets written */
		int32 SocketCount = 0;

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
		/** Module data reader (owned here, shared with processors) */
		TSharedPtr<PCGExData::TBuffer<int64>> ModuleDataReader;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);
		virtual ~FBatch() override;

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void OnProcessingPreparationComplete() override;
		virtual bool PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor) override;
		virtual void CompleteWork() override;
	};
}
