// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
namespace primitives
{
	struct capsule;
}
namespace Cry::DefaultComponents
{
	class CCameraComponent;
	class CInputComponent;
	class CCharacterControllerComponent;
	class CAdvancedAnimationComponent;
}

////////////////////////////////////////////////////////
// Represents a player participating in gameplay
////////////////////////////////////////////////////////
class CPlayerComponent final : public IEntityComponent
{
public:
	enum class EPlayerState
	{
		Walking,
		Sprinting
	};
	enum class EPlayerStance
	{
		Standing,
		Crouching
	};

private:
	static constexpr float DEFAULT_SPEED_WALKING = 3;
	static constexpr float DEFAULT_SPEED_RUNNING = 6;
	static constexpr float DEFAULT_JUMP_ENERGY = 4;
	static constexpr float DEFAULT_WALLJUMP_SIDE_ENERGY = 5;
	static constexpr float DEFAULT_WALLJUMP_HEIGHT_ENERGY = 4;
	static constexpr float DEFAULT_DOUBLE_JUMP_ENERGY = 6;
	static constexpr float DEFAULT_ROTATION_SPEED = 0.002;
	static constexpr float DEFAULT_CAMERA_HEIGHT_CROUCHING = 1.0;
	static constexpr float DEFAULT_CAPSULE_HEIGHT_CROUCHING = 0.75;
	static constexpr float DEFAULT_CAPSULE_HEIGHT_STANDING = 1.7;
	static constexpr float DEFAULT_CAPSULE_GROUND_OFFSET = 0.2;
	static constexpr CryTransform::CAngle DEFAULT_WALLRUN_FOV = CryTransform::CAngle::FromDegrees(75.0f);
	static constexpr CryTransform::CAngle DEFAULT_PLAYER_FOV = CryTransform::CAngle::FromDegrees(65.0f);
	static constexpr float DEFAULT_CAMERA_HEIGHT_STANDING = 1.7;
	static constexpr float DEFAULT_ROT_LIMIT_PITCH_MAX = 1.5;
	static constexpr float DEFAULT_ROT_LIMIT_PITCH_MIN = -1.5;
	static constexpr EPlayerState DEFAULT_STATE = EPlayerState::Walking;
	static constexpr EPlayerStance DEFAULT_STANCE = EPlayerStance::Standing;

public:
	CPlayerComponent();
	virtual ~CPlayerComponent() override {};

	virtual void Initialize() override;

	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void ProcessEvent(const SEntityEvent& event) override;
	// Reflect type to set a unique identifier for this component
	static void ReflectType(Schematyc::CTypeDesc<CPlayerComponent>& desc)
	{
		desc.SetGUID("{63F4C0C6-32AF-4ACB-8FB0-57D45DD14725}"_cry_guid);
		desc.AddMember(&CPlayerComponent::m_walkSpeed, 'pws', "playerwalkspeed", "Player Walk Speed", "Sets the Player Walk Speed", DEFAULT_SPEED_WALKING);
		desc.AddMember(&CPlayerComponent::m_runSpeed, 'prs', "playerrunspeed", "Player Run Speed", "Sets the Players Run Speed", DEFAULT_SPEED_RUNNING);
		desc.AddMember(&CPlayerComponent::m_doublejumpheight, 'djh', "playerdoublejumpheight", "Player Double Jump Height", "Sets the Players Jump Height", DEFAULT_DOUBLE_JUMP_ENERGY);
		desc.AddMember(&CPlayerComponent::m_jumpheight, 'pjh', "playerjumpheight", "Player Jump Height", "Sets the Players Jump Height", DEFAULT_JUMP_ENERGY);
		desc.AddMember(&CPlayerComponent::m_walljumpside, 'wjs', "playerwalljumpside", "Player Wall Jump Side Height", "Sets the Players Wall Jump Side Height", DEFAULT_WALLJUMP_SIDE_ENERGY);
		desc.AddMember(&CPlayerComponent::m_walljumpheight, 'wjh', "playerwalljumpheight", "Player Wall Jump Height", "Sets the Players Wall Jump Height", DEFAULT_WALLJUMP_HEIGHT_ENERGY);

		desc.AddMember(&CPlayerComponent::m_cameraOffsetCrouching, 'camc', "cameraoffsetcrouching", "Camera Crouching Offset", "Offset of the camera while crouching", Vec3(0.f, 0.f, DEFAULT_CAMERA_HEIGHT_CROUCHING));
		desc.AddMember(&CPlayerComponent::m_cameraOffsetStanding, 'cams', "cameraoffsetstanding", "Camera Standing Offset", "Offset of the camera while standing", Vec3(0.f, 0.f, DEFAULT_CAMERA_HEIGHT_STANDING));
		desc.AddMember(&CPlayerComponent::m_capsuleHeightCrouching, 'capc', "capsuleheightcrouching", "Capsule Crouching Height", "Height of collision capsule while crouching", DEFAULT_CAPSULE_HEIGHT_CROUCHING);
		desc.AddMember(&CPlayerComponent::m_capsuleHeightStanding, 'caps', "capsuleheightstanding", "Capsule Standing Height", "Height of collision capsule while standing", DEFAULT_CAPSULE_HEIGHT_STANDING);
		desc.AddMember(&CPlayerComponent::m_capsuleGroundOffset, 'capo', "capsulegroundoffset", "Capsule Ground Offset", "Offset of the capsule from the entity floor", DEFAULT_CAPSULE_GROUND_OFFSET);
		desc.AddMember(&CPlayerComponent::m_wallrunFOV, 'wfov', "wallrunfov", "Player Wall Run FOV", "Player FOV when wall running", DEFAULT_WALLRUN_FOV);
		desc.AddMember(&CPlayerComponent::m_FOV, 'fov', "playerfov", "Player FOV", "Player FOV", DEFAULT_PLAYER_FOV);
		desc.AddMember(&CPlayerComponent::m_rotationSpeed, 'pros', "playerrotationspeed", "Player Rotation Speed", "Sets player rotation speed", DEFAULT_ROTATION_SPEED);
		desc.AddMember(&CPlayerComponent::m_rotationLimitsMaxPitch, 'cpm', "camerapitchmax", "Camera Pitch Max", "Maximum Rotation Value for camera pitch", DEFAULT_ROT_LIMIT_PITCH_MAX);
		desc.AddMember(&CPlayerComponent::m_rotationLimitsMinPitch, 'cpmi', "camerapitchmin", "Camera Pitch Min", "Minimum Rotation Value for camera pitch", -DEFAULT_ROT_LIMIT_PITCH_MIN);
	}
protected:
	void Reset();
	void RecenterCollider();
	void InitializeInput();


	void UpdateMovement();
	void UpdateRotation();
	void UpdateCamera(float frametime);
	void TryUpdateStance();
	bool IsCapsuleIntersectingGeometry(const primitives::capsule& capsule) const;
	void IsWall();
	void TransitionFOV();
	void onGroundCollision();
	bool canDoubleJump = true;
	bool canJump = true;
	bool wallrunning = false;
	bool m_isMovingForward = false;
	bool canWallrun = false;



private:
	Cry::DefaultComponents::CCameraComponent* m_pCameraComponent;
	Cry::DefaultComponents::CInputComponent* m_pInputComponent;
	Cry::DefaultComponents::CCharacterControllerComponent* m_pCharacterController;
	Cry::DefaultComponents::CAdvancedAnimationComponent* m_pAdvancedAnimationComponent;


	Quat m_lookOrientation;

	// Runtime Variables
	Quat m_currentYaw;
	float m_currentPitch;
	Vec2 m_movementDelta;
	Vec2 m_mouseDeltaRotation;
	EPlayerState m_currentPlayerState;
	EPlayerStance m_currentStance;
	EPlayerStance m_desiredStance;
	Vec3 m_cameraEndOffset;

	// Component Properties
	float m_rotationSpeed;

	Vec3 m_wallNormal;
	Vec3 m_cameraOffsetStanding;
	Vec3 m_cameraOffsetCrouching;
	float m_capsuleHeightStanding;
	float m_capsuleHeightCrouching;
	float m_capsuleGroundOffset;

	float FOV_CHANGE_RATE = 35.0f;
	CryTransform::CAngle m_wallrunFOV;
	CryTransform::CAngle m_FOV;
	CryTransform::CAngle m_desiredFOV = m_FOV;

	float frametime = 0.0f;
	float m_wallrunCooldown = 0.2f;
	float m_wallrunTimer = 0.0f;
	float m_wallrunYaw;
	float m_yaw = 0.0f;

	float m_walkSpeed;
	float m_runSpeed;
	float m_jumpheight;
	float m_walljumpside;
	float m_walljumpheight;
	float m_doublejumpheight;

	float m_rotationLimitsMinPitch;
	float m_rotationLimitsMaxPitch;
};
