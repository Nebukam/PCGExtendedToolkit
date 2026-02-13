// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

class UPCGExValencyCageSocketComponent;

/**
 * Hit proxy for socket component visualizer.
 * Inherits HComponentVisProxy so the standard component selection pipeline works.
 */
struct HPCGExSocketHitProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HPCGExSocketHitProxy(const UActorComponent* InComponent)
		: HComponentVisProxy(InComponent, HPP_Wireframe)
	{
	}
};

/**
 * Component visualizer for UPCGExValencyCageSocketComponent.
 * Draws a diamond shape at the socket's world position with a direction arrow.
 * Enables click-to-select and transform gizmo interaction.
 */
class PCGEXELEMENTSVALENCYEDITOR_API FPCGExValencyCageSocketVisualizer : public FComponentVisualizer
{
public:
	//~ Begin FComponentVisualizer Interface
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	//~ End FComponentVisualizer Interface

private:
	/** Currently selected socket component (for gizmo placement) */
	TWeakObjectPtr<UPCGExValencyCageSocketComponent> SelectedSocket;

	/** Draw a diamond/rhombus shape at a world position */
	static void DrawDiamond(FPrimitiveDrawInterface* PDI, const FVector& Center, float Size, const FLinearColor& Color, float Thickness);
};
