// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example cube mesh

#pragma once

#include "GameFramework/Actor.h"
#include "RuntimeMeshComponent.h"
#include "SimpleCubeActor.generated.h"

UCLASS()
class PROCEDURALMESHES_API ASimpleCubeActor : public AActor
{
	GENERATED_BODY()

public:
	ASimpleCubeActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	FVector Size = FVector(100.0f, 100.0f, 100.0f);

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
	void GenerateCube(TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, FVector InSize);
	void BuildQuad(TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, FVector BottomLeft, FVector BottomRight, FVector TopRight, FVector TopLeft, int32& VertexOffset, int32& TriangleOffset, FPackedNormal Normal, FPackedNormal Tangent);

	// Mesh buffers
	void SetupMeshBuffers();
	bool bHaveBuffersBeenInitialized = false;
	TArray<FRuntimeMeshVertexSimple> Vertices;
	TArray<int32> Triangles;
};
