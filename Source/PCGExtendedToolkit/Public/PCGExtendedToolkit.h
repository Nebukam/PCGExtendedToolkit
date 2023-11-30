// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBuilder.h"
#include "MeshDescriptionBuilder.h"
#include "Modules/ModuleManager.h"

class FPCGExtendedToolkitModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
