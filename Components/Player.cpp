// Copyright 2017-2020 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "Player.h"
#include "GamePlugin.h"
#include <CryPhysics/RayCastQueue.h>
#include <CryPhysics/physinterface.h>
#include <CryEntitySystem/IEntitySystem.h>

#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

#include <DefaultComponents/Cameras/CameraComponent.h>
#include <DefaultComponents/Input/InputComponent.h>
#include <DefaultComponents/Physics/CharacterControllerComponent.h>
#include <DefaultComponents/Geometry/AdvancedAnimationComponent.h>

namespace
{
	static void RegisterPlayerComponent(Schematyc::IEnvRegistrar& registrar)
	{
		Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CPlayerComponent));
		}
	}

	CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterPlayerComponent);
}

CPlayerComponent::CPlayerComponent() :
	m_pCameraComponent(nullptr),
	m_pInputComponent(nullptr),
	m_pCharacterController(nullptr),
	m_pAdvancedAnimationComponent(nullptr),
	m_currentYaw(IDENTITY),
	m_currentPitch(0.f),
	m_movementDelta(ZERO),
	m_mouseDeltaRotation(ZERO),
	m_currentPlayerState(DEFAULT_STATE),
	m_currentStance(DEFAULT_STANCE)
	, m_desiredStance(DEFAULT_STANCE)
	, m_cameraEndOffset(Vec3(0.f, 0.f, DEFAULT_CAMERA_HEIGHT_STANDING))
	, m_cameraOffsetStanding(Vec3(0.f, 0.f, DEFAULT_CAMERA_HEIGHT_STANDING))
	, m_cameraOffsetCrouching(Vec3(0.f, 0.f, DEFAULT_CAMERA_HEIGHT_CROUCHING))
	, m_capsuleHeightStanding(DEFAULT_CAPSULE_HEIGHT_STANDING)
	, m_capsuleHeightCrouching(DEFAULT_CAPSULE_HEIGHT_CROUCHING)
	, m_capsuleGroundOffset(DEFAULT_CAPSULE_GROUND_OFFSET),
	m_rotationSpeed(DEFAULT_ROTATION_SPEED),
	m_walkSpeed(DEFAULT_SPEED_WALKING),
	m_runSpeed(DEFAULT_SPEED_RUNNING),
	m_jumpheight(DEFAULT_JUMP_ENERGY),
	m_rotationLimitsMaxPitch(DEFAULT_ROT_LIMIT_PITCH_MAX),
	m_rotationLimitsMinPitch(DEFAULT_ROT_LIMIT_PITCH_MIN)

{}

void CPlayerComponent::Initialize()
{
	m_pCameraComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCameraComponent>();
	m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();
	m_pCharacterController = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCharacterControllerComponent>();
	m_pAdvancedAnimationComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CAdvancedAnimationComponent>();

	Reset();
}

void CPlayerComponent::Reset()
{
	InitializeInput();

	m_currentPlayerState = EPlayerState::Walking;
	m_currentStance = EPlayerStance::Standing;
	m_desiredStance = m_currentStance;

	m_movementDelta = ZERO;
	m_mouseDeltaRotation = ZERO;
	m_currentYaw = Quat::CreateRotationZ(m_pEntity->GetWorldRotation().GetRotZ());
	m_currentPitch = 0.f;

	m_cameraEndOffset = m_cameraOffsetStanding;
}

void CPlayerComponent::RecenterCollider()
{
	static bool skip = false;
	if (skip)
	{
		skip = false;
		return;
	}

	auto pCharacterController = m_pEntity->GetComponent<Cry::DefaultComponents::CCharacterControllerComponent>();
	if (pCharacterController == nullptr)
	{
		return;
	}
	const auto& physParams = pCharacterController->GetPhysicsParameters();
	float heightOffset = physParams.m_height * 0.5f;

	if (physParams.m_bCapsule)
	{
		heightOffset = heightOffset * 0.5f + physParams.m_radius * 0.5f;
	}

	pCharacterController->SetTransformMatrix(Matrix34(IDENTITY, Vec3(0.f, 0.f, 0.005f + heightOffset)));
	skip = true;
	pCharacterController->Physicalize();

}

void CPlayerComponent::InitializeInput()
{
	m_pInputComponent->RegisterAction("player", "moveforward", [this](int activationMode, float value) {m_movementDelta.y = value; });
	m_pInputComponent->BindAction("player", "moveforward", eAID_KeyboardMouse, eKI_W);

	m_pInputComponent->RegisterAction("player", "moveback", [this](int activationMode, float value) {m_movementDelta.y = -value; });
	m_pInputComponent->BindAction("player", "moveback", eAID_KeyboardMouse, eKI_S);

	m_pInputComponent->RegisterAction("player", "moveright", [this](int activationMode, float value) {m_movementDelta.x = value; });
	m_pInputComponent->BindAction("player", "moveright", eAID_KeyboardMouse, eKI_D);

	m_pInputComponent->RegisterAction("player", "moveleft", [this](int activationMode, float value) {m_movementDelta.x = -value; });
	m_pInputComponent->BindAction("player", "moveleft", eAID_KeyboardMouse, eKI_A);

	m_pInputComponent->RegisterAction("player", "sprint", [this](int activationMode, float value) 
		{
			if (activationMode == (int)eAAM_OnPress && m_currentStance == EPlayerStance::Standing)
			{ 
				m_currentPlayerState = EPlayerState::Sprinting;
			}
			else if (activationMode == eAAM_OnRelease)
			{ 
				m_currentPlayerState = EPlayerState::Walking;
			}
		});
	m_pInputComponent->BindAction("player", "sprint", eAID_KeyboardMouse, eKI_LShift);

	m_pInputComponent->RegisterAction("player", "jump", [this](int activationMode, float value)
		{
			if (m_pCharacterController->IsOnGround() && activationMode == eAAM_OnPress)
			{
				m_pCharacterController->AddVelocity(Vec3(0.0f, 0.0f, m_jumpheight));
			}
			
		});
	m_pInputComponent->BindAction("player", "jump", eAID_KeyboardMouse, eKI_Space);

	m_pInputComponent->RegisterAction("player", "crouch", [this](int activationMode, float value)
		{
			if (m_pCharacterController->IsOnGround() && activationMode == eAAM_OnPress)
			{
				m_desiredStance = EPlayerStance::Crouching;
				m_currentPlayerState = EPlayerState::Walking;
			}
			else if (activationMode == (int)eAAM_OnRelease)
			{
				m_desiredStance = EPlayerStance::Standing;
			}

		});
	m_pInputComponent->BindAction("player", "crouch", eAID_KeyboardMouse, eKI_LCtrl);

	m_pInputComponent->RegisterAction("player", "yaw", [this](int activationMode, float value) {m_mouseDeltaRotation.y = -value; });
	m_pInputComponent->BindAction("player", "yaw", eAID_KeyboardMouse, eKI_MouseY);

	m_pInputComponent->RegisterAction("player", "pitch", [this](int activationMode, float value) {m_mouseDeltaRotation.x = -value; });
	m_pInputComponent->BindAction("player", "pitch", eAID_KeyboardMouse, eKI_MouseX);
}

Cry::Entity::EventFlags CPlayerComponent::GetEventMask() const
{
	return Cry::Entity::EEvent::GameplayStarted | Cry::Entity::EEvent::Update | Cry::Entity::EEvent::Reset | Cry::Entity::EEvent::EditorPropertyChanged | Cry::Entity::EEvent::PhysicalTypeChanged;
}

void CPlayerComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case Cry::Entity::EEvent::GameplayStarted:
	{
		Reset();
	}
	break;
	case Cry::Entity::EEvent::Update:
	{
		const float frametime = event.fParam[0];

		TryUpdateStance();
		UpdateMovement();
		UpdateRotation();
		UpdateCamera(frametime);
		IsWall();
		StartWallRun();
	}break;

	case Cry::Entity::EEvent::PhysicalTypeChanged:
	{
		RecenterCollider();
	} break;

	case Cry::Entity::EEvent::EditorPropertyChanged:
	case Cry::Entity::EEvent::Reset:
	{
		Reset();
	} break;

	}
}

void CPlayerComponent::TryUpdateStance()
{
	if (m_desiredStance == m_currentStance)
		return;

	IPhysicalEntity* pPhysEnt = m_pEntity->GetPhysicalEntity();

	if (pPhysEnt == nullptr)
		return;

	const float radius = m_pCharacterController->GetPhysicsParameters().m_radius * 0.5f;

	float height = 0.f;
	Vec3 camOffset = ZERO;
	switch (m_desiredStance)
	{
	case EPlayerStance::Crouching:
	{
		height = m_capsuleHeightCrouching;
		camOffset = m_cameraOffsetCrouching;
	} break;

	case EPlayerStance::Standing:
	{
		height = m_capsuleHeightStanding;
		camOffset = m_cameraOffsetStanding;

		primitives::capsule capsule;

		capsule.axis.Set(0, 0, 1);

		capsule.center = m_pEntity->GetWorldPos() + Vec3(0, 0, m_capsuleGroundOffset + radius + height * 0.5f);
		capsule.r = radius;
		capsule.hh = height * 0.5f;

		if (IsCapsuleIntersectingGeometry(capsule))
		{
			return;
		}
	}break;
	}
	pe_player_dimensions playerDimensions;
	pPhysEnt->GetParams(&playerDimensions);

	playerDimensions.heightCollider = m_capsuleGroundOffset + radius + height * 0.5f;
	playerDimensions.sizeCollider = Vec3(radius, radius, height * 0.5f);
	m_cameraEndOffset = camOffset;
	m_currentStance = m_desiredStance;

	pPhysEnt->SetParams(&playerDimensions);
}

bool CPlayerComponent::IsCapsuleIntersectingGeometry(const primitives::capsule& capsule) const
{
	IPhysicalEntity* pPhysEnt = m_pEntity->GetPhysicalEntity();

	if (pPhysEnt == nullptr)
		return false;

	IPhysicalWorld::SPWIParams pwiParams;

	pwiParams.itype = capsule.type;
	pwiParams.pprim = &capsule;

	pwiParams.pSkipEnts = &pPhysEnt;
	pwiParams.nSkipEnts = 1;

	intersection_params intersectionParams;
	intersectionParams.bSweepTest = false;
	pwiParams.pip = &intersectionParams;

	const int contactCount = static_cast<int>(gEnv->pPhysicalWorld -> PrimitiveWorldIntersection(pwiParams));
	return contactCount > 0;
}

void CPlayerComponent::IsWall()
{
	const float halfRenderWidth = static_cast<float>(gEnv->pRenderer->GetWidth()) * 0.5f;
	const float halfRenderHeight = static_cast<float>(gEnv->pRenderer->GetHeight()) * 0.5f;

	const float searchRange = 0.5f;
	Vec3 cameraCenterNear, cameraCenterFar;
	gEnv->pRenderer->UnProjectFromScreen(halfRenderWidth, halfRenderHeight, 0, &cameraCenterNear.x, &cameraCenterNear.y, &cameraCenterNear.z);
	gEnv->pRenderer->UnProjectFromScreen(halfRenderWidth, halfRenderHeight, 1, &cameraCenterFar.x, &cameraCenterFar.y, &cameraCenterFar.z);

	// Calculate the left and right direction
	Vec3 playerRightDir = m_pEntity->GetRightDir().GetNormalized();
	Vec3 checkLeft = -playerRightDir * searchRange;
	Vec3 checkRight = playerRightDir * searchRange;

	std::array<ray_hit, 1> hits;
	const uint32 queryFlags = ent_static;
	const uint32 rayFlags = rwi_stop_at_pierceable;

	// Exclude the player entity from the raycast query
	const int numHitsLeft = gEnv->pPhysicalWorld->RayWorldIntersection(cameraCenterNear, checkLeft, queryFlags, rayFlags, hits.data(), hits.max_size(), m_pEntity->GetPhysicalEntity());
	const int numHitsRight = gEnv->pPhysicalWorld->RayWorldIntersection(cameraCenterNear, checkRight, queryFlags, rayFlags, hits.data(), hits.max_size(), m_pEntity->GetPhysicalEntity());


	if (numHitsLeft > 0 || numHitsRight > 0)
	{
		const ray_hit& hit = hits[0];
		IPhysicalEntity* pCollider = hit.pCollider;
		IEntity* pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pCollider);

		if (pEntity)
		{
			const char* objectName = pEntity->GetName();
			if (strcmp(objectName, "wallrunnable") == 0 && !m_pCharacterController->IsOnGround())
			{
				wallerman = true;
			}
		}
	}

	// The raycast did not hit a wall or the object is not "wallrunnable"
	wallerman = false;
}

void CPlayerComponent::StartWallRun()
{
	if (!m_pCharacterController->IsOnGround() && wallerman)
		//CryLogAlways("LEFT Wall detected");
		TurnOffGravity();
}
void CPlayerComponent::TurnOffGravity()
{
	IPhysicalEntity* pPlayerPhysicalEntity = m_pEntity->GetPhysicalEntity();
	

	if (pPlayerPhysicalEntity)
	{
		pe_simulation_params simParams;

		simParams.gravity.zero();
		pPlayerPhysicalEntity->SetParams(&simParams);
	}
}

void CPlayerComponent::StopWallRun()
{

}


void CPlayerComponent::UpdateMovement()
{
	Vec3 velocity = Vec3(m_movementDelta.x, m_movementDelta.y, 0.0f);
	velocity.Normalize();
	const float playerMoveSpeed = m_currentPlayerState == EPlayerState::Sprinting ? m_runSpeed : m_walkSpeed;
	m_pCharacterController->SetVelocity(m_pEntity->GetWorldRotation() * velocity * playerMoveSpeed);
}

void CPlayerComponent::UpdateRotation()
{
	m_currentYaw *= Quat::CreateRotationZ(m_mouseDeltaRotation.x * m_rotationSpeed);
	m_pEntity->SetRotation(m_currentYaw);
}

void CPlayerComponent::UpdateCamera(float frametime)
{
	m_currentPitch = crymath::clamp(m_currentPitch + m_mouseDeltaRotation.y * m_rotationSpeed, m_rotationLimitsMinPitch, m_rotationLimitsMaxPitch);

	Vec3 currentCameraOffset = m_pCameraComponent->GetTransformMatrix().GetTranslation();
	currentCameraOffset = Vec3::CreateLerp(currentCameraOffset, m_cameraEndOffset, 10.0f * frametime);

	Matrix34 finalCamMatrix;
	finalCamMatrix.SetTranslation(currentCameraOffset);
	finalCamMatrix.SetRotation33(Matrix33::CreateRotationX(m_currentPitch));
	m_pCameraComponent->SetTransformMatrix(finalCamMatrix);
}
