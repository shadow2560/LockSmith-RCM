/*
 * Copyright (c) 2019 shchmue
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "incognito.h"

#include "../config.h"
#include <gfx_utils.h>
#include "../hos/hos.h"
#include <libs/fatfs/ff.h>
#include <mem/heap.h>
#include <mem/mc.h>
#include <mem/sdram.h>
#include <sec/se.h>
#include <sec/se_t210.h>
#include <sec/tsec.h>
#include <soc/fuse.h>
#include <mem/smmu.h>
#include <soc/t210.h>
#include "../storage/emummc.h"
#include <storage/emmc.h>
#include "../storage/nx_emmc_bis.h"
#include <storage/sdmmc.h>
#include <storage/sd.h>
#include <utils/btn.h>
#include <utils/list.h>
#include <utils/sprintf.h>
#include <utils/util.h>

#include "../keys/key_sources.inl"
#include "../keys/cal0_read.h"
#include "../keys/crypto.h"

#include <string.h>

#include "../tools.h"
#include "../gfx/messages.h"
#include "../prodinfo_rewrite/prodinfo_rewrite.h"

#define RETRY_COUNT 5
#define RETRY(exp) \
	({ \
		u8 _attemptc_ = RETRY_COUNT; \
		bool _resultb_ = false; \
		while (_attemptc_--) \
		{ \
			if ((_resultb_ = exp)) \
				break; \
			log_printf(false, LOG_ERR, LOG_MSG_INCOGNITO_RETRY_DEF, RETRY_COUNT - _attemptc_, RETRY_COUNT); \
		} \
		_resultb_; \
	})

#define SECTORS_IN_CLUSTER 32

#define BACKUP_NAME_EMUNAND "sd:/prodinfo_emunand.bin"
#define BACKUP_NAME_SYSNAND "sd:/prodinfo_sysnand.bin"

static inline u32 _read_le_u32(const void *buffer, u32 offset)
{
	return (*(u8 *)(buffer + offset + 0)) |
		   (*(u8 *)(buffer + offset + 1) << 0x08) |
		   (*(u8 *)(buffer + offset + 2) << 0x10) |
		   (*(u8 *)(buffer + offset + 3) << 0x18);
}

bool incognito()
{
	LIST_INIT(gpt);
	if (!mount_nand_part(&gpt, "PRODINFO", true, true, false, true, NULL, NULL, NULL, NULL))
		return false;

	bool ok = false;

	/* Read full PRODINFO partition into memory, apply mutations on DATA fields only,
	 * then rewrite all CRC/SHA fields using the shared table, verify, and finally write back once.
	 */
	/*
	u8 *cal0_buf = (u8 *)malloc(NX_EMMC_CALIBRATION_SIZE);
	if (!cal0_buf)
		goto out;
	*/

	// if (!readData(cal0_buf, 0, NX_EMMC_CALIBRATION_SIZE, NULL))
		if (!cal0_read(KS_BIS_00_TWEAK, KS_BIS_00_CRYPT, cal0_buf, NULL))
		goto out;

	/* Optional pre-check: log warnings if input already has checksum issues */
	verifyProdinfo(cal0_buf);

	/* 1) Fake serial (DATA field) */
	const char *junkSerial = menu_on_sysnand ? "XAW00000000000" : "XAW00000000001";
	prodinfo_write_data(cal0_buf, NX_EMMC_CALIBRATION_SIZE, PI_F_SerialNumber, junkSerial, 14);

	/* 2) Erase SSL certificate (DATA field) */
	prodinfo_zero_data(cal0_buf, NX_EMMC_CALIBRATION_SIZE, PI_F_SslCertificate);

	/* 3) Erase extended SSL key (DATA field) */
	prodinfo_zero_data(cal0_buf, NX_EMMC_CALIBRATION_SIZE, PI_F_ExtendedSslKey);

	/* 4) Erase Amiibo ECDSA certificate (DATA field) */
	prodinfo_zero_data(cal0_buf, NX_EMMC_CALIBRATION_SIZE, PI_F_AmiiboEcdsaCertificate);

	/* 5) Erase Amiibo ECQV-BLS root certificate (DATA field) */
	prodinfo_zero_data(cal0_buf, NX_EMMC_CALIBRATION_SIZE, PI_F_AmiiboEcqvBlsRootCertificate);

	/* Rewrite all CRC/SHA fields consistently */
	if (prodinfo_verify_or_rewrite_hashes(cal0_buf, NX_EMMC_CALIBRATION_SIZE, NULL, cal0_buf, NX_EMMC_CALIBRATION_SIZE) != PI_OK)
	// if (prodinfo_rewrite_hashes(cal0_buf, NX_EMMC_CALIBRATION_SIZE, cal0_buf, NX_EMMC_CALIBRATION_SIZE) != PI_OK)
		goto out;

	/* Verify post-rewrite */
	if (!verifyProdinfo(cal0_buf))
		goto out;

	/* Write back once */
	if (!writeData(cal0_buf, 0, NX_EMMC_CALIBRATION_SIZE, NULL))
		goto out;

	ok = true;

out:
	/*
	if (cal0_buf)
		free(pi);
	*/

	if (ok)
	{
		log_printf(true, LOG_OK, LOG_MSG_INCOGNITO_SUCCESS);
		unmount_nand_part(&gpt, false, true, true, false);
		save_screenshot_and_go_back("incognito");
		return true;
	}

	log_printf(true, LOG_ERR, LOG_MSG_INCOGNITO_ERR);
	unmount_nand_part(&gpt, false, true, true, false);
	save_screenshot_and_go_back("incognito");
	return false;
}

bool readData(u8 *buffer, u32 offset, u32 length, void (*progress_callback)(u32, u32))
{
	if (progress_callback != NULL)
	{
		(*progress_callback)(0, length);
	}
	bool result = false;
	u32 sector = (offset / EMMC_BLOCKSIZE);
	u32 newOffset = (offset % EMMC_BLOCKSIZE);

	u32 sectorCount = (newOffset + length + (EMMC_BLOCKSIZE - 1)) / EMMC_BLOCKSIZE;

	u8 *tmp = (u8 *)malloc(sectorCount * EMMC_BLOCKSIZE);

	u32 clusterOffset = sector % SECTORS_IN_CLUSTER;
	u32 sectorOffset = 0;
	while (clusterOffset + sectorCount > SECTORS_IN_CLUSTER)
	{
		u32 sectorsToRead = SECTORS_IN_CLUSTER - clusterOffset;
		if (!RETRY(nx_emmc_bis_read(sector, sectorsToRead, tmp + (sectorOffset * EMMC_BLOCKSIZE))))
			goto out;

		sector += sectorsToRead;
		sectorCount -= sectorsToRead;
		clusterOffset = 0;
		sectorOffset += sectorsToRead;
		if (progress_callback != NULL)
		{
			(*progress_callback)(sectorOffset * EMMC_BLOCKSIZE, length);
		}
	}
	if (sectorCount == 0)
		goto done;

	if (!RETRY(nx_emmc_bis_read(sector, sectorCount, tmp + (sectorOffset * EMMC_BLOCKSIZE))))
		goto out;

	memcpy(buffer, tmp + newOffset, length);
done:
	result = true;
	if (progress_callback != NULL)
	{
		(*progress_callback)(length, length);
	}
out:
	free(tmp);
	return result;
}

bool writeData(u8 *buffer, u32 offset, u32 length, void (*progress_callback)(u32, u32))
{
	if (progress_callback != NULL)
	{
		(*progress_callback)(0, length);
	}
	bool result = false;

	u32 initialLength = length;

	u8 *tmp_sec = (u8 *)malloc(EMMC_BLOCKSIZE);
	u8 *tmp = NULL;

	u32 sector = (offset / EMMC_BLOCKSIZE);
	u32 newOffset = (offset % EMMC_BLOCKSIZE);

	// if there is a sector offset, read involved sector, write data to it with offset and write back whole sector to be sector aligned
	if (newOffset > 0)
	{
		u32 bytesToRead = EMMC_BLOCKSIZE - newOffset;
		u32 bytesToWrite;
		if (length >= bytesToRead)
		{
			bytesToWrite = bytesToRead;
		}
		else
		{
			bytesToWrite = length;
		}
		if (!RETRY(nx_emmc_bis_read(sector, 1, tmp_sec)))
			goto out;

		memcpy(tmp_sec + newOffset, buffer, bytesToWrite);
		if (!RETRY(nx_emmc_bis_write(sector, 1, tmp_sec)))
			goto out;

		sector++;
		length -= bytesToWrite;
		newOffset = bytesToWrite;

		if (progress_callback != NULL)
		{
			(*progress_callback)(initialLength - length, initialLength);
		}
		// are we done?
		if (length == 0)
			goto done;
	}

	// write whole sectors in chunks while being cluster aligned
	u32 sectorCount = length / EMMC_BLOCKSIZE;
	tmp = (u8 *)malloc(sectorCount * EMMC_BLOCKSIZE);

	u32 clusterOffset = sector % SECTORS_IN_CLUSTER;
	u32 sectorOffset = 0;
	while (clusterOffset + sectorCount >= SECTORS_IN_CLUSTER)
	{
		u32 sectorsToRead = SECTORS_IN_CLUSTER - clusterOffset;
		if (!RETRY(nx_emmc_bis_write(sector, sectorsToRead, buffer + newOffset + (sectorOffset * EMMC_BLOCKSIZE))))
			goto out;

		sector += sectorsToRead;
		sectorOffset += sectorsToRead;
		sectorCount -= sectorsToRead;
		clusterOffset = 0;
		length -= sectorsToRead * EMMC_BLOCKSIZE;

		if (progress_callback != NULL)
		{
			(*progress_callback)(initialLength - length, initialLength);
		}
	}

	// write remaining sectors
	if (sectorCount > 0)
	{
		if (!RETRY(nx_emmc_bis_write(sector, sectorCount, buffer + newOffset + (sectorOffset * EMMC_BLOCKSIZE))))
			goto out;

		length -= sectorCount * EMMC_BLOCKSIZE;
		sector += sectorCount;
		sectorOffset += sectorCount;

		if (progress_callback != NULL)
		{
			(*progress_callback)(initialLength - length, initialLength);
		}
	}

	// if there is data remaining that is smaller than a sector, read that sector, write remaining data to it and write back whole sector
	if (length == 0)
		goto done;

	if (length > EMMC_BLOCKSIZE)
	{
		log_printf(true, LOG_ERR, LOG_MSG_INCOGNITO_ERR_WRITE, length);
		goto out;
	}

	if (!RETRY(nx_emmc_bis_read(sector, 1, tmp_sec)))
		goto out;

	memcpy(tmp_sec, buffer + newOffset + (sectorOffset * EMMC_BLOCKSIZE), length);
	if (!RETRY(nx_emmc_bis_write(sector, 1, tmp_sec)))
		goto out;

done:
	result = true;
	if (progress_callback != NULL)
	{
		(*progress_callback)(initialLength, initialLength);
	}
out:
	free(tmp_sec);
	free(tmp);
	return result;
}

bool checkBackupExists()
{
	char *name;
	if (menu_on_sysnand)
	{
		name = BACKUP_NAME_SYSNAND;
	}
	else
	{
		name = BACKUP_NAME_EMUNAND;
	}
	return f_stat(name, NULL) == FR_OK;
}

bool backupProdinfo()
{
	bool result = false;
	char *name;
	if (menu_on_sysnand)
	{
		name = BACKUP_NAME_SYSNAND;
	}
	else
	{
		name = BACKUP_NAME_EMUNAND;
	}

	gfx_printf("%kBacking up %s...\n", COLOR_YELLOW, name);
	if (checkBackupExists())
	{
		gfx_printf("%kBackup already exists!\nWill rename old backup.\n", COLOR_ORANGE);
		u32 filenameSuffix = 0;
		char newName[255];
		do
		{
			s_printf(newName, "%s.%d", name, filenameSuffix);
			filenameSuffix++;
		} while (f_stat(newName, NULL) == FR_OK);
		f_rename(name, newName);
		gfx_printf("%kOld backup renamed to:\n%s\n", COLOR_YELLOW, newName);
	}

	FIL fp;
	if (f_open(&fp, name, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
	{
		gfx_printf("\n%kCannot write to %s!\n", COLOR_RED, name);
		return false;
	}

	u8 *bufferNX = (u8 *)malloc(NX_EMMC_CALIBRATION_SIZE);
	gfx_printf("%kReading from NAND...\n", COLOR_YELLOW);
	if (!readData(bufferNX, 0, NX_EMMC_CALIBRATION_SIZE, NULL))
	{
		gfx_printf("\n%kError reading from NAND!\n", COLOR_RED);
		goto out;
	}
	gfx_putc('\n');
	if (!verifyProdinfo(bufferNX))
	{
		goto out;
	}
	gfx_printf("%k\nWriting to file...\n", COLOR_YELLOW);
	u32 bytesWritten;
	if (f_write(&fp, bufferNX, NX_EMMC_CALIBRATION_SIZE, &bytesWritten) != FR_OK || bytesWritten != NX_EMMC_CALIBRATION_SIZE)
	{
		gfx_printf("\n%kError writing to file!\nPlease try again. If this doesn't work, you don't have a working backup!\n", COLOR_RED);
		goto out;
	}
	f_sync(&fp);

	result = true;
	gfx_printf("\n%kBackup to %s done!\n", COLOR_GREEN, name);

out:
	f_close(&fp);
	free(bufferNX);

	return result;
}

bool restoreProdinfo()
{
	bool result = false;
	sd_mount();

	const char *name;
	if (menu_on_sysnand)
	{
		name = BACKUP_NAME_SYSNAND;
	}
	else
	{
		name = BACKUP_NAME_EMUNAND;
	}

	gfx_printf("%kRestoring from %s...\n", COLOR_YELLOW, name);

	FIL fp;
	if (f_open(&fp, name, FA_READ) != FR_OK)
	{
		gfx_printf("\n%kCannot open %s!\n", COLOR_RED, name);
		return false;
	}

	u8 *bufferNX = (u8 *)malloc(NX_EMMC_CALIBRATION_SIZE);
	u32 bytesRead;
	gfx_printf("%kReading from file...\n", COLOR_YELLOW);
	if (f_read(&fp, bufferNX, NX_EMMC_CALIBRATION_SIZE, &bytesRead) != FR_OK || bytesRead != NX_EMMC_CALIBRATION_SIZE)
	{
		gfx_printf("\n%kError reading from file!\n", COLOR_RED);
		goto out;
	}
	if (!verifyProdinfo(bufferNX))
	{
		goto out;
	}
	gfx_printf("%kWriting to NAND...\n", COLOR_YELLOW);
	if (!writeData(bufferNX, 0, NX_EMMC_CALIBRATION_SIZE, NULL))
	{
		gfx_printf("\n%kError writing to NAND!\nThis is bad. Try again, because your switch probably won't boot.\n"
				   "If you see this error again, you should restore via NAND backup in hekate.\n",
				   COLOR_RED);
		goto out;
	}

	result = true;
	gfx_printf("\n%kRestore from %s done!\n\n", COLOR_GREEN, name);
out:
	f_close(&fp);
	free(bufferNX);

	return result;
}
