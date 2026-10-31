/* SPDX-License-Identifier: <SPDX License Expression> */
/*****************************************************************************
 *
 * Copyright (c) 2012 - 2024 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_UTILS_80211_H__
#define __SLSI_UTILS_80211_H__

#define SLSI_WLAN_AKM_SUITE_SAE_EXT_KEY       SUITE(0x000FAC, 24)
#define SLSI_WLAN_AKM_SUITE_FT_SAE_EXT_KEY    SUITE(0x000FAC, 25)

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0))
#define WLAN_AKM_SUITE_OWE                    SUITE(0x000FAC, 18)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
#define WLAN_AKM_SUITE_FILS_SHA256            SUITE(0x000FAC, 14)
#define WLAN_AKM_SUITE_FILS_SHA384            SUITE(0x000FAC, 15)
#define WLAN_AKM_SUITE_FT_FILS_SHA256         SUITE(0x000FAC, 16)
#define WLAN_AKM_SUITE_FT_FILS_SHA384         SUITE(0x000FAC, 17)
#endif

static inline bool slsi_is_wpa3_support(const u8 *rsnie, size_t len)
{
	int pos, akm_suite_count, rsn_len;
	int i;
	u32 akm;

	if (!rsnie)
		return false;

	if (len < 2 || len < rsnie[1])
		return false;
	/* Calculate the position of AKM suite count in RSNIE
	 * RSNIE TAG(1 byte) + length(1 byte) + version(2 byte) + Group cipher suite(4 bytes)
	 * Pairwise Cipher Suite Count(2) + pairwise suite count * 4.
	 */
	if (rsnie[1] < 10)
		return false;
	pos = 8 + 2 + ((rsnie[8] | (rsnie[9] << 8)) * 4);
	if ((pos + 2) > (rsnie[1] + 2))
		return false;

	akm_suite_count = rsnie[pos] | (rsnie[pos + 1] << 8);
	rsn_len = pos + akm_suite_count * 4;
	if (rsn_len > (rsnie[1] + 2))
		return false;

	/* Add AKM suite count in pos */
	pos += 2;
	for (i = 0; i < akm_suite_count; i++) {
		akm = __cpu_to_be32(*(u32 *)&rsnie[pos]);
		if (akm == WLAN_AKM_SUITE_SAE || akm == WLAN_AKM_SUITE_FT_OVER_SAE ||
		    akm == SLSI_WLAN_AKM_SUITE_SAE_EXT_KEY || akm == SLSI_WLAN_AKM_SUITE_FT_SAE_EXT_KEY)
			return true;
		pos += 4;
	}

	return false;
}

static inline bool slsi_is_owe_support(const u8 *rsnie, int len)
{
	int pos, akm_suite_count, rsn_len;
	int i;
	u32 akm;

	if (!rsnie)
		return false;

	if (len < 2 || len < rsnie[1])
		return false;
	/* Calculate the position of AKM suite count in RSNIE
	 * RSNIE TAG(1 byte) + length(1 byte) + version(2 byte) + Group cipher suite(4 bytes)
	 * Pairwise Cipher Suite Count(2) + pairwise suite count * 4.
	 */
	if (rsnie[1] < 10)
		return false;
	pos = 8 + 2 + ((rsnie[8] | (rsnie[9] << 8)) * 4);
	if ((pos + 2) > (rsnie[1] + 2))
		return false;

	akm_suite_count = rsnie[pos] | (rsnie[pos + 1] << 8);
	rsn_len = pos + akm_suite_count * 4;
	if (rsn_len > (rsnie[1] + 2))
		return false;

	/* Add AKM suite count in pos */
	pos += 2;
	for (i = 0; i < akm_suite_count; i++) {
		akm = __cpu_to_be32(*(u32 *)&rsnie[pos]);
		if (akm == WLAN_AKM_SUITE_OWE)
			return true;
		pos += 4;
	}
	return false;
}

static inline bool slsi_is_fils_akm(const u8 *rsnie, int len)
{
	int pos, akm_suite_count, rsn_len;
	int i;
	u32 akm;

	if (!rsnie)
		return false;

	if (len < 2 || len < rsnie[1])
		return false;
	/* Calculate the position of AKM suite count in RSNIE
	 * RSNIE TAG(1 byte) + length(1 byte) + version(2 byte) + Group cipher suite(4 bytes)
	 * Pairwise Cipher Suite Count(2) + pairwise suite count * 4.
	 */
	if (rsnie[1] < 10)
		return false;
	pos = 8 + 2 + ((rsnie[8] | (rsnie[9] << 8)) * 4);
	if ((pos + 2) > (rsnie[1] + 2))
		return false;

	akm_suite_count = rsnie[pos] | (rsnie[pos + 1] << 8);
	rsn_len = pos + akm_suite_count * 4;
	if (rsn_len > (rsnie[1] + 2))
		return false;

	/* Add AKM suite count in pos */
	pos += 2;
	for (i = 0; i < akm_suite_count; i++) {
		akm = __cpu_to_be32(*(u32 *)&rsnie[pos]);
		if (akm == WLAN_AKM_SUITE_FILS_SHA256 || akm == WLAN_AKM_SUITE_FT_FILS_SHA256 ||
			akm == WLAN_AKM_SUITE_FILS_SHA384 || akm == WLAN_AKM_SUITE_FT_FILS_SHA384)
			return true;
		pos += 4;
	}
	return false;
}


#endif
