// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Helpers/PCGExPointDataHelpers.h"

#include "Data/PCGPointArrayData.h"
#include "Helpers/PCGExTestHelpers.h"
#include "UObject/Package.h"

#include "Data/PCGPointData.h"

namespace PCGExTest
{
	FPointDataBuilder::FPointDataBuilder()
	{
	}

	FPointDataBuilder::~FPointDataBuilder()
	{
	}

	FPointDataBuilder& FPointDataBuilder::WithGridPositions(
		const FVector& Origin,
		const FVector& Spacing,
		int32 CountX,
		int32 CountY,
		int32 CountZ)
	{
		Positions = GenerateGridPositions(Origin, Spacing, CountX, CountY, CountZ);
		return *this;
	}

	FPointDataBuilder& FPointDataBuilder::WithRandomPositions(
		const FBox& Bounds,
		int32 NumPoints,
		uint32 Seed)
	{
		Positions = GenerateRandomPositions(NumPoints, Bounds, Seed);
		return *this;
	}

	FPointDataBuilder& FPointDataBuilder::WithPositions(const TArray<FVector>& InPositions)
	{
		Positions = InPositions;
		return *this;
	}

	FPointDataBuilder& FPointDataBuilder::WithSpherePositions(
		const FVector& Center,
		double Radius,
		int32 NumPoints,
		uint32 Seed)
	{
		Positions = GenerateSpherePositions(Center, Radius, NumPoints, Seed);
		return *this;
	}

	FPointDataBuilder& FPointDataBuilder::WithScale(const FVector& Scale)
	{
		Scales.Init(Scale, FMath::Max(1, Positions.Num()));
		return *this;
	}

	FPointDataBuilder& FPointDataBuilder::WithRotation(const FRotator& Rotation)
	{
		Rotations.Init(Rotation, FMath::Max(1, Positions.Num()));
		return *this;
	}

	UPCGBasePointData* FPointDataBuilder::Build()
	{
		if (Positions.Num() == 0)
		{
			return nullptr;
		}

		UPCGBasePointData* PointData = NewObject<UPCGPointArrayData>(GetTransientPackage(), NAME_None, RF_Transient);
		if (!PointData)
		{
			return nullptr;
		}

		const int32 NumPoints = Positions.Num();

		// Allocate points using 5.7 API
		PointData->SetNumPoints(NumPoints);

		// Get mutable value ranges (no parameter = default behavior for writing)
		TPCGValueRange<FTransform> Transforms = PointData->GetTransformValueRange();
		TPCGValueRange<int32> Seeds = PointData->GetSeedValueRange();

		// Set transforms for each point
		for (int32 i = 0; i < NumPoints; ++i)
		{
			FVector Scale = Scales.Num() > 0 ? Scales[i % Scales.Num()] : FVector::OneVector;
			FRotator Rotation = Rotations.Num() > 0 ? Rotations[i % Rotations.Num()] : FRotator::ZeroRotator;

			Transforms[i] = FTransform(Rotation, Positions[i], Scale);
			Seeds[i] = i; // Deterministic seed based on index
		}

		// Apply pending attributes
		for (const FPendingAttribute& Attr : PendingAttributes)
		{
			for (int32 i = 0; i < NumPoints; ++i)
			{
				Attr.ApplyFunc(PointData, i);
			}
		}

		return PointData;
	}

	// Template specializations for common attribute types
	template <>
	FPointDataBuilder& FPointDataBuilder::WithAttribute<float>(FName Name, const TArray<float>& Values)
	{
		if (Values.Num() == 0) return *this;

		FPendingAttribute Attr;
		Attr.Name = Name;
		Attr.ApplyFunc = [Name, Values](UPCGBasePointData* Data, int32 Index)
		{
			if (UPCGMetadata* Metadata = Data->MutableMetadata())
			{
				FPCGMetadataAttribute<float>* Attribute = Metadata->FindOrCreateAttribute<float>(Name, 0.0f, true, false);
				if (Attribute)
				{
					Attribute->SetValue(Data->GetMetadataEntry(Index), Values[Index % Values.Num()]);
				}
			}
		};
		PendingAttributes.Add(MoveTemp(Attr));
		return *this;
	}

	template <>
	FPointDataBuilder& FPointDataBuilder::WithAttribute<double>(FName Name, const TArray<double>& Values)
	{
		if (Values.Num() == 0) return *this;

		FPendingAttribute Attr;
		Attr.Name = Name;
		Attr.ApplyFunc = [Name, Values](UPCGBasePointData* Data, int32 Index)
		{
			if (UPCGMetadata* Metadata = Data->MutableMetadata())
			{
				FPCGMetadataAttribute<double>* Attribute = Metadata->FindOrCreateAttribute<double>(Name, 0.0, true, false);
				if (Attribute)
				{
					Attribute->SetValue(Data->GetMetadataEntry(Index), Values[Index % Values.Num()]);
				}
			}
		};
		PendingAttributes.Add(MoveTemp(Attr));
		return *this;
	}

	template <>
	FPointDataBuilder& FPointDataBuilder::WithAttribute<int32>(FName Name, const TArray<int32>& Values)
	{
		if (Values.Num() == 0) return *this;

		FPendingAttribute Attr;
		Attr.Name = Name;
		Attr.ApplyFunc = [Name, Values](UPCGBasePointData* Data, int32 Index)
		{
			if (UPCGMetadata* Metadata = Data->MutableMetadata())
			{
				FPCGMetadataAttribute<int32>* Attribute = Metadata->FindOrCreateAttribute<int32>(Name, 0, true, false);
				if (Attribute)
				{
					Attribute->SetValue(Data->GetMetadataEntry(Index), Values[Index % Values.Num()]);
				}
			}
		};
		PendingAttributes.Add(MoveTemp(Attr));
		return *this;
	}

	template <>
	FPointDataBuilder& FPointDataBuilder::WithAttribute<FVector>(FName Name, const TArray<FVector>& Values)
	{
		if (Values.Num() == 0) return *this;

		FPendingAttribute Attr;
		Attr.Name = Name;
		Attr.ApplyFunc = [Name, Values](UPCGBasePointData* Data, int32 Index)
		{
			if (UPCGMetadata* Metadata = Data->MutableMetadata())
			{
				FPCGMetadataAttribute<FVector>* Attribute = Metadata->FindOrCreateAttribute<FVector>(Name, FVector::ZeroVector, true, false);
				if (Attribute)
				{
					Attribute->SetValue(Data->GetMetadataEntry(Index), Values[Index % Values.Num()]);
				}
			}
		};
		PendingAttributes.Add(MoveTemp(Attr));
		return *this;
	}

	namespace PointDataVerify
	{
		bool HasPointCount(const UPCGBasePointData* Data, int32 ExpectedCount)
		{
			if (!Data) return false;
			return Data->GetNumPoints() == ExpectedCount;
		}

		template <>
		bool HasAttribute<float>(const UPCGBasePointData* Data, FName AttributeName)
		{
			if (!Data || !Data->ConstMetadata()) return false;
			const FPCGMetadataAttributeBase* Attr = Data->ConstMetadata()->GetConstAttribute(AttributeName);
			return Attr && Attr->GetTypeId() == PCG::Private::MetadataTypes<float>::Id;
		}

		template <>
		bool HasAttribute<double>(const UPCGBasePointData* Data, FName AttributeName)
		{
			if (!Data || !Data->ConstMetadata()) return false;
			const FPCGMetadataAttributeBase* Attr = Data->ConstMetadata()->GetConstAttribute(AttributeName);
			return Attr && Attr->GetTypeId() == PCG::Private::MetadataTypes<double>::Id;
		}

		template <>
		bool HasAttribute<int32>(const UPCGBasePointData* Data, FName AttributeName)
		{
			if (!Data || !Data->ConstMetadata()) return false;
			const FPCGMetadataAttributeBase* Attr = Data->ConstMetadata()->GetConstAttribute(AttributeName);
			return Attr && Attr->GetTypeId() == PCG::Private::MetadataTypes<int32>::Id;
		}

		template <>
		bool HasAttribute<FVector>(const UPCGBasePointData* Data, FName AttributeName)
		{
			if (!Data || !Data->ConstMetadata()) return false;
			const FPCGMetadataAttributeBase* Attr = Data->ConstMetadata()->GetConstAttribute(AttributeName);
			return Attr && Attr->GetTypeId() == PCG::Private::MetadataTypes<FVector>::Id;
		}

		template <>
		float GetAttributeValue<float>(const UPCGBasePointData* Data, FName AttributeName, int32 Index, float DefaultValue)
		{
			if (!Data || !Data->ConstMetadata()) return DefaultValue;
			const FPCGMetadataAttribute<float>* Attr = Data->ConstMetadata()->GetConstTypedAttribute<float>(AttributeName);
			if (!Attr) return DefaultValue;

			if (Index >= 0 && Index < Data->GetNumPoints())
			{
				return Attr->GetValueFromItemKey(Data->GetMetadataEntry(Index));
			}
			return DefaultValue;
		}

		template <>
		int32 GetAttributeValue<int32>(const UPCGBasePointData* Data, FName AttributeName, int32 Index, int32 DefaultValue)
		{
			if (!Data || !Data->ConstMetadata()) return DefaultValue;
			const FPCGMetadataAttribute<int32>* Attr = Data->ConstMetadata()->GetConstTypedAttribute<int32>(AttributeName);
			if (!Attr) return DefaultValue;

			if (Index >= 0 && Index < Data->GetNumPoints())
			{
				return Attr->GetValueFromItemKey(Data->GetMetadataEntry(Index));
			}
			return DefaultValue;
		}
	}
}
