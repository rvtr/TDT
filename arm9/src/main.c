#include "main.h"
#include "menu.h"
#include "message.h"
#include "nand/nandio.h"
#include <time.h>

#define VERSION "0.7.1"

bool programEnd = false;
bool nandWritten = false;

PrintConsole topScreen;
PrintConsole bottomScreen;

enum {
	MAIN_MENU_INSTALL,
	MAIN_MENU_TITLES,
	MAIN_MENU_BACKUP,
	MAIN_MENU_TEST,
	MAIN_MENU_EXIT
};

static void _setupScreens()
{
	REG_DISPCNT = MODE_FB0;
	VRAM_A_CR = VRAM_ENABLE;

	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);

	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);

	consoleInit(&topScreen,    3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true,  true);
	consoleInit(&bottomScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

	clearScreen(&bottomScreen);

	VRAM_A[100] = 0xFFFF;
}

static int _mainMenu(int cursor)
{
	//top screen
	clearScreen(&topScreen);

	iprintf("\t  Title Manager for NAND\n");
	iprintf("\t\t\tmodified from\n");
	iprintf("\tTitle Manager for HiyaCFW\n");
	iprintf("\nversion %s\n", VERSION);
	iprintf("\n\n\x1B[41mWARNING:\x1B[47m This tool writes to\nyour internal NAND!\n\nThis always has a risk, albeit\nlow, of \x1B[41mbricking\x1B[47m your system\nand should be done with caution!\n");
	iprintf("\nDo not exit by holding POWER,\ntap it or choose \"Shut Down\".\n");
	iprintf("\x1b[22;0HJeff - 2018-2019");
	iprintf("\x1b[23;0HPk11 - 2022");

	//menu
	Menu* m = newMenu();
	setMenuHeader(m, "MAIN MENU");

	addMenuItem(m, "Install", NULL, 0);
	addMenuItem(m, "Titles", NULL, 0);
	addMenuItem(m, "Restore", NULL, 0);
	addMenuItem(m, "Test", NULL, 0);
	addMenuItem(m, "Shut Down", NULL, 0);

	m->cursor = cursor;

	//bottom screen
	printMenu(m);

	while (!programEnd)
	{
		swiWaitForVBlank();
		scanKeys();

		if (moveCursor(m))
			printMenu(m);

		if (keysDown() & KEY_A)
			break;
	}

	int result = m->cursor;
	freeMenu(m);

	return result;
}

void fifoHandler(u32 value32, void* userdata) {
	if(value32 == 0x54495845) // 'EXIT'
		programEnd = true;
}

int main(int argc, char **argv)
{
	srand(time(0));
	_setupScreens();

	//DSi check
	if (!isDSiMode())
	{
		messageBox("\x1B[31mError:\x1B[33m This app is only for DSi.");
		return 0;
	}

	//setup sd card access
	if (!fatInitDefault())
	{
		messageBox("fatInitDefault()...\x1B[31mFailed\n\x1B[47m");
		return 0;
	}

	//setup nand access
	if (!fatMountSimple("nand", &io_dsi_nand))
	{
		messageBox("nand init \x1B[31mfailed\n\x1B[47m");
		return 0;
	}

	messageBox("\x1B[41mWARNING:\x1B[47m This tool writes to\nyour internal NAND!\n\nThis always has a risk, albeit\nlow, of \x1B[41mbricking\x1B[47m your system\nand should be done with caution!\n");
	messageBox("Do not exit by holding POWER,\ntap it or choose \"Shut Down\".\n");

	//main menu
	int cursor = 0;

	fifoSetValue32Handler(FIFO_USER_01, fifoHandler, NULL);

	while (!programEnd)
	{
		cursor = _mainMenu(cursor);

		switch (cursor)
		{
			case MAIN_MENU_INSTALL:
				installMenu();
				break;

			case MAIN_MENU_TITLES:
				titleMenu();
				break;

			case MAIN_MENU_BACKUP:
				backupMenu();
				break;

			case MAIN_MENU_TEST:
				testMenu();
				break;

			case MAIN_MENU_EXIT:
				programEnd = true;
				break;
		}
	}

	clearScreen(&bottomScreen);
	printf("Unmounting NAND...\n");
	fatUnmount("nand:");
	if(nandWritten) {
		printf("Merging stages...\n");
		nandio_shutdown();
	}

	fifoSendValue32(FIFO_USER_02, 0x54495845); // 'EXIT'

	return 0;
}

void clearScreen(PrintConsole* screen)
{
	consoleSelect(screen);
	consoleClear();
}