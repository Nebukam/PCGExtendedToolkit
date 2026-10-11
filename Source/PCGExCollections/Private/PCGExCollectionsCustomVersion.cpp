// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExCollectionsCustomVersion.h"

#include "Serialization/CustomVersion.h"

const FGuid FPCGExCollectionsCustomVersion::GUID(0x7B2E4C91, 0xA6D34F58, 0x9C01B7E3, 0x5D8F2A46);

namespace PCGExCollectionsCustomVersion
{
	// Global registration object: announces the version to the engine's custom-version registry
	// at module load so packages can record and query it.
	FCustomVersionRegistration GRegisterPCGExCollectionsCustomVersion(
		FPCGExCollectionsCustomVersion::GUID,
		FPCGExCollectionsCustomVersion::LatestVersion,
		TEXT("PCGExCollections"));
}
