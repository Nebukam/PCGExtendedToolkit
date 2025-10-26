﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathSplineMeshSimple.h"

#include "PCGComponent.h"
#include "Components/SplineMeshComponent.h"

#include "PCGExHelpers.h"
#include "Collections/PCGExAssetLoader.h"
#include "Data/PCGExDataTag.h"
#include "Details/PCGExDetailsSettings.h"
#include "Metadata/PCGObjectPropertyOverride.h"
#include "Engine/StaticMesh.h"


#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExPathSplineMeshSimpleElement"
#define PCGEX_NAMESPACE BuildCustomGraph

PCGEX_SETTING_VALUE_IMPL(UPCGExPathSplineMeshSimpleSettings, StartOffset, FVector2D, StartOffsetInput, StartOffsetAttribute, StartOffset)
PCGEX_SETTING_VALUE_IMPL(UPCGExPathSplineMeshSimpleSettings, EndOffset, FVector2D, EndOffsetInput, EndOffsetAttribute, EndOffset)

#if WITH_EDITOR
void UPCGExPathSplineMeshSimpleSettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	if (SplineMeshAxisConstant_DEPRECATED != EPCGExMinimalAxis::None && StaticMeshDescriptor.SplineMeshAxis == EPCGExSplineMeshAxis::Default)
	{
		StaticMeshDescriptor.SplineMeshAxis = static_cast<EPCGExSplineMeshAxis>(SplineMeshAxisConstant_DEPRECATED);
	}

	Tangents.ApplyDeprecation(bApplyCustomTangents_DEPRECATED, ArriveTangentAttribute_DEPRECATED, LeaveTangentAttribute_DEPRECATED);

	Super::ApplyDeprecation(InOutNode);
}
#endif

PCGEX_INITIALIZE_ELEMENT(PathSplineMeshSimple)

PCGExData::EIOInit UPCGExPathSplineMeshSimpleSettings::GetIOPreInitForMainPoints() const{ return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(PathSplineMeshSimple)

UPCGExPathSplineMeshSimpleSettings::UPCGExPathSplineMeshSimpleSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (SplineMeshUpVectorAttribute.GetName() == FName("@Last")) { SplineMeshUpVectorAttribute.Update(TEXT("$Rotation.Up")); }
}

void FPCGExPathSplineMeshSimpleContext::AddExtraStructReferencedObjects(FReferenceCollector& Collector)
{
	if (StaticMeshLoader) { StaticMeshLoader->AddExtraStructReferencedObjects(Collector); }
	if (StaticMesh) { Collector.AddReferencedObject(StaticMesh); }

	FPCGExPathProcessorContext::AddExtraStructReferencedObjects(Collector);
}

bool FPCGExPathSplineMeshSimpleElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathSplineMeshSimple)

	if (!Context->Tangents.Init(Context, Settings->Tangents)) { return false; }

	if (Settings->AssetType == EPCGExInputValueType::Attribute)
	{
		PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->AssetPathAttributeName)

		TArray<FName> Names = {Settings->AssetPathAttributeName};
		Context->StaticMeshLoader = MakeShared<PCGEx::TAssetLoader<UStaticMesh>>(Context, Context->MainPoints.ToSharedRef(), Names);
	}
	else
	{
		Context->StaticMesh = PCGExHelpers::LoadBlocking_AnyThread(Settings->StaticMesh);
		if (!Context->StaticMesh)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Static mesh could not be loaded."));
			return false;
		}
	}

	if (Settings->bReadMaterialFromAttribute)
	{
		TArray<FName> Names = {Settings->MaterialAttributeName};
		Context->MaterialLoader = MakeShared<PCGEx::TAssetLoader<UMaterialInterface>>(Context, Context->MainPoints.ToSharedRef(), Names);
	}

	return true;
}

bool FPCGExPathSplineMeshSimpleElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathSplineMeshSimpleElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathSplineMeshSimple)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (Context->StaticMesh)
		{
			if (Context->MaterialLoader)
			{
				Context->SetAsyncState(PCGExCommon::State_WaitingOnAsyncWork);

				if (!Context->MaterialLoader->Start(Context->GetAsyncManager()))
				{
					return Context->CancelExecution(TEXT("Failed to find any material to load."));
				}
			}
			else
			{
				Context->SetState(PCGExCommon::State_WaitingOnAsyncWork);
			}
		}
		else
		{
			Context->SetAsyncState(PCGExCommon::State_WaitingOnAsyncWork);

			if (!Context->StaticMeshLoader->Start(Context->GetAsyncManager()))
			{
				return Context->CancelExecution(TEXT("Failed to find any asset to load."));
			}

			if (Context->MaterialLoader)
			{
				if (!Context->MaterialLoader->Start(Context->GetAsyncManager()))
				{
					return Context->CancelExecution(TEXT("Failed to find any material to load."));
				}
			}

			return false;
		}
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExCommon::State_WaitingOnAsyncWork)
	{
		if (Context->StaticMeshLoader && Context->StaticMeshLoader->IsEmpty())
		{
			return Context->CancelExecution(TEXT("Failed to load any assets."));
		}

		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bHasInvalidInputs = true;
					Entry->InitializeOutput(PCGExData::EIOInit::Forward);
					return false;
				}

				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to write tangents to."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)
	Context->ExecuteOnNotifyActors(Settings->PostProcessFunctionNames);

	return Context->TryComplete();
}

namespace PCGExPathSplineMeshSimple
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		bIsPreviewMode = ExecutionContext->GetComponent()->IsInPreviewMode();

		MutationDetails = Settings->MutationDetails;
		if (!MutationDetails.Init(PointDataFacade)) { return false; }

		StartOffset = Settings->GetValueSettingStartOffset();
		if (!StartOffset->Init(PointDataFacade)) { return false; }

		EndOffset = Settings->GetValueSettingEndOffset();
		if (!EndOffset->Init(PointDataFacade)) { return false; }

		if (Settings->SplineMeshUpMode == EPCGExSplineMeshUpMode::Attribute)
		{
			UpGetter = PointDataFacade->GetBroadcaster<FVector>(Settings->SplineMeshUpVectorAttribute, true);

			if (!UpGetter)
			{
				PCGEX_LOG_INVALID_SELECTOR_C(Context, Spline Mesh Up Vector, Settings->SplineMeshUpVectorAttribute)
				return false;
			}
		}

		if (Settings->AssetType == EPCGExInputValueType::Attribute)
		{
			AssetPathReader = PointDataFacade->GetBroadcaster<FSoftObjectPath>(Settings->AssetPathAttributeName, true);
			if (!AssetPathReader)
			{
				PCGEX_LOG_INVALID_ATTR_C(Context, Asset Path, Settings->AssetPathAttributeName)
				return false;
			}
		}

		if (Settings->bReadMaterialFromAttribute)
		{
			MaterialPathReader = PointDataFacade->GetBroadcaster<FSoftObjectPath>(Settings->MaterialAttributeName, true);
			if (!MaterialPathReader)
			{
				PCGEX_LOG_INVALID_ATTR_C(Context, Material Path, Settings->MaterialAttributeName)
				return false;
			}
		}

		bClosedLoop = PCGExPaths::GetClosedLoop(PointDataFacade->GetIn());
		bUseTags = Settings->TaggingDetails.IsEnabled();

		TangentsHandler = MakeShared<PCGExTangents::FTangentsHandler>(bClosedLoop);
		if (!TangentsHandler->Init(Context, Context->Tangents, PointDataFacade)) { return false; }

		LastIndex = PointDataFacade->GetNum() - 1;

		Segments.Init(PCGExPaths::FSplineMeshSegment(), bClosedLoop ? LastIndex + 1 : LastIndex);
		Meshes.Init(nullptr, Segments.Num());
		if (MaterialPathReader) { Materials.Init(nullptr, Segments.Num()); }

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
		TProcessor<FPCGExPathSplineMeshSimpleContext, UPCGExPathSplineMeshSimpleSettings>::PrepareLoopScopesForPoints(Loops);
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::PathSplineMeshSimple::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		auto InvalidPoint = [&]()
		{
			// Do nothing
		};

		const UPCGBasePointData* InPointData = PointDataFacade->GetIn();
		TConstPCGValueRange<FTransform> Transforms = InPointData->GetConstTransformValueRange();

		bool bAnyValidSegment = false;

		PCGEX_SCOPE_LOOP(Index)
		{
			if (Index == LastIndex && !bClosedLoop)
			{
				// Ignore last index, only used for maths reasons
				InvalidPoint();
				continue;
			}

			if (!PointFilterCache[Index])
			{
				InvalidPoint();
				continue;
			}

			const TObjectPtr<UStaticMesh>* SM = AssetPathReader ? Context->StaticMeshLoader->GetAsset(AssetPathReader->Read(Index)) : &Context->StaticMesh;

			if (!SM)
			{
				InvalidPoint();
				continue;
			}

			Meshes[Index] = *SM;
			if (const TObjectPtr<UMaterialInterface>* MI = MaterialPathReader ? Context->MaterialLoader->GetAsset(MaterialPathReader->Read(Index)) : nullptr)
			{
				Materials[Index] = *MI;
			}

			PCGExPaths::FSplineMeshSegment& Segment = Segments[Index];
			//

			const int32 NextIndex = Index + 1 > LastIndex ? 0 : Index + 1;

			//

			FVector OutScale = Transforms[Index].GetScale3D();

			//

			int32 C1 = 1;
			int32 C2 = 2;
			PCGExPaths::GetAxisForEntry(Settings->StaticMeshDescriptor, Segment.SplineMeshAxis, C1, C2, EPCGExSplineMeshAxis::X);

			Segment.Params.StartPos = Transforms[Index].GetLocation();
			Segment.Params.StartScale = FVector2D(OutScale[C1], OutScale[C2]);
			Segment.Params.StartRoll = Transforms[Index].GetRotation().Rotator().Roll;

			const FVector Scale = Transforms[NextIndex].GetScale3D();
			Segment.Params.EndPos = Transforms[NextIndex].GetLocation();
			Segment.Params.EndScale = FVector2D(Scale[C1], Scale[C2]);
			Segment.Params.EndRoll = Transforms[NextIndex].GetRotation().Rotator().Roll;

			Segment.Params.StartOffset = StartOffset->Read(Index);
			Segment.Params.EndOffset = EndOffset->Read(Index);

			if (TangentsHandler->IsEnabled())
			{
				TangentsHandler->GetSegmentTangents(Index, Segment.Params.StartTangent, Segment.Params.EndTangent);
			}
			else
			{
				Segment.Params.StartTangent = Transforms[Index].GetRotation().GetForwardVector();
				Segment.Params.EndTangent = Transforms[NextIndex].GetRotation().GetForwardVector();
			}

			if (UpGetter) { Segment.UpVector = UpGetter->Read(Index); }
			else if (Settings->SplineMeshUpMode == EPCGExSplineMeshUpMode::Constant) { Segment.UpVector = Settings->SplineMeshUpVector; }
			else { Segment.ComputeUpVectorFromTangents(); }

			MutationDetails.Mutate(Index, Segment);
			bAnyValidSegment = true;
		}

		if (bAnyValidSegment) { FPlatformAtomics::InterlockedExchange(&bHasValidSegments, 1); }
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		if (!bHasValidSegments)
		{
			bIsProcessorValid = false;
			return;
		}

		//

		TargetActor = Settings->TargetActor.Get() ? Settings->TargetActor.Get() : ExecutionContext->GetTargetActor(nullptr);
		ObjectFlags = (bIsPreviewMode ? RF_Transient : RF_NoFlags);
		DataTags = PointDataFacade->Source->Tags->FlattenToArrayOfNames();

		if (!TargetActor)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Invalid target actor."));
			bIsProcessorValid = false;
			return;
		}

		const int32 FinalNumSegments = Segments.Num();

		if (!FinalNumSegments)
		{
			bIsProcessorValid = false;
			return;
		}

		MainThreadLoop = MakeShared<PCGExMT::FScopeLoopOnMainThread>(FinalNumSegments);
		MainThreadLoop->OnIterationCallback = [&](const int32 Index, const PCGExMT::FScope& Scope) { ProcessSegment(Index); };

		PCGEX_ASYNC_HANDLE_CHKD_VOID(AsyncManager, MainThreadLoop)
	}

	void FProcessor::ProcessSegment(const int32 Index)
	{
		const PCGExPaths::FSplineMeshSegment& Segment = Segments[Index];
		if (!Meshes[Index]) { return; }

		USplineMeshComponent* SplineMeshComponent = Context->ManagedObjects->New<USplineMeshComponent>(
			TargetActor, MakeUniqueObjectName(
				TargetActor, USplineMeshComponent::StaticClass(),
				Context->UniqueNameGenerator->Get(TEXT("PCGSplineMeshComponent_") + Meshes[Index].GetName())), ObjectFlags);

		Segment.ApplySettings(SplineMeshComponent); // Init Component

		if (MaterialPathReader)
		{
			int32 SlotIndex = Settings->MaterialSlotConstant;

			if (SlotIndex < 0) { SlotIndex = 0; }
			if (Materials[Index]) { SplineMeshComponent->SetMaterial(SlotIndex, Materials[Index]); }
		}

		if (Settings->TaggingDetails.bForwardInputDataTags) { SplineMeshComponent->ComponentTags.Append(DataTags); }
		if (!Segment.Tags.IsEmpty()) { SplineMeshComponent->ComponentTags.Append(Segment.Tags.Array()); }

		Settings->StaticMeshDescriptor.InitComponent(SplineMeshComponent);

		if (!Settings->PropertyOverrideDescriptions.IsEmpty())
		{
			FPCGObjectOverrides DescriptorOverride(SplineMeshComponent);
			DescriptorOverride.Initialize(Settings->PropertyOverrideDescriptions, SplineMeshComponent, PointDataFacade->Source->GetIn(), Context);
			if (DescriptorOverride.IsValid() && !DescriptorOverride.Apply(Index))
			{
				PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("FailOverride", "Failed to override descriptor for input {0}"), Index));
			}
		}

		SplineMeshComponent->SetStaticMesh(Meshes[Index]); // Will trigger a force rebuild, so put this last

		Context->AttachManagedComponent(
			TargetActor, SplineMeshComponent,
			FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));

		Context->AddNotifyActor(TargetActor);
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->WriteFastest(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
