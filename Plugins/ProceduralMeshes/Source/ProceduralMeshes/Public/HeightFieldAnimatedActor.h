// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example heightfield grid animated with sine and cosine waves

#pragma once

#include "CoreMinimal.h"
#include "RuntimeMeshActor.h"
#include "HeightFieldAnimatedActor.generated.h"

UCLASS()
class PROCEDURALMESHES_API AHeightFieldAnimatedActor : public ARuntimeMeshActor // TODO Create a new provider instead of using the static provider
{
	GENERATED_BODY()

public:
	AHeightFieldAnimatedActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	FVector Size = FVector(1000.0f, 1000.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float ScaleFactor = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	int32 LengthSections = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	int32 WidthSections = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	UMaterialInterface* Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	bool AnimateMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float AnimationSpeedX = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float AnimationSpeedY = 4.5f;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostLoad() override;

	virtual void Tick(float DeltaSeconds) override;

protected:
	// TODO Create a new provider instead of using the static provider
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	URuntimeMeshProviderStatic* StaticProvider;

	float CurrentAnimationFrameX = 0.0f;
	float CurrentAnimationFrameY = 0.0f;

private:
	void GenerateMesh();
	void GeneratePoints();
	static void GenerateGrid(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FVector2D>& InTexCoords, const FVector2D InSize, const int32 InLengthSections, const int32 InWidthSections, const TArray<float>& InHeightValues);

	UPROPERTY(Transient)
	TArray<float> HeightValues;
	float MaxHeightValue = 0.0f;

	// Mesh buffers
	void SetupMeshBuffers();
	UPROPERTY(Transient)
	TArray<FVector> Positions;
	UPROPERTY(Transient)
	TArray<int32> Triangles;
	UPROPERTY(Transient)
	TArray<FVector> Normals;
	UPROPERTY(Transient)
	TArray<FVector2D> TexCoords;
};
