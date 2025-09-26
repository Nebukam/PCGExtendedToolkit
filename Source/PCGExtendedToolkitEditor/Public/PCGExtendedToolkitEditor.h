// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Collections/PCGExActorDataPackerActions.h"
#include "Modules/ModuleInterface.h"
#include "Styling/SlateStyle.h"

class FPCGExActorCollectionActions;
class FPCGExMeshCollectionActions;

class FPCGExtendedToolkitEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

protected:
	TSharedPtr<FPCGExMeshCollectionActions> MeshCollectionActions;
	TSharedPtr<FPCGExActorCollectionActions> ActorCollectionActions;
	TSharedPtr<FPCGExActorDataPackerActions> ActorPackerActions;
	TSharedPtr<FSlateStyleSet> Style;

	void RegisterDataVisualizations();
	void RegisterPinColorAndIcons();
	void RegisterMenuExtensions();
	void UnregisterMenuExtensions();

#pragma region Pin Colors

	FLinearColor GetPinColorAction() const;
	FLinearColor GetPinColorBlendOp() const;
	FLinearColor GetPinColorMatchRule() const;
	FLinearColor GetPinColorFilter() const;
	FLinearColor GetPinColorFilterPoint() const;
	FLinearColor GetPinColorFilterCollection() const;
	FLinearColor GetPinColorFilterCluster() const;
	FLinearColor GetPinColorFilterVtx() const;
	FLinearColor GetPinColorFilterEdge() const;
	FLinearColor GetPinColorVtxProperty() const;
	FLinearColor GetPinColorNeighborSampler() const;
	FLinearColor GetPinColorFillControl() const;
	FLinearColor GetPinColorHeuristics() const;
	FLinearColor GetPinColorProbe() const;
	FLinearColor GetPinColorClusterState() const;
	FLinearColor GetPinColorPicker() const;
	FLinearColor GetPinColorTexParam() const;
	FLinearColor GetPinColorShape() const;
	FLinearColor GetPinColorTensor() const;
	FLinearColor GetPinColorSortRule() const;
	FLinearColor GetPinColorPartitionRule() const;
	FLinearColor GetPinColorVtx() const;
	FLinearColor GetPinColorEdges() const;
	
#pragma endregion 
	
};
