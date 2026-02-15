// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsValencyEditor.h"

#include "PCGExAssetTypesMacros.h"
#include "PropertyEditorModule.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorMode/PCGExValencyEditorModeToolkit.h"
#include "EditorMode/PCGExValencyCageConnectorVisualizer.h"
#include "EditorMode/PCGExConstraintVisualizer.h"
#include "EditorMode/Constraints/PCGExConstraintVis_AngularRange.h"
#include "EditorMode/Constraints/PCGExConstraintVis_SurfaceOffset.h"
#include "EditorMode/Constraints/PCGExConstraintVis_VolumeOffset.h"
#include "EditorMode/Constraints/PCGExConstraintVis_HemisphereOffset.h"
#include "Growth/Constraints/PCGExConstraint_AngularRange.h"
#include "Growth/Constraints/PCGExConstraint_SurfaceOffset.h"
#include "Growth/Constraints/PCGExConstraint_VolumeOffset.h"
#include "Growth/Constraints/PCGExConstraint_HemisphereOffset.h"
#include "Components/PCGExValencyCageConnectorComponent.h"
#include "Details/PCGExPropertyOutputConfigCustomization.h"
#include "Details/PCGExValencyConnectorCompatibilityCustomization.h"

void FPCGExElementsValencyEditorModule::StartupModule()
{
	IPCGExEditorModuleInterface::StartupModule();

	// Register editor mode command bindings
	FValencyEditorCommands::Register();

	// Register connector component visualizer
	if (GUnrealEd)
	{
		GUnrealEd->RegisterComponentVisualizer(
			UPCGExValencyCageConnectorComponent::StaticClass()->GetFName(),
			MakeShareable(new FPCGExValencyCageConnectorVisualizer()));
	}

	// Register constraint visualizers
	{
		FConstraintVisualizerRegistry& Registry = FConstraintVisualizerRegistry::Get();
		Registry.Register<FPCGExConstraint_AngularRange, FAngularRangeVisualizer>();
		Registry.Register<FPCGExConstraint_SurfaceOffset, FSurfaceOffsetVisualizer>();
		Registry.Register<FPCGExConstraint_VolumeOffset, FVolumeOffsetVisualizer>();
		Registry.Register<FPCGExConstraint_HemisphereOffset, FHemisphereOffsetVisualizer>();
	}

	// Property customizations
	PCGEX_REGISTER_CUSTO_START
	PCGEX_REGISTER_CUSTO("PCGExValencyPropertyOutputConfig", FPCGExPropertyOutputConfigCustomization)
	PCGEX_REGISTER_CUSTO("PCGExValencyConnectorEntry", FPCGExValencyConnectorEntryCustomization)
}

void FPCGExElementsValencyEditorModule::ShutdownModule()
{
	// Unregister connector component visualizer
	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(
			UPCGExValencyCageConnectorComponent::StaticClass()->GetFName());
	}

	// Unregister editor mode command bindings
	FValencyEditorCommands::Unregister();

	IPCGExEditorModuleInterface::ShutdownModule();
}

PCGEX_IMPLEMENT_MODULE(FPCGExElementsValencyEditorModule, PCGExElementsValencyEditor)
