﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"
#include "Data/Blending/PCGExDataBlending.h"

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

	TSharedPtr<PCGExData::FFacade> PointDataFacade;

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


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPackActorDataSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPackActorDataSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PackActorData, "Pack Actor Data", "Use custom blueprint to read data from actor references.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorSampler; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings

public:
	virtual FName GetMainInputLabel() const override;
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;

	//~End UPCGExPointsProcessorSettings

	/** Actor reference */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Mode==EPCGExCustomGraphActorSourceMode::ActorReferences", EditConditionHides))
	FName ActorReferenceAttribute = FName(TEXT("ActorReference"));

	/** Builder instance. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExCustomActorDataPacker> Packer;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPackActorDataContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExPackActorDataElement;
	UPCGExCustomActorDataPacker* Packer = nullptr;
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
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExPackActorDataContext, UPCGExPackActorDataSettings>
	{
		UPCGExCustomActorDataPacker* Packer = nullptr;
		TSharedPtr<PCGEx::TAttributeBroadcaster<FString>> ActorReferences;
		TArray<AActor*> InputActors;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}