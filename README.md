# LockSmith-RCM

## Warnings

* This payload can dod a lot of things that can dammage your console, I'm not responsable for loss of datas, brick  or even hardware destruction by using this payload.
* Please be sure to have the console's battery charged, especialy if you use the unbrick functions witch can take time and corrupt your storage if brutaly interrupted.
* Don't remove the SD during execution of the payload, if you want to change SD shutdown the console, replace the SD ans launch the payload.
* Copy Hekate's minerva module in "bootloader/sys/libsys_minerva.bso", already in place if you have installed Hekate. If you don't do it the payload will be slower than if you do it.

## Functionnalities
This payload, largely based on [Hekate](https://github.com/CTCaer/hekate), it BDK, [TegraExplorer](https://github.com/suchmememanyskill/TegraExplorer), Lockpick-RCM and Incognito-RCM and some of my personal projects can do a lot of things:
* Bootloader (file "LockSmith-RCM.bin") will only launch my signed "sd:/LockSmith/LockSmith.bin" or "sd:/LockSmith/LockSmith_debug.bin" files, if these second files are not found or are not signed for the bootloader the payload will poweroff the console, debug version has priority if the two files are present on the SD
* Work on most configs than before for each project based on Lockpick-RCM included (all witch work on or with the nand) cause Hekate BDK is updated
* Work on emunand (Atmosphere's one in "emummc/emummc.ini" or those configure in Hekate's configs with the var "emupath") or sysnand
* Launch almost functions when flags files are founded, in this case the menu will not display and the reboot will be done on "payload.bin", "bootloader/update.bin" or "atmosphere/reboot_to_payload.bin". At the end a log will be displayed to show up what has been done and the log file will be saved to "LockSmith-RCM/log.txt" on the SD. Flag files are removed if the function has been executed.
* Grey out options that can't be used in your config (Mariko consoles can't reboot to RCM, build PRODINFO from donor can't be donne if files are missing, etc...)
* Register a screenshot at the end of each function if launched without flag file (99999 screenshots for each function)
* Load the file "sd:/LockSmith-RCM/prod.keys" to set bis keys slots (decrypt nands), usful to work on a nand that is not from the console (this will grey out some options like generating PRODINFO, dump keys, show Efuses infos). If the file is present it will load this keys by default so be careful if you use flags files cause they will use these keys. If error when reading the file (some bis keys miss) or if no nandd can be read via these keys this will fallback to the console's keys. If payload launched without flag file you can switch to console keys or file keys at any time.
* Navigate with Joycons
* Dump keys
* Launch TegraExplorer scripts, files with ".te" extension must be in "sd:/LockSmith/scripts" folder, 100% compatible with TegraExplorer script engine V4 with even more option, see [this page for the scripts' specific documentation](LockSmith-RCM_TegraExplorer_Scripts.md). Also, if a script named "script.te" is placed at the SD root, like with TegraExplorer it will be executed before the payload launch it menu excepted if a batch flag or the AIO_LS_pack_Updater flag is present.
* Dump firmware, largely based on TegraExplorer firmware dump function but without needing PKG1 identification (based on FuseCheck NCA identification, if can't be identified it will be copied in "Firmware unknown" folder).
* Verify, fix, dump or restore PRODINFO (decrypted, placed in "LockSmith-RCM/backup/<emmc_id>"), one for the sysnand and one for the emunand, will not ask to override so be careful. Verify and fix functions are ported from [this python script](https://github.com/shadow2560/Ultimate-Switch-Hack-Script/blob/master/tools/python3_scripts/prodinfo_rewrite/prodinfo_rewrite.py) and verify function is often used during functions witch interact with PRODINFO read/write (Incognito apply, Prodinfogen functions, dump/restore PRODINFO, etc...).
* Aply incognito (don't make the backup of PRODINFO so do it first), don't need PKG1 identification like before so don't need to be updated for each new firmware.
* Build and flash a PRODINFO, from donor or from scratch, based on [ProdinfoGen](https://github.com/CaramelDunes/prodinfo_gen) but largely modified. If you choose to flash it you must backup your PRODINFO first if you want to restore it later, the payload will not do it.
* Set auto-RCM for unpatched consoles (largely based on Hekate's same function)
* Fix archive bits on the SD (largely based on Hekate's same function)
* Mount SD, EMMC BOOT0/BOOT1/RAWNAND via USB (largely based on Hekate's same functions)
* Obtain infos of the console, largely based on Hekate same functions
* Fix downgrade from firmware 21.0.0+ to a lower firmware (20.0.0 max), based on [DowngradeFixer](https://github.com/sthetix/DowngradeFixer) but largely modified
* Remove parental control (disabled for now cause not worked on new firmwares)
* Wip nand
* Flash an EmmcHaccGen package placed in "sd:/cdj_package_files", with or without wip.
* Create an EmmcHaccGen package (largely inspired from LibHac and [EmmcHaccGen](https://github.com/suchmememanyskill/EmmcHaccGen)) and even use it to unbrick the console (with or without wip) or restore EXFAT driver, for this last the firmware used to create the package should be the same as the firmware on the nand. The source firmware can be placed in "sd:/LockSmith-RCM/firmware" or "sd:/dernier_firmware_compatible", if both folders are present you can choose witch one to use.
* Remove ERPT save (dangerous)
* Synchronize joycons between nands (dangerous) (disabled for now cause not worked on new firmwares)
* Display efuses check and diagnostic, largely based on [FuseCheck](https://github.com/sthetix/FuseCheck) but with direct firmware identification, not based on a NCAs database anymore (slower but more fiable)
* Reboot to a payload
* Reboot to OFW with bypass fuses or not
* Reboot to RCM
* Poweroff the console
* Reboot the payload itself
* Change the payload's display orientation
* AIO_LS_pack_Updater can use a special file flag to update some files during pack update, in this case the menu will not display and the reboot will be done on "payload.bin", "bootloader/update.bin" or "atmosphere/reboot_to_payload.bin" or will reboot the console on Mariko models, this is based on amssu-rcm. If using this flag no log will be recorded and reboot will be done automaticaly without user interaction.

## Flags files
These files can be placed in "sd:/LockSmith-RCM" folder to launch a specific functions. You can place any flag you want, all functions will be executed then the reboot will be done on "payload.bin", "bootloader/update.bin" or "atmosphere/reboot_to_payload.bin" or will reboot the console on Mariko models. Files should only be named and placed correctly and can be empty.

List of files and associated functions, executed in this order:
| filename | function launched |
|----------|-------------------|
| fuse_check_sysnand | Obtain Efuses infos of the console |
| dump_prodinfo_sysnand | Dump PRODINFO from sysnand |
| dump_prodinfo_emunand | Dump PRODINFO from emunand |
| restore_prodinfo_sysnand | Restore PRODINFO to sysnand |
| restore_prodinfo_emunand | Restore PRODINFO to emunand |
| dump_fw_sysnand | Dump firmware from sysnand |
| dump_fw_emunand | Dump firmware from emunand |
| incognito_sysnand | Aply incognito to sysnand |
| incognito_emunand | Aply incognito to emunand |
| fix_dg_sysnand | Fix downgrade from firmware 21.0.0+ to a lower firmware on sysnand |
| fix_dg_emunand | Fix downgrade from firmware 21.0.0+ to a lower firmware on emunand |
| wip_sysnand | Wip sysnand |
| wip_emunand | Wip emunand |
| rm_parental_control_sysnand | Remove parental control on sysnand (disabled for now cause not worked on new firmwares) |
| rm_parental_control_emunand | Remove parental control on emunand (disabled for now cause not worked on new firmwares) |
| unbrick_sysnand | Flash a generated EmmcHaccGen package placed in "cdj_package_files" on the SD to the sysnand |
| unbrick_emunand | Flash a generated EmmcHaccGen package placed in "cdj_package_files" on the SD to the emunand (could take some time to flash) |
| unbrick_and_wip_sysnand | Flash a generated EmmcHaccGen package placed in "cdj_package_files" on the SD to the sysnand and wip the sysnand (don't mix with "unbrick_sysnand", it will double the work time for nothing) |
| unbrick_and_wip_emunand | Flash a generated EmmcHaccGen package placed in "cdj_package_files" on the SD to the emunand and wip the emunand (don't mix with "unbrick_emunand", it will double the work time for nothing) (could take some time to flash) |
| rm_erpt_sysnand | Remove ERPT save on sysnand, do it only if someone told you to do so |
| rm_erpt_emunand | Remove ERPT save on emunand, do it only if someone told you to do so |
| sync_joycons_sysnand | Synchronize joycons configs from sysnand to emunand, dangerous, use it if you know what you're doing (disabled for now cause not worked on new firmwares) |
| sync_joycons_emunand | Synchronize joycons configs from emunand to sysnand, normaly not needed, dangerous, use it if you know what you're doing (disabled for now cause not worked on new firmwares) |
| prodinfogen_flash_scratch_sysnand | Build and flash a PRODINFO from scratch on sysnand, dont't do it on an other nand than the console's one |
| prodinfogen_flash_scratch_emunand | Build and flash a PRODINFO from scratch on emunand, dont't do it on an other nand than the console's one |
| prodinfogen_flash_donor_sysnand | Build and flash a PRODINFO from donor on sysnand (need a decrypted donor PRODINFO and eventualy a donor keys file placed in the "switch" folder of the SD, like in ProdinfoGen payload), dont't do it on an other nand than the console's one |
| prodinfogen_flash_donor_emunand | Build and flash a PRODINFO from donor on emunand (need a decrypted donor PRODINFO and eventualy a donor keys file placed in the "switch" folder of the SD, like in ProdinfoGen payload), dont't do it on an other nand than the console's one |
| test_and_fix_prodinfo_nand_sysnand | test sysnand nand PRODINFO  CRCs and hashes and fix if errors |
| test_and_fix_prodinfo_nand_emunand | test emunand nand PRODINFO  CRCs and hashes and fix if errors |
| test_and_fix_prodinfo_backup_sysnand | test sysnand backup PRODINFO  CRCs and hashes and fix if errors |
| test_and_fix_prodinfo_backup_emunand | test emunand backup PRODINFO  CRCs and hashes and fix if errors |
| dump_keys_sysnand | Dump keys from sysnand, if nand is not the console's one it will only dump console's keys |
| dump_keys_emunand | Dump keys from emunand, don't mix up with "dump_keys_sysnand" or else the sysnand dump will be erased, if nand is not the console's one it will only dump console's keys |
| dump_amiibo_keys_sysnand | Dump amiibo keys of the console |
| dump_mariko_partial_keys_sysnand | Dump mariko partial keys of the console (will force shutdown of the console after log recording instead of trying to reboot to Hekate or Atmosphere payload), only work on Mariko consoles |

## To do:
* Optimize or factorize some functions or elements, like sd_mount() calls, in general work on payload's size.
* Remove all gfx_printf() without params and use gfx_puts() witch is more appropriate in this case.
* Dump games' saves, for encrypted saves it could be done easily but for decrypted and well organized saves I don't know what is possible but we have a save lib in the project so it will be investigated (started to code it, it seems that it is possible to save decrypted saves but will need more work to implement and will probably take too much size at least for now).
* Dump/restore PRODINFO and eventualy PRODINFOF, BOOT0 and BOOT1 partitions (to/from "LockSmith-RCM/backups/[emmc_id]" folder) (PRODINFO done, I don't know if I will do it for the others).
* Choose the emunand to work on by parsing also the Hekate's configs and config the emunand vars accordingly. Done but not tested.
* Test Hekate's emunand configs, if we have the same "emupath" for X emunands we must keep only one
* Add the possibility to work on a forced emunand config if a file is defined with some values (will be usful for AIO_LS_pack_Updater work). Seems to be difficult to obtain the info of start sector for partition emunand when HOS is launched so this may not be done.
* A problem could occure when a lot of big malloc are done, probably caused by memory fragmentation, should be investigated. For now big malloc are done on global variables, probably not the best solution but at least it limits this problem for now.
* If "LOG_MAX_ENTRIES" is defined to a value of 300 we have corrupted memory, using a value of 250 or 1024 seems to work but it's a strange behaviour, shoud be investigated. May be a problem of a too large BSS, may be a problem with the flag "-os" used during compilation, normaly Hekate use "-o2" flag but if "-o2" is used on this payload it has a too big size with compressed version. Seems to be a too big BSS during the tests, now based on malloc.
* Fix a potential memory corruption in the main function, for now if we move the content of the function "init_payload()" in the main function the payload reboot functions are broken, maybe this could be fixed by updating the BDK or change some init value on the main function (update like Hekate do) or in the Makefile (also update it like Hekate's one). This has not been re-tested since BDK update, should be investigated, probably a too big BSS during the tests. After investigation it was a problem in the main function, we shouldn't declare variables here cause of the "pivot_stack(IPL_LOAD_ADDR);" and "heap_init((void *)IPL_HEAP_START);", these functions change some memory access and some variables can be placed where the payload is stocked in ram so it cause some corruptions in it; Now this shouldn't be a problem anymore cause the only way where these functions are called are in the bootloader of the payload.

## Update for new keys

Update "/LockSmith/hos/hos.h", "/LockSmith/keys/crypto.h" and "/LockSmith/keys/key_sources.inl"  with the  new keys fromm [this file from Atmosphere](https://github.com/Atmosphere-NX/Atmosphere/blob/master/exosphere/program/source/boot/secmon_boot_key_data.s).

For "mariko_master_kek_sources_dev", "mariko_master_kek_sources" and "master_kek_sources" in "/LockSmith/keys/key_sources.inl" you must use the keys on [this file from Atmosphere](https://github.com/Atmosphere-NX/Atmosphere/blob/master/fusee/program/source/fusee_key_derivation.cpp) (updated when keys change so if you want an old key you must download the sources of Atmosphere released for this firmware):

If the payload freeze during keys dump try to modify the file "/LockSmith/keys/keys.c" (search the line starting with "u32 text_buffer_size = MAX(_titlekey_count * sizeof(titlekey_text_buffer_t) + 1, " in function "static void _save_keys_to_sd(key_storage_t *keys, titlekey_buffer_t *titlekey_buffer, bool is_dev) {") and increase the size for the second param of max() function (now it's SZ_32K, before was SZ_16K).

For "/LockSmith/hos/pkg1.c" infos look at [this file from hekate](https://github.com/cTCaer/hekate/blob/master/bootloader/hos/hos.c).

## Changelog:

### 0.90.0 (no sources published)

* The payload now only launch "sd:/LockSmith/LockSmith.bin" or "sd:/LockSmith/LockSmith_debug.bin" (this last one have priority if the two are present), it's only a bootloader for the real payload witch work like NYX in Hekate, this make the payload size not a problem anymore. The file launched by this bootloader is signed so it can only launch the signed files that I will release for this payload. Adding this introduce lots of changes in the payload's code.
* Bis cache fixed, no more files based EMUMMC slow treatments.
* Add suport for splited dump for USER partition dump/restore.
* Most main menu elements are now dispatched in sub-menus, the payload is more organized now
* Add support for TegraExplorer's scripts, they must be in "sd:/LockSmith/scripts" folder and they are compatible with TegraExplorer scripts engine V4. Adding this introduce lots of changes in the payload's code.
* Add support for portrait and landscape display, to have the possibility to display TegraExplorer's scripts without modifications (they are in landscape by default). Adding this introduce lots of changes in the payload's code.
* Menus can now display a menu that have more entries than the screen can display, now it can scroll the menu elements with the selection. Adding this introduce lots of changes in the payload's code.
* Reintroduce Hekate's emummc config to select an emunand, removed in 0.14.0. The menu used to select the emunand also use the same menu type than others menus of the payload.
* Navigation via the joycons added. Adding this introduce lots of changes in the payload's code.
* Add EmmcHaccGen package creation, witch can be chained with an unbrick with or without wip and can also install/restore EXFAT driver if firmware source and on the nand are the same for this last one.
* Add the possibility to set auto-RCM for compatibles consoles
* Add the possibility to fix archive bits on the SD
* Add the possibility to mount SD or EMMC BOOT0/BOOT1/RAWNAND via USB
* Add the possibility to display some console infos, like those witch Hekate can display
* Firmware detection also try to detect EXFAT drivers, displayed in fuse_check and also used when dumping firmware to name the firmware folder (detection not based on NCAs database anymore, slower but increase fiability)
* Add the possibility to change the payload's display orientation (landscape is the default orientation)
* Add an easter egg.
* Lots of bugs fixed, errors verifications and code optimization.

### 0.14.0

* Add suport for firmware 23.0.0 and 23.0.1.
* Add new dump methods, largely inspired from [this pull request](https://github.com/THZoria/Lockpick_RCMaster/pull/5).
* Bis cache can be enabled or not during a partition mount that suport it, not an option directly accessible for the user but in the code.
* Remove Hekate's emummc config to select an emunand (only the classic one is supported for now).
* Some bugs fixed and some code optimization done.

### 0.13.0

* Remove joycons synchronization between nands and parental control removal cause these functions doesn't work on new firmwares.
* Bypass rebuilt nand Titlekeys dump, largely inspired by [this pull request](https://github.com/THZoria/Lockpick_RCMaster/pull/3).
* Remove specific dump function for Prodinfogen functions.

### 0.12.2

* add firmware 22.1.0 and 22.5.0 identifications.
* Update Hekate's BDK.

### 0.12.1

* Fix wip function and fix vol+ press wait when launching wip function.

### 0.12.0

* Update Hekate's BDK to it latest version and modify the project files to adapt them for these changes.

### 0.11.1

* An other fix for the menu display.

### 0.11.0

* Update to support firmware 22.0.0 and to fix an important bug in the menu display.

### 0.10.0

* Update Hekate's BDK
* Add verify, fix, backup and restore PRODINFO functions. Verify and fix functions are ported from [my Ultimate-Switch-Hack-Script python script](https://github.com/shadow2560/Ultimate-Switch-Hack-Script/blob/master/tools/python3_scripts/prodinfo_rewrite/prodinfo_rewrite.py)
* Add incognito function.
* Add firmware dump function.
* All functions are now accessibles via flag files.
* Fix unbrick without wip function witch now realy not wip.
* Fix FuseCheck messages, particularely if Efuses are underburnt, in this case the console will boot normaly but will burn the missing Efuses; but other messages are also rewrited. This function has also been largely optimized in size by packing the NCAs table during compilation and by modifying how the NCA names are checked to identify a firmware.
* The majority of messages displayed are now centralized and the functions to display/log them have been rewrited, now it's based on a message pool generated during compilation, small gain in size but it's always a gain.
* Rewrite ProdinfoGen functions (now based on PRODINFO verify/fix implemented functions), big gain in size.
* Rewrite cal0_read functions (now based on PRODINFO verify/fix implemented functions), no gain in size but at least now use the same functions and structures everywhere.
* Greatly optimize  the payload size by lots of code rewrites, to many changes to be listed.
* Lots of bugs fixed.
* Preparations for futur possible futur improvements.
* Update this readme file.

### 0.9.1

* Add firmware 21.2.0 identification for FuseCheck function.
* Some bugs fixed and preparations for futur possibles improvements.

### 0.9.0

* Initial release.

## Build

Follow the guide located [here](https://devkitpro.org/wiki/Getting_Started) to install and configure all the tools necessary for the build process. You need to install these package via "pacman" or "dkp-pacman" to build:
` pacman -Syuu
` pacman -S switch-dev devkitARM devkitarm-rules

Then clone andd build the project:
```
git clone https://github.com/shadow2560/LockSmith-RCM.git
cd LockSmith-RCM
make

## Build architecture

LockSmith-RCM is built as three stages, following the same general chainloading architecture as Hekate:

1. `loader/` is the small RCM payload. It only relocates and decompresses the next stage.
2. `bootloader/` is the intermediate payload. It initializes the BDK, mounts the SD card and loads the full LockSmith binary.
3. `LockSmith/` contains the complete LockSmith application, including the former `source/` contents and `keygen/` directory.

The global `Makefile` builds these stages in order. The resulting files are:

- `output/LockSmith.bin`: full LockSmith payload to place on the SD card.
- `output/bootloader/bootloader.bin`: intermediate SD-loading stage before compression.
- `output/LockSmith-RCM.bin`: payload to inject through RCM.

The intermediate stage expects the full payload at:

`sd:/LockSmith/LockSmith.bin`


### Scripts TegraExplorer intégrés au payload

Lors de la compilation de LockSmith-RCM, les fichiers `*.te` présents dans `TegraExplorer/scripts/` sont intégrés automatiquement au payload. Ils apparaissent en tête du menu **Launch TegraExplorer scripts**, avant les scripts lus sur la carte SD dans `sd:/LockSmith-RCM/scripts/`. Le générateur et la minification sont réalisés par un petit outil C hôte ; Python n'est pas requis pour cette étape.

Pour choisir un autre dossier source, passer `EMBEDDED_SCRIPTS_DIR` à la compilation du sous-projet LockSmith, par exemple `make -C LockSmith EMBEDDED_SCRIPTS_DIR=../mon_dossier/scripts`. Un dossier absent ou vide produit simplement un payload sans scripts intégrés.
