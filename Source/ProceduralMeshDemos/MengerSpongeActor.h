// Copyright Sigurdur Gunnarsson. All Rights Reserved.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// Example Menger sponge fractal mesh

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RuntimeProceduralMeshComponent.h"
#include "MengerSpongeActor.generated.h"

UCLASS()
class PROCEDURALMESHDEMOS_API AMengerSpongeActor : public AActor
{
	GENERATED_BODY()

public:
	AMengerSpongeActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float Size = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (ClampMin = "0", ClampMax = "4"))
	int32 Iterations = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	UMaterialInterface* Material;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	URuntimeProceduralMeshComponent* MeshComponent;

private:
	bool bRequiresMeshRebuild = false;

	void GenerateMesh();
	void SetupMeshBuffers(int32 InFaceCount);
	static void BuildQuad(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FProcMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, const FVector BottomLeft, const FVector BottomRight, const FVector TopRight, const FVector TopLeft, int32& VertexOffset, int32& TriangleOffset, const FVector Normal, const FProcMeshTangent Tangent);
	static bool IsSolid(int32 X, int32 Y, int32 Z, int32 N);

	// Mesh buffers
	TArray<FVector> Positions;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	TArray<FVector2D> TexCoords;
};
