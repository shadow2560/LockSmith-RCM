/* prodinfo_rewrite.h - Nintendo Switch payload utility (BDK/Hekate style)
 *
 * Converted from prodinfo_rewrite.py (shadow256) - GPLv3
 * Operates on memory buffers (u8*) instead of files.
 */

#ifndef _PRODINFO_REWRITE_H_
#define _PRODINFO_REWRITE_H_

#include <utils/types.h>

typedef enum prodinfo_field_id {
	PI_F_MagicNumber = 0,
	PI_F_Version = 1,
	PI_F_BodySize = 2,
	PI_F_Model = 3,
	PI_F_UpdateCount = 4,
	PI_F_BeginFileCrc16 = 5,
	PI_F_BodyHash = 6,
	PI_F_ConfigurationId1 = 7,
	PI_F_ConfigurationId1Crc16 = 8,
	PI_F_Reserved0 = 9,
	PI_F_WlanCountryCodesNum = 10,
	PI_F_WlanCountryCodesLastIndex = 11,
	PI_F_WlanCountryCodes = 12,
	PI_F_WlanCountryCodesCrc16 = 13,
	PI_F_WlanMacAddress = 14,
	PI_F_WlanMacAddressCrc16 = 15,
	PI_F_Reserved1 = 16,
	PI_F_BdAddress = 17,
	PI_F_BdAddressCrc16 = 18,
	PI_F_Reserved2 = 19,
	PI_F_AccelerometerOffset = 20,
	PI_F_AccelerometerOffsetCrc16 = 21,
	PI_F_AccelerometerScale = 22,
	PI_F_AccelerometerScaleCrc16 = 23,
	PI_F_GyroscopeOffset = 24,
	PI_F_GyroscopeOffsetCrc16 = 25,
	PI_F_GyroscopeScale = 26,
	PI_F_GyroscopeScaleCrc16 = 27,
	PI_F_SerialNumber = 28,
	PI_F_SerialNumberCrc16 = 29,
	PI_F_EccP256DeviceKey = 30,
	PI_F_EccP256DeviceKeyCrc16 = 31,
	PI_F_EccP256DeviceCertificate = 32,
	PI_F_EccP256DeviceCertificateCrc16 = 33,
	PI_F_EccB233DeviceKey = 34,
	PI_F_EccB233DeviceKeyCrc16 = 35,
	PI_F_EccB233DeviceCertificate = 36,
	PI_F_EccB233DeviceCertificateCrc16 = 37,
	PI_F_EccP256ETicketKey = 38,
	PI_F_EccP256ETicketKeyCrc16 = 39,
	PI_F_EccP256ETicketCertificate = 40,
	PI_F_EccP256ETicketCertificateCrc16 = 41,
	PI_F_EccB233ETicketKey = 42,
	PI_F_EccB233ETicketKeyCrc16 = 43,
	PI_F_EccB233ETicketCertificate = 44,
	PI_F_EccB233ETicketCertificateCrc16 = 45,
	PI_F_SslKey = 46,
	PI_F_SslKeyCrc16 = 47,
	PI_F_SslCertificateSize = 48,
	PI_F_SslCertificateSizeCrc16 = 49,
	PI_F_SslCertificate = 50,
	PI_F_SslCertificateHash = 51,
	PI_F_RandomNumber = 52,
	PI_F_RandomNumberHash = 53,
	PI_F_GameCardKey = 54,
	PI_F_GameCardKeyCrc16 = 55,
	PI_F_GameCardCertificate = 56,
	PI_F_GameCardCertificateHash = 57,
	PI_F_Rsa2048ETicketKey = 58,
	PI_F_Rsa2048ETicketKeyCrc16 = 59,
	PI_F_Rsa2048ETicketCertificate = 60,
	PI_F_Rsa2048ETicketCertificateCrc16 = 61,
	PI_F_BatteryLot = 62,
	PI_F_BatteryLotCrc16 = 63,
	PI_F_SpeakerCalibrationValue = 64,
	PI_F_SpeakerCalibrationValueCrc16 = 65,
	PI_F_RegionCode = 66,
	PI_F_RegionCodeCrc16 = 67,
	PI_F_AmiiboKey = 68,
	PI_F_AmiiboKeyCrc16 = 69,
	PI_F_AmiiboEcqvCertificate = 70,
	PI_F_AmiiboEcqvCertificateCrc16 = 71,
	PI_F_AmiiboEcdsaCertificate = 72,
	PI_F_AmiiboEcdsaCertificateCrc16 = 73,
	PI_F_AmiiboEcqvBlsKey = 74,
	PI_F_AmiiboEcqvBlsKeyCrc16 = 75,
	PI_F_AmiiboEcqvBlsCertificate = 76,
	PI_F_AmiiboEcqvBlsCertificateCrc16 = 77,
	PI_F_AmiiboEcqvBlsRootCertificate = 78,
	PI_F_AmiiboEcqvBlsRootCertificateCrc16 = 79,
	PI_F_ProductModel = 80,
	PI_F_ProductModelCrc16 = 81,
	PI_F_HomeMenuSchemeMainColorVariation = 82,
	PI_F_HomeMenuSchemeMainColorVariationCrc16 = 83,
	PI_F_LcdBacklightBrightnessMapping = 84,
	PI_F_LcdBacklightBrightnessMappingCrc16 = 85,
	PI_F_ExtendedEccB233DeviceKey = 86,
	PI_F_ExtendedEccB233DeviceKeyCrc16 = 87,
	PI_F_ExtendedEccP256ETicketKey = 88,
	PI_F_ExtendedEccP256ETicketKeyCrc16 = 89,
	PI_F_ExtendedEccB233ETicketKey = 90,
	PI_F_ExtendedEccB233ETicketKeyCrc16 = 91,
	PI_F_ExtendedRsa2048ETicketKey = 92,
	PI_F_ExtendedRsa2048ETicketKeyCrc16 = 93,
	PI_F_ExtendedSslKey = 94,
	PI_F_ExtendedSslKeyCrc16 = 95,
	PI_F_ExtendedGameCardKey = 96,
	PI_F_ExtendedGameCardKeyCrc16 = 97,
	PI_F_LcdVendorId = 98,
	PI_F_LcdVendorIdCrc16 = 99,
	PI_F_ExtendedRsa2048DeviceKey = 100,
	PI_F_ExtendedRsa2048DeviceKeyCrc16 = 101,
	PI_F_Rsa2048DeviceCertificate = 102,
	PI_F_Rsa2048DeviceCertificateCrc16 = 103,
	PI_F_UsbTypeCPowerSourceCircuitVersion = 104,
	PI_F_UsbTypeCPowerSourceCircuitVersionCrc16 = 105,
	PI_F_HomeMenuSchemeSubColor = 106,
	PI_F_HomeMenuSchemeSubColorCrc16 = 107,
	PI_F_HomeMenuSchemeBezelColor = 108,
	PI_F_HomeMenuSchemeBezelColorCrc16 = 109,
	PI_F_HomeMenuSchemeMainColor1 = 110,
	PI_F_HomeMenuSchemeMainColor1Crc16 = 111,
	PI_F_HomeMenuSchemeMainColor2 = 112,
	PI_F_HomeMenuSchemeMainColor2Crc16 = 113,
	PI_F_HomeMenuSchemeMainColor3 = 114,
	PI_F_HomeMenuSchemeMainColor3Crc16 = 115,
	PI_F_AnalogStickModuleTypeL = 116,
	PI_F_AnalogStickModuleTypeLCrc16 = 117,
	PI_F_AnalogStickModelParameterL = 118,
	PI_F_AnalogStickModelParameterLCrc16 = 119,
	PI_F_AnalogStickFactoryCalibrationL = 120,
	PI_F_AnalogStickFactoryCalibrationLCrc16 = 121,
	PI_F_AnalogStickModuleTypeR = 122,
	PI_F_AnalogStickModuleTypeRCrc16 = 123,
	PI_F_AnalogStickModelParameterR = 124,
	PI_F_AnalogStickModelParameterRCrc16 = 125,
	PI_F_AnalogStickFactoryCalibrationR = 126,
	PI_F_AnalogStickFactoryCalibrationRCrc16 = 127,
	PI_F_ConsoleSixAxisSensorModuleType = 128,
	PI_F_ConsoleSixAxisSensorModuleTypeCrc16 = 129,
	PI_F_ConsoleSixAxisSensorHorizontalOffset = 130,
	PI_F_ConsoleSixAxisSensorHorizontalOffsetCrc16 = 131,
	PI_F_BatteryVersion = 132,
	PI_F_BatteryVersionCrc16 = 133,
	PI_F_Reserved3 = 134,
	PI_F_HomeMenuSchemeModel = 135,
	PI_F_HomeMenuSchemeModelCrc16 = 136,
	PI_F_ConsoleSixAxisSensorMountType = 137,
	PI_F_ConsoleSixAxisSensorMountTypeCrc16 = 138,
	PI_F__COUNT = 139
} prodinfo_field_id_t;

/* Error codes */
#define PI_OK              0
#define PI_ERR_ARG        -1
#define PI_ERR_OOB        -2
#define PI_ERR_BAD_MAGIC  -3
#define PI_ERR_BAD_DESC   -4
#define PI_ERR_INTERNAL   -5
#define PI_ERR_TEXT       -6

typedef struct {
	u32 prodinfo_version;
	u32 body_size;
	u32 offset_stop;
	u32 crc_errors;
	u32 sha_errors;
} prodinfo_verify_report_t;

typedef enum {
	PI_ITEM_DATA = 0,
	PI_ITEM_CRC  = 1,
	PI_ITEM_SHA  = 2
} pi_item_type_t;

typedef struct {
	pi_item_type_t type;
	u32 offset;
	u32 length;

	/* CRC-specific */
	u32 crc_pad;          /* number of padding bytes inside CRC field */
	u32 crc_src_offset;   /* where to take the source bytes for CRC input (special: 0xFFFFFFFF means "use previous data field bytes") */
	u32 crc_src_length;   /* length of source bytes (for CRC input) */

	/* SHA-specific */
	u32 sha_data_offset;  /* begin of data to hash */
	u32 sha_data_size;    /* size of data to hash */
} pi_field_desc_t;

extern const pi_field_desc_t g_fields[];

#define FIELD_COUNT ((u32)(sizeof(g_fields)/sizeof(g_fields[0])))

/* Verify CRC16 and SHA256 fields inside PRODINFO.
 * - out_report may be NULL.
 * Returns PI_OK if executed, even if errors were found (see report).
 * For strict success, check report->crc_errors and report->sha_errors == 0.
 */
int prodinfo_verify_hashes(const u8 *prodinfo, u32 prodinfo_size, prodinfo_verify_report_t *out_report);

/* Rewrite all CRC16 and SHA256 fields into out_prodinfo.
 * - out_prodinfo can be same as in_prodinfo for in-place update.
 * - out_size must be >= in_size.
 */
int prodinfo_rewrite_hashes(const u8 *in_prodinfo, u32 in_size, u8 *out_prodinfo, u32 out_size);

/* Verify or rewrite all CRC16 and SHA256 fields into out_prodinfo.
 * For verify, pass NULL for out_prodinfo.
 * - out_report may be NULL.
 * Returns PI_OK if executed, even if errors were found (see report).
 * For strict success with verify, check report->crc_errors and report->sha_errors == 0.
 * - out_prodinfo can be same as in_prodinfo for in-place update for rewrite.
 * - out_size must be >= in_size for rewrite, else could be 0.
 * Report is also generated during rewrite if needed.
 * Verifications are done only on in_prodinfo, even for rewrite.
 */
int prodinfo_verify_or_rewrite_hashes(const u8 *in_prodinfo, u32 in_size, prodinfo_verify_report_t *out_report, u8 *out_prodinfo, u32 out_size);

/* Field metadata accessors */
int prodinfo_field_get_offset_size(prodinfo_field_id_t id, u32 *out_offset, u32 *out_size, int *out_is_data);

/* Write only into DATA fields.
 * - Writes min(in_size, field_size) bytes to prodinfo[field_offset..]
 * - Returns PI_ERR_BAD_DESC if field is not DATA.
 */
int prodinfo_write_data(u8 *prodinfo, u32 prodinfo_size, prodinfo_field_id_t id, const void *in, u32 in_size);

/* Convenience: fill DATA field with zeroes */
int prodinfo_zero_data(u8 *prodinfo, u32 prodinfo_size, prodinfo_field_id_t id);

/* Dump all fields (data/crc/sha) as text hex into out_text.
 * This is the buffer-based equivalent of get_infos in python.
 * Caller must provide a sufficiently large output buffer.
 */
// int prodinfo_get_infos_text(const u8 *prodinfo, u32 prodinfo_size, char *out_text, u32 out_text_size);

u32 pi_off(prodinfo_field_id_t id);
u32 pi_len(prodinfo_field_id_t id);

u16 rd_u16_le(const u8 *p);
u32 rd_u32_le(const u8 *p);

#define PI_PTR(buf, fid)   ((const u8 *)(buf) + pi_off(fid))
#define PI_U32(buf, fid)   rd_u32_le(PI_PTR(buf, fid))
#define CAL0_IV_PTR(buf, fid)   (PI_PTR(buf, fid))
#define CAL0_KEY_PTR(buf, fid)  (PI_PTR(buf, fid) + 0x10)

#endif