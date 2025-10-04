// �� ������ �𸮾� ������ ���� ĳ����(AZombie) ������ �����մϴ�.
// �ٽ� ���:
// - ������ ������(�� ����) ���� �� ������ ������ ���� ����(BreakConstraint)
// - ���� �� ���׵�(���� �ùķ��̼�)�� ��ȯ�ϰ� ĸ�� �浹�� ��
// - ���¸� "ũ�Ѹ�"���� ��ȯ�� ��, �Ӹ�/�ȿ� ���� ���� �־� ������ ���� ��
// - Ÿ�̸ӷ� ũ�Ѹ� ���� ���� �� �� ������ ������ ����

#include "Zombie.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Dismamberment/DismambermentCharacter.h"
#include "Dismamberment/DismambermentProjectile.h"

// Sets default values
AZombie::AZombie()
{
	// �� ĳ���Ͱ� �� ������ Tick()�� ȣ���ϵ��� ���� (ũ�Ѹ�/���� �� ���� ���� ���� �ʿ�)
	PrimaryActorTick.bCanEverTick = true;

	// ĳ������ ���̷�Ż �޽� ������Ʈ�� ���� (���� ���Ƿ� ����� ĳ��)
	SkeletalMesh = GetMesh();

	// ������ ������(ü��) �ʱ�ȭ: �ش� �� �̸��� ���� ������ ���� ����
	// �������� 0 ���ϰ� �Ǹ� �ش� ���� ����(�극��ũ) ó���մϴ�.
	BreakableBoneDurability.Add(FName("spine_01"), 20);
	BreakableBoneDurability.Add(FName("upperarm_r"), 15);
	BreakableBoneDurability.Add(FName("upperarm_l"), 15);
	BreakableBoneDurability.Add(FName("lowerarm_r"), 10);
	BreakableBoneDurability.Add(FName("lowerarm_l"), 10);
	BreakableBoneDurability.Add(FName("hand_r"), 5);
	BreakableBoneDurability.Add(FName("hand_l"), 5);
	BreakableBoneDurability.Add(FName("thigh_r"), 15);
	BreakableBoneDurability.Add(FName("thigh_l"), 15);
	BreakableBoneDurability.Add(FName("calf_r"), 10);
	BreakableBoneDurability.Add(FName("calf_l"), 10);
	BreakableBoneDurability.Add(FName("foot_r"), 5);
	BreakableBoneDurability.Add(FName("foot_l"), 5);

}

// Called when the game starts or when spawned
void AZombie::BeginPlay()
{
	Super::BeginPlay();
	// ���� �������� Ư���� �� �� ����
}

// Called every frame
void AZombie::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ���� ���°� "ũ�Ѹ�"�� ���� ũ�Ѹ� ������ ����
	if (ZombieState == EZombieState::EZS_Crawling) {

		if (!bIsCrawling) {
			// ���� ũ�Ѹ��� �������� �ʾҴٸ� 3�� ���� �� TimerEnd() ȣ���Ͽ� ���� �÷��׸� �մϴ�.
			// ����: �� ������ ���ο� �ڵ��� �����ϹǷ�, ��� FTimerHandle�� �ߺ� ������ �����ϸ� �� �����մϴ�.
			FTimerHandle UnusedHandle;
			GetWorldTimerManager().SetTimer(UnusedHandle, this, &AZombie::TimerEnd, 3.f, false);
		}
		else {
			// ũ�Ѹ� ���� �Ŀ��� ĸ�� ��ġ�� �Ӹ� ���Ͽ� ���߰�, �Ӹ�/�ȿ� ���� ���� ����
			SetCapsuleLocation();
			AddForceToBones();
		}
	}
}

// �ܺηκ��� �������� �޾��� �� ȣ��Ǵ� �Լ�(�߻�ü OnHit ��� ȣ���ϵ��� �����)
void AZombie::AnyDamage(int32 const Damage, FName HitBoneName, AActor* NewDamageCauser)
{
	// ����(������) �������� ���ߴ��� ���� (���޽�/�� ���� ��꿡 ���)
	DamageCauser = NewDamageCauser;

	// Ư�� ��ü ����(head/neck/pelvis/spine_02/03)�� spine_01�� �����Ͽ� ���� �������� �����ϵ��� ��
	RenameBoneName(HitBoneName);

	// �ش� ���� �������� Damage��ŭ ���ҽ�Ű��, 0 ���Ϸ� �������� true ��ȯ
	if (DealDamageToBone(HitBoneName, Damage)) {
		// ������ ���� �� ���� �� ���� ��ȯ(ũ�Ѹ�) �� ���� ���� ������ ó��
		Dismemberment(HitBoneName);
		ChangeMovementType();
		ApplyPhysics(HitBoneName);
	}
}

// Ư�� ���� ���� �׷�(spine_01)���� �����Ͽ� ������ ���� �ܼ�ȭ
void AZombie::RenameBoneName(FName& HitBoneName)
{
	if (HitBoneName == "pelvis" ||
		HitBoneName == "spine_02" ||
		HitBoneName == "spine_03" ||
		HitBoneName == "head" ||
		HitBoneName == "neck_01")
	{
		HitBoneName = "spine_01";
	}
}

// Return true if the bone has reach the durability limit
// �ش� ���� �������� ���ҽ�Ű��, 0 ���ϰ� �Ǹ� true(���� ���� ����) ��ȯ
bool AZombie::DealDamageToBone(FName const HitBoneName, int32 const Damage)
{
	UE_LOG(LogTemp, Warning, TEXT("I am %s"), *HitBoneName.ToString());

	// �ʿ� �ش� �� Ű�� ������ ���� ó��
	if (BreakableBoneDurability.Find(HitBoneName)) {
		// ���� �������� �о�� Damage��ŭ ����
		int32 NewHealth = *BreakableBoneDurability.Find(HitBoneName) - Damage;

		// 0 �����̸� ���� ���� ���� �� true ��ȯ
		if (NewHealth <= 0) {
			return true;
		}

		// ���� �����ִٸ� ����
		BreakableBoneDurability.Add(HitBoneName, NewHealth);
	}
	return false;
}

// ���� ���� ó��: �ش� �� ���� ����(Constraint)�� ��� �и� ȿ���� ��
void AZombie::Dismemberment(FName const HitBoneName)
{
	// CalculateImpulse()�� ����� ���޽��� ����� BreakConstraint�� ȣ��
	SkeletalMesh->BreakConstraint(CalculateImpulse(), GetSocketLocation(HitBoneName), HitBoneName);

	// ô��(spine_01)�� �μ������� ��� �� ���� ũ�Ѹ� �� ����ġ � ���
	if (HitBoneName == "spine_01") {
		bHasSpineBroken = true;
	}

	// �ı��� �� ��Ͽ� �߰� (�� ������ ���� �� �ش� ���� �μ������� �Ǵ�)
	BrokenBones.Add(HitBoneName);
}

// ũ�Ѹ� ���·� ��ȯ
void AZombie::ChangeMovementType()
{
	// ���� ���¿����� ũ�Ѹ����� ���� (�ߺ� ��ȯ ����)
	if (ZombieState == EZombieState::EZS_Normal) {
		ZombieState = EZombieState::EZS_Crawling;
	}
}

// ���� �ùķ��̼� ��ȯ �� ���޽� ����, ĸ�� �浹 ��Ȱ��ȭ
void AZombie::ApplyPhysics(FName const HitBoneName)
{
	// �Ʒ� ������ ���� �� �ϳ��� ���� ó��
	if (HitBoneName == "spine_01" ||
		HitBoneName == "upperarm_r" ||
		HitBoneName == "upperarm_l" ||
		HitBoneName == "lowerarm_r" ||
		HitBoneName == "lowerarm_l" ||
		HitBoneName == "hand_r" ||
		HitBoneName == "hand_l" ||
		HitBoneName == "thigh_r" ||
		HitBoneName == "thigh_l" ||
		HitBoneName == "calf_r" ||
		HitBoneName == "calf_l" ||
		HitBoneName == "foot_r" ||
		HitBoneName == "foot_l")
	{
		// ���� ���� �ùķ��̼� Ȱ��ȭ(���׵�). �κ� ����ȭ�� ���ϸ� SetAllBodiesBelowSimulatePhysics ��� ����.
		SkeletalMesh->SetSimulatePhysics(true);

		// ���� ���� ��ġ�� ���޽� ����(ƨ�ܳ����� ȿ��)
		SkeletalMesh->AddImpulseAtLocation(CalculateImpulse(), GetSocketLocation(HitBoneName), HitBoneName);

		// ĸ�� �浹�� ����(������ ������ �����̹Ƿ�), Tick���� ĸ�� ��ġ�� �Ӹ��� �ٿ��� ����
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

// ���׵� ��ȯ ��, ĸ���� �� ������ �Ӹ� ���� ��ġ�� �Ű� ��Ʈ�� �ð������� ��ġ��Ű�� ����
void AZombie::SetCapsuleLocation()
{
	GetCapsuleComponent()->SetWorldLocation(SkeletalMesh->GetSocketLocation(FName("head")));
}

// �Ӹ�/�ȿ� �ֱ������� ���� ���� ������ ���� �ϴ� ����
void AZombie::AddForceToBones()
{
	// �Ӹ����� �׻� ���� ���� ��(�� + ��)�� ����
	SkeletalMesh->AddForce(CalaculateCrawlingForce(500.f, 2000.f), FName("head"), true);
	UE_LOG(LogTemp, Warning, TEXT("Head Moving"));

	// bCrawl�� true�� ���� �� ����(ū ��)�� �� �� �����ϰ� �ٽ� false�� ������ ������ ��
	if (bCrawl) {
		bCrawl = false;

		// �����Ȱ� ������ ���� �� ��� (���� �μ������� �˻翡 ���)
		TArray<FName> RightBones = { "upperarm_r", "lowerarm_r", "hand_r" };
		TArray<FName> LeftBones = { "upperarm_l", "lowerarm_l", "hand_l" };

		FTimerHandle UnusedHandle;

		// �������� �����ϰ�(broken �ƴ�) ���� ���ʰ� �������̸� �����տ� ū �� ����
		if (!FindBrokenBones(RightBones) && bRightArm) {
			UE_LOG(LogTemp, Warning, TEXT("Right Arm Not Broken"));
			SkeletalMesh->AddForce(CalaculateCrawlingForce(3000.f, 2000.f), FName("hand_r"), true);
			// ���� ���ʴ� ����
			bRightArm = false;
			bLeftArm = true;
		}
		// ������ �����ϰ� ���� ���ʰ� �����̸� �޼տ� ū �� ����
		else if (!FindBrokenBones(LeftBones) && bLeftArm) {
			UE_LOG(LogTemp, Warning, TEXT("Left Arm Not Broken"));
			SkeletalMesh->AddForce(CalaculateCrawlingForce(3000.f, 2000.f), FName("hand_l"), true);
			// ���� ���ʴ� ������
			bLeftArm = false;
			bRightArm = true;
		}

		// 3�� �� TimerEnd ȣ�� �� bCrawl�� �ٽ� true�� ����� ���� �� ���� ���
		// ����: �� Ÿ�̸ӵ� �Ź� �� �ڵ��� ����Ƿ�, ��� �ڵ�� �����ϸ� ����մϴ�.
		GetWorldTimerManager().SetTimer(UnusedHandle, this, &AZombie::TimerEnd, 3, false);
	}
}

// Ÿ�̸� �ݹ�: ù ȣ�� �� bIsCrawling�� true�� ����� ũ�Ѹ� ����,
// ���� ȣ�⿡���� bCrawl�� true�� ����� ���� �� ������ Ʈ����
void AZombie::TimerEnd()
{
	if (!bIsCrawling) {
		bIsCrawling = true;  // �ʱ� 1ȸ: ũ�Ѹ� ���� ��ȣ
	}
	bCrawl = true;           // �� ���� ���� ���·� ��ȯ
}

// ���޵� �� ��� �� �ϳ��� BrokenBones�� ������ true ��ȯ(�ش� ���� �μ������� Ȯ�ο� ���)
bool AZombie::FindBrokenBones(TArray<FName> Bones)
{
	for (FName Bone : Bones) {
		if (BrokenBones.Contains(Bone)) {
			return true;
		}
	}
	return false;
}

// ����/���޽��� ����� �� ���� ���
// ���� ����: �÷��̾� 0�� ī�޶� ���麤�͸� ū ��Į��(������)�� ������ + ���� ������ ���� ���� ��ȯ
// ����: ��Ƽ�÷���/���� �پ�ȭ �� DamageCauser/Instigator �������� ������ �ٲٴ� ���� �� �ڿ������� �� �ֽ��ϴ�.
FVector AZombie::CalculateImpulse()
{
	if (GetWorld()) {
		return (UKismetMathLibrary::GetForwardVector(UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0)->GetCameraRotation()) *
			FMath::RandRange(80000, 120000))
			+ (GetActorUpVector() * FMath::RandRange(5000, 8000));
	}
	return FVector(); // ���尡 ������ 0 ����
}

// ũ�Ѹ� �� �Ӹ�/�տ� ���� �� ���� ���
// - ForwardForce: �� ���� ���� ũ��
// - UpwardForce: �� ���� ���� ũ��
// - DamageCauser(���� �߻�ü)�κ��� Shooter�� ã��, "���� ĸ�� �� Shooter" �������� ���� ���ϴ� ���
// - ô�߰� �����ϸ�(= bHasSpineBroken == false) ����ġ(Weight)�� 1.2�� �÷� �� ū ���� �ֵ��� ����
FVector AZombie::CalaculateCrawlingForce(float const ForwardForce, float const UpwardForce)
{
	FVector Result; // �⺻ 0,0,0 (�ʱ�ȭ �� �Ǿ� �����Ƿ� �Ʒ��� �ݵ�� ���õ�)
	if (DamageCauser) {
		FVector ForceLocation; // �� ����(����ȭ ����) �����

		// ������ �����ڸ� �߻�ü�� ĳ���� (���� �� nullptr)
		ADismambermentProjectile* Projectile = Cast<ADismambermentProjectile>(DamageCauser);

		// ����: Projectile�� nullptr�� ���� �����Ƿ� �� üũ�� �ʿ��մϴ�.
		// �� �ڵ忡���� Shooter ���θ� üũ�ϹǷ� ĳ���� ���� �� �浹 ����.
		if (Projectile && Projectile->GetShooter()) {
			// ���� ĸ�� ��ġ���� Shooter(�߻���) ��ġ�� �ٶ󺸴� ȸ�� �� �� ���� ����(����ȭ)�� ���� �� �������� ���
			ForceLocation = UKismetMathLibrary::GetForwardVector(
				UKismetMathLibrary::FindLookAtRotation(GetCapsuleComponent()->GetComponentLocation(),
					Projectile->GetShooter()->GetActorLocation()));
			UE_LOG(LogTemp, Warning, TEXT("Following Actor is %s"), *Projectile->GetShooter()->GetName());
		}
		// ũ�Ѹ� ���� ����ġ: ô�߰� �����ϸ� �� ū ��
		float Weight = 1.f;
		if (!bHasSpineBroken) {
			Weight = 1.2f;
		}
		float UpwardForceWeight = Weight * UpwardForce;
		float ForwardForceWeight = Weight * ForwardForce;

		// �� ���� ���� + ���� ���� ����
		Result = ForwardForceWeight * ForceLocation;
		Result = Result + FVector(0.f, 0.f, UpwardForceWeight);
	}
	return Result;
}

// Ư�� ��(����)�� ���� ��ġ�� ��ȯ (���޽�/�� ���� ���� ��꿡 ���)
FVector AZombie::GetSocketLocation(FName const HitBoneName)
{
	return SkeletalMesh->GetSocketLocation(HitBoneName);
}