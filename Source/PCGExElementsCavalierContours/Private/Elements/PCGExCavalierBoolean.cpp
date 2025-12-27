// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExCavalierBoolean.h"

#include "Core/PCGExCCOffset.h"
#include "Core/PCGExCCPolyline.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"
#include "Helpers/PCGExAsyncHelpers.h"
#include "Math/PCGExBestFitPlane.h"
#include "Paths/PCGExPathsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExCavalierBooleanElement"
#define PCGEX_NAMESPACE CavalierBoolean

PCGEX_INITIALIZE_ELEMENT(CavalierBoolean)

TArray<FPCGPinProperties> UPCGExCavalierBooleanSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties =  Super::InputPinProperties();
	//PCGEX_PIN_POINT(FName("Operands"), "The secondary set of paths that will be used as operands on the first one.", Required) 
	return PinProperties;
}



bool FPCGExCavalierBooleanElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CavalierBoolean)

	// TODO : Build all root paths in bulk
	const int32 NumInputs = Context->MainPoints->Num();
	Context->RootPaths.Reserve(NumInputs);
	Context->RootPolylines.Reserve(NumInputs);
	
	/*
	{
		PCGExAsyncHelpers::FAsyncExecutionScope BuildRootPathsTask(NumInputs);
		for (const TSharedPtr<PCGExData::FPointIO> IO : Context->MainPoints )
		{
			BuildRootPathsTask.Execute([CtxHandle = Context->GetOrCreateHandle(), IO]()
			{
				
			});
		}
	}
	*/
	
	return true;
}

bool FPCGExCavalierBooleanElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCavalierBooleanElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(CavalierBoolean)

	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
				
	}

	//PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
