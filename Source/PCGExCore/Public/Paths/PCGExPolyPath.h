// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExVersion.h"
#include "PCGExPath.h"
#include "Math/PCGExWinding.h"

namespace PCGExMT
{
	struct FScope;
}

struct FPCGExContext;
class UPCGPolygon2DData;
struct FPCGSplineStruct;
class UPCGSplineData;

namespace PCGExData
{
	class FPointIO;
	struct FElement;
	class FFacadePreloader;

	template <typename T>
	class TBuffer;
}

namespace PCGExPaths
{
	class PCGEXCORE_API FPolyPath : public FPath
	{
		TSharedPtr<FPCGSplineStruct> LocalSpline;
		TArray<FTransform> LocalTransforms;

		const FPCGSplineStruct* Spline = nullptr;

	public:
		FPolyPath(
			const TSharedPtr<PCGExData::FPointIO>& InPointIO,
			const FPCGExGeo2DProjectionDetails& InProjection,
			const double Expansion = 0, const double ExpansionZ = -1,
			const EPCGExWindingMutation WindingMutation = EPCGExWindingMutation::Unchanged);

		FPolyPath(
			const TSharedPtr<PCGExData::FFacade>& InPathFacade,
			const FPCGExGeo2DProjectionDetails& InProjection,
			const double Expansion = 0, const double ExpansionZ = -1,
			const EPCGExWindingMutation WindingMutation = EPCGExWindingMutation::Unchanged);

		FPolyPath(
			const UPCGSplineData* SplineData, const double Fidelity,
			const FPCGExGeo2DProjectionDetails& InProjection,
			const double Expansion = 0, const double ExpansionZ = -1,
			const EPCGExWindingMutation WindingMutation = EPCGExWindingMutation::Unchanged);

#if PCGEX_ENGINE_VERSION > 506
		FPolyPath(
			const UPCGPolygon2DData* PolygonData, const FPCGExGeo2DProjectionDetails& InProjection,
			const double Expansion = 0, const double ExpansionZ = -1,
			const EPCGExWindingMutation WindingMutation = EPCGExWindingMutation::Unchanged);
#endif

		FORCEINLINE const FPCGSplineStruct* GetSpline() const { return Spline; }

	protected:
		void InitFromTransforms(const EPCGExWindingMutation WindingMutation = EPCGExWindingMutation::Unchanged);

	public:
		virtual FTransform GetClosestTransform(const FVector& WorldPosition, int32& OutEdgeIndex, float& OutLerp, const bool bUseScale = false) const override;
		virtual FTransform GetClosestTransform(const FVector& WorldPosition, float& OutAlpha, const bool bUseScale = false) const override;
		virtual FTransform GetClosestTransform(const FVector& WorldPosition, bool& bIsInside, const bool bUseScale = false) const override;
		virtual FTransform GetClosestTransform(const FVector& WorldPosition, const bool bUseScale = false) const override;

		virtual bool GetClosestPosition(const FVector& WorldPosition, FVector& OutPosition) const override;
		virtual bool GetClosestPosition(const FVector& WorldPosition, FVector& OutPosition, bool& bIsInside) const override;

		virtual int32 GetClosestEdge(const FVector& WorldPosition, float& OutLerp) const override;
		virtual int32 GetClosestEdge(const double InTime, float& OutLerp) const override;

		void GetEdgeElements(const int32 EdgeIndex, PCGExData::FElement& OutEdge, PCGExData::FElement& OutEdgeStart, PCGExData::FElement& OutEdgeEnd) const;
	};
}
