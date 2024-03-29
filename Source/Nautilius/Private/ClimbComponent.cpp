// Fill out your copyright notice in the Description page of Project Settings.

#include "ClimbComponent.h"
#include <Kismet/KismetMathLibrary.h>
#include "GameFramework/Character.h"

// Sets default values for this component's properties
UClimbComponent::UClimbComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UClimbComponent::BeginPlay()
{
	Super::BeginPlay();

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (OwnerCharacter == nullptr) return;

	OwnerController = OwnerCharacter->GetController();
	if (OwnerController == nullptr) return;

	OwnerCapsule = OwnerController->GetCharacter()->GetCapsuleComponent();
	if (OwnerCapsule == nullptr) return;
	PlayerCapsuleHeight = OwnerCapsule->GetScaledCapsuleHalfHeight();
	PlayerCapsuleRadius = OwnerCapsule->GetScaledCapsuleRadius();
	
}


// Called every frame
void UClimbComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsClimbing) Climb(DeltaTime);

}

void UClimbComponent::Climb(float DeltaTime)
{
	FVector NewLocation;
	if (ClimbProgress < 1.f)
	{
		const float ClimbStep = (DeltaTime * ClimbSpeed) / ClimbLength;
		ClimbProgress += ClimbStep;
		ClimbProgress = FMath::Clamp<float>(ClimbProgress, 0.f, 1.f);

		NewLocation = FMath::Lerp(ClimbStartLocation, ClimbEndLocation, ClimbProgress);
	}
	else if (MoveForwardProgress < 1.f)
	{
		const float MoveForwardStep = (DeltaTime * ClimbSpeed) / ForwardDistanceAfterClimb;
		MoveForwardProgress += MoveForwardStep;
		MoveForwardProgress = FMath::Clamp<float>(MoveForwardProgress, 0.f, 1.f);

		NewLocation = FMath::Lerp(ClimbEndLocation, ForwardEndLocation, MoveForwardProgress);
	}
	else if (AfterClimbRestProgress < AfterClimbRestTime)
	{
		AfterClimbRestProgress += DeltaTime;
		return;
	}
	else
	{
		bIsClimbing = false;
		ClimbProgress = 0.f;
		MoveForwardProgress = 0.f;
		AfterClimbRestProgress = 0.f;
		ClimbStartLocation = FVector::ZeroVector;
		ClimbEndLocation = FVector::ZeroVector;
		ForwardEndLocation = FVector::ZeroVector;
		return;
	}
	OwnerController->GetPawn()->SetActorLocation(NewLocation);
}

void UClimbComponent::TryClimb()
{
	FVector Location = OwnerController->GetPawn()->GetActorLocation();
	FRotator Rotation = OwnerController->GetPawn()->GetActorForwardVector().Rotation();
	FVector ForwardEnd = Location + Rotation.Vector() * MaxRange;

	FCollisionShape Detector = FCollisionShape::MakeSphere(SurfaceDetectionRadius);
	FHitResult DetectedForwardSurface;
	bool bForwardHit = GetWorld()->SweepSingleByChannel(OUT DetectedForwardSurface, Location, ForwardEnd, Rotation.Quaternion(), ECollisionChannel::ECC_GameTraceChannel1, Detector);
	if (bForwardHit)
	{
		FVector ClimbMaxHeight = DetectedForwardSurface.ImpactPoint + FVector(0.f, 0.f, AvailableClimbHeight) + Rotation.Vector() * CheckingDownwardDistance;
		FVector DownwardEnd = DetectedForwardSurface.ImpactPoint + Rotation.Vector() * CheckingDownwardDistance;

		FHitResult DetectedUpSurface;
		bool bDownwardHit = GetWorld()->SweepSingleByChannel(OUT DetectedUpSurface, ClimbMaxHeight, DownwardEnd, Rotation.Quaternion(), ECollisionChannel::ECC_GameTraceChannel1, Detector);
		if (bDownwardHit)
		{
			if (DetectedUpSurface.Normal.Z > cosf(UKismetMathLibrary::DegreesToRadians(AvailableSlopeAngle)))
			{
				ClimbStartLocation = OwnerController->GetPawn()->GetActorLocation();
				ClimbEndLocation = ClimbStartLocation;
				ClimbEndLocation.Z = DetectedUpSurface.Location.Z + PlayerCapsuleHeight + UpDistanceAfterClimb;
				ClimbLength = (ClimbEndLocation - ClimbStartLocation).Size();

				TArray<AActor*> ActorsToIgnore{ GetOwner() };
				FCollisionQueryParams QueryParams;
				QueryParams.AddIgnoredActors(ActorsToIgnore);
			
				FCollisionShape PlayerCapsule = FCollisionShape::MakeCapsule(PlayerCapsuleRadius, PlayerCapsuleHeight);

				FHitResult CapsuleHitUp;
				GetWorld()->SweepSingleByChannel(OUT CapsuleHitUp, ClimbStartLocation, ClimbEndLocation, FQuat::Identity, ECC_WorldStatic, PlayerCapsule, QueryParams);

				ForwardEndLocation = ClimbEndLocation + Rotation.Vector() * ForwardDistanceAfterClimb;

				FHitResult CapsuleHitForward;
				GetWorld()->SweepSingleByChannel(OUT CapsuleHitForward, ClimbEndLocation, ForwardEndLocation, FQuat::Identity, ECC_WorldStatic, PlayerCapsule, QueryParams);

				if (!CapsuleHitUp.bBlockingHit && !CapsuleHitForward.bBlockingHit)
				{
					bIsClimbing = true;
				}
			}
		}
	}
}
