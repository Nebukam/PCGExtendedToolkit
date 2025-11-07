// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathSolidify.h"

#include "Data/PCGExPointIO.h"
#include "Details/PCGExDetailsSettings.h"
#include "Details/PCGExVersion.h"
#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExPathSolidifyElement"
#define PCGEX_NAMESPACE PathSolidify

#if WITH_EDITOR

void UPCGExPathSolidifySettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	PCGEX_IF_DATA_VERSION(1, 70, 11)
	{
#define PCGEX_COPY_TO(_SOURCE, _TARGET)\
		_TARGET##Axis.Radius = Radius##_SOURCE##Constant_DEPRECATED;\
		_TARGET##Axis.RadiusAttribute = Radius##_SOURCE##SourceAttribute_DEPRECATED;\
		_TARGET##Axis.RadiusInput = bWriteRadius##_SOURCE##_DEPRECATED ? Radius##_SOURCE##Input##_DEPRECATED == EPCGExInputValueType::Constant ? EPCGExInputValueToggle::Constant : EPCGExInputValueToggle::Attribute :  EPCGExInputValueToggle::Disabled;

		if (SolidificationAxis_DEPRECATED == EPCGExMinimalAxis::X)
		{
			SolidificationOrder = EPCGExAxisOrder::XYZ;
			PCGEX_COPY_TO(Z, Right)
			PCGEX_COPY_TO(Y, Up)
		}
		else if (SolidificationAxis_DEPRECATED == EPCGExMinimalAxis::Y)
		{
			SolidificationOrder = EPCGExAxisOrder::YZX;
			PCGEX_COPY_TO(Z, Right)
			PCGEX_COPY_TO(X, Up)
		}
		else
		{
			SolidificationOrder = EPCGExAxisOrder::ZXY;
			PCGEX_COPY_TO(X, Right)
			PCGEX_COPY_TO(Y, Up)
		}
#undef PCGEX_COPY_TO
	}

	PCGEX_UPDATE_DATA_VERSION
	Super::ApplyDeprecation(InOutNode);
}

#endif

PCGEX_INITIALIZE_ELEMENT(PathSolidify)

PCGExData::EIOInit UPCGExPathSolidifySettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(PathSolidify)

PCGEX_SETTING_VALUE_IMPL_TOGGLE(FPCGExPathSolidificationAxisDetails, Flip, bool, FlipInput, FlipAttributeName, bFlip, false)
PCGEX_SETTING_VALUE_IMPL_BOOL(FPCGExPathSolidificationRadiusDetails, Radius, double, RadiusInput == EPCGExInputValueToggle::Attribute, RadiusAttribute, Radius)
PCGEX_SETTING_VALUE_IMPL_BOOL(FPCGExPathSolidificationRadiusOnlyDetails, Radius, double, RadiusInput == EPCGExInputValueToggle::Attribute, RadiusAttribute, Radius)

PCGEX_SETTING_VALUE_IMPL(UPCGExPathSolidifySettings, SolidificationLerp, double, SolidificationLerpInput, SolidificationLerpAttribute, SolidificationLerpConstant)

bool FPCGExPathSolidificationAxisDetails::Validate(FPCGExContext* InContext) const
{
	if (FlipInput == EPCGExInputValueToggle::Attribute) { PCGEX_VALIDATE_NAME_C(InContext, FlipAttributeName); }
	return true;
}

bool FPCGExPathSolidificationRadiusDetails::Validate(FPCGExContext* InContext) const
{
	if (!FPCGExPathSolidificationAxisDetails::Validate(InContext)) { return false; }
	return true;
}


bool FPCGExPathSolidificationRadiusOnlyDetails::Validate(FPCGExContext* InContext) const
{
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

		const FVector Up = GetDefault<UPCGExGlobalSettings>()->WorldUp;

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

		// Flip settings

		ForwardFlipBuffer = Settings->ForwardAxis.GetValueSettingFlip();
		if (!ForwardFlipBuffer->Init(PointDataFacade)) { return false; }

		RightFlipBuffer = Settings->RightAxis.GetValueSettingFlip();
		if (!RightFlipBuffer->Init(PointDataFacade)) { return false; }

		// Radius settings

		if (Settings->UpAxis.RadiusInput != EPCGExInputValueToggle::Disabled)
		{
			UpRadiusBuffer = Settings->UpAxis.GetValueSettingRadius();
			if (!UpRadiusBuffer->Init(PointDataFacade)) { return false; }
		}

		if (Settings->RightAxis.RadiusInput != EPCGExInputValueToggle::Disabled)
		{
			RightRadiusBuffer = Settings->RightAxis.GetValueSettingRadius();
			if (!RightRadiusBuffer->Init(PointDataFacade)) { return false; }
		}

		PointDataFacade->GetOut()->AllocateProperties(
			EPCGPointNativeProperties::Transform |
			EPCGPointNativeProperties::BoundsMin |
			EPCGPointNativeProperties::BoundsMax);

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

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!Path->IsValidEdgeIndex(Index))
				continue;

			const PCGExPaths::FPathEdge& Edge = Path->Edges[Index];
			const double Length = PathLength->Get(Index);
			const FVector InvScale = FVector::One() / Transforms[Index].GetScale3D();

			// user-aligned directions			
			FVector Forward = Edge.Dir; // initial "toward" vector
			FVector Right = FVector::CrossProduct(Forward, PathNormal ? PathNormal->Get(Index) : CrossGetter->Read(Index)).GetSafeNormal();
			FVector Up = FVector::CrossProduct(Right, Forward).GetSafeNormal();

			// determine order
			PCGEx::GetAxisOrder(Settings->SolidificationOrder, X, Y, Z);
			FVector BaseAxes[3] = {Forward, Right, Up};

			// Apply forward flip safely
			bool bFlipFwd = ForwardFlipBuffer && ForwardFlipBuffer->Read(Index);
			bool bFlipRight = RightFlipBuffer && RightFlipBuffer->Read(Index);

			Forward = BaseAxes[X];
			Right = BaseAxes[Y];
			Up = BaseAxes[Z];

			// Create initial direction
			FQuat Quat = FRotationMatrix::MakeFromX(Forward).ToQuat();
			const FVector TUp = Quat.GetUpVector();			
			Quat = FQuat::FindBetweenNormals((TUp - (TUp | Forward) * Forward).GetSafeNormal(), (Right - (Right | Forward) * Forward).GetSafeNormal()) * Quat;

			double EdgeLerp = FMath::Clamp(SolidificationLerp->Read(Index), 0.0, 1.0);
			if (bFlipFwd || bFlipRight) { EdgeLerp = 1.0 - EdgeLerp; }
			const double EdgeLerpInv = 1.0 - EdgeLerp;

			// update transform
			Transforms[Index] = FTransform(Quat, Path->GetEdgePositionAtAlpha(Index, EdgeLerp), Transforms[Index].GetScale3D());

			// bounds per axis index
			FVector& OutBoundsMin = BoundsMin[Index];
			FVector& OutBoundsMax = BoundsMax[Index];

			OutBoundsMin[X] = (-Length * EdgeLerp) * InvScale[X];
			OutBoundsMax[X] = (Length * EdgeLerpInv) * InvScale[X];

			if (RightRadiusBuffer)
			{
				const double Rad = FMath::Abs(RightRadiusBuffer->Read(Index));
				OutBoundsMin[Y] = -Rad * InvScale[Y];
				OutBoundsMax[Y] = Rad * InvScale[Y];
			}

			if (UpRadiusBuffer)
			{
				const double Rad = FMath::Abs(UpRadiusBuffer->Read(Index));
				OutBoundsMin[Z] = -Rad * InvScale[Z];
				OutBoundsMax[Z] = Rad * InvScale[Z];
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
