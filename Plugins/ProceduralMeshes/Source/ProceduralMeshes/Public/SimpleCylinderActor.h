// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example cylinder mesh

#pragma once

#include "GameFramework/Actor.h"
#include "RuntimeMeshComponent.h"
#include "SimpleCylinderActor.generated.h"

UCLASS()
class PROCEDURALMESHES_API ASimpleCylinderActor : public AActor
{
	GENERATED_BODY()

public:
	ASimpleCylinderActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float Radius = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float Height = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	int32 RadialSegmentCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	bool bCapEnds = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	bool bDoubleSided = false;

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
	void GenerateCylinder(TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, float Height, float InWidth, int32 InCrossSectionCount, bool bCapEnds = false, bool bDoubleSided = false, bool bInSmoothNormals = true);

	// Mesh buffers
	void SetupMeshBuffers();
	bool bHaveBuffersBeenInitialized = false;
	TArray<FRuntimeMeshVertexSimple> Vertices;
	TArray<int32> Triangles;
};
