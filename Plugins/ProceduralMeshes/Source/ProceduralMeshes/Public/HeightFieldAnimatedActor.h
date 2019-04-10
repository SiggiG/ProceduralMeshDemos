// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example heightfield grid animated with sine and cosine waves

#pragma once

#include "GameFramework/Actor.h"
#include "RuntimeMeshComponent.h"
#include "HeightFieldAnimatedActor.generated.h"

UCLASS()
class PROCEDURALMESHES_API AHeightFieldAnimatedActor : public AActor
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

	virtual void PostLoad() override;
	virtual void PostActorCreated() override;

	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Default)
	USceneComponent* RootNode;

	UPROPERTY()
	URuntimeMeshComponent* MeshComponent;

	float CurrentAnimationFrameX = 0.0f;
	float CurrentAnimationFrameY = 0.0f;

private:
	void GenerateMesh();
	void GeneratePoints();
	void GenerateGrid(TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, FVector2D InSize, int32 InLengthSections, int32 InWidthSections, const TArray<float>& InHeightValues);

	TArray<float> HeightValues;
	float MaxHeightValue = 0.0f;

	// Mesh buffers
	void SetupMeshBuffers();
	bool bHaveBuffersBeenInitialized = false;
	TArray<FRuntimeMeshVertexSimple> Vertices;
	TArray<int32> Triangles;
};
