// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGExCommon.h"
#include "Data/PCGExRelationsParamsData.h"
//#include "PCGExRelationsHelpers.generated.h"

class UPCGPointData;

namespace PCGExRelational
{
	struct PCGEXTENDEDTOOLKIT_API FParamsInputs
	{
		FParamsInputs()
		{
			Params.Empty();
		}

		FParamsInputs(FPCGContext* Context, FName InputLabel): FParamsInputs()
		{
			TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(InputLabel);
			Initialize(Context, Sources);
		}

		FParamsInputs(FPCGContext* Context, TArray<FPCGTaggedData>& Sources): FParamsInputs()
		{
			Initialize(Context, Sources);
		}

	public:
		TArray<UPCGExRelationsParamsData*> Params;

		/**
		 * Initialize from Sources
		 * @param Context 
		 * @param Sources 
		 * @param bInitializeOutput 
		 */
		void Initialize(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, bool bInitializeOutput = false)
		{
			Params.Empty(Sources.Num());
			for (FPCGTaggedData& Source : Sources)
			{
				const UPCGExRelationsParamsData* ParamsData = Cast<UPCGExRelationsParamsData>(Source.Data);
				if (!ParamsData) { continue; }
				Params.Add(const_cast<UPCGExRelationsParamsData*>(ParamsData));
			}
		}

		void ForEach(FPCGContext* Context, const TFunction<void(UPCGExRelationsParamsData*, const int32)>& BodyLoop)
		{
			for (int i = 0; i < Params.Num(); i++)
			{
				UPCGExRelationsParamsData* ParamsData = Params[i];
				BodyLoop(ParamsData, i);
			}
		}

		bool IsEmpty() { return Params.IsEmpty(); }

		~FParamsInputs()
		{
			Params.Empty();
		}
	};

	/** Per-socket temp data structure for processing only*/
	struct PCGEXTENDEDTOOLKIT_API FSocketCandidate : FPCGExSocketDirection
	{
		FSocketCandidate()
		{
		}

		void PrepareForPoint(const FSocket& InSocket, const FPCGPoint& Point)
		{
			Direction = InSocket.Descriptor.Direction.Direction;
			DotTolerance = InSocket.Descriptor.Direction.DotTolerance;
			MaxDistance = InSocket.Descriptor.Direction.MaxDistance;

			const FTransform PtTransform = Point.Transform;
			Origin = PtTransform.GetLocation();

			if (InSocket.Descriptor.bRelativeOrientation)
			{
				Direction = PtTransform.Rotator().RotateVector(Direction);
				Direction.Normalize();
			}
		}

	public:
		FVector Origin = FVector::Zero();
		int32 Index = -1;
		double IndexedDistance = TNumericLimits<double>::Max();
		double IndexedDot = -1;
		double DistanceScale = 1.0;

		double GetScaledDistance() const { return MaxDistance * DistanceScale; }

		bool ProcessPoint(const FPCGPoint* Point)
		{
			const double LocalMaxDistance = MaxDistance * DistanceScale;
			const FVector PtPosition = Point->Transform.GetLocation();
			const FVector DirToPt = (PtPosition - Origin).GetSafeNormal();

			const double SquaredDistance = FVector::DistSquared(Origin, PtPosition);

			// Is distance smaller than last registered one?
			if (SquaredDistance > IndexedDistance) { return false; }

			//UE_LOG(LogTemp, Warning, TEXT("Dist %f / %f "), SquaredDistance, LocalMaxDistance * LocalMaxDistance)
			// Is distance inside threshold?
			if (SquaredDistance >= (LocalMaxDistance * LocalMaxDistance)) { return false; }

			const double Dot = Direction.Dot(DirToPt);

			// Is dot within tolerance?
			if (Dot < DotTolerance) { return false; }

			if (IndexedDistance == SquaredDistance)
			{
				// In case of distance equality, favor candidate closer to dot == 1
				if (Dot < IndexedDot) { return false; }
			}

			IndexedDistance = SquaredDistance;
			IndexedDot = Dot;

			return true;
		}

		FSocketData ToSocketData() const
		{
			return FSocketData{Index, IndexedDot, IndexedDistance};
		}
	};

	// Detail stored in a attribute array
	class PCGEXTENDEDTOOLKIT_API Helpers
	{
	public:
		static bool FindRelationalParams(
			TArray<FPCGTaggedData>& Sources,
			TArray<UPCGExRelationsParamsData*>& OutParams)
		{
			OutParams.Empty();
			bool bFoundAny = false;
			for (const FPCGTaggedData& TaggedData : Sources)
			{
				UPCGExRelationsParamsData* Params = Cast<UPCGExRelationsParamsData>(TaggedData.Data);
				if (!Params) { continue; }
				OutParams.Add(Params);
				bFoundAny = true;
			}

			return bFoundAny;
		}

		/**
		 * Prepare a list of SocketCandidate data to be used for the duration of a PointData processing.
		 * Assumes that the Params have been properly set-up before.
		 * @param Point 
		 * @param Data 
		 * @return 
		 */
		static double PrepareCandidatesForPoint(
			const FPCGPoint& Point,
			UPCGExRelationsParamsData* Params,
			TArray<FSocketCandidate>& Candidates)
		{
			const int32 NumSockets = Params->GetSocketMapping()->NumSockets;
			Candidates.Empty(NumSockets);

			double MaxDistance = Params->GreatestStaticMaxDistance;
			if (Params->bHasVariableMaxDistance) { MaxDistance = 0.0; }


			const TArray<FSocket>& Sockets = Params->GetSocketMapping()->GetSockets();
			const TArray<FModifier>& Modifiers = Params->GetSocketMapping()->GetModifiers();

			for (int i = 0; i < NumSockets; i++)
			{
				const PCGExRelational::FSocket& Socket = Sockets[i];
				const PCGExRelational::FModifier& Modifier = Modifiers[i];

				FSocketCandidate& NewCandidate = Candidates.Emplace_GetRef();
				NewCandidate.PrepareForPoint(Socket, Point);

				if (Modifier.bValid) { NewCandidate.DistanceScale = Modifier.GetValue(Point); }

				MaxDistance = FMath::Max(MaxDistance, NewCandidate.GetScaledDistance());
			}

			return MaxDistance;
		}

	};
}
