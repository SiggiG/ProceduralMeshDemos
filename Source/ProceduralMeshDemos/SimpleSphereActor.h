// Copyright Sigurdur Gunnarsson. All Rights Reserved.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// Example sphere mesh

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RuntimeProceduralMeshComponent.h"
#include "SimpleSphereActor.generated.h"

UENUM(BlueprintType)
enum class ESphereType : uint8
{
	UVSphere        UMETA(DisplayName = "UV Sphere"),
	Geodesic        UMETA(DisplayName = "Geodesic (Icosphere)"),
	NormalizedCube  UMETA(DisplayName = "Normalized Cube")
};

UCLASS()
class PROCEDURALMESHDEMOS_API ASimpleSphereActor : public AActor
{
	GENERATED_BODY()

public:
	ASimpleSphereActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float Radius = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	ESphereType SphereType = ESphereType::UVSphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (ClampMin = "1", ClampMax = "256"))
	int32 Segments = 16;

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
	void SetupMeshBuffers();
	void GenerateUVSphere();
	void GenerateGeodesicSphere();
	void GenerateNormalizedCubeSphere();

	// Mesh buffers
	TArray<FVector> Positions;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	TArray<FVector2D> TexCoords;
};
