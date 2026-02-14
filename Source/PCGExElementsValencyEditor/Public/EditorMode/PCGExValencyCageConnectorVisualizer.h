// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

class UPCGExValencyCageConnectorComponent;

/**
 * Hit proxy for connector component visualizer.
 */
struct HPCGExConnectorHitProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HPCGExConnectorHitProxy(const UActorComponent* InComponent)
		: HComponentVisProxy(InComponent, HPP_Wireframe)
	{
	}
};

/**
 * Component visualizer for UPCGExValencyCageConnectorComponent.
 * Draws a diamond shape at the connector's world position with polarity-aware arrows.
 */
class PCGEXELEMENTSVALENCYEDITOR_API FPCGExValencyCageConnectorVisualizer : public FComponentVisualizer
{
public:
	//~ Begin FComponentVisualizer Interface
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual void EndEditing() override;
	//~ End FComponentVisualizer Interface

private:
	static void DrawDiamond(FPrimitiveDrawInterface* PDI, const FVector& Center, float Size, const FLinearColor& Color, float Thickness);
};
