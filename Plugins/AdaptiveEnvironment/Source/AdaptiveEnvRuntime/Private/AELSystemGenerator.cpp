#include "AELSystemGenerator.h"

#include "AELSystemRuleAsset.h"

namespace AELSystemGeneratorPrivate
{
	struct FTurtleState
	{
		FVector Location = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
		float RadiusCm = 1.0f;
		int32 BranchOrder = 0;
		int32 Depth = 0;
		int64 ParentBranchId = 0;
		int64 BranchModuleId = 0;
	};

	/* Create one non-zero deterministic signed identity from stable inputs. */
	int64 MakeStableId(const int64 PlantId, const int32 Sequence, const uint32 Salt)
	{
		uint64 Hash = HashCombineFast(GetTypeHash(PlantId), GetTypeHash(Sequence));
		Hash = HashCombineFast(Hash, Salt);
		Hash &= static_cast<uint64>(MAX_int64);
		return static_cast<int64>(Hash == 0 ? 1 : Hash);
	}

	/* Return whether one symbol has an implemented Turtle interpretation. */
	bool IsSupportedSymbol(const TCHAR Symbol)
	{
		return Symbol == TEXT('F') || Symbol == TEXT('f') || Symbol == TEXT('+')
			|| Symbol == TEXT('-') || Symbol == TEXT('&') || Symbol == TEXT('^')
			|| Symbol == TEXT('\\') || Symbol == TEXT('/') || Symbol == TEXT('[')
			|| Symbol == TEXT(']') || Symbol == TEXT('L') || Symbol == TEXT('!');
	}
}

/* Generate one complete deterministic M8 plant without accessing World or UObject state during computation. */
bool FAELSystemGenerator::Generate(
	const UAELSystemRuleAsset& RuleAsset,
	const int32 Seed,
	const int64 StablePlantId,
	FAELSystemGeneratedPlant& OutPlant,
	FString& OutError)
{
	OutPlant = FAELSystemGeneratedPlant();
	if (!RuleAsset.Validate(OutError))
	{
		OutPlant.TruncationReason = EAELSystemTruncationReason::InvalidInput;
		return false;
	}

	// Expand the immutable asset snapshot before any geometry is created.
	FRandomStream Random(Seed);
	FString Symbols;
	if (!ExpandGrammar(RuleAsset, Random, Symbols, OutPlant.TruncationReason, OutError))
	{
		return false;
	}
	OutPlant.ExpandedSymbolCount = Symbols.Len();

	// Interpret stable topology and build one fixed mesh buffer per module.
	if (!InterpretTurtle(RuleAsset, Symbols, Seed, StablePlantId, OutPlant, OutError))
	{
		return false;
	}
	TSet<int64> ModuleIds;
	for (const FAEBranchSegment& Segment : OutPlant.BranchSegments)
	{
		ModuleIds.Add(Segment.BranchModuleId);
	}
	for (const int64 ModuleId : ModuleIds)
	{
		BuildModuleMesh(RuleAsset, OutPlant.BranchSegments, ModuleId, OutPlant.ModuleMeshes.Add(ModuleId));
	}

	// Hash logical output independently from runtime component identity.
	uint64 Hash = HashCombineFast(RuleAsset.ComputeContentHash(), GetTypeHash(Seed));
	for (const FAEBranchSegment& Segment : OutPlant.BranchSegments)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Segment.BranchId));
		Hash = HashCombineFast(Hash, GetTypeHash(Segment.ParentBranchId));
		Hash = HashCombineFast(Hash, GetTypeHash(Segment.BranchModuleId));
		Hash = HashCombineFast(Hash, GetTypeHash(Segment.StartLocationCm));
		Hash = HashCombineFast(Hash, GetTypeHash(Segment.EndLocationCm));
	}
	OutPlant.ContentHash = Hash;
	return true;
}

/* Expand all grammar generations from the same previous string for parallel-rewrite semantics. */
bool FAELSystemGenerator::ExpandGrammar(
	const UAELSystemRuleAsset& RuleAsset,
	FRandomStream& Random,
	FString& OutSymbols,
	EAELSystemTruncationReason& OutTruncation,
	FString& OutError)
{
	OutSymbols = RuleAsset.Axiom;
	const int32 Iterations = FMath::Min(RuleAsset.IterationCount, RuleAsset.MaxIterations);
	if (RuleAsset.IterationCount > RuleAsset.MaxIterations)
	{
		OutTruncation = EAELSystemTruncationReason::IterationLimit;
	}

	for (int32 Iteration = 0; Iteration < Iterations; ++Iteration)
	{
		FString Next;
		Next.Reserve(FMath::Min(OutSymbols.Len() * 2, RuleAsset.MaxSymbolCount));
		for (const TCHAR Symbol : OutSymbols)
		{
			TArray<const FAELSystemProductionRule*> Candidates;
			float TotalWeight = 0.0f;
			for (const FAELSystemProductionRule& Rule : RuleAsset.ProductionRules)
			{
				if (Rule.Predecessor[0] == Symbol)
				{
					Candidates.Add(&Rule);
					TotalWeight += Rule.Weight;
				}
			}

			if (Candidates.IsEmpty())
			{
				Next.AppendChar(Symbol);
			}
			else
			{
				float Choice = Random.FRandRange(0.0f, TotalWeight);
				const FAELSystemProductionRule* Selected = Candidates.Last();
				for (const FAELSystemProductionRule* Candidate : Candidates)
				{
					Choice -= Candidate->Weight;
					if (Choice <= 0.0f)
					{
						Selected = Candidate;
						break;
					}
				}
				Next.Append(Selected->Successor);
			}

			if (Next.Len() > RuleAsset.MaxSymbolCount)
			{
				Next.LeftInline(RuleAsset.MaxSymbolCount);
				OutTruncation = EAELSystemTruncationReason::SymbolLimit;
				break;
			}
		}
		OutSymbols = MoveTemp(Next);
		if (OutTruncation == EAELSystemTruncationReason::SymbolLimit)
		{
			break;
		}
	}

	for (const TCHAR Symbol : OutSymbols)
	{
		if (!AELSystemGeneratorPrivate::IsSupportedSymbol(Symbol))
		{
			OutError = FString::Printf(TEXT("Unsupported M8 symbol '%c'."), Symbol);
			OutTruncation = EAELSystemTruncationReason::InvalidInput;
			return false;
		}
	}
	return true;
}

/* Interpret bracketed Turtle commands into stable segment, module, and leaf-emitter identities. */
bool FAELSystemGenerator::InterpretTurtle(
	const UAELSystemRuleAsset& RuleAsset,
	const FString& Symbols,
	const int32 Seed,
	const int64 StablePlantId,
	FAELSystemGeneratedPlant& OutPlant,
	FString& OutError)
{
	using namespace AELSystemGeneratorPrivate;
	FTurtleState State;
	State.RadiusCm = RuleAsset.RootRadiusCm;
	TArray<FTurtleState> Stack;
	int32 Sequence = 0;
	int32 EmitterSequence = 0;
	int64 NextModuleId = 1;

	for (const TCHAR Symbol : Symbols)
	{
		switch (Symbol)
		{
		case TEXT('F'):
		{
			if (OutPlant.BranchSegments.Num() >= RuleAsset.MaxBranchSegments)
			{
				OutPlant.TruncationReason = EAELSystemTruncationReason::BranchLimit;
				return true;
			}
			const FVector Direction = State.Rotation.RotateVector(FVector::UpVector);
			FAEBranchSegment& Segment = OutPlant.BranchSegments.AddDefaulted_GetRef();
			Segment.BranchId = MakeStableId(StablePlantId, Sequence++, 0x4252414e);
			Segment.ParentBranchId = State.ParentBranchId;
			Segment.BranchModuleId = State.BranchModuleId;
			Segment.StartLocationCm = State.Location;
			Segment.EndLocationCm = State.Location + Direction * RuleAsset.SegmentLengthCm;
			Segment.StartRadiusCm = State.RadiusCm;
			Segment.EndRadiusCm = FMath::Max(State.RadiusCm * RuleAsset.RadiusDecayPerSegment, 0.01f);
			Segment.BranchOrder = State.BranchOrder;
			Segment.DerivationDepth = State.Depth;
			State.Location = Segment.EndLocationCm;
			State.RadiusCm = Segment.EndRadiusCm;
			State.ParentBranchId = Segment.BranchId;
			++State.Depth;
			break;
		}
		case TEXT('f'):
			State.Location += State.Rotation.RotateVector(FVector::UpVector) * RuleAsset.SegmentLengthCm;
			break;
		case TEXT('+'):
			State.Rotation = State.Rotation * FQuat(
				FVector::ForwardVector,
				FMath::DegreesToRadians(RuleAsset.BranchAngleDegrees));
			break;
		case TEXT('-'):
			State.Rotation = State.Rotation * FQuat(
				FVector::ForwardVector,
				-FMath::DegreesToRadians(RuleAsset.BranchAngleDegrees));
			break;
		case TEXT('&'):
			State.Rotation = State.Rotation * FQuat(FVector::RightVector, FMath::DegreesToRadians(RuleAsset.BranchAngleDegrees));
			break;
		case TEXT('^'):
			State.Rotation = State.Rotation * FQuat(FVector::RightVector, -FMath::DegreesToRadians(RuleAsset.BranchAngleDegrees));
			break;
		case TEXT('\\'):
			State.Rotation = State.Rotation * FQuat(
				FVector::UpVector,
				FMath::DegreesToRadians(RuleAsset.BranchAngleDegrees));
			break;
		case TEXT('/'):
			State.Rotation = State.Rotation * FQuat(
				FVector::UpVector,
				-FMath::DegreesToRadians(RuleAsset.BranchAngleDegrees));
			break;
		case TEXT('['):
		{
			Stack.Add(State);
			++State.BranchOrder;
			const bool bCanCreateModule =
				RuleAsset.StructuralResponse == EAEPlantStructuralResponse::HardStemBreakable
				&& State.BranchOrder <= RuleAsset.MaximumBreakableBranchOrder
				&& NextModuleId <= RuleAsset.MaxBreakableBranchModules;
			if (bCanCreateModule)
			{
				State.BranchModuleId = NextModuleId++;
			}
			else if (RuleAsset.StructuralResponse == EAEPlantStructuralResponse::HardStemBreakable
				&& State.BranchOrder <= RuleAsset.MaximumBreakableBranchOrder
				&& NextModuleId > RuleAsset.MaxBreakableBranchModules
				&& OutPlant.TruncationReason == EAELSystemTruncationReason::None)
			{
				OutPlant.TruncationReason = EAELSystemTruncationReason::BreakableModuleLimit;
			}
			break;
		}
		case TEXT(']'):
			if (Stack.IsEmpty())
			{
				OutError = TEXT("M8 Turtle stack underflow.");
				OutPlant.TruncationReason = EAELSystemTruncationReason::InvalidInput;
				return false;
			}
			State = Stack.Pop(EAllowShrinking::No);
			break;
		case TEXT('L'):
			if (State.ParentBranchId != 0 && OutPlant.LeafEmitters.Num() < RuleAsset.MaxLeafEmitters)
			{
				FAELeafEmitterDescriptor& Emitter = OutPlant.LeafEmitters.AddDefaulted_GetRef();
				Emitter.EmitterId = MakeStableId(StablePlantId, EmitterSequence++, 0x4c454146);
				Emitter.OwnerBranchId = State.ParentBranchId;
				Emitter.OwnerBranchModuleId = State.BranchModuleId;
				Emitter.LocalTransform = FTransform(State.Rotation, State.Location);
				Emitter.EmitterLengthCm = RuleAsset.SegmentLengthCm;
				Emitter.EmitterRadiusCm = State.RadiusCm;
				Emitter.DensityPerMeter = RuleAsset.LeafDensityPerMeter;
				Emitter.Seed = Seed ^ static_cast<int32>(Emitter.EmitterId);
				for (FAEBranchSegment& Segment : OutPlant.BranchSegments)
				{
					if (Segment.BranchId == State.ParentBranchId)
					{
						Segment.bSupportsLeaves = true;
						break;
					}
				}
			}
			else if (OutPlant.LeafEmitters.Num() >= RuleAsset.MaxLeafEmitters)
			{
				OutPlant.TruncationReason = EAELSystemTruncationReason::LeafEmitterLimit;
			}
			break;
		case TEXT('!'):
			State.RadiusCm = FMath::Max(State.RadiusCm * RuleAsset.RadiusDecayPerSegment, 0.01f);
			break;
		default:
			break;
		}
	}

	if (!Stack.IsEmpty())
	{
		OutError = TEXT("M8 Turtle stack is unbalanced.");
		OutPlant.TruncationReason = EAELSystemTruncationReason::InvalidInput;
		return false;
	}
	return true;
}

/* Build capped frustum geometry for every segment in one immutable branch module. */
void FAELSystemGenerator::BuildModuleMesh(
	const UAELSystemRuleAsset& RuleAsset,
	const TArray<FAEBranchSegment>& Segments,
	const int64 ModuleId,
	FAEM8MeshBuffers& OutMesh)
{
	OutMesh = FAEM8MeshBuffers();
	const int32 Sides = FMath::Max(RuleAsset.RadialSegments, 3);
	for (const FAEBranchSegment& Segment : Segments)
	{
		if (Segment.BranchModuleId != ModuleId)
		{
			continue;
		}

		// Build a stable orthonormal frame around the segment axis.
		const FVector Axis = (Segment.EndLocationCm - Segment.StartLocationCm).GetSafeNormal();
		const FVector Reference = FMath::Abs(Axis.Z) < 0.99f ? FVector::UpVector : FVector::RightVector;
		const FVector Right = FVector::CrossProduct(Reference, Axis).GetSafeNormal();
		const FVector Up = FVector::CrossProduct(Axis, Right).GetSafeNormal();
		const int32 VertexBase = OutMesh.Vertices.Num();
		for (int32 Side = 0; Side < Sides; ++Side)
		{
			const float Angle = UE_TWO_PI * static_cast<float>(Side) / static_cast<float>(Sides);
			const FVector Radial = Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle);
			OutMesh.Vertices.Add(FVector3f(Segment.StartLocationCm + Radial * Segment.StartRadiusCm));
			OutMesh.Vertices.Add(FVector3f(Segment.EndLocationCm + Radial * Segment.EndRadiusCm));
			OutMesh.Normals.Add(FVector3f(Radial));
			OutMesh.Normals.Add(FVector3f(Radial));
			const float U = static_cast<float>(Side) / static_cast<float>(Sides);
			OutMesh.UV0.Add(FVector2f(U, 0.0f));
			OutMesh.UV0.Add(FVector2f(U, 1.0f));
		}

		// Connect adjacent rings with two consistently wound triangles per side.
		for (int32 Side = 0; Side < Sides; ++Side)
		{
			const int32 Next = (Side + 1) % Sides;
			const int32 A = VertexBase + Side * 2;
			const int32 B = VertexBase + Next * 2;
			const int32 C = A + 1;
			const int32 D = B + 1;
			OutMesh.Triangles.Add(FIntVector(A, B, C));
			OutMesh.Triangles.Add(FIntVector(C, B, D));
		}
	}
}
