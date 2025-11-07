// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathSolidify.h"

#include "Data/PCGExPointIO.h"
#include "Details/PCGExDetailsSettings.h"
#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExPathSolidifyElement"
#define PCGEX_NAMESPACE PathSolidify

#if WITH_EDITOR

void UPCGExPathSolidifySettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	Super::ApplyDeprecation(InOutNode);
	if (PCGExVersion < 1) // First path refactor
	{
#define PCGEX_COPY_TO(_SOURCE, _TARGET)\
		_TARGET##Axis.Radius = Radius##_SOURCE##Constant;\
		_TARGET##Axis.RadiusAttribute = Radius##_SOURCE##SourceAttribute;\
		_TARGET##Axis.RadiusInput = bWriteRadius##_SOURCE ? Radius##_SOURCE##Input == EPCGExInputValueType::Constant ? EPCGExInputValueToggle::Constant : EPCGExInputValueToggle::Attribute :  EPCGExInputValueToggle::Disabled;

		if (SolidificationAxis == EPCGExMinimalAxis::X)
		{
			SolidificationOrder = EPCGExAxisOrder::XZY;
			PCGEX_COPY_TO(Z, Normal)
			PCGEX_COPY_TO(Y, Cross)
		}
		else if (SolidificationAxis == EPCGExMinimalAxis::Y)
		{
			SolidificationOrder = EPCGExAxisOrder::YZX;
			PCGEX_COPY_TO(Z, Normal)
			PCGEX_COPY_TO(X, Cross)
		}
		else
		{
			SolidificationOrder = EPCGExAxisOrder::ZXY;
			PCGEX_COPY_TO(X, Normal)
			PCGEX_COPY_TO(Y, Cross)
		}

		PCGExVersion = 1;
	}

#undef PCGEX_COPY_TO
}

#endif

PCGEX_INITIALIZE_ELEMENT(PathSolidify)

PCGExData::EIOInit UPCGExPathSolidifySettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(PathSolidify)

PCGEX_SETTING_VALUE_IMPL(FPCGExPathSolidificationAxisDetails, Flip, bool, FlipInput, FlipAttributeName, bFlip)
PCGEX_SETTING_VALUE_IMPL_BOOL(FPCGExPathSolidificationRadiusDetails, Radius, double, RadiusInput == EPCGExInputValueToggle::Attribute, RadiusAttribute, Radius)

PCGEX_SETTING_VALUE_IMPL(UPCGExPathSolidifySettings, SolidificationLerp, double, SolidificationLerpInput, SolidificationLerpAttribute, SolidificationLerpConstant)

bool FPCGExPathSolidificationAxisDetails::Validate(FPCGExContext* InContext) const
{
	if (FlipInput == EPCGExInputValueType::Attribute) { PCGEX_VALIDATE_NAME_C(InContext, FlipAttributeName); }
	return true;
}

bool FPCGExPathSolidificationRadiusDetails::Validate(FPCGExContext* InContext) const
{
	if (!FPCGExPathSolidificationAxisDetails::Validate(InContext)) { return false; }
	return true;
}

bool FPCGExPathSolidifyElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathSolidify)

	return true;
}

bool FPCGExPathSolidifyElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathSolidifyElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathSolidify)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some input have less than 2 points and will be ignored."))
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				PCGEX_SKIP_INVALID_PATH_ENTRY
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any valid path."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExPathSolidify
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathSolidify::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;
		bClosedLoop = PCGExPaths::GetClosedLoop(PointIO->GetIn());

		Path = MakeShared<PCGExPaths::FPath>(PointDataFacade->GetIn(), 0);
		Path->IOIndex = PointDataFacade->Source->IOIndex;

		PathLength = Path->AddExtra<PCGExPaths::FPathEdgeLength>();

		const FVector Up = FVector::UpVector;

		if (Settings->CrossDirectionType == EPCGExInputValueType::Attribute)
		{
			CrossGetter = PointDataFacade->GetBroadcaster<FVector>(Settings->CrossDirectionAttribute, true);
			if (!CrossGetter)
			{
				PCGEX_LOG_INVALID_SELECTOR_C(ExecutionContext, Cross Direction, Settings->CrossDirectionAttribute)
				return false;
			}
		}
		else
		{
			switch (Settings->CrossDirection)
			{
			case EPCGExPathNormalDirection::Normal:
				PathNormal = StaticCastSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>>(Path->AddExtra<PCGExPaths::FPathEdgeNormal>(false, Up));
				break;
			case EPCGExPathNormalDirection::Binormal:
				PathNormal = StaticCastSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>>(Path->AddExtra<PCGExPaths::FPathEdgeBinormal>(false, Up));
				break;
			case EPCGExPathNormalDirection::AverageNormal:
				PathNormal = StaticCastSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>>(Path->AddExtra<PCGExPaths::FPathEdgeAvgNormal>(false, Up));
				break;
			}
		}

		if (!bClosedLoop && Settings->bRemoveLastPoint) { PointDataFacade->GetOut()->SetNumPoints(Path->LastIndex); }

		PointDataFacade->GetOut()->AllocateProperties(
			EPCGPointNativeProperties::Transform |
			EPCGPointNativeProperties::BoundsMin |
			EPCGPointNativeProperties::BoundsMax);

#define PCGEX_CREATE_LOCAL_AXIS_SET_CONST(_AXIS) if (Settings->bWriteRadius##_AXIS){\
		SolidificationRad##_AXIS = PCGExDetails::MakeSettingValue(Settings->Radius##_AXIS##Input, Settings->Radius##_AXIS##SourceAttribute, Settings->Radius##_AXIS##Constant);\
		if (!SolidificationRad##_AXIS->Init(PointDataFacade, false)){ return false; }}
		PCGEX_FOREACH_XYZ(PCGEX_CREATE_LOCAL_AXIS_SET_CONST)
#undef PCGEX_CREATE_LOCAL_AXIS_SET_CONST

		SolidificationLerp = Settings->GetValueSettingSolidificationLerp();
		if (!SolidificationLerp->Init(PointDataFacade, false)) { return false; }

		Path->ComputeAllEdgeExtra();

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::PathSolidify::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		TPCGValueRange<FTransform> Transforms = PointDataFacade->GetOut()->GetTransformValueRange(false);
		TPCGValueRange<FVector> BoundsMin = PointDataFacade->GetOut()->GetBoundsMinValueRange(false);
		TPCGValueRange<FVector> BoundsMax = PointDataFacade->GetOut()->GetBoundsMaxValueRange(false);

		int32 X = 0;
		int32 Y = 0;
		int32 Z = 0;

		FVector OutAxis[3] = {FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector};

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!Path->IsValidEdgeIndex(Index)) { continue; }

			const PCGExPaths::FPathEdge& Edge = Path->Edges[Index];
			const double Length = PathLength->Get(Index);

			FVector& OutBoundsMin = BoundsMin[Index];
			FVector& OutBoundsMax = BoundsMax[Index];

			PCGEx::GetAxisOrder(Settings->SolidificationOrder, X, Y, Z);

			OutAxis[X] = Edge.Dir;
			OutAxis[Y] = PathNormal ? PathNormal->Get(Index) : CrossGetter->Read(Index).GetSafeNormal();
			OutAxis[Z] = FVector::CrossProduct(OutAxis[X], OutAxis[Y]).GetSafeNormal();

			double EdgeLerp = FMath::Clamp(SolidificationLerp->Read(Index), 0, 1);

			// Handle flipping
			
			if (ForwardFlipBuffer && ForwardFlipBuffer->Read(Index))
			{
				EdgeLerp = 1 - EdgeLerp;
				OutAxis[X] *= -1;
			}

			if (CrossFlipBuffer && CrossFlipBuffer->Read(Index)) { OutAxis[Y] *= -1; }
			if (NormalFlipBuffer && NormalFlipBuffer->Read(Index)) { OutAxis[Z] *= -1; }

			const double EdgeLerpInv = 1 - EdgeLerp;
			const FVector InvScale = FVector::One() / Transforms[Index].GetScale3D();

			// Write bounds
			
			OutBoundsMin[X] = (-Length * EdgeLerp) * InvScale[X];
			OutBoundsMax[X] = (Length * EdgeLerpInv) * InvScale[X];

			if (CrossRadiusBuffer)
			{
				const double Rad = CrossRadiusBuffer->Read(Index);
				OutBoundsMin[Y] = (-Rad) * InvScale[Y];
				OutBoundsMax[Y] = (Rad) * InvScale[Y];
			}

			if (NormalRadiusBuffer)
			{
				const double Rad = NormalRadiusBuffer->Read(Index);
				OutBoundsMin[Z] = (-Rad) * InvScale[Z];
				OutBoundsMax[Z] = (Rad) * InvScale[Z];
			}

			Transforms[Index] = FTransform(
				FMatrix(OutAxis[X], OutAxis[Y], OutAxis[Z], FVector::ZeroVector).ToQuat(),
				Path->GetEdgePositionAtAlpha(Index, EdgeLerp),
				Transforms[Index].GetScale3D());
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
