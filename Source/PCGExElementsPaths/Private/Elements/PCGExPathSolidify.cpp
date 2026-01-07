// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExPathSolidify.h"


#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"
#include "PCGExVersion.h"
#include "Paths/PCGExPathsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExPathSolidifyElement"
#define PCGEX_NAMESPACE PathSolidify

UPCGExPathSolidifySettings::UPCGExPathSolidifySettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RotationMapping.Add(EPCGExAxisOrder::XYZ, EPCGExMakeRotAxis::X);
	RotationMapping.Add(EPCGExAxisOrder::YZX, EPCGExMakeRotAxis::Z);
	RotationMapping.Add(EPCGExAxisOrder::ZXY, EPCGExMakeRotAxis::X);
	RotationMapping.Add(EPCGExAxisOrder::YXZ, EPCGExMakeRotAxis::XY);
	RotationMapping.Add(EPCGExAxisOrder::ZYX, EPCGExMakeRotAxis::XY);
	RotationMapping.Add(EPCGExAxisOrder::XZY, EPCGExMakeRotAxis::X);
}

#if WITH_EDITOR

void UPCGExPathSolidifySettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	PCGEX_UPDATE_TO_DATA_VERSION(1, 70, 11)
	{
#define PCGEX_COPY_TO(_SOURCE, _TARGET)\
		_TARGET##Axis.Radius = Radius##_SOURCE##Constant_DEPRECATED;\
		_TARGET##Axis.RadiusAttribute = Radius##_SOURCE##SourceAttribute_DEPRECATED;\
		if(bWriteRadius##_SOURCE##_DEPRECATED){ _TARGET##Axis.RadiusInput = static_cast<EPCGExInputValueToggle>(Radius##_SOURCE##Input##_DEPRECATED); }\
		else{_TARGET##Axis.RadiusInput = EPCGExInputValueToggle::Disabled;}

		if (SolidificationAxis_DEPRECATED == EPCGExMinimalAxis::X)
		{
			SolidificationOrder = EPCGExAxisOrder::XYZ;
			PCGEX_COPY_TO(Z, Secondary)
			PCGEX_COPY_TO(Y, Tertiary)
		}
		else if (SolidificationAxis_DEPRECATED == EPCGExMinimalAxis::Y)
		{
			SolidificationOrder = EPCGExAxisOrder::YZX;
			PCGEX_COPY_TO(Z, Secondary)
			PCGEX_COPY_TO(X, Tertiary)
		}
		else
		{
			SolidificationOrder = EPCGExAxisOrder::ZXY;
			PCGEX_COPY_TO(X, Secondary)
			PCGEX_COPY_TO(Y, Tertiary)
		}
#undef PCGEX_COPY_TO
	}

	Super::ApplyDeprecation(InOutNode);
}

#endif

PCGEX_INITIALIZE_ELEMENT(PathSolidify)

PCGExData::EIOInit UPCGExPathSolidifySettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(PathSolidify)

PCGEX_SETTING_VALUE_IMPL_TOGGLE(FPCGExPathSolidificationAxisDetails, Flip, bool, FlipInput, FlipAttributeName, bFlip, false)
PCGEX_SETTING_VALUE_IMPL_BOOL(FPCGExPathSolidificationRadiusDetails, Radius, double, RadiusInput == EPCGExInputValueToggle::Attribute, RadiusAttribute, Radius)

PCGEX_SETTING_VALUE_IMPL(UPCGExPathSolidifySettings, SolidificationLerp, double, SolidificationLerpInput, SolidificationLerpAttribute, SolidificationLerpConstant)

bool FPCGExPathSolidifyElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathSolidify)

	Context->RotationConstructionsMap.Reserve(6);
	for (int i = 0; i < 6; i++)
	{
		Context->RotationConstructionsMap.Add(Settings->RotationMapping[static_cast<EPCGExAxisOrder>(i)]);
	}

	return true;
}

bool FPCGExPathSolidifyElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathSolidifyElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathSolidify)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some input have less than 2 points and will be ignored."))
		if (!Context->StartBatchProcessingPoints([&](const TSharedPtr<PCGExData::FPointIO>& Entry)
		                                         {
			                                         PCGEX_SKIP_INVALID_PATH_ENTRY
			                                         return true;
		                                         }, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
		                                         {
		                                         }))
		{
			return Context->CancelExecution(TEXT("Could not find any valid path."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExPathSolidify
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathSolidify::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;
		bClosedLoop = PCGExPaths::Helpers::GetClosedLoop(PointIO->GetIn());

		Path = MakeShared<PCGExPaths::FPath>(PointDataFacade->GetIn(), 0);
		Path->IOIndex = PointDataFacade->Source->IOIndex;

		PathLength = Path->AddExtra<PCGExPaths::FPathEdgeLength>();

		const FVector Up = PCGEX_CORE_SETTINGS.WorldUp;

		if (Settings->NormalType == EPCGExInputValueType::Attribute)
		{
			NormalGetter = PointDataFacade->GetBroadcaster<FVector>(Settings->NormalAttribute, true);
			if (!NormalGetter)
			{
				PCGEX_LOG_INVALID_SELECTOR_C(ExecutionContext, Cross Direction, Settings->NormalAttribute)
				return false;
			}
		}
		else
		{
			switch (Settings->Normal)
			{
			case EPCGExPathNormalDirection::Normal: PathNormal = StaticCastSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>>(Path->AddExtra<PCGExPaths::FPathEdgeNormal>(false, Up));
				break;
			case EPCGExPathNormalDirection::Binormal: PathNormal = StaticCastSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>>(Path->AddExtra<PCGExPaths::FPathEdgeBinormal>(false, Up));
				break;
			case EPCGExPathNormalDirection::AverageNormal: PathNormal = StaticCastSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>>(Path->AddExtra<PCGExPaths::FPathEdgeAvgNormal>(false, Up));
				break;
			}
		}

		if (!bClosedLoop && Settings->bRemoveLastPoint) { PointDataFacade->GetOut()->SetNumPoints(Path->LastIndex); }

		// Axis order overrides

		if (Settings->bReadOrderFromAttribute)
		{
			AxisOrder = PointDataFacade->GetBroadcaster<int32>(Settings->OrderAttribute, true);
			if (!AxisOrder) { PCGEX_LOG_INVALID_ATTR_C(ExecutionContext, Axis Order, Settings->OrderAttribute) }
		}

		// Axis construction overrides
		if (Settings->bReadConstructionFromAttribute)
		{
			RotationConstruction = PointDataFacade->GetBroadcaster<int32>(Settings->ConstructionAttribute, true);
			if (!AxisOrder) { PCGEX_LOG_INVALID_ATTR_C(ExecutionContext, Rotation Construction, Settings->ConstructionAttribute) }
		}

		// Flip settings

		PrimaryFlip = Settings->PrimaryAxis.GetValueSettingFlip();
		if (!PrimaryFlip->Init(PointDataFacade)) { return false; }

		SecondaryFlip = Settings->SecondaryAxis.GetValueSettingFlip();
		if (!SecondaryFlip->Init(PointDataFacade)) { return false; }

		TertiaryFlip = Settings->TertiaryAxis.GetValueSettingFlip();
		if (!TertiaryFlip->Init(PointDataFacade)) { return false; }

		// Radius settings

		if (Settings->TertiaryAxis.RadiusInput != EPCGExInputValueToggle::Disabled)
		{
			TertiaryRadius = Settings->TertiaryAxis.GetValueSettingRadius();
			if (!TertiaryRadius->Init(PointDataFacade)) { return false; }
		}

		if (Settings->SecondaryAxis.RadiusInput != EPCGExInputValueToggle::Disabled)
		{
			SecondaryRadius = Settings->SecondaryAxis.GetValueSettingRadius();
			if (!SecondaryRadius->Init(PointDataFacade)) { return false; }
		}

		PointDataFacade->GetOut()->AllocateProperties(EPCGPointNativeProperties::Transform | EPCGPointNativeProperties::BoundsMin | EPCGPointNativeProperties::BoundsMax);

		SolidificationLerp = Settings->GetValueSettingSolidificationLerp();
		if (!SolidificationLerp->Init(PointDataFacade, false)) { return false; }

		Path->ComputeAllEdgeExtra();

		StartParallelLoopForPoints();

		return true;
	}

	EPCGExAxisOrder FProcessor::GetOrder(const int32 Index) const
	{
		if (!AxisOrder) { return Settings->SolidificationOrder; }
		int32 CustomValue = PCGExMath::SanitizeIndex(AxisOrder->Read(Index), 5);
		if (CustomValue == -1) { return Settings->SolidificationOrder; }
		return static_cast<EPCGExAxisOrder>(CustomValue);
	}

	EPCGExMakeRotAxis FProcessor::GetConstruction(const EPCGExAxisOrder Order, const int32 Index) const
	{
		if (!RotationConstruction)
		{
			if (Settings->bUseConstructionMapping) { return Context->RotationConstructionsMap[static_cast<int32>(Order)]; }
			return Settings->RotationConstruction;
		}
		int32 CustomValue = PCGExMath::SanitizeIndex(AxisOrder->Read(Index), 5);
		if (CustomValue == -1)
		{
			if (Settings->bUseConstructionMapping) { return Context->RotationConstructionsMap[static_cast<int32>(Order)]; }
			return Settings->RotationConstruction;
		}
		return static_cast<EPCGExMakeRotAxis>(CustomValue);
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::PathSolidify::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		TPCGValueRange<FTransform> Transforms = PointDataFacade->GetOut()->GetTransformValueRange(false);
		TPCGValueRange<FVector> BoundsMin = PointDataFacade->GetOut()->GetBoundsMinValueRange(false);
		TPCGValueRange<FVector> BoundsMax = PointDataFacade->GetOut()->GetBoundsMaxValueRange(false);

		int32 A = 0;
		int32 B = 0;
		int32 C = 0;

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!Path->IsValidEdgeIndex(Index))
			{
				continue;
			}

			const PCGExPaths::FPathEdge& Edge = Path->Edges[Index];
			const double Length = PathLength->Get(Index);
			const FVector Scale = Transforms[Index].GetScale3D();
			const FVector InvScale = FVector(1 / Scale.X, 1 / Scale.Y, 1 / Scale.Z);

			const FVector Normal = PathNormal ? PathNormal->Get(Index) : NormalGetter->Read(Index).GetSafeNormal();
			const FVector RealXAxis = Edge.Dir;
			const FVector RealYAxis = FVector::CrossProduct(RealXAxis, Normal);
			const FVector RealZAxis = FVector::CrossProduct(RealYAxis, RealXAxis);

			const FVector Flip(PrimaryFlip->Read(Index) ? -1.0f : 1.0f, SecondaryFlip->Read(Index) ? -1.0f : 1.0f, TertiaryFlip->Read(Index) ? -1.0f : 1.0f);

			FVector XAxis = RealXAxis * Flip.X;
			FVector YAxis = RealYAxis * Flip.Y;
			FVector ZAxis = RealZAxis * Flip.Z;

			const EPCGExAxisOrder Order = GetOrder(Index);

			PCGExMath::ReorderAxes(Order, XAxis, YAxis, ZAxis);
			const FQuat Quat = PCGExMath::MakeRot(GetConstruction(Order, Index), XAxis, YAxis, ZAxis);

			// Find primary / secondary / tertiary components
			PCGExMath::FindOrderMatch(Quat, RealXAxis, RealYAxis, RealZAxis, A, B, C);

			const FVector QuatAxes[3] = {Quat.GetAxisX(), Quat.GetAxisY(), Quat.GetAxisZ()};
			const bool bForwardFlipped = FVector::DotProduct(QuatAxes[A], RealXAxis) < 0;
			double EdgeLerp = FMath::Clamp(SolidificationLerp->Read(Index), 0.0, 1.0);
			//if (bForwardFlipped) { EdgeLerp = 1.0 - EdgeLerp; }

			// update transform
			const FVector Position = Path->GetEdgePositionAtAlpha(Index, bForwardFlipped ? 1.0 - EdgeLerp : EdgeLerp);
			Transforms[Index] = FTransform(Quat, Position, Scale);

			// bounds per axis index
			FVector& OutBoundsMin = BoundsMin[Index];
			FVector& OutBoundsMax = BoundsMax[Index];

			// Yey
			const double EdgeLerpInv = 1.0 - EdgeLerp;

			OutBoundsMin[A] = (-Length * EdgeLerp) * InvScale[A];
			OutBoundsMax[A] = (Length * EdgeLerpInv) * InvScale[A];

			if (SecondaryRadius)
			{
				const double Rad = FMath::Abs(SecondaryRadius->Read(Index));
				OutBoundsMin[B] = -Rad * InvScale[B];
				OutBoundsMax[B] = Rad * InvScale[B];
			}

			if (TertiaryRadius)
			{
				const double Rad = FMath::Abs(TertiaryRadius->Read(Index));
				OutBoundsMin[C] = -Rad * InvScale[C];
				OutBoundsMax[C] = Rad * InvScale[C];
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
