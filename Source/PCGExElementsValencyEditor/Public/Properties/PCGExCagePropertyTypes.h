// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCagePropertyBase.h"

#include "PCGExCagePropertyTypes.generated.h"

class UPCGExAssetCollection;

#pragma region Asset Collection

/**
 * Property component referencing a PCGExAssetCollection.
 * Used for mesh/actor swapping based on pattern matches.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Asset Collection"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_AssetCollection : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	/** The asset collection to reference */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	TSoftObjectPtr<UPCGExAssetCollection> AssetCollection;

	//~ Begin UPCGExCagePropertyBase Interface
	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
	//~ End UPCGExCagePropertyBase Interface
};

#pragma endregion

#pragma region Atomic Typed Properties

/**
 * String property component - outputs as FString attribute.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: String"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_String : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	FString Value;

	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
};

/**
 * Name property component - outputs as FName attribute.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Name"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_Name : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	FName Value;

	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
};

/**
 * Int32 property component - outputs as int32 attribute.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Int32"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_Int32 : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	int32 Value = 0;

	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
};

/**
 * Int64 property component - outputs as int64 attribute.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Int64"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_Int64 : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	int64 Value = 0;

	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
};

/**
 * Float property component - outputs as float attribute.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Float"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_Float : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	float Value = 0.0f;

	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
};

/**
 * Double property component - outputs as double attribute.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Double"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_Double : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	double Value = 0.0;

	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
};

/**
 * Bool property component - outputs as bool attribute.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Bool"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_Bool : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	bool Value = false;

	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
};

/**
 * Vector property component - outputs as FVector attribute.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Vector"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_Vector : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	FVector Value = FVector::ZeroVector;

	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
};

/**
 * Vector2D property component - outputs as FVector2D attribute.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Vector2D"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_Vector2 : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	FVector2D Value = FVector2D::ZeroVector;

	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
};

/**
 * Vector4 property component - outputs as FVector4 attribute.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Vector4"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_Vector4 : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	FVector4 Value = FVector4::Zero();

	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
};

/**
 * Color property component - authored as FLinearColor, outputs as FVector4 attribute.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Color"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_Color : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	FLinearColor Value = FLinearColor::White;

	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
};

/**
 * Rotator property component - outputs as FRotator attribute.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Rotator"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_Rotator : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	FRotator Value = FRotator::ZeroRotator;

	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
};

/**
 * Quaternion property component - outputs as FQuat attribute.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Quaternion"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_Quat : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	FQuat Value = FQuat::Identity;

	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
};

/**
 * Transform property component - outputs as FTransform attribute.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Transform"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_Transform : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	FTransform Value = FTransform::Identity;

	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
};

/**
 * SoftObjectPath property component - outputs as FSoftObjectPath attribute.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Soft Object Path"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_SoftObjectPath : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	FSoftObjectPath Value;

	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
};

/**
 * SoftClassPath property component - outputs as FSoftClassPath attribute.
 */
UCLASS(ClassGroup = "PCGEx|Valency", meta = (BlueprintSpawnableComponent, DisplayName = "Cage Property: Soft Class Path"))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageProperty_SoftClassPath : public UPCGExCagePropertyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Property")
	FSoftClassPath Value;

	virtual bool CompileProperty(FInstancedStruct& OutCompiled) const override;
	virtual UScriptStruct* GetCompiledStructType() const override;
};

#pragma endregion
