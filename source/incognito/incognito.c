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
#include "../prodinfogen/crc16.h"

#define RETRY_COUNT 5
#define RETRY(exp)                                                                              \
	({                                                                                          \
		u8 _attemptc_ = RETRY_COUNT;                                                            \
		bool _resultb_ = false;                                                                 \
		while (_attemptc_--)                                                                    \
		{                                                                                       \
			if ((_resultb_ = exp))                                                              \
				break;                                                                          \
			gfx_printf("%kretry %d/%d...\n", COLOR_RED, RETRY_COUNT - _attemptc_, RETRY_COUNT); \
		}                                                                                       \
		_resultb_;                                                                              \
	})

#define SECTORS_IN_CLUSTER 32

#define BACKUP_NAME_EMUNAND "sd:/prodinfo_emunand.bin"
#define BACKUP_NAME_SYSNAND "sd:/prodinfo_sysnand.bin"

u16 calculateCrc(u32 offset, u32 size, u8 *blob)
{
	unsigned char buffer[size + 1];
	if (blob == NULL)
		readData((u8 *)buffer, offset, size, NULL);
	else
		memcpy((u8 *)buffer, blob + offset, size);

	return get_crc_16(buffer, size);
}

u16 readCrc(u32 offset, u8 *blob)
{
	u16 buffer;
	if (blob == NULL)
		readData((u8 *)&buffer, offset, sizeof(u16), NULL);
	else
		memcpy((u8 *)&buffer, blob + offset, sizeof(u16));
	
	return buffer;
}

bool validateCrc(u32 offset, u32 size, u8 *blob)
{
	return calculateCrc(offset, size, blob) == readCrc(offset + size, blob);
}

bool calculateAndWriteCrc(u32 offset, u32 size)
{
	u16 crcValue = calculateCrc(offset, size, NULL);
	u8 crc[2] = { crcValue & 0xff, crcValue >> 8 }; // bytes of u16
	return writeData(crc, offset + size, sizeof(u16), NULL);
}

void validateChecksums(u8 *blob)
{
	if (!validateCrc(0x0250, 0x1E, blob))
		gfx_printf("%kWarning - invalid serial crc\n", COLOR_RED);

	if (!validateCrc(0x0480, 0x18E, blob))
		gfx_printf("%kWarning - invalid ECC-B233 crc...\n", COLOR_RED);

	if (!validateCrc(0x3AE0, 0x13E, blob))
		gfx_printf("%kWarning - invalid ext SSL key crc...\n", COLOR_RED);

	if (!validateCrc(0x35A0, 0x07E, blob))
		gfx_printf("%kWarning - invalid ECDSA cert crc...\n", COLOR_RED);

	if (!validateCrc(0x36A0, 0x09E, blob))
		gfx_printf("%kWarning - invalid ECQV-BLS cert crc...\n", COLOR_RED);
}

bool erase(u32 offset, u32 length)
{
	u8 *tmp = (u8 *)calloc(length, sizeof(u8));
	bool result = writeData(tmp, offset, length, NULL);
	free(tmp);
	return result;
}

bool writeSerial()
{
	const char *junkSerial;
	if (menu_on_sysnand)
	{
		junkSerial = "XAW00000000000";
	}
	else
	{
		junkSerial = "XAW00000000001";
	}

	const u32 serialOffset = 0x250;
	if (!writeData((u8 *)junkSerial, serialOffset, 14, NULL))
		return false;

	return calculateAndWriteCrc(serialOffset, 0x1E);
}

bool incognito()
{
	LIST_INIT(gpt);
	if (!mount_nand_part(&gpt, "PRODINFO", true, true, false, true, NULL, NULL, NULL, NULL)) {
		return false;
	}
	/*
	void* cal0_buf;
	if (!cal0_read(KS_BIS_00_TWEAK, KS_BIS_00_CRYPT, cal0_buf)) {
		unmount_nand_part(&gpt, false, true, true, false);
		return false;
	}
	nx_emmc_cal0_t *cal0 = (nx_emmc_cal0_t *)cal0_buf;
	*/

	/*
	gfx_printf("%kChecking if backup exists...\n", COLOR_YELLOW);
	if (!checkBackupExists())
	{
		gfx_printf("%kI'm sorry Dave, I'm afraid I can't do that..\n%kWill make a backup first...\n", COLOR_RED, COLOR_YELLOW);
		if (!backupProdinfo()) {
			unmount_nand_part(&gpt, false, true, true, false);
			return false;
		}
	}
	*/

	validateChecksums(NULL);

	gfx_printf("%kWriting fake serial...\n", COLOR_YELLOW);
	if (!writeSerial()) {
		unmount_nand_part(&gpt, false, true, true, false);
		return false;
	}
/*
	gfx_printf("%kErasing ECC-B233 device cert...\n", COLOR_YELLOW);
	if (!erase(0x0480, 0x180)) {
		unmount_nand_part(&gpt, false, true, true, false);
		return false;
	}

	if (!calculateAndWriteCrc(0x0480, 0x18E)) { // whatever I do here, it crashes Atmos..?
		unmount_nand_part(&gpt, false, true, true, false);
		return false;
	}
*/
	gfx_printf("%kErasing SSL cert...\n", COLOR_YELLOW);
	if (!erase(0x0AE0, 0x800)) {
		unmount_nand_part(&gpt, false, true, true, false);
		return false;
	}

	gfx_printf("%kErasing extended SSL key...\n", COLOR_YELLOW);
	if (!erase(0x3AE0, 0x130)) {
		unmount_nand_part(&gpt, false, true, true, false);
		return false;
	}

	gfx_printf("%kWriting checksum...\n", COLOR_YELLOW);
	if (!calculateAndWriteCrc(0x3AE0, 0x13E)) {
		unmount_nand_part(&gpt, false, true, true, false);
		return false;
	}

	gfx_printf("%kErasing Amiibo ECDSA cert...\n", COLOR_YELLOW);
	if (!erase(0x35A0, 0x070)) {
		unmount_nand_part(&gpt, false, true, true, false);
		return false;
	}

	gfx_printf("%kWriting checksum...\n", COLOR_YELLOW);
	if (!calculateAndWriteCrc(0x35A0, 0x07E)) {
		unmount_nand_part(&gpt, false, true, true, false);
		return false;
	}

	gfx_printf("%kErasing Amiibo ECQV-BLS root cert...\n", COLOR_YELLOW);
	if (!erase(0x36A0, 0x090)) {
		unmount_nand_part(&gpt, false, true, true, false);
		return false;
	}

	gfx_printf("%kWriting checksum...\n", COLOR_YELLOW);
	if (!calculateAndWriteCrc(0x36A0, 0x09E)) {
		unmount_nand_part(&gpt, false, true, true, false);
		return false;
	}

	gfx_printf("%kErasing RSA-2048 extended device key...\n", COLOR_YELLOW);
	if (!erase(0x3D70, 0x240)) { // seems empty & unused!
		unmount_nand_part(&gpt, false, true, true, false);
		return false;
	}

	gfx_printf("%kErasing RSA-2048 device certificate...\n", COLOR_YELLOW);
	if (!erase(0x3FC0, 0x240)) { // seems empty & unused!
		unmount_nand_part(&gpt, false, true, true, false);
		return false;
	}

	gfx_printf("%kWriting SSL cert hash...\n", COLOR_YELLOW);
	if (!writeClientCertHash()) {
		unmount_nand_part(&gpt, false, true, true, false);
		return false;
	}

	gfx_printf("%kWriting body hash...\n", COLOR_YELLOW);
	if (!writeCal0Hash()) {
		unmount_nand_part(&gpt, false, true, true, false);
		return false;
	}

	gfx_printf("\n%kIncognito done!\n", COLOR_GREEN);
	unmount_nand_part(&gpt, false, true, true, false);
	return true;
}

u32 divideCeil(u32 x, u32 y)
{
	return 1 + ((x - 1) / y);
}

static inline u32 _read_le_u32(const void *buffer, u32 offset)
{
	return (*(u8 *)(buffer + offset + 0)) |
		   (*(u8 *)(buffer + offset + 1) << 0x08) |
		   (*(u8 *)(buffer + offset + 2) << 0x10) |
		   (*(u8 *)(buffer + offset + 3) << 0x18);
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

	u32 sectorCount = divideCeil(newOffset + length, EMMC_BLOCKSIZE);

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
		gfx_printf("%kERROR, ERRO! Length is %d!\n", COLOR_RED, length);
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

bool writeHash(u32 hashOffset, u32 offset, u32 sz)
{
	bool result = false;
	u8 *buffer = (u8 *)malloc(sz);
	if (!readData(buffer, offset, sz, NULL))
	{
		goto out;
	}
	u8 hash[0x20];
	se_calc_sha256_oneshot(hash, buffer, sz);

	if (!writeData(hash, hashOffset, 0x20, NULL))
	{
		goto out;
	}
	result = true;
out:
	free(buffer);
	return result;
}

bool verifyHash(u32 hashOffset, u32 offset, u32 sz, u8 *blob)
{
	bool result = false;
	u8 *buffer = (u8 *)malloc(sz);
	if (blob == NULL)
	{
		if (!readData(buffer, offset, sz, NULL))
			goto out;
	}
	else
	{
		memcpy(buffer, blob + offset, sz);
	}
	u8 hash1[0x20];
	se_calc_sha256_oneshot(hash1, buffer, sz);

	u8 hash2[0x20];

	if (blob == NULL)
	{
		if (!readData(hash2, hashOffset, 0x20, NULL))
			goto out;
	}
	else
	{
		memcpy(hash2, blob + hashOffset, 0x20);
	}

	if (memcmp(hash1, hash2, 0x20) != 0)
	{
		EPRINTF("error: hash verification failed\n");
		// gfx_hexdump(0, hash1, 0x20);
		// gfx_hexdump(0, hash2, 0x20);
		goto out;
	}

	result = true;
out:
	free(buffer);
	return result;
}

s32 getClientCertSize(u8 *blob)
{
	s32 buffer;
	if (blob == NULL)
	{
		if (!RETRY(readData((u8 *)&buffer, 0x0AD0, sizeof(buffer), NULL)))
		{
			return -1;
		}
	}
	else
	{
		memcpy(&buffer, blob + 0x0AD0, sizeof(buffer));
	}
	return buffer;
}

s32 getCalibrationDataSize(u8 *blob)
{
	s32 buffer;
	if (blob == NULL)
	{
		if (!RETRY(readData((u8 *)&buffer, 0x08, sizeof(buffer), NULL)))
		{
			return -1;
		}
	}
	else
	{
		memcpy(&buffer, blob + 0x08, sizeof(buffer));
	}

	return buffer;
}

bool writeCal0Hash()
{
	s32 calibrationSize = getCalibrationDataSize(NULL);
	if (calibrationSize == -1)
		return false;

	return writeHash(0x20, 0x40, calibrationSize);
}

bool writeClientCertHash()
{
	s32 certSize = getClientCertSize(NULL);
	if (certSize == -1)
		return false;

	return writeHash(0x12E0, 0xAE0, certSize);
}

bool verifyCal0Hash(u8 *blob)
{
	s32 calibrationSize = getCalibrationDataSize(blob);
	if (calibrationSize == -1)
		return false;

	return verifyHash(0x20, 0x40, calibrationSize, blob);
}

bool verifyClientCertHash(u8 *blob)
{
	s32 certSize = getClientCertSize(blob);
	if (certSize == -1)
		return false;

	return verifyHash(0x12E0, 0xAE0, certSize, blob);
}

bool verifyProdinfo(u8 *blob)
{
	gfx_printf("%kVerifying client cert hash and CAL0 hash%s...\n", COLOR_YELLOW, blob != NULL ? "\nfrom backup" : "");

	if (verifyClientCertHash(blob) && verifyCal0Hash(blob))
	{
		validateChecksums(blob);

		char serial[15] = "";
		if (blob == NULL)
		{
			readData((u8 *)serial, 0x250, 14, NULL);
		}
		else
		{
			memcpy(serial, blob + 0x250, 14);
		}

		gfx_printf("%kVerification successful!\n%kSerial:%s\n", COLOR_GREEN, COLOR_BLUE, serial);
		return true;
	}
	gfx_printf("%kVerification not successful!\n", COLOR_RED);
	return false;
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
