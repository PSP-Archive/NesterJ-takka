/*
	main.c
*/
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psppower.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include "main.h"
#include "menu.h"
#include "emu_main.h"
#include "screenmanager.h"
#include "nes/nes.h"
#include "nes/nes_config.h"
#include "fileio.h"
#include "menu_submenu.h"

/* Define the module info section */
PSP_MODULE_INFO("NesterJPSP", 0, 1, 10);

/* Define the main thread's attribute value (optional) */
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

int bSleep=0;

char RomPath[MAX_PATH];
char szLastGeniePath[MAX_PATH];
SETTING setting;

const EXTENTIONS stExtRom[] = {
 {"nes", EXT_NES},
 {"fds", EXT_NES},
 {"fam", EXT_NES},
 {"unf", EXT_NES},
 {"nsf", EXT_NES},
 {"zip", EXT_ZIP},
 {NULL,  EXT_UNKNOWN}
};

extern 	unsigned char g_PadBit[6];	// pad state for NES

// ----------------------------------------------------------------------------------------
int exit_callback(int arg1, int arg2, void *arg)
{
	bSleep=1;

	scePowerSetClockFrequency(222,222,111);
// Cleanup the games resources etc (if required)
	save_config();

	PSPEMU_SaveRAM();
	// �ꉞ�N���b�N�߂�
// Exit game
	sceKernelExitGame();

	return 0;
}

int power_callback(int unknown, int pwrflags, void *arg)
{
//	int cbid;

	// Combine pwrflags and the above defined masks
	if(pwrflags & PSP_POWER_CB_POWER_SWITCH){
		bSleep=1;
		save_config();
		PSPEMU_SaveRAM();
	}
	if (pwrflags & PSP_POWER_CB_BATTERY_LOW) {
//		scePowerSetClockFrequency(222,222,111);
		Scr_SetMessage( "PSP Battery is Low!", 300, 0xFFFF);
	}

	// �R�[���o�b�N�֐��̍ēo�^
//	cbid = sceKernelCreateCallback("Power Callback", power_callback, NULL);
//	scePowerRegisterCallback(0, cbid);
	return 0;
}

// Thread to create the callbacks and then begin polling
int CallbackThread(SceSize args, void *argp)
{
	int cbid;

	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);
	cbid = sceKernelCreateCallback("Power Callback", power_callback, NULL);
	scePowerRegisterCallback(0, cbid);

	// �|�[�����O
	sceKernelSleepThreadCB();
	return 0;
}

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void)
{
	int thid;

	thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0x10000, 0, 0);
	if(thid >= 0)
	{
		sceKernelStartThread(thid, 0, 0);
	}

	return thid;
}


// ----------------------------------------------------------------------------------------

int main(int argc, char *argp[])
{
	int tid;
	char dir[MAX_PATH];
	_memset(RomPath, 0x00, sizeof(RomPath));
	_memset(szLastGeniePath, 0x00, sizeof(szLastGeniePath));

	pgMain(argc, argp);

	tid = SetupCallbacks();
	pgInit();
	pgScreenFrame(2,0);
	sceCtrlSetSamplingCycle(0);
	wavoutInit();

	PSPEMU_NES_Init();
	NES_Config_SetDefaults_All();

	load_config();
	load_menu_bg();
	bgbright_change();
	wavout_enable=1;

	// mkdir save and state
	GetModulePath(dir, sizeof(dir));
	_strcat(dir,"SAVE");
	sceIoMkdir(dir,0777);
	GetModulePath(dir, sizeof(dir));
	_strcat(dir,"STATE");
	sceIoMkdir(dir,0777);
	DEBUG("MKDirs - done.");

	if (_strlen(setting.szLastPath) < 5) {
		GetModulePath(setting.szLastPath, sizeof(setting.szLastPath));
	}
	if (_strlen(setting.szLastGeniePath) < 5) {
		GetModulePath(setting.szLastGeniePath, sizeof(setting.szLastGeniePath));
	}
	FilerMsg[0]=0;

	_memset(g_PadBit, 0x00, sizeof(g_PadBit));
	// �p�b�h�ݒ�
	NES_set_pad(g_PadBit);

	DEBUG("pgGeInit()");
//	pgGeInit();
	DEBUG("pgGeInit() - done.");

	// CPU Frequency�ύX
	if (setting.cpufrequency < 222 || setting.cpufrequency > 333) setting.cpufrequency = 222;
	scePowerSetClockFrequency(setting.cpufrequency,setting.cpufrequency,setting.cpufrequency/2);
	DEBUG("Frequency Change - done.");

	PSPEMU_Freeze();
	for(;;){
		extern int nSelRomFiler; // menu.c
		while((nSelRomFiler = getFilePath(RomPath, setting.szLastPath, (LPEXTENTIONS)&stExtRom,
										  setting.szLastFile, nSelRomFiler)) < 0)
			;
		// init and rom load
		if (NES_Init(RomPath)) {
			break;
		}
	}

	pgFillvram(0);
	pgScreenFlipV();
	pgFillvram(0);
	pgScreenFlip();
	PSPEMU_Thaw();

	for(;;) {
		PSPEMU_DoFrame();

		if(setting.key_config[10] && (now_pad & setting.key_config[10])==setting.key_config[10]){
			PSPEMU_Freeze();
			if (!nesterj_menu()) {
				// exit nesterj
				break;
			}
			if(g_NESConfig.sound.enabled) wavout_enable=1;
			PSPEMU_Thaw();
		}

		if(bSleep){
			PSPEMU_Freeze();
			pgWaitVn(180);
			bSleep=0;
			PSPEMU_Thaw();
		}
	}
	// exit
	pgFillBox( 129, 104, 351, 168, 0 );
	mh_print(210, 132, "Good bye ...", 0xffff);
	pgScreenFlipV();
	save_config();
	PSPEMU_SaveRAM();
	// Exit game
	sceKernelExitGame();
	return 0;
}