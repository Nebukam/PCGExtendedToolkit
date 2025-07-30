// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExPolyPathFilterFactory.h"

#include "Cooker/CookDependency.h"
#include "Data/PCGSplineData.h"


#define LOCTEXT_NAMESPACE "PCGExPathInclusionFilterDefinition"
#define PCGEX_NAMESPACE PCGExPathInclusionFilterDefinition

bool UPCGExPolyPathFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }
	return true;
}

bool UPCGExPolyPathFilterFactory::WantsPreparation(FPCGExContext* InContext)
{
	return true;
}

PCGExFactories::EPreparationResult UPCGExPolyPathFilterFactory::Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	PCGExFactories::EPreparationResult Result = Super::Prepare(InContext, AsyncManager);
	if (Result != PCGExFactories::EPreparationResult::Success) { return Result; }

	TempTargets = InContext->InputData.GetInputsByPin(GetInputLabel());

	if (TempTargets.IsEmpty())
	{
		if (MissingDataHandling == EPCGExFilterNoDataFallback::Error) { if (!bQuietMissingInputError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No splines (no input matches criteria or empty dataset)")); } }
		return PCGExFactories::EPreparationResult::MissingData;
	}

	TempPolyPaths.Init(nullptr, TempTargets.Num());
	PolyPaths.Reserve(TempTargets.Num());

	TWeakPtr<FPCGContextHandle> CtxHandle = InContext->GetOrCreateHandle();

	InitConfig_Internal();

	PCGEX_ASYNC_GROUP_CHKD_CUSTOM(AsyncManager, CreatePolyPaths, PCGExFactories::EPreparationResult::Fail)

	CreatePolyPaths->OnCompleteCallback =
		[CtxHandle, this]()
		{
			PCGEX_SHARED_CONTEXT_VOID(CtxHandle)

			FBox OctreeBounds = FBox(ForceInit);

			TArray<FBox> BoundsList;
			BoundsList.Reserve(TempTargets.Num());

			for (int i = 0; i < TempTargets.Num(); i++)
			{
				const TSharedPtr<PCGExPaths::FPath>& Path = TempPolyPaths[i];

				if (!Path || !Path.IsValid()) { continue; }

				const UPCGSpatialData* Data = Cast<UPCGSpatialData>(TempTargets[i].Data);
				FBox DataBounds = Data->GetBounds().ExpandBy(LocalExpansion * 2);
				if (bScaleTolerance) { DataBounds = DataBounds.ExpandBy(DataBounds.GetSize().Length() * 10); }
				BoundsList.Add(DataBounds);
				OctreeBounds += Data->GetBounds();

				PolyPaths.Add(Path);
			}

			if (PolyPaths.IsEmpty())
			{
				PrepResult = PCGExFactories::EPreparationResult::MissingData;
				if (!bQuietMissingInputError) { PCGE_LOG_C(Error, GraphAndLog, SharedContext.Get(), FTEXT("No splines (no input matches criteria or empty dataset)")); }
				return;
			}

			TempPolyPaths.Empty();

			Octree = MakeShared<PCGExOctree::FItemOctree>(OctreeBounds.GetCenter(), OctreeBounds.GetExtent().Length());
			for (int i = 0; i < BoundsList.Num(); i++) { Octree->AddElement(PCGExOctree::FItem(i, BoundsList[i])); }
		};

	CreatePolyPaths->OnIterationCallback =
		[CtxHandle, this](const int32 Index, const PCGExMT::FScope& Scope)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExPolyPathFilterFactory::CreatePolyPath);

			PCGEX_SHARED_CONTEXT_VOID(CtxHandle)

			const UPCGData* Data = TempTargets[Index].Data;
			if (!Data) { return; }

			const bool bIsClosedLoop = PCGExPaths::GetClosedLoop(Data);
			if (LocalSampleInputs == EPCGExSplineSamplingIncludeMode::ClosedLoopOnly && !bIsClosedLoop) { return; }
			if (LocalSampleInputs == EPCGExSplineSamplingIncludeMode::OpenSplineOnly && bIsClosedLoop) { return; }

			TSharedPtr<PCGExPaths::FPolyPath> Path = nullptr;

			if (const UPCGBasePointData* PointData = Cast<UPCGBasePointData>(Data))
			{
				const TSharedPtr<PCGExData::FPointIO> PointIO = MakeShared<PCGExData::FPointIO>(CtxHandle, PointData);
				Path = MakeShared<PCGExPaths::FPolyPath>(PointIO, LocalProjection, LocalExpansion, LocalExpansionZ, WindingMutation);
			}
			else if (const UPCGSplineData* SplineData = Cast<UPCGSplineData>(Data))
			{
				Path = MakeShared<PCGExPaths::FPolyPath>(SplineData, LocalFidelity, LocalProjection, LocalExpansion, LocalExpansionZ, WindingMutation);
			}

			if (Path) { TempPolyPaths[Index] = Path; }
		};

	CreatePolyPaths->StartIterations(TempTargets.Num(), 1);
	return Result;
}

TSharedPtr<PCGExPathInclusion::FHandler> UPCGExPolyPathFilterFactory::CreateHandler() const
{
	TSharedPtr<PCGExPathInclusion::FHandler> Handler = MakeShared<PCGExPathInclusion::FHandler>(this);
	Handler->bScaleTolerance = bScaleTolerance;
	return Handler;
}

void UPCGExPolyPathFilterFactory::BeginDestroy()
{
	PolyPaths.Reset();
	Octree.Reset();
	Super::BeginDestroy();
}

namespace PCGExPathInclusion
{
	FHandler::FHandler(const UPCGExPolyPathFilterFactory* InFactory)
	{
		Paths = &InFactory->PolyPaths;
		Octree = InFactory->Octree;
		Tolerance = InFactory->LocalExpansion;
		ToleranceSquared = FMath::Square(InFactory->LocalExpansion);
	}

	void FHandler::Init(const EPCGExSplineCheckType InCheckType)
	{
		Check = InCheckType;

		switch (Check)
		{
		case EPCGExSplineCheckType::IsInside:
			GoodFlags = Inside;
			BadFlags = On;
			FlagScope = Any;
			bFastCheck = Tolerance <= 0;
			break;
		case EPCGExSplineCheckType::IsInsideOrOn:
			GoodFlags = static_cast<EFlags>(Inside | On);
			FlagScope = Any;
			break;
		case EPCGExSplineCheckType::IsInsideAndOn:
			GoodFlags = static_cast<EFlags>(Inside | On);
			FlagScope = All;
			break;
		case EPCGExSplineCheckType::IsOutside:
			GoodFlags = Outside;
			BadFlags = On;
			FlagScope = Any;
			bFastCheck = Tolerance <= 0;
			break;
		case EPCGExSplineCheckType::IsOutsideOrOn:
			GoodFlags = static_cast<EFlags>(Outside | On);
			FlagScope = Any;
			break;
		case EPCGExSplineCheckType::IsOutsideAndOn:
			GoodFlags = static_cast<EFlags>(Outside | On);
			FlagScope = All;
			break;
		case EPCGExSplineCheckType::IsOn:
			GoodFlags = On;
			FlagScope = Any;
			bDistanceCheckOnly = true;
			break;
		case EPCGExSplineCheckType::IsNotOn:
			BadFlags = On;
			FlagScope = Skip;
			bDistanceCheckOnly = true;
			break;
		}
	}

	EFlags FHandler::GetInclusionFlags(const FVector& WorldPosition, int32& InclusionCount, const bool bClosestOnly) const
	{
		EFlags OutFlags = None;
		bool bIsOn = false;

		// TODO : Optimize later, some checks are expensive and unnecessary depending on settings
		// Since poly paths have their own internal segment octree we can use that one much more efficiently than the spline
		// Lack of result mean it's not on
		// and compute distance against closest edge might require much less internal maths

		if (bFastCheck)
		{
			if (bClosestOnly)
			{
				Octree->FindElementsWithBoundsTest(
					FBoxCenterAndExtent(WorldPosition, FVector::OneVector), [&](
					const PCGExOctree::FItem& Item)
					{
						if ((*(Paths->GetData() + Item.Index))->IsInsideProjection(WorldPosition))
						{
							InclusionCount++;
							EnumAddFlags(OutFlags, Inside);
							EnumRemoveFlags(OutFlags, Outside);
						}
						else
						{
							EnumAddFlags(OutFlags, Outside);
							EnumRemoveFlags(OutFlags, Inside);
						}
					});
			}
			else
			{
				Octree->FindElementsWithBoundsTest(
					FBoxCenterAndExtent(WorldPosition, FVector::OneVector), [&](
					const PCGExOctree::FItem& Item)
					{
						if ((*(Paths->GetData() + Item.Index))->IsInsideProjection(WorldPosition))
						{
							InclusionCount++;
							EnumAddFlags(OutFlags, Inside);
						}
						else
						{
							EnumAddFlags(OutFlags, Outside);
						}
					});
			}
		}
		else
		{
			if (bClosestOnly)
			{
				double BestDist = MAX_dbl;

				if (Check == EPCGExSplineCheckType::IsOn)
				{
				}

				Octree->FindElementsWithBoundsTest(
					FBoxCenterAndExtent(WorldPosition, FVector::OneVector),
					[&](const PCGExOctree::FItem& Item)
					{
						bool bLocalIsInside = false;
						const FTransform Closest = (*(Paths->GetData() + Item.Index))->GetClosestTransform(WorldPosition, bLocalIsInside, bScaleTolerance);
						InclusionCount += bLocalIsInside;

						EnumAddFlags(OutFlags, bLocalIsInside ? Inside : Outside);

						if (const double Dist = FVector::DistSquared(WorldPosition, Closest.GetLocation()); Dist < BestDist)
						{
							BestDist = Dist;
							const double Tol = bScaleTolerance ? FMath::Square(Tolerance * (Closest.GetScale3D() * ToleranceScaleFactor).Length()) : ToleranceSquared;
							bIsOn = Dist < Tol;
						}
					});
			}
			else
			{
				Octree->FindElementsWithBoundsTest(
					FBoxCenterAndExtent(WorldPosition, FVector::OneVector), [&](
					const PCGExOctree::FItem& Item)
					{
						bool bLocalIsInside = false;
						const FTransform Closest = (*(Paths->GetData() + Item.Index))->GetClosestTransform(WorldPosition, bLocalIsInside, bScaleTolerance);
						InclusionCount += bLocalIsInside;

						EnumAddFlags(OutFlags, bLocalIsInside ? Inside : Outside);

						const double Tol = bScaleTolerance ? FMath::Square(Tolerance * (Closest.GetScale3D() * ToleranceScaleFactor).Length()) : ToleranceSquared;
						if (FVector::DistSquared(WorldPosition, Closest.GetLocation()) < Tol) { bIsOn = true; }
					});
			}
		}

		if (OutFlags == None) { OutFlags = Outside; }
		if (bIsOn) { EnumAddFlags(OutFlags, On); }

		return OutFlags;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
