// Copyright Sigurdur Gunnarsson. All Rights Reserved.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// Branching mesh actor with Space Colonization algorithm and Catmull-Rom spline sweep

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RuntimeProceduralMeshComponent.h"
#include "BranchingLinesActor.h"
#include "BranchingMeshActor.generated.h"

UENUM(BlueprintType)
enum class EBranchCollisionType : uint8
{
	None              UMETA(DisplayName = "None"),
	ComplexAsSimple   UMETA(DisplayName = "Complex As Simple"),
	SimpleCapsules    UMETA(DisplayName = "Simple Capsules")
};

UENUM(BlueprintType)
enum class ECrownShape : uint8
{
	Sphere      UMETA(DisplayName = "Sphere"),
	Hemisphere  UMETA(DisplayName = "Hemisphere"),
	Cone        UMETA(DisplayName = "Cone"),
	Cylinder    UMETA(DisplayName = "Cylinder")
};

UCLASS()
class PROCEDURALMESHDEMOS_API ABranchingMeshActor : public AActor
{
	GENERATED_BODY()

public:
	ABranchingMeshActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (MakeEditWidget))
	FVector Start;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (MakeEditWidget))
	FVector End = FVector(0, 0, 300);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float TrunkWidth = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	int32 RadialSegmentCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	int32 RandomSeed = 1238;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	ECrownShape CrownShape = ECrownShape::Sphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (ClampMin = "1.0"))
	float CrownRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (ClampMin = "1"))
	int32 AttractorCount = 500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (ClampMin = "1.0"))
	float InfluenceRadius = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (ClampMin = "0.5"))
	float KillDistance = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (ClampMin = "0.1"))
	float GrowthStepLength = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (ClampMin = "1"))
	int32 MaxGrowthIterations = 200;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (ClampMin = "0.01"))
	float TipWidth = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float PipeModelExponent = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	UMaterialInterface* Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	EBranchEndCapType EndCapType = EBranchEndCapType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (EditCondition = "EndCapType == EBranchEndCapType::Taper", ClampMin = "0.1"))
	float TaperLength = 5.0f;

	/** Number of interpolation steps per segment along spline paths. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (ClampMin = "1", ClampMax = "32"))
	int32 SplineSubdivisions = 4;

	/** Length of the smooth fork transition zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (ClampMin = "0.1"))
	float ForkTransitionLength = 5.0f;

	/** Number of rings in each fork transition tube. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (ClampMin = "2", ClampMax = "16"))
	int32 ForkTransitionRings = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	EBranchCollisionType CollisionType = EBranchCollisionType::None;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostLoad() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	URuntimeProceduralMeshComponent* MeshComponent;

private:
	void GenerateMesh();
	void PreCacheCrossSection();

	// Internal data structures — no UPROPERTY to avoid CDO serialization bloat
	struct FBranchNode
	{
		FVector Position;
		float Width;
		int32 ParentIndex;
		TArray<int32> ChildIndices;
		bool bIsFork;
		bool bIsLeaf;
		bool bIsRoot;
	};

	struct FBranchPath
	{
		TArray<int32> NodeIndices;
		TArray<FVector> SplinePoints;
		TArray<float> SplineWidths;
		TArray<float> SplineDistances;
		float TotalLength;
	};

	struct FForkTrimInfo
	{
		FVector Position;
		float Width;
		FVector Direction;
	};

	void GenerateAttractors(TArray<FVector>& OutAttractors);
	void BuildTreeSpaceColonization(TArray<FBranchNode>& OutNodes);
	void ExtractBranchPaths(const TArray<FBranchNode>& Nodes, TArray<FBranchPath>& OutPaths);
	void EvaluateSplines(TArray<FBranchPath>& Paths, const TArray<FBranchNode>& Nodes);
	void TrimPathsAtForks(TArray<FBranchPath>& Paths, const TArray<FBranchNode>& Nodes,
		TMap<int32, FForkTrimInfo>& OutParentTrims, TMap<int32, FForkTrimInfo>& OutChildTrims);
	void GenerateTubeMesh(const TArray<FBranchPath>& Paths, int32& VertIdx, int32& TriIdx);
	void GenerateForkTransitions(const TArray<FBranchNode>& Nodes, const TArray<FBranchPath>& Paths,
		const TMap<int32, FForkTrimInfo>& ParentTrims, const TMap<int32, FForkTrimInfo>& ChildTrims,
		int32& VertIdx, int32& TriIdx);
	void GenerateEndCap(const FVector& RingCenter, const FQuat& RingOrientation, const FVector& OutwardDir, float Width, float InTaperLength, int32& VertIdx, int32& TriIdx);
	void GenerateEndCaps(const TArray<FBranchNode>& Nodes, const TArray<FBranchPath>& Paths, int32& VertIdx, int32& TriIdx);
	void GenerateCollision(const TArray<FBranchPath>& Paths);

	static FVector EvalCatmullRom(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float T, float Alpha = 0.5f);
	static float CatmullRomKnot(float Ti, const FVector& Pi, const FVector& Pj, float Alpha);

	FRandomStream RngStream;

	int32 LastCachedCrossSectionCount = 0;
	TArray<FVector> CachedCrossSectionPoints;

	// Mesh buffers — no UPROPERTY to avoid CDO serialization
	TArray<FVector> Positions;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	TArray<FVector2D> TexCoords;
};
