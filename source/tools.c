#include <string.h>
#include <stdarg.h>

#include "tools.h"
#include "config.h"
#include <gfx_utils.h>
#include "gfx/tui.h"
#include "keys/keys.h"
#include <libs/fatfs/ff.h>
#include <mem/heap.h>
#include <sec/se.h>
#include <soc/hw_init.h>
#include <soc/timer.h>
#include "storage/emummc.h"
#include <storage/emmc.h>
#include "storage/nx_emmc_bis.h"
#include <storage/sd.h>
#include <storage/sdmmc.h>
#include <utils/btn.h>
#include <utils/ini.h>
#include <utils/list.h>
#include <utils/sprintf.h>

#include "gfx/messages.h"
// #include "hos/pkg1.h"
#include "fuse_check/fuse_check.h"
#include "incognito/incognito.h"
#include "keys/cal0_read.h"
#include "keys/crypto.h"
#include "prodinfo_rewrite/prodinfo_rewrite.h"

bool emunand_probe_path(const char *path)
{
	FIL fp;
	char tmp[256];

	// Tester raw_based
	s_printf(tmp, "%s/raw_based", path);
	if (!f_open(&fp, tmp, FA_READ))
	{
		u32 sector = 0;
		if (!f_read(&fp, &sector, 4, NULL) && sector) {
			f_close(&fp);

			s_printf(tmp, "%s/Nintendo", path);
			if (!f_stat(tmp, NULL)) {
				// out->sector = sector;
				// s_printf(out->base_path, "%s", path);
				// s_printf(out->nintendo_path, "%s/Nintendo", path);
				return true;
			}
		}
		f_close(&fp);
	}

	// Tester file_based
	s_printf(tmp, "%s/file_based", path);
	if (!f_stat(tmp, NULL))
	{
		s_printf(tmp, "%s/Nintendo", path);
		if (!f_stat(tmp, NULL)) {
			// out->sector = 0;
			// s_printf(out->base_path, "%s", path);
			// s_printf(out->nintendo_path, "%s/Nintendo", path);
			return true;
		}
	}

	return false;
}

char *bdk_strdup(const char *s)
{
	if (!s)
		return NULL;

	size_t len = strlen(s) + 1;
	char *dst = malloc(len);
	if (!dst) {
		log_printf(true, LOG_ERR, LOG_MSG_MALLOC_ERROR);
		return NULL;
	}

	memcpy(dst, s, len);
	return dst;
}

FRESULT mkdir_recursive(const char *path) {
	char tmp[256];
	char *p = NULL;

	s_printf(tmp, "%s", path);
	tmp[sizeof(tmp) - 1] = '\0';

	for (p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			f_mkdir(tmp);
			*p = '/';
		}
	}
	return FR_OK;
}

bool verifyProdinfo(u8 *blob) {
	log_printf(true, LOG_INFO, LOG_MSG_INCOGNITO_VERIF_BEGIN);

	/* This verification is now performed using the unified PRODINFO table-based verifier. */
	prodinfo_verify_report_t report;
	int rc = prodinfo_verify_or_rewrite_hashes(blob, NX_EMMC_CALIBRATION_SIZE, &report, NULL, NX_EMMC_CALIBRATION_SIZE);
	// int rc = prodinfo_verify_hashes(blob, NX_EMMC_CALIBRATION_SIZE, &report);
	if (rc != PI_OK) {
		log_printf(true, LOG_ERR, LOG_MSG_INCOGNITO_VERIF_ERR);
		return false;
	}

	if (report.crc_errors == 0 && report.sha_errors == 0) {
		char serial[15] = "";
		if (blob)
			memcpy(serial, blob + 0x250, 14);

		log_printf(true, LOG_OK, LOG_MSG_INCOGNITO_VERIF_SUCCESS, serial);
		return true;
	}

	log_printf(true, LOG_ERR, LOG_MSG_INCOGNITO_VERIF_ERR);
	return false;
}

void emunand_list_build(link_t *inilist, bool count_only) {
	LIST_FOREACH_ENTRY(ini_sec_t, ini_sec, inilist, link){
		if (ini_sec->type == INI_CHOICE){
			LIST_FOREACH_ENTRY(ini_kv_t, kv, &ini_sec->kvs, link) {
				if (!strcmp("emupath", kv->key) && emunand_count < MAX_EMUNANDS) {
					// emunand_entry_t e;
					// s_printf(e.name, "%s", ini_sec->name);
					if (emunand_probe_path(kv->val)) {
						if (!count_only) {
							emunands[emunand_count].name = bdk_strdup(ini_sec->name);
							emunands[emunand_count].base_path = bdk_strdup(kv->val);
							// emunands[emunand_count] = e;
						}
						emunand_count++;
					}
				}
			}
		}   
	}
}

void build_emunand_list() {
	if (emu_cfg.sector != 0 || emu_cfg.path) {
		emunand_count++;
	}
	LIST_INIT(list);
	if (!f_stat("sd:/bootloader/hekate_ipl.ini", NULL)) {
		if (!ini_parse(&list, "sd:/bootloader/hekate_ipl.ini", false)) {
			debug_log_write("Parsing hekate_ipl.ini failed.\n");
		} else {
			emunand_list_build(&list, true);
			ini_free(&list);
			list_init(&list);
		}
	}
	if (!f_stat("sd:/bootloader/ini", NULL)) {
		if (!ini_parse(&list, "sd:/bootloader/ini", true)) {
			debug_log_write("Parsing bootloader/ini folder  failed.\n");
		} else {
			emunand_list_build(&list, true);
			ini_free(&list);
			list_init(&list);
		}
	}
	emunands = calloc(emunand_count * sizeof(emunand_entry_t), 1);
	if (!emunands) {
		return;
	}
	emunand_count = 0;
	if (emu_cfg.sector != 0 || emu_cfg.path) {
		debug_log_write("Default emunand added.\n");
		emunands[emunand_count].name = bdk_strdup("default_config");
		emunands[emunand_count].base_path = bdk_strdup(emu_cfg.path);
		// s_printf(emunands[0].name, "default_config");
		// s_printf(emunands[0].base_path, "%s", emu_cfg.path);
		emunand_count++;
	}
	if (!f_stat("sd:/bootloader/hekate_ipl.ini", NULL)) {
		if (!ini_parse(&list, "sd:/bootloader/hekate_ipl.ini", false)) {
			debug_log_write("Parsing hekate_ipl.ini failed.\n");
		} else {
			emunand_list_build(&list, false);
			ini_free(&list);
			list_init(&list);
		}
	}
	if (!f_stat("sd:/bootloader/ini", NULL)) {
		if (!ini_parse(&list, "sd:/bootloader/ini", true)) {
			debug_log_write("Parsing bootloader/ini folder  failed.\n");
		} else {
			emunand_list_build(&list, false);
			ini_free(&list);
			list_init(&list);
		}
	}
}

void apply_emunand(const emunand_entry_t *e) {
	debug_log_write("applying emunand %s\n", e->name);
	if (strcmp(e->name, "default_config") == 0) {
		emummc_load_cfg();
	} else {
		emummc_set_path(e->base_path);
	}
	emu_cfg.enabled = true;
	h_cfg.emummc_force_disable = false;
	menu_on_sysnand = false;

	/*
	if (e->sector == 0) {
		s_printf(emu_cfg.path, "%s", e->base_path);
	} else {
		emu_cfg.path = NULL;
	}

	s_printf(emu_cfg.nintendo_path, "%s", e->nintendo_path);
	*/
}

int emunand_select_menu() {
	int idx = 0;
	int cnt = emunand_count;

	gfx_clear_partial_grey(0x1B, 0, 1256);

	while (1) {
		gfx_con_setpos(0, 0);
		display_title();
		gfx_printf("Select emuNAND:\n\n");

		for (int i = 0; i < cnt; i++) {
			if (i == idx)
				gfx_con_setcol(0xFF1B1B1B, 1, 0xFFCCCCCC);
			else
				gfx_con_setcol(0xFFCCCCCC, 1, 0xFF1B1B1B);

			gfx_printf(" %s\n", emunands[i].name);
		}

		gfx_con_setcol(0xFFCCCCCC, 1, 0xFF1B1B1B);
		gfx_con_setpos(0, 1191);
		gfx_printf("VOL: Move | PWR: Select");

		u32 btn = btn_wait();

		if (btn & BTN_VOL_DOWN) {
			idx++;
			if (idx >= cnt)
				idx = 0;
		} else if (btn & BTN_VOL_UP) {
			if (idx == 0)
				idx = cnt - 1;
			else
				idx--;
		} else if (btn & BTN_POWER) {
			return idx;
		}

		gfx_clear_partial_grey(0x1B, 0, 1256);
	}
}

void select_and_apply_emunand() {
	if (emunand_count == 0) {
		menu_on_sysnand = true;
		emummc_available = false;
		h_cfg.emummc_force_disable = true;
		emu_cfg.enabled = false;
		debug_log_write("No emunand, switching to sysnand\n");
		return;
	}

	if (emunand_count == 1) {
		debug_log_write("Applying default emunand\n");
		apply_emunand(&emunands[0]);
		return;
	}

	int sel = emunand_select_menu();
	if (sel >= 0 && sel < emunand_count) {
		debug_log_write("Applying emunand %s\n", emunands[sel]);
		apply_emunand(&emunands[sel]);
		prev_sec_emunand = cur_sec_emunand;
		cur_sec_emunand = sel;
	}
}

void emunand_list_free() {
	if (!emunands)
		return;

	for (int i = 0; i < emunand_count; i++) {
		if (emunands[i].name)
			free(emunands[i].name);

		if (emunands[i].base_path)
			free(emunands[i].base_path);
	}

	free(emunands);
	emunands = NULL;
	emunand_count = 0;
}

void debug_log_start_impl() {
	FIL debug_log_file;
	f_open(&debug_log_file, "sd:/locksmith-rcm.log", FA_CREATE_ALWAYS | FA_WRITE);
	f_close(&debug_log_file);
}

void debug_log_write_impl(const char *text, ...) {
	FIL debug_log_file;
	UINT bw;

	char buffer[4096];
	va_list args;
	va_start(args, text);
	s_vprintf(buffer, text, args);
	va_end(args);

	f_open(&debug_log_file, "sd:/locksmith-rcm.log", FA_OPEN_APPEND | FA_WRITE);
	f_write(&debug_log_file, buffer, strlen(buffer), &bw);
	// f_sync(&debug_log_file);
	f_close(&debug_log_file);
}

static bool part_is_encrypted(emmc_part_t *part) {
	switch (part->index) {
		case 0:  /* PRODINFO */
		case 1:  /* PRODINFOF */
		case 8:  /* SAFE */
		case 9:  /* SYSTEM */
		case 10: /* USER */
			return true;
			break;
		default:
			return false;
			break;
	}
	return false;
}

bool mount_nand_part(link_t *gpt, const char *part_name, bool nand_open, bool set_partition, bool fatfs_mount, bool test_loaded_keys, u64 *part_size_bytes_buf, bool *is_boot_buf, bool *is_bis_buf, emmc_part_t *part_buf) {
	bool is_boot = false;
	bool use_bis = false;
	u64 part_size_bytes = 0;
	emmc_part_t *part = NULL;
	if (nand_open) {
				sd_mount();
		if (emummc_storage_init_mmc()) {
			log_printf(true, LOG_ERR, LOG_MSG_ERR_INIT_EMMC);
			return false;
		}
	}

	/* Select partition context */
	if (strcmp(part_name, "BOOT0") == 0) {
		if (set_partition && !emummc_storage_set_mmc_partition(EMMC_BOOT0)) {
			log_printf(true, LOG_ERR, LOG_MSG_ERR_SET_PARTITION);
			return false;
		}
		is_boot = true;
		part_size_bytes = (u64)emmc_storage.ext_csd.boot_mult << 17; // boot size from ext_csd
	} else if (strcmp(part_name, "BOOT1") == 0) {
		if (set_partition && !emummc_storage_set_mmc_partition(EMMC_BOOT1)) {
			log_printf(true, LOG_ERR, LOG_MSG_ERR_SET_PARTITION);
			return false;
		}
		is_boot = true;
		part_size_bytes = (u64)emmc_storage.ext_csd.boot_mult << 17;
	} else {
		if (set_partition && !emummc_storage_set_mmc_partition(EMMC_GPP)) {
			log_printf(true, LOG_ERR, LOG_MSG_ERR_SET_PARTITION);
			return false;
		}
		emmc_gpt_parse(gpt);
		part = emmc_part_find(gpt, part_name);
		if (!part) {
			log_printf(true, LOG_ERR, LOG_MSG_ERR_FOUND_PARTITION, part_name);
			emmc_gpt_free(gpt);
			list_init(gpt);
			emummc_storage_end();
			sd_mount();
			return false;
		}
		part_size_bytes = (u64)(part->lba_end - part->lba_start + 1) * EMMC_BLOCKSIZE;

		if (part_buf != NULL) {
			*part_buf = *part;
		}

		/* determine if partition is BIS-encrypted */
		use_bis = part_is_encrypted(part);
		if (use_bis) {
			/* init bis for this partition (cache disabled) */
			nx_emmc_bis_init(part, false, 0);
		}
		if (strcmp(part_name, "PRODINFO") == 0 && test_loaded_keys) {
			if (!cal0_read(KS_BIS_00_TWEAK, KS_BIS_00_CRYPT, cal0_buf, NULL)) {
				nx_emmc_bis_end();
				emmc_gpt_free(gpt);
				list_init(gpt);
				emummc_storage_end();
				sd_mount();
				return false;
			}
		}
		if ((fatfs_mount || test_loaded_keys) && (strcmp(part_name, "PRODINFOF") == 0 || strcmp(part_name, "SAFE") == 0 || strcmp(part_name, "SYSTEM") == 0 || strcmp(part_name, "USER") == 0)) {
			FRESULT test_mount = f_mount(&emmc_fs, "bis:", 1);
			if (test_mount) {
				log_printf(true, LOG_ERR, LOG_MSG_ERR_MOUNT_PARTITION, part_name);
				debug_log_write("Failed to mounte partition, error %d.\n", (u32) test_mount);
				nx_emmc_bis_end();
				emmc_gpt_free(gpt);
				list_init(gpt);
				emummc_storage_end();
				sd_mount();
				return false;
			}
			if (test_loaded_keys && !fatfs_mount) {
				f_mount(NULL, "bis:", 1);
			}
		}
	}
	if (part_size_bytes_buf != NULL) {
	*part_size_bytes_buf = part_size_bytes;
	}
	if (is_boot_buf != NULL) {
	*is_boot_buf = is_boot;
	}
	if (is_bis_buf != NULL) {
		*is_bis_buf = use_bis;
	}
	return true;
}

void unmount_nand_part(link_t *gpt, bool is_boot_part, bool is_bis, bool nand_close, bool fatfs_unmount) {
	if (fatfs_unmount) {
		f_mount(NULL, "bis:", 1);
	}
	if (!is_boot_part) {
		if (is_bis) {
			nx_emmc_bis_end();
		}
		if (gpt != NULL) {
			emmc_gpt_free(gpt);
			list_init(gpt);
		}
	}
	if (nand_close) {
		emummc_storage_end();
		sd_mount();
	}
}

bool wait_vol_plus() {
	if (!called_from_config_files && !called_from_AIO_LS_Pack_Updater) {
		while (btn_read_vol() & (BTN_VOL_UP | BTN_VOL_DOWN)) { } // Flush the volume butons before waiting an other entry, needed if timing is to short from the caller previous buton press
		u8 btn = btn_wait();
		if (btn != BTN_VOL_UP) {
			return false;
		}
	}
	return true;
}

bool delete_save_from_nand(const char* savename, bool on_system_part) {
	if (!bis_loaded) {
		return false;
	}

	bool success = false;
	LIST_INIT(gpt);

	if (on_system_part) {
		log_printf(true, LOG_INFO, LOG_MSG_DELETE_SAVE_SYSTEM, savename);
		if (!mount_nand_part(&gpt, "SYSTEM", true, true, true, true, NULL, NULL, NULL, NULL)) {
			return false;
		}
	} else {
		log_printf(true, LOG_INFO, LOG_MSG_DELETE_SAVE_USER, savename);
		if (!mount_nand_part(&gpt, "USER", true, true, true, true, NULL, NULL, NULL, NULL)) {
			return false;
		}
	}

	char buff[50];
	s_printf(buff, "bis:/save/%s", savename);
	FRESULT fr = f_unlink(buff);
	if (fr == FR_OK) {
		gfx_printf("\n\n");
		log_printf(true, LOG_OK, LOG_MSG_DELETE_FILE_SUCCESS);
		gfx_printf("\n");
		success = true;
	} else if (fr == FR_NO_FILE) {
		gfx_printf("\n\n");
		log_printf(true, LOG_WARN, LOG_MSG_DELETE_FILE_WARNING);
		gfx_printf("\n");
		success = true;
	} else {
		gfx_printf("\n\n");
		log_printf(true, LOG_ERR, LOG_MSG_DELETE_FILE_ERROR, (u32)fr);
		gfx_printf("\n");
	}

unmount_nand_part(&gpt, false, true, true, true);
	return success;
}

static ui_pos_t spinner_pos;
static u32 spinner_timer_count = 0;
static bool spinner_visible = false;
void ui_spinner_begin() {
	gfx_con_getpos(&spinner_pos.x, &spinner_pos.y);
	spinner_timer_count = 0;
	spinner_visible = false;
}

void ui_spinner_draw() {
	if (get_tmr_ms() < spinner_timer_count) {
		return;
	}
	const char spin_chars[] = "-----";

	gfx_con_setpos(spinner_pos.x, spinner_pos.y);
	if (spinner_visible) {
		gfx_printf("%s", spin_chars);
		spinner_visible = false;
	} else {
		gfx_printf("     ");
		spinner_visible = true;
	}
	gfx_con_setpos(spinner_pos.x, spinner_pos.y);
	spinner_timer_count = get_tmr_ms() + 500;
}

void ui_spinner_clear() {
	spinner_timer_count = 0;
	spinner_visible = false;
	gfx_con_setpos(spinner_pos.x, spinner_pos.y);
	gfx_printf("     ");
	gfx_con_setpos(spinner_pos.x, spinner_pos.y);
}

// Helper function to get eMMC ID (hex string from CID serial, matching Hekate format)
bool get_emmc_id(char *emmc_id_out) {
	// Initialize eMMC if not already done
	if (!emmc_storage.initialized && emummc_storage_init_mmc()) {
		return false;
	}

	// Convert CID serial to hexadecimal string (without leading zeros, like Hekate)
	s_printf(emmc_id_out, "%x", emmc_storage.cid.serial);

	emummc_storage_end();
	sd_mount();

	return true;
}

/*
static bool sd_has_enough_space(u64 required_bytes) {
	FATFS *fs;
	DWORD fre_clust;
	u64 free_bytes;

	if (f_getfree("sd:", &fre_clust, &fs) != FR_OK)
		return false;

	free_bytes = (u64)fre_clust * fs->csize * 512;
	return free_bytes >= required_bytes;
}
*/

bool flash_or_dump_part(bool flash, const char *sd_filepath, const char *part_name, bool bis_read_or_write_enable) {
	if (bis_read_or_write_enable && !bis_loaded) {
		return false;
	}
	bool is_boot = false;
	bool use_bis = false;
	u64 part_size_bytes = 0;
	FRESULT fr;
	FIL fp;
	emmc_part_t part;
	u64 filesize = 0;
	u8 *buff = NULL;

bool return_value = false;
bool file_is_closed = true;

	sd_mount();

	if (flash) {
		log_printf(true, LOG_INFO, LOG_MSG_FLASH_PARTITION_BEGIN, sd_filepath, part_name);
		fr = f_open(&fp, sd_filepath, FA_READ);
		if (fr != FR_OK) {
			log_printf(true, LOG_ERR, LOG_MSG_ERR_OPEN_FILE, sd_filepath);
			return return_value;
		} else {
			file_is_closed = false;
		}
		filesize = f_size(&fp);
		if (filesize == 0) {
			log_printf(true, LOG_ERR, LOG_MSG_ERR_EMPTY_FILE);
			f_close(&fp);
			return return_value;
		}
	} else {
	log_printf(true, LOG_INFO, LOG_MSG_DUMP_PARTITION_BEGIN, part_name, sd_filepath);
	}

	LIST_INIT(gpt);
	bool test_part;
	if (!bis_read_or_write_enable) {
		test_part = mount_nand_part(&gpt, part_name, true, true, false, false, &part_size_bytes, &is_boot, &use_bis, &part);
	} else {
		test_part = mount_nand_part(&gpt, part_name, true, true, false, true, &part_size_bytes, &is_boot, &use_bis, &part);
	}
	if (!test_part) {
		if (flash) f_close(&fp);
		return return_value;
	}

	buff = (BYTE*)malloc(COPY_BUF_SIZE);
	bool file_is_created = false;
	if (!buff) {
		log_printf(true, LOG_ERR, LOG_MSG_MALLOC_ERROR);
		goto cleanup;
	}

	if (strcmp(part_name, "PRODINFO") == 0) {
		f_close(&fp);
		file_is_closed = true;
		// u8* cal0_buf = (u8 *)malloc(NX_EMMC_CALIBRATION_SIZE);
		if (!flash) {
			if (!cal0_read(KS_BIS_00_TWEAK, KS_BIS_00_CRYPT, cal0_buf, NULL)) {
				// free((u8*)cal0_buf);
				goto cleanup;
			}
		} else {
			if (!cal0_read(KS_BIS_00_TWEAK, KS_BIS_00_CRYPT, cal0_buf, sd_filepath)) {
				// free(cal0_buf);
				goto cleanup;
			}
		}
		if (!verifyProdinfo(cal0_buf)) {
			// free(cal0_buf);
			goto cleanup;
		}
		// free(cal0_buf);
		if (flash) {
			fr = f_open(&fp, sd_filepath, FA_READ);
			if (fr != FR_OK) {
				log_printf(true, LOG_ERR, LOG_MSG_ERR_OPEN_FILE, sd_filepath);
				goto cleanup;
			} else {
				file_is_closed = false;
			}
		}
	}
	if (flash) {
		if (filesize > part_size_bytes) {
			log_printf(true, LOG_ERR, LOG_MSG_FLASH_PARTITION_FILE_TO_BIG);
			goto cleanup;
		}
		// For safety: require sector-aligned input (avoid implicit padding issues when writing encrypted partitions)
		if ((filesize % EMMC_BLOCKSIZE) != 0) {
			log_printf(true, LOG_ERR, LOG_MSG_FLASH_PARTITION_FILE_NOT_ALLIGNED);
			goto cleanup;
		}
	} else {
		if ((part_size_bytes % EMMC_BLOCKSIZE) != 0) {
			log_printf(true, LOG_ERR, LOG_MSG_DUMP_PARTITION_NOT_ALLIGNED);
			goto cleanup;
		}
		// if (!sd_has_enough_space(part_size_bytes)) {
			// log_printf(true, LOG_ERR, LOG_MSG_dump_PARTITION_FILE_TO_BIG);
			// goto cleanup;
		// }
		char dirpath[256];
		s_printf(dirpath, "%s", sd_filepath);
		char *last_slash = strrchr(dirpath, '/');
		if (last_slash && last_slash[1] != '\0') {
			last_slash[1] = '\0';
			mkdir_recursive(dirpath);
		}
		fr = f_open(&fp, sd_filepath, FA_WRITE | FA_CREATE_ALWAYS);
		if (fr != FR_OK) {
			log_printf(true, LOG_ERR, LOG_MSG_ERR_OPEN_FILE, sd_filepath);
			goto cleanup;
		} else {
			file_is_closed = false;
		}
		file_is_created = true;
	}

	// bool do_bis_io = (use_bis && bis_read_or_write_enable);
	u32 lba_start;
	if (is_boot) {
		lba_start = 0;
	} else if (use_bis && bis_read_or_write_enable) {
		lba_start = 0;
	} else {
		lba_start = part.lba_start;
	}
	u32 curLba = lba_start;
	u64 totalSectorsSrc;
	if (flash) {
		totalSectorsSrc = f_size(&fp) / EMMC_BLOCKSIZE;
	} else {
		totalSectorsSrc = part_size_bytes / EMMC_BLOCKSIZE;;
	}

	ui_spinner_begin();
	while (totalSectorsSrc > 0){
		ui_spinner_draw();
		int Res = 0;
		u32 num = MIN(totalSectorsSrc, COPY_BUF_SIZE / EMMC_BLOCKSIZE);

		if (flash) {
			UINT br;
			if ((f_read(&fp, buff, num * EMMC_BLOCKSIZE, &br))){
				log_printf(true, LOG_ERR, LOG_MSG_ERR_FILE_READ);
				ui_spinner_clear();
				goto cleanup;
				break;
			}
			if (use_bis && bis_read_or_write_enable) {
				Res = nx_emmc_bis_write(curLba, num, buff);
			} else  {
				Res = emummc_storage_write(curLba, num, buff);
			}
			if (!Res){
				log_printf(true, LOG_ERR, LOG_MSG_FLASH_PARTITION_ERR_PARTITION_WRITE);
				ui_spinner_clear();
				goto cleanup;
				break;
			}
		} else {
			if (use_bis && bis_read_or_write_enable) {
				Res = nx_emmc_bis_read(curLba, num, buff);
			} else {
				Res = emummc_storage_read(curLba, num, buff);
			}
			if (!Res) {
				log_printf(true, LOG_ERR, LOG_MSG_ERR_FILE_READ);
				ui_spinner_clear();
				goto cleanup;
			}
			UINT bw;
			fr = f_write(&fp, buff, num * EMMC_BLOCKSIZE, &bw);
			if (fr != FR_OK || bw != num * EMMC_BLOCKSIZE) {
				log_printf(true, LOG_ERR, LOG_MSG_DUMP_PARTITION_ERR_PARTITION_WRITE);
				ui_spinner_clear();
				goto cleanup;
			}
		}

		curLba += num;
		totalSectorsSrc -= num;
	}

	if (strcmp(part_name, "PRODINFO") == 0) {
		f_close(&fp);
		file_is_closed = true;
		if (use_bis && bis_read_or_write_enable) {
			nx_emmc_bis_end();
			use_bis = false;
		}
		// u8* cal0_buf = (u8 *)malloc(NX_EMMC_CALIBRATION_SIZE);
		if (flash) {
			if (!cal0_read(KS_BIS_00_TWEAK, KS_BIS_00_CRYPT, cal0_buf, NULL)) {
				// free(cal0_buf);
				goto cleanup;
			}
		} else {
			if (!cal0_read(KS_BIS_00_TWEAK, KS_BIS_00_CRYPT, cal0_buf, sd_filepath)) {
				// free(cal0_buf);
				goto cleanup;
			}
		}
		if (!verifyProdinfo(cal0_buf)) {
			// free(cal0_buf);
			goto cleanup;
		}
		// free(cal0_buf);
	}

	return_value = true;
	if (flash) {
		log_printf(true, LOG_OK, LOG_MSG_FLASH_PARTITION_SUCCESS);
	} else {
		log_printf(true, LOG_OK, LOG_MSG_DUMP_PARTITION_SUCCESS);
	}

cleanup:
	if (!return_value && file_is_created) {
		f_unlink(sd_filepath);
	}
	if (buff) free(buff);
	if (!file_is_closed) f_close(&fp);
	unmount_nand_part(&gpt, is_boot, use_bis, true, false);
	return return_value;
}

u8 *load_file_to_mem(const char *path, UINT *out_size) {
    FIL fp;
    FRESULT fr = f_open(&fp, path, FA_READ);
    if (fr != FR_OK) return NULL;

    FSIZE_t sz = f_size(&fp);
    if (sz == 0 || sz > 0xFFFFFFFF) { f_close(&fp); return NULL; }

    u8 *buf = malloc((size_t)sz);
    if (!buf) { f_close(&fp); return NULL; }

    UINT br = 0;
    fr = f_read(&fp, buf, (UINT)sz, &br);
    f_close(&fp);

    if (fr != FR_OK || br != (UINT)sz) {
        free(buf);
        return NULL;
    }

    *out_size = br;
    return buf;
}

bool f_transfer_from_nands(const char *file_path, bool on_system_part) {
	if (menu_on_sysnand) {
		if (on_system_part) {
			log_printf(true, LOG_INFO, LOG_MSG_FILE_TRANSFERT_FROM_NANDS_SYS_TO_EMU_SYSTEM, file_path);
		} else {
			log_printf(true, LOG_INFO, LOG_MSG_FILE_TRANSFERT_FROM_NANDS_SYS_TO_EMU_USER, file_path);
		}
	} else {
		if (on_system_part) {
			log_printf(true, LOG_INFO, LOG_MSG_FILE_TRANSFERT_FROM_NANDS_EMU_TO_SYS_SYSTEM, file_path);
		} else {
			log_printf(true, LOG_INFO, LOG_MSG_FILE_TRANSFERT_FROM_NANDS_EMU_TO_SYS_USER, file_path);
		}
	}

	if (!sysmmc_available) {
		log_printf(true, LOG_ERR, LOG_MSG_ERR_SYSMMC_NOT_AVAILABLE);
		return false;
	}
	if (!emummc_available) {
		log_printf(true, LOG_ERR, LOG_MSG_ERR_EMUMMC_NOT_AVAILABLE);
		return false;
	}

	if (!bis_loaded) {
		return false;
	}

	LIST_INIT(gpt);

	FIL fs, fd;
	FRESULT res = FR_OK;
	char full_file_path[256];
	s_printf(full_file_path, "bis:/%s", file_path);

	uint64_t off = 0;
	bool first = true;
	while (1) {
		if (menu_on_sysnand) {
			h_cfg.emummc_force_disable = true;
			emu_cfg.enabled = false;
		} else {
			h_cfg.emummc_force_disable = false;
			emu_cfg.enabled = true;
		}
		if (on_system_part) {
			if (!mount_nand_part(&gpt, "SYSTEM", true, true, true, true, NULL, NULL, NULL, NULL)) {
				return false;
			}
		} else {
			if (!mount_nand_part(&gpt, "USER", true, true, true, true, NULL, NULL, NULL, NULL)) {
				return false;
			}
		}
		res = f_open(&fs, full_file_path, FA_READ);
		if (res != FR_OK) {
			log_printf(true, LOG_ERR, LOG_MSG_FILE_TRANSFERT_FROM_NANDS_ERR_OPEN_SRC);
			unmount_nand_part(&gpt, false, true, true, true);
			return false;
		}
		res = f_lseek(&fs, off);
		if (res != FR_OK) {
			log_printf(true, LOG_ERR, LOG_MSG_FILE_TRANSFERT_FROM_NANDS_ERR_READ_SRC);
			f_close(&fs);
			unmount_nand_part(&gpt, false, true, true, true);
			return false;
		}
		UINT br;
		res = f_read(&fs, copy_buf, COPY_BUF_SIZE, &br);
		if (res != FR_OK) {
			log_printf(true, LOG_ERR, LOG_MSG_FILE_TRANSFERT_FROM_NANDS_ERR_READ_SRC);
			f_close(&fs);
			unmount_nand_part(&gpt, false, true, true, true);
			return false;
		}
		f_close(&fs);
		unmount_nand_part(&gpt, false, true, true, true);
		if (br == 0) {
			res = FR_OK;
			break;
		}
		if (menu_on_sysnand) {
			h_cfg.emummc_force_disable = false;
			emu_cfg.enabled = true;
		} else {
			h_cfg.emummc_force_disable = true;
			emu_cfg.enabled = false;
		}
		if (on_system_part) {
			if (!mount_nand_part(&gpt, "SYSTEM", true, true, true, true, NULL, NULL, NULL, NULL)) {
				res = FR_DISK_ERR;
				break;
			}
		} else {
			if (!mount_nand_part(&gpt, "USER", true, true, true, true, NULL, NULL, NULL, NULL)) {
				res = FR_DISK_ERR;
				break;
			}
		}
		if (first) {
			res = f_open(&fd, full_file_path, FA_WRITE | FA_CREATE_ALWAYS);
			first = false;
		} else {
			res = f_open(&fd, full_file_path, FA_WRITE);
		}
		if (res != FR_OK) {
			log_printf(true, LOG_ERR, LOG_MSG_FILE_TRANSFERT_FROM_NANDS_ERR_OPEN_DST);
			unmount_nand_part(&gpt, false, true, true, true);
			break;
		}
		res = f_lseek(&fd, off);
		if (res != FR_OK) {
			log_printf(true, LOG_ERR, LOG_MSG_FILE_TRANSFERT_FROM_NANDS_ERR_WRITE_DST);
			f_close(&fd);
			unmount_nand_part(&gpt, false, true, true, true);
			break;
		}
		UINT bw;
		res = f_write(&fd, copy_buf, br, &bw);
		if (res != FR_OK || bw != br) {
			res = FR_DISK_ERR;
			log_printf(true, LOG_ERR, LOG_MSG_FILE_TRANSFERT_FROM_NANDS_ERR_WRITE_DST);
			f_close(&fd);
			unmount_nand_part(&gpt, false, true, true, true);
			break;
		}
		f_sync(&fd);
		f_close(&fd);
		unmount_nand_part(&gpt, false, true, true, true);
		off += br;
	}

		if (menu_on_sysnand) {
			h_cfg.emummc_force_disable = true;
			emu_cfg.enabled = false;
		} else {
			h_cfg.emummc_force_disable = false;
			emu_cfg.enabled = true;
		}

	if (!res) {
		log_printf(true, LOG_OK, LOG_MSG_FILE_TRANSFERT_FROM_NANDS_SUCCESS);
		return true;
	} else {
		return false;
	}
}

bool is_autorcm_enabled() {
	if (!physical_emmc_ok) {
		return true;
	}
	u8 mod0, mod1;
	u8 *tempbuf = (u8 *)malloc(0x200);

	// Get the correct RSA modulus byte masks.
	nx_emmc_get_autorcm_masks(&mod0, &mod1);

	// Get 1st RSA modulus.
	emmc_set_partition(EMMC_BOOT0);
	sdmmc_storage_read(&emmc_storage, 0x200 / EMMC_BLOCKSIZE, 1, tempbuf);

	// Check if 2nd byte of modulus is correct.
	bool enabled = false;
	if (tempbuf[0x11] == mod1)
		if (tempbuf[0x10] != mod0)
			enabled = true;

	free(tempbuf);

	return enabled;
}

void save_screenshot_and_go_back(const char* filename) {
	if (called_from_config_files || called_from_AIO_LS_Pack_Updater) {
		return;
	}
	log_printf(false, LOG_INFO, LOG_MSG_PROPOSE_TAKE_SCREENSHOT);
	if (wait_vol_plus()) {
		int res = save_fb_to_bmp(filename);
		if (!res) {
			log_printf(false, LOG_OK, LOG_MSG_TAKE_SCREENSHOT_SUCCESS, filename);
		} else {
			log_printf(false, LOG_ERR, LOG_MSG_TAKE_SCREENSHOT_ERROR);
		}
		log_printf(false, LOG_INFO, LOG_MSG_BACK_TO_MENU);
		btn_wait();
	}
}

void display_title() {
	if (menu_on_sysnand) {
		log_printf(false, LOG_ERR, LOG_MSG_TITLE_SYSNAND, LS_VER_MJ, LS_VER_MN, LS_VER_HF);
	} else {
		log_printf(false, LOG_OK, LOG_MSG_TITLE_EMUNAND, LS_VER_MJ, LS_VER_MN, LS_VER_HF);
	}
}

void cls() {
	gfx_clear_grey(0x1B);
	gfx_con_setpos(0, 0);
	display_title();
}

FRESULT easy_rename(const char* old, const char* new) {
	log_printf(true, LOG_INFO, LOG_MSG_FILE_RENAME, old, new);
	FRESULT res = FR_OK;
	if (f_stat(old, NULL) != FR_OK) {
		return res;
	}
	if (f_stat(old, NULL) == FR_OK) {
		if (f_stat(new, NULL) == FR_OK) {
			res = f_unlink(new);
		}
		if (res == FR_OK) {
			res = f_rename(old, new);
		}
	}
	return res;
}

FRESULT f_copy(const char *src, const char *dst) {
	FIL fs, fd;
	FRESULT fr;
	UINT r, w;

	debug_log_write("Copying file '%s' -> '%s'\n", src, dst);

	fr = f_open(&fs, src, FA_READ);
	if (fr != FR_OK)
		return fr;

	fr = f_open(&fd, dst, FA_WRITE | FA_CREATE_ALWAYS);
	if (fr != FR_OK) {
		f_close(&fs);
		return fr;
	}

	/*
	bool null_buf = false;
	if (buf == NULL) {
		null_buf = true;
		buf = (BYTE*)malloc(COPY_BUF_SIZE);
		if (!buf) {
			log_printf(true, LOG_ERR, LOG_MSG_MALLOC_ERROR);
			f_close(&fd);
			f_close(&fs);
			return FR_NOT_ENOUGH_CORE;
		}
	}
	*/

	ui_spinner_begin();
	do {
		ui_spinner_draw();
		fr = f_read(&fs, copy_buf, COPY_BUF_SIZE, &r);
		if (fr != FR_OK) {
			log_printf(true, LOG_ERR, LOG_MSG_ERR_FILE_READ);
			break;
		}

		if (r == 0)
			break;

		fr = f_write(&fd, copy_buf, r, &w);
		if (fr != FR_OK || w != r) {
			break;
		}
	} while (1);

	f_sync(&fd);

	ui_spinner_clear();
	f_close(&fs);
	f_close(&fd);
	// FILINFO fno;
	// f_stat(dst, &fno);
	// if (null_buf) free((BYTE*)buf);
	return fr;
}

FRESULT f_cp_or_rm_rf(const char *src_root, const char *dst_root)
{
	const bool do_copy = (dst_root != NULL);
	FRESULT res;

// cp_rm_ctx_t ctx[sizeof(cp_rm_ctx_t)];
	cp_rm_ctx_t *ctx = malloc(sizeof(cp_rm_ctx_t));
	if (!ctx) {
		log_printf(true, LOG_ERR, LOG_MSG_MALLOC_ERROR);
		return FR_NOT_ENOUGH_CORE;
	}

	/*
	ctx->copy_buf = (BYTE*)malloc(COPY_BUF_SIZE);
	if (!ctx->copy_buf) {
		free(ctx);
		log_printf(true, LOG_ERR, LOG_MSG_MALLOC_ERROR);
		return FR_NOT_ENOUGH_CORE;
	}
	*/

	int sp = 0;

	s_printf(ctx->src_stack[0], "%s", src_root);

	if (do_copy) {
		log_printf(true, LOG_INFO, LOG_MSG_FOLDER_COPY_BEGIN, src_root, dst_root);
		s_printf(ctx->dst_stack[0], "%s", dst_root);
		f_mkdir(dst_root);
	} else {
		log_printf(true, LOG_INFO, LOG_MSG_FOLDER_DELETE_BEGIN, src_root);
	}

	res = f_opendir(&ctx->dir[0], ctx->src_stack[0]);
	if (res != FR_OK) {
		log_printf(true, LOG_ERR, LOG_MSG_ERR_OPEN_FOLDER, ctx->src_stack[0]);
		goto out;
	}

	while (sp >= 0) {
		char name[256 + 1];
		res = f_readdir(&ctx->dir[sp], &ctx->fno[sp]);
		if (res != FR_OK || ctx->fno[sp].fname[0] == 0) {
			f_closedir(&ctx->dir[sp]);
			if (!do_copy && sp > 0)
				f_unlink(ctx->src_stack[sp]);
			sp--;
			continue;
		}

	s_printf(name, "%s", ctx->fno[sp].fname);

		if (!strcmp(name, ".") || !strcmp(name, ".."))
			continue;

		s_printf(ctx->src, "%s/%s", ctx->src_stack[sp], name);
		if (do_copy)
			s_printf(ctx->dst, "%s/%s", ctx->dst_stack[sp], name);

		if (ctx->fno[sp].fattrib & AM_DIR) {
			if (sp + 1 >= MAX_STACK_DEPTH)
				continue;

			if (do_copy)
				f_mkdir(ctx->dst);

			s_printf(ctx->src_stack[sp + 1], "%s", ctx->src);
			if (do_copy)
				s_printf(ctx->dst_stack[sp + 1], "%s", ctx->dst);

			if (f_opendir(&ctx->dir[sp + 1], ctx->src) == FR_OK)
				sp++;
			continue;
		}

		if (do_copy) {
			res = f_copy(ctx->src, ctx->dst);
			if (res != FR_OK) {
				log_printf(true, LOG_ERR, LOG_MSG_FOLDER_COPY_ERROR, ctx->src, res);
				goto out;
			}
		} else {
			debug_log_write("Delete %s\n", ctx->src);
			f_unlink(ctx->src);
		}
	}

	if (!do_copy) {
		log_printf(true, LOG_INFO, LOG_MSG_FOLDER_DELETE_END);
		f_unlink(src_root);
	} else {
		log_printf(true, LOG_INFO, LOG_MSG_FOLDER_COPY_END);
	}

out:
	// free((BYTE*)ctx->copy_buf);
	free(ctx);
	return res;
}

int save_fb_to_bmp(const char* filename)
{
	// Disallow screenshots if less than 2s passed.
	static u32 timer = 0;
	if (get_tmr_ms() < timer)
		return 1;

	const u32 file_size = 0x384000 + 0x36;
	u8 *bitmap = malloc(file_size);
	u32 *fb = malloc(0x384000);
	u32 *fb_ptr = gfx_ctxt.fb;

	// Reconstruct FB for bottom-top, portrait bmp.
	for (int y = 1279; y > -1; y--)
	{
		for (u32 x = 0; x < 720; x++)
			fb[y * 720 + x] = *fb_ptr++;
	}

	memcpy(bitmap + 0x36, fb, 0x384000);

	typedef struct _bmp_t
	{
		u16 magic;
		u32 size;
		u32 rsvd;
		u32 data_off;
		u32 hdr_size;
		u32 width;
		u32 height;
		u16 planes;
		u16 pxl_bits;
		u32 comp;
		u32 img_size;
		u32 res_h;
		u32 res_v;
		u64 rsvd2;
	} __attribute__((packed)) bmp_t;

	bmp_t *bmp = (bmp_t *)bitmap;

	bmp->magic    = 0x4D42;
	bmp->size     = file_size;
	bmp->rsvd     = 0;
	bmp->data_off = 0x36;
	bmp->hdr_size = 40;
	bmp->width    = 720;
	bmp->height   = 1280;
	bmp->planes   = 1;
	bmp->pxl_bits = 32;
	bmp->comp     = 0;
	bmp->img_size = 0x384000;
	bmp->res_h    = 2834;
	bmp->res_v    = 2834;
	bmp->rsvd2    = 0;

	sd_mount();

	// f_mkdir("sd:/LockSmith-RCM");
	// f_mkdir("sd:/LockSmith-RCM/screenshots");
	char path[256];
	s_printf(path, "sd:/LockSmith-RCM/screenshots/%s.bmp", filename);
	mkdir_recursive(path);
	int i = 0;
	if (f_stat(path, NULL) == FR_OK) {
		for (i = 1; i <= 99999; i++) {
			// int v = i;
			// char num[6];
			// num[5] = 0;
			// num[4] = '0' + (v % 10); v /= 10;
			// num[3] = '0' + (v % 10); v /= 10;
			// num[2] = '0' + (v % 10); v /= 10;
			// num[1] = '0' + (v % 10); v /= 10;
			// num[0] = '0' + (v % 10);
			// s_printf(path, "sd:/LockSmith-RCM/screenshots/%s_%s.bmp", filename, num);
			s_printf(path, "sd:/LockSmith-RCM/screenshots/%s_%d.bmp", filename, i);
			if (f_stat(path, NULL) != FR_OK) {
				break;
			}
		}
	}

	// Save screenshot and log.
	int res = -1;
	if (i <= 100000) res = sd_save_to_file(bitmap, file_size, path);

	// sd_unmount();

	free(bitmap);
	free(fb);

	// Set timer to 2s.
	timer = get_tmr_ms() + 2000;

	return res;
}

static void _reloc_append(u32 payload_dst, u32 payload_src, u32 payload_size)
{
	memcpy((u8 *)payload_src, (u8 *)IPL_LOAD_ADDR, PATCHED_RELOC_SZ);

	volatile reloc_meta_t *relocator = (reloc_meta_t *)(payload_src + RELOC_META_OFF);

	relocator->start = payload_dst - ALIGN(PATCHED_RELOC_SZ, 0x10);
	relocator->stack = PATCHED_RELOC_STACK;
	relocator->end   = payload_dst + payload_size;
	relocator->ep    = payload_dst;
}

void launch_payload(char *path, bool clear_screen)
{
	if (clear_screen)
		gfx_clear_grey(0x1B);
	gfx_con_setpos(0, 0);

	// Read payload.
	u32 size = 0;
	void *buf = sd_file_read(path, &size);
	if (!buf)
	{
		gfx_con.mute = false;
		log_printf(false, LOG_ERR, LOG_MSG_ERR_PAYLOAD_FILE_ERROR, path);

		goto out;
	}

	// Check if it safely fits IRAM.
	if (size > 0x30000)
	{
		gfx_con.mute = false;
		log_printf(false, LOG_ERR, LOG_MSG_ERR_PAYLOAD_TOO_BIG);
		goto out;
	}

	sd_end();

	// Copy the payload to our chosen address.
	memcpy((void *)RCM_PAYLOAD_ADDR, buf, size);

	// Append relocator or set config.
	void (*payload_ptr)();
	_reloc_append(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, ALIGN(size, 0x10));

	payload_ptr = (void *)EXT_PAYLOAD_ADDR;
	free((BYTE*)copy_buf);
	free((u8*)cal0_buf);
	emunand_list_free();

	hw_deinit(false);

	// Launch our payload.
	(*payload_ptr)();

out:
	free(buf);
	gfx_con.mute = false;
	log_printf(false, LOG_ERR, LOG_MSG_ERR_PAYLOAD_LAUNCH);
}

void auto_reboot() {
	sd_mount();
	// If the console is a patched or Mariko unit
	if (h_cfg.t210b01 || h_cfg.rcm_patched) {
		free((BYTE*)copy_buf);
		free((u8*)cal0_buf);
		emunand_list_free();
		power_set_state(POWER_OFF_REBOOT);
	} else {
		if (f_stat("payload.bin", NULL) == FR_OK)
			launch_payload("payload.bin", false);

		if (f_stat("bootloader/update.bin", NULL) == FR_OK)
			launch_payload("bootloader/update.bin", false);

		if (f_stat("atmosphere/reboot_payload.bin", NULL) == FR_OK)
			launch_payload("atmosphere/reboot_payload.bin", false);

		log_printf(false, LOG_ERR, LOG_MSG_ERR_PAYLOAD_LAUNCH);
	}
}

enum NcaTypes {
	Porgram = 0,
	Meta,
	Control,
	Manual,
	Data,
	PublicData
};

// Thanks switchbrew https://switchbrew.org/wiki/NCA_Format
// This function is hacky, should change it but am lazy
int GetNcaType(char *path){
	FIL fp;
	u32 read_bytes = 0;

	if (f_open(&fp, path, FA_READ | FA_OPEN_EXISTING))
		return -1;

	// u8 *dec_header = (u8*)malloc(0x400);
	u8 dec_header[0x400];

	if (f_lseek(&fp, 0x200) || f_read(&fp, dec_header, 32, &read_bytes) || read_bytes != 32){
		f_close(&fp);
		// free(dec_header);
		return -1;
	}

	se_aes_crypt_xts(7,6,0,1, dec_header + 0x200, dec_header, 32, 1);

	u8 ContentType = dec_header[0x205];
	
	f_close(&fp);
	// free(dec_header);
	return ContentType;
}

/*
static ALWAYS_INLINE u8 *_read_pkg1(const pkg1_id_t **pkg1_id) {
	// Read package1.
	u8 *pkg1 = (u8 *)malloc(PKG1_MAX_SIZE);
	if (!mount_nand_part(NULL, "BOOT0", true, true, false, false, NULL, NULL, NULL, NULL)) {
		return NULL;
	}
	if (!emummc_storage_read(PKG1_OFFSET / EMMC_BLOCKSIZE, PKG1_MAX_SIZE / EMMC_BLOCKSIZE, pkg1)) {
		unmount_nand_part(NULL, true, false, true, false);
		return NULL;
	}

	u32 pk1_offset = h_cfg.t210b01 ? sizeof(bl_hdr_t210b01_t) : 0; // Skip T210B01 OEM header.
	*pkg1_id = pkg1_identify(pkg1 + pk1_offset);
	if (!*pkg1_id) {
		//gfx_hexdump(0, pkg1 + pk1_offset, 0x20);
		char pkg1txt[16] = {0};
		memcpy(pkg1txt, pkg1 + pk1_offset + 0x10, 14);
		log_printf(true, LOG_ERR, LOG_MSG_DUMP_FW_PKG1_GET_ERROR, pkg1txt);
		unmount_nand_part(NULL, true, false, true, false);
		return NULL;
	}

	unmount_nand_part(NULL, true, false, true, false);
	return pkg1;
}
*/

void DumpFw() {
	cls();
	char sysPath[25 + 36 + 3 + 1]; // 24 for "bis:/Contents/registered", 36 for ncaName.nca, 3 for /00, and 1 to make sure :)
	int res = 0;
	char baseSdPath[256];

	log_printf(true, LOG_INFO, LOG_MSG_DUMP_FW_BEGIN);

	u32 timer = get_tmr_s();

	if (!sd_mount()) {
		log_printf(true, LOG_ERR, LOG_MSG_ERR_SD_MOUNT);
		res = 1;
		goto out;
	}

	if (!bis_loaded) {
		res = 1;
		goto out;
	}

	/*
	const pkg1_id_t *pkg1_id;
	u8 *pkg1 = _read_pkg1(&pkg1_id);
	if (!pkg1) {
		res = 1;
		goto out;
		return;
	}

	LIST_INIT(gpt);
	if (!mount_nand_part(&gpt, "SYSTEM", true, true, true, true, NULL, NULL, NULL, NULL)) {
		free(pkg1);
		save_screenshot_and_go_back("fw_dump");
		return;
	}

	log_printf(false, LOG_INFO, LOG_MSG_DUMP_FW_PKG1_INFOS, pkg1_id->id, (u8)pkg1_id->kb);
	free(pkg1);

	s_printf(baseSdPath, "sd:/LockSmith-RCM/Firmwares/%d (%s)", (u8)pkg1_id->kb, pkg1_id->id);
	*/

	u8 fw_major = 0, fw_minor = 0, fw_patch = 0;
	bool fw_detected = false;
	if (emummc_storage_init_mmc() == 0) {
		fw_detected = detect_firmware_from_nca(&fw_major, &fw_minor, &fw_patch);
		emummc_storage_end();
		sd_mount();
	}
	if (fw_detected) {
		s_printf(baseSdPath, "sd:/LockSmith-RCM/Firmwares/Firmware %d.%d.%d", fw_major, fw_minor, fw_patch);
	} else {
		s_printf(baseSdPath, "sd:/LockSmith-RCM/Firmwares/Firmware UNKNOWN");
	}

	f_mkdir("sd:/LockSmith-RCM");
	f_mkdir("sd:/LockSmith-RCM/Firmwares");

	FILINFO fno;
	if (f_stat(baseSdPath, &fno) == FR_OK) {
		log_printf(true, LOG_WARN, LOG_MSG_DUMP_FW_DIR_REPLACE_ASK);
		if (!wait_vol_plus()) {
			return;
		}
		RESETCOLOR;
		gfx_printf("Deleting... ");
		f_cp_or_rm_rf(baseSdPath, NULL);
		gfx_putc('\n');
	}

	f_mkdir(baseSdPath);

	gfx_printf("Out: %s\nReading entries...\n", baseSdPath);

	LIST_INIT(gpt);
	if (!mount_nand_part(&gpt, "SYSTEM", true, true, true, true, NULL, NULL, NULL, NULL)) {
		save_screenshot_and_go_back("fw_dump");
		return;
	}
	int readRes = 0;
	DIR  dir;
	const char bis_fw_dir_path[] = "bis:/Contents/registered";
	readRes = f_opendir(&dir, bis_fw_dir_path);
	if (readRes){
		log_printf(true, LOG_ERR, LOG_MSG_ERR_OPEN_FOLDER, bis_fw_dir_path);
		unmount_nand_part(&gpt, false, true, true, true);
		save_screenshot_and_go_back("fw_dump");
		return;
	}

	gfx_printf("Starting dump...\n");
	SETCOLOR(COLOR_GREEN, COLOR_DEFAULT);

	int total = 1;
	ui_pos_t con_pos;
	gfx_con_getpos(&con_pos.x, &con_pos.y);
	/*
	BYTE *copy_buf = (BYTE*)malloc(COPY_BUF_SIZE);
	if (!copy_buf) {
		log_printf(true, LOG_ERR, LOG_MSG_MALLOC_ERROR);
		unmount_nand_part(&gpt, false, true, true, true);
		save_screenshot_and_go_back("fw_dump");
		return;
	}
	*/
	while(true) {
		readRes = f_readdir(&dir, &fno);
		if (readRes != FR_OK) {
			log_printf(true, LOG_ERR, LOG_MSG_ERR_FOLDER_READ);
			res = 1;
			break;
		}

		if (fno.fname[0] == '\0') {
			break;
		}

		if (!strcmp(fno.fname, ".") || !strcmp(fno.fname, "..") || fno.fattrib & AM_DIR)
			continue;

		// s_printf(sysPath, (fno.fattrib & AM_DIR) ? "%s/%s/00" : "%s/%s", "bis:/Contents/registered", fno.fname);
		s_printf(sysPath, "%s/%s", bis_fw_dir_path, fno.fname);
		int contentType = GetNcaType(sysPath);

		if (contentType < 0){
			res = 1;
			break;
		}

		char sdPath[256];
		s_printf(sdPath, "%s/%s", baseSdPath, fno.fname);
		if (contentType == Meta)
			memcpy(sdPath + strlen(sdPath) - 4, ".cnmt.nca", 10);
		
		gfx_con_setpos(con_pos.x, con_pos.y);
		gfx_printf("[%3d] %s\n", total, fno.fname);
		total++;
		int err = f_copy(sysPath, sdPath);
		if (err) {
			log_printf(true, LOG_ERR, LOG_MSG_ERR_FILE_COPY);
			res = 1;
			break;
		}
	}
	f_closedir(&dir);
	RESETCOLOR;
	unmount_nand_part(&gpt, false, true, true, true);
	// free((BYTE*)copy_buf);

out:
	if (res) {
		gfx_printf("\n");
		log_printf(true, LOG_ERR, LOG_MSG_DUMP_FW_ERROR);
		gfx_printf("\n");
	} else {
		gfx_printf("\n\n");
		log_printf(true, LOG_OK, LOG_MSG_DUMP_FW_END, get_tmr_s() - timer);
		gfx_printf("\n");
	}
	save_screenshot_and_go_back("fw_dump");
}