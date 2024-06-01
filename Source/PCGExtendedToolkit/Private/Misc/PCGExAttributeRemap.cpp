// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExAttributeRemap.h"

#define LOCTEXT_NAMESPACE "PCGExAttributeRemap"
#define PCGEX_NAMESPACE AttributeRemap


PCGExData::EInit UPCGExAttributeRemapSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGExAttributeRemapContext::~FPCGExAttributeRemapContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGEX_INITIALIZE_ELEMENT(AttributeRemap)

bool FPCGExAttributeRemapElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AttributeRemap)

	PCGEX_VALIDATE_NAME(Settings->SourceAttributeName)
	PCGEX_VALIDATE_NAME(Settings->TargetAttributeName)

	Context->RemapSettings[0] = Settings->BaseRemap;
	Context->RemapSettings[1] = Settings->Component2RemapOverride;
	Context->RemapSettings[2] = Settings->Component3RemapOverride;
	Context->RemapSettings[3] = Settings->Component4RemapOverride;

	for (int i = 0; i < 4; i++) { Context->RemapSettings[i].RemapSettings.LoadCurve(); }

	Context->RemapIndices[0] = 0;
	Context->RemapIndices[1] = Settings->bOverrideComponent2 ? 1 : 0;
	Context->RemapIndices[2] = Settings->bOverrideComponent3 ? 2 : 0;
	Context->RemapIndices[3] = Settings->bOverrideComponent4 ? 3 : 0;

	return true;
}

bool FPCGExAttributeRemapElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributeRemapElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(AttributeRemap)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		int32 IOIndex = 0;
		while (Context->AdvancePointsIO())
		{
			PCGEx::FAttributesInfos* Infos = PCGEx::FAttributesInfos::Get(Context->CurrentIO->GetIn()->Metadata);
			const PCGEx::FAttributeIdentity* AttIdentity = Infos->Find(Settings->SourceAttributeName);

			if (!AttIdentity)
			{
				PCGEX_DELETE(Infos)
				PCGE_LOG(Error, GraphAndLog, FTEXT("Some inputs are missing the specified source attribute."));
				continue;
			}

			int32 Dimensions;
			switch (AttIdentity->UnderlyingType)
			{
			case EPCGMetadataTypes::Float:
			case EPCGMetadataTypes::Double:
			case EPCGMetadataTypes::Integer32:
			case EPCGMetadataTypes::Integer64:
				Dimensions = 1;
				break;
			case EPCGMetadataTypes::Vector2:
				Dimensions = 2;
				break;
			case EPCGMetadataTypes::Vector:
			case EPCGMetadataTypes::Rotator:
				Dimensions = 3;
				break;
			case EPCGMetadataTypes::Vector4:
			case EPCGMetadataTypes::Quaternion:
				Dimensions = 4;
				break;
			default:
			case EPCGMetadataTypes::Transform:
			case EPCGMetadataTypes::String:
			case EPCGMetadataTypes::Boolean:
			case EPCGMetadataTypes::Name:
			case EPCGMetadataTypes::Unknown:
				Dimensions = -1;
				break;
			}

			if (Dimensions == -1)
			{
				PCGEX_DELETE(Infos)
				PCGE_LOG(Error, GraphAndLog, FTEXT("Source attribute type cannot be remapped."));
				continue;
			}

			if (AttIdentity)
			{
				Context->GetAsyncManager()->Start<FPCGExRemapPointIO>(
					IOIndex++, Context->CurrentIO,
					AttIdentity->UnderlyingType, Dimensions);
			}
			else
			{
				PCGE_LOG(Error, GraphAndLog, FTEXT("Some inputs are missing the specified source attribute."));
			}

			PCGEX_DELETE(Infos)
		}

		Context->SetAsyncState(PCGExMT::State_ProcessingPoints);
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		PCGEX_WAIT_ASYNC

		Context->OutputPoints();
		Context->Done();
	}

	return Context->IsDone();
}

bool FPCGExRemapPointIO::ExecuteTask()
{
	const FPCGExAttributeRemapContext* Context = Manager->GetContext<FPCGExAttributeRemapContext>();
	PCGEX_SETTINGS(AttributeRemap);

	FPCGExComponentRemapRule RemapSettings[4];

	int32 NumPoints = PointIO->GetNum();

	auto ProcessValues = [&](auto DummyValue) -> void
	{
		using RawT = decltype(DummyValue);

		TArray<RawT> RawValues;
		RawValues.SetNumUninitialized(NumPoints);

		FPCGMetadataAttribute<RawT>* AttributeIn = PointIO->GetIn()->Metadata->GetMutableTypedAttribute<RawT>(Settings->SourceAttributeName);
		FPCGAttributeAccessor<RawT>* Getter = new FPCGAttributeAccessor<RawT>(AttributeIn, PointIO->GetIn()->Metadata);

		TArrayView<RawT> Values(RawValues);
		Getter->GetRange(
			Values, 0, *PointIO->CreateInKeys(),
			EPCGAttributeAccessorFlags::AllowBroadcast);

		for (int i = 0; i < Dimensions; i++)
		{
			FPCGExComponentRemapRule Rule = FPCGExComponentRemapRule(Context->RemapSettings[Context->RemapIndices[i]]);

			const double CachedRMin = Rule.RemapSettings.InMin;
			const double CachedRMax = Rule.RemapSettings.InMax;

			Rule.RemapSettings.InMin = TNumericLimits<double>::Max();
			Rule.RemapSettings.InMax = TNumericLimits<double>::Min();

			double VAL;

			if (Rule.RemapSettings.bUseAbsoluteRange)
			{
				for (RawT& V : RawValues)
				{
					VAL = Rule.InputClampSettings.GetClampedValue(PCGExMath::GetComponent(V, i));
					PCGExMath::SetComponent(V, i, VAL);

					Rule.RemapSettings.InMin = FMath::Min(Rule.RemapSettings.InMin, FMath::Abs(VAL));
					Rule.RemapSettings.InMax = FMath::Max(Rule.RemapSettings.InMax, FMath::Abs(VAL));
				}

				if (Rule.RemapSettings.bUseInMin) { Rule.RemapSettings.InMin = CachedRMin; }
				if (Rule.RemapSettings.bUseInMax) { Rule.RemapSettings.InMax = CachedRMax; }

				if (Rule.RemapSettings.RangeMethod == EPCGExRangeType::FullRange) { Rule.RemapSettings.InMin = 0; }

				if (Rule.RemapSettings.bPreserveSign)
				{
					for (RawT& V : RawValues)
					{
						VAL = PCGExMath::GetComponent(V, i);
						VAL = Rule.RemapSettings.GetRemappedValue(FMath::Abs(VAL)) * PCGExMath::SignPlus(VAL);
						VAL = Rule.OutputClampSettings.GetClampedValue(VAL);

						PCGExMath::SetComponent(V, i, VAL);
					}
				}
				else
				{
					for (RawT& V : RawValues)
					{
						VAL = PCGExMath::GetComponent(V, i);
						VAL = Rule.RemapSettings.GetRemappedValue(FMath::Abs(VAL));
						VAL = Rule.OutputClampSettings.GetClampedValue(VAL);

						PCGExMath::SetComponent(V, i, VAL);
					}
				}
			}
			else
			{
				for (RawT& V : RawValues)
				{
					VAL = Rule.InputClampSettings.GetClampedValue(PCGExMath::GetComponent(V, i));
					PCGExMath::SetComponent(V, i, VAL);

					Rule.RemapSettings.InMin = FMath::Min(Rule.RemapSettings.InMin, VAL);
					Rule.RemapSettings.InMax = FMath::Max(Rule.RemapSettings.InMax, VAL);
				}

				if (Rule.RemapSettings.bUseInMin) { Rule.RemapSettings.InMin = CachedRMin; }
				if (Rule.RemapSettings.bUseInMax) { Rule.RemapSettings.InMax = CachedRMax; }

				if (Rule.RemapSettings.RangeMethod == EPCGExRangeType::FullRange) { Rule.RemapSettings.InMin = 0; }

				if (Rule.RemapSettings.bPreserveSign)
				{
					for (RawT& V : RawValues)
					{
						VAL = PCGExMath::GetComponent(V, i);
						VAL = Rule.RemapSettings.GetRemappedValue(VAL);
						VAL = Rule.OutputClampSettings.GetClampedValue(VAL);

						PCGExMath::SetComponent(V, i, VAL);
					}
				}
				else
				{
					for (RawT& V : RawValues)
					{
						VAL = PCGExMath::GetComponent(V, i);
						VAL = Rule.RemapSettings.GetRemappedValue(FMath::Abs(VAL));
						VAL = Rule.OutputClampSettings.GetClampedValue(VAL);

						PCGExMath::SetComponent(V, i, VAL);
					}
				}
			}
		}

		FPCGMetadataAttribute<RawT>* AttributeOut = PointIO->GetOut()->Metadata->FindOrCreateAttribute<RawT>(
			Settings->TargetAttributeName, RawT{},
			AttributeIn->AllowsInterpolation(), true, true);

		FPCGAttributeAccessor<RawT>* Setter = new FPCGAttributeAccessor<RawT>(AttributeOut, PointIO->GetOut()->Metadata);
		Setter->template SetRange<RawT>(
			Values, 0, *PointIO->CreateOutKeys(),
			EPCGAttributeAccessorFlags::AllowBroadcast);

		RawValues.Empty();

		delete Getter;
		delete Setter;
	};

	PCGMetadataAttribute::CallbackWithRightType(static_cast<uint16>(DataType), ProcessValues);

	return false;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
