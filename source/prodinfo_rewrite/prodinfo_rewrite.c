/* prodinfo_rewrite.c - Nintendo Switch payload utility (BDK/Hekate style)
 *
 * Converted from prodinfo_rewrite.py (shadow256) - GPLv3
 * This C implementation operates on a buffer u8* instead of files.
 *
 * Constraints respected:
 * - uses s_printf() for formatting text output
 * - uses se_sha_hash_256_oneshot() for SHA256
 * - no realloc()
 * - avoid using large BSS (large tables are static const in .rodata)
 *
 * NOTE:
 * This module does NOT provide main() nor any CLI/help. It only provides utility functions.
 */

#include "prodinfo_rewrite.h"
#include <mem/heap.h>
#include <sec/se.h>
#include <utils/sprintf.h>
#include "../gfx/messages.h"
#include "../keys/cal0_read.h"

/* ----- little-endian helpers ----- */

u16 rd_u16_le(const u8 *p)
{
	return (u16)(p[0] | ((u16)p[1] << 8));
}

u32 rd_u32_le(const u8 *p)
{
	return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

/* Replaced by memcpy() */
static inline void wr_mem(u8 *dst, const void *src, u32 len)
{
	for (u32 i = 0; i < len; i++)
		dst[i] = ((const u8*)src)[i];
}

/* ----- field description table (complete as in python) ----- */

/* For CRC items:
 * - the CAL0 format stores a "crc field" of length N, where first crc_pad bytes are a padding copied as-is,
 *   and remaining bytes store little-endian CRC16 computed over (crc_source_bytes + padding_bytes).
 * - In python: expected_crc_field = padding + crc16_le(crc_source + padding)
 */
const pi_field_desc_t g_fields[] = {
	/* Header */
	{ PI_ITEM_DATA, 0x0000, 0x4,  0,0,0, 0,0 }, /* MagicNumber */
	{ PI_ITEM_DATA, 0x0004, 0x4,  0,0,0, 0,0 }, /* Version */
	{ PI_ITEM_DATA, 0x0008, 0x4,  0,0,0, 0,0 }, /* BodySize */
	{ PI_ITEM_DATA, 0x000C, 0x2,  0,0,0, 0,0 }, /* Model */
	{ PI_ITEM_DATA, 0x000E, 0x2,  0,0,0, 0,0 }, /* UpdateCount */
	/* Begin file crc16: field at current tell() which is 0x10, len 0x10, pad 0x0E, source is concatenation of previous header bytes (CAL0+Version+BodySize+Model+UpdateCount) */
	{ PI_ITEM_CRC,  0x0010, 0x10, 0x0E, 0x0000, 0x10, 0,0 },

	{ PI_ITEM_SHA,  0x0020, 0x20, 0,0,0, 0x0040, 0 }, /* BodyHash over calibration body (size = BodySize, set at runtime) */

	/* ConfigurationId1 + its crc */
	{ PI_ITEM_DATA, 0x0040, 0x1E, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x005E, 0x02, 0x00, 0x0040, 0x1E, 0,0 },

	{ PI_ITEM_DATA, 0x0060, 0x20, 0,0,0, 0,0 }, /* Reserved */

	/* WlanCountryCodes block + crc */
	{ PI_ITEM_DATA, 0x0080, 0x4,  0,0,0, 0,0 }, /* Num */
	{ PI_ITEM_DATA, 0x0084, 0x4,  0,0,0, 0,0 }, /* LastIndex */
	{ PI_ITEM_DATA, 0x0088, 0x180,0,0,0, 0,0 }, /* Array */
	{ PI_ITEM_CRC,  0x0208, 0x08, 0x06, 0x0080, 0x188, 0,0 }, /* CRC over Num+LastIndex+Array */

	/* WlanMacAddress + crc */
	{ PI_ITEM_DATA, 0x0210, 0x6,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x0216, 0x2,  0x0, 0x0210, 0x6, 0,0 },

	{ PI_ITEM_DATA, 0x0218, 0x8,  0,0,0, 0,0 }, /* Reserved */

	/* BdAddress + crc */
	{ PI_ITEM_DATA, 0x0220, 0x6,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x0226, 0x2,  0x0, 0x0220, 0x6, 0,0 },

	{ PI_ITEM_DATA, 0x0228, 0x8,  0,0,0, 0,0 }, /* Reserved */

	/* AccelerometerOffset + crc */
	{ PI_ITEM_DATA, 0x0230, 0x6,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x0236, 0x2,  0x0, 0x0230, 0x6, 0,0 },

	/* AccelerometerScale + crc */
	{ PI_ITEM_DATA, 0x0238, 0x6,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x023E, 0x2,  0x0, 0x0238, 0x6, 0,0 },

	/* GyroscopeOffset + crc */
	{ PI_ITEM_DATA, 0x0240, 0x6,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x0246, 0x2,  0x0, 0x0240, 0x6, 0,0 },

	/* GyroscopeScale + crc */
	{ PI_ITEM_DATA, 0x0248, 0x6,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x024E, 0x2,  0x0, 0x0248, 0x6, 0,0 },

	/* SerialNumber + crc */
	{ PI_ITEM_DATA, 0x0250, 0x18, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x0268, 0x08, 0x06, 0x0250, 0x18, 0,0 },

	/* EccP256DeviceKey + crc */
	{ PI_ITEM_DATA, 0x0270, 0x30, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x02A0, 0x10, 0x0E, 0x0270, 0x30, 0,0 },

	/* EccP256DeviceCertificate + crc */
	{ PI_ITEM_DATA, 0x02B0, 0x180,0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x0430, 0x10, 0x0E, 0x02B0, 0x180,0,0 },

	/* EccB233DeviceKey + crc */
	{ PI_ITEM_DATA, 0x0440, 0x30, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x0470, 0x10, 0x0E, 0x0440, 0x30, 0,0 },

	/* EccB233DeviceCertificate + crc */
	{ PI_ITEM_DATA, 0x0480, 0x180,0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x0600, 0x10, 0x0E, 0x0480, 0x180,0,0 },

	/* EccP256ETicketKey + crc */
	{ PI_ITEM_DATA, 0x0610, 0x30, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x0640, 0x10, 0x0E, 0x0610, 0x30, 0,0 },

	/* EccP256ETicketCertificate + crc */
	{ PI_ITEM_DATA, 0x0650, 0x180,0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x07D0, 0x10, 0x0E, 0x0650, 0x180,0,0 },

	/* EccB233ETicketKey + crc */
	{ PI_ITEM_DATA, 0x07E0, 0x30, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x0810, 0x10, 0x0E, 0x07E0, 0x30, 0,0 },

	/* EccB233ETicketCertificate + crc */
	{ PI_ITEM_DATA, 0x0820, 0x180,0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x09A0, 0x10, 0x0E, 0x0820, 0x180,0,0 },

	/* SslKey + crc */
	{ PI_ITEM_DATA, 0x09B0, 0x110,0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x0AC0, 0x10, 0x0E, 0x09B0, 0x110,0,0 },

	/* SslCertificateSize + crc */
	{ PI_ITEM_DATA, 0x0AD0, 0x4,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x0AD4, 0x0C, 0x0A, 0x0AD0, 0x4,  0,0 },

	/* SslCertificate + hash */
	{ PI_ITEM_DATA, 0x0AE0, 0x800,0,0,0, 0,0 },
	{ PI_ITEM_SHA,  0x12E0, 0x20, 0,0,0, 0x0AE0, 0 }, /* size = SslCertificateSize at runtime */

	/* RandomNumber + hash */
	{ PI_ITEM_DATA, 0x1300, 0x1000,0,0,0, 0,0 },
	{ PI_ITEM_SHA,  0x2300, 0x20, 0,0,0, 0x1300, 0x1000 },

	/* GameCardKey + crc */
	{ PI_ITEM_DATA, 0x2320, 0x110,0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x2430, 0x10, 0x0E, 0x2320, 0x110,0,0 },

	/* GameCardCertificate + hash */
	{ PI_ITEM_DATA, 0x2440, 0x400,0,0,0, 0,0 },
	{ PI_ITEM_SHA,  0x2840, 0x20, 0,0,0, 0x2440, 0x400 },

	/* Rsa2048ETicketKey + crc */
	{ PI_ITEM_DATA, 0x2860, 0x220,0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x2A80, 0x10, 0x0E, 0x2860, 0x220,0,0 },

	/* Rsa2048ETicketCertificate + crc */
	{ PI_ITEM_DATA, 0x2A90, 0x240,0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x2CD0, 0x10, 0x0E, 0x2A90, 0x240,0,0 },

	/* BatteryLot + crc */
	{ PI_ITEM_DATA, 0x2CE0, 0x18, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x2CF8, 0x08, 0x06, 0x2CE0, 0x18, 0,0 },

	/* SpeakerCalibrationValue + crc */
	{ PI_ITEM_DATA, 0x2D00, 0x800,0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x3500, 0x10, 0x0E, 0x2D00, 0x800,0,0 },

	/* RegionCode + crc */
	{ PI_ITEM_DATA, 0x3510, 0x4,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x3514, 0x0C, 0x0A, 0x3510, 0x4,  0,0 },

	/* AmiiboKey + crc */
	{ PI_ITEM_DATA, 0x3520, 0x50, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x3570, 0x10, 0x0E, 0x3520, 0x50, 0,0 },

	/* AmiiboEcqvCertificate + crc */
	{ PI_ITEM_DATA, 0x3580, 0x14, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x3594, 0x0C, 0x0A, 0x3580, 0x14, 0,0 },

	/* AmiiboEcdsaCertificate + crc */
	{ PI_ITEM_DATA, 0x35A0, 0x70, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x3610, 0x10, 0x0E, 0x35A0, 0x70, 0,0 },

	/* AmiiboEcqvBlsKey + crc */
	{ PI_ITEM_DATA, 0x3620, 0x40, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x3660, 0x10, 0x0E, 0x3620, 0x40, 0,0 },

	/* AmiiboEcqvBlsCertificate + crc */
	{ PI_ITEM_DATA, 0x3670, 0x20, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x3690, 0x10, 0x0E, 0x3670, 0x20, 0,0 },

	/* AmiiboEcqvBlsRootCertificate + crc */
	{ PI_ITEM_DATA, 0x36A0, 0x90, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x3730, 0x10, 0x0E, 0x36A0, 0x90, 0,0 },

	/* ProductModel + crc */
	{ PI_ITEM_DATA, 0x3740, 0x4,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x3744, 0x0C, 0x0A, 0x3740, 0x4,  0,0 },

	/* HomeMenuSchemeMainColorVariation + crc */
	{ PI_ITEM_DATA, 0x3750, 0x6,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x3756, 0x0A, 0x08, 0x3750, 0x6,  0,0 },

	/* LcdBacklightBrightnessMapping + crc */
	{ PI_ITEM_DATA, 0x3760, 0x0C, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x376C, 0x04, 0x02, 0x3760, 0x0C, 0,0 },

	/* ExtendedEccB233DeviceKey + crc */
	{ PI_ITEM_DATA, 0x3770, 0x50, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x37C0, 0x10, 0x0E, 0x3770, 0x50, 0,0 },

	/* ExtendedEccP256ETicketKey + crc */
	{ PI_ITEM_DATA, 0x37D0, 0x50, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x3820, 0x10, 0x0E, 0x37D0, 0x50, 0,0 },

	/* ExtendedEccB233ETicketKey + crc */
	{ PI_ITEM_DATA, 0x3830, 0x50, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x3880, 0x10, 0x0E, 0x3830, 0x50, 0,0 },

	/* ExtendedRsa2048ETicketKey + crc */
	{ PI_ITEM_DATA, 0x3890, 0x240,0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x3AD0, 0x10, 0x0E, 0x3890, 0x240,0,0 },

	/* ExtendedSslKey + crc */
	{ PI_ITEM_DATA, 0x3AE0, 0x130,0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x3C10, 0x10, 0x0E, 0x3AE0, 0x130,0,0 },

	/* ExtendedGameCardKey + crc */
	{ PI_ITEM_DATA, 0x3C20, 0x130,0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x3D50, 0x10, 0x0E, 0x3C20, 0x130,0,0 },

	/* LcdVendorId + crc */
	{ PI_ITEM_DATA, 0x3D60, 0x4,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x3D64, 0x0C, 0x0A, 0x3D60, 0x4,  0,0 },

	/* [5.0.0+] ExtendedRsa2048DeviceKey + crc (only if version > 0x7 from these) */
	{ PI_ITEM_DATA, 0x3D70, 0x240,0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x3FB0, 0x10, 0x0E, 0x3D70, 0x240,0,0 },

	/* [5.0.0+] Rsa2048DeviceCertificate + crc */
	{ PI_ITEM_DATA, 0x3FC0, 0x240,0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x4200, 0x10, 0x0E, 0x3FC0, 0x240,0,0 },

	/* [5.0.0+] UsbTypeCPowerSourceCircuitVersion + crc */
	{ PI_ITEM_DATA, 0x4210, 0x1,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x4211, 0x0F, 0x0D, 0x4210, 0x1,  0,0 },

	/* [9.0.0+] HomeMenuSchemeSubColor + crc */
	{ PI_ITEM_DATA, 0x4220, 0x4,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x4224, 0x0C, 0x0A, 0x4220, 0x4,  0,0 },

	/* [9.0.0+] HomeMenuSchemeBezelColor + crc */
	{ PI_ITEM_DATA, 0x4230, 0x4,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x4234, 0x0C, 0x0A, 0x4230, 0x4,  0,0 },

	/* [9.0.0+] HomeMenuSchemeMainColor1 + crc */
	{ PI_ITEM_DATA, 0x4240, 0x4,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x4244, 0x0C, 0x0A, 0x4240, 0x4,  0,0 },

	/* [9.0.0+] HomeMenuSchemeMainColor2 + crc */
	{ PI_ITEM_DATA, 0x4250, 0x4,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x4254, 0x0C, 0x0A, 0x4250, 0x4,  0,0 },

	/* [9.0.0+] HomeMenuSchemeMainColor3 + crc */
	{ PI_ITEM_DATA, 0x4260, 0x4,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x4264, 0x0C, 0x0A, 0x4260, 0x4,  0,0 },

	/* [9.0.0+] AnalogStickModuleTypeL + crc */
	{ PI_ITEM_DATA, 0x4270, 0x1,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x4271, 0x0F, 0x0D, 0x4270, 0x1,  0,0 },

	/* [9.0.0+] AnalogStickModelParameterL + crc */
	{ PI_ITEM_DATA, 0x4280, 0x12, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x4292, 0x0E, 0x0C, 0x4280, 0x12, 0,0 },

	/* [9.0.0+] AnalogStickFactoryCalibrationL + crc */
	{ PI_ITEM_DATA, 0x42A0, 0x9,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x42A9, 0x7,  0x5, 0x42A0, 0x9,  0,0 },

	/* [9.0.0+] AnalogStickModuleTypeR + crc */
	{ PI_ITEM_DATA, 0x42B0, 0x1,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x42B1, 0x0F, 0x0D, 0x42B0, 0x1,  0,0 },

	/* [9.0.0+] AnalogStickModelParameterR + crc */
	{ PI_ITEM_DATA, 0x42C0, 0x12, 0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x42D2, 0x0E, 0x0C, 0x42C0, 0x12, 0,0 },

	/* [9.0.0+] AnalogStickFactoryCalibrationR + crc */
	{ PI_ITEM_DATA, 0x42E0, 0x9,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x42E9, 0x7,  0x5, 0x42E0, 0x9,  0,0 },

	/* [9.0.0+] ConsoleSixAxisSensorModuleType + crc */
	{ PI_ITEM_DATA, 0x42F0, 0x1,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x42F1, 0x0F, 0x0D, 0x42F0, 0x1,  0,0 },

	/* [9.0.0+] ConsoleSixAxisSensorHorizontalOffset + crc */
	{ PI_ITEM_DATA, 0x4300, 0x6,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x4306, 0x0A, 0x08, 0x4300, 0x6,  0,0 },

	/* [6.0.0+] BatteryVersion + crc */
	{ PI_ITEM_DATA, 0x4310, 0x1,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x4311, 0x0F, 0x0D, 0x4310, 0x1,  0,0 },

	{ PI_ITEM_DATA, 0x4320, 0x10, 0,0,0, 0,0 }, /* Reserved */

	/* [9.0.0+] HomeMenuSchemeModel + crc */
	{ PI_ITEM_DATA, 0x4330, 0x4,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x4334, 0x0C, 0x0A, 0x4330, 0x4,  0,0 },

	/* [10.0.0+] ConsoleSixAxisSensorMountType + crc */
	{ PI_ITEM_DATA, 0x4340, 0x1,  0,0,0, 0,0 },
	{ PI_ITEM_CRC,  0x4341, 0x0F, 0x0D, 0x4340, 0x1,  0,0 },

	/* End marker (python read rest from 0x4350) is not an item. */
};

#define FIELD_COUNT ((u32)(sizeof(g_fields)/sizeof(g_fields[0])))

static const u8 k_magic_cal0[4] = { 'C','A','L','0' };

/* ----- internal compute helpers ----- */

static int compute_expected_crc_field(
	const u8 *buf, u32 buf_size,
	const pi_field_desc_t *f,
	u8 *out_expected
)
{
	if (f->length < 2 || f->crc_pad > f->length)
		return PI_ERR_BAD_DESC;

	const u32 pad = f->crc_pad;
	const u32 src_off = f->crc_src_offset;
	const u32 src_len = f->crc_src_length;

	if (f->offset + f->length > buf_size) return PI_ERR_OOB;
	if (src_off + src_len > buf_size)     return PI_ERR_OOB;
	if (f->length < pad + 2)              return PI_ERR_BAD_DESC;

	/* padding */
	memcpy(out_expected, buf + f->offset, pad);

	/* crc over src + padding */
	u16 crc = crc16_calc_continue(0x55AA, buf + src_off, src_len);
	crc = crc16_calc_continue(crc, out_expected, pad);

	/* zero rest then write crc */
	memset(out_expected + pad, 0, f->length - pad);
	out_expected[pad + 0] = (u8)(crc & 0xFF);
	out_expected[pad + 1] = (u8)((crc >> 8) & 0xFF);

	return PI_OK;
}

static int compute_expected_sha256(
	const u8 *buf, u32 buf_size,
	u32 data_off, u32 data_size,
	u8 out_hash[32]
)
{
	if (data_off + data_size > buf_size)
		return PI_ERR_OOB;

	se_sha_hash_256_oneshot(out_hash, buf + data_off, data_size);
	return PI_OK;
}

static inline int _field_id_valid(prodinfo_field_id_t id)
{
	return (u32)id < FIELD_COUNT;
}

/* ----- public API ----- */

int prodinfo_field_get_offset_size(prodinfo_field_id_t id, u32 *out_offset, u32 *out_size, int *out_is_data)
{
	if (!_field_id_valid(id))
		return PI_ERR_ARG;

	const pi_field_desc_t *f = &g_fields[(u32)id];
	if (out_offset) *out_offset = f->offset;
	if (out_size)   *out_size   = f->length;
	if (out_is_data) *out_is_data = (f->type == PI_ITEM_DATA);
	return PI_OK;
}

int prodinfo_write_data(u8 *prodinfo, u32 prodinfo_size, prodinfo_field_id_t id, const void *in, u32 in_size)
{
	if (!prodinfo || !in)
		return PI_ERR_ARG;
	if (!_field_id_valid(id))
		return PI_ERR_ARG;

	const pi_field_desc_t *f = &g_fields[(u32)id];
	if (f->type != PI_ITEM_DATA)
		return PI_ERR_BAD_DESC;

	if (f->offset + f->length > prodinfo_size)
		return PI_ERR_OOB;

	u32 n = in_size;
	if (n > f->length)
		n = f->length;

	memcpy(prodinfo + f->offset, in, n);
	return PI_OK;
}

int prodinfo_zero_data(u8 *prodinfo, u32 prodinfo_size, prodinfo_field_id_t id)
{
	if (!prodinfo)
		return PI_ERR_ARG;
	if (!_field_id_valid(id))
		return PI_ERR_ARG;

	const pi_field_desc_t *f = &g_fields[(u32)id];
	if (f->type != PI_ITEM_DATA)
		return PI_ERR_BAD_DESC;

	if (f->offset + f->length > prodinfo_size)
		return PI_ERR_OOB;

	/*
	for (u32 i = 0; i < f->length; i++)
		prodinfo[f->offset + i] = 0;
	*/
	memset(prodinfo + f->offset, 0, f->length);

	return PI_OK;
}

int prodinfo_verify_hashes(const u8 *prodinfo, u32 prodinfo_size, prodinfo_verify_report_t *out_report)
{
	if (!prodinfo || prodinfo_size < 0x40)
		return PI_ERR_ARG;

	if (out_report)
	{
		out_report->prodinfo_version = 0;
		out_report->body_size = 0;
		out_report->offset_stop = 0;
		out_report->crc_errors = 0;
		out_report->sha_errors = 0;
	}

	/* Check magic */
	if (prodinfo_size < 4)
		return PI_ERR_OOB;
	for (u32 i = 0; i < 4; i++)
		if (prodinfo[i] != k_magic_cal0[i]) {
			log_printf(true, LOG_ERR, LOG_MSG_ERROR_PRODINFO_MAGIC_READ);
			return PI_ERR_BAD_MAGIC;
		}

	const u32 version = rd_u32_le(prodinfo + 0x4);
	const u32 body_size = rd_u32_le(prodinfo + 0x8);
	const u32 offset_stop = body_size + 0x40;

	if (out_report)
	{
		out_report->prodinfo_version = version;
		out_report->body_size = body_size;
		out_report->offset_stop = offset_stop;
	}

	/* Verify items */
	for (u32 idx = 0; idx < FIELD_COUNT; idx++)
	{
		const pi_field_desc_t *f = &g_fields[idx];

		if (f->offset > offset_stop)
			continue;

		/* Python skips items >= 0x3D70 if version <= 0x7 */
		if (version <= 0x7 && f->offset >= 0x3D70)
			continue;

		if (f->type == PI_ITEM_CRC)
		{
			u8 expected[0x20];
			if (f->length > sizeof(expected))
				return PI_ERR_INTERNAL;

			int rc = compute_expected_crc_field(prodinfo, prodinfo_size, f, expected);
			if (rc != PI_OK) {
				log_printf(true, LOG_ERR, LOG_MSG_ERROR_PRODINFO_CRC_VERIF, f->offset);
				return rc;
			}

			/* Compare */
			u32 mismatch = 0;
			for (u32 i = 0; i < f->length; i++)
			{
				if (prodinfo[f->offset + i] != expected[i])
				{
					mismatch = 1;
					break;
				}
			}
			if (mismatch)
			{
				log_printf(true, LOG_ERR, LOG_MSG_ERROR_PRODINFO_CRC_COMPARE, f->offset);
				if (out_report) out_report->crc_errors++;
			}
		}
		else if (f->type == PI_ITEM_SHA)
		{
			/* SHA size is always 0x20 */
			u8 expected[32];
			u32 data_off = f->sha_data_offset;
			u32 data_size = f->sha_data_size;

			/* Special runtime sizes */
			if (f->offset == 0x0020) /* BodyHash */
				data_size = body_size;
			else if (f->offset == 0x12E0) /* SslCertificate hash */
				data_size = rd_u32_le(prodinfo + 0x0AD0);

			int rc = compute_expected_sha256(prodinfo, prodinfo_size, data_off, data_size, expected);
			if (rc != PI_OK) {
				log_printf(true, LOG_ERR, LOG_MSG_ERROR_PRODINFO_SHA_VERIF, f->offset);
				return rc;
			}

			u32 mismatch = 0;
			for (u32 i = 0; i < 32; i++)
			{
				if (prodinfo[f->offset + i] != expected[i])
				{
					mismatch = 1;
					break;
				}
			}
			if (mismatch)
			{
				log_printf(true, LOG_ERR, LOG_MSG_ERROR_PRODINFO_SHA_COMPARE, f->offset);
				if (out_report) out_report->sha_errors++;
			}
		}
		else
		{
			/* data fields: nothing to verify */
		}
	}

	return PI_OK;
}

int prodinfo_rewrite_hashes(const u8 *in_prodinfo, u32 in_size, u8 *out_prodinfo, u32 out_size)
{
	if (!in_prodinfo || !out_prodinfo)
		return PI_ERR_ARG;
	if (in_size < 0x40 || out_size < in_size)
		return PI_ERR_OOB;

	/* Copy input first */
	memcpy(out_prodinfo, in_prodinfo, in_size);

	/* Check magic */
	for (u32 i = 0; i < 4; i++)
		if (in_prodinfo[i] != k_magic_cal0[i]) {
			log_printf(true, LOG_ERR, LOG_MSG_ERROR_PRODINFO_MAGIC_READ);
			return PI_ERR_BAD_MAGIC;
		}

	const u32 version = rd_u32_le(in_prodinfo + 0x4);
	const u32 body_size = rd_u32_le(in_prodinfo + 0x8);
	const u32 offset_stop = body_size + 0x40;

	/* Rewrite CRC and SHA fields that are within valid range */
	for (u32 idx = 0; idx < FIELD_COUNT; idx++)
	{
		const pi_field_desc_t *f = &g_fields[idx];

		if (f->offset > offset_stop)
			continue;

		if (version <= 0x7 && f->offset >= 0x3D70)
			continue;

		if (f->type == PI_ITEM_CRC)
		{
			u8 expected[0x20];
			if (f->length > sizeof(expected))
				return PI_ERR_INTERNAL;

			int rc = compute_expected_crc_field(out_prodinfo, in_size, f, expected);
			if (rc != PI_OK) {
				log_printf(true, LOG_ERR, LOG_MSG_ERROR_PRODINFO_CRC_VERIF, f->offset);
				return rc;
			}

			memcpy(out_prodinfo + f->offset, expected, f->length);
		}
		else if (f->type == PI_ITEM_SHA)
		{
			u8 expected[32];
			u32 data_off = f->sha_data_offset;
			u32 data_size = f->sha_data_size;

			if (f->offset == 0x0020) /* BodyHash */
				data_size = body_size;
			else if (f->offset == 0x12E0) /* SslCertificate hash */
				data_size = rd_u32_le(out_prodinfo + 0x0AD0);

			int rc = compute_expected_sha256(out_prodinfo, in_size, data_off, data_size, expected);
			if (rc != PI_OK) {
				log_printf(true, LOG_ERR, LOG_MSG_ERROR_PRODINFO_SHA_VERIF, f->offset);
				return rc;
			}

			memcpy(out_prodinfo + f->offset, expected, 32);
		}
		else
		{
			/* copy already done */
		}
	}

	/* Finally recompute BodyHash (SHA256 over calibration data at 0x40, size BodySize) and write at 0x20.
	 * In python rewrite_prodinfo_hashes does this at the end to ensure internal SHA fields are already updated.
	 */
	u8 body_hash[32];
	if (0x40 + body_size > in_size)
		return PI_ERR_OOB;
	se_sha_hash_256_oneshot(body_hash, out_prodinfo + 0x40, body_size);
	memcpy(out_prodinfo + 0x20, body_hash, 32);

	return PI_OK;
}

int prodinfo_verify_or_rewrite_hashes(const u8 *in_prodinfo, u32 in_size, prodinfo_verify_report_t *out_report, u8 *out_prodinfo, u32 out_size) {
	bool rewrite = (out_prodinfo != NULL);
	if (!in_prodinfo)
		return PI_ERR_ARG;
	if (in_size < 0x40)
		return PI_ERR_OOB;
	if (rewrite && out_size < in_size)
		return PI_ERR_OOB;

	if (out_report) {
		out_report->prodinfo_version = 0;
		out_report->body_size = 0;
		out_report->offset_stop = 0;
		out_report->crc_errors = 0;
		out_report->sha_errors = 0;
	}

	/* Copy input first */
	if (rewrite) memcpy(out_prodinfo, in_prodinfo, in_size);

	/* Check magic */
	for (u32 i = 0; i < 4; i++)
		if (in_prodinfo[i] != k_magic_cal0[i]) {
			log_printf(true, LOG_ERR, LOG_MSG_ERROR_PRODINFO_MAGIC_READ);
			return PI_ERR_BAD_MAGIC;
		}

	const u32 version = rd_u32_le(in_prodinfo + 0x4);
	const u32 body_size = rd_u32_le(in_prodinfo + 0x8);
	const u32 offset_stop = body_size + 0x40;

	if (out_report)
	{
		out_report->prodinfo_version = version;
		out_report->body_size = body_size;
		out_report->offset_stop = offset_stop;
	}

	/* Rewrite CRC and SHA fields that are within valid range */
	for (u32 idx = 0; idx < FIELD_COUNT; idx++) {
		const pi_field_desc_t *f = &g_fields[idx];

		if (f->offset > offset_stop)
			continue;
		/* Python skips items >= 0x3D70 if version <= 0x7 */
		if (version <= 0x7 && f->offset >= 0x3D70)
			continue;

		if (f->type == PI_ITEM_CRC)
		{
			u8 expected[0x20];
			if (f->length > sizeof(expected))
				return PI_ERR_INTERNAL;

			int rc = compute_expected_crc_field(in_prodinfo, in_size, f, expected);
			if (rc != PI_OK) {
				log_printf(true, LOG_ERR, LOG_MSG_ERROR_PRODINFO_CRC_VERIF, f->offset);
				return rc;
			}

			/* Compare */
			/*
			u32 mismatch = 0;
			for (u32 i = 0; i < f->length; i++) {
				if (in_prodinfo[f->offset + i] != expected[i]) {
					mismatch = 1;
					break;
				}
			}
			*/
			u32 mismatch = (memcmp(in_prodinfo + f->offset, expected, f->length) != 0);
			if (mismatch) {
				if (rewrite) {
					memcpy(out_prodinfo + f->offset, expected, f->length);
				} else {
					log_printf(true, LOG_ERR, LOG_MSG_ERROR_PRODINFO_CRC_COMPARE, f->offset);
				}
				if (out_report) out_report->crc_errors++;
			}
		} else if (f->type == PI_ITEM_SHA) {
			if (rewrite && f->offset == 0x0020)
				continue;
			/* SHA size is always 0x20 */
			u8 expected[32];
			u32 data_off = f->sha_data_offset;
			u32 data_size = f->sha_data_size;

			if (f->offset == 0x0020) /* BodyHash */
				data_size = body_size;
			else if (f->offset == 0x12E0) /* SslCertificate hash */
				data_size = rd_u32_le(in_prodinfo + 0x0AD0);

			int rc = compute_expected_sha256(in_prodinfo, in_size, data_off, data_size, expected);
			if (rc != PI_OK) {
				log_printf(true, LOG_ERR, LOG_MSG_ERROR_PRODINFO_SHA_VERIF, f->offset);
				return rc;
			}

			/*
			u32 mismatch = 0;
			for (u32 i = 0; i < 32; i++) {
				if (in_prodinfo[f->offset + i] != expected[i]) {
					mismatch = 1;
					break;
				}
			}
			*/
			u32 mismatch = (memcmp(in_prodinfo + f->offset, expected, 32) != 0);
			if (mismatch) {
				if (rewrite) {
					memcpy(out_prodinfo + f->offset, expected, 32);
				} else {
					log_printf(true, LOG_ERR, LOG_MSG_ERROR_PRODINFO_SHA_COMPARE, f->offset);
				}
				if (out_report) out_report->sha_errors++;
			}
		} else {
			/* Data field, no need to do anything */
		}
	}

	/* Finally recompute BodyHash (SHA256 over calibration data at 0x40, size BodySize) and write at 0x20.
	 * In python rewrite_prodinfo_hashes does this at the end to ensure internal SHA fields are already updated.
	 */
	if (rewrite) {
		u8 body_hash[32];
		if (0x40 + body_size > in_size)
			return PI_ERR_OOB;
		se_sha_hash_256_oneshot(body_hash, out_prodinfo + 0x40, body_size);
		memcpy(out_prodinfo + 0x20, body_hash, 32);
	}

	return PI_OK;
}

/*
int prodinfo_get_infos_text(const u8 *prodinfo, u32 prodinfo_size, char *out_text, u32 out_text_size)
{
	if (!prodinfo || !out_text || out_text_size == 0)
		return PI_ERR_ARG;

	u32 pos = 0;
	out_text[0] = 0;

	// minimal validation
	if (prodinfo_size < 0x40)
		return PI_ERR_OOB;
	for (u32 i = 0; i < 4; i++)
		if (prodinfo[i] != k_magic_cal0[i])
			return PI_ERR_BAD_MAGIC;

	const u32 version = rd_u32_le(prodinfo + 0x4);
	const u32 body_size = rd_u32_le(prodinfo + 0x8);
	const u32 offset_stop = body_size + 0x40;

	for (u32 idx = 0; idx < FIELD_COUNT; idx++)
	{
		const pi_field_desc_t *f = &g_fields[idx];

		if (f->offset > offset_stop)
			continue;
		if (version <= 0x7 && f->offset >= 0x3D70)
			continue;

		// Description labels: we keep them short & stable (offset-based)
		char label[64];
		s_printf(label, "Field @0x%X len 0x%X", (unsigned)f->offset, (unsigned)f->length);

		if (pos < out_text_size)
		{
			int w = s_printf(out_text + pos, "%s :\n", label);
			if (w <= 0) return PI_ERR_TEXT;
			pos += (u32)w;
		}
		else return PI_ERR_TEXT;

		if (f->type == PI_ITEM_DATA)
		{
			// dump raw bytes
			for (u32 i = 0; i < f->length; i++)
			{
				if (pos + 2 >= out_text_size) return PI_ERR_TEXT;
				int w = s_printf(out_text + pos, "%02X", prodinfo[f->offset + i]);
				if (w <= 0) return PI_ERR_TEXT;
				pos += (u32)w;
			}
		}
		else if (f->type == PI_ITEM_CRC)
		{
			for (u32 i = 0; i < f->length; i++)
			{
				if (pos + 2 >= out_text_size) return PI_ERR_TEXT;
				int w = s_printf(out_text + pos, "%02X", prodinfo[f->offset + i]);
				if (w <= 0) return PI_ERR_TEXT;
				pos += (u32)w;
			}
		}
		else // SHA
		{
			for (u32 i = 0; i < 32; i++)
			{
				if (pos + 2 >= out_text_size) return PI_ERR_TEXT;
				int w = s_printf(out_text + pos, "%02X", prodinfo[f->offset + i]);
				if (w <= 0) return PI_ERR_TEXT;
				pos += (u32)w;
			}
		}

		if (pos + 2 >= out_text_size) return PI_ERR_TEXT;
		out_text[pos++] = '\n';
		out_text[pos++] = '\n';
		out_text[pos] = 0;
	}

	return PI_OK;
}
*/

u32 pi_off(prodinfo_field_id_t id)
{
	u32 o = 0;
	if (prodinfo_field_get_offset_size(id, &o, NULL, NULL) != PI_OK)
		return 0;
	return o;
}

u32 pi_len(prodinfo_field_id_t id)
{
	u32 s = 0;
	if (prodinfo_field_get_offset_size(id, NULL, &s, NULL) != PI_OK)
		return 0;
	return s;
}
