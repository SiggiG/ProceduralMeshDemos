// Copyright Sigurdur Gunnarsson. All Rights Reserved.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// Example cylinder strip mesh with smooth joints at corners

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RuntimeProceduralMeshComponent.h"
#include "SmoothCylinderStripActor.generated.h"

/**
 * Generates a continuous tube along a polyline with smooth corner joints.
 * Unlike CylinderStripActor which creates independent cylinders per segment (leaving gaps),
 * this actor builds a single continuous mesh. At each joint, the cross-section orientation
 * is interpolated using quaternion slerp over JointSegments steps, producing a smooth
 * spherical joint that cleanly connects angled segments.
 */
UCLASS()
class PROCEDURALMESHDEMOS_API ASmoothCylinderStripActor : public AActor
{
	GENERATED_BODY()

public:
	ASmoothCylinderStripActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	TArray<FVector> LinePoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float Radius = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	int32 RadialSegmentCount = 10;

	/** Number of interpolation steps per joint. 0 = sharp miter, higher = smoother arc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (ClampMin = "0", ClampMax = "32"))
	int32 JointSegments = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	UMaterialInterface* Material;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostLoad() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	URuntimeProceduralMeshComponent* MeshComponent;

private:
	void GenerateMesh();
	void PreCacheCrossSection();

	int32 LastCachedCrossSectionCount = 0;
	TArray<FVector> CachedCrossSectionPoints;

	// Mesh buffers
	TArray<FVector> Positions;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	TArray<FVector2D> TexCoords;
};
