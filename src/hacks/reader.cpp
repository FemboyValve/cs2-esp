#include "reader.hpp"
#include <thread>
#include <map>
#include <cmath>
#include "classes/auto_updater.hpp"
#include "classes/config.hpp"
#include "classes/render.hpp"
#include "window.hpp"

static const std::unordered_map<std::string, int> boneMap = {
	{"head", 6},
	{"neck_0", 5},
	{"spine_1", 4},
	{"spine_2", 2},
	{"pelvis", 0},
	{"arm_upper_L", 8},
	{"arm_lower_L", 9},
	{"hand_L", 10},
	{"arm_upper_R", 13},
	{"arm_lower_R", 14},
	{"hand_R", 15},
	{"leg_upper_L", 22},
	{"leg_lower_L", 23},
	{"ankle_L", 24},
	{"leg_upper_R", 25},
	{"leg_lower_R", 26},
	{"ankle_R", 27}
};

// Constants for better performance
constexpr float HEAD_OFFSET = 75.0f;
constexpr size_t BONE_SIZE = 32;
constexpr int HEAD_BONE_INDEX = 6;

inline uintptr_t CC4::get_planted() {
	return g_game.process->read<uintptr_t>(g_game.process->read<uintptr_t>(g_game.base_client.base + updater::offsets::dwPlantedC4));
}

inline uintptr_t CC4::get_node() {
	return g_game.process->read<uintptr_t>(get_planted() + updater::offsets::m_pGameSceneNode);
}

Vector3 CC4::get_origin() {
	return g_game.process->read<Vector3>(get_node() + updater::offsets::m_vecAbsOrigin);
}

// CGame
void CGame::init() {
	CLOG_INFO("[cs2] Waiting for cs2.exe...");

	process = std::make_shared<pProcess>();
	while (!process->AttachProcess("cs2.exe"))
		std::this_thread::sleep_for(std::chrono::seconds(1));

	CLOG_INFO("[cs2] Attached to cs2.exe");

	do {
		base_client = process->GetModule("client.dll");
		base_engine = process->GetModule("engine2.dll");
		if (base_client.base == 0 || base_engine.base == 0) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			std::cout << "[cs2] Failed to find module client.dll/engine2.dll, waiting for the game to load it..." << std::endl;
		}
	} while (base_client.base == 0 || base_engine.base == 0);

	GetClientRect(process->hwnd_, &game_bounds);

	buildNumber = process->read<uintptr_t>(base_engine.base + updater::offsets::dwBuildNumber);
}

void CGame::close() {
	CLOG_INFO("Deatachig from process...");
	if (process->Close())
	{
		CLOG_INFO("Detachted from process successfully!");
	}
	else
	{
		CLOG_ERROR("Error while detaching from process!");
	}
}

void CGame::loop() {
	std::lock_guard<std::mutex> lock(reader_mutex);

	if (updater::offsets::dwLocalPlayerController == 0x0)
		throw std::runtime_error("Offsets have not been correctly set, cannot proceed.");

	inGame = false;
	isC4Planted = false;

	localPlayer = process->read<uintptr_t>(base_client.base + updater::offsets::dwLocalPlayerController);
	if (!localPlayer) return;

	localPlayerPawn = process->read<std::uint32_t>(localPlayer + updater::offsets::m_hPlayerPawn);
	if (!localPlayerPawn) return;

	entity_list = process->read<uintptr_t>(base_client.base + updater::offsets::dwEntityList);

	localList_entry2 = process->read<uintptr_t>(entity_list + 0x8 * ((localPlayerPawn & 0x7FFF) >> 9) + 16);
	localpCSPlayerPawn = process->read<uintptr_t>(localList_entry2 + 120 * (localPlayerPawn & 0x1FF));
	if (!localpCSPlayerPawn) return;

	view_matrix = process->read<view_matrix_t>(base_client.base + updater::offsets::dwViewMatrix);

	localTeam = process->read<int>(localPlayer + updater::offsets::m_iTeamNum);
	localOrigin = process->read<Vector3>(localpCSPlayerPawn + updater::offsets::m_vOldOrigin);
	isC4Planted = process->read<bool>(base_client.base + updater::offsets::dwPlantedC4 - 0x8);

	inGame = true;

	// Pre-allocate vector with reasonable capacity to avoid reallocations
	std::vector<CPlayer> list;
	list.reserve(32); // Typical max players in CS2

	// Cache config values to avoid repeated global access
	const bool teamEsp = config::team_esp;
	const bool showSkeletonEsp = config::show_skeleton_esp;
	const bool showHeadTracker = config::show_head_tracker;
	const bool showExtraFlags = config::show_extra_flags;
	const int renderDistance = config::render_distance;
	const bool NoFlashBool = false; // this doesnt work currently we can read but not write

	int playerIndex = 0;
	CPlayer player;
	uintptr_t list_entry, list_entry2, playerPawn, playerMoneyServices, clippingWeapon, weaponData;

	while (true) {
		playerIndex++;
		list_entry = process->read<uintptr_t>(entity_list + (8 * (playerIndex & 0x7FFF) >> 9) + 16);
		if (!list_entry) break;

		player.entity = process->read<uintptr_t>(list_entry + 120 * (playerIndex & 0x1FF));
		if (!player.entity) continue;

		player.team = process->read<int>(player.entity + updater::offsets::m_iTeamNum);
		if (teamEsp && (player.team == localTeam)) continue;

		playerPawn = process->read<std::uint32_t>(player.entity + updater::offsets::m_hPlayerPawn);

		list_entry2 = process->read<uintptr_t>(entity_list + 0x8 * ((playerPawn & 0x7FFF) >> 9) + 16);
		if (!list_entry2) continue;

		player.pCSPlayerPawn = process->read<uintptr_t>(list_entry2 + 120 * (playerPawn & 0x1FF));
		if (!player.pCSPlayerPawn) continue;

		player.health = process->read<int>(player.pCSPlayerPawn + updater::offsets::m_iHealth);
		if (player.health <= 0 || player.health > 100) continue;

		player.armor = process->read<int>(player.pCSPlayerPawn + updater::offsets::m_ArmorValue);

		if (teamEsp && (player.pCSPlayerPawn == localPlayer)) continue;

		// Read entity controller from the player pawn - optimized bit operations
		const uintptr_t handle = process->read<std::uintptr_t>(player.pCSPlayerPawn + updater::offsets::m_hController);
		const int index = handle & 0x7FFF;
		const int segment = index >> 9;
		const int entry = index & 0x1FF;

		const uintptr_t controllerListSegment = process->read<uintptr_t>(entity_list + 0x8 * segment + 0x10);
		const uintptr_t controller = process->read<uintptr_t>(controllerListSegment + 120 * entry);

		if (!controller) continue;

		// Read player name from the controller - use static buffer to avoid allocations
		static char nameBuffer[256];
		process->read_raw(controller + updater::offsets::m_iszPlayerName, nameBuffer, sizeof(nameBuffer) - 1);
		nameBuffer[sizeof(nameBuffer) - 1] = '\0';
		player.name = nameBuffer;

		player.origin = process->read<Vector3>(player.pCSPlayerPawn + updater::offsets::m_vOldOrigin);

		// Early origin validation
		if (player.origin.x == 0 && player.origin.y == 0) continue;
		if (player.origin.x == localOrigin.x && player.origin.y == localOrigin.y && player.origin.z == localOrigin.z) continue;

		// Distance check optimization - avoid sqrt when possible
		if (renderDistance != -1) {
			const float distanceSquared = (localOrigin - player.origin).length2d_squared();
			if (distanceSquared > renderDistance * renderDistance) continue;
		}

		// Pre-calculate head position
		player.head = { player.origin.x, player.origin.y, player.origin.z + HEAD_OFFSET };

		// Conditional bone reading based on what's actually needed
		if (showSkeletonEsp || showHeadTracker) {
			player.gameSceneNode = process->read<uintptr_t>(player.pCSPlayerPawn + updater::offsets::m_pGameSceneNode);
			player.boneArray = process->read<uintptr_t>(player.gameSceneNode + 0x1F0);

			if (showSkeletonEsp) {
				player.ReadBones();
			}
			else if (showHeadTracker) {
				player.ReadHead();
			}
		}

		if (showExtraFlags) {
			player.is_defusing = process->read<bool>(player.pCSPlayerPawn + updater::offsets::m_bIsDefusing);
			player.flashAlpha = process->read<float>(player.pCSPlayerPawn + updater::offsets::m_flFlashOverlayAlpha);

			playerMoneyServices = process->read<uintptr_t>(player.entity + updater::offsets::m_pInGameMoneyServices);
			player.money = process->read<int32_t>(playerMoneyServices + updater::offsets::m_iAccount);

			clippingWeapon = process->read<std::uint64_t>(player.pCSPlayerPawn + updater::offsets::m_pClippingWeapon);
			const std::uint64_t firstLevel = process->read<std::uint64_t>(clippingWeapon + 0x10);
			weaponData = process->read<std::uint64_t>(firstLevel + 0x20);

			static char weaponBuffer[MAX_PATH];
			process->read_raw(weaponData, weaponBuffer, sizeof(weaponBuffer));
			std::string weaponName = std::string(weaponBuffer);

			if (weaponName.length() > 7 && weaponName.compare(0, 7, "weapon_") == 0) {
				player.weapon = weaponName.substr(7);
			}
			else {
				player.weapon = "Invalid Weapon Name";
			}
		}

		list.push_back(std::move(player));
	}

	m_flashAlpha = process->read<float>(localpCSPlayerPawn + updater::offsets::m_flFlashOverlayAlpha);

	if (NoFlashBool)
	{
		process->write<float>(localpCSPlayerPawn + updater::offsets::m_flFlashOverlayAlpha, 0.0f);
	}

	// Use move semantics to avoid copy
	m_players = std::move(list);
}

void CPlayer::ReadHead() {
	const uintptr_t boneAddress = boneArray + HEAD_BONE_INDEX * BONE_SIZE;
	Vector3 bonePosition = g_game.process->read<Vector3>(boneAddress);
	bones.bonePositions["head"] = g_game.world_to_screen(&bonePosition);
}

void CPlayer::ReadBones() {
	// Pre-allocate map capacity
	for (const auto& [boneName, boneIndex] : boneMap) {
		bones.bonePositions.emplace(boneName, Vector3());
	}

	Vector3 bonePosition;
	for (const auto& [boneName, boneIndex] : boneMap) {
		const uintptr_t boneAddress = boneArray + boneIndex * BONE_SIZE;
		bonePosition = g_game.process->read<Vector3>(boneAddress);
		bones.bonePositions[boneName] = g_game.world_to_screen(&bonePosition);
	}
}

Vector3 CGame::world_to_screen(Vector3* v)
{
	__m128 vec = _mm_set_ps(1.0f, v->z, v->y, v->x);  // [w=1, z, y, x]

	__m128 row0 = _mm_loadu_ps(view_matrix[0]);
	__m128 row1 = _mm_loadu_ps(view_matrix[1]);
	__m128 row3 = _mm_loadu_ps(view_matrix[3]);

	float x = _mm_cvtss_f32(_mm_dp_ps(row0, vec, 0xF1));
	float y = _mm_cvtss_f32(_mm_dp_ps(row1, vec, 0xF1));
	float w = _mm_cvtss_f32(_mm_dp_ps(row3, vec, 0xF1));

	constexpr float MIN_W = 0.001f;
	if (w < MIN_W) return { 0, 0, -1 };

	float inv_w = 1.0f / w;
	float norm_x = x * inv_w;
	float norm_y = y * inv_w;

	float half_width = game_bounds.right * 0.5f;
	float half_height = game_bounds.bottom * 0.5f;

	float screen_x = half_width + norm_x * half_width;
	float screen_y = half_height - norm_y * half_height;

	return { screen_x, screen_y, w };
}