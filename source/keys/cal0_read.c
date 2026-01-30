/*
 * Copyright (c) 2022 shchmue
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

#include "cal0_read.h"

#include <gfx_utils.h>
#include <mem/heap.h>
#include <sec/se.h>
#include <sec/se_t210.h>
#include "../storage/emummc.h"
#include <storage/emmc.h>
#include <storage/sd.h>
#include <utils/util.h>

#include "../tools.h"
#include "../gfx/messages.h"
#include "../prodinfo_rewrite/prodinfo_rewrite.h"

static const u16 crc16_table16[16] = {
	0x0000, 0xCC01, 0xD801, 0x1400,
	0xF001, 0x3C00, 0x2800, 0xE401,
	0xA001, 0x6C00, 0x7800, 0xB401,
	0x5000, 0x9C01, 0x8801, 0x4400
};

/* ----- CRC16 (CAL0) implementation (ported from switchbrew/wiki/Calibration) ----- */
/* Core: continue CRC starting from `crc` */
u16 crc16_calc_continue(u16 crc, const u8 *buf, u32 len)
{
	const u8 *p = buf;
	const u8 *q = buf + len;

	for (; p < q; p++) {
		u8 oct = *p;
		crc = (crc >> 4) ^ crc16_table16[crc & 0xF] ^ crc16_table16[(oct >> 0) & 0xF];
		crc = (crc >> 4) ^ crc16_table16[crc & 0xF] ^ crc16_table16[(oct >> 4) & 0xF];
	}
	return crc;
}

u16 crc16_calc(const u8 *buf, u32 len)
{
	return crc16_calc_continue(0x55AA, buf, len);
}

/*
u16 crc16_calc(const u8 *buf, u32 len)
{
	const u8 *p, *q;
	u16 crc = 0x55aa;

	static u16 table[16] = {
		0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
		0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400
	};

	q = buf + len;
	for (p = buf; p < q; p++)
	{
		u8 oct = *p;
		crc = (crc >> 4) ^ table[crc & 0xf] ^ table[(oct >> 0) & 0xf];
		crc = (crc >> 4) ^ table[crc & 0xf] ^ table[(oct >> 4) & 0xf];
	}

	return crc;
}
*/

static int se_aes_crypt_xts_nx(u32 tweak_ks, u32 crypt_ks, int enc, u64 sec, void *dst, void *src, u32 secsize, u32 num_secs) {
	u8 tweak[SE_AES_BLOCK_SIZE] __attribute__((aligned(4)));

	u8 *pdst = (u8 *)dst;
	u8 *psrc = (u8 *)src;

	for (u32 i = 0; i < num_secs; i++) {
		if (!se_aes_crypt_xts_sec_nx(tweak_ks, crypt_ks, enc, sec + i, tweak, true, 0, pdst + secsize * i, psrc + secsize * i, secsize)) {
			return 0;
		}
	}

	return 1;
}

bool cal0_read(u32 tweak_ks, u32 crypt_ks, void *read_buffer, const char* sd_path) {
	// nx_emmc_cal0_t *cal0 = (nx_emmc_cal0_t *)read_buffer;

	// Check if CAL0 was already read into this buffer
	// if (cal0->magic != MAGIC_CAL0) {
	if (rd_u32_le(read_buffer + pi_off(PI_F_MagicNumber)) == MAGIC_CAL0) {
		return true;
	}

	u32 sector = NX_EMMC_CALIBRATION_OFFSET / EMMC_BLOCKSIZE;

	if (sd_path == NULL) {
		if (!emummc_storage_read(sector, NX_EMMC_CALIBRATION_SIZE / EMMC_BLOCKSIZE, read_buffer)) {
			log_printf(true, LOG_ERR, LOG_MSG_ERROR_PRODINFO_READ);
			return false;
		}
	} else {
		u32 temp_size = 0;
		void *tmp = sd_file_read(sd_path, &temp_size);
// gfx_printf("cal0_read: read_buffer=%p cal0=%p sd_path=%s", read_buffer, cal0, sd_path ? sd_path : "(null)");
		if (!tmp || temp_size < NX_EMMC_CALIBRATION_SIZE) {
			if (tmp) free(tmp);
			log_printf(true, LOG_ERR, LOG_MSG_ERROR_PRODINFO_READ);
			return false;
		}
		memcpy(read_buffer, tmp, NX_EMMC_CALIBRATION_SIZE);
		free(tmp);
	}

	// if ((sd_path == NULL) || (sd_path != NULL && cal0->magic != MAGIC_CAL0)) {
	if ((sd_path == NULL) || (sd_path != NULL && rd_u32_le(read_buffer + pi_off(PI_F_MagicNumber)) != MAGIC_CAL0)) {
		// u8 *b = (u8*)read_buffer;
// gfx_printf("file head: %02X %02X %02X %02X", b[0], b[1], b[2], b[3]);
		se_aes_crypt_xts_nx(tweak_ks, crypt_ks, DECRYPT, 0, read_buffer, read_buffer, XTS_CLUSTER_SIZE, NX_EMMC_CALIBRATION_SIZE / XTS_CLUSTER_SIZE);
		// gfx_printf("file head: %02X %02X %02X %02X", b[0], b[1], b[2], b[3]);
	}

// gfx_printf("cal0_read: read_buffer=%p cal0=%p sd_path=%s", read_buffer, cal0, sd_path ? sd_path : "(null)");

	// if (cal0->magic != MAGIC_CAL0) {
		if (rd_u32_le(read_buffer + pi_off(PI_F_MagicNumber)) != MAGIC_CAL0) {
		log_printf(true, LOG_ERR, LOG_MSG_ERROR_PRODINFO_MAGIC_READ);
		return false;
	}

	return true;
}

static inline u16 swap16(u16 v)
{
	return (v >> 8) | (v << 8);
}

static bool cal0_check_crc16_field(const u8 *cal0, u32 cal0_size, int data_field_id, int crc_field_id, u32 *out_generation) {
	const u32 data_off = pi_off(data_field_id);
	const u32 data_len = pi_len(data_field_id);

	const u32 crc_off  = pi_off(crc_field_id);
	const u32 crc_len  = pi_len(crc_field_id);

	if (!cal0 || data_len == 0 || crc_len < 0x10)
		return false;

	if (data_off + data_len > cal0_size) return false;
	if (crc_off  + 0x10    > cal0_size) return false;

	const u8 *padding = &cal0[crc_off];      // 0x0E first octets
	const u8 *crc_stored_le = &cal0[crc_off + 0x0E];

	if (out_generation) {
		*out_generation = rd_u32_le(padding);
	}

	// CRC16 on (data||padding[0x0E])
	const u32 pad_len = 0x0E;
	u8 tmp[data_len + pad_len];

	memcpy(tmp, &cal0[data_off], data_len);
	memcpy(tmp + data_len, padding, pad_len);

	const u16 crc_calc = crc16_calc(tmp, data_len + pad_len);
	const u16 crc_stored = rd_u16_le(crc_stored_le);

	return (crc_calc == crc_stored);
}

/*
bool cal0_get_ssl_rsa_key(const nx_emmc_cal0_t *cal0, const void **out_key, u32 *out_key_size, const void **out_iv, u32 *out_generation) {
	const u32 ext_key_size = sizeof(cal0->ext_ssl_key_iv) + sizeof(cal0->ext_ssl_key);
	const u32 ext_key_crc_size = ext_key_size + sizeof(cal0->ext_ssl_key_ver) + sizeof(cal0->crc16_pad39);
	const u32 key_size = sizeof(cal0->ssl_key_iv) + sizeof(cal0->ssl_key);
	const u32 key_crc_size = key_size + sizeof(cal0->crc16_pad18);

	if (cal0->ext_ssl_key_crc == crc16_calc(cal0->ext_ssl_key_iv, ext_key_crc_size)) {
		*out_key = cal0->ext_ssl_key;
		*out_key_size = ext_key_size;
		*out_iv = cal0->ext_ssl_key_iv;
		// Settings sysmodule manually zeroes this out below cal version 9
		*out_generation = cal0->version <= 8 ? 0 : cal0->ext_ssl_key_ver;
	} else if (cal0->ssl_key_crc == crc16_calc(cal0->ssl_key_iv, key_crc_size)) {
		*out_key = cal0->ssl_key;
		*out_key_size = key_size;
		*out_iv = cal0->ssl_key_iv;
		*out_generation = 0;
	} else {
		log_printf(true, LOG_ERR, LOG_MSG_KEYS_DUMP_ERROR_CRC_SSL_KEY);
		return false;
	}
	return true;
}

bool cal0_get_eticket_rsa_key(const nx_emmc_cal0_t *cal0, const void **out_key, u32 *out_key_size, const void **out_iv, u32 *out_generation) {
	const u32 ext_key_size = sizeof(cal0->ext_ecc_rsa2048_eticket_key_iv) + sizeof(cal0->ext_ecc_rsa2048_eticket_key);
	const u32 ext_key_crc_size = ext_key_size + sizeof(cal0->ext_ecc_rsa2048_eticket_key_ver) + sizeof(cal0->crc16_pad38);
	const u32 key_size = sizeof(cal0->rsa2048_eticket_key_iv) + sizeof(cal0->rsa2048_eticket_key);
	const u32 key_crc_size = key_size + sizeof(cal0->crc16_pad21);

	if (cal0->ext_ecc_rsa2048_eticket_key_crc == crc16_calc(cal0->ext_ecc_rsa2048_eticket_key_iv, ext_key_crc_size)) {
		*out_key = cal0->ext_ecc_rsa2048_eticket_key;
		*out_key_size = ext_key_size;
		*out_iv = cal0->ext_ecc_rsa2048_eticket_key_iv;
		// Settings sysmodule manually zeroes this out below cal version 9
		*out_generation = cal0->version <= 8 ? 0 : cal0->ext_ecc_rsa2048_eticket_key_ver;
	} else if (cal0->rsa2048_eticket_key_crc == crc16_calc(cal0->rsa2048_eticket_key_iv, key_crc_size)) {
		*out_key = cal0->rsa2048_eticket_key;
		*out_key_size = key_size;
		*out_iv = cal0->rsa2048_eticket_key_iv;
		*out_generation = 0;
	} else {
		log_printf(true, LOG_ERR, LOG_MSG_KEYS_DUMP_ERROR_CRC_DEVICE_KEY);
		return false;
	}
	return true;
}
*/

bool cal0_get_ssl_rsa_key(const u8 *buf, const void **out_key, u32 *out_key_size, const void **out_iv, u32 *out_generation)
{
	const u32 cal_ver = rd_u32_le(&buf[pi_off(PI_F_Version)]);

	u32 gen = 0;
	if (cal0_check_crc16_field(buf, NX_EMMC_CALIBRATION_SIZE, PI_F_ExtendedSslKey, PI_F_ExtendedSslKeyCrc16, &gen))
	{
		const u32 data_off = pi_off(PI_F_ExtendedSslKey);
		const u32 data_len = pi_len(PI_F_ExtendedSslKey);

		*out_iv = &buf[data_off];
		*out_key = &buf[data_off + 0x10];
		*out_key_size = data_len;
		*out_generation = (cal_ver <= 8) ? 0 : gen;
		return true;
	}

	// Legacy SSL key
	if (cal0_check_crc16_field(buf, NX_EMMC_CALIBRATION_SIZE, PI_F_SslKey, PI_F_SslKeyCrc16, NULL))
	{
		const u32 data_off = pi_off(PI_F_SslKey);
		const u32 data_len = pi_len(PI_F_SslKey);

		*out_iv = &buf[data_off];
		*out_key = &buf[data_off + 0x10];
		*out_key_size = data_len;
		*out_generation = 0;
		return true;
	}

	log_printf(true, LOG_ERR, LOG_MSG_KEYS_DUMP_ERROR_CRC_SSL_KEY);
	return false;
}

bool cal0_get_eticket_rsa_key(const u8 *buf, const void **out_key, u32 *out_key_size, const void **out_iv, u32 *out_generation)
{
	const u32 cal_ver = rd_u32_le(&buf[pi_off(PI_F_Version)]);

	u32 gen = 0;
	if (cal0_check_crc16_field(buf, NX_EMMC_CALIBRATION_SIZE, PI_F_ExtendedRsa2048ETicketKey, PI_F_ExtendedRsa2048ETicketKeyCrc16, &gen))
	{
		const u32 data_off = pi_off(PI_F_ExtendedRsa2048ETicketKey);
		const u32 data_len = pi_len(PI_F_ExtendedRsa2048ETicketKey);

		*out_iv = &buf[data_off];
		*out_key = &buf[data_off + 0x10];
		*out_key_size = data_len;
		*out_generation = (cal_ver <= 8) ? 0 : gen;
		return true;
	}

	if (cal0_check_crc16_field(buf, NX_EMMC_CALIBRATION_SIZE, PI_F_Rsa2048ETicketKey, PI_F_Rsa2048ETicketKeyCrc16, NULL))
	{
		const u32 data_off = pi_off(PI_F_Rsa2048ETicketKey);
		const u32 data_len = pi_len(PI_F_Rsa2048ETicketKey);

		*out_iv = &buf[data_off];
		*out_key = &buf[data_off + 0x10];
		*out_key_size = data_len;
		*out_generation = 0;
		return true;
	}

	log_printf(true, LOG_ERR, LOG_MSG_KEYS_DUMP_ERROR_CRC_DEVICE_KEY);
	return false;
}