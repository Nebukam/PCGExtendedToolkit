// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPropertyCompiled.h"

#include "PCGExPropertyTypes.generated.h"

// Note: AssetCollection property type is defined in modules that depend on both
// PCGExProperties and PCGExCollections (e.g., PCGExElementsValency)

#pragma region Atomic Typed Properties

/**
 * String property - outputs as FString attribute.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyCompiled_String : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	FString Value;

protected:
	TSharedPtr<PCGExData::TBuffer<FString>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void CopyValueFrom(const FPCGExPropertyCompiled* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::String; }
	virtual FName GetTypeName() const override { return FName("String"); }
};

/**
 * Name property - outputs as FName attribute.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyCompiled_Name : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	FName Value;

protected:
	TSharedPtr<PCGExData::TBuffer<FName>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void CopyValueFrom(const FPCGExPropertyCompiled* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::Name; }
	virtual FName GetTypeName() const override { return FName("Name"); }
};

/**
 * Int32 property - outputs as int32 attribute.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyCompiled_Int32 : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	int32 Value = 0;

protected:
	TSharedPtr<PCGExData::TBuffer<int32>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void CopyValueFrom(const FPCGExPropertyCompiled* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::Integer32; }
	virtual FName GetTypeName() const override { return FName("Int32"); }
};

/**
 * Int64 property - outputs as int64 attribute.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyCompiled_Int64 : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	int64 Value = 0;

protected:
	TSharedPtr<PCGExData::TBuffer<int64>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void CopyValueFrom(const FPCGExPropertyCompiled* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::Integer64; }
	virtual FName GetTypeName() const override { return FName("Int64"); }
};

/**
 * Float property - outputs as float attribute.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyCompiled_Float : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	float Value = 0.0f;

protected:
	TSharedPtr<PCGExData::TBuffer<float>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void CopyValueFrom(const FPCGExPropertyCompiled* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::Float; }
	virtual FName GetTypeName() const override { return FName("Float"); }
};

/**
 * Double property - outputs as double attribute.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyCompiled_Double : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	double Value = 0.0;

protected:
	TSharedPtr<PCGExData::TBuffer<double>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void CopyValueFrom(const FPCGExPropertyCompiled* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::Double; }
	virtual FName GetTypeName() const override { return FName("Double"); }
};

/**
 * Bool property - outputs as bool attribute.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyCompiled_Bool : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	bool Value = false;

protected:
	TSharedPtr<PCGExData::TBuffer<bool>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void CopyValueFrom(const FPCGExPropertyCompiled* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::Boolean; }
	virtual FName GetTypeName() const override { return FName("Bool"); }
};

/**
 * Vector property - outputs as FVector attribute.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyCompiled_Vector : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	FVector Value = FVector::ZeroVector;

protected:
	TSharedPtr<PCGExData::TBuffer<FVector>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void CopyValueFrom(const FPCGExPropertyCompiled* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::Vector; }
	virtual FName GetTypeName() const override { return FName("Vector"); }
};

/**
 * Vector2D property - outputs as FVector2D attribute.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyCompiled_Vector2 : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	FVector2D Value = FVector2D::ZeroVector;

protected:
	TSharedPtr<PCGExData::TBuffer<FVector2D>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void CopyValueFrom(const FPCGExPropertyCompiled* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::Vector2; }
	virtual FName GetTypeName() const override { return FName("Vector2D"); }
};

/**
 * Vector4 property - outputs as FVector4 attribute.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyCompiled_Vector4 : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	FVector4 Value = FVector4::Zero();

protected:
	TSharedPtr<PCGExData::TBuffer<FVector4>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void CopyValueFrom(const FPCGExPropertyCompiled* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::Vector4; }
	virtual FName GetTypeName() const override { return FName("Vector4"); }
};

/**
 * Color property - authored as FLinearColor, outputs as FVector4 attribute.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyCompiled_Color : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	FLinearColor Value = FLinearColor::White;

protected:
	TSharedPtr<PCGExData::TBuffer<FVector4>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void CopyValueFrom(const FPCGExPropertyCompiled* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::Vector4; }
	virtual FName GetTypeName() const override { return FName("Color"); }
};

/**
 * Rotator property - outputs as FRotator attribute.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyCompiled_Rotator : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	FRotator Value = FRotator::ZeroRotator;

protected:
	TSharedPtr<PCGExData::TBuffer<FRotator>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void CopyValueFrom(const FPCGExPropertyCompiled* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::Rotator; }
	virtual FName GetTypeName() const override { return FName("Rotator"); }
};

/**
 * Quaternion property - outputs as FQuat attribute.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyCompiled_Quat : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	FQuat Value = FQuat::Identity;

protected:
	TSharedPtr<PCGExData::TBuffer<FQuat>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void CopyValueFrom(const FPCGExPropertyCompiled* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::Quaternion; }
	virtual FName GetTypeName() const override { return FName("Quat"); }
};

/**
 * Transform property - outputs as FTransform attribute.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyCompiled_Transform : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	FTransform Value = FTransform::Identity;

protected:
	TSharedPtr<PCGExData::TBuffer<FTransform>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void CopyValueFrom(const FPCGExPropertyCompiled* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::Transform; }
	virtual FName GetTypeName() const override { return FName("Transform"); }
};

/**
 * SoftObjectPath property - outputs as FSoftObjectPath attribute.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyCompiled_SoftObjectPath : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	FSoftObjectPath Value;

protected:
	TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void CopyValueFrom(const FPCGExPropertyCompiled* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::SoftObjectPath; }
	virtual FName GetTypeName() const override { return FName("SoftObjectPath"); }
};

/**
 * SoftClassPath property - outputs as FSoftClassPath attribute.
 */
USTRUCT(BlueprintType)
struct PCGEXPROPERTIES_API FPCGExPropertyCompiled_SoftClassPath : public FPCGExPropertyCompiled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	FSoftClassPath Value;

protected:
	TSharedPtr<PCGExData::TBuffer<FSoftClassPath>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void CopyValueFrom(const FPCGExPropertyCompiled* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::SoftClassPath; }
	virtual FName GetTypeName() const override { return FName("SoftClassPath"); }
};

#pragma endregion
