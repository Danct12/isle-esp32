/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include "bsp/esp-bsp.h"
#include "esp_log.h"

#include "isleapp.h"

#include "3dmanager/lego3dmanager.h"
#include "decomp.h"
#include "legoanimationmanager.h"
#include "legobuildingmanager.h"
#include "legogamestate.h"
#include "legoinputmanager.h"
#include "legomain.h"
#include "legomodelpresenter.h"
#include "legopartpresenter.h"
#include "legoutils.h"
#include "legovideomanager.h"
#include "legoworldpresenter.h"
#include "misc.h"
#include "mxbackgroundaudiomanager.h"
#include "mxdirectx/mxdirect3d.h"
#include "mxdsaction.h"
#include "mxmisc.h"
#include "mxomnicreateflags.h"
#include "mxomnicreateparam.h"
#include "mxstreamer.h"
#include "mxticklemanager.h"
#include "mxtimer.h"
#include "mxtransitionmanager.h"
#include "mxutilities.h"
#include "mxvariabletable.h"
#include "roi/legoroi.h"
#include "tgl/d3drm/impl.h"
#include "viewmanager/viewmanager.h"

#include "miniwin/miniwindevice.h"

#include <SDL3/SDL.h>
#include "errno.h"
#include "iniparser.h"
#include "stdlib.h"
#include "time.h"

static const char* TAG = "isleapp";

DECOMP_SIZE_ASSERT(IsleApp, 0x8c)

// GLOBAL: ISLE 0x410030
IsleApp* g_isle = NULL;

// GLOBAL: ISLE 0x410034
MxU8 g_mousedown = FALSE;

// GLOBAL: ISLE 0x410038
MxU8 g_mousemoved = FALSE;

// GLOBAL: ISLE 0x41003c
MxS32 g_closed = FALSE;

// GLOBAL: ISLE 0x410050
MxS32 g_rmDisabled = FALSE;

// GLOBAL: ISLE 0x410058
MxS32 g_targetWidth = BSP_LCD_H_RES;

// GLOBAL: ISLE 0x41005c
MxS32 g_targetHeight = BSP_LCD_V_RES;

// GLOBAL: ISLE 0x410064
MxS32 g_reqEnableRMDevice = FALSE;

// STRING: ISLE 0x4101dc
#define WINDOW_TITLE "LEGO®"

SDL_Window* window;

extern const char* g_files[46];

// FUNCTION: ISLE 0x401000
IsleApp::IsleApp()
{
	m_hdPath = NULL;
	m_cdPath = NULL;
	m_deviceId = NULL;
	m_savePath = NULL;
	m_fullScreen = FALSE;
	m_flipSurfaces = FALSE;
	m_backBuffersInVram = TRUE;
	m_using8bit = FALSE;
	m_using16bit = TRUE;
	m_hasLightSupport = FALSE;
	m_drawCursor = FALSE;
	m_use3dSound = TRUE;
	m_useMusic = TRUE;
	m_useJoystick = TRUE;
	m_joystickIndex = 0;
	m_wideViewAngle = TRUE;
	m_islandQuality = 2;
	m_islandTexture = 1;
	m_gameStarted = FALSE;
	m_frameDelta = 10;
	m_windowActive = TRUE;

	{
		MxRect32 r(0, 0, 639, 479);
		MxVideoParamFlags flags;
		m_videoParam = MxVideoParam(r, NULL, 1, flags);
	}
	m_videoParam.Flags().Set16Bit(MxDirectDraw::GetPrimaryBitDepth() == 16);

	m_windowHandle = NULL;
	m_cursorArrow = NULL;
	m_cursorBusy = NULL;
	m_cursorNo = NULL;
	m_cursorCurrent = NULL;

	LegoOmni::CreateInstance();

	m_iniPath = NULL;
	m_maxLod = RealtimeView::GetUserMaxLOD();
	m_maxAllowedExtras = m_islandQuality <= 1 ? 10 : 20;
}

// FUNCTION: ISLE 0x4011a0
IsleApp::~IsleApp()
{
	if (LegoOmni::GetInstance()) {
		Close();
		MxOmni::DestroyInstance();
	}

	if (m_hdPath) {
		delete[] m_hdPath;
	}

	if (m_cdPath) {
		delete[] m_cdPath;
	}

	if (m_deviceId) {
		delete[] m_deviceId;
	}

	if (m_savePath) {
		delete[] m_savePath;
	}

	if (m_mediaPath) {
		delete[] m_mediaPath;
	}
}

// FUNCTION: ISLE 0x401260
void IsleApp::Close()
{
	MxDSAction ds;
	ds.SetUnknown24(-2);

	if (Lego()) {
		GameState()->Save(0);
		if (InputManager()) {
			InputManager()->QueueEvent(c_notificationKeyPress, 0, 0, 0, SDLK_SPACE);
		}

		VideoManager()->Get3DManager()->GetLego3DView()->GetViewManager()->RemoveAll(NULL);

		Lego()->RemoveWorld(ds.GetAtomId(), ds.GetObjectId());
		Lego()->DeleteObject(ds);
		TransitionManager()->SetWaitIndicator(NULL);
		Lego()->Resume();

		while (Streamer()->Close(NULL) == SUCCESS) {
		}

		while (Lego() && !Lego()->DoesEntityExist(ds)) {
			Timer()->GetRealTime();
			TickleManager()->Tickle();
		}
	}
}

// FUNCTION: ISLE 0x4013b0
MxS32 IsleApp::SetupLegoOmni()
{
	MxS32 result = FALSE;
	MxS32 failure;
	{
		MxOmniCreateParam param(m_mediaPath, m_windowHandle, m_videoParam, MxOmniCreateFlags());
		failure = Lego()->Create(param) == FAILURE;
	}

	if (!failure) {
		VariableTable()->SetVariable("ACTOR_01", "");
		TickleManager()->SetClientTickleInterval(VideoManager(), 10);
		result = TRUE;
	}

	return result;
}

// FUNCTION: ISLE 0x401560
void IsleApp::SetupVideoFlags(
	MxS32 fullScreen,
	MxS32 flipSurfaces,
	MxS32 backBuffers,
	MxS32 using8bit,
	MxS32 using16bit,
	MxS32 hasLightSupport,
	MxS32 param_7,
	MxS32 wideViewAngle,
	char* deviceId
)
{
	m_videoParam.Flags().SetFullScreen(fullScreen);
	m_videoParam.Flags().SetFlipSurfaces(flipSurfaces);
	m_videoParam.Flags().SetBackBuffers(!backBuffers);
	m_videoParam.Flags().SetLacksLightSupport(!hasLightSupport);
	m_videoParam.Flags().SetF1bit7(param_7);
	m_videoParam.Flags().SetWideViewAngle(wideViewAngle);
	m_videoParam.Flags().SetF2bit1(1);
	m_videoParam.SetDeviceName(deviceId);
	if (using8bit) {
		m_videoParam.Flags().Set16Bit(0);
	}
	if (using16bit) {
		m_videoParam.Flags().Set16Bit(1);
	}
}

static int isle_update_renderer(void)
{
	if (!g_isle)
		return 0;

	if (!g_isle->Tick()) {
		return 1;
	}

	if (!g_closed) {
		if (g_reqEnableRMDevice) {
			g_reqEnableRMDevice = FALSE;
			VideoManager()->EnableRMDevice();
			g_rmDisabled = FALSE;
			Lego()->Resume();
		}

		if (g_closed) {
			return 0;
		}

		if (g_mousedown && g_mousemoved && g_isle) {
			if (!g_isle->Tick()) {
				return 1;
			}
		}

		if (g_mousemoved) {
			g_mousemoved = FALSE;
		}
	}

	return 0;
}

void* isle_init(void* args)
{
	SDL_Event event_unptr;
	SDL_Event *event = &event_unptr;
	int ret = 0;

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		char buffer[256];
		SDL_snprintf(
			buffer,
			sizeof(buffer),
			"\"LEGO® Island\" failed to start.\nSDL error: %s\n",
			SDL_GetError()
		);
		ESP_LOGE(TAG, "LEGO® Island Error\n%s", buffer);

		ret = 1;
		goto exit;
	}

	// [library:window]
	// Original game checks for an existing instance here.
	// We don't really need that.

	// Create global app instance
	g_isle = new IsleApp();

	// Create window
	if (g_isle->SetupWindow() != SUCCESS) {
		ESP_LOGE(TAG, "%s\n%s", "LEGO® Island Error",
			"\"LEGO® Island\" failed to start.\nPlease quit all other applications and try again."
		);
		ret = 1;
		goto exit;
	}

	while (!g_closed) {
		while (SDL_PollEvent(event) > 0) {
			switch (event->type) {
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			case SDL_EVENT_MOUSE_MOTION:
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
				IDirect3DRMMiniwinDevice* device = GetD3DRMMiniwinDevice();
				if (device && !device->ConvertEventToRenderCoordinates(event)) {
					SDL_Log("Failed to convert event coordinates: %s", SDL_GetError());
				}
				break;
			}

			switch (event->type) {
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				if (!g_closed) {
					delete g_isle;
					g_isle = NULL;
					g_closed = TRUE;
				}
				break;
			case SDL_EVENT_FINGER_MOTION: {
				g_mousemoved = TRUE;

				float x = SDL_clamp(event->tfinger.x, 0, 1) * 640;
				float y = SDL_clamp(event->tfinger.y, 0, 1) * 480;

				if (InputManager()) {
					InputManager()->QueueEvent(c_notificationMouseMove, LegoEventNotificationParam::c_lButtonState, x, y, 0);
				}

				if (g_isle->GetDrawCursor()) {
					VideoManager()->MoveCursor(Min((MxS32) x, 639), Min((MxS32) y, 479));
				}
				break;
			}
			case SDL_EVENT_FINGER_DOWN: {
				g_mousedown = TRUE;

				float x = SDL_clamp(event->tfinger.x, 0, 1) * 640;
				float y = SDL_clamp(event->tfinger.y, 0, 1) * 480;

				if (InputManager()) {
					InputManager()->QueueEvent(c_notificationButtonDown, LegoEventNotificationParam::c_lButtonState, x, y, 0);
				}
				break;
			}
			case SDL_EVENT_FINGER_UP: {
				g_mousedown = FALSE;

				float x = SDL_clamp(event->tfinger.x, 0, 1) * 640;
				float y = SDL_clamp(event->tfinger.y, 0, 1) * 480;

				if (InputManager()) {
					InputManager()->QueueEvent(c_notificationButtonUp, 0, x, y, 0);
				}
				break;
			}
			default:
				break;
			}

			if (event->user.type == g_legoSdlEvents.m_windowsMessage) {
				switch (event->user.code) {
				case WM_ISLE_SETCURSOR:
					break;
				case WM_TIMER:
					if (InputManager()) {
						InputManager()->QueueEvent(c_notificationTimer, (MxU8) (uintptr_t) event->user.data1, 0, 0, 0);
					}
					break;
				default:
					SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown SDL Windows message: 0x%" SDL_PRIx32, event->user.code);
					break;
				}
			}
			else if (event->user.type == g_legoSdlEvents.m_presenterProgress) {
				MxDSAction* action = static_cast<MxDSAction*>(event->user.data1);
				MxPresenter::TickleState state = static_cast<MxPresenter::TickleState>(event->user.code);

				if (!g_isle->GetGameStarted() && action && state == MxPresenter::e_ready &&
					!SDL_strncmp(action->GetObjectName(), "Lego_Smk", 8)) {
					g_isle->SetGameStarted(TRUE);
					SDL_Log("Game started");
				}
			}
		}

		ret = isle_update_renderer();
		if (ret) {
			ESP_LOGE(TAG, "%s\n%s",
				"LEGO® Island Error",
				"\"LEGO® Island\" failed to start.\nPlease quit all other applications and try again."
				"\nFailed to initialize; see logs for details"
			);
			ret = 1;
			goto exit;
		}

		vTaskDelay(1 / portTICK_PERIOD_MS);
	}

exit:
	if (window)
		SDL_DestroyWindow(window);

	ESP_LOGI(TAG, "Game quitted (ret=%d)", ret);
	return (void*)ret;
}

MxU8 IsleApp::MapMouseButtonFlagsToModifier(SDL_MouseButtonFlags p_flags)
{
	// [library:window]
	// Map button states to Windows button states (LegoEventNotificationParam)
	// Not mapping mod keys SHIFT and CTRL since they are not used by the game.

	MxU8 modifier = 0;
	if (p_flags & SDL_BUTTON_LMASK) {
		modifier |= LegoEventNotificationParam::c_lButtonState;
	}
	if (p_flags & SDL_BUTTON_RMASK) {
		modifier |= LegoEventNotificationParam::c_rButtonState;
	}

	return modifier;
}

// FUNCTION: ISLE 0x4023e0
MxResult IsleApp::SetupWindow()
{
	if (!LoadConfig()) {
		return FAILURE;
	}

	SetupVideoFlags(
		m_fullScreen,
		m_flipSurfaces,
		m_backBuffersInVram,
		m_using8bit,
		m_using16bit,
		m_hasLightSupport,
		FALSE,
		m_wideViewAngle,
		m_deviceId
	);

	MxOmni::SetSound3D(m_use3dSound);

	srand(time(NULL));

	// [library:window] Use original game cursors in the resources instead?
	m_cursorCurrent = m_cursorArrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
	m_cursorBusy = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
	m_cursorNo = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED);
	SDL_SetCursor(m_cursorCurrent);

	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, g_targetWidth);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, g_targetHeight);
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, m_fullScreen);
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, WINDOW_TITLE);
#ifdef MINIWIN
	//SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
#endif

	window = SDL_CreateWindowWithProperties(props);
#ifdef MINIWIN
	m_windowHandle = reinterpret_cast<HWND>(window);
#else
	m_windowHandle =
		(HWND) SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#endif

	SDL_DestroyProperties(props);

	if (!m_windowHandle)
		return FAILURE;

	if (!SetupLegoOmni())
		return FAILURE;

	GameState()->SetSavePath(m_savePath);

	if (VerifyFilesystem() != SUCCESS) {
		return FAILURE;
	}

	GameState()->SerializePlayersInfo(LegoStorage::c_read);
	GameState()->SerializeScoreHistory(LegoStorage::c_read);

	MxS32 iVar10;
	switch (m_islandQuality) {
	case 0:
		iVar10 = 1;
		break;
	case 1:
		iVar10 = 2;
		break;
	default:
		iVar10 = 100;
	}

	MxS32 uVar1 = (m_islandTexture == 0);
	LegoModelPresenter::configureLegoModelPresenter(uVar1);
	LegoPartPresenter::configureLegoPartPresenter(uVar1, iVar10);
	LegoWorldPresenter::configureLegoWorldPresenter(m_islandQuality);
	LegoBuildingManager::configureLegoBuildingManager(m_islandQuality);
	LegoROI::configureLegoROI(iVar10);
	LegoAnimationManager::configureLegoAnimationManager(m_maxAllowedExtras);
	RealtimeView::SetUserMaxLOD(m_maxLod);
	if (LegoOmni::GetInstance()) {
		if (LegoOmni::GetInstance()->GetInputManager()) {
			LegoOmni::GetInstance()->GetInputManager()->SetUseJoystick(m_useJoystick);
			LegoOmni::GetInstance()->GetInputManager()->SetJoystickIndex(m_joystickIndex);
		}
		MxDirect3D* d3d = LegoOmni::GetInstance()->GetVideoManager()->GetDirect3D();
		if (d3d) {
			SDL_Log(
				"Direct3D driver name=\"%s\" description=\"%s\"",
				d3d->GetDeviceName().c_str(),
				d3d->GetDeviceDescription().c_str()
			);
		}
		else {
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to get D3D device name and description");
		}
	}

	return SUCCESS;
}

// FUNCTION: ISLE 0x4028d0
bool IsleApp::LoadConfig()
{
	char* prefPath = CFG_PATH;
	char* iniConfig;
	if (m_iniPath) {
		iniConfig = new char[strlen(m_iniPath) + 1];
		strcpy(iniConfig, m_iniPath);
	}
	else if (prefPath) {
		iniConfig = new char[strlen(prefPath) + strlen("isle.ini") + 1]();
		strcat(iniConfig, prefPath);
		strcat(iniConfig, "isle.ini");
	}
	else {
		iniConfig = new char[strlen("isle.ini") + 1];
		strcpy(iniConfig, "isle.ini");
	}
	SDL_Log("Reading configuration from \"%s\"", iniConfig);

	dictionary* dict = iniparser_load(iniConfig);

	// [library:config]
	// Load sane defaults if dictionary failed to load
	if (!dict) {
		if (m_iniPath) {
			SDL_Log("Invalid config path '%s'", m_iniPath);
			return false;
		}

		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Loading sane defaults");
		FILE* iniFP = fopen(iniConfig, "wb");

		if (!iniFP) {
			SDL_LogError(
				SDL_LOG_CATEGORY_APPLICATION,
				"Failed to write config at '%s': %s",
				iniConfig,
				strerror(errno)
			);
			return false;
		}

		char buf[32];
		dict = iniparser_load(iniConfig);
		iniparser_set(dict, "isle", NULL);

		iniparser_set(dict, "isle:diskpath", DATA_PATH);
		iniparser_set(dict, "isle:cdpath", MxOmni::GetCD());
		iniparser_set(dict, "isle:mediapath", DATA_PATH);
		iniparser_set(dict, "isle:savepath", prefPath);

		iniparser_set(dict, "isle:Flip Surfaces", m_flipSurfaces ? "true" : "false");
		iniparser_set(dict, "isle:Full Screen", m_fullScreen ? "true" : "false");
		iniparser_set(dict, "isle:Wide View Angle", m_wideViewAngle ? "true" : "false");

		iniparser_set(dict, "isle:3DSound", m_use3dSound ? "true" : "false");
		iniparser_set(dict, "isle:Music", m_useMusic ? "true" : "false");

		iniparser_set(dict, "isle:UseJoystick", m_useJoystick ? "true" : "false");
		iniparser_set(dict, "isle:JoystickIndex", SDL_itoa(m_joystickIndex, buf, 10));
		iniparser_set(dict, "isle:Draw Cursor", m_drawCursor ? "true" : "false");

		iniparser_set(dict, "isle:Back Buffers in Video RAM", "-1");

		iniparser_set(dict, "isle:Island Quality", SDL_itoa(m_islandQuality, buf, 10));
		iniparser_set(dict, "isle:Island Texture", SDL_itoa(m_islandTexture, buf, 10));
		SDL_snprintf(buf, sizeof(buf), "%f", m_maxLod);
		iniparser_set(dict, "isle:Max LOD", buf);
		iniparser_set(dict, "isle:Max Allowed Extras", SDL_itoa(m_maxAllowedExtras, buf, 10));

		iniparser_dump_ini(dict, iniFP);
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "New config written at '%s'", iniConfig);
		fclose(iniFP);
	}

	const char* hdPath = iniparser_getstring(dict, "isle:diskpath", DATA_PATH);
	m_hdPath = new char[strlen(hdPath) + 1];
	strcpy(m_hdPath, hdPath);
	MxOmni::SetHD(m_hdPath);

	const char* cdPath = iniparser_getstring(dict, "isle:cdpath", MxOmni::GetCD());
	m_cdPath = new char[strlen(cdPath) + 1];
	strcpy(m_cdPath, cdPath);
	MxOmni::SetCD(m_cdPath);

	const char* mediaPath = iniparser_getstring(dict, "isle:mediapath", hdPath);
	m_mediaPath = new char[strlen(mediaPath) + 1];
	strcpy(m_mediaPath, mediaPath);

	m_flipSurfaces = iniparser_getboolean(dict, "isle:Flip Surfaces", m_flipSurfaces);
	m_fullScreen = iniparser_getboolean(dict, "isle:Full Screen", m_fullScreen);
	m_wideViewAngle = iniparser_getboolean(dict, "isle:Wide View Angle", m_wideViewAngle);
	m_use3dSound = iniparser_getboolean(dict, "isle:3DSound", m_use3dSound);
	m_useMusic = iniparser_getboolean(dict, "isle:Music", m_useMusic);
	m_useJoystick = iniparser_getboolean(dict, "isle:UseJoystick", m_useJoystick);
	m_joystickIndex = iniparser_getint(dict, "isle:JoystickIndex", m_joystickIndex);
	m_drawCursor = iniparser_getboolean(dict, "isle:Draw Cursor", m_drawCursor);

	MxS32 backBuffersInVRAM = iniparser_getboolean(dict, "isle:Back Buffers in Video RAM", -1);
	if (backBuffersInVRAM != -1) {
		m_backBuffersInVram = !backBuffersInVRAM;
	}

	MxS32 bitDepth = iniparser_getint(dict, "isle:Display Bit Depth", -1);
	if (bitDepth != -1) {
		if (bitDepth == 8) {
			m_using8bit = TRUE;
		}
		else if (bitDepth == 16) {
			m_using16bit = TRUE;
		}
	}

	m_islandQuality = iniparser_getint(dict, "isle:Island Quality", m_islandQuality);
	m_islandTexture = iniparser_getint(dict, "isle:Island Texture", m_islandTexture);
	m_maxLod = iniparser_getdouble(dict, "isle:Max LOD", m_maxLod);
	m_maxAllowedExtras = iniparser_getint(dict, "isle:Max Allowed Extras", m_maxAllowedExtras);

	const char* deviceId = iniparser_getstring(dict, "isle:3D Device ID", NULL);
	if (deviceId != NULL) {
		m_deviceId = new char[strlen(deviceId) + 1];
		strcpy(m_deviceId, deviceId);
	}

	// [library:config]
	// The original game does not save any data if no savepath is given.
	// Instead, we use SDLs prefPath as a default fallback and always save data.
	const char* savePath = iniparser_getstring(dict, "isle:savepath", prefPath);
	m_savePath = new char[strlen(savePath) + 1];
	strcpy(m_savePath, savePath);

	iniparser_freedict(dict);
	delete[] iniConfig;

	return true;
}

// FUNCTION: ISLE 0x402c20
inline bool IsleApp::Tick()
{
	// GLOBAL: ISLE 0x4101c0
	static MxLong g_lastFrameTime = 0;

	// GLOBAL: ISLE 0x4101bc
	static MxS32 g_startupDelay = 200;

	if (!m_windowActive) {
		SDL_Delay(1);
		return true;
	}

	if (!Lego()) {
		return true;
	}
	if (!TickleManager()) {
		return true;
	}
	if (!Timer()) {
		return true;
	}

	MxLong currentTime = Timer()->GetRealTime();
	if (currentTime < g_lastFrameTime) {
		g_lastFrameTime = -m_frameDelta;
	}

	if (m_frameDelta + g_lastFrameTime >= currentTime) {
		SDL_Delay(1);
		return true;
	}

	if (!Lego()->IsPaused()) {
		TickleManager()->Tickle();
	}
	g_lastFrameTime = currentTime;

	if (g_startupDelay == 0) {
		return true;
	}

	g_startupDelay--;
	if (g_startupDelay != 0) {
		return true;
	}

	LegoOmni::GetInstance()->CreateBackgroundAudio();
	BackgroundAudioManager()->Enable(m_useMusic);

	MxStreamController* stream = Streamer()->Open("\\lego\\scripts\\isle\\isle", MxStreamer::e_diskStream);
	MxDSAction ds;

	if (!stream) {
		stream = Streamer()->Open("\\lego\\scripts\\nocd", MxStreamer::e_diskStream);
		if (!stream) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open NOCD.si: Streamer failed to load");
			return false;
		}

		ds.SetAtomId(stream->GetAtom());
		ds.SetUnknown24(-1);
		ds.SetObjectId(0);
		VideoManager()->EnableFullScreenMovie(TRUE, TRUE);

		if (Start(&ds) != SUCCESS) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open NOCD.si: Failed to start initial action");
			return false;
		}
	}
	else {
		ds.SetAtomId(stream->GetAtom());
		ds.SetUnknown24(-1);
		ds.SetObjectId(0);
		if (Start(&ds) != SUCCESS) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open ISLE.si: Failed to start initial action");
			return false;
		}
	}

	return true;
}

MxResult IsleApp::VerifyFilesystem()
{
	for (const char* file : g_files) {
		const char* searchPaths[] = {".", m_hdPath, m_cdPath};
		bool found = false;

		for (const char* base : searchPaths) {
			MxString path(base);
			path += file;
			path.MapPathToFilesystem();

			if (SDL_GetPathInfo(path.GetData(), NULL)) {
				found = true;
				break;
			}
		}

		if (!found) {
			char buffer[1024];
			SDL_snprintf(
				buffer,
				sizeof(buffer),
				"\"LEGO® Island\" failed to start.\nPlease make sure the file %s is located in either diskpath or "
				"cdpath.\nSDL error: %s",
				file,
				SDL_GetError()
			);

			ESP_LOGE(TAG, "LEGO® Island Error\n%s", buffer);
			return FAILURE;
		}
	}

	return SUCCESS;
}

IDirect3DRMMiniwinDevice* GetD3DRMMiniwinDevice()
{
	LegoVideoManager* videoManager = LegoOmni::GetInstance()->GetVideoManager();
	if (!videoManager) {
		return nullptr;
	}
	Lego3DManager* lego3DManager = videoManager->Get3DManager();
	if (!lego3DManager) {
		return nullptr;
	}
	Lego3DView* lego3DView = lego3DManager->GetLego3DView();
	if (!lego3DView) {
		return nullptr;
	}
	TglImpl::DeviceImpl* tgl_device = (TglImpl::DeviceImpl*) lego3DView->GetDevice();
	if (!tgl_device) {
		return nullptr;
	}
	IDirect3DRMDevice2* d3drmdev = tgl_device->ImplementationData();
	if (!d3drmdev) {
		return nullptr;
	}
	IDirect3DRMMiniwinDevice* d3drmMiniwinDev = nullptr;
	if (!SUCCEEDED(d3drmdev->QueryInterface(IID_IDirect3DRMMiniwinDevice, (void**) &d3drmMiniwinDev))) {
		return nullptr;
	}
	return d3drmMiniwinDev;
}
