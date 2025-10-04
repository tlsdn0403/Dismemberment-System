// Copyright Epic Games, Inc. All Rights Reserved.


#include "TP_WeaponComponent.h"
#include "DismambermentCharacter.h"
#include "DismambermentProjectile.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

// Sets default values for this component's properties
UTP_WeaponComponent::UTP_WeaponComponent()
{
	// Default offset from the character location for projectiles to spawn
	MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);
}


//
void UTP_WeaponComponent::Fire()
{
    // 캐릭터 또는 컨트롤러가 없으면 발사를 중단 (안전장치)
    if (Character == nullptr || Character->GetController() == nullptr)
    {
        return;
    }

    // 발사체 클래스가 설정되어 있는지 확인
    if (ProjectileClass != nullptr)
    {
        UWorld* const World = GetWorld();
        if (World != nullptr)
        {
            // 플레이어 컨트롤러 가져오기 (카메라 회전을 얻기 위함)
            APlayerController* PlayerController = Cast<APlayerController>(Character->GetController());

            // 스폰 회전은 카메라(플레이어) 회전과 일치시킴
            const FRotator SpawnRotation = PlayerController->PlayerCameraManager->GetCameraRotation();

            // 스폰 위치 계산:
            // - MuzzleOffset은 "카메라 공간" 기준
            // - 따라서 카메라 회전(SpawnRotation)으로 월드 벡터로 변환 후
            // - GetOwner()의 월드 위치(보통 캐릭터 위치)에 더함
            // => 카메라 방향으로 살짝 앞/위에서 스폰됨
            const FVector SpawnLocation = GetOwner()->GetActorLocation() + SpawnRotation.RotateVector(MuzzleOffset);

            // 스폰 충돌 처리 설정:
            // - AdjustIfPossibleButDontSpawnIfColliding: 겹치면 스폰하지 않고, 가능하면 위치 조정
            FActorSpawnParameters ActorSpawnParams;
            ActorSpawnParams.SpawnCollisionHandlingOverride =
                ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

            if (Character)
            {
                // 실제 발사체 스폰
                ADismambermentProjectile* Projectile =
                    World->SpawnActor<ADismambermentProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);

                if (Projectile)
                {
                    // 발사자 정보를 전달(데미지 소유자 판정 등에 사용)
                    Projectile->SetShooter(Character);
                }
            }
            // NOTE: 스폰 실패(벽 너무 가까움 등) 시 아무 일도 안 일어날 수 있음
        }
    }

    // 발사 사운드 재생 (설정된 경우)
    if (FireSound != nullptr)
    {
        UGameplayStatics::PlaySoundAtLocation(this, FireSound, Character->GetActorLocation());
    }

    // 발사 애니메이션 재생 (설정된 경우)
    if (FireAnimation != nullptr)
    {
        // 1인칭 팔 메쉬의 AnimInstance를 얻어 몽타주 재생
        UAnimInstance* AnimInstance = Character->GetMesh1P()->GetAnimInstance();
        if (AnimInstance != nullptr)
        {
            AnimInstance->Montage_Play(FireAnimation, 1.f);
        }
    }
}

void UTP_WeaponComponent::AttachWeapon(ADismambermentCharacter* TargetCharacter)
{
    Character = TargetCharacter;
    if (Character == nullptr)
    {
        return;
    }

    // 무기를 1인칭 캐릭터의 "GripPoint" 소켓에 스냅 방식으로 부착
    FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
    AttachToComponent(Character->GetMesh1P(), AttachmentRules, FName(TEXT("GripPoint")));

    // 애님 블루프린트 전환용 플래그 (예: 소총 보유 시 다른 애님셋)
    Character->SetHasRifle(true);

    // 입력 바인딩 설정
    if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
    {
        // 로컬 플레이어 서브시스템에서 매핑 컨텍스트 추가 (우선순위 1)
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            // 우선순위를 1로 설정하여 특정 상황(터치 입력)에서 점프보다 Fire가 우선되게 함
            Subsystem->AddMappingContext(FireMappingContext, 1);
        }

        // 플레이어 입력 컴포넌트에서 FireAction을 이 컴포넌트의 Fire 함수에 바인딩
        if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerController->InputComponent))
        {
            // Triggered는 버튼을 누르고 있는 동안 매 틱 트리거될 수 있음(연사에 적합)
            // 단발 사격이면 Started로 바꾸는 것을 고려
            EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &UTP_WeaponComponent::Fire);
        }
    }
}

void UTP_WeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Character == nullptr)
	{
		return;
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(FireMappingContext);
		}
	}
}