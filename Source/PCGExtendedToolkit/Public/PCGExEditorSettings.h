// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PCGExEditorSettings.generated.h"

UCLASS(config = Editor, defaultconfig)
class PCGEXTENDEDTOOLKIT_API UPCGExEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorDebug = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorMisc = FLinearColor(0.958333, 0.961800, 1.000000, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorMiscWrite = FLinearColor(1.000000, 0.316174, 0.000000, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorMiscAdd = FLinearColor(0.000000, 1.000000, 0.298310, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorMiscRemove = FLinearColor(0.05, 0.01, 0.01, 1.000000);


	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorSampler = FLinearColor(1.000000, 0.000000, 0.147106, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorSamplerNeighbor = FLinearColor(0.447917, 0.000000, 0.065891, 1.000000);


	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorGraphGen = FLinearColor(0.000000, 0.318537, 1.000000, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorGraph = FLinearColor(0.000000, 0.615363, 1.000000, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorSocket = FLinearColor(0.171875, 0.681472, 1.000000, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorSocketState = FLinearColor(0.000000, 0.249991, 0.406250, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorPathfinding = FLinearColor(0.000000, 1.000000, 0.670588, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorHeuristics = FLinearColor(0.243896, 0.578125, 0.371500, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorHeuristicsAtt = FLinearColor(0.497929, 0.515625, 0.246587, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorClusterFilter = FLinearColor(0.351486, 0.744792, 0.647392, 1.000000);


	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorEdge = FLinearColor(0.000000, 0.670117, 0.760417, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorClusterState = FLinearColor(0.000000, 0.249991, 0.406250, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorPath = FLinearColor(0.000000, 0.239583, 0.160662, 1.000000);


	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorFilterHub = FLinearColor(0.226841, 1.000000, 0.000000, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorFilter = FLinearColor(0.312910, 0.744792, 0.186198, 1.000000);


	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorPrimitives = FLinearColor(35.0f / 255.0f, 253.0f / 255.0f, 113.0f / 255.0f, 1.0f);
};
