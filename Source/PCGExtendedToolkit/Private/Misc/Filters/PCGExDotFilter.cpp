// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExDotFilter.h"

PCGExPointFilter::TFilter* UPCGExDotFilterFactory::CreateFilter() const
{
	return new PCGExPointsFilter::TDotFilter(this);
}

bool PCGExPointsFilter::TDotFilter::Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade)
{
	if (!TFilter::Init(InContext, InPointDataFacade)) { return false; }

	OperandA = PointDataFacade->GetOrCreateGetter<FVector>(TypedFilterFactory->Descriptor.OperandA);
	if (!OperandA)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.OperandA.GetName())));
		return false;
	}

	if (TypedFilterFactory->Descriptor.CompareAgainst == EPCGExFetchType::Attribute)
	{
		OperandB = PointDataFacade->GetOrCreateGetter<FVector>(TypedFilterFactory->Descriptor.OperandB);
		if (!OperandB)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.OperandB.GetName())));
			return false;
		}
	}

	return true;
}

bool PCGExPointsFilter::TDotFilter::Test(const int32 PointIndex) const
{
	const FPCGPoint& Point = PointDataFacade->Source->GetInPoint(PointIndex);

	const FVector A = TypedFilterFactory->Descriptor.bTransformOperandA ?
		                  OperandA->Values[PointIndex] :
		                  Point.Transform.TransformVectorNoScale(OperandA->Values[PointIndex]);

	FVector B = OperandB ? OperandB->Values[PointIndex] : TypedFilterFactory->Descriptor.OperandBConstant;
	if (TypedFilterFactory->Descriptor.bTransformOperandB) { B = Point.Transform.TransformVectorNoScale(B); }

	const double Dot = DotComparison.bUnsignedDot ? FMath::Abs(FVector::DotProduct(A, B)) : FVector::DotProduct(A, B);
	return DotComparison.Test(Dot, DotComparison.GetDot(PointIndex));
}

#define LOCTEXT_NAMESPACE "PCGExDotFilterDefinition"
#define PCGEX_NAMESPACE PCGExDotFilterDefinition

PCGEX_CREATE_FILTER_FACTORY(Dot)

#if WITH_EDITOR
FString UPCGExDotFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = Descriptor.OperandA.GetName().ToString() + " . ";

	if (Descriptor.CompareAgainst == EPCGExFetchType::Attribute) { DisplayName += Descriptor.OperandB.GetName().ToString(); }
	else { DisplayName += " (Constant)"; }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
