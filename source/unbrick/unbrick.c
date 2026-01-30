#include <string.h>
#include "unbrick.h"
#include "../keys/keys.h"
#include <mem/minerva.h>
#include "../prodinfogen/build_prodinfo.h"
#include "../storage/emummc.h"
#include <storage/sd.h>
#include <utils/btn.h>
#include <utils/sprintf.h>

#include "../tools.h"
#include "../gfx/messages.h"

static int is_file_in_keep_list(const char *name)
{
	const char *keep_files[] = {
		"8000000000000000", // save index
		"8000000000000120", // save modules list index
		"80000000000000d1", // erpt save
		"8000000000000047",
		"8000000000000053" // calibration datas
	};
	for (size_t i = 0; i < sizeof(keep_files) / sizeof(keep_files[0]); i++) {
		if (strcmp(name, keep_files[i]) == 0)
			return 1;
	}
	return 0;
}

void unbrick(const char *sd_folder_path, bool reset) {
	// minerva_change_freq(FREQ_1600);
	char screenshot_name[20];
	if (reset) {
		s_printf(screenshot_name, "unbrick_and_wip");
	} else {
		s_printf(screenshot_name, "unbrick");
	}
	sd_mount();
	if (f_stat("cdj_package_files", NULL) || f_stat("cdj_package_files/BCPKG2-1-Normal-Main.bin", NULL) || f_stat("cdj_package_files/BCPKG2-2-Normal-Sub.bin", NULL) || f_stat("cdj_package_files/BCPKG2-3-SafeMode-Main.bin", NULL) || f_stat("cdj_package_files/BCPKG2-4-SafeMode-Sub.bin", NULL) || f_stat("cdj_package_files/BOOT0.bin", NULL) || f_stat("cdj_package_files/BOOT1.bin", NULL) || f_stat("cdj_package_files/SYSTEM/Contents/placehld", NULL) || f_stat("cdj_package_files/SYSTEM/Contents/registered", NULL) || f_stat("cdj_package_files/SYSTEM/save", NULL)) {
		log_printf(true, LOG_ERR, LOG_MSG_UNBRICK_FOLDER_ERROR);
		return;
	}
	if (!bis_loaded) {
		return;
	}
	LIST_INIT(gpt);
	if (!mount_nand_part(&gpt, "SYSTEM", true, true, true, true, NULL, NULL, NULL, NULL)) {
		save_screenshot_and_go_back(screenshot_name);
		return;
	}
	unmount_nand_part(&gpt, false, true, true, true);
	if (reset) {
		if (!mount_nand_part(&gpt, "USER", true, true, true, true, NULL, NULL, NULL, NULL)) {
			save_screenshot_and_go_back(screenshot_name);
			return;
		} else {
			unmount_nand_part(&gpt, false, true, true, true);
		}
	}
	char temp_path[MAX_PATH_LEN];
	s_printf(temp_path, "%s/BOOT0.bin", sd_folder_path);
	if (!flash_or_dump_part(true, temp_path, "BOOT0", false)) {
		save_screenshot_and_go_back(screenshot_name);
		return;
	}
			s_printf(temp_path, "%s/BOOT1.bin", sd_folder_path);
			if (!flash_or_dump_part(true, temp_path, "BOOT1", false)) {
				save_screenshot_and_go_back(screenshot_name);
				return;
			}
	s_printf(temp_path, "%s/BCPKG2-1-Normal-Main.bin", sd_folder_path);
	if (!flash_or_dump_part(true, temp_path, "BCPKG2-1-Normal-Main", false)) {
		save_screenshot_and_go_back(screenshot_name);
		return;
	}
	s_printf(temp_path, "%s/BCPKG2-2-Normal-Sub.bin", sd_folder_path);
	if (!flash_or_dump_part(true, temp_path, "BCPKG2-2-Normal-Sub", false)) {
		save_screenshot_and_go_back(screenshot_name);
		return;
	}
	s_printf(temp_path, "%s/BCPKG2-3-SafeMode-Main.bin", sd_folder_path);
	if (!flash_or_dump_part(true, temp_path, "BCPKG2-3-SafeMode-Main", false)) {
		save_screenshot_and_go_back(screenshot_name);
		return;
	}
	s_printf(temp_path, "%s/BCPKG2-4-SafeMode-Sub.bin", sd_folder_path);
	if (!flash_or_dump_part(true, temp_path, "BCPKG2-4-SafeMode-Sub", false)) {
		save_screenshot_and_go_back(screenshot_name);
		return;
	}

	if (!mount_nand_part(&gpt, "SYSTEM", true, true, true, true, NULL, NULL, NULL, NULL)) {
		save_screenshot_and_go_back(screenshot_name);
		return;
	}

f_cp_or_rm_rf("bis:/Contents/registered", NULL);
	if (reset) {
		f_cp_or_rm_rf("bis:/Contents", NULL);
		f_cp_or_rm_rf("bis:/save", NULL);
		f_cp_or_rm_rf("bis:/saveMeta", NULL);
		f_unlink("bis:/PRF2SAFE.RCV");
	}
	s_printf(temp_path, "%s/SYSTEM/Contents", sd_folder_path);
	f_cp_or_rm_rf(temp_path, "bis:/Contents");
	bool index_save_moved = false;
	if (f_stat("bis:/save/8000000000000000", NULL) == FR_OK) {
		easy_rename("bis:/save/8000000000000000", "bis:/8000000000000000");
		index_save_moved = true;
	}
	s_printf(temp_path, "%s/SYSTEM/save", sd_folder_path);
	f_cp_or_rm_rf(temp_path, "bis:/save");
	if (index_save_moved && f_stat("bis:/8000000000000000", NULL) == FR_OK) {
		easy_rename("bis:/8000000000000000", "bis:/save/8000000000000000");
	}
	if (reset) {
		unmount_nand_part(&gpt, false, true, false, true);
		if (!mount_nand_part(&gpt, "USER", false, false, true, true, NULL, NULL, NULL, NULL)) {
			save_screenshot_and_go_back(screenshot_name);
			return;
		}
		f_cp_or_rm_rf("bis:/Album", NULL);
		f_cp_or_rm_rf("bis:/Contents", NULL);
		f_cp_or_rm_rf("bis:/save", NULL);
		f_cp_or_rm_rf("bis:/saveMeta", NULL);
		f_cp_or_rm_rf("bis:/temp", NULL);
		f_unlink("bis:/PRF2SAFE.RCV");
		f_mkdir("bis:/Album");
		f_mkdir("bis:/Contents");
		f_mkdir("bis:/Contents/placehld");
		f_mkdir("bis:/Contents/registered");
		f_mkdir("bis:/save");
		f_mkdir("bis:/saveMeta");
		f_mkdir("bis:/temp");
	}
unmount_nand_part(&gpt, false, true, true, true);

	// minerva_change_freq(FREQ_800);
	if (!reset) {
	log_printf(true, LOG_OK, LOG_MSG_UNBRICK_SUCCESS);
	} else {
		log_printf(true, LOG_OK, LOG_MSG_UNBRICK_AND_WIP_SUCCESS);
	}
	save_screenshot_and_go_back(screenshot_name);
	return;
}

void wip_nand() {
	cls();
	log_printf(true, LOG_INFO, LOG_MSG_FNC_BEGIN, "wip");
	if (!bis_loaded && !wait_vol_plus()) {
		return;
	}

	LIST_INIT(gpt);
	if (!mount_nand_part(&gpt, "SYSTEM", true, true, true, true, NULL, NULL, NULL, NULL)) {
		save_screenshot_and_go_back("wip_nand");
		return;
	}

	DIR dir;
	FILINFO fno;
	FRESULT res;
	char fullpath[256];

	const char *path = "bis:/save";

	res = f_opendir(&dir, path);
	if (res == FR_OK) {
		while (1) {
			res = f_readdir(&dir, &fno);
			if (res != FR_OK || fno.fname[0] == 0)
				break;

			if (!strcmp(fno.fname, ".") || !strcmp(fno.fname, ".."))
				continue;

			if (fno.fattrib & AM_DIR)
				continue;

			if (is_file_in_keep_list(fno.fname)) {
				debug_log_write("Keep file: %s\n", fno.fname);
				continue;
			}

			s_printf(fullpath, "%s/%s", path, fno.fname);
			debug_log_write("Delete file: %s\n", fullpath);
			f_unlink(fullpath);
		}

		f_closedir(&dir);
	}
	f_cp_or_rm_rf("bis:/saveMeta", NULL);
	f_mkdir("bis:/saveMeta");
	f_unlink("bis:/PRF2SAFE.RCV");
	unmount_nand_part(&gpt, false, true, false, true);

	if (!mount_nand_part(&gpt, "USER", false, false, true, true, NULL, NULL, NULL, NULL)) {
		save_screenshot_and_go_back("wip_nand");
		return;
	}
	f_cp_or_rm_rf("bis:/Album", NULL);
	f_cp_or_rm_rf("bis:/Contents", NULL);
	f_cp_or_rm_rf("bis:/save", NULL);
	f_cp_or_rm_rf("bis:/saveMeta", NULL);
	f_cp_or_rm_rf("bis:/temp", NULL);
	f_unlink("bis:/PRF2SAFE.RCV");
	f_mkdir("bis:/Album");
	f_mkdir("bis:/Contents");
	f_mkdir("bis:/Contents/placehld");
	f_mkdir("bis:/Contents/registered");
	f_mkdir("bis:/save");
	f_mkdir("bis:/saveMeta");
	f_mkdir("bis:/temp");
unmount_nand_part(&	gpt, false, true, true, true);
	if (menu_on_sysnand) {
		f_cp_or_rm_rf("sd:/Nintendo", NULL);
	} else {
		if (strcmp(emu_cfg.nintendo_path, "") != 0 && emu_cfg.nintendo_path != NULL) {
			f_cp_or_rm_rf(emu_cfg.nintendo_path, NULL);
		}
	}
	log_printf(true, LOG_OK, LOG_MSG_WIP_SUCCESS);
	save_screenshot_and_go_back("wip_nand");
}

void fix_downgrade() {
	cls();
	log_printf(true, LOG_INFO, LOG_MSG_FNC_BEGIN, "downgrade fix");
	if (!wait_vol_plus()) {
		return;
	}
	delete_save_from_nand("8000000000000073", true);
	log_printf(true, LOG_OK, LOG_MSG_DG_SUCCESS);
	save_screenshot_and_go_back("dg_fix");
}

void del_erpt_save() {
	cls();
	log_printf(false, LOG_ERR, LOG_MSG_RM_ERPT_WARN);
	log_printf(true, LOG_INFO, LOG_MSG_FNC_BEGIN, "ERPT deletion");
	if (!wait_vol_plus()) {
		return;
	}
	delete_save_from_nand("80000000000000D1", true);
	log_printf(true, LOG_OK, LOG_MSG_RM_ERPT_SUCCESS);
	save_screenshot_and_go_back("del_erpt_save");
}

void remove_parental_control() {
	cls();
	log_printf(true, LOG_INFO, LOG_MSG_FNC_BEGIN, "parental control deletion");
	if (!wait_vol_plus()) {
		return;
	}
	delete_save_from_nand("8000000000000100", true);
	log_printf(true, LOG_OK, LOG_MSG_RM_PARENTAL_CONTROL_SUCCESS);
	save_screenshot_and_go_back("remove_parental_control");
}

void sync_joycons_between_nands() {
	cls();
	log_printf(false, LOG_INFO, LOG_MSG_SYNCH_JOYCONS_INFOS_1);
	log_printf(false, LOG_ERR, LOG_MSG_SYNCH_JOYCONS_INFOS_2);
	log_printf(true, LOG_INFO, LOG_MSG_FNC_BEGIN, "joycons synchronization");
	if (!wait_vol_plus()) {
		return;
	}
	f_transfer_from_nands("save/8000000000000050", true);
	log_printf(true, LOG_OK, LOG_MSG_SYNCH_JOYCONS_SUCCESS);
	save_screenshot_and_go_back("synch_joycons");
}

void build_prodinfo_and_flash(bool from_scratch) {
	extern const char* DONOR_PRODINFO_FILENAME;
	char generated_prodinfo_path[60];
	if (from_scratch) {
		s_printf(generated_prodinfo_path, "sd:/switch/generated_prodinfo_from_scratch.bin");
		f_unlink(generated_prodinfo_path);
		build_prodinfo(NULL, false);
	} else {
		s_printf(generated_prodinfo_path, "sd:/switch/generated_prodinfo_from_donor.bin");
		f_unlink(generated_prodinfo_path);
		build_prodinfo(DONOR_PRODINFO_FILENAME, false);
	}

	if (f_stat(generated_prodinfo_path, NULL) == FR_OK) {
		if (!flash_or_dump_part(true, generated_prodinfo_path, "PRODINFO", true)) {
			save_screenshot_and_go_back("PRODINFO_build_and_flash");
			return;
		}
	}

	if (from_scratch) {
		log_printf(true, LOG_OK, LOG_MSG_PRODINFO_BUILD_AND_FLASH_FROM_SCRATCH_SUCCESS);
	} else {
		log_printf(true, LOG_OK, LOG_MSG_PRODINFO_BUILD_AND_FLASH_FROM_DONOR_SUCCESS);
	}
	save_screenshot_and_go_back("PRODINFO_build_and_flash");
	return;
}