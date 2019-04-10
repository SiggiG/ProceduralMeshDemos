// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example cylinder strip mesh

#pragma once

#include "GameFramework/Actor.h"
#include "RuntimeMeshComponent.h"
#include "CylinderStripActor.generated.h"

UCLASS()
class PROCEDURALMESHES_API ACylinderStripActor : public AActor
{
	GENERATED_BODY()

public:
	ACylinderStripActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	TArray<FVector> LinePoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float Radius = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	int32 RadialSegmentCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	bool bSmoothNormals = true;

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
	void GenerateCylinder(TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, FVector StartPoint, FVector EndPoint, float InWidth, int32 InCrossSectionCount, int32& InVertexIndex, int32& InTriangleIndex, bool bInSmoothNormals = true);
	FBox GetBounds();

	FVector RotatePointAroundPivot(FVector InPoint, FVector InPivot, FVector InAngles);
	void PreCacheCrossSection();

	int32 LastCachedCrossSectionCount;
	UPROPERTY(Transient)
	TArray<FVector> CachedCrossSectionPoints;

	// Mesh buffers
	void SetupMeshBuffers();
	bool bHaveBuffersBeenInitialized = false;
	TArray<FRuntimeMeshVertexSimple> Vertices;
	TArray<int32> Triangles;
};
