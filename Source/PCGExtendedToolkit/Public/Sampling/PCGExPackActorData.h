// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"
#include "Data/PCGExBufferHelper.h"

#include "PCGExPackActorData.generated.h"

namespace PCGExPackActorData
{
}

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, Abstract, MinimalAPI, DisplayName = "[PCGEx] Custom Actor Data Packer")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCustomActorDataPacker : public UPCGExOperation
{
	GENERATED_BODY()

public:
	/**
	 * Main initialization function. Called once, and is responsible for populating graph builder settings.
	 * At least one setting is expected to be found in the GraphSettings array. This is executed on the main thread.
	 * @param InContext - Context of the initialization
	 * @param OutSuccess
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "PCGEx|Execution")
	void InitializeWithContext(UPARAM(ref)const FPCGContext& InContext, bool& OutSuccess);

	/**
	 * Process an actor reference. This method is executed in a multi-threaded context
	 * @param InActor The actor to be processed.
	 * @param InPoint PCG point that correspond to the provided actor.
	 * @param InPointIndex The point index
	 * @param OutPoint Muted PCG Point that correspond to the actor
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "PCGEx|Execution")
	void ProcessEntry(AActor* InActor, const FPCGPoint& InPoint, const int32 InPointIndex, FPCGPoint& OutPoint);

	virtual void Cleanup() override
	{
		InputActors.Empty();
		Super::Cleanup();
	}

	UPROPERTY(BlueprintReadOnly, Category = "PCGEx|Inputs")
	TArray<TObjectPtr<AActor>> InputActors;

	TSharedPtr<PCGExData::FBufferHelper> Buffers;

#pragma region Init

	/**
	 * Initialize a point' attribute default value.
	 * Must be called during initialization.
	 * @param InAttributeName
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool InitInt32(const FName& InAttributeName, const int32& InValue);

	/**
	* Initialize a point' attribute default value.
	 * Must be called during initialization.
	 * @param InAttributeName
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool InitInt64(const FName& InAttributeName, const int64& InValue);

	/**
	* Initialize a point' attribute default value.
	 * Must be called during initialization.
	 * @param InAttributeName
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool InitFloat(const FName& InAttributeName, const float& InValue);

	/**
	* Initialize a point' attribute default value.
	 * Must be called during initialization.
	 * @param InAttributeName
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool InitDouble(const FName& InAttributeName, const double& InValue);

	/**
	* Initialize a point' attribute default value.
	 * Must be called during initialization.
	 * @param InAttributeName
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool InitVector2(const FName& InAttributeName, const FVector2D& InValue);

	/**
	* Initialize a point' attribute default value.
	 * Must be called during initialization.
	 * @param InAttributeName
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool InitVector(const FName& InAttributeName, const FVector& InValue);

	/**
	* Initialize a point' attribute default value.
	 * Must be called during initialization.
	 * @param InAttributeName
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool InitVector4(const FName& InAttributeName, const FVector4& InValue);

	/**
	* Initialize a point' attribute default value.
	 * Must be called during initialization.
	 * @param InAttributeName
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool InitQuat(const FName& InAttributeName, const FQuat& InValue);

	/**
	* Initialize a point' attribute default value.
	 * Must be called during initialization.
	 * @param InAttributeName
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Execution")
	bool InitTransform(const FName& InAttributeName, const FTransform& InValue);

	/**
	* Initialize a point' attribute default value.
	 * Must be called during initialization.
	 * @param InAttributeName
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool InitString(const FName& InAttributeName, const FString& InValue);

	/**
	* Initialize a point' attribute default value.
	 * Must be called during initialization.
	 * @param InAttributeName
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool InitBool(const FName& InAttributeName, const bool& InValue);

	/**
	* Initialize a point' attribute default value.
	 * Must be called during initialization.
	 * @param InAttributeName
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool InitRotator(const FName& InAttributeName, const FRotator& InValue);

	/**
	* Initialize a point' attribute default value.
	 * Must be called during initialization.
	 * @param InAttributeName
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool InitName(const FName& InAttributeName, const FName& InValue);

	/**
	* Initialize a point' attribute default value.
	 * Must be called during initialization.
	 * @param InAttributeName
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool InitSoftObjectPath(const FName& InAttributeName, const FSoftObjectPath& InValue);

	/**
	* Initialize a point' attribute default value.
	 * Must be called during initialization.
	 * @param InAttributeName
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool InitSoftClassPath(const FName& InAttributeName, const FSoftClassPath& InValue);

#pragma endregion

#pragma region Setters

	/**
	 * Set a point' attribute value at a given index.
	 * @param InAttributeName
	 * @param InPointIndex The point index to set the value to.
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool PackInt32(const FName& InAttributeName, const int32 InPointIndex, const int32& InValue);

	/**
	 * Set a point' attribute value at a given index.
	 * @param InAttributeName
	 * @param InPointIndex The point index to set the value to.
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool PackInt64(const FName& InAttributeName, const int32 InPointIndex, const int64& InValue);

	/**
	 * Set a point' attribute value at a given index.
	 * @param InAttributeName
	 * @param InPointIndex The point index to set the value to.
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool PackFloat(const FName& InAttributeName, const int32 InPointIndex, const float& InValue);

	/**
	 * Set a point' attribute value at a given index.
	 * @param InAttributeName
	 * @param InPointIndex The point index to set the value to.
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool PackDouble(const FName& InAttributeName, const int32 InPointIndex, const double& InValue);

	/**
	 * Set a point' attribute value at a given index.
	 * @param InAttributeName
	 * @param InPointIndex The point index to set the value to.
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool PackVector2(const FName& InAttributeName, const int32 InPointIndex, const FVector2D& InValue);

	/**
	 * Set a point' attribute value at a given index.
	 * @param InAttributeName
	 * @param InPointIndex The point index to set the value to.
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool PackVector(const FName& InAttributeName, const int32 InPointIndex, const FVector& InValue);

	/**
	 * Set a point' attribute value at a given index.
	 * @param InAttributeName
	 * @param InPointIndex The point index to set the value to.
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool PackVector4(const FName& InAttributeName, const int32 InPointIndex, const FVector4& InValue);

	/**
	 * Set a point' attribute value at a given index.
	 * @param InAttributeName
	 * @param InPointIndex The point index to set the value to.
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool PackQuat(const FName& InAttributeName, const int32 InPointIndex, const FQuat& InValue);

	/**
	 * Set a point' attribute value at a given index.
	 * @param InAttributeName
	 * @param InPointIndex The point index to set the value to.
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Execution")
	bool PackTransform(const FName& InAttributeName, const int32 InPointIndex, const FTransform& InValue);

	/**
	 * Set a point' attribute value at a given index.
	 * @param InAttributeName
	 * @param InPointIndex The point index to set the value to.
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool PackString(const FName& InAttributeName, const int32 InPointIndex, const FString& InValue);

	/**
	 * Set a point' attribute value at a given index.
	 * @param InAttributeName
	 * @param InPointIndex The point index to set the value to.
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool PackBool(const FName& InAttributeName, const int32 InPointIndex, const bool& InValue);

	/**
	 * Set a point' attribute value at a given index.
	 * @param InAttributeName
	 * @param InPointIndex The point index to set the value to.
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool PackRotator(const FName& InAttributeName, const int32 InPointIndex, const FRotator& InValue);

	/**
	 * Set a point' attribute value at a given index.
	 * @param InAttributeName
	 * @param InPointIndex The point index to set the value to.
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool PackName(const FName& InAttributeName, const int32 InPointIndex, const FName& InValue);

	/**
	 * Set a point' attribute value at a given index.
	 * @param InAttributeName
	 * @param InPointIndex The point index to set the value to.
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool PackSoftObjectPath(const FName& InAttributeName, const int32 InPointIndex, const FSoftObjectPath& InValue);

	/**
	 * Set a point' attribute value at a given index.
	 * @param InAttributeName
	 * @param InPointIndex The point index to set the value to.
	 * @param InValue
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Setter")
	bool PackSoftClassPath(const FName& InAttributeName, const int32 InPointIndex, const FSoftClassPath& InValue);

#pragma endregion
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Sampling")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPackActorDataSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPackActorDataSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		PackActorData, "Pack Actor Data", "Use custom blueprint to read data from actor references.",
		(Packer ? FName(Packer.GetClass()->GetMetaData(TEXT("DisplayName"))) : FName("...")));
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorSampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings

public:
	virtual FName GetMainInputPin() const override;
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Actor reference */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName ActorReferenceAttribute = FName(TEXT("ActorReference"));

	/** Builder instance. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExCustomActorDataPacker> Packer;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bOmitUnresolvedEntries = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bOmitEmptyOutputs = true;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPackActorDataContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExPackActorDataElement;
	UPCGExCustomActorDataPacker* Packer = nullptr;
	TArray<UPCGParamData*> OutputParams;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPackActorDataElement final : public FPCGExPointsProcessorElement
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

namespace PCGExPackActorDatas
{
	const FName SourceOverridesPacker = TEXT("Overrides : Packer");

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExPackActorDataContext, UPCGExPackActorDataSettings>
	{
		TArray<FPCGMetadataAttributeBase*> Attributes;
		UPCGExCustomActorDataPacker* Packer = nullptr;
		TSharedPtr<PCGEx::TAttributeBroadcaster<FSoftObjectPath>> ActorReferences;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
