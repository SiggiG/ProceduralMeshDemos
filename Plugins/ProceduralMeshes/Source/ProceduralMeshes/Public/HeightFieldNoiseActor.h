// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example heightfield generated with noise

#pragma once

#include "GameFramework/Actor.h"
#include "RuntimeMeshComponent.h"
#include "HeightFieldNoiseActor.generated.h"

UCLASS()
class PROCEDURALMESHES_API AHeightFieldNoiseActor : public AActor
{
	GENERATED_BODY()

public:
	AHeightFieldNoiseActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	FVector Size = FVector(1000.0f, 1000.0f, 20.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	int32 LengthSections = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	int32 WidthSections = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	int32 RandomSeed = 1238;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	UMaterialInterface* Material;

	virtual void PostLoad() override;
	virtual void PostActorCreated() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Default)
	USceneComponent* RootNode;

	UPROPERTY()
	URuntimeMeshComponent* MeshComponent;

private:
	void GenerateMesh();
	void GeneratePoints();
	void GenerateGrid(TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, FVector2D InSize, int32 InLengthSections, int32 InWidthSections, const TArray<float>& InHeightValues);

	UPROPERTY(Transient)
	FRandomStream RngStream;

	TArray<float> HeightValues;

	// Mesh buffers
	void SetupMeshBuffers();
	bool bHaveBuffersBeenInitialized = false;
	TArray<FRuntimeMeshVertexSimple> Vertices;
	TArray<int32> Triangles;
};
