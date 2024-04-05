# TAD Delivery Tool
A TAD installer for the DSi that works with hiyaCFW's SDNAND and SysNAND, modified from Epicpkmn11's NAND Title Manager (which was modified from JeffRuLz's Title Manager for HiyaCFW).

TADs are renamed WADs for the DSi that are created by the TwlSDK. They normally are signed and encrypted for development units, and official tools like `TwlNmenu` and `Twl SystemUpdater` are unable to install them on retail consoles. TDT gets past this and allows you to install any TADs, regardless of signing. 

## WARNING
This can modify your internal system NAND! There is *always* a risk of **bricking**, albeit small, when you modify NAND. Please proceed with caution. Having Unlaunch installed is also strongly recommended as it will likely protect you if something manages to go wrong.

## Notes
- Backup your SD card and your NAND! There are checks in place to make sure you don't overwrite your home menu and brick, but you are still able to change and delete other system apps.
- You will need unlaunch when installing dev and updater/debugger TADs. 
   - Homebrew and DSiWare without a legit TMD require Unlaunch installed with its launcher patches enabled when installed to SysNAND

## Credits
- [DevkitPro](https://devkitpro.org/): devkitARM and libnds
- [Tuxality](https://github.com/Tuxality): [maketmd](https://github.com/Tuxality/maketmd)
- [Martin Korth (nocash)](https://problemkaputt.de): [GBATEK](https://problemkaputt.de/gbatek.htm)
- [Epicpkmn11](https://github.com/Epicpkmn11): [NTM](https://github.com/Epicpkmn11/NTM) (what this is a fork of)
- [JeffRuLz](https://github.com/JeffRuLz): [TMFH](https://github.com/JeffRuLz/TMFH) (what NTM is a fork of)
- [DesperateProgrammer](https://github.com/DesperateProgrammer): [DSi Language Patcher](https://github.com/DesperateProgrammer/DSiLanguagePacher) (working NAND writing code)
- [NinjaCheetah](https://github.com/NinjaCheetah): Helping fix some TAD decryption issues
- [DamiDoop](https://github.com/DamiDoop): Making the very nice icon
