// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/PCGExValencyEditorSettings.h"

UPCGExValencyEditorSettings::UPCGExValencyEditorSettings()
{
	// Default values are set in the header via UPROPERTY initializers
}

const UPCGExValencyEditorSettings* UPCGExValencyEditorSettings::Get()
{
	return GetDefault<UPCGExValencyEditorSettings>();
}
