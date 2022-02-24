/******************************************************************************
 *
 * Copyright(c) 2015-2017 Intel Deutschland GmbH
 * Copyright(c) 2018        Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#ifndef SRC_CONNECTIVITY_WLAN_DRIVERS_THIRD_PARTY_INTEL_IWLWIFI_FW_API_NAN_H_
#define SRC_CONNECTIVITY_WLAN_DRIVERS_THIRD_PARTY_INTEL_IWLWIFI_FW_API_NAN_H_

#include <stdint.h>

#include "src/connectivity/wlan/drivers/third_party/intel/iwlwifi/fw/api/phy-ctxt.h"
#include "src/connectivity/wlan/drivers/third_party/intel/iwlwifi/platform/compiler.h"

/* TODO: read it from tlv */
#define NAN_MAX_SUPPORTED_DE_ENTRIES 10

/**
 * enum iwl_nan_subcmd_ids - Neighbor Awareness Networking (NaN) commands IDS
 */
enum iwl_nan_subcmd_ids {
  /**
   * @NAN_CONFIG_CMD:
   * &struct iwl_nan_cfg_cmd_v2 or &struct iwl_nan_cfg_cmd
   */
  NAN_CONFIG_CMD = 0,

  /**
   * @NAN_DISCOVERY_FUNC_CMD:
   * &struct iwl_nan_add_func_cmd or &struct iwl_nan_add_func_cmd_v2
   */
  NAN_DISCOVERY_FUNC_CMD = 0x1,

  /**
   * @NAN_FAW_CONFIG_CMD:
   * &struct iwl_nan_faw_config
   */
  NAN_FAW_CONFIG_CMD = 0x2,

  /**
   * @NAN_DISCOVERY_EVENT_NOTIF:
   * &struct iwl_nan_disc_evt_notify_v1 or
   * &struct iwl_nan_disc_evt_notify_v2
   */
  NAN_DISCOVERY_EVENT_NOTIF = 0xFD,

  /**
   * @NAN_DISCOVERY_TERMINATE_NOTIF:
   * &struct iwl_nan_de_term
   */
  NAN_DISCOVERY_TERMINATE_NOTIF = 0xFE,

  /**
   * @NAN_FAW_START_NOTIF:
   * Further availability window started.
   */
  NAN_FAW_START_NOTIF = 0xFF,
};

/**
 * struct iwl_fw_chan_avail - Defines per op. class channel availability
 *
 * @op_class: operating class
 * @chan_bitmap: channel bitmap
 */
struct iwl_fw_chan_avail {
  uint8_t op_class;
  __le16 chan_bitmap;
} __packed;

/**
 * struct iwl_nan_umac_cfg - NAN umac config parameters
 *
 * @action: one of the FW_CTXT_ACTION_*
 * @tsf_id: tsf id to use in beacons
 * @sta_id: STA used for NAN operations. Currently it is AUX STA.
 * @node_addr: our address
 * @reserved1: reserved
 * @master_pref: master preference value
 * @master_rand: random factor to override fw's decision (DEBUG)
 * @cluster_id: cluster id, if 0 the fw will choose one for us.
 * @dual_band: enables dual band operation.
 * @beacon_template_id: beacon template id for NAN
 */
struct iwl_nan_umac_cfg {
  __le32 action;
  __le32 tsf_id;
  __le32 sta_id;
  uint8_t node_addr[6];
  __le16 reserved1;
  uint8_t master_pref;
  uint8_t master_rand;
  __le16 cluster_id;
  __le32 dual_band;
  __le32 beacon_template_id;
} __packed; /* _NAN_UMAC_CONFIG_CMD_API_S_VER_1 */

/**
 * struct iwl_nan_testbed_cfg - NAN testbed specific config parameters
 *
 * @chan24: override default 2.4GHz channel (DEBUG)
 * @chan52: override default 5.2GHz channel (DEBUG)
 * @hop_count: fake hop count (DEBUG)
 * @op_bands: band bit mask (DEBUG)
 * @warmup_timer: warmup_timer in us (DEBUG)
 * @custom_tsf: fake tsf value (DEBUG)
 * @action_delay: usecs to delay SDFs (DEBUG)
 */
struct iwl_nan_testbed_cfg {
  uint8_t chan24;
  uint8_t chan52;
  uint8_t hop_count;
  uint8_t op_bands;
  __le32 warmup_timer;
  __le64 custom_tsf;
  __le32 action_delay;
} __packed; /* NAN_TEST_BED_SPECIFIC_CONFIG_S_VER_1 */

/*
 * struct iwl_nan_nan2_cfg - NAN2 specific configuration
 *
 * @cdw: committed DW interval as defined in NAN2 spec (NAN2)
 * @op_mode: operation mode as defined in NAN2 spec (NAN2)
 * @pot_avail_len: length of pot_avail array (NAN2)
 * @pot_avail: potential availability per op. class (NAN2)
 */
struct iwl_nan_nan2_cfg {
  __le16 cdw;
  uint8_t op_mode;
  uint8_t pot_avail_len;
  struct iwl_fw_chan_avail pot_avail[20];
} __packed; /* NAN_CONFIG_CMD_API_S_VER_1 */

/**
 * struct iwl_nan_cfg_cmd - NAN configuration command
 *
 * This command starts/stops/modifies NAN sync engine.
 *
 * @umac_cfg: umac specific configuration
 * @tb_cfg: testbed specific configuration
 * @nan2_cfg: nan2 specific configuration
 */
struct iwl_nan_cfg_cmd {
  struct iwl_nan_umac_cfg umac_cfg;
  struct iwl_nan_testbed_cfg tb_cfg;
  /* NAN 2 specific configuration */
  struct iwl_nan_nan2_cfg nan2_cfg;
} __packed; /* NAN_CONFIG_CMD_API_S_VER_1 */

/**
 * struct iwl_nan_cfg_cmd_v2 - NAN configuration command, version 2
 *
 * This command starts/stops/modifies NAN sync engine.
 *
 * @umac_cfg: umac specific configuration
 * @tb_cfg: testbed specific configuration
 * @unavailable_slots: Force this amount of slots to be unavailable in potential
 *  map
 * @nan2_cfg: nan2 specific configuration
 */
struct iwl_nan_cfg_cmd_v2 {
  struct iwl_nan_umac_cfg umac_cfg;
  struct iwl_nan_testbed_cfg tb_cfg;
  __le32 unavailable_slots;
  /* NAN 2 specific configuration */
  struct iwl_nan_nan2_cfg nan2_cfg;
} __packed; /* NAN_CONFIG_CMD_API_S_VER_2 */

/* NAN DE function type */
enum iwl_fw_nan_func_type {
  IWL_NAN_DE_FUNC_PUBLISH = 0,
  IWL_NAN_DE_FUNC_SUBSCRIBE = 1,
  IWL_NAN_DE_FUNC_FOLLOW_UP = 2,

  /* keep last */
  IWL_NAN_DE_FUNC_NOT_VALID,
};

/* NAN DE function flags */
enum iwl_fw_nan_func_flags {
  IWL_NAN_DE_FUNC_FLAG_UNSOLICITED_OR_ACTIVE = BIT(0),
  IWL_NAN_DE_FUNC_FLAG_SOLICITED = BIT(1),
  IWL_NAN_DE_FUNC_FLAG_UNICAST = BIT(2),
  IWL_NAN_DE_FUNC_FLAG_CLOSE_RANGE = BIT(3),
  IWL_NAN_DE_FUNC_FLAG_FAW_PRESENT = BIT(4),
  IWL_NAN_DE_FUNC_FLAG_FAW_TYPE = BIT(5),
  IWL_NAN_DE_FUNC_FLAG_FAW_NOTIFY = BIT(6),
  IWL_NAN_DE_FUNC_FLAG_RAISE_EVENTS = BIT(7),
};

/**
 * struct iwl_nan_add_func_common - NAN add/remove function, common part
 *
 * @action: one of the FW_CTXT_ACTION_ADD/REMOVE
 * @instance_id: instance id of the DE function to remove
 * @type: enum %iwl_fw_nan_func_type
 * @service_id: service id
 * @flags: a combination of &enum iwl_fw_nan_func_flags
 * @flw_up_id: follow up id
 * @flw_up_req_id: follow up requestor id
 * @flw_up_addr: follow up destination address
 * @reserved1: reserved
 * @ttl: ttl in DW's or 0 for infinite
 * @faw_ci: &struct iwl_fw_channel_info for further availability
 * @faw_attrtype: further availability bitmap
 * @serv_info_len: length of service specific information
 * @srf_len: length of the srf
 * @rx_filter_len: length of rx filter
 * @tx_filter_len: length of tx filter
 * @dw_interval: awake dw interval
 */
struct iwl_nan_add_func_common {
  __le32 action;
  uint8_t instance_id;
  uint8_t type;
  uint8_t service_id[6];
  __le16 flags;
  uint8_t flw_up_id;
  uint8_t flw_up_req_id;
  uint8_t flw_up_addr[6];
  __le16 reserved1;
  __le32 ttl;
  struct iwl_fw_channel_info faw_ci;
  uint8_t faw_attrtype;
  uint8_t serv_info_len;
  uint8_t srf_len;
  uint8_t rx_filter_len;
  uint8_t tx_filter_len;
  uint8_t dw_interval;
} __packed; /* NAN_DISCO_FUNC_FIXED_CMD_API_S_VER_1 */

/**
 * struct iwl_nan_add_func_cmd_v2 - NAN add/remove function command, version 2
 *
 * @cmn: nan add function common part
 * @cipher_capa: Cipher suite information capabilities
 * @cipher_suite_id: Bitmap of the list of cipher suite IDs
 * @security_ctx_len: length of tx security context attributes
 * @sdea_ctrl: SDEA control field
 * @data: dw aligned fields: service_info, srf, rxFilter, txFilter,
 *  security_ctx
 */
struct iwl_nan_add_func_cmd_v2 {
  struct iwl_nan_add_func_common cmn;
  uint8_t cipher_capa;
  uint8_t cipher_suite_id;
  __le16 security_ctx_len;
  __le16 sdea_ctrl;
  uint8_t data[0];
} __packed; /* NAN_DISCO_FUNC_FIXED_CMD_API_S_VER_2 */

/**
 * struct iwl_nan_add_func_cmd - NAN add/remove function command
 *
 * @cmn: nan add function common part
 * @reserved: reserved
 * @data: dw aligned fields -service_info, srf, rxFilter, txFilter
 */
struct iwl_nan_add_func_cmd {
  struct iwl_nan_add_func_common cmn;
  uint8_t reserved[2];
  uint8_t data[0];
} __packed; /* NAN_DISCO_FUNC_FIXED_CMD_API_S_VER_1 */

enum iwl_nan_add_func_resp_status {
  IWL_NAN_DE_FUNC_STATUS_SUCCESSFUL,
  IWL_NAN_DE_FUNC_STATUS_INSUFFICIENT_ENTRIES,
  IWL_NAN_DE_FUNC_STATUS_INSUFFICIENT_MEMORY,
  IWL_NAN_DE_FUNC_STATUS_INVALID_INSTANCE,
  IWL_NAN_DE_FUNC_STATUS_UNSPECIFIED,
};

/**
 * struct iwl_nan_add_func_res - Add NAN function response
 *
 * @instance_id: assigned instance_id (if added)
 * @status: status of the command (&enum iwl_nan_add_func_resp_status)
 * @reserved: reserved
 */
struct iwl_nan_add_func_res {
  uint8_t instance_id;
  uint8_t status;
  __le16 reserved;
} __packed; /* NAN_DISCO_FUNC_CMD_API_S_VER_1 */

/* Shared key cipher suite with CCMP with a 128 bit TK */
#define IWL_NAN_DE_FUNC_CS_SK_CCM_128 BIT(0)

/* Shared key cipher suite with GCMP with a 256 bit TK */
#define IWL_NAN_DE_FUNC_CS_SK_GCM_256 BIT(1)

/**
 * enum iwl_nan_de_func_sdea_flags - NAN func SDEA flags
 * @IWL_NAN_DE_FUNC_SDEA_FSD_REQ: Further Service Discovery (FSD) is required
 *     for the service
 * @IWL_NAN_DE_FUNC_SDEA_FSD_GAS: GAS is used for FSD
 * @IWL_NAN_DE_FUNC_SDEA_DP_REQ: Data path required for the service
 * @IWL_NAN_DE_FUNC_SDEA_DP_MCAST: If data path is required, the type is
 *     multicast
 * @IWL_NAN_DE_FUNC_SDEA_DP_MCAST_M_TO_M:if multicast data path is required then
 *     it is many to many
 * @IWL_NAN_DE_FUNC_SDEA_QOS_REQ: QoS is required for the service
 * @IWL_NAN_DE_FUNC_SDEA_SEC_REQ: Security is required for the service
 * @IWL_NAN_DE_FUNC_SDEA_RANGIGN_REQ: Ranging is required prior to subscription
 *     to the service
 */
enum iwl_nan_de_func_sdea_flags {
  IWL_NAN_DE_FUNC_SDEA_FSD_REQ = BIT(0),
  IWL_NAN_DE_FUNC_SDEA_FSD_GAS = BIT(1),
  IWL_NAN_DE_FUNC_SDEA_DP_REQ = BIT(2),
  IWL_NAN_DE_FUNC_SDEA_DP_MCAST = BIT(3),
  IWL_NAN_DE_FUNC_SDEA_DP_MCAST_M_TO_M = BIT(4),
  IWL_NAN_DE_FUNC_SDEA_QOS_REQ = BIT(5),
  IWL_NAN_DE_FUNC_SDEA_SEC_REQ = BIT(6),
  IWL_NAN_DE_FUNC_SDEA_RANGIGN_REQ = BIT(7),
};

/**
 * struct iwl_nan_disc_evt_notify_v1 - NaN discovery event
 *
 * @peer_mac_addr: peer address
 * @reserved1: reserved
 * @type: Type of matching function
 * @instance_id: local matching instance id
 * @peer_instance: peer matching instance id
 * @service_info_len: service specific information length
 * @attrs_len: additional peer attributes length
 * @buf: service specific information followed by attributes
 */
struct iwl_nan_disc_evt_notify_v1 {
  uint8_t peer_mac_addr[6];
  __le16 reserved1;
  uint8_t type;
  uint8_t instance_id;
  uint8_t peer_instance;
  uint8_t service_info_len;
  __le32 attrs_len;
  uint8_t buf[0];
} __packed; /* NAN_DISCO_EVENT_NTFY_API_S_VER_1 */

/**
 * struct iwl_nan_sec_ctxt_info - NaN security context information
 *
 * @type: the security context type
 * @reserved: for alignment
 * @len: the length of the security context
 * @buf: security context data
 */
struct iwl_nan_sec_ctxt_info {
  uint8_t type;
  uint8_t reserved;
  __le16 len;
  uint8_t buf[0];
} __packed; /* NAN_DISCO_SEC_CTXT_ID_API_S_VER_1 */

/**
 * struct iwl_nan_disc_info - NaN match information
 *
 * @type: Type of matching function
 * @instance_id: local matching instance id
 * @peer_instance: peer matching instance id
 * @service_info_len: service specific information length
 * @sdea_control: bit mask of &enum iwl_nan_de_func_sdea_flags_
 * @sdea_service_info_len: sdea service specific information length
 * @sec_ctxt_len: security context information length
 * @cipher_suite_ids: bit mask of cipher suite IDs
 * @sdea_update_indicator: update indicator from the sdea attribute
 * @buf: service specific information followed by sdea service specific
 *     information and by security context information which encapsulates 0 or
 *     more iwl_nan_sec_ctxt_info entries.
 */
struct iwl_nan_disc_info {
  uint8_t type;
  uint8_t instance_id;
  uint8_t peer_instance;
  uint8_t service_info_len;
  __le16 sdea_control;
  __le16 sdea_service_info_len;
  __le16 sec_ctxt_len;
  uint8_t cipher_suite_ids;
  uint8_t sdea_update_indicator;
  uint8_t buf[0];
} __packed; /* NAN_DISCO_INFO_API_S_VER_1 */

/**
 * struct iwl_nan_disc_evt_notify_v2 - NaN discovery information
 *
 * @peer_mac_addr: peer address
 * @reserved1: reserved
 * @match_len: Length of the match data
 * @avail_attrs_len: Length of the availability attributes associated with the
 *     peer
 * @buf: aggregation of matches, each starting on a dword aligned address,
 *     followed by the peer availability attributes, which also start on a
 *     dword aligned address.
 */
struct iwl_nan_disc_evt_notify_v2 {
  uint8_t peer_mac_addr[6];
  __le16 reserved1;
  __le32 match_len;
  __le32 avail_attrs_len;
  uint8_t buf[0];
} __packed; /* NAN_DISCO_EVENT_NTFY_API_S_VER_2 */

/* NAN function termination reasons */
enum iwl_fw_nan_de_term_reason {
  IWL_NAN_DE_TERM_FAILURE = 0,
  IWL_NAN_DE_TERM_TTL_REACHED,
  IWL_NAN_DE_TERM_USER_REQUEST,
};

/**
 * struct iwl_nan_de_term - NAN function termination event
 *
 * @type: type of terminated function (&enum iwl_fw_nan_func_type)
 * @instance_id: instance id
 * @reason: termination reason (&enum iwl_fw_nan_de_term_reason)
 * @reserved1: reserved
 */
struct iwl_nan_de_term {
  uint8_t type;
  uint8_t instance_id;
  uint8_t reason;
  uint8_t reserved1;
} __packed; /* NAN_DISCO_TERM_NTFY_API_S_VER_1 */

enum iwl_fw_post_nan_type {
  IWL_NAN_POST_NAN_ATTR_WLAN = 0,
  IWL_NAN_POST_NAN_ATTR_P2P,
  IWL_NAN_POST_NAN_ATTR_IBSS,
  IWL_NAN_POST_NAN_ATTR_MESH,
  IWL_NAN_POST_NAN_ATTR_FURTHER_NAN,
};

enum iwl_fw_config_flags {
  NAN_FAW_FLAG_NOTIFY_HOST = BIT(0),
};

/**
 * struct iwl_nan_faw_config - NAN further availability configuration command
 *
 * @id_n_color: id and color of the mac used for further availability
 * @faw_ci: channel to be available on
 * @type: type of post NAN availability (enum %iwl_fw_post_nan_type)
 * @slots: number of 16TU slots to be available on (should be < 32)
 * @flags: NAN_FAW_FLAG_*
 * @op_class: operating class which corresponds to faw_ci
 */
struct iwl_nan_faw_config {
  __le32 id_n_color;
  struct iwl_fw_channel_info faw_ci;
  uint8_t type;
  uint8_t slots;
  uint8_t flags;
  uint8_t op_class;
} __packed; /* _NAN_DISCO_FAW_CMD_API_S_VER_1 */

#endif  // SRC_CONNECTIVITY_WLAN_DRIVERS_THIRD_PARTY_INTEL_IWLWIFI_FW_API_NAN_H_