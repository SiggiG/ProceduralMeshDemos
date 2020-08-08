// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example cylinder mesh

#pragma once

#include "CoreMinimal.h"
#include "RuntimeMeshActor.h"
#include "SimpleCylinderActor.generated.h"

UCLASS()
class PROCEDURALMESHES_API ASimpleCylinderActor : public ARuntimeMeshActor
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

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	URuntimeMeshProviderStatic* StaticProvider;

private:

	void GenerateMesh();
	void GenerateCylinder(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FRuntimeMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, float Height, float InWidth, int32 InCrossSectionCount, bool bCapEnds = false, bool bDoubleSided = false, bool bInSmoothNormals = true);

	// Mesh buffers
	void SetupMeshBuffers();
	TArray<FVector> Positions;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FRuntimeMeshTangent> Tangents;
	TArray<FVector2D> TexCoords;
};
