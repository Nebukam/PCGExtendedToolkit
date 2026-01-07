// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExMT.h"

namespace PCGExData
{
	class FPointIO;
}

struct FPCGExTransformDetails;

namespace PCGExFitting::Tasks
{
	class PCGEXCORE_API FTransformPointIO final : public PCGExMT::FPCGExIndexedTask
	{
	public:
		FTransformPointIO(const int32 InTaskIndex, const TSharedPtr<PCGExData::FPointIO>& InPointIO, const TSharedPtr<PCGExData::FPointIO>& InToBeTransformedIO, FPCGExTransformDetails* InTransformDetails, bool bAllocate = false);

		TSharedPtr<PCGExData::FPointIO> PointIO;
		TSharedPtr<PCGExData::FPointIO> ToBeTransformedIO;
		FPCGExTransformDetails* TransformDetails = nullptr;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager) override;
	};
}
