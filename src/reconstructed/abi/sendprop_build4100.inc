// Generated from original build4100 server_i486.so by sendprop_field_oracle.py.
// 198 SendTables / 1088 runtime properties; static recovery metadata only.

static constexpr OriginalSendPropDescriptor kOriginalProps_AI_BaseNPC[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_lifeState", OriginalSendPropKind::Int, 160, 160, 3, 1, nullptr},
    {"m_bPerformAvoidance", OriginalSendPropKind::Bool, 3240, 3240, -1, -1, nullptr},
    {"m_bIsMoving", OriginalSendPropKind::Bool, 3241, 3241, -1, -1, nullptr},
    {"m_bFadeCorpse", OriginalSendPropKind::Bool, 3242, 3242, -1, -1, nullptr},
    {"m_iDeathPose", OriginalSendPropKind::Int, 2180, 2180, 11, 0, nullptr},
    {"m_iDeathFrame", OriginalSendPropKind::Int, 2184, 2184, 5, 0, nullptr},
    {"m_bSpeedModActive", OriginalSendPropKind::Bool, 3244, 3244, -1, -1, nullptr},
    {"m_iSpeedModRadius", OriginalSendPropKind::Int, 3248, 3248, -1, 0, nullptr},
    {"m_iSpeedModSpeed", OriginalSendPropKind::Int, 3252, 3252, -1, 0, nullptr},
    {"m_bImportanRagdoll", OriginalSendPropKind::Bool, 3243, 3243, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_AnimTimeMustBeFirst[] = {
    {"m_flAnimTime", OriginalSendPropKind::Int, 48, 48, 8, 1025, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_Animationlayer[] = {
    {"m_nSequence", OriginalSendPropKind::Int, 12, 12, 11, 1, nullptr},
    {"m_flCycle", OriginalSendPropKind::Float, 16, 16, 15, 8, nullptr},
    {"m_flPrevCycle", OriginalSendPropKind::Float, 20, 20, 15, 8, nullptr},
    {"m_flWeight", OriginalSendPropKind::Float, 24, 24, 8, 0, nullptr},
    {"m_nOrder", OriginalSendPropKind::Int, 64, 64, 4, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BCCLocalPlayerExclusive[] = {
    {"m_flNextAttack", OriginalSendPropKind::Time, 1524, 1524, -1, -1, nullptr},
    {"m_hMyWeapons", OriginalSendPropKind::Array3, 1724, 1724, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BaseAnimating[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_nForceBone", OriginalSendPropKind::Int, 876, 876, 8, 0, nullptr},
    {"m_vecForce", OriginalSendPropKind::Vector, 880, 880, -1, 4, nullptr},
    {"m_nSkin", OriginalSendPropKind::Int, 892, 892, 10, 0, nullptr},
    {"m_nBody", OriginalSendPropKind::Int, 896, 896, 32, 0, nullptr},
    {"m_nHitboxSet", OriginalSendPropKind::Int, 900, 900, 2, 1, nullptr},
    {"m_flModelWidthScale", OriginalSendPropKind::Float, 904, 904, 6, 16, nullptr},
    {"m_flPoseParameter", OriginalSendPropKind::Array3, 956, 956, -1, -1, nullptr},
    {"m_nSequence", OriginalSendPropKind::Int, 952, 952, 11, 1, nullptr},
    {"m_flPlaybackRate", OriginalSendPropKind::Float, 908, 908, 8, 16, nullptr},
    {"m_flEncodedController", OriginalSendPropKind::Array3, 1052, 1052, -1, -1, nullptr},
    {"m_bClientSideAnimation", OriginalSendPropKind::Int, 1068, 1068, 1, 1, nullptr},
    {"m_bClientSideFrameReset", OriginalSendPropKind::Int, 1069, 1069, 1, 1, nullptr},
    {"m_nNewSequenceParity", OriginalSendPropKind::Int, 1072, 1072, 3, 1025, nullptr},
    {"m_nResetEventsParity", OriginalSendPropKind::Int, 1076, 1076, 3, 1025, nullptr},
    {"m_nMuzzleFlashParity", OriginalSendPropKind::Int, 1080, 1080, 2, 1025, nullptr},
    {"m_hLightingOrigin", OriginalSendPropKind::EHandle, 1084, 1084, -1, 0, nullptr},
    {"m_hLightingOriginRelative", OriginalSendPropKind::EHandle, 1088, 1088, -1, 0, nullptr},
    {"serveranimdata", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_fadeMinDist", OriginalSendPropKind::Float, 1108, 1108, 0, 4, nullptr},
    {"m_fadeMaxDist", OriginalSendPropKind::Float, 1112, 1112, 0, 4, nullptr},
    {"m_flFadeScale", OriginalSendPropKind::Float, 1116, 1116, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BaseAnimatingOverlay[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"overlay_vars", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BaseBeam[] = {
    {"m_nModelIndex", OriginalSendPropKind::Int, 12, 12, 11, 0, nullptr},
    {"m_nHaloIndex", OriginalSendPropKind::Int, 16, 16, 11, 0, nullptr},
    {"m_nStartFrame", OriginalSendPropKind::Int, 20, 20, 8, 1, nullptr},
    {"m_nFrameRate", OriginalSendPropKind::Int, 24, 24, 8, 1, nullptr},
    {"m_fLife", OriginalSendPropKind::Float, 28, 28, 8, 0, nullptr},
    {"m_fWidth", OriginalSendPropKind::Float, 32, 32, 10, 0, nullptr},
    {"m_fEndWidth", OriginalSendPropKind::Float, 36, 36, 10, 0, nullptr},
    {"m_nFadeLength", OriginalSendPropKind::Int, 40, 40, 8, 1, nullptr},
    {"m_fAmplitude", OriginalSendPropKind::Float, 44, 44, 8, 0, nullptr},
    {"m_nSpeed", OriginalSendPropKind::Int, 64, 64, 8, 1, nullptr},
    {"r", OriginalSendPropKind::Int, 48, 48, 8, 1, nullptr},
    {"g", OriginalSendPropKind::Int, 52, 52, 8, 1, nullptr},
    {"b", OriginalSendPropKind::Int, 56, 56, 8, 1, nullptr},
    {"a", OriginalSendPropKind::Int, 60, 60, 8, 1, nullptr},
    {"m_nFlags", OriginalSendPropKind::Int, 68, 68, 32, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BaseCSGrenade[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_bRedraw", OriginalSendPropKind::Bool, 1392, 1392, -1, -1, nullptr},
    {"m_bPinPulled", OriginalSendPropKind::Bool, 1393, 1393, -1, -1, nullptr},
    {"m_fThrowTime", OriginalSendPropKind::Float, 1396, 1396, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BaseCSGrenadeProjectile[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vInitialVelocity", OriginalSendPropKind::Vector, 1952, 1952, 20, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BaseCombatCharacter[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"bcc_localdata", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_hActiveWeapon", OriginalSendPropKind::EHandle, 1916, 1916, -1, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BaseCombatWeapon[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"LocalWeaponData", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"LocalActiveWeaponData", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_iViewModelIndex", OriginalSendPropKind::Int, 1172, 1172, 11, 0, nullptr},
    {"m_iWorldModelIndex", OriginalSendPropKind::Int, 1176, 1176, 11, 0, nullptr},
    {"m_iState", OriginalSendPropKind::Int, 1208, 1208, 8, 1, nullptr},
    {"m_hOwner", OriginalSendPropKind::EHandle, 1148, 1148, -1, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BaseDoor[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flWaveHeight", OriginalSendPropKind::Float, 1060, 1060, 8, 16, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BaseEntity[] = {
    {"AnimTimeMustBeFirst", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flSimulationTime", OriginalSendPropKind::Int, 52, 52, 8, 1025, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 816, 816, -1, 1026, nullptr},
    {"m_nModelIndex", OriginalSendPropKind::Int, 38, 38, 11, 0, nullptr},
    {"m_Collision", OriginalSendPropKind::DataTable, 256, 256, -1, -1, nullptr},
    {"m_nRenderFX", OriginalSendPropKind::Int, 36, 36, 8, 1, nullptr},
    {"m_nRenderMode", OriginalSendPropKind::Int, 37, 37, 8, 1, nullptr},
    {"m_fEffects", OriginalSendPropKind::Int, 116, 116, 10, 1, nullptr},
    {"m_clrRender", OriginalSendPropKind::Int, 40, 40, 32, 1, nullptr},
    {"m_iTeamNum", OriginalSendPropKind::Int, 540, 540, 6, 0, nullptr},
    {"m_CollisionGroup", OriginalSendPropKind::Int, 520, 520, 5, 1, nullptr},
    {"m_flElasticity", OriginalSendPropKind::Float, 660, 660, 0, 2, nullptr},
    {"m_flShadowCastDistance", OriginalSendPropKind::Float, 528, 528, 12, 1, nullptr},
    {"m_hOwnerEntity", OriginalSendPropKind::EHandle, 512, 512, -1, 0, nullptr},
    {"m_hEffectEntity", OriginalSendPropKind::EHandle, 516, 516, -1, 0, nullptr},
    {"moveparent", OriginalSendPropKind::EHandle, 244, 244, -1, 0, nullptr},
    {"m_iParentAttachment", OriginalSendPropKind::Int, 241, 241, 6, 1, nullptr},
    {"movetype", OriginalSendPropKind::Int, 242, 242, 4, 1, nullptr},
    {"movecollide", OriginalSendPropKind::Int, 243, 243, 3, 1, nullptr},
    {"m_angRotation", OriginalSendPropKind::QAngles, 828, 828, 13, 1024, nullptr},
    {"m_iTextureFrameIndex", OriginalSendPropKind::Int, 704, 704, 8, 1, nullptr},
    {"predictable_id", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_bSimulatedEveryTick", OriginalSendPropKind::Int, 705, 705, 1, 1, nullptr},
    {"m_bAnimatedEveryTick", OriginalSendPropKind::Int, 706, 706, 1, 1, nullptr},
    {"m_bAlternateSorting", OriginalSendPropKind::Bool, 707, 707, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BaseFlex[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flexWeight", OriginalSendPropKind::Array3, 1168, 1168, -1, -1, nullptr},
    {"m_blinktoggle", OriginalSendPropKind::Int, 1436, 1436, 1, 1, nullptr},
    {"m_viewtarget", OriginalSendPropKind::Vector, 1424, 1424, -1, 2, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BaseGrenade[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flDamage", OriginalSendPropKind::Float, 1940, 1940, 10, 8, nullptr},
    {"m_DmgRadius", OriginalSendPropKind::Float, 1928, 1928, 10, 8, nullptr},
    {"m_bIsLive", OriginalSendPropKind::Int, 1925, 1925, 1, 1, nullptr},
    {"m_hThrower", OriginalSendPropKind::EHandle, 1948, 1948, -1, 0, nullptr},
    {"m_vecVelocity", OriginalSendPropKind::Vector, 692, 692, 0, 4, nullptr},
    {"m_fFlags", OriginalSendPropKind::Int, 192, 192, 9, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BaseParticleEntity[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BasePlayer[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"pl", OriginalSendPropKind::DataTable, 2360, 2360, -1, -1, nullptr},
    {"m_hVehicle", OriginalSendPropKind::EHandle, 2520, 2520, -1, 0, nullptr},
    {"m_hUseEntity", OriginalSendPropKind::EHandle, 2504, 2504, -1, 0, nullptr},
    {"m_iHealth", OriginalSendPropKind::Int, 156, 156, 10, 0, nullptr},
    {"m_lifeState", OriginalSendPropKind::Int, 160, 160, 3, 1, nullptr},
    {"m_flMaxspeed", OriginalSendPropKind::Float, 3180, 3180, 12, 8, nullptr},
    {"m_fFlags", OriginalSendPropKind::Int, 192, 192, 9, 1025, nullptr},
    {"m_iObserverMode", OriginalSendPropKind::Int, 2564, 2564, 3, 1, nullptr},
    {"m_hObserverTarget", OriginalSendPropKind::EHandle, 2580, 2580, -1, 0, nullptr},
    {"m_iFOV", OriginalSendPropKind::Int, 2568, 2568, 8, 1, nullptr},
    {"m_iDefaultFOV", OriginalSendPropKind::Int, 2572, 2572, 8, 1, nullptr},
    {"m_hViewModel", OriginalSendPropKind::EHandle, 3100, 3100, -1, 0, nullptr},
    {"m_hViewModel", OriginalSendPropKind::InternalArray, 0, 0, -1, 0, nullptr, 2, 4, false},
    {"m_szLastPlaceName", OriginalSendPropKind::String, 3408, 3408, 0, 0, nullptr},
    {"localdata", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BasePropDoor[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BaseViewModel[] = {
    {"m_nModelIndex", OriginalSendPropKind::Int, 38, 38, 11, 0, nullptr},
    {"m_nBody", OriginalSendPropKind::Int, 896, 896, 8, 0, nullptr},
    {"m_nSkin", OriginalSendPropKind::Int, 892, 892, 10, 0, nullptr},
    {"m_nSequence", OriginalSendPropKind::Int, 952, 952, 8, 1, nullptr},
    {"m_nViewModelIndex", OriginalSendPropKind::Int, 1148, 1148, 1, 1, nullptr},
    {"m_flPlaybackRate", OriginalSendPropKind::Float, 908, 908, 8, 16, nullptr},
    {"m_fEffects", OriginalSendPropKind::Int, 116, 116, 10, 1, nullptr},
    {"m_nAnimationParity", OriginalSendPropKind::Int, 1164, 1164, 3, 1, nullptr},
    {"m_hWeapon", OriginalSendPropKind::EHandle, 1188, 1188, -1, 0, nullptr},
    {"m_hOwner", OriginalSendPropKind::EHandle, 1152, 1152, -1, 0, nullptr},
    {"m_nNewSequenceParity", OriginalSendPropKind::Int, 1072, 1072, 3, 1, nullptr},
    {"m_nResetEventsParity", OriginalSendPropKind::Int, 1076, 1076, 3, 1, nullptr},
    {"m_nMuzzleFlashParity", OriginalSendPropKind::Int, 1080, 1080, 2, 1, nullptr},
    {"m_flPoseParameter", OriginalSendPropKind::Float, 956, 956, 8, 0, nullptr},
    {"m_flPoseParameter", OriginalSendPropKind::InternalArray, 0, 0, -1, 0, nullptr, 24, 4, false},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_Beam[] = {
    {"m_nBeamType", OriginalSendPropKind::Int, 892, 892, -1, 1, nullptr},
    {"m_nBeamFlags", OriginalSendPropKind::Int, 896, 896, 16, 1, nullptr},
    {"m_nNumBeamEnts", OriginalSendPropKind::Int, 884, 884, 5, 1, nullptr},
    {"m_hAttachEntity", OriginalSendPropKind::EHandle, 900, 900, -1, 0, nullptr},
    {"m_hAttachEntity", OriginalSendPropKind::InternalArray, 0, 0, -1, 0, nullptr, 10, 4, false},
    {"m_nAttachIndex", OriginalSendPropKind::Int, 940, 940, 5, 1, nullptr},
    {"m_nAttachIndex", OriginalSendPropKind::InternalArray, 0, 0, -1, 0, nullptr, 10, 4, false},
    {"m_nHaloIndex", OriginalSendPropKind::Int, 888, 888, 16, 1, nullptr},
    {"m_fHaloScale", OriginalSendPropKind::Float, 992, 992, 0, 4, nullptr},
    {"m_fWidth", OriginalSendPropKind::Float, 980, 980, 10, 16, nullptr},
    {"m_fEndWidth", OriginalSendPropKind::Float, 984, 984, 10, 16, nullptr},
    {"m_fFadeLength", OriginalSendPropKind::Float, 988, 988, 0, 4, nullptr},
    {"m_fAmplitude", OriginalSendPropKind::Float, 996, 996, 8, 8, nullptr},
    {"m_fStartFrame", OriginalSendPropKind::Float, 1000, 1000, 8, 8, nullptr},
    {"m_fSpeed", OriginalSendPropKind::Float, 1004, 1004, 8, 4, nullptr},
    {"m_nRenderFX", OriginalSendPropKind::Int, 36, 36, 8, 1, nullptr},
    {"m_nRenderMode", OriginalSendPropKind::Int, 37, 37, 8, 1, nullptr},
    {"m_flFrameRate", OriginalSendPropKind::Float, 868, 868, 10, 16, nullptr},
    {"m_flHDRColorScale", OriginalSendPropKind::Float, 872, 872, 0, 4, nullptr},
    {"m_flFrame", OriginalSendPropKind::Float, 1008, 1008, 20, 8, nullptr},
    {"m_clrRender", OriginalSendPropKind::Int, 40, 40, 32, 1, nullptr},
    {"m_vecEndPos", OriginalSendPropKind::Vector, 1012, 1012, -1, 2, nullptr},
    {"m_nModelIndex", OriginalSendPropKind::Int, 38, 38, 11, 0, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 816, 816, 19, 0, nullptr},
    {"moveparent", OriginalSendPropKind::EHandle, 244, 244, -1, 0, nullptr},
    {"beampredictable_id", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BeamPredictableId[] = {
    {"m_PredictableID", OriginalSendPropKind::PredictableId, 60, 60, -1, -1, nullptr},
    {"m_bIsPlayerSimulated", OriginalSendPropKind::Int, 856, 856, 1, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BoneFollower[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_modelIndex", OriginalSendPropKind::Int, 868, 868, 11, 0, nullptr},
    {"m_solidIndex", OriginalSendPropKind::Int, 872, 872, 6, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BreakableProp[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_BreakableSurface[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_nNumWide", OriginalSendPropKind::Int, 1040, 1040, 8, 1, nullptr},
    {"m_nNumHigh", OriginalSendPropKind::Int, 1044, 1044, 8, 1, nullptr},
    {"m_flPanelWidth", OriginalSendPropKind::Float, 1048, 1048, 0, 4, nullptr},
    {"m_flPanelHeight", OriginalSendPropKind::Float, 1052, 1052, 0, 4, nullptr},
    {"m_vNormal", OriginalSendPropKind::Vector, 1056, 1056, -1, 2, nullptr},
    {"m_vCorner", OriginalSendPropKind::Vector, 1068, 1068, -1, 2, nullptr},
    {"m_bIsBroken", OriginalSendPropKind::Int, 1080, 1080, 1, 1, nullptr},
    {"m_nSurfaceType", OriginalSendPropKind::Int, 1084, 1084, 2, 1, nullptr},
    {"m_RawPanelBitVec", OriginalSendPropKind::Array3, 2172, 2172, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_CFish[] = {
    {"m_poolOrigin", OriginalSendPropKind::Vector, 1200, 1200, -1, 2, nullptr},
    {"m_angle", OriginalSendPropKind::Float, 1168, 1168, 7, 0, nullptr},
    {"m_x", OriginalSendPropKind::Float, 1156, 1156, 7, 0, nullptr},
    {"m_y", OriginalSendPropKind::Float, 1160, 1160, 7, 0, nullptr},
    {"m_z", OriginalSendPropKind::Float, 1164, 1164, -1, 2, nullptr},
    {"m_nModelIndex", OriginalSendPropKind::Int, 38, 38, 11, 0, nullptr},
    {"m_lifeState", OriginalSendPropKind::Int, 160, 160, -1, 0, nullptr},
    {"m_waterLevel", OriginalSendPropKind::Float, 1212, 1212, 32, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_CHostage[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flPoseParameter", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_BaseAnimating"},
    {"m_flPlaybackRate", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_BaseAnimating"},
    {"m_nSequence", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_BaseAnimating"},
    {"m_nNewSequenceParity", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_BaseAnimating"},
    {"m_nResetEventsParity", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_BaseAnimating"},
    {"overlay_vars", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_BaseAnimatingOverlay"},
    {"m_flCycle", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_ServerAnimationData"},
    {"m_flAnimTime", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_AnimTimeMustBeFirst"},
    {"m_isRescued", OriginalSendPropKind::Bool, 1932, 1932, -1, -1, nullptr},
    {"m_iHealth", OriginalSendPropKind::Int, 156, 156, 10, 0, nullptr},
    {"m_iMaxHealth", OriginalSendPropKind::Int, 152, 152, 10, 0, nullptr},
    {"m_lifeState", OriginalSendPropKind::Int, 160, 160, 3, 1, nullptr},
    {"m_leader", OriginalSendPropKind::EHandle, 1936, 1936, -1, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_CSGameRules[] = {
    {"m_bFreezePeriod", OriginalSendPropKind::Bool, 531, 531, -1, -1, nullptr},
    {"m_iRoundTime", OriginalSendPropKind::Int, 532, 532, 16, 0, nullptr},
    {"m_fRoundStartTime", OriginalSendPropKind::Float, 536, 536, 32, 4, nullptr},
    {"m_flGameStartTime", OriginalSendPropKind::Float, 540, 540, 32, 4, nullptr},
    {"m_iHostagesRemaining", OriginalSendPropKind::Int, 544, 544, 4, 0, nullptr},
    {"m_bMapHasBombTarget", OriginalSendPropKind::Bool, 548, 548, -1, -1, nullptr},
    {"m_bMapHasRescueZone", OriginalSendPropKind::Bool, 549, 549, -1, -1, nullptr},
    {"m_bLogoMap", OriginalSendPropKind::Bool, 550, 550, -1, -1, nullptr},
    {"m_bBlackMarket", OriginalSendPropKind::Bool, 551, 551, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_CSGameRulesProxy[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"cs_gamerules_data", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_CSLocalPlayerExclusive[] = {
    {"m_flStamina", OriginalSendPropKind::Float, 4728, 4728, 14, 0, nullptr},
    {"m_iDirection", OriginalSendPropKind::Int, 4732, 4732, 1, 1, nullptr},
    {"m_iShotsFired", OriginalSendPropKind::Int, 4736, 4736, 8, 1, nullptr},
    {"m_flVelocityModifier", OriginalSendPropKind::Float, 4740, 4740, 8, 0, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 816, 816, -1, 1028, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_CSNonLocalPlayerExclusive[] = {
    {"m_vecOrigin", OriginalSendPropKind::Vector, 816, 816, -1, 1026, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_CSPlayer[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flPoseParameter", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_BaseAnimating"},
    {"m_flPlaybackRate", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_BaseAnimating"},
    {"m_nSequence", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_BaseAnimating"},
    {"m_nNewSequenceParity", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_BaseAnimating"},
    {"m_nResetEventsParity", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_BaseAnimating"},
    {"m_nMuzzleFlashParity", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_BaseAnimating"},
    {"m_angRotation", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_BaseEntity"},
    {"overlay_vars", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_BaseAnimatingOverlay"},
    {"m_flCycle", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_ServerAnimationData"},
    {"m_flAnimTime", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_AnimTimeMustBeFirst"},
    {"m_vecOrigin", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_BaseEntity"},
    {"cslocaldata", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"csnonlocaldata", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_iThrowGrenadeCounter", OriginalSendPropKind::Int, 4808, 4808, 3, 1, nullptr},
    {"m_iAddonBits", OriginalSendPropKind::Int, 5076, 5076, 9, 1, nullptr},
    {"m_iPrimaryAddon", OriginalSendPropKind::Int, 5080, 5080, 8, 1, nullptr},
    {"m_iSecondaryAddon", OriginalSendPropKind::Int, 5084, 5084, 8, 1, nullptr},
    {"m_iPlayerState", OriginalSendPropKind::Int, 3580, 3580, -1, 1, nullptr},
    {"m_iAccount", OriginalSendPropKind::Int, 3632, 3632, 16, 1, nullptr},
    {"m_bInBombZone", OriginalSendPropKind::Int, 4716, 4716, 1, 1, nullptr},
    {"m_bInBuyZone", OriginalSendPropKind::Int, 4717, 4717, 1, 1, nullptr},
    {"m_iClass", OriginalSendPropKind::Int, 4864, 4864, -1, 1, nullptr},
    {"m_ArmorValue", OriginalSendPropKind::Int, 3084, 3084, 8, 0, nullptr},
    {"m_angEyeAngles[0]", OriginalSendPropKind::Angle, -4900, -1, 11, 1024, nullptr},
    {"m_angEyeAngles[1]", OriginalSendPropKind::Angle, -4904, -1, 11, 1024, nullptr},
    {"m_bHasDefuser", OriginalSendPropKind::Bool, 3676, 3676, -1, -1, nullptr},
    {"m_bNightVisionOn", OriginalSendPropKind::Bool, 3678, 3678, -1, -1, nullptr},
    {"m_bHasNightVision", OriginalSendPropKind::Bool, 3677, 3677, -1, -1, nullptr},
    {"m_bInHostageRescueZone", OriginalSendPropKind::Bool, 4724, 4724, -1, -1, nullptr},
    {"m_bIsDefusing", OriginalSendPropKind::Bool, 3608, 3608, -1, -1, nullptr},
    {"m_bHasHelmet", OriginalSendPropKind::Bool, 3616, 3616, -1, -1, nullptr},
    {"m_flFlashDuration", OriginalSendPropKind::Float, 4792, 4792, 0, 4, nullptr},
    {"m_flFlashMaxAlpha", OriginalSendPropKind::Float, 4796, 4796, 0, 4, nullptr},
    {"m_iProgressBarDuration", OriginalSendPropKind::Int, 4804, 4804, 4, 1, nullptr},
    {"m_flProgressBarStartTime", OriginalSendPropKind::Float, 4800, 4800, 0, 4, nullptr},
    {"m_hRagdoll", OriginalSendPropKind::EHandle, 4812, 4812, -1, 0, nullptr},
    {"m_cycleLatch", OriginalSendPropKind::Int, 5208, 5208, 4, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_CSPlayerResource[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_iPlayerC4", OriginalSendPropKind::Int, 2692, 2692, 8, 1, nullptr},
    {"m_iPlayerVIP", OriginalSendPropKind::Int, 2696, 2696, 8, 1, nullptr},
    {"m_vecC4", OriginalSendPropKind::Vector, 2700, 2700, -1, 2, nullptr},
    {"m_bHostageAlive", OriginalSendPropKind::Array3, 2712, 2712, -1, -1, nullptr},
    {"m_isHostageFollowingSomeone", OriginalSendPropKind::Array3, 2724, 2724, -1, -1, nullptr},
    {"m_iHostageEntityIDs", OriginalSendPropKind::Array3, 2736, 2736, -1, -1, nullptr},
    {"m_iHostageY", OriginalSendPropKind::Array3, 2832, 2832, -1, -1, nullptr},
    {"m_iHostageX", OriginalSendPropKind::Array3, 2784, 2784, -1, -1, nullptr},
    {"m_iHostageZ", OriginalSendPropKind::Array3, 2880, 2880, -1, -1, nullptr},
    {"m_bombsiteCenterA", OriginalSendPropKind::Vector, 2928, 2928, -1, 2, nullptr},
    {"m_bombsiteCenterB", OriginalSendPropKind::Vector, 2940, 2940, -1, 2, nullptr},
    {"m_hostageRescueX", OriginalSendPropKind::Array3, 2952, 2952, -1, -1, nullptr},
    {"m_hostageRescueY", OriginalSendPropKind::Array3, 2968, 2968, -1, -1, nullptr},
    {"m_hostageRescueZ", OriginalSendPropKind::Array3, 2984, 2984, -1, -1, nullptr},
    {"m_bBombSpotted", OriginalSendPropKind::Bool, 3000, 3000, -1, -1, nullptr},
    {"m_bPlayerSpotted", OriginalSendPropKind::Array3, 3001, 3001, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_CSRagdoll[] = {
    {"m_vecOrigin", OriginalSendPropKind::Vector, 816, 816, -1, 1026, nullptr},
    {"m_vecRagdollOrigin", OriginalSendPropKind::Vector, 1184, 1184, -1, 2, nullptr},
    {"m_hPlayer", OriginalSendPropKind::EHandle, 1168, 1168, -1, 0, nullptr},
    {"m_nModelIndex", OriginalSendPropKind::Int, 38, 38, 11, 0, nullptr},
    {"m_nForceBone", OriginalSendPropKind::Int, 876, 876, 8, 0, nullptr},
    {"m_vecForce", OriginalSendPropKind::Vector, 880, 880, -1, 4, nullptr},
    {"m_vecRagdollVelocity", OriginalSendPropKind::Vector, 1172, 1172, 32, 4, nullptr},
    {"m_iDeathPose", OriginalSendPropKind::Int, 1196, 1196, 11, 1, nullptr},
    {"m_iDeathFrame", OriginalSendPropKind::Int, 1200, 1200, 5, 0, nullptr},
    {"m_iTeamNum", OriginalSendPropKind::Int, 540, 540, 6, 0, nullptr},
    {"m_bClientSideAnimation", OriginalSendPropKind::Int, 1068, 1068, 1, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_CSTeam[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_CollisionProperty[] = {
    {"m_vecMins", OriginalSendPropKind::Vector, 8, 8, 0, 4, nullptr},
    {"m_vecMaxs", OriginalSendPropKind::Vector, 20, 20, 0, 4, nullptr},
    {"m_nSolidType", OriginalSendPropKind::Int, 41, 41, 3, 1, nullptr},
    {"m_usSolidFlags", OriginalSendPropKind::Int, 36, 36, 9, 1, nullptr},
    {"m_nSurroundType", OriginalSendPropKind::Int, 40, 40, 3, 1, nullptr},
    {"m_triggerBloat", OriginalSendPropKind::Int, 42, 42, 0, 1, nullptr},
    {"m_vecSpecifiedSurroundingMins", OriginalSendPropKind::Vector, 44, 44, 0, 4, nullptr},
    {"m_vecSpecifiedSurroundingMaxs", OriginalSendPropKind::Vector, 56, 56, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_ColorCorrection[] = {
    {"m_vecOrigin", OriginalSendPropKind::Vector, 816, 816, -1, 4, nullptr},
    {"m_MinFalloff", OriginalSendPropKind::Float, 876, 876, 32, 0, nullptr},
    {"m_MaxFalloff", OriginalSendPropKind::Float, 880, 880, 32, 0, nullptr},
    {"m_MaxWeight", OriginalSendPropKind::Float, 884, 884, 32, 0, nullptr},
    {"m_netlookupFilename", OriginalSendPropKind::String, 888, 888, 0, 0, nullptr},
    {"m_bEnabled", OriginalSendPropKind::Bool, 869, 869, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_ColorCorrectionVolume[] = {
    {"m_Weight", OriginalSendPropKind::Float, 1092, 1092, 32, 0, nullptr},
    {"m_lookupFilename", OriginalSendPropKind::String, 1100, 1100, 0, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_DetailController[] = {
    {"m_flFadeStartDist", OriginalSendPropKind::Float, 868, 868, 32, 0, nullptr},
    {"m_flFadeEndDist", OriginalSendPropKind::Float, 872, 872, 32, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_DynamicLight[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_Flags", OriginalSendPropKind::Int, 869, 869, 4, 1, nullptr},
    {"m_LightStyle", OriginalSendPropKind::Int, 870, 870, 4, 1, nullptr},
    {"m_Radius", OriginalSendPropKind::Float, 872, 872, 0, 4, nullptr},
    {"m_Exponent", OriginalSendPropKind::Int, 876, 876, 8, 0, nullptr},
    {"m_InnerAngle", OriginalSendPropKind::Float, 880, 880, 8, 0, nullptr},
    {"m_OuterAngle", OriginalSendPropKind::Float, 884, 884, 8, 0, nullptr},
    {"m_SpotRadius", OriginalSendPropKind::Float, 888, 888, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_DynamicProp[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_bUseHitboxesForRenderBox", OriginalSendPropKind::Bool, 1519, 1519, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_EffectData[] = {
    {"m_vOrigin[0]", OriginalSendPropKind::Float, 0, 0, 14, 0, nullptr},
    {"m_vOrigin[1]", OriginalSendPropKind::Float, 4, 4, 14, 0, nullptr},
    {"m_vOrigin[2]", OriginalSendPropKind::Float, 8, 8, 14, 0, nullptr},
    {"m_vStart[0]", OriginalSendPropKind::Float, 12, 12, 14, 0, nullptr},
    {"m_vStart[1]", OriginalSendPropKind::Float, 16, 16, 14, 0, nullptr},
    {"m_vStart[2]", OriginalSendPropKind::Float, 20, 20, 14, 0, nullptr},
    {"m_vAngles", OriginalSendPropKind::QAngles, 36, 36, 7, 0, nullptr},
    {"m_vNormal", OriginalSendPropKind::Vector, 24, 24, 0, 32, nullptr},
    {"m_fFlags", OriginalSendPropKind::Int, 48, 48, 8, 1, nullptr},
    {"m_flMagnitude", OriginalSendPropKind::Float, 60, 60, 12, 8, nullptr},
    {"m_flScale", OriginalSendPropKind::Float, 56, 56, 0, 4, nullptr},
    {"m_nAttachmentIndex", OriginalSendPropKind::Int, 68, 68, 5, 1, nullptr},
    {"m_nSurfaceProp", OriginalSendPropKind::IntMinusOne, 72, 72, 8, 0, nullptr},
    {"m_iEffectName", OriginalSendPropKind::Int, 92, 92, 10, 1, nullptr},
    {"m_nMaterial", OriginalSendPropKind::Int, 76, 76, 16, 1, nullptr},
    {"m_nDamageType", OriginalSendPropKind::Int, 80, 80, 32, 1, nullptr},
    {"m_nHitBox", OriginalSendPropKind::Int, 84, 84, 11, 1, nullptr},
    {"entindex", OriginalSendPropKind::Int, 52, 52, 11, 1, nullptr},
    {"m_nColor", OriginalSendPropKind::Int, 88, 88, 8, 1, nullptr},
    {"m_flRadius", OriginalSendPropKind::Float, 64, 64, 10, 8, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_Embers[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_nDensity", OriginalSendPropKind::Int, 868, 868, 32, 1, nullptr},
    {"m_nLifetime", OriginalSendPropKind::Int, 872, 872, 32, 1, nullptr},
    {"m_nSpeed", OriginalSendPropKind::Int, 876, 876, 32, 1, nullptr},
    {"m_bEmit", OriginalSendPropKind::Int, 880, 880, 2, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_EntityDissolve[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flStartTime", OriginalSendPropKind::Time, 868, 868, -1, -1, nullptr},
    {"m_flFadeInStart", OriginalSendPropKind::Float, 872, 872, 0, 4, nullptr},
    {"m_flFadeInLength", OriginalSendPropKind::Float, 876, 876, 0, 4, nullptr},
    {"m_flFadeOutModelStart", OriginalSendPropKind::Float, 880, 880, 0, 4, nullptr},
    {"m_flFadeOutModelLength", OriginalSendPropKind::Float, 884, 884, 0, 4, nullptr},
    {"m_flFadeOutStart", OriginalSendPropKind::Float, 888, 888, 0, 4, nullptr},
    {"m_flFadeOutLength", OriginalSendPropKind::Float, 892, 892, 0, 4, nullptr},
    {"m_nDissolveType", OriginalSendPropKind::Int, 896, 896, 3, 1, nullptr},
    {"m_vDissolverOrigin", OriginalSendPropKind::Vector, 900, 900, 0, 4, nullptr},
    {"m_nMagnitude", OriginalSendPropKind::Int, 912, 912, 8, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_EntityFlame[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flSize", OriginalSendPropKind::Float, 872, 872, 16, 4, nullptr},
    {"m_hEntAttached", OriginalSendPropKind::EHandle, 868, 868, -1, 0, nullptr},
    {"m_bUseHitboxes", OriginalSendPropKind::Int, 876, 876, 1, 1, nullptr},
    {"m_flLifetime", OriginalSendPropKind::Time, 880, 880, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_EntityParticleTrail[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_iMaterialName", OriginalSendPropKind::Int, 868, 868, 10, 1, nullptr},
    {"m_Info", OriginalSendPropKind::DataTable, 872, 872, -1, -1, nullptr},
    {"m_hConstraintEntity", OriginalSendPropKind::EHandle, 892, 892, -1, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_EntityParticleTrailInfo[] = {
    {"m_flLifetime", OriginalSendPropKind::Float, 8, 8, 0, 4, nullptr},
    {"m_flStartSize", OriginalSendPropKind::Float, 12, 12, 0, 4, nullptr},
    {"m_flEndSize", OriginalSendPropKind::Float, 16, 16, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_EnvParticleScript[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flSequenceScale", OriginalSendPropKind::Float, 1148, 1148, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_EnvProjectedTexture[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_hTargetEntity", OriginalSendPropKind::EHandle, 868, 868, -1, 0, nullptr},
    {"m_bState", OriginalSendPropKind::Bool, 872, 872, -1, -1, nullptr},
    {"m_flLightFOV", OriginalSendPropKind::Float, 876, 876, 32, 0, nullptr},
    {"m_bEnableShadows", OriginalSendPropKind::Bool, 880, 880, -1, -1, nullptr},
    {"m_bLightOnlyTarget", OriginalSendPropKind::Bool, 881, 881, -1, -1, nullptr},
    {"m_bLightWorld", OriginalSendPropKind::Bool, 882, 882, -1, -1, nullptr},
    {"m_bCameraSpace", OriginalSendPropKind::Bool, 883, 883, -1, -1, nullptr},
    {"m_cLightColor", OriginalSendPropKind::Int, 884, 884, 32, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_EnvScreenEffect[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flDuration", OriginalSendPropKind::Float, 868, 868, 0, 4, nullptr},
    {"m_nType", OriginalSendPropKind::Int, 872, 872, 32, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_EnvScreenOverlay[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_iszOverlayNames", OriginalSendPropKind::String, 868, 868, 0, 0, nullptr},
    {"m_iszOverlayNames", OriginalSendPropKind::InternalArray, 0, 0, -1, 0, nullptr, 10, 4, false},
    {"m_flOverlayTimes", OriginalSendPropKind::Float, 908, 908, 11, 8, nullptr},
    {"m_flOverlayTimes", OriginalSendPropKind::InternalArray, 0, 0, -1, 0, nullptr, 10, 4, false},
    {"m_flStartTime", OriginalSendPropKind::Float, 948, 948, 32, 4, nullptr},
    {"m_iDesiredOverlay", OriginalSendPropKind::Int, 952, 952, 5, 0, nullptr},
    {"m_bIsActive", OriginalSendPropKind::Bool, 956, 956, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_EnvTonemapController[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_bUseCustomAutoExposureMin", OriginalSendPropKind::Int, 884, 884, 1, 1, nullptr},
    {"m_bUseCustomAutoExposureMax", OriginalSendPropKind::Int, 885, 885, 1, 1, nullptr},
    {"m_bUseCustomBloomScale", OriginalSendPropKind::Int, 886, 886, 1, 1, nullptr},
    {"m_flCustomAutoExposureMin", OriginalSendPropKind::Float, 888, 888, 0, 4, nullptr},
    {"m_flCustomAutoExposureMax", OriginalSendPropKind::Float, 892, 892, 0, 4, nullptr},
    {"m_flCustomBloomScale", OriginalSendPropKind::Float, 896, 896, 0, 4, nullptr},
    {"m_flCustomBloomScaleMinimum", OriginalSendPropKind::Float, 900, 900, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_EnvWind[] = {
    {"m_EnvWindShared", OriginalSendPropKind::DataTable, 868, 868, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_EnvWindShared[] = {
    {"m_iMinWind", OriginalSendPropKind::Int, 12, 12, 10, 1, nullptr},
    {"m_iMaxWind", OriginalSendPropKind::Int, 16, 16, 10, 1, nullptr},
    {"m_iMinGust", OriginalSendPropKind::Int, 20, 20, 10, 1, nullptr},
    {"m_iMaxGust", OriginalSendPropKind::Int, 24, 24, 10, 1, nullptr},
    {"m_flMinGustDelay", OriginalSendPropKind::Float, 28, 28, 0, 4, nullptr},
    {"m_flMaxGustDelay", OriginalSendPropKind::Float, 32, 32, 0, 4, nullptr},
    {"m_iGustDirChange", OriginalSendPropKind::Int, 40, 40, 9, 1, nullptr},
    {"m_iWindSeed", OriginalSendPropKind::Int, 8, 8, 32, 1, nullptr},
    {"m_iInitialWindDir", OriginalSendPropKind::Int, 56, 56, 9, 1, nullptr},
    {"m_flInitialWindSpeed", OriginalSendPropKind::Float, 60, 60, 0, 4, nullptr},
    {"m_flStartTime", OriginalSendPropKind::Float, 4, 4, 0, 4, nullptr},
    {"m_flGustDuration", OriginalSendPropKind::Float, 36, 36, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_FireSmoke[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flStartScale", OriginalSendPropKind::Float, 868, 868, 0, 4, nullptr},
    {"m_flScale", OriginalSendPropKind::Float, 872, 872, 0, 4, nullptr},
    {"m_flScaleTime", OriginalSendPropKind::Float, 876, 876, 0, 4, nullptr},
    {"m_nFlags", OriginalSendPropKind::Int, 880, 880, 8, 1, nullptr},
    {"m_nFlameModelIndex", OriginalSendPropKind::Int, 884, 884, 11, 0, nullptr},
    {"m_nFlameFromAboveModelIndex", OriginalSendPropKind::Int, 888, 888, 11, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_Flashbang[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_FootstepControl[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_source", OriginalSendPropKind::StringT, 1088, 1088, -1, -1, nullptr},
    {"m_destination", OriginalSendPropKind::StringT, 1092, 1092, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_FuncAreaPortalWindow[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flFadeDist", OriginalSendPropKind::Float, 884, 884, 0, 4, nullptr},
    {"m_flFadeStartDist", OriginalSendPropKind::Float, 880, 880, 0, 4, nullptr},
    {"m_flTranslucencyLimit", OriginalSendPropKind::Float, 888, 888, 0, 4, nullptr},
    {"m_iBackgroundModelIndex", OriginalSendPropKind::Int, 896, 896, 11, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_FuncConveyor[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flConveyorSpeed", OriginalSendPropKind::Float, 884, 884, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_FuncLadder[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecPlayerMountPositionTop", OriginalSendPropKind::Vector, 900, 900, 2, 4, nullptr},
    {"m_vecPlayerMountPositionBottom", OriginalSendPropKind::Vector, 912, 912, 2, 4, nullptr},
    {"m_vecLadderDir", OriginalSendPropKind::Vector, 868, 868, 2, 4, nullptr},
    {"m_bFakeLadder", OriginalSendPropKind::Bool, 925, 925, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_FuncMonitor[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_FuncOccluder[] = {
    {"m_bActive", OriginalSendPropKind::Bool, 868, 868, -1, -1, nullptr},
    {"m_nOccluderIndex", OriginalSendPropKind::Int, 872, 872, 10, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_FuncSmokeVolume[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_Color1", OriginalSendPropKind::Int, 868, 868, 32, 1, nullptr},
    {"m_Color2", OriginalSendPropKind::Int, 872, 872, 32, 1, nullptr},
    {"m_MaterialName", OriginalSendPropKind::String, 876, 876, 0, 0, nullptr},
    {"m_ParticleDrawWidth", OriginalSendPropKind::Float, 1136, 1136, 0, 4, nullptr},
    {"m_ParticleSpacingDistance", OriginalSendPropKind::Float, 1140, 1140, 0, 4, nullptr},
    {"m_DensityRampSpeed", OriginalSendPropKind::Float, 1144, 1144, 0, 4, nullptr},
    {"m_RotationSpeed", OriginalSendPropKind::Float, 1148, 1148, 0, 4, nullptr},
    {"m_MovementSpeed", OriginalSendPropKind::Float, 1152, 1152, 0, 4, nullptr},
    {"m_Density", OriginalSendPropKind::Float, 1156, 1156, 0, 4, nullptr},
    {"m_spawnflags", OriginalSendPropKind::Int, 184, 184, 8, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_FuncTrackTrain[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_Func_Dust[] = {
    {"m_Color", OriginalSendPropKind::Int, 868, 868, 32, 1, nullptr},
    {"m_SpawnRate", OriginalSendPropKind::Int, 872, 872, 12, 1, nullptr},
    {"m_SpeedMax", OriginalSendPropKind::Int, 884, 884, 12, 1, nullptr},
    {"m_flSizeMin", OriginalSendPropKind::Float, 876, 876, 0, 4, nullptr},
    {"m_flSizeMax", OriginalSendPropKind::Float, 880, 880, 0, 4, nullptr},
    {"m_DistMax", OriginalSendPropKind::Int, 896, 896, 16, 1, nullptr},
    {"m_LifetimeMin", OriginalSendPropKind::Int, 888, 888, 4, 1, nullptr},
    {"m_LifetimeMax", OriginalSendPropKind::Int, 892, 892, 4, 1, nullptr},
    {"m_DustFlags", OriginalSendPropKind::Int, 904, 904, 3, 1, nullptr},
    {"m_nModelIndex", OriginalSendPropKind::Int, 38, 38, 11, 0, nullptr},
    {"m_FallSpeed", OriginalSendPropKind::Float, 900, 900, 0, 4, nullptr},
    {"m_Collision", OriginalSendPropKind::DataTable, 256, 256, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_Func_LOD[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_fDisappearDist", OriginalSendPropKind::Float, 868, 868, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_HEGrenade[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_HandleTest[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_Handle", OriginalSendPropKind::EHandle, 868, 868, -1, 0, nullptr},
    {"m_bSendHandle", OriginalSendPropKind::Int, 872, 872, -1, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_InfoLadderDismount[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_InfoLightingRelative[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_hLightingLandmark", OriginalSendPropKind::EHandle, 868, 868, -1, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_LightGlow[] = {
    {"m_clrRender", OriginalSendPropKind::Int, 40, 40, 32, 1, nullptr},
    {"m_nHorizontalSize", OriginalSendPropKind::Int, 868, 868, 16, 1, nullptr},
    {"m_nVerticalSize", OriginalSendPropKind::Int, 872, 872, 16, 1, nullptr},
    {"m_nMinDist", OriginalSendPropKind::Int, 876, 876, 16, 1, nullptr},
    {"m_nMaxDist", OriginalSendPropKind::Int, 880, 880, 16, 1, nullptr},
    {"m_nOuterMaxDist", OriginalSendPropKind::Int, 884, 884, 16, 1, nullptr},
    {"m_spawnflags", OriginalSendPropKind::Int, 184, 184, 8, 1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 816, 816, -1, 2, nullptr},
    {"m_angRotation", OriginalSendPropKind::QAngles, 828, 828, 13, 0, nullptr},
    {"moveparent", OriginalSendPropKind::EHandle, 244, 244, -1, 0, nullptr},
    {"m_flGlowProxySize", OriginalSendPropKind::Float, 888, 888, 6, 16, nullptr},
    {"HDRColorScale", OriginalSendPropKind::Float, 892, 892, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_Local[] = {
    {"m_chAreaBits", OriginalSendPropKind::Array3, 4, 4, -1, -1, nullptr},
    {"m_chAreaPortalBits", OriginalSendPropKind::Array3, 36, 36, -1, -1, nullptr},
    {"m_iHideHUD", OriginalSendPropKind::Int, 60, 60, 11, 1, nullptr},
    {"m_flFOVRate", OriginalSendPropKind::Float, 64, 64, 0, 4, nullptr},
    {"m_bDucked", OriginalSendPropKind::Int, 80, 80, 1, 1, nullptr},
    {"m_bDucking", OriginalSendPropKind::Int, 81, 81, 1, 1, nullptr},
    {"m_bInDuckJump", OriginalSendPropKind::Int, 82, 82, 1, 1, nullptr},
    {"m_flDucktime", OriginalSendPropKind::Float, 84, 84, 12, 1032, nullptr},
    {"m_flDuckJumpTime", OriginalSendPropKind::Float, 88, 88, 12, 8, nullptr},
    {"m_flJumpTime", OriginalSendPropKind::Float, 92, 92, 12, 8, nullptr},
    {"m_flFallVelocity", OriginalSendPropKind::Float, 100, 100, 17, 1024, nullptr},
    {"m_vecPunchAngle", OriginalSendPropKind::Vector, 112, 112, -1, 1026, nullptr},
    {"m_vecPunchAngleVel", OriginalSendPropKind::Vector, 124, 124, -1, 2, nullptr},
    {"m_bDrawViewmodel", OriginalSendPropKind::Int, 136, 136, 1, 1, nullptr},
    {"m_bWearingSuit", OriginalSendPropKind::Int, 137, 137, 1, 1, nullptr},
    {"m_bPoisoned", OriginalSendPropKind::Bool, 138, 138, -1, -1, nullptr},
    {"m_flStepSize", OriginalSendPropKind::Float, 140, 140, 16, 16, nullptr},
    {"m_bAllowAutoMovement", OriginalSendPropKind::Int, 144, 144, 1, 1, nullptr},
    {"m_skybox3d.scale", OriginalSendPropKind::Int, 152, 152, 12, 0, nullptr},
    {"m_skybox3d.origin", OriginalSendPropKind::Vector, 156, 156, -1, 2, nullptr},
    {"m_skybox3d.area", OriginalSendPropKind::Int, 168, 168, 8, 1, nullptr},
    {"m_skybox3d.fog.enable", OriginalSendPropKind::Int, 176, 176, 1, 1, nullptr},
    {"m_skybox3d.fog.blend", OriginalSendPropKind::Int, 177, 177, 1, 1, nullptr},
    {"m_skybox3d.fog.dirPrimary", OriginalSendPropKind::Vector, 180, 180, -1, 2, nullptr},
    {"m_skybox3d.fog.colorPrimary", OriginalSendPropKind::Int, 192, 192, 32, 1, nullptr},
    {"m_skybox3d.fog.colorSecondary", OriginalSendPropKind::Int, 196, 196, 32, 1, nullptr},
    {"m_skybox3d.fog.start", OriginalSendPropKind::Float, 200, 200, 0, 4, nullptr},
    {"m_skybox3d.fog.end", OriginalSendPropKind::Float, 204, 204, 0, 4, nullptr},
    {"m_fog.enable", OriginalSendPropKind::Int, 240, 240, 1, 1, nullptr},
    {"m_fog.blend", OriginalSendPropKind::Int, 241, 241, 1, 1, nullptr},
    {"m_fog.dirPrimary", OriginalSendPropKind::Vector, 244, 244, -1, 2, nullptr},
    {"m_fog.colorPrimary", OriginalSendPropKind::Int, 256, 256, 32, 1, nullptr},
    {"m_fog.colorSecondary", OriginalSendPropKind::Int, 260, 260, 32, 1, nullptr},
    {"m_fog.start", OriginalSendPropKind::Float, 264, 264, 0, 4, nullptr},
    {"m_fog.end", OriginalSendPropKind::Float, 268, 268, 0, 4, nullptr},
    {"m_fog.farz", OriginalSendPropKind::Float, 272, 272, 0, 4, nullptr},
    {"m_fog.colorPrimaryLerpTo", OriginalSendPropKind::Int, 276, 276, 32, 1, nullptr},
    {"m_fog.colorSecondaryLerpTo", OriginalSendPropKind::Int, 280, 280, 32, 1, nullptr},
    {"m_fog.startLerpTo", OriginalSendPropKind::Float, 284, 284, 0, 4, nullptr},
    {"m_fog.endLerpTo", OriginalSendPropKind::Float, 288, 288, 0, 4, nullptr},
    {"m_fog.lerptime", OriginalSendPropKind::Float, 292, 292, 0, 4, nullptr},
    {"m_fog.duration", OriginalSendPropKind::Float, 296, 296, 0, 4, nullptr},
    {"m_audio.localSound[0]", OriginalSendPropKind::Vector, 304, 304, -1, 2, nullptr},
    {"m_audio.localSound[1]", OriginalSendPropKind::Vector, 316, 316, -1, 2, nullptr},
    {"m_audio.localSound[2]", OriginalSendPropKind::Vector, 328, 328, -1, 2, nullptr},
    {"m_audio.localSound[3]", OriginalSendPropKind::Vector, 340, 340, -1, 2, nullptr},
    {"m_audio.localSound[4]", OriginalSendPropKind::Vector, 352, 352, -1, 2, nullptr},
    {"m_audio.localSound[5]", OriginalSendPropKind::Vector, 364, 364, -1, 2, nullptr},
    {"m_audio.localSound[6]", OriginalSendPropKind::Vector, 376, 376, -1, 2, nullptr},
    {"m_audio.localSound[7]", OriginalSendPropKind::Vector, 388, 388, -1, 2, nullptr},
    {"m_audio.soundscapeIndex", OriginalSendPropKind::Int, 400, 400, 17, 0, nullptr},
    {"m_audio.localBits", OriginalSendPropKind::Int, 404, 404, 8, 1, nullptr},
    {"m_audio.ent", OriginalSendPropKind::EHandle, 408, 408, -1, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_LocalActiveWeaponData[] = {
    {"m_flNextPrimaryAttack", OriginalSendPropKind::Time, 1156, 1156, -1, -1, nullptr},
    {"m_flNextSecondaryAttack", OriginalSendPropKind::Time, 1160, 1160, -1, -1, nullptr},
    {"m_nNextThinkTick", OriginalSendPropKind::Int, 112, 112, -1, 0, nullptr},
    {"m_flTimeWeaponIdle", OriginalSendPropKind::Time, 1164, 1164, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_LocalActiveWeaponKnifeData[] = {
    {"m_flSmackTime", OriginalSendPropKind::Time, 1480, 1480, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_LocalPlayerExclusive[] = {
    {"m_Local", OriginalSendPropKind::DataTable, 1924, 1924, -1, -1, nullptr},
    {"m_vecViewOffset[0]", OriginalSendPropKind::Float, -844, -1, 8, 8, nullptr},
    {"m_vecViewOffset[1]", OriginalSendPropKind::Float, -848, -1, 8, 8, nullptr},
    {"m_vecViewOffset[2]", OriginalSendPropKind::Float, -852, -1, 10, 1024, nullptr},
    {"m_flFriction", OriginalSendPropKind::Float, 656, 656, 8, 8, nullptr},
    {"m_iAmmo", OriginalSendPropKind::Array3, 1596, 1596, -1, -1, nullptr},
    {"m_fOnTarget", OriginalSendPropKind::Int, 2428, 2428, 2, 1, nullptr},
    {"m_nTickBase", OriginalSendPropKind::Int, 3264, 3264, -1, 1024, nullptr},
    {"m_nNextThinkTick", OriginalSendPropKind::Int, 112, 112, -1, 0, nullptr},
    {"m_hLastWeapon", OriginalSendPropKind::EHandle, 3276, 3276, -1, 0, nullptr},
    {"m_hGroundEntity", OriginalSendPropKind::EHandle, 552, 552, 1024, 0, nullptr},
    {"m_vecVelocity[0]", OriginalSendPropKind::Float, -692, -1, 20, 1024, nullptr},
    {"m_vecVelocity[1]", OriginalSendPropKind::Float, -696, -1, 20, 1024, nullptr},
    {"m_vecVelocity[2]", OriginalSendPropKind::Float, -700, -1, 16, 1024, nullptr},
    {"m_vecBaseVelocity", OriginalSendPropKind::Vector, 564, 564, 20, 0, nullptr},
    {"m_hConstraintEntity", OriginalSendPropKind::EHandle, 3316, 3316, -1, 0, nullptr},
    {"m_vecConstraintCenter", OriginalSendPropKind::Vector, 3320, 3320, 0, 4, nullptr},
    {"m_flConstraintRadius", OriginalSendPropKind::Float, 3332, 3332, 0, 4, nullptr},
    {"m_flConstraintWidth", OriginalSendPropKind::Float, 3336, 3336, 0, 4, nullptr},
    {"m_flConstraintSpeedFactor", OriginalSendPropKind::Float, 3340, 3340, 0, 4, nullptr},
    {"m_flDeathTime", OriginalSendPropKind::Float, 2560, 2560, 0, 4, nullptr},
    {"m_nWaterLevel", OriginalSendPropKind::Int, 547, 547, 2, 1, nullptr},
    {"m_flLaggedMovementValue", OriginalSendPropKind::Float, 3376, 3376, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_LocalWeaponData[] = {
    {"m_iClip1", OriginalSendPropKind::IntMinusOne, 1224, 1224, 8, 0, nullptr},
    {"m_iClip2", OriginalSendPropKind::IntMinusOne, 1228, 1228, 8, 0, nullptr},
    {"m_iPrimaryAmmoType", OriginalSendPropKind::Int, 1216, 1216, 8, 0, nullptr},
    {"m_iSecondaryAmmoType", OriginalSendPropKind::Int, 1220, 1220, 8, 0, nullptr},
    {"m_nViewModelIndex", OriginalSendPropKind::Int, 1152, 1152, 1, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_MaterialModifyControl[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_szMaterialName", OriginalSendPropKind::String, 868, 868, 0, 0, nullptr},
    {"m_szMaterialVar", OriginalSendPropKind::String, 1123, 1123, 0, 0, nullptr},
    {"m_szMaterialVarValue", OriginalSendPropKind::String, 1378, 1378, 0, 0, nullptr},
    {"m_iFrameStart", OriginalSendPropKind::Int, 1636, 1636, 8, 0, nullptr},
    {"m_iFrameEnd", OriginalSendPropKind::Int, 1640, 1640, 8, 0, nullptr},
    {"m_bWrap", OriginalSendPropKind::Int, 1644, 1644, 1, 1, nullptr},
    {"m_flFramerate", OriginalSendPropKind::Float, 1648, 1648, 0, 4, nullptr},
    {"m_bNewAnimCommandsSemaphore", OriginalSendPropKind::Int, 1652, 1652, 1, 1, nullptr},
    {"m_flFloatLerpStartValue", OriginalSendPropKind::Float, 1656, 1656, 0, 4, nullptr},
    {"m_flFloatLerpEndValue", OriginalSendPropKind::Float, 1660, 1660, 0, 4, nullptr},
    {"m_flFloatLerpTransitionTime", OriginalSendPropKind::Float, 1664, 1664, 0, 4, nullptr},
    {"m_nModifyMode", OriginalSendPropKind::Int, 1668, 1668, 2, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_MovieExplosion[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_OverlayVars[] = {
    {"m_AnimOverlay", OriginalSendPropKind::UtlVector, 1148, 1148, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_ParticleFire[] = {
    {"m_vOrigin", OriginalSendPropKind::Vector, 868, 868, 0, 2, nullptr},
    {"m_vDirection", OriginalSendPropKind::Vector, 880, 880, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_ParticleSmokeGrenade[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flSpawnTime", OriginalSendPropKind::Time, 872, 872, -1, -1, nullptr},
    {"m_FadeStartTime", OriginalSendPropKind::Float, 876, 876, 0, 4, nullptr},
    {"m_FadeEndTime", OriginalSendPropKind::Float, 880, 880, 0, 4, nullptr},
    {"m_CurrentStage", OriginalSendPropKind::Int, 868, 868, 1, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_PhysBox[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_PhysBoxMultiplayer[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_iPhysicsMode", OriginalSendPropKind::Int, 1252, 1252, 1, 1, nullptr},
    {"m_fMass", OriginalSendPropKind::Float, 1256, 1256, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_PhysMagnet[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_PhysicsProp[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_bAwake", OriginalSendPropKind::Bool, 1642, 1642, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_PhysicsPropMultiplayer[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_iPhysicsMode", OriginalSendPropKind::Int, 1648, 1648, 2, 1, nullptr},
    {"m_fMass", OriginalSendPropKind::Float, 1652, 1652, 0, 4, nullptr},
    {"m_collisionMins", OriginalSendPropKind::Vector, 1660, 1660, 0, 4, nullptr},
    {"m_collisionMaxs", OriginalSendPropKind::Vector, 1672, 1672, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_PlantedC4[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_bBombTicking", OriginalSendPropKind::Bool, 1148, 1148, -1, -1, nullptr},
    {"m_flC4Blow", OriginalSendPropKind::Float, 1152, 1152, 0, 4, nullptr},
    {"m_flTimerLength", OriginalSendPropKind::Float, 1156, 1156, 0, 4, nullptr},
    {"m_flDefuseLength", OriginalSendPropKind::Float, 1176, 1176, 0, 4, nullptr},
    {"m_flDefuseCountDown", OriginalSendPropKind::Float, 1180, 1180, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_Plasma[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flScale", OriginalSendPropKind::Float, 872, 872, 0, 4, nullptr},
    {"m_flScaleTime", OriginalSendPropKind::Float, 876, 876, 0, 4, nullptr},
    {"m_nFlags", OriginalSendPropKind::Int, 880, 880, 8, 1, nullptr},
    {"m_nPlasmaModelIndex", OriginalSendPropKind::Int, 884, 884, 11, 0, nullptr},
    {"m_nPlasmaModelIndex2", OriginalSendPropKind::Int, 888, 888, 11, 0, nullptr},
    {"m_nGlowModelIndex", OriginalSendPropKind::Int, 892, 892, 11, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_PlayerResource[] = {
    {"m_iPing", OriginalSendPropKind::Array3, 868, 868, -1, -1, nullptr},
    {"m_iScore", OriginalSendPropKind::Array3, 1128, 1128, -1, -1, nullptr},
    {"m_iDeaths", OriginalSendPropKind::Array3, 1388, 1388, -1, -1, nullptr},
    {"m_bConnected", OriginalSendPropKind::Array3, 1648, 1648, -1, -1, nullptr},
    {"m_iTeam", OriginalSendPropKind::Array3, 1908, 1908, -1, -1, nullptr},
    {"m_bAlive", OriginalSendPropKind::Array3, 2168, 2168, -1, -1, nullptr},
    {"m_iHealth", OriginalSendPropKind::Array3, 2428, 2428, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_PlayerState[] = {
    {"deadflag", OriginalSendPropKind::Int, 4, 4, 1, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_PointCamera[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_FOV", OriginalSendPropKind::Float, 876, 876, 0, 4, nullptr},
    {"m_Resolution", OriginalSendPropKind::Float, 880, 880, 0, 4, nullptr},
    {"m_bFogEnable", OriginalSendPropKind::Int, 884, 884, 1, 1, nullptr},
    {"m_FogColor", OriginalSendPropKind::Int, 885, 885, 32, 1, nullptr},
    {"m_flFogStart", OriginalSendPropKind::Float, 892, 892, 0, 4, nullptr},
    {"m_flFogEnd", OriginalSendPropKind::Float, 896, 896, 0, 4, nullptr},
    {"m_bActive", OriginalSendPropKind::Int, 900, 900, 1, 1, nullptr},
    {"m_bUseScreenAspectRatio", OriginalSendPropKind::Int, 901, 901, 1, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_PointCommentaryNode[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_bActive", OriginalSendPropKind::Bool, 1280, 1280, -1, -1, nullptr},
    {"m_iszCommentaryFile", OriginalSendPropKind::StringT, 1156, 1156, -1, -1, nullptr},
    {"m_iszCommentaryFileNoHDR", OriginalSendPropKind::StringT, 1160, 1160, -1, -1, nullptr},
    {"m_flStartTime", OriginalSendPropKind::Time, 1284, 1284, -1, -1, nullptr},
    {"m_iszSpeakers", OriginalSendPropKind::StringT, 1288, 1288, -1, -1, nullptr},
    {"m_iNodeNumber", OriginalSendPropKind::Int, 1292, 1292, 8, 1, nullptr},
    {"m_iNodeNumberMax", OriginalSendPropKind::Int, 1296, 1296, 8, 1, nullptr},
    {"m_hViewPosition", OriginalSendPropKind::EHandle, 1176, 1176, -1, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_PoseController[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_hProps", OriginalSendPropKind::Array3, 868, 868, -1, -1, nullptr},
    {"m_chPoseIndex", OriginalSendPropKind::Array3, 884, 884, -1, -1, nullptr},
    {"m_bPoseValueParity", OriginalSendPropKind::Bool, 889, 889, -1, -1, nullptr},
    {"m_fPoseValue", OriginalSendPropKind::Float, 900, 900, 11, 0, nullptr},
    {"m_fCycleFrequency", OriginalSendPropKind::Float, 904, 904, 11, 0, nullptr},
    {"m_nFModType", OriginalSendPropKind::Int, 908, 908, 3, 1, nullptr},
    {"m_fFModTimeOffset", OriginalSendPropKind::Float, 912, 912, 11, 0, nullptr},
    {"m_fFModRate", OriginalSendPropKind::Float, 916, 916, 11, 0, nullptr},
    {"m_fFModAmplitude", OriginalSendPropKind::Float, 920, 920, 11, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_Precipitation[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_nPrecipType", OriginalSendPropKind::Int, 868, 868, -1, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_PredictableId[] = {
    {"m_PredictableID", OriginalSendPropKind::PredictableId, 60, 60, -1, -1, nullptr},
    {"m_bIsPlayerSimulated", OriginalSendPropKind::Int, 856, 856, 1, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_PredictedViewModel[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_PropJeep[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_bHeadlightIsOn", OriginalSendPropKind::Bool, 1957, 1957, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_PropVehicleDriveable[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_hPlayer", OriginalSendPropKind::EHandle, 1660, 1660, -1, 0, nullptr},
    {"m_nSpeed", OriginalSendPropKind::Int, 1664, 1664, 8, 0, nullptr},
    {"m_nRPM", OriginalSendPropKind::Int, 1668, 1668, 13, 0, nullptr},
    {"m_flThrottle", OriginalSendPropKind::Float, 1672, 1672, 0, 4, nullptr},
    {"m_nBoostTimeLeft", OriginalSendPropKind::Int, 1676, 1676, 8, 0, nullptr},
    {"m_nHasBoost", OriginalSendPropKind::Int, 1680, 1680, 1, 1, nullptr},
    {"m_nScannerDisabledWeapons", OriginalSendPropKind::Int, 1710, 1710, 1, 1, nullptr},
    {"m_nScannerDisabledVehicle", OriginalSendPropKind::Int, 1711, 1711, 1, 1, nullptr},
    {"m_bEnterAnimOn", OriginalSendPropKind::Int, 1728, 1728, 1, 1, nullptr},
    {"m_bExitAnimOn", OriginalSendPropKind::Int, 1729, 1729, 1, 1, nullptr},
    {"m_bUnableToFire", OriginalSendPropKind::Int, 1708, 1708, 1, 1, nullptr},
    {"m_vecEyeExitEndpoint", OriginalSendPropKind::Vector, 1684, 1684, -1, 2, nullptr},
    {"m_bHasGun", OriginalSendPropKind::Bool, 1709, 1709, -1, -1, nullptr},
    {"m_vecGunCrosshair", OriginalSendPropKind::Vector, 1696, 1696, -1, 2, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_ProxyToggle[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"blah", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_ProxyToggle_ProxiedData[] = {
    {"m_WithProxy", OriginalSendPropKind::Int, 868, 868, -1, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_QuadraticBeam[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_targetPosition", OriginalSendPropKind::Vector, 868, 868, -1, 2, nullptr},
    {"m_controlPosition", OriginalSendPropKind::Vector, 880, 880, -1, 2, nullptr},
    {"m_scrollRate", OriginalSendPropKind::Float, 892, 892, 8, 0, nullptr},
    {"m_flWidth", OriginalSendPropKind::Float, 896, 896, -1, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_Ragdoll[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_ragAngles", OriginalSendPropKind::QAngles, 2140, 2140, 13, 0, nullptr},
    {"m_ragAngles", OriginalSendPropKind::InternalArray, 0, 0, -1, 0, nullptr, 24, 12, false},
    {"m_ragPos", OriginalSendPropKind::Vector, 1852, 1852, -1, 2, nullptr},
    {"m_ragPos", OriginalSendPropKind::InternalArray, 0, 0, -1, 0, nullptr, 24, 12, false},
    {"m_hUnragdoll", OriginalSendPropKind::EHandle, 2432, 2432, -1, 0, nullptr},
    {"m_flBlendWeight", OriginalSendPropKind::Float, 2476, 2476, 8, 8, nullptr},
    {"m_nOverlaySequence", OriginalSendPropKind::Int, 2480, 2480, 11, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_RagdollManager[] = {
    {"m_iMaxRagdollCount", OriginalSendPropKind::Int, 868, 868, 6, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_Ragdoll_Attached[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_boneIndexAttached", OriginalSendPropKind::Int, 3064, 3064, 7, 1, nullptr},
    {"m_ragdollAttachedObjectIndex", OriginalSendPropKind::Int, 3068, 3068, 5, 1, nullptr},
    {"m_attachmentPointBoneSpace", OriginalSendPropKind::Vector, 3072, 3072, -1, 2, nullptr},
    {"m_attachmentPointRagdollSpace", OriginalSendPropKind::Vector, 3084, 3084, -1, 2, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_RopeKeyframe[] = {
    {"m_hStartPoint", OriginalSendPropKind::EHandle, 928, 928, -1, 0, nullptr},
    {"m_hEndPoint", OriginalSendPropKind::EHandle, 932, 932, -1, 0, nullptr},
    {"m_iStartAttachment", OriginalSendPropKind::Int, 936, 936, 5, 0, nullptr},
    {"m_iEndAttachment", OriginalSendPropKind::Int, 938, 938, 5, 0, nullptr},
    {"m_Slack", OriginalSendPropKind::Int, 880, 880, 12, 0, nullptr},
    {"m_RopeLength", OriginalSendPropKind::Int, 908, 908, 15, 0, nullptr},
    {"m_fLockedPoints", OriginalSendPropKind::Int, 912, 912, 4, 1, nullptr},
    {"m_RopeFlags", OriginalSendPropKind::Int, 872, 872, 9, 1, nullptr},
    {"m_nSegments", OriginalSendPropKind::Int, 892, 892, 4, 1, nullptr},
    {"m_iRopeMaterialModel", OriginalSendPropKind::Int, 900, 900, 16, 1, nullptr},
    {"m_Subdiv", OriginalSendPropKind::Int, 904, 904, 4, 1, nullptr},
    {"m_TextureScale", OriginalSendPropKind::Float, 888, 888, 10, 0, nullptr},
    {"m_Width", OriginalSendPropKind::Float, 884, 884, 0, 4, nullptr},
    {"m_flScrollSpeed", OriginalSendPropKind::Float, 920, 920, 0, 4, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 816, 816, -1, 2, nullptr},
    {"moveparent", OriginalSendPropKind::EHandle, 244, 244, -1, 0, nullptr},
    {"m_iParentAttachment", OriginalSendPropKind::Int, 241, 241, 6, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_SceneEntity[] = {
    {"m_nSceneStringIndex", OriginalSendPropKind::Int, 1044, 1044, 12, 1, nullptr},
    {"m_bIsPlayingBack", OriginalSendPropKind::Bool, 956, 956, -1, -1, nullptr},
    {"m_bPaused", OriginalSendPropKind::Bool, 957, 957, -1, -1, nullptr},
    {"m_flForceClientTime", OriginalSendPropKind::Float, 960, 960, 32, 0, nullptr},
    {"m_hActorList", OriginalSendPropKind::UtlVector, 996, 996, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_ServerAnimationData[] = {
    {"m_flCycle", OriginalSendPropKind::Float, 948, 948, 15, 8, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_ShadowControl[] = {
    {"m_shadowDirection", OriginalSendPropKind::Vector, 868, 868, -1, 4, nullptr},
    {"m_shadowColor", OriginalSendPropKind::Int, 880, 880, 32, 1, nullptr},
    {"m_flShadowMaxDist", OriginalSendPropKind::Float, 884, 884, 0, 4, nullptr},
    {"m_bDisableShadows", OriginalSendPropKind::Bool, 888, 888, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_SmokeGrenade[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_SmokeStack[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_SpreadSpeed", OriginalSendPropKind::Float, 868, 868, 0, 4, nullptr},
    {"m_Speed", OriginalSendPropKind::Float, 872, 872, 0, 4, nullptr},
    {"m_StartSize", OriginalSendPropKind::Float, 876, 876, 0, 4, nullptr},
    {"m_EndSize", OriginalSendPropKind::Float, 880, 880, 0, 4, nullptr},
    {"m_Rate", OriginalSendPropKind::Float, 884, 884, 0, 4, nullptr},
    {"m_JetLength", OriginalSendPropKind::Float, 888, 888, 0, 4, nullptr},
    {"m_bEmit", OriginalSendPropKind::Int, 896, 896, 1, 1, nullptr},
    {"m_flBaseSpread", OriginalSendPropKind::Float, 900, 900, 0, 4, nullptr},
    {"m_flRollSpeed", OriginalSendPropKind::Float, 892, 892, 0, 4, nullptr},
    {"m_DirLight.m_vPos", OriginalSendPropKind::Vector, 948, 948, 0, 4, nullptr},
    {"m_DirLight.m_vColor", OriginalSendPropKind::Vector, 960, 960, 0, 4, nullptr},
    {"m_DirLight.m_flIntensity", OriginalSendPropKind::Float, 972, 972, 0, 4, nullptr},
    {"m_AmbientLight.m_vPos", OriginalSendPropKind::Vector, 912, 912, 0, 4, nullptr},
    {"m_AmbientLight.m_vColor", OriginalSendPropKind::Vector, 924, 924, 0, 4, nullptr},
    {"m_AmbientLight.m_flIntensity", OriginalSendPropKind::Float, 936, 936, 0, 4, nullptr},
    {"m_vWind", OriginalSendPropKind::Vector, 996, 996, 0, 4, nullptr},
    {"m_flTwist", OriginalSendPropKind::Float, 976, 976, 0, 4, nullptr},
    {"m_iMaterialModel", OriginalSendPropKind::IntMinusOne, 984, 984, 16, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_SpotlightEnd[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flLightScale", OriginalSendPropKind::Float, 868, 868, 0, 4, nullptr},
    {"m_Radius", OriginalSendPropKind::Float, 872, 872, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_Sprite[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_hAttachedToEntity", OriginalSendPropKind::EHandle, 868, 868, -1, 0, nullptr},
    {"m_nAttachment", OriginalSendPropKind::Int, 872, 872, 8, 0, nullptr},
    {"m_flScaleTime", OriginalSendPropKind::Float, 900, 900, 0, 4, nullptr},
    {"m_flSpriteScale", OriginalSendPropKind::Float, 896, 896, 8, 16, nullptr},
    {"m_flGlowProxySize", OriginalSendPropKind::Float, 908, 908, 6, 16, nullptr},
    {"m_flHDRColorScale", OriginalSendPropKind::Float, 912, 912, 0, 4, nullptr},
    {"m_flSpriteFramerate", OriginalSendPropKind::Float, 876, 876, 8, 16, nullptr},
    {"m_flFrame", OriginalSendPropKind::Float, 880, 880, 20, 8, nullptr},
    {"m_flBrightnessTime", OriginalSendPropKind::Float, 892, 892, 0, 4, nullptr},
    {"m_nBrightness", OriginalSendPropKind::Int, 888, 888, 8, 1, nullptr},
    {"m_bWorldSpaceScale", OriginalSendPropKind::Bool, 904, 904, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_SpriteTrail[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flLifeTime", OriginalSendPropKind::Float, 948, 948, 0, 4, nullptr},
    {"m_flStartWidth", OriginalSendPropKind::Float, 952, 952, 0, 4, nullptr},
    {"m_flEndWidth", OriginalSendPropKind::Float, 956, 956, 0, 4, nullptr},
    {"m_flStartWidthVariance", OriginalSendPropKind::Float, 960, 960, 0, 4, nullptr},
    {"m_flTextureRes", OriginalSendPropKind::Float, 964, 964, 0, 4, nullptr},
    {"m_flMinFadeLength", OriginalSendPropKind::Float, 968, 968, 0, 4, nullptr},
    {"m_vecSkyboxOrigin", OriginalSendPropKind::Vector, 972, 972, 0, 4, nullptr},
    {"m_flSkyboxScale", OriginalSendPropKind::Float, 984, 984, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_SteamJet[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_SpreadSpeed", OriginalSendPropKind::Float, 868, 868, 0, 4, nullptr},
    {"m_Speed", OriginalSendPropKind::Float, 872, 872, 0, 4, nullptr},
    {"m_StartSize", OriginalSendPropKind::Float, 876, 876, 0, 4, nullptr},
    {"m_EndSize", OriginalSendPropKind::Float, 880, 880, 0, 4, nullptr},
    {"m_Rate", OriginalSendPropKind::Float, 884, 884, 0, 4, nullptr},
    {"m_JetLength", OriginalSendPropKind::Float, 888, 888, 0, 4, nullptr},
    {"m_bEmit", OriginalSendPropKind::Int, 892, 892, 1, 1, nullptr},
    {"m_bFaceLeft", OriginalSendPropKind::Int, 896, 896, 1, 1, nullptr},
    {"m_nType", OriginalSendPropKind::Int, 900, 900, 32, 1, nullptr},
    {"m_spawnflags", OriginalSendPropKind::Int, 184, 184, 8, 1, nullptr},
    {"m_flRollSpeed", OriginalSendPropKind::Float, 904, 904, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_Sun[] = {
    {"m_clrRender", OriginalSendPropKind::Int, 40, 40, 32, 1, nullptr},
    {"m_clrOverlay", OriginalSendPropKind::Int, 908, 908, 32, 1, nullptr},
    {"m_vDirection", OriginalSendPropKind::Vector, 868, 868, 0, 32, nullptr},
    {"m_bOn", OriginalSendPropKind::Int, 912, 912, 1, 1, nullptr},
    {"m_nSize", OriginalSendPropKind::Int, 900, 900, 10, 1, nullptr},
    {"m_nOverlaySize", OriginalSendPropKind::Int, 904, 904, 10, 1, nullptr},
    {"m_nMaterial", OriginalSendPropKind::Int, 916, 916, 32, 1, nullptr},
    {"m_nOverlayMaterial", OriginalSendPropKind::Int, 920, 920, 32, 1, nullptr},
    {"HDRColorScale", OriginalSendPropKind::Float, 924, 924, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEArmorRicochet[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEBSPDecal[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_nEntity", OriginalSendPropKind::Int, 24, 24, 11, 1, nullptr},
    {"m_nIndex", OriginalSendPropKind::Int, 28, 28, 9, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEBeamEntPoint[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_nStartEntity", OriginalSendPropKind::Int, 72, 72, 24, 1, nullptr},
    {"m_nEndEntity", OriginalSendPropKind::Int, 88, 88, 24, 1, nullptr},
    {"m_vecStartPoint", OriginalSendPropKind::Vector, 76, 76, -1, 2, nullptr},
    {"m_vecEndPoint", OriginalSendPropKind::Vector, 92, 92, -1, 2, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEBeamEnts[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_nStartEntity", OriginalSendPropKind::Int, 72, 72, 24, 1, nullptr},
    {"m_nEndEntity", OriginalSendPropKind::Int, 76, 76, 24, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEBeamFollow[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_iEntIndex", OriginalSendPropKind::Int, 72, 72, 24, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEBeamLaser[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_nStartEntity", OriginalSendPropKind::Int, 72, 72, 24, 1, nullptr},
    {"m_nEndEntity", OriginalSendPropKind::Int, 76, 76, 24, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEBeamPoints[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecStartPoint", OriginalSendPropKind::Vector, 72, 72, -1, 2, nullptr},
    {"m_vecEndPoint", OriginalSendPropKind::Vector, 84, 84, -1, 2, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEBeamRing[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_nStartEntity", OriginalSendPropKind::Int, 72, 72, 11, 1, nullptr},
    {"m_nEndEntity", OriginalSendPropKind::Int, 76, 76, 11, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEBeamRingPoint[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecCenter", OriginalSendPropKind::Vector, 72, 72, -1, 2, nullptr},
    {"m_flStartRadius", OriginalSendPropKind::Float, 84, 84, 16, 16, nullptr},
    {"m_flEndRadius", OriginalSendPropKind::Float, 88, 88, 16, 16, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEBeamSpline[] = {
    {"m_nPoints", OriginalSendPropKind::Int, 204, 204, 5, 1, nullptr},
    {"m_vecPoints", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_vecPoints", OriginalSendPropKind::InternalArray, 0, 0, -1, 0, nullptr, 16, 12, false},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEBloodSprite[] = {
    {"m_vecOrigin", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_vecDirection", OriginalSendPropKind::Vector, 24, 24, -1, 2, nullptr},
    {"r", OriginalSendPropKind::Int, 44, 44, 8, 1, nullptr},
    {"g", OriginalSendPropKind::Int, 48, 48, 8, 1, nullptr},
    {"b", OriginalSendPropKind::Int, 52, 52, 8, 1, nullptr},
    {"a", OriginalSendPropKind::Int, 56, 56, 8, 1, nullptr},
    {"m_nSprayModel", OriginalSendPropKind::Int, 36, 36, 11, 0, nullptr},
    {"m_nDropModel", OriginalSendPropKind::Int, 40, 40, 11, 0, nullptr},
    {"m_nSize", OriginalSendPropKind::Int, 60, 60, 8, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEBloodStream[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecDirection", OriginalSendPropKind::Vector, 24, 24, 11, 0, nullptr},
    {"r", OriginalSendPropKind::Int, 36, 36, 8, 1, nullptr},
    {"g", OriginalSendPropKind::Int, 40, 40, 8, 1, nullptr},
    {"b", OriginalSendPropKind::Int, 44, 44, 8, 1, nullptr},
    {"a", OriginalSendPropKind::Int, 48, 48, 8, 1, nullptr},
    {"m_nAmount", OriginalSendPropKind::Int, 52, 52, 8, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEBreakModel[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_angRotation[0]", OriginalSendPropKind::Angle, -48, -1, 13, 0, nullptr},
    {"m_angRotation[1]", OriginalSendPropKind::Angle, -52, -1, 13, 0, nullptr},
    {"m_angRotation[2]", OriginalSendPropKind::Angle, -56, -1, 13, 0, nullptr},
    {"m_vecSize", OriginalSendPropKind::Vector, 24, 24, -1, 2, nullptr},
    {"m_vecVelocity", OriginalSendPropKind::Vector, 36, 36, -1, 2, nullptr},
    {"m_nModelIndex", OriginalSendPropKind::Int, 64, 64, 11, 0, nullptr},
    {"m_nRandomization", OriginalSendPropKind::Int, 60, 60, 9, 1, nullptr},
    {"m_nCount", OriginalSendPropKind::Int, 68, 68, 8, 1, nullptr},
    {"m_fTime", OriginalSendPropKind::Float, 72, 72, 10, 0, nullptr},
    {"m_nFlags", OriginalSendPropKind::Int, 76, 76, 8, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEBubbleTrail[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecMins", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_vecMaxs", OriginalSendPropKind::Vector, 24, 24, -1, 2, nullptr},
    {"m_nModelIndex", OriginalSendPropKind::Int, 40, 40, 11, 0, nullptr},
    {"m_flWaterZ", OriginalSendPropKind::Float, 36, 36, 17, 0, nullptr},
    {"m_nCount", OriginalSendPropKind::Int, 44, 44, 8, 1, nullptr},
    {"m_fSpeed", OriginalSendPropKind::Float, 48, 48, 17, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEBubbles[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecMins", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_vecMaxs", OriginalSendPropKind::Vector, 24, 24, -1, 2, nullptr},
    {"m_nModelIndex", OriginalSendPropKind::Int, 40, 40, 11, 0, nullptr},
    {"m_fHeight", OriginalSendPropKind::Float, 36, 36, 17, 0, nullptr},
    {"m_nCount", OriginalSendPropKind::Int, 44, 44, 8, 1, nullptr},
    {"m_fSpeed", OriginalSendPropKind::Float, 48, 48, 17, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEDecal[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_vecStart", OriginalSendPropKind::Vector, 24, 24, -1, 2, nullptr},
    {"m_nEntity", OriginalSendPropKind::Int, 36, 36, 11, 1, nullptr},
    {"m_nHitbox", OriginalSendPropKind::Int, 40, 40, 11, 1, nullptr},
    {"m_nIndex", OriginalSendPropKind::Int, 44, 44, 9, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEDust[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flSize", OriginalSendPropKind::Float, 24, 24, -1, 2, nullptr},
    {"m_flSpeed", OriginalSendPropKind::Float, 28, 28, -1, 2, nullptr},
    {"m_vecDirection", OriginalSendPropKind::Vector, 32, 32, 4, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEDynamicLight[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"r", OriginalSendPropKind::Int, 28, 28, 8, 1, nullptr},
    {"g", OriginalSendPropKind::Int, 32, 32, 8, 1, nullptr},
    {"b", OriginalSendPropKind::Int, 36, 36, 8, 1, nullptr},
    {"exponent", OriginalSendPropKind::Int, 40, 40, 8, 0, nullptr},
    {"m_fRadius", OriginalSendPropKind::Float, 24, 24, 8, 16, nullptr},
    {"m_fTime", OriginalSendPropKind::Float, 44, 44, 8, 8, nullptr},
    {"m_fDecay", OriginalSendPropKind::Float, 48, 48, 8, 8, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEEffectDispatch[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_EffectData", OriginalSendPropKind::DataTable, 12, 12, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEEnergySplash[] = {
    {"m_vecPos", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_vecDir", OriginalSendPropKind::Vector, 24, 24, -1, 2, nullptr},
    {"m_bExplosive", OriginalSendPropKind::Int, 36, 36, 1, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEExplosion[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_nModelIndex", OriginalSendPropKind::Int, 24, 24, 11, 0, nullptr},
    {"m_fScale", OriginalSendPropKind::Float, 28, 28, 9, 0, nullptr},
    {"m_nFrameRate", OriginalSendPropKind::Int, 32, 32, 8, 1, nullptr},
    {"m_nFlags", OriginalSendPropKind::Int, 36, 36, 8, 1, nullptr},
    {"m_vecNormal", OriginalSendPropKind::Vector, 40, 40, -1, 2, nullptr},
    {"m_chMaterialType", OriginalSendPropKind::Int, 52, 52, 8, 1, nullptr},
    {"m_nRadius", OriginalSendPropKind::Int, 56, 56, 32, 1, nullptr},
    {"m_nMagnitude", OriginalSendPropKind::Int, 60, 60, 32, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEFireBullets[] = {
    {"m_vecOrigin", OriginalSendPropKind::Vector, 16, 16, -1, 2, nullptr},
    {"m_vecAngles[0]", OriginalSendPropKind::Angle, -28, -1, 13, 0, nullptr},
    {"m_vecAngles[1]", OriginalSendPropKind::Angle, -32, -1, 13, 0, nullptr},
    {"m_iWeaponID", OriginalSendPropKind::Int, 40, 40, 5, 1, nullptr},
    {"m_iMode", OriginalSendPropKind::Int, 44, 44, 1, 1, nullptr},
    {"m_iSeed", OriginalSendPropKind::Int, 48, 48, 8, 1, nullptr},
    {"m_iPlayer", OriginalSendPropKind::Int, 12, 12, 6, 1, nullptr},
    {"m_flSpread", OriginalSendPropKind::Float, 52, 52, 10, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEFizz[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_nEntity", OriginalSendPropKind::Int, 12, 12, 11, 1, nullptr},
    {"m_nModelIndex", OriginalSendPropKind::Int, 16, 16, 11, 0, nullptr},
    {"m_nDensity", OriginalSendPropKind::Int, 20, 20, 8, 1, nullptr},
    {"m_nCurrent", OriginalSendPropKind::Int, 24, 24, 16, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEFootprintDecal[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_vecDirection", OriginalSendPropKind::Vector, 24, 24, -1, 2, nullptr},
    {"m_nEntity", OriginalSendPropKind::Int, 36, 36, 11, 1, nullptr},
    {"m_nIndex", OriginalSendPropKind::Int, 40, 40, 8, 1, nullptr},
    {"m_chMaterialType", OriginalSendPropKind::Int, 44, 44, 8, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEGaussExplosion[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_nType", OriginalSendPropKind::Int, 24, 24, 2, 1, nullptr},
    {"m_vecDirection", OriginalSendPropKind::Vector, 28, 28, -1, 2, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEGlowSprite[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_nModelIndex", OriginalSendPropKind::Int, 24, 24, 11, 0, nullptr},
    {"m_fScale", OriginalSendPropKind::Float, 28, 28, 8, 8, nullptr},
    {"m_fLife", OriginalSendPropKind::Float, 32, 32, 8, 8, nullptr},
    {"m_nBrightness", OriginalSendPropKind::Int, 36, 36, 8, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEImpact[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_vecNormal", OriginalSendPropKind::Vector, 24, 24, -1, 2, nullptr},
    {"m_iType", OriginalSendPropKind::Int, 36, 36, 32, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEKillPlayerAttachments[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_nPlayer", OriginalSendPropKind::Int, 12, 12, 5, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TELargeFunnel[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_nModelIndex", OriginalSendPropKind::Int, 24, 24, 11, 0, nullptr},
    {"m_nReversed", OriginalSendPropKind::Int, 28, 28, 2, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEMetalSparks[] = {
    {"m_vecPos", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_vecDir", OriginalSendPropKind::Vector, 24, 24, -1, 2, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEMuzzleFlash[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_vecAngles", OriginalSendPropKind::Vector, 24, 24, -1, 2, nullptr},
    {"m_flScale", OriginalSendPropKind::Float, 36, 36, -1, 4, nullptr},
    {"m_nType", OriginalSendPropKind::Int, 40, 40, 32, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEParticleSystem[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecOrigin[0]", OriginalSendPropKind::Float, -12, -1, -1, 2, nullptr},
    {"m_vecOrigin[1]", OriginalSendPropKind::Float, -16, -1, -1, 2, nullptr},
    {"m_vecOrigin[2]", OriginalSendPropKind::Float, -20, -1, -1, 2, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEPhysicsProp[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_angRotation[0]", OriginalSendPropKind::Angle, -24, -1, 13, 0, nullptr},
    {"m_angRotation[1]", OriginalSendPropKind::Angle, -28, -1, 13, 0, nullptr},
    {"m_angRotation[2]", OriginalSendPropKind::Angle, -32, -1, 13, 0, nullptr},
    {"m_vecVelocity", OriginalSendPropKind::Vector, 36, 36, -1, 2, nullptr},
    {"m_nModelIndex", OriginalSendPropKind::Int, 48, 48, 11, 0, nullptr},
    {"m_nSkin", OriginalSendPropKind::Int, 52, 52, 10, 0, nullptr},
    {"m_nFlags", OriginalSendPropKind::Int, 56, 56, 2, 1, nullptr},
    {"m_nEffects", OriginalSendPropKind::Int, 60, 60, 10, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEPlantBomb[] = {
    {"m_vecOrigin", OriginalSendPropKind::Vector, 16, 16, -1, 2, nullptr},
    {"m_iPlayer", OriginalSendPropKind::Int, 12, 12, 6, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEPlayerAnimEvent[] = {
    {"m_hPlayer", OriginalSendPropKind::EHandle, 12, 12, -1, 0, nullptr},
    {"m_iEvent", OriginalSendPropKind::Int, 16, 16, -1, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEPlayerDecal[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 16, 16, -1, 2, nullptr},
    {"m_nEntity", OriginalSendPropKind::Int, 28, 28, 11, 1, nullptr},
    {"m_nPlayer", OriginalSendPropKind::Int, 12, 12, -1, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEProjectedDecal[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_angRotation", OriginalSendPropKind::QAngles, 32, 32, 10, 0, nullptr},
    {"m_flDistance", OriginalSendPropKind::Float, 28, 28, 10, 16, nullptr},
    {"m_nIndex", OriginalSendPropKind::Int, 24, 24, 9, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TERadioIcon[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_iAttachToClient", OriginalSendPropKind::Int, 12, 12, 8, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEShatterSurface[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_vecAngles", OriginalSendPropKind::Vector, 24, 24, -1, 2, nullptr},
    {"m_vecForce", OriginalSendPropKind::Vector, 36, 36, -1, 2, nullptr},
    {"m_vecForcePos", OriginalSendPropKind::Vector, 48, 48, -1, 2, nullptr},
    {"m_flWidth", OriginalSendPropKind::Float, 60, 60, 0, 4, nullptr},
    {"m_flHeight", OriginalSendPropKind::Float, 64, 64, 0, 4, nullptr},
    {"m_flShardSize", OriginalSendPropKind::Float, 68, 68, 0, 4, nullptr},
    {"m_nSurfaceType", OriginalSendPropKind::Int, 72, 72, 2, 1, nullptr},
    {"m_uchFrontColor[0]", OriginalSendPropKind::Int, 76, 76, 8, 1, nullptr},
    {"m_uchFrontColor[1]", OriginalSendPropKind::Int, 77, 77, 8, 1, nullptr},
    {"m_uchFrontColor[2]", OriginalSendPropKind::Int, 78, 78, 8, 1, nullptr},
    {"m_uchBackColor[0]", OriginalSendPropKind::Int, 79, 79, 8, 1, nullptr},
    {"m_uchBackColor[1]", OriginalSendPropKind::Int, 80, 80, 8, 1, nullptr},
    {"m_uchBackColor[2]", OriginalSendPropKind::Int, 81, 81, 8, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEShowLine[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecEnd", OriginalSendPropKind::Vector, 24, 24, -1, 2, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TESmoke[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_nModelIndex", OriginalSendPropKind::Int, 24, 24, 11, 0, nullptr},
    {"m_fScale", OriginalSendPropKind::Float, 28, 28, 8, 8, nullptr},
    {"m_nFrameRate", OriginalSendPropKind::Int, 32, 32, 8, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TESparks[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_nMagnitude", OriginalSendPropKind::Int, 24, 24, 4, 1, nullptr},
    {"m_nTrailLength", OriginalSendPropKind::Int, 28, 28, 4, 1, nullptr},
    {"m_vecDir", OriginalSendPropKind::Vector, 32, 32, -1, 2, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TESprite[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_nModelIndex", OriginalSendPropKind::Int, 24, 24, 11, 0, nullptr},
    {"m_fScale", OriginalSendPropKind::Float, 28, 28, 8, 8, nullptr},
    {"m_nBrightness", OriginalSendPropKind::Int, 32, 32, 8, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TESpriteSpray[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_vecDirection", OriginalSendPropKind::Vector, 24, 24, -1, 2, nullptr},
    {"m_nModelIndex", OriginalSendPropKind::Int, 36, 36, 11, 0, nullptr},
    {"m_fNoise", OriginalSendPropKind::Float, 44, 44, 8, 8, nullptr},
    {"m_nSpeed", OriginalSendPropKind::Int, 40, 40, 8, 1, nullptr},
    {"m_nCount", OriginalSendPropKind::Int, 48, 48, 8, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TEWorldDecal[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 12, 12, -1, 2, nullptr},
    {"m_nIndex", OriginalSendPropKind::Int, 24, 24, 9, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_Team[] = {
    {"m_iTeamNum", OriginalSendPropKind::Int, 952, 952, 5, 0, nullptr},
    {"m_iScore", OriginalSendPropKind::Int, 940, 940, 0, 0, nullptr},
    {"m_szTeamname", OriginalSendPropKind::String, 908, 908, 0, 0, nullptr},
    {"player_array_element", OriginalSendPropKind::Int, 0, 0, 10, 1, nullptr},
    {"\"player_array\"", OriginalSendPropKind::InternalArray, 0, 0, -1, 0, nullptr, 64, 0, true},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_Tesla[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_SoundName", OriginalSendPropKind::StringT, 872, 872, -1, -1, nullptr},
    {"m_iszSpriteName", OriginalSendPropKind::StringT, 920, 920, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_TestTraceline[] = {
    {"m_clrRender", OriginalSendPropKind::Int, 40, 40, 32, 1, nullptr},
    {"m_vecOrigin", OriginalSendPropKind::Vector, 816, 816, 19, 0, nullptr},
    {"m_angRotation[0]", OriginalSendPropKind::Float, -828, -1, 19, 0, nullptr},
    {"m_angRotation[1]", OriginalSendPropKind::Float, -832, -1, 19, 0, nullptr},
    {"m_angRotation[2]", OriginalSendPropKind::Float, -836, -1, 19, 0, nullptr},
    {"moveparent", OriginalSendPropKind::EHandle, 244, 244, -1, 0, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_VGuiScreen[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flWidth", OriginalSendPropKind::Float, 872, 872, 0, 4, nullptr},
    {"m_flHeight", OriginalSendPropKind::Float, 876, 876, 0, 4, nullptr},
    {"m_nAttachmentIndex", OriginalSendPropKind::Int, 884, 884, 5, 1, nullptr},
    {"m_nPanelName", OriginalSendPropKind::Int, 880, 880, 8, 1, nullptr},
    {"m_fScreenFlags", OriginalSendPropKind::Int, 892, 892, 3, 1, nullptr},
    {"m_nOverlayMaterial", OriginalSendPropKind::Int, 888, 888, 10, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WORLD[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flWaveHeight", OriginalSendPropKind::Float, 872, 872, 8, 16, nullptr},
    {"m_WorldMins", OriginalSendPropKind::Vector, 876, 876, -1, 2, nullptr},
    {"m_WorldMaxs", OriginalSendPropKind::Vector, 888, 888, -1, 2, nullptr},
    {"m_bStartDark", OriginalSendPropKind::Int, 920, 920, 1, 1, nullptr},
    {"m_flMaxOccludeeArea", OriginalSendPropKind::Float, 900, 900, 0, 4, nullptr},
    {"m_flMinOccluderArea", OriginalSendPropKind::Float, 904, 904, 0, 4, nullptr},
    {"m_flMaxPropScreenSpaceWidth", OriginalSendPropKind::Float, 912, 912, 0, 4, nullptr},
    {"m_flMinPropScreenSpaceWidth", OriginalSendPropKind::Float, 908, 908, 0, 4, nullptr},
    {"m_iszDetailSpriteMaterial", OriginalSendPropKind::StringT, 916, 916, -1, -1, nullptr},
    {"m_bColdWorld", OriginalSendPropKind::Int, 921, 921, 1, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WaterBullet[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WaterLODControl[] = {
    {"m_flCheapWaterStartDistance", OriginalSendPropKind::Float, 868, 868, 0, 4, nullptr},
    {"m_flCheapWaterEndDistance", OriginalSendPropKind::Float, 872, 872, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponAK47[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponAWP[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponAug[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponC4[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_bStartedArming", OriginalSendPropKind::Bool, 1392, 1392, -1, -1, nullptr},
    {"m_bBombPlacedAnimation", OriginalSendPropKind::Bool, 1400, 1400, -1, -1, nullptr},
    {"m_fArmedTime", OriginalSendPropKind::Float, 1396, 1396, 0, 4, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponCSBase[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_flAnimTime", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_AnimTimeMustBeFirst"},
    {"m_nSequence", OriginalSendPropKind::Exclude, -1, -1, -1, -1, "DT_BaseAnimating"},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponCSBaseGun[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponCubemap[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponCycler[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponDEagle[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponElite[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_bFireRight", OriginalSendPropKind::Bool, 1396, 1396, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponFamas[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_bBurstMode", OriginalSendPropKind::Bool, 1396, 1396, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponFiveSeven[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponG3SG1[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponGalil[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponGlock[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_bBurstMode", OriginalSendPropKind::Bool, 1392, 1392, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponKnife[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"LocalActiveWeaponKnifeData", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponM249[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponM3[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_fInSpecialReload", OriginalSendPropKind::Int, 1396, 1396, 2, 1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponM4A1[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_bSilencerOn", OriginalSendPropKind::Bool, 1396, 1396, -1, -1, nullptr},
    {"m_flDoneSwitchingSilencer", OriginalSendPropKind::Time, 1400, 1400, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponMAC10[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponMP5Navy[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponP228[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponP90[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponSG550[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponSG552[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponScout[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponTMP[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponUMP45[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponUSP[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_bSilencerOn", OriginalSendPropKind::Bool, 1392, 1392, -1, -1, nullptr},
    {"m_flDoneSwitchingSilencer", OriginalSendPropKind::Time, 1396, 1396, -1, -1, nullptr},
};

static constexpr OriginalSendPropDescriptor kOriginalProps_WeaponXM1014[] = {
    {"baseclass", OriginalSendPropKind::DataTable, 0, 0, -1, -1, nullptr},
    {"m_fInSpecialReload", OriginalSendPropKind::Int, 1396, 1396, 2, 1, nullptr},
};

static constexpr OriginalSendTableMetadata kOriginalSendTableMetadata[] = {
    {"DT_AI_BaseNPC", kOriginalProps_AI_BaseNPC, sizeof(kOriginalProps_AI_BaseNPC) / sizeof(kOriginalProps_AI_BaseNPC[0])},
    {"DT_AnimTimeMustBeFirst", kOriginalProps_AnimTimeMustBeFirst, sizeof(kOriginalProps_AnimTimeMustBeFirst) / sizeof(kOriginalProps_AnimTimeMustBeFirst[0])},
    {"DT_Animationlayer", kOriginalProps_Animationlayer, sizeof(kOriginalProps_Animationlayer) / sizeof(kOriginalProps_Animationlayer[0])},
    {"DT_BCCLocalPlayerExclusive", kOriginalProps_BCCLocalPlayerExclusive, sizeof(kOriginalProps_BCCLocalPlayerExclusive) / sizeof(kOriginalProps_BCCLocalPlayerExclusive[0])},
    {"DT_BaseAnimating", kOriginalProps_BaseAnimating, sizeof(kOriginalProps_BaseAnimating) / sizeof(kOriginalProps_BaseAnimating[0])},
    {"DT_BaseAnimatingOverlay", kOriginalProps_BaseAnimatingOverlay, sizeof(kOriginalProps_BaseAnimatingOverlay) / sizeof(kOriginalProps_BaseAnimatingOverlay[0])},
    {"DT_BaseBeam", kOriginalProps_BaseBeam, sizeof(kOriginalProps_BaseBeam) / sizeof(kOriginalProps_BaseBeam[0])},
    {"DT_BaseCSGrenade", kOriginalProps_BaseCSGrenade, sizeof(kOriginalProps_BaseCSGrenade) / sizeof(kOriginalProps_BaseCSGrenade[0])},
    {"DT_BaseCSGrenadeProjectile", kOriginalProps_BaseCSGrenadeProjectile, sizeof(kOriginalProps_BaseCSGrenadeProjectile) / sizeof(kOriginalProps_BaseCSGrenadeProjectile[0])},
    {"DT_BaseCombatCharacter", kOriginalProps_BaseCombatCharacter, sizeof(kOriginalProps_BaseCombatCharacter) / sizeof(kOriginalProps_BaseCombatCharacter[0])},
    {"DT_BaseCombatWeapon", kOriginalProps_BaseCombatWeapon, sizeof(kOriginalProps_BaseCombatWeapon) / sizeof(kOriginalProps_BaseCombatWeapon[0])},
    {"DT_BaseDoor", kOriginalProps_BaseDoor, sizeof(kOriginalProps_BaseDoor) / sizeof(kOriginalProps_BaseDoor[0])},
    {"DT_BaseEntity", kOriginalProps_BaseEntity, sizeof(kOriginalProps_BaseEntity) / sizeof(kOriginalProps_BaseEntity[0])},
    {"DT_BaseFlex", kOriginalProps_BaseFlex, sizeof(kOriginalProps_BaseFlex) / sizeof(kOriginalProps_BaseFlex[0])},
    {"DT_BaseGrenade", kOriginalProps_BaseGrenade, sizeof(kOriginalProps_BaseGrenade) / sizeof(kOriginalProps_BaseGrenade[0])},
    {"DT_BaseParticleEntity", kOriginalProps_BaseParticleEntity, sizeof(kOriginalProps_BaseParticleEntity) / sizeof(kOriginalProps_BaseParticleEntity[0])},
    {"DT_BasePlayer", kOriginalProps_BasePlayer, sizeof(kOriginalProps_BasePlayer) / sizeof(kOriginalProps_BasePlayer[0])},
    {"DT_BasePropDoor", kOriginalProps_BasePropDoor, sizeof(kOriginalProps_BasePropDoor) / sizeof(kOriginalProps_BasePropDoor[0])},
    {"DT_BaseTempEntity", nullptr, 0},
    {"DT_BaseViewModel", kOriginalProps_BaseViewModel, sizeof(kOriginalProps_BaseViewModel) / sizeof(kOriginalProps_BaseViewModel[0])},
    {"DT_Beam", kOriginalProps_Beam, sizeof(kOriginalProps_Beam) / sizeof(kOriginalProps_Beam[0])},
    {"DT_BeamPredictableId", kOriginalProps_BeamPredictableId, sizeof(kOriginalProps_BeamPredictableId) / sizeof(kOriginalProps_BeamPredictableId[0])},
    {"DT_BoneFollower", kOriginalProps_BoneFollower, sizeof(kOriginalProps_BoneFollower) / sizeof(kOriginalProps_BoneFollower[0])},
    {"DT_BreakableProp", kOriginalProps_BreakableProp, sizeof(kOriginalProps_BreakableProp) / sizeof(kOriginalProps_BreakableProp[0])},
    {"DT_BreakableSurface", kOriginalProps_BreakableSurface, sizeof(kOriginalProps_BreakableSurface) / sizeof(kOriginalProps_BreakableSurface[0])},
    {"DT_CFish", kOriginalProps_CFish, sizeof(kOriginalProps_CFish) / sizeof(kOriginalProps_CFish[0])},
    {"DT_CHostage", kOriginalProps_CHostage, sizeof(kOriginalProps_CHostage) / sizeof(kOriginalProps_CHostage[0])},
    {"DT_CSGameRules", kOriginalProps_CSGameRules, sizeof(kOriginalProps_CSGameRules) / sizeof(kOriginalProps_CSGameRules[0])},
    {"DT_CSGameRulesProxy", kOriginalProps_CSGameRulesProxy, sizeof(kOriginalProps_CSGameRulesProxy) / sizeof(kOriginalProps_CSGameRulesProxy[0])},
    {"DT_CSLocalPlayerExclusive", kOriginalProps_CSLocalPlayerExclusive, sizeof(kOriginalProps_CSLocalPlayerExclusive) / sizeof(kOriginalProps_CSLocalPlayerExclusive[0])},
    {"DT_CSNonLocalPlayerExclusive", kOriginalProps_CSNonLocalPlayerExclusive, sizeof(kOriginalProps_CSNonLocalPlayerExclusive) / sizeof(kOriginalProps_CSNonLocalPlayerExclusive[0])},
    {"DT_CSPlayer", kOriginalProps_CSPlayer, sizeof(kOriginalProps_CSPlayer) / sizeof(kOriginalProps_CSPlayer[0])},
    {"DT_CSPlayerResource", kOriginalProps_CSPlayerResource, sizeof(kOriginalProps_CSPlayerResource) / sizeof(kOriginalProps_CSPlayerResource[0])},
    {"DT_CSRagdoll", kOriginalProps_CSRagdoll, sizeof(kOriginalProps_CSRagdoll) / sizeof(kOriginalProps_CSRagdoll[0])},
    {"DT_CSTeam", kOriginalProps_CSTeam, sizeof(kOriginalProps_CSTeam) / sizeof(kOriginalProps_CSTeam[0])},
    {"DT_CollisionProperty", kOriginalProps_CollisionProperty, sizeof(kOriginalProps_CollisionProperty) / sizeof(kOriginalProps_CollisionProperty[0])},
    {"DT_ColorCorrection", kOriginalProps_ColorCorrection, sizeof(kOriginalProps_ColorCorrection) / sizeof(kOriginalProps_ColorCorrection[0])},
    {"DT_ColorCorrectionVolume", kOriginalProps_ColorCorrectionVolume, sizeof(kOriginalProps_ColorCorrectionVolume) / sizeof(kOriginalProps_ColorCorrectionVolume[0])},
    {"DT_DetailController", kOriginalProps_DetailController, sizeof(kOriginalProps_DetailController) / sizeof(kOriginalProps_DetailController[0])},
    {"DT_DynamicLight", kOriginalProps_DynamicLight, sizeof(kOriginalProps_DynamicLight) / sizeof(kOriginalProps_DynamicLight[0])},
    {"DT_DynamicProp", kOriginalProps_DynamicProp, sizeof(kOriginalProps_DynamicProp) / sizeof(kOriginalProps_DynamicProp[0])},
    {"DT_EffectData", kOriginalProps_EffectData, sizeof(kOriginalProps_EffectData) / sizeof(kOriginalProps_EffectData[0])},
    {"DT_Embers", kOriginalProps_Embers, sizeof(kOriginalProps_Embers) / sizeof(kOriginalProps_Embers[0])},
    {"DT_EntityDissolve", kOriginalProps_EntityDissolve, sizeof(kOriginalProps_EntityDissolve) / sizeof(kOriginalProps_EntityDissolve[0])},
    {"DT_EntityFlame", kOriginalProps_EntityFlame, sizeof(kOriginalProps_EntityFlame) / sizeof(kOriginalProps_EntityFlame[0])},
    {"DT_EntityParticleTrail", kOriginalProps_EntityParticleTrail, sizeof(kOriginalProps_EntityParticleTrail) / sizeof(kOriginalProps_EntityParticleTrail[0])},
    {"DT_EntityParticleTrailInfo", kOriginalProps_EntityParticleTrailInfo, sizeof(kOriginalProps_EntityParticleTrailInfo) / sizeof(kOriginalProps_EntityParticleTrailInfo[0])},
    {"DT_EnvParticleScript", kOriginalProps_EnvParticleScript, sizeof(kOriginalProps_EnvParticleScript) / sizeof(kOriginalProps_EnvParticleScript[0])},
    {"DT_EnvProjectedTexture", kOriginalProps_EnvProjectedTexture, sizeof(kOriginalProps_EnvProjectedTexture) / sizeof(kOriginalProps_EnvProjectedTexture[0])},
    {"DT_EnvScreenEffect", kOriginalProps_EnvScreenEffect, sizeof(kOriginalProps_EnvScreenEffect) / sizeof(kOriginalProps_EnvScreenEffect[0])},
    {"DT_EnvScreenOverlay", kOriginalProps_EnvScreenOverlay, sizeof(kOriginalProps_EnvScreenOverlay) / sizeof(kOriginalProps_EnvScreenOverlay[0])},
    {"DT_EnvTonemapController", kOriginalProps_EnvTonemapController, sizeof(kOriginalProps_EnvTonemapController) / sizeof(kOriginalProps_EnvTonemapController[0])},
    {"DT_EnvWind", kOriginalProps_EnvWind, sizeof(kOriginalProps_EnvWind) / sizeof(kOriginalProps_EnvWind[0])},
    {"DT_EnvWindShared", kOriginalProps_EnvWindShared, sizeof(kOriginalProps_EnvWindShared) / sizeof(kOriginalProps_EnvWindShared[0])},
    {"DT_FireSmoke", kOriginalProps_FireSmoke, sizeof(kOriginalProps_FireSmoke) / sizeof(kOriginalProps_FireSmoke[0])},
    {"DT_Flashbang", kOriginalProps_Flashbang, sizeof(kOriginalProps_Flashbang) / sizeof(kOriginalProps_Flashbang[0])},
    {"DT_FootstepControl", kOriginalProps_FootstepControl, sizeof(kOriginalProps_FootstepControl) / sizeof(kOriginalProps_FootstepControl[0])},
    {"DT_FuncAreaPortalWindow", kOriginalProps_FuncAreaPortalWindow, sizeof(kOriginalProps_FuncAreaPortalWindow) / sizeof(kOriginalProps_FuncAreaPortalWindow[0])},
    {"DT_FuncConveyor", kOriginalProps_FuncConveyor, sizeof(kOriginalProps_FuncConveyor) / sizeof(kOriginalProps_FuncConveyor[0])},
    {"DT_FuncLadder", kOriginalProps_FuncLadder, sizeof(kOriginalProps_FuncLadder) / sizeof(kOriginalProps_FuncLadder[0])},
    {"DT_FuncMonitor", kOriginalProps_FuncMonitor, sizeof(kOriginalProps_FuncMonitor) / sizeof(kOriginalProps_FuncMonitor[0])},
    {"DT_FuncOccluder", kOriginalProps_FuncOccluder, sizeof(kOriginalProps_FuncOccluder) / sizeof(kOriginalProps_FuncOccluder[0])},
    {"DT_FuncSmokeVolume", kOriginalProps_FuncSmokeVolume, sizeof(kOriginalProps_FuncSmokeVolume) / sizeof(kOriginalProps_FuncSmokeVolume[0])},
    {"DT_FuncTrackTrain", kOriginalProps_FuncTrackTrain, sizeof(kOriginalProps_FuncTrackTrain) / sizeof(kOriginalProps_FuncTrackTrain[0])},
    {"DT_Func_Dust", kOriginalProps_Func_Dust, sizeof(kOriginalProps_Func_Dust) / sizeof(kOriginalProps_Func_Dust[0])},
    {"DT_Func_LOD", kOriginalProps_Func_LOD, sizeof(kOriginalProps_Func_LOD) / sizeof(kOriginalProps_Func_LOD[0])},
    {"DT_GameRulesProxy", nullptr, 0},
    {"DT_HEGrenade", kOriginalProps_HEGrenade, sizeof(kOriginalProps_HEGrenade) / sizeof(kOriginalProps_HEGrenade[0])},
    {"DT_HandleTest", kOriginalProps_HandleTest, sizeof(kOriginalProps_HandleTest) / sizeof(kOriginalProps_HandleTest[0])},
    {"DT_InfoLadderDismount", kOriginalProps_InfoLadderDismount, sizeof(kOriginalProps_InfoLadderDismount) / sizeof(kOriginalProps_InfoLadderDismount[0])},
    {"DT_InfoLightingRelative", kOriginalProps_InfoLightingRelative, sizeof(kOriginalProps_InfoLightingRelative) / sizeof(kOriginalProps_InfoLightingRelative[0])},
    {"DT_LightGlow", kOriginalProps_LightGlow, sizeof(kOriginalProps_LightGlow) / sizeof(kOriginalProps_LightGlow[0])},
    {"DT_Local", kOriginalProps_Local, sizeof(kOriginalProps_Local) / sizeof(kOriginalProps_Local[0])},
    {"DT_LocalActiveWeaponData", kOriginalProps_LocalActiveWeaponData, sizeof(kOriginalProps_LocalActiveWeaponData) / sizeof(kOriginalProps_LocalActiveWeaponData[0])},
    {"DT_LocalActiveWeaponKnifeData", kOriginalProps_LocalActiveWeaponKnifeData, sizeof(kOriginalProps_LocalActiveWeaponKnifeData) / sizeof(kOriginalProps_LocalActiveWeaponKnifeData[0])},
    {"DT_LocalPlayerExclusive", kOriginalProps_LocalPlayerExclusive, sizeof(kOriginalProps_LocalPlayerExclusive) / sizeof(kOriginalProps_LocalPlayerExclusive[0])},
    {"DT_LocalWeaponData", kOriginalProps_LocalWeaponData, sizeof(kOriginalProps_LocalWeaponData) / sizeof(kOriginalProps_LocalWeaponData[0])},
    {"DT_MaterialModifyControl", kOriginalProps_MaterialModifyControl, sizeof(kOriginalProps_MaterialModifyControl) / sizeof(kOriginalProps_MaterialModifyControl[0])},
    {"DT_MovieExplosion", kOriginalProps_MovieExplosion, sizeof(kOriginalProps_MovieExplosion) / sizeof(kOriginalProps_MovieExplosion[0])},
    {"DT_OverlayVars", kOriginalProps_OverlayVars, sizeof(kOriginalProps_OverlayVars) / sizeof(kOriginalProps_OverlayVars[0])},
    {"DT_ParticleFire", kOriginalProps_ParticleFire, sizeof(kOriginalProps_ParticleFire) / sizeof(kOriginalProps_ParticleFire[0])},
    {"DT_ParticleSmokeGrenade", kOriginalProps_ParticleSmokeGrenade, sizeof(kOriginalProps_ParticleSmokeGrenade) / sizeof(kOriginalProps_ParticleSmokeGrenade[0])},
    {"DT_PhysBox", kOriginalProps_PhysBox, sizeof(kOriginalProps_PhysBox) / sizeof(kOriginalProps_PhysBox[0])},
    {"DT_PhysBoxMultiplayer", kOriginalProps_PhysBoxMultiplayer, sizeof(kOriginalProps_PhysBoxMultiplayer) / sizeof(kOriginalProps_PhysBoxMultiplayer[0])},
    {"DT_PhysMagnet", kOriginalProps_PhysMagnet, sizeof(kOriginalProps_PhysMagnet) / sizeof(kOriginalProps_PhysMagnet[0])},
    {"DT_PhysicsProp", kOriginalProps_PhysicsProp, sizeof(kOriginalProps_PhysicsProp) / sizeof(kOriginalProps_PhysicsProp[0])},
    {"DT_PhysicsPropMultiplayer", kOriginalProps_PhysicsPropMultiplayer, sizeof(kOriginalProps_PhysicsPropMultiplayer) / sizeof(kOriginalProps_PhysicsPropMultiplayer[0])},
    {"DT_PlantedC4", kOriginalProps_PlantedC4, sizeof(kOriginalProps_PlantedC4) / sizeof(kOriginalProps_PlantedC4[0])},
    {"DT_Plasma", kOriginalProps_Plasma, sizeof(kOriginalProps_Plasma) / sizeof(kOriginalProps_Plasma[0])},
    {"DT_PlayerResource", kOriginalProps_PlayerResource, sizeof(kOriginalProps_PlayerResource) / sizeof(kOriginalProps_PlayerResource[0])},
    {"DT_PlayerState", kOriginalProps_PlayerState, sizeof(kOriginalProps_PlayerState) / sizeof(kOriginalProps_PlayerState[0])},
    {"DT_PointCamera", kOriginalProps_PointCamera, sizeof(kOriginalProps_PointCamera) / sizeof(kOriginalProps_PointCamera[0])},
    {"DT_PointCommentaryNode", kOriginalProps_PointCommentaryNode, sizeof(kOriginalProps_PointCommentaryNode) / sizeof(kOriginalProps_PointCommentaryNode[0])},
    {"DT_PoseController", kOriginalProps_PoseController, sizeof(kOriginalProps_PoseController) / sizeof(kOriginalProps_PoseController[0])},
    {"DT_Precipitation", kOriginalProps_Precipitation, sizeof(kOriginalProps_Precipitation) / sizeof(kOriginalProps_Precipitation[0])},
    {"DT_PredictableId", kOriginalProps_PredictableId, sizeof(kOriginalProps_PredictableId) / sizeof(kOriginalProps_PredictableId[0])},
    {"DT_PredictedViewModel", kOriginalProps_PredictedViewModel, sizeof(kOriginalProps_PredictedViewModel) / sizeof(kOriginalProps_PredictedViewModel[0])},
    {"DT_PropJeep", kOriginalProps_PropJeep, sizeof(kOriginalProps_PropJeep) / sizeof(kOriginalProps_PropJeep[0])},
    {"DT_PropVehicleDriveable", kOriginalProps_PropVehicleDriveable, sizeof(kOriginalProps_PropVehicleDriveable) / sizeof(kOriginalProps_PropVehicleDriveable[0])},
    {"DT_ProxyToggle", kOriginalProps_ProxyToggle, sizeof(kOriginalProps_ProxyToggle) / sizeof(kOriginalProps_ProxyToggle[0])},
    {"DT_ProxyToggle_ProxiedData", kOriginalProps_ProxyToggle_ProxiedData, sizeof(kOriginalProps_ProxyToggle_ProxiedData) / sizeof(kOriginalProps_ProxyToggle_ProxiedData[0])},
    {"DT_QuadraticBeam", kOriginalProps_QuadraticBeam, sizeof(kOriginalProps_QuadraticBeam) / sizeof(kOriginalProps_QuadraticBeam[0])},
    {"DT_Ragdoll", kOriginalProps_Ragdoll, sizeof(kOriginalProps_Ragdoll) / sizeof(kOriginalProps_Ragdoll[0])},
    {"DT_RagdollManager", kOriginalProps_RagdollManager, sizeof(kOriginalProps_RagdollManager) / sizeof(kOriginalProps_RagdollManager[0])},
    {"DT_Ragdoll_Attached", kOriginalProps_Ragdoll_Attached, sizeof(kOriginalProps_Ragdoll_Attached) / sizeof(kOriginalProps_Ragdoll_Attached[0])},
    {"DT_RopeKeyframe", kOriginalProps_RopeKeyframe, sizeof(kOriginalProps_RopeKeyframe) / sizeof(kOriginalProps_RopeKeyframe[0])},
    {"DT_SceneEntity", kOriginalProps_SceneEntity, sizeof(kOriginalProps_SceneEntity) / sizeof(kOriginalProps_SceneEntity[0])},
    {"DT_ServerAnimationData", kOriginalProps_ServerAnimationData, sizeof(kOriginalProps_ServerAnimationData) / sizeof(kOriginalProps_ServerAnimationData[0])},
    {"DT_ShadowControl", kOriginalProps_ShadowControl, sizeof(kOriginalProps_ShadowControl) / sizeof(kOriginalProps_ShadowControl[0])},
    {"DT_SmokeGrenade", kOriginalProps_SmokeGrenade, sizeof(kOriginalProps_SmokeGrenade) / sizeof(kOriginalProps_SmokeGrenade[0])},
    {"DT_SmokeStack", kOriginalProps_SmokeStack, sizeof(kOriginalProps_SmokeStack) / sizeof(kOriginalProps_SmokeStack[0])},
    {"DT_SpotlightEnd", kOriginalProps_SpotlightEnd, sizeof(kOriginalProps_SpotlightEnd) / sizeof(kOriginalProps_SpotlightEnd[0])},
    {"DT_Sprite", kOriginalProps_Sprite, sizeof(kOriginalProps_Sprite) / sizeof(kOriginalProps_Sprite[0])},
    {"DT_SpriteTrail", kOriginalProps_SpriteTrail, sizeof(kOriginalProps_SpriteTrail) / sizeof(kOriginalProps_SpriteTrail[0])},
    {"DT_SteamJet", kOriginalProps_SteamJet, sizeof(kOriginalProps_SteamJet) / sizeof(kOriginalProps_SteamJet[0])},
    {"DT_Sun", kOriginalProps_Sun, sizeof(kOriginalProps_Sun) / sizeof(kOriginalProps_Sun[0])},
    {"DT_TEArmorRicochet", kOriginalProps_TEArmorRicochet, sizeof(kOriginalProps_TEArmorRicochet) / sizeof(kOriginalProps_TEArmorRicochet[0])},
    {"DT_TEBSPDecal", kOriginalProps_TEBSPDecal, sizeof(kOriginalProps_TEBSPDecal) / sizeof(kOriginalProps_TEBSPDecal[0])},
    {"DT_TEBeamEntPoint", kOriginalProps_TEBeamEntPoint, sizeof(kOriginalProps_TEBeamEntPoint) / sizeof(kOriginalProps_TEBeamEntPoint[0])},
    {"DT_TEBeamEnts", kOriginalProps_TEBeamEnts, sizeof(kOriginalProps_TEBeamEnts) / sizeof(kOriginalProps_TEBeamEnts[0])},
    {"DT_TEBeamFollow", kOriginalProps_TEBeamFollow, sizeof(kOriginalProps_TEBeamFollow) / sizeof(kOriginalProps_TEBeamFollow[0])},
    {"DT_TEBeamLaser", kOriginalProps_TEBeamLaser, sizeof(kOriginalProps_TEBeamLaser) / sizeof(kOriginalProps_TEBeamLaser[0])},
    {"DT_TEBeamPoints", kOriginalProps_TEBeamPoints, sizeof(kOriginalProps_TEBeamPoints) / sizeof(kOriginalProps_TEBeamPoints[0])},
    {"DT_TEBeamRing", kOriginalProps_TEBeamRing, sizeof(kOriginalProps_TEBeamRing) / sizeof(kOriginalProps_TEBeamRing[0])},
    {"DT_TEBeamRingPoint", kOriginalProps_TEBeamRingPoint, sizeof(kOriginalProps_TEBeamRingPoint) / sizeof(kOriginalProps_TEBeamRingPoint[0])},
    {"DT_TEBeamSpline", kOriginalProps_TEBeamSpline, sizeof(kOriginalProps_TEBeamSpline) / sizeof(kOriginalProps_TEBeamSpline[0])},
    {"DT_TEBloodSprite", kOriginalProps_TEBloodSprite, sizeof(kOriginalProps_TEBloodSprite) / sizeof(kOriginalProps_TEBloodSprite[0])},
    {"DT_TEBloodStream", kOriginalProps_TEBloodStream, sizeof(kOriginalProps_TEBloodStream) / sizeof(kOriginalProps_TEBloodStream[0])},
    {"DT_TEBreakModel", kOriginalProps_TEBreakModel, sizeof(kOriginalProps_TEBreakModel) / sizeof(kOriginalProps_TEBreakModel[0])},
    {"DT_TEBubbleTrail", kOriginalProps_TEBubbleTrail, sizeof(kOriginalProps_TEBubbleTrail) / sizeof(kOriginalProps_TEBubbleTrail[0])},
    {"DT_TEBubbles", kOriginalProps_TEBubbles, sizeof(kOriginalProps_TEBubbles) / sizeof(kOriginalProps_TEBubbles[0])},
    {"DT_TEDecal", kOriginalProps_TEDecal, sizeof(kOriginalProps_TEDecal) / sizeof(kOriginalProps_TEDecal[0])},
    {"DT_TEDust", kOriginalProps_TEDust, sizeof(kOriginalProps_TEDust) / sizeof(kOriginalProps_TEDust[0])},
    {"DT_TEDynamicLight", kOriginalProps_TEDynamicLight, sizeof(kOriginalProps_TEDynamicLight) / sizeof(kOriginalProps_TEDynamicLight[0])},
    {"DT_TEEffectDispatch", kOriginalProps_TEEffectDispatch, sizeof(kOriginalProps_TEEffectDispatch) / sizeof(kOriginalProps_TEEffectDispatch[0])},
    {"DT_TEEnergySplash", kOriginalProps_TEEnergySplash, sizeof(kOriginalProps_TEEnergySplash) / sizeof(kOriginalProps_TEEnergySplash[0])},
    {"DT_TEExplosion", kOriginalProps_TEExplosion, sizeof(kOriginalProps_TEExplosion) / sizeof(kOriginalProps_TEExplosion[0])},
    {"DT_TEFireBullets", kOriginalProps_TEFireBullets, sizeof(kOriginalProps_TEFireBullets) / sizeof(kOriginalProps_TEFireBullets[0])},
    {"DT_TEFizz", kOriginalProps_TEFizz, sizeof(kOriginalProps_TEFizz) / sizeof(kOriginalProps_TEFizz[0])},
    {"DT_TEFootprintDecal", kOriginalProps_TEFootprintDecal, sizeof(kOriginalProps_TEFootprintDecal) / sizeof(kOriginalProps_TEFootprintDecal[0])},
    {"DT_TEGaussExplosion", kOriginalProps_TEGaussExplosion, sizeof(kOriginalProps_TEGaussExplosion) / sizeof(kOriginalProps_TEGaussExplosion[0])},
    {"DT_TEGlowSprite", kOriginalProps_TEGlowSprite, sizeof(kOriginalProps_TEGlowSprite) / sizeof(kOriginalProps_TEGlowSprite[0])},
    {"DT_TEImpact", kOriginalProps_TEImpact, sizeof(kOriginalProps_TEImpact) / sizeof(kOriginalProps_TEImpact[0])},
    {"DT_TEKillPlayerAttachments", kOriginalProps_TEKillPlayerAttachments, sizeof(kOriginalProps_TEKillPlayerAttachments) / sizeof(kOriginalProps_TEKillPlayerAttachments[0])},
    {"DT_TELargeFunnel", kOriginalProps_TELargeFunnel, sizeof(kOriginalProps_TELargeFunnel) / sizeof(kOriginalProps_TELargeFunnel[0])},
    {"DT_TEMetalSparks", kOriginalProps_TEMetalSparks, sizeof(kOriginalProps_TEMetalSparks) / sizeof(kOriginalProps_TEMetalSparks[0])},
    {"DT_TEMuzzleFlash", kOriginalProps_TEMuzzleFlash, sizeof(kOriginalProps_TEMuzzleFlash) / sizeof(kOriginalProps_TEMuzzleFlash[0])},
    {"DT_TEParticleSystem", kOriginalProps_TEParticleSystem, sizeof(kOriginalProps_TEParticleSystem) / sizeof(kOriginalProps_TEParticleSystem[0])},
    {"DT_TEPhysicsProp", kOriginalProps_TEPhysicsProp, sizeof(kOriginalProps_TEPhysicsProp) / sizeof(kOriginalProps_TEPhysicsProp[0])},
    {"DT_TEPlantBomb", kOriginalProps_TEPlantBomb, sizeof(kOriginalProps_TEPlantBomb) / sizeof(kOriginalProps_TEPlantBomb[0])},
    {"DT_TEPlayerAnimEvent", kOriginalProps_TEPlayerAnimEvent, sizeof(kOriginalProps_TEPlayerAnimEvent) / sizeof(kOriginalProps_TEPlayerAnimEvent[0])},
    {"DT_TEPlayerDecal", kOriginalProps_TEPlayerDecal, sizeof(kOriginalProps_TEPlayerDecal) / sizeof(kOriginalProps_TEPlayerDecal[0])},
    {"DT_TEProjectedDecal", kOriginalProps_TEProjectedDecal, sizeof(kOriginalProps_TEProjectedDecal) / sizeof(kOriginalProps_TEProjectedDecal[0])},
    {"DT_TERadioIcon", kOriginalProps_TERadioIcon, sizeof(kOriginalProps_TERadioIcon) / sizeof(kOriginalProps_TERadioIcon[0])},
    {"DT_TEShatterSurface", kOriginalProps_TEShatterSurface, sizeof(kOriginalProps_TEShatterSurface) / sizeof(kOriginalProps_TEShatterSurface[0])},
    {"DT_TEShowLine", kOriginalProps_TEShowLine, sizeof(kOriginalProps_TEShowLine) / sizeof(kOriginalProps_TEShowLine[0])},
    {"DT_TESmoke", kOriginalProps_TESmoke, sizeof(kOriginalProps_TESmoke) / sizeof(kOriginalProps_TESmoke[0])},
    {"DT_TESparks", kOriginalProps_TESparks, sizeof(kOriginalProps_TESparks) / sizeof(kOriginalProps_TESparks[0])},
    {"DT_TESprite", kOriginalProps_TESprite, sizeof(kOriginalProps_TESprite) / sizeof(kOriginalProps_TESprite[0])},
    {"DT_TESpriteSpray", kOriginalProps_TESpriteSpray, sizeof(kOriginalProps_TESpriteSpray) / sizeof(kOriginalProps_TESpriteSpray[0])},
    {"DT_TEWorldDecal", kOriginalProps_TEWorldDecal, sizeof(kOriginalProps_TEWorldDecal) / sizeof(kOriginalProps_TEWorldDecal[0])},
    {"DT_Team", kOriginalProps_Team, sizeof(kOriginalProps_Team) / sizeof(kOriginalProps_Team[0])},
    {"DT_Tesla", kOriginalProps_Tesla, sizeof(kOriginalProps_Tesla) / sizeof(kOriginalProps_Tesla[0])},
    {"DT_TestTraceline", kOriginalProps_TestTraceline, sizeof(kOriginalProps_TestTraceline) / sizeof(kOriginalProps_TestTraceline[0])},
    {"DT_VGuiScreen", kOriginalProps_VGuiScreen, sizeof(kOriginalProps_VGuiScreen) / sizeof(kOriginalProps_VGuiScreen[0])},
    {"DT_WORLD", kOriginalProps_WORLD, sizeof(kOriginalProps_WORLD) / sizeof(kOriginalProps_WORLD[0])},
    {"DT_WaterBullet", kOriginalProps_WaterBullet, sizeof(kOriginalProps_WaterBullet) / sizeof(kOriginalProps_WaterBullet[0])},
    {"DT_WaterLODControl", kOriginalProps_WaterLODControl, sizeof(kOriginalProps_WaterLODControl) / sizeof(kOriginalProps_WaterLODControl[0])},
    {"DT_WeaponAK47", kOriginalProps_WeaponAK47, sizeof(kOriginalProps_WeaponAK47) / sizeof(kOriginalProps_WeaponAK47[0])},
    {"DT_WeaponAWP", kOriginalProps_WeaponAWP, sizeof(kOriginalProps_WeaponAWP) / sizeof(kOriginalProps_WeaponAWP[0])},
    {"DT_WeaponAug", kOriginalProps_WeaponAug, sizeof(kOriginalProps_WeaponAug) / sizeof(kOriginalProps_WeaponAug[0])},
    {"DT_WeaponC4", kOriginalProps_WeaponC4, sizeof(kOriginalProps_WeaponC4) / sizeof(kOriginalProps_WeaponC4[0])},
    {"DT_WeaponCSBase", kOriginalProps_WeaponCSBase, sizeof(kOriginalProps_WeaponCSBase) / sizeof(kOriginalProps_WeaponCSBase[0])},
    {"DT_WeaponCSBaseGun", kOriginalProps_WeaponCSBaseGun, sizeof(kOriginalProps_WeaponCSBaseGun) / sizeof(kOriginalProps_WeaponCSBaseGun[0])},
    {"DT_WeaponCubemap", kOriginalProps_WeaponCubemap, sizeof(kOriginalProps_WeaponCubemap) / sizeof(kOriginalProps_WeaponCubemap[0])},
    {"DT_WeaponCycler", kOriginalProps_WeaponCycler, sizeof(kOriginalProps_WeaponCycler) / sizeof(kOriginalProps_WeaponCycler[0])},
    {"DT_WeaponDEagle", kOriginalProps_WeaponDEagle, sizeof(kOriginalProps_WeaponDEagle) / sizeof(kOriginalProps_WeaponDEagle[0])},
    {"DT_WeaponElite", kOriginalProps_WeaponElite, sizeof(kOriginalProps_WeaponElite) / sizeof(kOriginalProps_WeaponElite[0])},
    {"DT_WeaponFamas", kOriginalProps_WeaponFamas, sizeof(kOriginalProps_WeaponFamas) / sizeof(kOriginalProps_WeaponFamas[0])},
    {"DT_WeaponFiveSeven", kOriginalProps_WeaponFiveSeven, sizeof(kOriginalProps_WeaponFiveSeven) / sizeof(kOriginalProps_WeaponFiveSeven[0])},
    {"DT_WeaponG3SG1", kOriginalProps_WeaponG3SG1, sizeof(kOriginalProps_WeaponG3SG1) / sizeof(kOriginalProps_WeaponG3SG1[0])},
    {"DT_WeaponGalil", kOriginalProps_WeaponGalil, sizeof(kOriginalProps_WeaponGalil) / sizeof(kOriginalProps_WeaponGalil[0])},
    {"DT_WeaponGlock", kOriginalProps_WeaponGlock, sizeof(kOriginalProps_WeaponGlock) / sizeof(kOriginalProps_WeaponGlock[0])},
    {"DT_WeaponKnife", kOriginalProps_WeaponKnife, sizeof(kOriginalProps_WeaponKnife) / sizeof(kOriginalProps_WeaponKnife[0])},
    {"DT_WeaponM249", kOriginalProps_WeaponM249, sizeof(kOriginalProps_WeaponM249) / sizeof(kOriginalProps_WeaponM249[0])},
    {"DT_WeaponM3", kOriginalProps_WeaponM3, sizeof(kOriginalProps_WeaponM3) / sizeof(kOriginalProps_WeaponM3[0])},
    {"DT_WeaponM4A1", kOriginalProps_WeaponM4A1, sizeof(kOriginalProps_WeaponM4A1) / sizeof(kOriginalProps_WeaponM4A1[0])},
    {"DT_WeaponMAC10", kOriginalProps_WeaponMAC10, sizeof(kOriginalProps_WeaponMAC10) / sizeof(kOriginalProps_WeaponMAC10[0])},
    {"DT_WeaponMP5Navy", kOriginalProps_WeaponMP5Navy, sizeof(kOriginalProps_WeaponMP5Navy) / sizeof(kOriginalProps_WeaponMP5Navy[0])},
    {"DT_WeaponP228", kOriginalProps_WeaponP228, sizeof(kOriginalProps_WeaponP228) / sizeof(kOriginalProps_WeaponP228[0])},
    {"DT_WeaponP90", kOriginalProps_WeaponP90, sizeof(kOriginalProps_WeaponP90) / sizeof(kOriginalProps_WeaponP90[0])},
    {"DT_WeaponSG550", kOriginalProps_WeaponSG550, sizeof(kOriginalProps_WeaponSG550) / sizeof(kOriginalProps_WeaponSG550[0])},
    {"DT_WeaponSG552", kOriginalProps_WeaponSG552, sizeof(kOriginalProps_WeaponSG552) / sizeof(kOriginalProps_WeaponSG552[0])},
    {"DT_WeaponScout", kOriginalProps_WeaponScout, sizeof(kOriginalProps_WeaponScout) / sizeof(kOriginalProps_WeaponScout[0])},
    {"DT_WeaponTMP", kOriginalProps_WeaponTMP, sizeof(kOriginalProps_WeaponTMP) / sizeof(kOriginalProps_WeaponTMP[0])},
    {"DT_WeaponUMP45", kOriginalProps_WeaponUMP45, sizeof(kOriginalProps_WeaponUMP45) / sizeof(kOriginalProps_WeaponUMP45[0])},
    {"DT_WeaponUSP", kOriginalProps_WeaponUSP, sizeof(kOriginalProps_WeaponUSP) / sizeof(kOriginalProps_WeaponUSP[0])},
    {"DT_WeaponXM1014", kOriginalProps_WeaponXM1014, sizeof(kOriginalProps_WeaponXM1014) / sizeof(kOriginalProps_WeaponXM1014[0])},
};
