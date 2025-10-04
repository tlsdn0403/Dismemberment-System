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
    // ĳ���� �Ǵ� ��Ʈ�ѷ��� ������ �߻縦 �ߴ� (������ġ)
    if (Character == nullptr || Character->GetController() == nullptr)
    {
        return;
    }

    // �߻�ü Ŭ������ �����Ǿ� �ִ��� Ȯ��
    if (ProjectileClass != nullptr)
    {
        UWorld* const World = GetWorld();
        if (World != nullptr)
        {
            // �÷��̾� ��Ʈ�ѷ� �������� (ī�޶� ȸ���� ��� ����)
            APlayerController* PlayerController = Cast<APlayerController>(Character->GetController());

            // ���� ȸ���� ī�޶�(�÷��̾�) ȸ���� ��ġ��Ŵ
            const FRotator SpawnRotation = PlayerController->PlayerCameraManager->GetCameraRotation();

            // ���� ��ġ ���:
            // - MuzzleOffset�� "ī�޶� ����" ����
            // - ���� ī�޶� ȸ��(SpawnRotation)���� ���� ���ͷ� ��ȯ ��
            // - GetOwner()�� ���� ��ġ(���� ĳ���� ��ġ)�� ����
            // => ī�޶� �������� ��¦ ��/������ ������
            const FVector SpawnLocation = GetOwner()->GetActorLocation() + SpawnRotation.RotateVector(MuzzleOffset);

            // ���� �浹 ó�� ����:
            // - AdjustIfPossibleButDontSpawnIfColliding: ��ġ�� �������� �ʰ�, �����ϸ� ��ġ ����
            FActorSpawnParameters ActorSpawnParams;
            ActorSpawnParams.SpawnCollisionHandlingOverride =
                ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

            if (Character)
            {
                // ���� �߻�ü ����
                ADismambermentProjectile* Projectile =
                    World->SpawnActor<ADismambermentProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);

                if (Projectile)
                {
                    // �߻��� ������ ����(������ ������ ���� � ���)
                    Projectile->SetShooter(Character);
                }
            }
            // NOTE: ���� ����(�� �ʹ� ����� ��) �� �ƹ� �ϵ� �� �Ͼ �� ����
        }
    }

    // �߻� ���� ��� (������ ���)
    if (FireSound != nullptr)
    {
        UGameplayStatics::PlaySoundAtLocation(this, FireSound, Character->GetActorLocation());
    }

    // �߻� �ִϸ��̼� ��� (������ ���)
    if (FireAnimation != nullptr)
    {
        // 1��Ī �� �޽��� AnimInstance�� ��� ��Ÿ�� ���
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

    // ���⸦ 1��Ī ĳ������ "GripPoint" ���Ͽ� ���� ������� ����
    FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
    AttachToComponent(Character->GetMesh1P(), AttachmentRules, FName(TEXT("GripPoint")));

    // �ִ� �������Ʈ ��ȯ�� �÷��� (��: ���� ���� �� �ٸ� �ִԼ�)
    Character->SetHasRifle(true);

    // �Է� ���ε� ����
    if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
    {
        // ���� �÷��̾� ����ý��ۿ��� ���� ���ؽ�Ʈ �߰� (�켱���� 1)
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            // �켱������ 1�� �����Ͽ� Ư�� ��Ȳ(��ġ �Է�)���� �������� Fire�� �켱�ǰ� ��
            Subsystem->AddMappingContext(FireMappingContext, 1);
        }

        // �÷��̾� �Է� ������Ʈ���� FireAction�� �� ������Ʈ�� Fire �Լ��� ���ε�
        if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerController->InputComponent))
        {
            // Triggered�� ��ư�� ������ �ִ� ���� �� ƽ Ʈ���ŵ� �� ����(���翡 ����)
            // �ܹ� ����̸� Started�� �ٲٴ� ���� ���
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