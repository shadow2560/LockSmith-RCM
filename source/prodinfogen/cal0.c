/*
 * Copyright (c) 2019-2021 CaramelDunes
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
#include "cal0.h"

#include <stdlib.h>
#include <string.h>
#include "../keys/gmac.h"
#include <sec/se.h>
#include <sec/se_t210.h>
#include <soc/fuse.h>
#include <utils/util.h>

// #include "crc16.h"

#include "../hos/hos.h"
#include "../keys/crypto.h"
#include "../keys/es_crypto.h"
#include "../keys/key_sources.inl"
#include "../keys/ssl_crypto.h"
#include "../prodinfo_rewrite/prodinfo_rewrite.h"

#include "../tools.h"

const u32 MINIMUM_PRODINFO_SIZE = 0x3D70;
const u32 MAXIMUM_PRODINFO_SIZE = 0x3FBC00;

static const u8 es_kek_source_y[0x10] = {0xaf, 0x44, 0xf3, 0x3e, 0x82, 0x4e, 0x83, 0x92, 0xed, 0x38, 0xe1, 0x2f, 0x29, 0xcf, 0x6f, 0x4d};

static const u8 lotus_kek_source_x[0x10] = {0xD0, 0xCC, 0xDE, 0x8D, 0x10, 0x98, 0x67, 0x2F, 0x64, 0x9A, 0x1E, 0xC4, 0xFE, 0x85, 0x62, 0xF0};
static const u8 lotus_kek_source_y[0x10] = {0xC3, 0xB5, 0x86, 0x0A, 0xE9, 0xF7, 0x80, 0xF5, 0xAF, 0xA8, 0x49, 0x2D, 0xD4, 0x33, 0xB5, 0xA9};

static inline void write64le(volatile void *qword, size_t offset, uint64_t value)
{
	*(uint64_t *)((uintptr_t)qword + offset) = value;
}

static inline void write64be(volatile void *qword, size_t offset, uint64_t value)
{
	write64le(qword, offset, __builtin_bswap64(value));
}

void device_id_string(char device_id_string[0x11])
{
	u64 device_id = fuse_get_device_id();
	device_id |= 0x6300000000000000ULL;

	static const char digits[] = "0123456789ABCDEF";
	int i = 0;
	u64 v = device_id;
	for (i = 0xF; i >= 0; i--)
	{
		device_id_string[i] = digits[v % 16];
		v /= 16;
	}
}

bool valid_prodinfo_checksums(u8 *prodinfo_buffer, u32 prodinfo_size)
{
	return prodinfo_size >= MINIMUM_PRODINFO_SIZE &&
		prodinfo_size <= MAXIMUM_PRODINFO_SIZE &&
		verifyProdinfo(prodinfo_buffer);
}

bool valid_own_prodinfo(u8 *prodinfo_buffer, u32 prodinfo_size, u8 *master_key)
{
	return valid_prodinfo_checksums(prodinfo_buffer, prodinfo_size) &&
		   valid_extended_rsa_2048_eticket_key(prodinfo_buffer, master_key) &&
		   valid_extended_ecc_b233_device_key(prodinfo_buffer, master_key);
}

static void _generate_kek(u32 ks, const void *key_source, void *master_key, const void *kek_seed)
{
	se_aes_key_set(ks, master_key, 0x10);
	se_aes_unwrap_key(ks, ks, kek_seed);
	se_aes_unwrap_key(ks, ks, key_source);
}

void unseal_key(const u8 *kek_source_x, const u8 *kek_source_y, u8 *master_key, u8 *dest, u8 usecase)
{
	u8 temp_key[0x10] = {0};
	const u8 *seed = NULL;
	switch (usecase)
	{
	case 1:
		seed = seal_key_masks[1];
		break;

	case 2:
		seed = seal_key_masks[2];
		break;

	case 3:
		seed = seal_key_masks[3];
		break;

	default:
		// gfx_printf("Invalid usescase: %c\n", usecase);
		break;
	}

	for (u32 i = 0; i < 0x10; i++)
		temp_key[i] = aes_kek_generation_source[i] ^ seed[i];

	_generate_kek(KEYSLOT_SWITCH_TEMPKEY, kek_source_x, master_key, temp_key);
	se_aes_crypt_block_ecb(KEYSLOT_SWITCH_TEMPKEY, 0, dest, kek_source_y);
}

/*
// Assumes key is in KEYSLOT_SWITCH_TEMPKEY.
void ghash_calc(const u8 *plaintext, u32 plaintext_size, const u8 ctr[0x10], u8 *dest)
{
	// J = GHASH(CTR);
	uint8_t j_block[0x10];
	ghash(j_block, ctr, 0x10, NULL, false);

	// MAC = GHASH(PLAINTEXT) ^ ENCRYPT(J)
	// Note: That MAC is calculated over plaintext_size is non-standard.
	// It is supposed to be over the ciphertext.
	ghash(dest, plaintext, plaintext_size, j_block, true);
}
*/

static void calc_gmac_nokey(void *out_gmac, const void *data, u32 size, const void *iv) {
	calc_gmac(KEYSLOT_SWITCH_TEMPKEY, out_gmac, data, size, NULL, iv);
}

// Assumes key is in KEYSLOT_SWITCH_TEMPKEY.
static bool _decrypt_gcm_block(const u8 *block, u32 block_size, u8 *plaintext)
{
	const u8 *ctr = block;
	const u8 *ciphertext = block + 0x10;
	u32 plaintext_size = block_size - 0x20; // Substract CTR and MAC sizes.
	const u8 *encrypted_ghash = ciphertext + plaintext_size;

	se_aes_crypt_ctr(KEYSLOT_SWITCH_TEMPKEY, plaintext, ciphertext, plaintext_size, (void*)ctr);

	// u64 file_device_id = *(u64 *)(plaintext + plaintext_size - 0x8) & 0x00FFFFFFFFFFFFFFULL;
	// gfx_hexdump(0, plaintext + plaintext_size - 0x10, 0x10);

	uint8_t calc_mac[0x10];
	// ghash_calc(plaintext, plaintext_size, ctr, calc_mac);
	// calc_gmac(KEYSLOT_SWITCH_TEMPKEY, calc_mac, plaintext, plaintext_size, NULL, ctr);
	calc_gmac_nokey(calc_mac, plaintext, plaintext_size, ctr);
	int match = memcmp(encrypted_ghash, calc_mac, 0x10);

	return match == 0;
}

static void _encrypt_gcm_block(u8 *block, u32 block_size, u8 *plaintext, u64 device_id)
{
	u8 *ctr = block;
	u8 *ciphertext = block + 0x10;
	u32 plaintext_size = block_size - 0x20;
	u8 *encrypted_ghash = ciphertext + plaintext_size;

	// Replace device id
	write64be(plaintext, plaintext_size - 0x8, device_id);

	// Copy new GHASH
	// ghash_calc(plaintext, plaintext_size, ctr, encrypted_ghash);
	// calc_gmac(KEYSLOT_SWITCH_TEMPKEY, encrypted_ghash, plaintext, plaintext_size, NULL, ctr);
	calc_gmac_nokey(encrypted_ghash, plaintext, plaintext_size, ctr);

	// Reencrypt
	se_aes_crypt_ctr(KEYSLOT_SWITCH_TEMPKEY, ciphertext, plaintext, plaintext_size, ctr);
}

static int _is_valid_gcm_content(const u8 *ctr, u8 *ciphertext, u32 ciphertext_size)
{
	u32 plaintext_size = ciphertext_size;
	u8 *encrypted_ghash = ciphertext + ciphertext_size;

	u8 *plaintext = malloc(plaintext_size);

	se_aes_crypt_ctr(KEYSLOT_SWITCH_TEMPKEY, plaintext, ciphertext, plaintext_size, (void*)ctr);
	// gfx_hexdump(0, plaintext + plaintext_size - 0x10, 0x10);

	uint8_t calc_mac[0x10];
	// ghash_calc(plaintext, plaintext_size, ctr, calc_mac);
	// calc_gmac(KEYSLOT_SWITCH_TEMPKEY, calc_mac, plaintext, plaintext_size, NULL, ctr);
	calc_gmac_nokey(calc_mac, plaintext, plaintext_size, ctr);
	int match = memcmp(encrypted_ghash, calc_mac, 0x10);

	free(plaintext);

	return match == 0;
}

void _prepare_eticket_key(u8 ks_dst, u8 *master_key)
{
	u8 the_key[0x10] = {0};
	unseal_key(eticket_rsa_kekek_source, eticket_rsa_kek_source, master_key, the_key, 3);
	se_aes_key_set(ks_dst, the_key, 0x10);
}

void _prepare_device_key(u8 ks_dst, u8 *master_key)
{
	u8 the_key[0x10] = {0};
	unseal_key(ssl_rsa_kekek_source, es_kek_source_y, master_key, the_key, 1);
	se_aes_key_set(ks_dst, the_key, 0x10);
}

void _prepare_gamecard_key(u8 ks_dst, u8 *master_key)
{
	u8 the_key[0x10] = {0};
	unseal_key(lotus_kek_source_x, lotus_kek_source_y, master_key, the_key, 2);
	se_aes_key_set(ks_dst, the_key, 0x10);
}

bool valid_extended_rsa_2048_eticket_key(u8 *prodinfo_buffer, u8 *master_key)
{
	_prepare_eticket_key(KEYSLOT_SWITCH_TEMPKEY, master_key);

	u8 *ctr = prodinfo_buffer + 0x3890;
	u8 *ciphertext = ctr + 0x10;

	int valid = _is_valid_gcm_content(ctr, ciphertext, 0x220);

	return valid;
}

bool valid_extended_ecc_b233_device_key(u8 *prodinfo_buffer, u8 *master_key)
{
	_prepare_device_key(KEYSLOT_SWITCH_TEMPKEY, master_key);

	u8 *ctr = prodinfo_buffer + 0x3770;
	u8 *ciphertext = ctr + 0x10;

	int valid = _is_valid_gcm_content(ctr, ciphertext, 0x30);

	return valid;
}

bool valid_extended_gamecard_key(u8 *prodinfo_buffer, u8 *master_key)
{
	_prepare_gamecard_key(KEYSLOT_SWITCH_TEMPKEY, master_key);

	u8 *ctr = prodinfo_buffer + 0x3C20;
	u8 *ciphertext = ctr + 0x10;

	int valid = _is_valid_gcm_content(ctr, ciphertext, 0x110);

	return valid;
}

/*
bool valid_ecc_b233_device_certificate(u8 *prodinfo_buffer)
{
	return has_valid_crc16(prodinfo_buffer, 0x0480, 0x190);
}

bool valid_rsa_2048_eticket_certificate(u8 *prodinfo_buffer)
{
	return has_valid_crc16(prodinfo_buffer, 0x2A90, 0x250);
}
*/

bool valid_cal0_signature(u8 *prodinfo_buffer, u32 prodinfo_size)
{
	return prodinfo_size > 4 && prodinfo_buffer[0] == 'C' && prodinfo_buffer[1] == 'A' && prodinfo_buffer[2] == 'L' && prodinfo_buffer[3] == '0';
}

void write_mac_addresses(u8 *prodinfo_buffer, u64 device_id)
{
	// Here we are using the device id to generate an almost unique, yet deterministic, MAC address.

	// WlanMacAddress
	u8 blank_nintendo_mac[] = {0xA4, 0x38, 0xCC};
	memcpy(prodinfo_buffer + pi_off(PI_F_WlanMacAddress), blank_nintendo_mac, 3);
	memcpy(prodinfo_buffer + pi_off(PI_F_WlanMacAddress) + 3, &device_id, 3);

	// BdAddress
	memcpy(prodinfo_buffer + pi_off(PI_F_BdAddress), blank_nintendo_mac, 3);
	memcpy(prodinfo_buffer + pi_off(PI_F_BdAddress) + 3, &device_id, 3);
}

void write_serial_number(u8 *prodinfo_buffer)
{
	const char serial_number[] = "XAW10000000000";
	memcpy(prodinfo_buffer + pi_off(PI_F_SerialNumber), serial_number, 14);
}

void write_device_id_string_at_offset(u8 *prodinfo_buffer, const char *device_id_string, u32 offset)
{
	prodinfo_buffer[offset] = 'N';
	prodinfo_buffer[offset + 1] = 'X';

	memcpy(prodinfo_buffer + offset + 2, device_id_string, 0x10);

	prodinfo_buffer[offset + 18] = '-';
	prodinfo_buffer[offset + 19] = '0';
}

void write_ssl_certificate(u8 *prodinfo_buffer)
{
	u8 ssl_certificate_size[] = {0xE9, 0x05};
	memcpy(prodinfo_buffer + pi_off(PI_F_SslCertificateSize), ssl_certificate_size, sizeof(ssl_certificate_size));
}

void write_random_number(u8 *prodinfo_buffer, u64 device_id)
{
	memset(prodinfo_buffer + pi_off(PI_F_RandomNumber), 0, pi_len(PI_F_RandomNumber) - 0x20);
	u64 key[2] = {device_id, device_id};
	u8 ctr[0x10] = {0};

	se_aes_key_set(KEYSLOT_SWITCH_TEMPKEY, key, 0x10);
	se_aes_crypt_ctr(KEYSLOT_SWITCH_TEMPKEY,
					 prodinfo_buffer + pi_off(PI_F_RandomNumber),
					 prodinfo_buffer + pi_off(PI_F_RandomNumber), pi_len(PI_F_RandomNumber) - 0x20, ctr);
}

void write_config_id(u8 *prodinfo_buffer)
{
	const char default_config_id[] = "MP_00_01_00_00";

	memcpy(prodinfo_buffer + pi_off(PI_F_ConfigurationId1), default_config_id, sizeof(default_config_id));
}

void write_wlan_country_codes(u8 *prodinfo_buffer)
{
	const u8 default_wlan_country_codes[11] = {
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x52, 0x31, 0x00};

	memcpy(prodinfo_buffer + pi_off(PI_F_WlanCountryCodesNum), default_wlan_country_codes, sizeof(default_wlan_country_codes));
}

void write_header(u8 *prodinfo_buffer)
{
	unsigned char header[32] = {
		'C', 'A', 'L', '0', 0x07, 0x00, 0x00, 0x00, 0x30, 0x3D, 0x00, 0x00,
		0x01, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x71, 0x0E};

	memcpy(prodinfo_buffer, header, sizeof(header));

	u32 size = 0x7FC0;
	memcpy(prodinfo_buffer + 0x8, &size, 4);
}

void write_sensors_offset_scale(u8 *prodinfo_buffer)
{
	unsigned char sensors_offset_scale[32] = {
		0xFC, 0xFF, 0xFA, 0xFF, 0xC4, 0x00, 0x46, 0x06, 0xFF, 0x3F, 0xFF, 0x3F,
		0xFF, 0x3F, 0x15, 0xF4, 0xFD, 0xFF, 0xDD, 0xFF, 0xF3, 0xFF, 0x1B, 0x13,
		0xFF, 0x3F, 0xFF, 0x3F, 0xFF, 0x3F, 0x15, 0xF4};

	memcpy(prodinfo_buffer + pi_off(PI_F_AccelerometerOffset), sensors_offset_scale, sizeof(sensors_offset_scale));
}

void write_battery_lot(u8 *prodinfo_buffer)
{
	const char battery_lot[] = "BHACHZZADM402211310199";
	memcpy(prodinfo_buffer + pi_off(PI_F_BatteryLot), battery_lot, sizeof(battery_lot));
}

void write_speaker_calibration_value(u8 *prodinfo_buffer)
{
	// Seems constant for all Switch models.
	unsigned char speaker_calibration_value[80] = {
		0x00, 0x03, 0x00, 0x5A, 0xED, 0x87, 0x00, 0x00, 0xC1, 0x61, 0x1E, 0xAF,
		0x09, 0x5B, 0xC9, 0x60, 0x18, 0x8D, 0x00, 0x00, 0xDE, 0x2A, 0x0F, 0xDB,
		0xFC, 0xB6, 0x00, 0x00, 0x08, 0x93, 0x01, 0xF3, 0x1F, 0xAA, 0x00, 0x00,
		0x1F, 0xB4, 0x00, 0x4B, 0x1F, 0xB4, 0x08, 0x00, 0x08, 0x00, 0x00, 0xC1,
		0x60, 0x41, 0x1F, 0x80, 0x04, 0x80, 0x6B, 0x30, 0x04, 0x04, 0x12, 0x12,
		0x00, 0x00, 0x94, 0x94, 0x00, 0x00, 0xAA, 0xAA, 0x50, 0x00, 0x00, 0x80,
		0x2F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	memcpy(prodinfo_buffer + pi_off(PI_F_SpeakerCalibrationValue), speaker_calibration_value, sizeof(speaker_calibration_value));
}

void write_short_values(u8 *prodinfo_buffer, u32 display_id)
{
	prodinfo_buffer[pi_off(PI_F_RegionCode)] = 1;

	u8 product_model = 1;
	switch (fuse_read_hw_type())
	{
	case FUSE_NX_HW_TYPE_ICOSA:
		product_model = 1;
		break;
	case FUSE_NX_HW_TYPE_IOWA:
		product_model = 3;
		break;
	case FUSE_NX_HW_TYPE_HOAG:
		product_model = 4;
		break;
	case FUSE_NX_HW_TYPE_AULA:
		product_model = 6;
		break;
	}
	prodinfo_buffer[pi_off(PI_F_ProductModel)] = product_model;

	unsigned char brightness_mapping[12] = {
		0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x0A, 0xD7, 0xA3, 0x3C};
	memcpy(prodinfo_buffer + pi_off(PI_F_LcdBacklightBrightnessMapping), brightness_mapping, sizeof(brightness_mapping));

	memcpy(prodinfo_buffer + pi_off(PI_F_LcdVendorId), (u8 *)&display_id + 1, 3); // Skip leading 00.

	// prodinfo_buffer[pi_off(PI_F_UsbTypeCPowerSourceCircuitVersion)] = 1;
	// prodinfo_buffer[pi_off(PI_F_Reserved3)] = 1;
}

void write_console_colors(u8 *prodinfo_buffer, u64 device_id)
{
	u8 *device_id_bytes = (u8 *)&device_id;

	prodinfo_buffer[pi_off(PI_F_HomeMenuSchemeMainColorVariation)] = 1;

	// Those colors can be seen in the Controllers screen.
	u8 sub_color[4] = {0x0, 0xFF, 0x00, 0xFF}; // Unused?

	u8 bezel_color[4] = {device_id_bytes[0], device_id_bytes[1], device_id_bytes[2], 0xFF};  // Round bezel
	u8 main_color_1[4] = {device_id_bytes[3], device_id_bytes[4], device_id_bytes[5], 0xFF}; // Border

	u8 main_color_2[4] = {0xFF, 0x00, 0xFF, 0xFF}; // Unused or depending on ColorVariation?
	u8 main_color_3[4] = {0xFF, 0xFF, 0x00, 0xFF}; // Unused or depending on ColorVariation?

	memcpy(prodinfo_buffer + pi_off(PI_F_HomeMenuSchemeSubColor), sub_color, 4);
	memcpy(prodinfo_buffer + pi_off(PI_F_HomeMenuSchemeBezelColor), bezel_color, 4);
	memcpy(prodinfo_buffer + pi_off(PI_F_HomeMenuSchemeMainColor1), main_color_1, 4);
	memcpy(prodinfo_buffer + pi_off(PI_F_HomeMenuSchemeMainColor2), main_color_2, 4);
	memcpy(prodinfo_buffer + pi_off(PI_F_HomeMenuSchemeMainColor3), main_color_3, 4);
}

void import_gamecard_certificate(u8 *donor_prodinfo_buffer, u8 *prodinfo_buffer)
{
	memcpy(prodinfo_buffer + pi_off(PI_F_GameCardCertificate), donor_prodinfo_buffer + pi_off(PI_F_GameCardCertificate), pi_len(PI_F_GameCardCertificate));
}

void import_amiiboo_certificates(u8 *donor_prodinfo_buffer, u8 *prodinfo_buffer)
{
	memcpy(prodinfo_buffer + pi_off(PI_F_AmiiboEcqvCertificate), donor_prodinfo_buffer + pi_off(PI_F_AmiiboEcqvCertificate),
		   pi_len(PI_F_AmiiboEcqvCertificate) +
			   pi_len(PI_F_AmiiboEcdsaCertificate));

	memcpy(prodinfo_buffer + pi_off(PI_F_AmiiboEcqvBlsCertificate), donor_prodinfo_buffer + pi_off(PI_F_AmiiboEcqvBlsCertificate),
		   pi_len(PI_F_AmiiboEcqvBlsCertificate) +
			   pi_len(PI_F_AmiiboEcqvBlsRootCertificate));
}

bool decrypt_extended_device_key(u8 *donor_prodinfo_buffer, u8 extended_device_key[0x30], u8 *master_key)
{
	_prepare_device_key(KEYSLOT_SWITCH_TEMPKEY, master_key);
	return _decrypt_gcm_block(donor_prodinfo_buffer + pi_off(PI_F_ExtendedEccB233DeviceKey), 0x50, extended_device_key);
}

void encrypt_extended_device_key(u8 *prodinfo_buffer, u8 extended_device_key[0x30], u64 device_id, u8 *master_key)
{
	_prepare_device_key(KEYSLOT_SWITCH_TEMPKEY, master_key);
	_encrypt_gcm_block(prodinfo_buffer + pi_off(PI_F_ExtendedEccB233DeviceKey), 0x50,
					   extended_device_key, device_id);
}

bool decrypt_extended_eticket_key(u8 *donor_prodinfo_buffer, u8 extended_eticket_key[0x220], u8 *master_key)
{
	_prepare_eticket_key(KEYSLOT_SWITCH_TEMPKEY, master_key);
	return _decrypt_gcm_block(donor_prodinfo_buffer + pi_off(PI_F_ExtendedRsa2048ETicketKey), 0x240, extended_eticket_key);
}

void encrypt_extended_eticket_key(u8 *prodinfo_buffer, u8 extended_eticket_key[0x220], u64 device_id, u8 *master_key)
{
	_prepare_eticket_key(KEYSLOT_SWITCH_TEMPKEY, master_key);
	_encrypt_gcm_block(prodinfo_buffer + pi_off(PI_F_ExtendedRsa2048ETicketKey), 0x240,
					   extended_eticket_key, device_id);
}

bool decrypt_extended_gamecard_key(u8 *donor_prodinfo_buffer, u8 extended_gamecard_key[0x110], u8 *master_key)
{
	_prepare_gamecard_key(KEYSLOT_SWITCH_TEMPKEY, master_key);
	return _decrypt_gcm_block(donor_prodinfo_buffer + pi_off(PI_F_ExtendedGameCardKey), 0x130, extended_gamecard_key);
}

void encrypt_extended_gamecard_key(u8 *prodinfo_buffer, u8 extended_gamecard_key[0x110], u64 device_id, u8 *master_key)
{
	_prepare_gamecard_key(KEYSLOT_SWITCH_TEMPKEY, master_key);
	_encrypt_gcm_block(prodinfo_buffer + pi_off(PI_F_ExtendedGameCardKey), 0x130,
					   extended_gamecard_key, device_id);
}
