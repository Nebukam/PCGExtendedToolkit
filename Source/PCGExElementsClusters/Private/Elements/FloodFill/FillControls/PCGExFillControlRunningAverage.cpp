// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Elements/FloodFill/FillControls/PCGExFillControlRunningAverage.h"


#include "Data/PCGExData.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Details/PCGExSettingsDetails.h"
#include "Clusters/PCGExCluster.h"
#include "Containers/PCGExHashLookup.h"
#include "Containers/PCGExManagedObjects.h"
#include "Elements/FloodFill/FillControls/PCGExFillControlsFactoryProvider.h"

PCGEX_SETTING_VALUE_IMPL(FPCGExFillControlConfigRunningAverage, WindowSize, int32, WindowSizeInput, WindowSizeAttribute, WindowSize)
PCGEX_SETTING_VALUE_IMPL(FPCGExFillControlConfigRunningAverage, Tolerance, double, ToleranceInput, ToleranceAttribute, Tolerance)

bool FPCGExFillControlRunningAverage::PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler)
{
	if (!FPCGExFillControlOperation::PrepareForDiffusions(InContext, InHandler)) { return false; }

	const UPCGExFillControlsFactoryRunningAverage* TypedFactory = Cast<UPCGExFillControlsFactoryRunningAverage>(Factory);

	WindowSize = TypedFactory->Config.GetValueSettingWindowSize();
	if (!WindowSize->Init(GetSourceFacade())) { return false; }

	Tolerance = TypedFactory->Config.GetValueSettingTolerance();
	if (!Tolerance->Init(GetSourceFacade())) { return false; }

	Operand = InHandler->VtxDataFacade->GetBroadcaster<double>(TypedFactory->Config.Operand);
	if (!Operand)
	{
		PCGEX_LOG_INVALID_SELECTOR_C(InContext, Operand, TypedFactory->Config.Operand)
		return false;
	}

	return true;
}

bool FPCGExFillControlRunningAverage::IsValidCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, const PCGExFloodFill::FCandidate& Candidate)
{
	const int32 Window = WindowSize->Read(GetSettingsIndex(Diffusion));

	int32 PathNodeIndex = PCGEx::NH64A(Diffusion->TravelStack->Get(From.Node->Index));
	int32 PathEdgeIndex = -1;

	if (PathNodeIndex != -1)
	{
		double Avg = Operand->Read(Cluster->GetNodePointIndex(PathNodeIndex));
		int32 Sampled = 1;
		while (PathNodeIndex != -1 && Sampled < Window)
		{
			const int32 CurrentIndex = PathNodeIndex;
			PCGEx::NH64(Diffusion->TravelStack->Get(CurrentIndex), PathNodeIndex, PathEdgeIndex);
			if (PathNodeIndex != -1)
			{
				Avg += Operand->Read(Cluster->GetNodePointIndex(PathNodeIndex));
				Sampled++;
			}
		}

		return FMath::IsNearlyEqual((Avg / static_cast<double>(Sampled)), Operand->Read(Candidate.Node->PointIndex), Tolerance->Read(GetSettingsIndex(Diffusion)));
	}
	return true;
}

TSharedPtr<FPCGExFillControlOperation> UPCGExFillControlsFactoryRunningAverage::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(FillControlRunningAverage)
	PCGEX_FORWARD_FILLCONTROL_OPERATION
	return NewOperation;
}

void UPCGExFillControlsFactoryRunningAverage::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);

	FacadePreloader.Register<double>(InContext, Config.Operand);

	if (Config.Source == EPCGExFloodFillSettingSource::Vtx)
	{
		FacadePreloader.Register<int32>(InContext, Config.WindowSizeAttribute);
		FacadePreloader.Register<double>(InContext, Config.ToleranceAttribute);
	}
}

UPCGExFactoryData* UPCGExFillControlsRunningAverageProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExFillControlsFactoryRunningAverage* NewFactory = InContext->ManagedObjects->New<UPCGExFillControlsFactoryRunningAverage>();
	PCGEX_FORWARD_FILLCONTROL_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExFillControlsRunningAverageProviderSettings::GetDisplayName() const
{
	FString DName = GetDefaultNodeTitle().ToString().Replace(TEXT("PCGEx | Fill Control"), TEXT("FC")) + TEXT(" @ ");
	DName += PCGExMetaHelpers::GetSelectorDisplayName(Config.Operand);
	return DName;
}
#endif
