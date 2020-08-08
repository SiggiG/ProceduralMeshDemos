// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example cube mesh

#pragma once

#include "CoreMinimal.h"
#include "RuntimeMeshActor.h"
#include "SimpleCubeActor.generated.h"

class URuntimeMeshProviderStatic;
struct FRuntimeMeshTangent;

UCLASS()
class PROCEDURALMESHES_API ASimpleCubeActor : public ARuntimeMeshActor
{
	GENERATED_BODY()

public:
	ASimpleCubeActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	FVector Size = FVector(100.0f, 100.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	UMaterialInterface* Material;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	URuntimeMeshProviderStatic* StaticProvider;

private:
	void GenerateMesh();
	static void GenerateCube(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FRuntimeMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, FVector InSize);
	static void BuildQuad(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FRuntimeMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, FVector BottomLeft, FVector BottomRight, FVector TopRight, FVector TopLeft, int32& VertexOffset, int32& TriangleOffset, FVector Normal, FRuntimeMeshTangent Tangent);

	// Mesh buffers
	void SetupMeshBuffers();
	TArray<FVector> Positions;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FRuntimeMeshTangent> Tangents;
	TArray<FVector2D> TexCoords;
};
