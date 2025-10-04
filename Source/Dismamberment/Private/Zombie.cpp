// 이 파일은 언리얼 엔진의 좀비 캐릭터(AZombie) 동작을 정의합니다.
// 핵심 기능:
// - 부위별 내구도(뼈 단위) 관리 → 데미지 누적에 따라 절단(BreakConstraint)
// - 절단 시 래그돌(물리 시뮬레이션)로 전환하고 캡슐 충돌을 끔
// - 상태를 "크롤링"으로 전환한 뒤, 머리/팔에 물리 힘을 주어 앞으로 기어가게 함
// - 타이머로 크롤링 시작 지연 및 팔 동작의 간격을 제어

#include "Zombie.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Dismamberment/DismambermentCharacter.h"
#include "Dismamberment/DismambermentProjectile.h"

// Sets default values
AZombie::AZombie()
{
	// 이 캐릭터가 매 프레임 Tick()을 호출하도록 설정 (크롤링/물리 힘 적용 등을 위해 필요)
	PrimaryActorTick.bCanEverTick = true;

	// 캐릭터의 스켈레탈 메시 컴포넌트를 저장 (자주 쓰므로 멤버로 캐시)
	SkeletalMesh = GetMesh();

	// 부위별 내구도(체력) 초기화: 해당 뼈 이름과 남은 내구도 값을 매핑
	// 내구도가 0 이하가 되면 해당 뼈를 절단(브레이크) 처리합니다.
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
	// 시작 시점에서 특별히 할 일 없음
}

// Called every frame
void AZombie::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 좀비 상태가 "크롤링"일 때만 크롤링 로직을 수행
	if (ZombieState == EZombieState::EZS_Crawling) {

		if (!bIsCrawling) {
			// 아직 크롤링을 시작하지 않았다면 3초 지연 후 TimerEnd() 호출하여 시작 플래그를 켭니다.
			// 주의: 매 프레임 새로운 핸들을 생성하므로, 멤버 FTimerHandle로 중복 예약을 방지하면 더 안전합니다.
			FTimerHandle UnusedHandle;
			GetWorldTimerManager().SetTimer(UnusedHandle, this, &AZombie::TimerEnd, 3.f, false);
		}
		else {
			// 크롤링 시작 후에는 캡슐 위치를 머리 소켓에 맞추고, 머리/팔에 힘을 가해 전진
			SetCapsuleLocation();
			AddForceToBones();
		}
	}
}

// 외부로부터 데미지를 받았을 때 호출되는 함수(발사체 OnHit 등에서 호출하도록 설계됨)
void AZombie::AnyDamage(int32 const Damage, FName HitBoneName, AActor* NewDamageCauser)
{
	// 누가(무엇이) 데미지를 가했는지 저장 (임펄스/힘 방향 계산에 사용)
	DamageCauser = NewDamageCauser;

	// 특정 상체 부위(head/neck/pelvis/spine_02/03)는 spine_01로 통합하여 같은 내구도를 공유하도록 함
	RenameBoneName(HitBoneName);

	// 해당 뼈의 내구도를 Damage만큼 감소시키고, 0 이하로 떨어지면 true 반환
	if (DealDamageToBone(HitBoneName, Damage)) {
		// 내구도 소진 시 절단 → 상태 전환(크롤링) → 물리 적용 순서로 처리
		Dismemberment(HitBoneName);
		ChangeMovementType();
		ApplyPhysics(HitBoneName);
	}
}

// 특정 뼈를 공통 그룹(spine_01)으로 매핑하여 내구도 관리 단순화
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
// 해당 뼈의 내구도를 감소시키고, 0 이하가 되면 true(절단 조건 충족) 반환
bool AZombie::DealDamageToBone(FName const HitBoneName, int32 const Damage)
{
	UE_LOG(LogTemp, Warning, TEXT("I am %s"), *HitBoneName.ToString());

	// 맵에 해당 뼈 키가 존재할 때만 처리
	if (BreakableBoneDurability.Find(HitBoneName)) {
		// 현재 내구도를 읽어와 Damage만큼 감소
		int32 NewHealth = *BreakableBoneDurability.Find(HitBoneName) - Damage;

		// 0 이하이면 절단 조건 충족 → true 반환
		if (NewHealth <= 0) {
			return true;
		}

		// 아직 남아있다면 갱신
		BreakableBoneDurability.Add(HitBoneName, NewHealth);
	}
	return false;
}

// 실제 절단 처리: 해당 뼈 관절 제약(Constraint)을 끊어서 분리 효과를 냄
void AZombie::Dismemberment(FName const HitBoneName)
{
	// CalculateImpulse()로 계산한 임펄스를 사용해 BreakConstraint를 호출
	SkeletalMesh->BreakConstraint(CalculateImpulse(), GetSocketLocation(HitBoneName), HitBoneName);

	// 척추(spine_01)가 부서졌음을 기록 → 이후 크롤링 힘 가중치 등에 사용
	if (HitBoneName == "spine_01") {
		bHasSpineBroken = true;
	}

	// 파괴된 뼈 목록에 추가 (팔 번갈이 동작 시 해당 팔이 부서졌는지 판단)
	BrokenBones.Add(HitBoneName);
}

// 크롤링 상태로 전환
void AZombie::ChangeMovementType()
{
	// 정상 상태에서만 크롤링으로 변경 (중복 전환 방지)
	if (ZombieState == EZombieState::EZS_Normal) {
		ZombieState = EZombieState::EZS_Crawling;
	}
}

// 물리 시뮬레이션 전환 및 임펄스 적용, 캡슐 충돌 비활성화
void AZombie::ApplyPhysics(FName const HitBoneName)
{
	// 아래 나열된 부위 중 하나일 때만 처리
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
		// 전신 물리 시뮬레이션 활성화(래그돌). 부분 물리화를 원하면 SetAllBodiesBelowSimulatePhysics 사용 가능.
		SkeletalMesh->SetSimulatePhysics(true);

		// 맞은 부위 위치에 임펄스 적용(튕겨나가는 효과)
		SkeletalMesh->AddImpulseAtLocation(CalculateImpulse(), GetSocketLocation(HitBoneName), HitBoneName);

		// 캡슐 충돌을 끄고(전신이 물리로 움직이므로), Tick에서 캡슐 위치를 머리에 붙여서 추적
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

// 래그돌 전환 후, 캡슐을 매 프레임 머리 소켓 위치로 옮겨 루트와 시각적으로 일치시키는 보정
void AZombie::SetCapsuleLocation()
{
	GetCapsuleComponent()->SetWorldLocation(SkeletalMesh->GetSocketLocation(FName("head")));
}

// 머리/팔에 주기적으로 힘을 가해 앞으로 기어가게 하는 로직
void AZombie::AddForceToBones()
{
	// 머리에는 항상 비교적 작은 힘(앞 + 위)을 가함
	SkeletalMesh->AddForce(CalaculateCrawlingForce(500.f, 2000.f), FName("head"), true);
	UE_LOG(LogTemp, Warning, TEXT("Head Moving"));

	// bCrawl이 true일 때만 팔 동작(큰 힘)을 한 번 실행하고 다시 false로 내려서 간격을 둠
	if (bCrawl) {
		bCrawl = false;

		// 오른팔과 왼팔의 관련 뼈 목록 (팔이 부서졌는지 검사에 사용)
		TArray<FName> RightBones = { "upperarm_r", "lowerarm_r", "hand_r" };
		TArray<FName> LeftBones = { "upperarm_l", "lowerarm_l", "hand_l" };

		FTimerHandle UnusedHandle;

		// 오른팔이 멀쩡하고(broken 아님) 현재 차례가 오른팔이면 오른손에 큰 힘 적용
		if (!FindBrokenBones(RightBones) && bRightArm) {
			UE_LOG(LogTemp, Warning, TEXT("Right Arm Not Broken"));
			SkeletalMesh->AddForce(CalaculateCrawlingForce(3000.f, 2000.f), FName("hand_r"), true);
			// 다음 차례는 왼팔
			bRightArm = false;
			bLeftArm = true;
		}
		// 왼팔이 멀쩡하고 현재 차례가 왼팔이면 왼손에 큰 힘 적용
		else if (!FindBrokenBones(LeftBones) && bLeftArm) {
			UE_LOG(LogTemp, Warning, TEXT("Left Arm Not Broken"));
			SkeletalMesh->AddForce(CalaculateCrawlingForce(3000.f, 2000.f), FName("hand_l"), true);
			// 다음 차례는 오른팔
			bLeftArm = false;
			bRightArm = true;
		}

		// 3초 뒤 TimerEnd 호출 → bCrawl을 다시 true로 만들어 다음 팔 동작 허용
		// 주의: 이 타이머도 매번 새 핸들을 만들므로, 멤버 핸들로 관리하면 깔끔합니다.
		GetWorldTimerManager().SetTimer(UnusedHandle, this, &AZombie::TimerEnd, 3, false);
	}
}

// 타이머 콜백: 첫 호출 시 bIsCrawling을 true로 만들어 크롤링 시작,
// 이후 호출에서는 bCrawl을 true로 만들어 다음 팔 동작을 트리거
void AZombie::TimerEnd()
{
	if (!bIsCrawling) {
		bIsCrawling = true;  // 초기 1회: 크롤링 시작 신호
	}
	bCrawl = true;           // 팔 동작 가능 상태로 전환
}

// 전달된 뼈 목록 중 하나라도 BrokenBones에 있으면 true 반환(해당 팔이 부서졌는지 확인에 사용)
bool AZombie::FindBrokenBones(TArray<FName> Bones)
{
	for (FName Bone : Bones) {
		if (BrokenBones.Contains(Bone)) {
			return true;
		}
	}
	return false;
}

// 절단/임펄스에 사용할 힘 벡터 계산
// 현재 구현: 플레이어 0의 카메라 정면벡터를 큰 스칼라(무작위)로 스케일 + 위쪽 성분을 조금 더해 반환
// 주의: 멀티플레이/시점 다양화 시 DamageCauser/Instigator 기준으로 방향을 바꾸는 것이 더 자연스러울 수 있습니다.
FVector AZombie::CalculateImpulse()
{
	if (GetWorld()) {
		return (UKismetMathLibrary::GetForwardVector(UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0)->GetCameraRotation()) *
			FMath::RandRange(80000, 120000))
			+ (GetActorUpVector() * FMath::RandRange(5000, 8000));
	}
	return FVector(); // 월드가 없으면 0 벡터
}

// 크롤링 시 머리/손에 가할 힘 벡터 계산
// - ForwardForce: 앞 방향 성분 크기
// - UpwardForce: 위 방향 성분 크기
// - DamageCauser(보통 발사체)로부터 Shooter를 찾아, "좀비 캡슐 → Shooter" 방향으로 힘을 가하는 방식
// - 척추가 멀쩡하면(= bHasSpineBroken == false) 가중치(Weight)를 1.2로 올려 더 큰 힘을 주도록 설계
FVector AZombie::CalaculateCrawlingForce(float const ForwardForce, float const UpwardForce)
{
	FVector Result; // 기본 0,0,0 (초기화 안 되어 있으므로 아래서 반드시 세팅됨)
	if (DamageCauser) {
		FVector ForceLocation; // 앞 방향(정규화 벡터) 저장용

		// 데미지 유발자를 발사체로 캐스팅 (실패 시 nullptr)
		ADismambermentProjectile* Projectile = Cast<ADismambermentProjectile>(DamageCauser);

		// 주의: Projectile이 nullptr일 수도 있으므로 널 체크가 필요합니다.
		// 현 코드에서는 Shooter 여부만 체크하므로 캐스팅 실패 시 충돌 가능.
		if (Projectile && Projectile->GetShooter()) {
			// 좀비 캡슐 위치에서 Shooter(발사자) 위치를 바라보는 회전 → 그 정면 벡터(정규화)를 구해 앞 방향으로 사용
			ForceLocation = UKismetMathLibrary::GetForwardVector(
				UKismetMathLibrary::FindLookAtRotation(GetCapsuleComponent()->GetComponentLocation(),
					Projectile->GetShooter()->GetActorLocation()));
			UE_LOG(LogTemp, Warning, TEXT("Following Actor is %s"), *Projectile->GetShooter()->GetName());
		}
		// 크롤링 힘의 가중치: 척추가 멀쩡하면 더 큰 힘
		float Weight = 1.f;
		if (!bHasSpineBroken) {
			Weight = 1.2f;
		}
		float UpwardForceWeight = Weight * UpwardForce;
		float ForwardForceWeight = Weight * ForwardForce;

		// 앞 방향 성분 + 위쪽 성분 결합
		Result = ForwardForceWeight * ForceLocation;
		Result = Result + FVector(0.f, 0.f, UpwardForceWeight);
	}
	return Result;
}

// 특정 뼈(소켓)의 월드 위치를 반환 (임펄스/힘 적용 지점 계산에 사용)
FVector AZombie::GetSocketLocation(FName const HitBoneName)
{
	return SkeletalMesh->GetSocketLocation(HitBoneName);
}