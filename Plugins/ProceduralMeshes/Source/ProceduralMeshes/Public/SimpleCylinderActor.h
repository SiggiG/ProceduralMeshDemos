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
	virtual void PostLoad() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	URuntimeMeshProviderStatic* StaticProvider;

private:

	void GenerateMesh();
	static void GenerateCylinder(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FRuntimeMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, const float InHeight, const float InWidth, const int32 InCrossSectionCount, const bool bInCapEnds = false, const bool bInDoubleSided = false, const bool bInSmoothNormals = true);

	// Mesh buffers
	void SetupMeshBuffers();
	UPROPERTY(Transient)
	TArray<FVector> Positions;
	UPROPERTY(Transient)
	TArray<int32> Triangles;
	UPROPERTY(Transient)
	TArray<FVector> Normals;
	UPROPERTY(Transient)
	TArray<FRuntimeMeshTangent> Tangents;
	UPROPERTY(Transient)
	TArray<FVector2D> TexCoords;
};
