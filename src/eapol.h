/*
 *
 *  Wireless daemon for Linux
 *
 *  Copyright (C) 2013-2014  Intel Corporation. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include <asm/byteorder.h>
#include <linux/types.h>

enum eapol_protocol_version {
	EAPOL_PROTOCOL_VERSION_2001	= 1,
	EAPOL_PROTOCOL_VERSION_2004	= 2,
};

/*
 * 802.1X-2010: Table 11-5—Descriptor Type value assignments
 * The WPA key type of 254 comes from somewhere else.  Seems it is a legacy
 * value that might still be used by older implementations
 */
enum eapol_descriptor_type {
	EAPOL_DESCRIPTOR_TYPE_RC4	= 1,
	EAPOL_DESCRIPTOR_TYPE_80211	= 2,
	EAPOL_DESCRIPTOR_TYPE_WPA	= 254,
};

enum eapol_key_descriptor_version {
	EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_MD5_ARC4	= 1,
	EAPOL_KEY_DESCRIPTOR_VERSION_HMAC_SHA1_AES	= 2,
	EAPOL_KEY_DESCRIPTOR_VERSION_AES_128_CMAC_AES	= 3,
};

struct eapol_sm;

struct eapol_header {
	uint8_t protocol_version;
	uint8_t packet_type;
	__be16 packet_len;
} __attribute__ ((packed));

struct eapol_frame {
	struct eapol_header header;
	uint8_t data[0];
} __attribute__ ((packed));

struct eapol_key {
	struct eapol_header header;
	uint8_t descriptor_type;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	bool key_mic:1;
	bool secure:1;
	bool error:1;
	bool request:1;
	bool encrypted_key_data:1;
	bool smk_message:1;
	uint8_t reserved2:2;
	uint8_t key_descriptor_version:3;
	bool key_type:1;
	uint8_t wpa_key_id:2; /* Bits 4-5 reserved in RSN, Key ID in WPA */
	bool install:1;
	bool key_ack:1;
#elif defined (__BIG_ENDIAN_BITFIELD)
	uint8_t reserved2:2;
	bool smk_message:1;
	bool encrypted_key_data:1;
	bool request:1;
	bool error:1;
	bool secure:1;
	bool key_mic:1;
	bool key_ack:1;
	bool install:1;
	uint8_t wpa_key_id:2; /* Bits 4-5 reserved in RSN, Key ID in WPA */
	bool key_type:1;
	uint8_t key_descriptor_version:3;
#else
#error  "Please fix <asm/byteorder.h>"
#endif

	__be16 key_length;
	__be64 key_replay_counter;
	uint8_t key_nonce[32];
	uint8_t eapol_key_iv[16];
	uint8_t key_rsc[8];
	uint8_t reserved[8];
	uint8_t key_mic_data[16];
	__be16 key_data_len;
	uint8_t key_data[0];
} __attribute__ ((packed));

typedef int (*eapol_tx_packet_func_t)(uint32_t ifindex, const uint8_t *aa,
				const uint8_t *spa,
				const struct eapol_frame *ef,
				void *user_data);
typedef bool (*eapol_get_nonce_func_t)(uint8_t nonce[]);
typedef void (*eapol_install_tk_func_t)(uint32_t ifindex, const uint8_t *aa,
					const uint8_t *tk, uint32_t cipher,
					void *user_data);
typedef void (*eapol_install_gtk_func_t)(uint32_t ifindex, uint8_t key_index,
					const uint8_t *gtk, uint8_t gtk_len,
					const uint8_t *rsc, uint8_t rsc_len,
					uint32_t cipher, void *user_data);
typedef void (*eapol_deauthenticate_func_t)(uint32_t ifindex, const uint8_t *aa,
						const uint8_t *spa,
						uint16_t reason_code,
						void *user_data);

bool eapol_calculate_mic(const uint8_t *kck, const struct eapol_key *frame,
				uint8_t *mic);
bool eapol_verify_mic(const uint8_t *kck, const struct eapol_key *frame);

uint8_t *eapol_decrypt_key_data(const uint8_t *kek,
				const struct eapol_key *frame,
				size_t *decrypted_size);

const struct eapol_key *eapol_key_validate(const uint8_t *frame, size_t len);

bool eapol_verify_ptk_1_of_4(const struct eapol_key *ek);
bool eapol_verify_ptk_2_of_4(const struct eapol_key *ek);
bool eapol_verify_ptk_3_of_4(const struct eapol_key *ek, bool is_wpa);
bool eapol_verify_ptk_4_of_4(const struct eapol_key *ek, bool is_wpa);
bool eapol_verify_gtk_1_of_2(const struct eapol_key *ek, bool is_wpa);
bool eapol_verify_gtk_2_of_2(const struct eapol_key *ek, bool is_wpa);

struct eapol_key *eapol_create_ptk_2_of_4(
				enum eapol_protocol_version protocol,
				enum eapol_key_descriptor_version version,
				uint64_t key_replay_counter,
				const uint8_t snonce[],
				size_t extra_len,
				const uint8_t *extra_data,
				bool is_wpa);

struct eapol_key *eapol_create_ptk_4_of_4(
				enum eapol_protocol_version protocol,
				enum eapol_key_descriptor_version version,
				uint64_t key_replay_counter,
				bool is_wpa);

struct eapol_key *eapol_create_gtk_2_of_2(
				enum eapol_protocol_version protocol,
				enum eapol_key_descriptor_version version,
				uint64_t key_replay_counter,
				bool is_wpa, uint8_t wpa_key_id);

void __eapol_rx_packet(uint32_t ifindex, const uint8_t *spa, const uint8_t *aa,
			const uint8_t *frame, size_t len);

void __eapol_set_tx_packet_func(eapol_tx_packet_func_t func);
void __eapol_set_get_nonce_func(eapol_get_nonce_func_t func);
void __eapol_set_protocol_version(enum eapol_protocol_version version);
void __eapol_set_install_tk_func(eapol_install_tk_func_t func);
void __eapol_set_install_gtk_func(eapol_install_gtk_func_t func);
void __eapol_set_deauthenticate_func(eapol_deauthenticate_func_t func);

struct eapol_sm *eapol_sm_new();
void eapol_sm_free(struct eapol_sm *sm);

void eapol_sm_set_supplicant_address(struct eapol_sm *sm, const uint8_t *spa);
void eapol_sm_set_authenticator_address(struct eapol_sm *sm, const uint8_t *aa);
void eapol_sm_set_pmk(struct eapol_sm *sm, const uint8_t *pmk);
void eapol_sm_set_8021x_config(struct eapol_sm *sm,
				struct l_settings *settings);
void eapol_sm_set_ap_rsn(struct eapol_sm *sm, const uint8_t *rsn_ie,
				size_t len);
bool eapol_sm_set_own_rsn(struct eapol_sm *sm, const uint8_t *rsn_ie,
				size_t len);
void eapol_sm_set_ap_wpa(struct eapol_sm *sm, const uint8_t *wpa_ie,
				size_t len);
bool eapol_sm_set_own_wpa(struct eapol_sm *sm, const uint8_t *wpa_ie,
				size_t len);
void eapol_sm_set_user_data(struct eapol_sm *sm, void *user_data);
void eapol_sm_set_tx_user_data(struct eapol_sm *sm, void *user_data);

uint32_t eapol_sm_get_pairwise_cipher(struct eapol_sm *sm);
uint32_t eapol_sm_get_group_cipher(struct eapol_sm *sm);
const uint8_t *eapol_sm_get_own_ie(struct eapol_sm *sm, size_t *out_ie_len);

struct l_io *eapol_open_pae(uint32_t index);

void eapol_start(uint32_t ifindex, struct eapol_sm *sm);
void eapol_cancel(uint32_t ifindex);

bool eapol_init();
bool eapol_exit();
