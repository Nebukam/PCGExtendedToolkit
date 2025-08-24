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

	void RegisterMenuExtensions();
	void UnregisterMenuExtensions();
};
