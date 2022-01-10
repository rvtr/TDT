# NAND Title Manager
A basic title manager for the Nintendo DSi supporting both hiyaCFW's SDNAND and SysNAND, modified from JeffRuLz's Title Manager for HiyaCFW.

## WARNING
This can modify your internal system NAND! There is *always* a risk of **bricking**, albeit small, when you modify NAND. Please proceed with caution.

This has *barely* been tested and should be used with *extreme caution*. Please always use with Unlaunch installed, homebrew installed to the DSi Menu won't work without it and I'm not even sure if DSiWare installed by this will.

## Features
- Install DSiWare and homebrew onto your hiyaCFW SDNAND and SysNAND DSi Menus
- Delete system titles and others hidden from Data Management on SDNAND
- Backup and restore installed titles
- View basic title header info

## Notes
- Backup your SD card and your NAND! Nothing bad should happen, but this is an early release so who knows
- This cannot install cartridge games or older DS homebrew directly, for those you need to make [forwarders](https://wiki.ds-homebrew.com/ds-index/forwarders)
   - Always test your forwarders from TWiLight Menu++ or Unlaunch before installing to SysNAND
- This is only for DSi systems, not 3DS or DS

## Credits
- [DevkitPro](https://devkitpro.org/): devkitARM and libnds
- [Tuxality](https://github.com/Tuxality): [maketmd](https://github.com/Tuxality/maketmd)
- [Martin Korth (nocash)](https://problemkaputt.de): [GBATEK](https://problemkaputt.de/gbatek.htm)
- [JeffRuLz](https://github.com/JeffRuLz): [TMFH](https://github.com/JeffRuLz/TMFH) (what this is a fork of)
- [DesperateProgrammer](https://github.com/DesperateProgrammer): [DSi Language Patcher](https://github.com/DesperateProgrammer/DSiLanguagePacher) (working NAND writing code)
