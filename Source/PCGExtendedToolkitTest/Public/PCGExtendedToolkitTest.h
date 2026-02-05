// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * PCGExtendedToolkitTest Module
 *
 * Provides automated testing infrastructure for PCGExtendedToolkit.
 *
 * Test Categories:
 * - PCGEx.Unit.*       - Pure function tests (math, containers)
 * - PCGEx.Integration.* - Component lifecycle tests (filters, elements)
 * - PCGEx.Functional.* - Full graph execution tests
 *
 * Running Tests:
 * - In Editor: Tools > Session Frontend > Automation, filter by "PCGEx"
 * - Command Line: UnrealEditor-Cmd.exe Project.uproject -ExecCmds="Automation RunTests PCGEx" -NullRHI
 */
class FPCGExtendedToolkitTestModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
