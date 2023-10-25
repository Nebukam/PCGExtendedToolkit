// CustomPoint.h

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "UObject/NoExportTypes.h"
#include "PCGExCustomPoint.generated.h"

const double CustomPointEpsilon = 0.001; // Define a small tolerance value for floating-point comparison

USTRUCT(BlueprintType)
struct FCustomPoint
{
    GENERATED_BODY()

        UPROPERTY(BlueprintReadWrite, EditAnywhere)
        double X;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
        double Y;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
        double Z;

    // Default constructor
    FCustomPoint() : X(0), Y(0), Z(0) {}

    // Constructor with parameters
    FCustomPoint(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}

    FCustomPoint(const FVector& Vector) : X(Vector.X), Y(Vector.Y), Z(Vector.Z) {}

    operator FVector() const
    {
        return FVector(X, Y, Z);
    }

    // Equality comparison operator
    bool operator==(const FCustomPoint& Other) const
    {
        return FMath::Abs(X - Other.X) < CustomPointEpsilon &&
            FMath::Abs(Y - Other.Y) < CustomPointEpsilon &&
            FMath::Abs(Z - Other.Z) < CustomPointEpsilon;
    }
    bool operator!=(const FCustomPoint& Other) const
    {
        return !(*this == Other);
    }

    // Overloaded + operator for adding two FCustomPoint instances
    FCustomPoint operator+(const FCustomPoint& Other) const
    {
        return FCustomPoint(X + Other.X, Y + Other.Y, Z + Other.Z);
    }

    // Calculate the squared Euclidean distance between two points (faster than calculating full Euclidean distance)
    double GetSquaredDistance(const FCustomPoint& Other) const
    {
        double DX = X - Other.X;
        double DY = Y - Other.Y;
        double DZ = Z - Other.Z;
        return DX * DX + DY * DY + DZ * DZ;
    }

    double GetEuclideanDistance(const FCustomPoint& Other) const
    {
        double DX = X - Other.X;
        double DY = Y - Other.Y;
        double DZ = Z - Other.Z;
        return FMath::Sqrt(DX * DX + DY * DY + DZ * DZ);
    }
};

USTRUCT(BlueprintType)
struct FGraphNode
{
    GENERATED_BODY()

        UPROPERTY(BlueprintReadWrite, EditAnywhere)
        FCustomPoint Position;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
        TArray<int32> ConnectedNodeIndices;

    // Equality comparison operator for FGraphNode
    bool operator==(const FGraphNode& Other) const
    {
        return Position == Other.Position; //&& ConnectedNodeIndices == Other.ConnectedNodeIndices;
    }


};

FORCEINLINE uint32 GetTypeHash(const FCustomPoint& Point)
{
    return HashCombine(GetTypeHash(Point.X), HashCombine(GetTypeHash(Point.Y), GetTypeHash(Point.Z)));
}

FORCEINLINE uint32 GetTypeHash(const FGraphNode& Node)
{
    return GetTypeHash(Node.Position);
}
