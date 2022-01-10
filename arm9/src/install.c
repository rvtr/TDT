#include "install.h"
#include "sav.h"
#include "main.h"
#include "message.h"
#include "maketmd.h"
#include "nand/nandio.h"
#include "rom.h"
#include "storage.h"
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

static bool _titleIsUsed(tDSiHeader* h)
{
	if (!h) return false;

	char path[64];
	sprintf(path, "%s:/title/%08x/%08x/", sdnandMode ? "sd" : "nand", (unsigned int)h->tid_high, (unsigned int)h->tid_low);

	return dirExists(path);
}

//patch homebrew roms if gameCode is #### or null
static bool _patchGameCode(tDSiHeader* h)
{
	if (!h) return false;

	if ((strcmp(h->ndshdr.gameCode, "####") == 0 && h->tid_low == 0x23232323) || (!*h->ndshdr.gameCode && h->tid_low == 0))
	{
		iprintf("Fixing Game Code...");
		swiWaitForVBlank();

		//set as standard app
		h->tid_high = 0x00030004;
		
		do {
			do {
				//generate a random game code
				for (int i = 0; i < 4; i++)
					h->ndshdr.gameCode[i] = 'A' + (rand() % 26);
			}
			while (h->ndshdr.gameCode[0] == 'A'); //first letter shouldn't be A
		
			//correct title id
			h->tid_low = ( (h->ndshdr.gameCode[0] << 24) | (h->ndshdr.gameCode[1] << 16) | (h->ndshdr.gameCode[2] << 8) | h->ndshdr.gameCode[3] );
		}
		while (_titleIsUsed(h));

		iprintf("\x1B[42m");	//green
		iprintf("Done\n");
		iprintf("\x1B[47m");	//white
		return true;
	}

	return false;
}

static bool _iqueHack(tDSiHeader* h)
{
	if (!h) return false;

	if (h->ndshdr.reserved1[8] == 0x80)
	{
		iprintf("iQue Hack...");	
		
		h->ndshdr.reserved1[8] = 0x00;

		iprintf("\x1B[42m");	//green
		iprintf("Done\n");
		iprintf("\x1B[47m");	//white
		return true;
	}

	return false;
}

static unsigned long long _getSaveDataSize(tDSiHeader* h)
{
	unsigned long long size = 0;

	if (h)
	{
		size += h->public_sav_size;
		size += h->private_sav_size;

		//banner.sav
		if (h->appflags & 0x4)
			size += 0x4000;
	}

	return size;
}

static bool _checkSdSpace(unsigned long long size)
{
	iprintf("Enough room on SD card?...");
	swiWaitForVBlank();

	if (getSDCardFree() < size)
	{
		iprintf("\x1B[31m");	//red
		iprintf("No\n");
		iprintf("\x1B[47m");	//white
		return false;
	}

	iprintf("\x1B[42m");	//green
	iprintf("Yes\n");
	iprintf("\x1B[47m");	//white
	return true;
}

static bool _checkDsiSpace(unsigned long long size)
{
	iprintf("Enough room on DSi?...");
	swiWaitForVBlank();

	if (getDsiFree() < size)
	{
		iprintf("\x1B[31m");	//red
		iprintf("No\n");
		iprintf("\x1B[47m");	//white
		return false;
	}

	iprintf("\x1B[42m");	//green
	iprintf("Yes\n");
	iprintf("\x1B[47m");	//white
	return true;
}

static bool _openMenuSlot()
{
	iprintf("Open DSi menu slot?...");
	swiWaitForVBlank();

	if (getMenuSlotsFree() <= 0)
	{
		iprintf("\x1B[31m");	//red
		iprintf("No\n");
		iprintf("\x1B[47m");	//white
		return choicePrint("Try installing anyway?");
	}

	iprintf("\x1B[42m");	//green
	iprintf("Yes\n");
	iprintf("\x1B[47m");	//white
	return true;
}

static void _createPublicSav(tDSiHeader* h, char* dataPath)
{
	if (!h) return;

	if (h->public_sav_size > 0)
	{
		iprintf("Creating public.sav...");
		swiWaitForVBlank();

		if (!dataPath)
		{
			iprintf("\x1B[31m");	//red
			iprintf("Failed\n");
			iprintf("\x1B[47m");	//white
		}
		else
		{
			char* publicPath = (char*)malloc(strlen(dataPath) + strlen("/public.sav") + 1);
			sprintf(publicPath, "%s/public.sav", dataPath);

			FILE* f = fopen(publicPath, "wb");

			if (!f)
			{
				iprintf("\x1B[31m");	//red
				iprintf("Failed\n");
				iprintf("\x1B[47m");	//white
			}
			else
			{
				fseek(f, h->public_sav_size-1, SEEK_SET);
				fputc(0, f);
				initFatHeader(f);

				iprintf("\x1B[42m");	//green
				iprintf("Done\n");
				iprintf("\x1B[47m");	//white
			}

			fclose(f);
			free(publicPath);
		}	
	}
}

static void _createPrivateSav(tDSiHeader* h, char* dataPath)
{
	if (!h) return;

	if (h->private_sav_size > 0)
	{
		iprintf("Creating private.sav...");
		swiWaitForVBlank();

		if (!dataPath)
		{
			iprintf("\x1B[31m");	//red
			iprintf("Failed\n");
			iprintf("\x1B[47m");	//white
		}
		else
		{
			char* privatePath = (char*)malloc(strlen(dataPath) + strlen("/private.sav") + 1);
			sprintf(privatePath, "%s/private.sav", dataPath);

			FILE* f = fopen(privatePath, "wb");

			if (!f)
			{
				iprintf("\x1B[31m");	//red
				iprintf("Failed\n");
				iprintf("\x1B[47m");	//white
			}
			else
			{
				fseek(f, h->private_sav_size-1, SEEK_SET);
				fputc(0, f);
				initFatHeader(f);

				iprintf("\x1B[42m");	//green
				iprintf("Done\n");
				iprintf("\x1B[47m");	//white
			}

			fclose(f);
			free(privatePath);
		}	
	}
}

static void _createBannerSav(tDSiHeader* h, char* dataPath)
{
	if (!h) return;

	if (h->appflags & 0x4)
	{
		iprintf("Creating banner.sav...");
		swiWaitForVBlank();

		if (!dataPath)
		{
			iprintf("\x1B[31m");	//red
			iprintf("Failed\n");
			iprintf("\x1B[47m");	//white
		}
		else
		{
			char* bannerPath = (char*)malloc(strlen(dataPath) + strlen("/banner.sav") + 1);
			sprintf(bannerPath, "%s/banner.sav", dataPath);

			FILE* f = fopen(bannerPath, "wb");

			if (!f)
			{
				iprintf("\x1B[31m");	//red
				iprintf("Failed\n");
				iprintf("\x1B[47m");	//white
			}
			else
			{
				fseek(f, 0x4000 - 1, SEEK_SET);
				fputc(0, f);

				iprintf("\x1B[42m");	//green
				iprintf("Done\n");
				iprintf("\x1B[47m");	//white
			}

			fclose(f);
			free(bannerPath);
		}	
	}
}

bool install(char* fpath, bool systemTitle)
{
	bool result = false;

	//confirmation message
	{
		char str[] = "Are you sure you want to install\n";
		char* msg = (char*)malloc(strlen(str) + strlen(fpath) + 8);
		sprintf(msg, "%s%s\n", str, fpath);
		
		bool choice = choiceBox(msg);
		free(msg);
		
		if (choice == NO)
			return false;
	}

	//start installation
	clearScreen(&bottomScreen);

	tDSiHeader* h = getRomHeader(fpath);	

	if (!h)
	{
		iprintf("\x1B[31m");	//red
		iprintf("Error: ");
		iprintf("\x1B[33m");	//yellow
		iprintf("Could not open file.\n");
		iprintf("\x1B[47m");	//white
		goto error;
	}
	else
	{
		bool fixHeader = false;

		if (_patchGameCode(h))
			fixHeader = true;

		//title id must be one of these
		if (h->tid_high == 0x00030004 ||
			h->tid_high == 0x00030005 ||
			h->tid_high == 0x00030015)
		{}
		else
		{
			iprintf("\x1B[31m");	//red
			iprintf("Error: ");
			iprintf("\x1B[33m");	//yellow
			iprintf("This is not a DSi rom.\n");
			iprintf("\x1B[47m");	//white
			goto error;
		}

		if (!sdnandMode &&
			(h->tid_high == 0x00030005 ||
			h->tid_high == 0x00030015))
		{
			iprintf("\x1B[31m");	//red
			iprintf("Error: ");
			iprintf("\x1B[33m");	//yellow
			iprintf("This title cannot be\ninstalled to SysNAND.\n");
			iprintf("\x1B[47m");	//white
			goto error;
		}

		if (!sdnandMode && !nandio_unlock_writing())
			return false;

		clearScreen(&bottomScreen);
		iprintf("Installing %s\n\n", fpath); swiWaitForVBlank();

		//get install size
		iprintf("Install Size: ");
		swiWaitForVBlank();
		
		unsigned long long fileSize = getRomSize(fpath);
		unsigned long long installSize = fileSize + _getSaveDataSize(h);

		printBytes(installSize);
		iprintf("\n");

		if (sdnandMode && !_checkSdSpace(installSize))
			goto error;

		//system title patch
		if (systemTitle)
		{
			iprintf("System Title Patch...");
			swiWaitForVBlank();
			h->tid_high = 0x00030015;
			iprintf("\x1B[42m");	//green
			iprintf("Done\n");
			iprintf("\x1B[47m");	//white

			fixHeader = true;
		}

		//skip nand check if system title
		if (h->tid_high != 0x00030015)
		{
			if (!_checkDsiSpace(installSize))
			{
				if (sdnandMode && choicePrint("Install as system title?"))
				{
					h->tid_high = 0x00030015;
					fixHeader = true;
				}				
				else
				{
					if (choicePrint("Try installing anyway?") == NO)
						goto error;
				}
			}
		}

		//check for saves or legit tmd
		int extensionPos = strrchr(fpath, '.') - fpath;

		char pubPath[PATH_MAX];
		strcpy(pubPath, fpath);
		strcpy(pubPath + extensionPos, ".pub");
		bool pubFound = getFileSizePath(pubPath) == h->public_sav_size;
		if (access(pubPath, F_OK) == 0 && !pubFound)
		{
			if (choicePrint("Incorrect public save.\nInstall anyway?") == YES)
				pubFound = false;
			else
				goto error;
		}

		char prvPath[PATH_MAX];
		strcpy(prvPath, fpath);
		strcpy(prvPath + extensionPos, ".prv");
		bool prvFound = getFileSizePath(prvPath) == h->private_sav_size;
		if (access(prvPath, F_OK) == 0 && !prvFound)
		{
			if (choicePrint("Incorrect private save.\nInstall anyway?") == YES)
				prvFound = false;
			else
				goto error;
		}

		char bnrPath[PATH_MAX];
		strcpy(bnrPath, fpath);
		strcpy(bnrPath + extensionPos, ".bnr");
		bool bnrFound = getFileSizePath(bnrPath) == 0x4000;
		if (access(bnrPath, F_OK) == 0 && !bnrFound)
		{
			if (choicePrint("Incorrect banner save.\nInstall anyway?") == YES)
				bnrFound = false;
			else
				goto error;
		}

		char tmdPath[PATH_MAX];
		strcpy(tmdPath, fpath);
		strcpy(tmdPath + extensionPos, ".tmd");
		bool tmdFound = getFileSizePath(tmdPath) == 520;

		if (_iqueHack(h))
			fixHeader = true;

		if (fixHeader && tmdFound)
		{
			if (choicePrint("Legit TMD cannot be used.\nInstall anyway?") == YES)
				tmdFound = false;
			else
				goto error;
		}

		//create title directory /title/XXXXXXXX/XXXXXXXX
		char dirPath[32];
		mkdir(sdnandMode ? "sd:/title" : "nand:/title", 0777);
		
		sprintf(dirPath, "%s:/title/%08x", sdnandMode ? "sd" : "nand", (unsigned int)h->tid_high);
		mkdir(dirPath, 0777);

		sprintf(dirPath, "%s:/title/%08x/%08x", sdnandMode ? "sd" : "nand", (unsigned int)h->tid_high, (unsigned int)h->tid_low);	

		//check if title is free
		if (_titleIsUsed(h))
		{
			char msg[64];
			sprintf(msg, "Title %08x is already used.\nInstall anyway?", (unsigned int)h->tid_low);

			if (choicePrint(msg) == NO)
				goto error;

			else
			{
				iprintf("\nDeleting:\n");
				deleteDir(dirPath);
				iprintf("\n");
			}
		}

		if (!_openMenuSlot())
			goto error;

		mkdir(dirPath, 0777);

		//content folder /title/XXXXXXXX/XXXXXXXXX/content
		{
			char contentPath[64];
			sprintf(contentPath, "%s/content", dirPath);

			mkdir(contentPath, 0777);

			u8 appVersion = 0;
			if (tmdFound)
			{
				FILE *file = fopen(tmdPath, "rb");
				if (file)
				{
					fseek(file, 0x1E7, SEEK_SET);
					fread(&appVersion, sizeof(appVersion), 1, file);
					fclose(file);
				}
			}

			//create 000000##.app
			{
				iprintf("Creating 000000%02x.app...", appVersion);
				swiWaitForVBlank();

				char appPath[80];
				sprintf(appPath, "%s/000000%02x.app", contentPath, appVersion);

				//copy nds file to app
				{
					int result = 0;

					if (!romIsCia(fpath))
						result = copyFile(fpath, appPath);
					else
						result = copyFilePart(fpath, 0x3900, fileSize, appPath);

					if (result != 0)
					{
						iprintf("\x1B[31m");	//red
						iprintf("Failed\n");
						iprintf("\x1B[33m");	//yellow
						iprintf("%s\n", appPath);
						iprintf("%s\n", strerror(errno));
						iprintf("\x1B[47m");	//white

						goto error;
					}

					iprintf("\x1B[42m");	//green
					iprintf("Done\n");
					iprintf("\x1B[47m");	//white
				}

				//pad out banner if it is the last part of the file
				{
					if (h->ndshdr.bannerOffset == fileSize - 0x1C00)
					{
						iprintf("Padding banner...");
						swiWaitForVBlank();

						if (padFile(appPath, 0x7C0) == false)
						{
							iprintf("\x1B[31m");	//red
							iprintf("Failed\n");
							iprintf("\x1B[47m");	//white
						}
						else
						{
							iprintf("\x1B[42m");	//green
							iprintf("Done\n");
							iprintf("\x1B[47m");	//white
						}
					}
				}

				//update header
				{
					if (fixHeader)
					{
						iprintf("Fixing header...");
						swiWaitForVBlank();

						//fix header checksum
						h->ndshdr.headerCRC16 = swiCRC16(0xFFFF, h, 0x15E);

						//fix RSA signature
						u8 buffer[20];
						swiSHA1Calc(&buffer, h, 0xE00);
						memcpy(&(h->rsa_signature[0x6C]), buffer, 20);

						FILE* f = fopen(appPath, "r+");

						if (!f)
						{
							iprintf("\x1B[31m");	//red
							iprintf("Failed\n");
							iprintf("\x1B[47m");	//white
						}
						else
						{
							fseek(f, 0, SEEK_SET);
							fwrite(h, sizeof(tDSiHeader), 1, f);

							iprintf("\x1B[42m");	//green
							iprintf("Done\n");
							iprintf("\x1B[47m");	//white
						}

						fclose(f);
					}
				}

				//make/copy TMD
				char newTmdPath[80];
				sprintf(newTmdPath, "%s/title.tmd", contentPath);
				if (tmdFound)
				{
					if (copyFile(tmdPath, newTmdPath) != 0)
						goto error;
				}
				else
				{
					if (maketmd(appPath, newTmdPath) != 0)
						goto error;
				}
			}
		}

		//data folder
		{
			char dataPath[64];
			sprintf(dataPath, "%s/data", dirPath);

			mkdir(dataPath, 0777);

			if (pubFound)
			{
				char newPubPath[80];
				sprintf(newPubPath, "%s/public.sav", dataPath);
				copyFile(pubPath, newPubPath);
			}
			else
			{
				_createPublicSav(h, dataPath);
			}

			if (prvFound)
			{
				char newPrvPath[80];
				sprintf(newPrvPath, "%s/private.sav", dataPath);
				copyFile(prvPath, newPrvPath);
			}
			else
			{
				_createPrivateSav(h, dataPath);
			}

			if (bnrFound)
			{
				char newBnrPath[80];
				sprintf(newBnrPath, "%s/banner.sav", dataPath);
				copyFile(bnrPath, newBnrPath);
			}
			else
			{
				_createBannerSav(h, dataPath);
			}
		}

		//end
		result = true;
		iprintf("\x1B[42m");	//green
		iprintf("\nInstallation complete.\n");
		iprintf("\x1B[47m");	//white
		iprintf("Back - [B]\n");
		keyWait(KEY_A | KEY_B);

		goto complete;
	}	

error:
	messagePrint("\x1B[31m\nInstallation failed.\n\x1B[47m");

complete:
	free(h);

	if (!sdnandMode)
		nandio_lock_writing();

	return result;
}