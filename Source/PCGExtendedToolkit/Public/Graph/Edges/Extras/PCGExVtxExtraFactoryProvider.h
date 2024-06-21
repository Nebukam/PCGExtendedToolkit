// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExGraph.h"
#include "PCGExOperation.h"

#include "PCGExVtxExtraFactoryProvider.generated.h"

#define PCGEX_VTX_EXTRA_CREATE \
	NewOperation->Descriptor = Descriptor;

namespace PCGExVtxExtra
{
	const FName SourceExtrasLabel = TEXT("Extras");
	const FName OutputExtraLabel = TEXT("Extra");
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSimpleEdgeOutputSettings
{
	GENERATED_BODY()
	virtual ~FPCGExSimpleEdgeOutputSettings() = default;

	FPCGExSimpleEdgeOutputSettings()
	{
	}

	explicit FPCGExSimpleEdgeOutputSettings(const FString& Name)
		: DirectionAttribute(FName(FText::Format(FText::FromString(TEXT("{0}Dir")), FText::FromString(Name)).ToString())),
		  LengthAttribute(FName(FText::Format(FText::FromString(TEXT("{0}Len")), FText::FromString(Name)).ToString()))
	{
	}

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDirection = false;

	/** Name of the attribute to output the direction to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteDirection"))
	FName DirectionAttribute = "Direction";
	PCGEx::TFAttributeWriter<FVector>* DirWriter = nullptr;

	/** Invert the direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteDirection"))
	bool bInvertDirection = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteLength = false;

	/** Name of the attribute to output the length to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteLength"))
	FName LengthAttribute = "Length";
	PCGEx::TFAttributeWriter<double>* LengthWriter = nullptr;

	virtual bool Validate(const FPCGContext* InContext) const
	{
		if (bWriteDirection) { PCGEX_VALIDATE_NAME_C(InContext, DirectionAttribute); }
		if (bWriteLength) { PCGEX_VALIDATE_NAME_C(InContext, LengthAttribute); }
		return true;
	}

	virtual void Init(PCGExData::FPointIO* InVtx)
	{
		if (bWriteDirection)
		{
			DirWriter = new PCGEx::TFAttributeWriter<FVector>(DirectionAttribute);
			DirWriter->BindAndSetNumUninitialized(*InVtx);
		}

		if (bWriteLength)
		{
			LengthWriter = new PCGEx::TFAttributeWriter<double>(LengthAttribute);
			LengthWriter->BindAndSetNumUninitialized(*InVtx);
		}
	}

	void Set(const int32 EntryIndex, const double InLength, const FVector& InDir)
	{
		if (DirWriter) { DirWriter->Values[EntryIndex] = bInvertDirection ? InDir * -1 : InDir; }
		if (LengthWriter) { LengthWriter->Values[EntryIndex] = InLength; }
	}

	virtual void Set(const int32 EntryIndex, const PCGExCluster::FAdjacencyData& Data)
	{
		if (DirWriter) { DirWriter->Values[EntryIndex] = bInvertDirection ? Data.Direction * -1 : Data.Direction; }
		if (LengthWriter) { LengthWriter->Values[EntryIndex] = Data.Length; }
	}

	virtual void Write() const
	{
		if (DirWriter) { DirWriter->Write(); }
		if (LengthWriter) { LengthWriter->Write(); }
	}

	virtual void Write(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_ASYNC_WRITE_DELETE(AsyncManager, DirWriter)
		PCGEX_ASYNC_WRITE_DELETE(AsyncManager, LengthWriter)
	}

	virtual void Cleanup()
	{
		PCGEX_DELETE(DirWriter)
		PCGEX_DELETE(LengthWriter)
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExEdgeOutputWithIndexSettings : public FPCGExSimpleEdgeOutputSettings
{
	GENERATED_BODY()

	FPCGExEdgeOutputWithIndexSettings()
		: FPCGExSimpleEdgeOutputSettings()
	{
	}

	explicit FPCGExEdgeOutputWithIndexSettings(const FString& Name)
		: FPCGExSimpleEdgeOutputSettings(Name),
		  EdgeIndexAttribute(FName(FText::Format(FText::FromString(TEXT("{0}EdgeIndex")), FText::FromString(Name)).ToString())),
		  VtxIndexAttribute(FName(FText::Format(FText::FromString(TEXT("{0}VtxIndex")), FText::FromString(Name)).ToString())),
		  NeighborCountAttribute(FName(FText::Format(FText::FromString(TEXT("{0}NeighborCount")), FText::FromString(Name)).ToString()))
	{
	}

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle, DisplayAfter="LengthAttribute"))
	bool bWriteEdgeIndex = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteEdgeIndex", DisplayAfter="bWriteEdgeIndex"))
	FName EdgeIndexAttribute = "EdgeIndex";
	PCGEx::TFAttributeWriter<int32>* EIdxWriter = nullptr;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle, DisplayAfter="EdgeIndexAttribute"))
	bool bWriteVtxIndex = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteVtxIndex", DisplayAfter="bWriteVtxIndex"))
	FName VtxIndexAttribute = "VtxIndex";
	PCGEx::TFAttributeWriter<int32>* VIdxWriter = nullptr;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle, DisplayAfter="VtxIndexAttribute"))
	bool bWriteNeighborCount = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteNeighborCount", DisplayAfter="bWriteNeighborCount"))
	FName NeighborCountAttribute = "Count";
	PCGEx::TFAttributeWriter<int32>* NCountWriter = nullptr;

	virtual bool Validate(const FPCGContext* InContext) const override
	{
		if (!FPCGExSimpleEdgeOutputSettings::Validate(InContext)) { return false; }
		if (bWriteEdgeIndex) { PCGEX_VALIDATE_NAME_C(InContext, EdgeIndexAttribute); }
		if (bWriteVtxIndex) { PCGEX_VALIDATE_NAME_C(InContext, VtxIndexAttribute); }
		if (bWriteNeighborCount) { PCGEX_VALIDATE_NAME_C(InContext, NeighborCountAttribute); }
		return true;
	}

	virtual void Init(PCGExData::FPointIO* InVtx) override
	{
		FPCGExSimpleEdgeOutputSettings::Init(InVtx);

		if (bWriteEdgeIndex)
		{
			EIdxWriter = new PCGEx::TFAttributeWriter<int32>(EdgeIndexAttribute);
			EIdxWriter->BindAndSetNumUninitialized(*InVtx);
		}

		if (bWriteVtxIndex)
		{
			VIdxWriter = new PCGEx::TFAttributeWriter<int32>(VtxIndexAttribute);
			VIdxWriter->BindAndSetNumUninitialized(*InVtx);
		}

		if (bWriteNeighborCount)
		{
			NCountWriter = new PCGEx::TFAttributeWriter<int32>(NeighborCountAttribute);
			NCountWriter->BindAndSetNumUninitialized(*InVtx);
		}
	}

	void Set(const int32 EntryIndex, const double InLength, const FVector& InDir, const int32 EIndex, const int32 VIndex, const int32 NeighborCount)
	{
		FPCGExSimpleEdgeOutputSettings::Set(EntryIndex, InLength, InDir);
		if (EIdxWriter) { EIdxWriter->Values[EntryIndex] = EIndex; }
		if (VIdxWriter) { VIdxWriter->Values[EntryIndex] = VIndex; }
		if (NCountWriter) { NCountWriter->Values[EntryIndex] = NeighborCount; }
	}

	virtual void Set(const int32 EntryIndex, const PCGExCluster::FAdjacencyData& Data) override
	{
		FPCGExSimpleEdgeOutputSettings::Set(EntryIndex, Data);
		if (EIdxWriter) { EIdxWriter->Values[EntryIndex] = Data.EdgeIndex; }
		if (VIdxWriter) { VIdxWriter->Values[EntryIndex] = Data.NodePointIndex; }
	}

	virtual void Set(const int32 EntryIndex, const PCGExCluster::FAdjacencyData& Data, const int32 NeighborCount)
	{
		Set(EntryIndex, Data);
		if (NCountWriter) { NCountWriter->Values[EntryIndex] = NeighborCount; }
	}

	virtual void Write() const override
	{
		FPCGExSimpleEdgeOutputSettings::Write();
		if (EIdxWriter) { EIdxWriter->Write(); }
		if (VIdxWriter) { VIdxWriter->Write(); }
		if (NCountWriter) { NCountWriter->Write(); }
	}

	virtual void Write(PCGExMT::FTaskManager* AsyncManager) override
	{
		FPCGExSimpleEdgeOutputSettings::Write(AsyncManager);
		PCGEX_ASYNC_WRITE_DELETE(AsyncManager, EIdxWriter)
		PCGEX_ASYNC_WRITE_DELETE(AsyncManager, VIdxWriter)
		PCGEX_ASYNC_WRITE_DELETE(AsyncManager, NCountWriter)
	}

	virtual void Cleanup() override
	{
		FPCGExSimpleEdgeOutputSettings::Cleanup();
		PCGEX_DELETE(EIdxWriter)
		PCGEX_DELETE(VIdxWriter)
		PCGEX_DELETE(NCountWriter)
	}
};

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExVtxExtraOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual bool PrepareForVtx(const FPCGContext* InContext, PCGExData::FPointIO* InVtx);
	virtual bool IsOperationValid();

	FORCEINLINE virtual void ProcessNode(const PCGExCluster::FCluster* Cluster, PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency);

	virtual void Write() override;
	virtual void Write(PCGExMT::FTaskManager* AsyncManager) override;

	virtual void Cleanup() override;

protected:
	bool bIsValidOperation = true;
	PCGExData::FPointIO* Vtx = nullptr;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExVtxExtraFactoryBase : public UPCGExNodeStateFactory
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override;
	virtual UPCGExVtxExtraOperation* CreateOperation() const;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|VtxExtra")
class PCGEXTENDEDTOOLKIT_API UPCGExVtxExtraProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(VtxExtraAttribute, "Vtx Extra : Abstract", "Abstract vtx extra settings.")
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorSamplerNeighbor; }

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
#endif
	//~End UPCGSettings

public:
	virtual FName GetMainOutputLabel() const override;
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Priority for sampling order. Higher values are processed last. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	//int32 Priority = 0;
};
