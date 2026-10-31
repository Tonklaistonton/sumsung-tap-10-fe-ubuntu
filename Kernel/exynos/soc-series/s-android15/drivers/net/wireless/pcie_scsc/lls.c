/*****************************************************************************
 *
 * Copyright (c) 2014 - 2023 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include "dev.h"
#include "debug.h"
#include "mgt.h"
#include "mlme.h"
#include "netif.h"
#include "lls.h"

#ifdef CONFIG_SCSC_WLAN_DEBUG
static void slsi_lls_debug_dump_stats(struct slsi_dev *sdev, struct slsi_lls_radio_stat *radio_stat,
				      struct slsi_lls_iface_stat *iface_stat, struct slsi_lls_iface_ml_stat *iface_ml_stat,
				      u8 *buf, int buf_len, int num_of_radios)
{
	int i, j;

	for (i = 0; i < num_of_radios; i++) {
		SLSI_INFO(sdev, "radio_stat====\n");
		SLSI_INFO(sdev, "\tradio_id : %d, on_time : %d, tx_time : %d, rx_time : %d,"
			  "on_time_scan : %d, num_channels : %d\n", radio_stat->radio, radio_stat->on_time,
			  radio_stat->tx_time, radio_stat->rx_time, radio_stat->on_time_scan,
			  radio_stat->num_channels);
		for (j = 0; j < radio_stat->num_channels; j++) {
			SLSI_INFO(sdev, "\t\tchannels[%d].on_time: %d, channels[%d].cca_busy_time: %d\n",
				  j, radio_stat->channels[j].on_time, j, radio_stat->channels[j].cca_busy_time);
		}

		radio_stat = (struct slsi_lls_radio_stat *)((u8 *)radio_stat + sizeof(struct slsi_lls_radio_stat) +
			     (sizeof(struct slsi_lls_channel_stat) * radio_stat->num_channels));
	}

	if (iface_stat != NULL) {
		SLSI_INFO(sdev, "iface_stat====\n");
		SLSI_INFO(sdev, "info : (mode : %d, mac_addr : %pM, state : %d, roaming : %d\n",
			  iface_stat->info.mode, iface_stat->info.mac_addr, iface_stat->info.state, iface_stat->info.roaming);
		SLSI_INFO(sdev, "\tcapabilities : %d, ssid : %s, bssid : %pM, ap_country_str : [%d%d%d]\n",
			  iface_stat->info.capabilities, iface_stat->info.ssid, iface_stat->info.bssid,
			  iface_stat->info.ap_country_str[0], iface_stat->info.ap_country_str[1],
			  iface_stat->info.ap_country_str[2]);
		SLSI_INFO(sdev, "\ttime_slicing_duty_cycle_percent: %d)\trssi_data : %d, rssi_mgmt : %d\n",
			  iface_stat->info.time_slicing_duty_cycle_percent, iface_stat->rssi_data, iface_stat->rssi_mgmt);
		SLSI_INFO(sdev, "\tnum_peers %d\n", iface_stat->num_peers);

		for (i = 0; i < SLSI_LLS_AC_MAX; i++) {
			SLSI_INFO(sdev, "\t\tiface_stat->ac[%d].retries:%u", i, iface_stat->ac[i].retries);
			SLSI_INFO(sdev, "\t\tiface_stat->ac[%d].retries_short:%u", i, iface_stat->ac[i].retries_short);
			SLSI_INFO(sdev, "\t\tiface_stat->ac[%d].retries_long:%u", i, iface_stat->ac[i].retries_long);
		}

		for (i = 0; i < iface_stat->num_peers; i++) {
			SLSI_INFO(sdev, "\t\tpeer_info[%d].type %d\n", i, iface_stat->peer_info[i].type);
			SLSI_INFO(sdev, "\t\tpeer_info[%d].capabilities %d\n", i, iface_stat->peer_info[i].capabilities);
			SLSI_INFO(sdev, "\t\tpeer_info[%d].peer_mac_address %pM\n", i, iface_stat->peer_info[i].peer_mac_address);
			SLSI_INFO(sdev, "\t\tpeer_info[%d].num_rate %d\n", i, iface_stat->peer_info[i].num_rate);
		}
	}

	if (iface_ml_stat != NULL) {
		struct slsi_lls_link_stat *link_stat = iface_ml_stat->links;
		struct slsi_lls_peer_info *peer_info;
		int link_stat_len = (u8 *)&(iface_ml_stat->links->peer_info) - (u8 *)&(iface_ml_stat->links);
		SLSI_INFO(sdev, "iface_ml_stat====\n");
		SLSI_INFO(sdev, "\tiface %p\n", iface_ml_stat->iface);
		SLSI_INFO(sdev, "info : (mode : %d, mac_addr : %pM, state : %d, roaming : %d\n",
			  iface_ml_stat->info.mode, iface_ml_stat->info.mac_addr, iface_ml_stat->info.state, iface_ml_stat->info.roaming);
		SLSI_INFO(sdev, "\tcapabilities : %d, ssid : %s, bssid : %pM, ap_country_str : [%d%d%d]\n",
			  iface_ml_stat->info.capabilities, iface_ml_stat->info.ssid, iface_ml_stat->info.bssid,
			  iface_ml_stat->info.ap_country_str[0], iface_ml_stat->info.ap_country_str[1],
			  iface_ml_stat->info.ap_country_str[2]);
		SLSI_INFO(sdev, "\ttime_slicing_duty_cycle_percent: %d\n",
			  iface_ml_stat->info.time_slicing_duty_cycle_percent);
		SLSI_INFO(sdev, "\tnum_links : %d\n", iface_ml_stat->num_links);

		for (i = 0; i < iface_ml_stat->num_links; i++) {
			SLSI_INFO(sdev, "\tlinks[%d].link_id : %u, num_peers : %d\n", i, link_stat->link_id, link_stat->num_peers);
			SLSI_INFO(sdev, "\tlinks[%d].state : %u\n", i, link_stat->state);
			SLSI_INFO(sdev, "\ttime_slicing_duty_cycle_percent: %d)\trssi_data : %d, rssi_mgmt : %d\n",
				 link_stat->time_slicing_duty_cycle_percent, link_stat->rssi_data, link_stat->rssi_mgmt);

			for (j = 0; j < SLSI_LLS_AC_MAX; j++) {
				SLSI_INFO(sdev, "\t\tiface_ml_stat->links[%d].ac[%d].retries:%u", i, j, link_stat->ac[j].retries);
				SLSI_INFO(sdev, "\t\tiface_ml_stat->links[%d].ac[%d].retries_short:%u", i, j, link_stat->ac[j].retries_short);
				SLSI_INFO(sdev, "\t\tiface_ml_stat->links[%d].ac[%d].retries_long:%u", i, j, link_stat->ac[j].retries_long);
			}

			if (link_stat->num_peers > 0) {
				peer_info = link_stat->peer_info;
				for (j = 0; j < link_stat->num_peers; j++) {
					SLSI_INFO(sdev, "\t\tiface_ml_stat->links[%d].peer_info[%d].peer_mac_address %pM\n", i, j, peer_info->peer_mac_address);
					SLSI_INFO(sdev, "\t\tiface_ml_stat->links[%d].peer_info[%d].type %d\n", i, j, peer_info->type);
					SLSI_INFO(sdev, "\t\tiface_ml_stat->links[%d].peer_info[%d].capabilities %d\n", i, j, peer_info->capabilities);
					SLSI_INFO(sdev, "\t\tiface_ml_stat->links[%d].peer_info[%d].num_rate %d\n", i, j, peer_info->num_rate);
					peer_info = (struct slsi_lls_peer_info *)((u8 *)peer_info + sizeof(struct slsi_lls_peer_info));
				}
				link_stat = (struct slsi_lls_link_stat *)((u8 *)link_stat + link_stat_len
					    + (sizeof(struct slsi_lls_peer_info) * link_stat->num_peers));

			} else {
				link_stat = (struct slsi_lls_link_stat *)((u8 *)link_stat + sizeof(struct slsi_lls_link_stat));
			}
		}
	}

	SLSI_DBG_HEX(sdev, SLSI_GSCAN, buf, buf_len, "return buffer\n");
}
#endif

void slsi_lls_start_stats(struct slsi_dev *sdev, u32 mpdu_size_threshold, u32 aggr_stat_gathering)
{
	struct net_device        *net_dev = NULL;
	struct netdev_vif        *ndev_vif = NULL;
	int                      i;

	SLSI_MUTEX_LOCK(sdev->device_config_mutex);
	/* start Statistics measurements in Firmware */
	(void)slsi_mlme_start_link_stats_req(sdev, mpdu_size_threshold, aggr_stat_gathering);

	net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	if (net_dev) {
		ndev_vif = netdev_priv(net_dev);
		for (i = 0; i < SLSI_LLS_AC_MAX; i++) {
			ndev_vif->rx_packets[i] = 0;
			ndev_vif->tx_packets[i] = 0;
			ndev_vif->tx_no_ack[i] = 0;
		}
	}
	SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);
}

void slsi_lls_stop_stats(struct slsi_dev *sdev, u32 stats_clear_req_mask)
{
	struct net_device        *net_dev = NULL;
	struct netdev_vif        *ndev_vif = NULL;
	int                      i;

	SLSI_MUTEX_LOCK(sdev->device_config_mutex);
	(void)slsi_mlme_stop_link_stats_req(sdev, stats_clear_req_mask);
	net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	if (net_dev) {
		ndev_vif = netdev_priv(net_dev);
		for (i = 0; i < SLSI_LLS_AC_MAX; i++) {
			ndev_vif->rx_packets[i] = 0;
			ndev_vif->tx_packets[i] = 0;
			ndev_vif->tx_no_ack[i] = 0;
		}
	}
	SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);
}

static u32 slsi_lls_ie_to_cap(const u8 *ies, int ies_len)
{
	u32 capabilities = 0;
	const u8 *ie_data;
	const u8 *ie;
	int ie_len;

	if (!ies || ies_len == 0) {
		SLSI_ERR_NODEV("no ie[&%p %d]\n", ies, ies_len);
		return 0;
	}
	ie = cfg80211_find_ie(WLAN_EID_EXT_CAPABILITY, ies, ies_len);
	if (ie) {
		ie_len = ie[1];
		ie_data = &ie[2];
		if ((ie_len >= 4) && (ie_data[3] & SLSI_WLAN_EXT_CAPA3_INTERWORKING_ENABLED))
			capabilities |= SLSI_LLS_CAPABILITY_INTERWORKING;
		if ((ie_len >= 7) && (ie_data[6] & 0x01)) /* Bit48: UTF-8 ssid */
			capabilities |= SLSI_LLS_CAPABILITY_SSID_UTF8;
	}

	ie = cfg80211_find_vendor_ie(WLAN_OUI_WFA, SLSI_WLAN_OUI_TYPE_WFA_HS20_IND, ies, ies_len);
	if (ie)
		capabilities |= SLSI_LLS_CAPABILITY_HS20;
	return capabilities;
}

static void slsi_lls_iface_sta_stats(struct slsi_dev *sdev, struct netdev_vif *ndev_vif,
				     struct slsi_lls_iface_stat *iface_stat)
{
	int                       i;
	struct slsi_lls_interface_link_layer_info *lls_info = &iface_stat->info;
	enum slsi_lls_peer_type   peer_type;
	struct slsi_peer          *peer;
	const u8                  *ie_data, *ie;
	int                       ie_len;

	SLSI_DBG3(sdev, SLSI_GSCAN, "\n");

	if (ndev_vif->ifnum == SLSI_NET_INDEX_WLAN) {
		lls_info->mode = SLSI_LLS_INTERFACE_STA;
		peer_type = SLSI_LLS_PEER_AP;
	} else {
		lls_info->mode = SLSI_LLS_INTERFACE_P2P_CLIENT;
		peer_type = SLSI_LLS_PEER_P2P_GO;
	}

	switch (ndev_vif->sta.vif_status) {
	case SLSI_VIF_STATUS_CONNECTING:
		lls_info->state = SLSI_LLS_AUTHENTICATING;
		break;
	case SLSI_VIF_STATUS_CONNECTED:
		lls_info->state = SLSI_LLS_ASSOCIATED;
		break;
	default:
		lls_info->state = SLSI_LLS_DISCONNECTED;
	}
	lls_info->roaming = ndev_vif->sta.roam_in_progress ?
				SLSI_LLS_ROAMING_ACTIVE : SLSI_LLS_ROAMING_IDLE;

	iface_stat->info.capabilities = 0;
	lls_info->ssid[0] = 0;
	if (ndev_vif->sta.sta_bss) {
		ie = cfg80211_find_ie(WLAN_EID_SSID, ndev_vif->sta.sta_bss->ies->data,
				      ndev_vif->sta.sta_bss->ies->len);
		if (ie) {
			ie_len = ie[1];
			ie_data = &ie[2];
			memcpy(lls_info->ssid, ie_data, ie_len);
			lls_info->ssid[ie_len] = 0;
		}
		SLSI_ETHER_COPY(lls_info->bssid, ndev_vif->sta.sta_bss->bssid);
		ie = cfg80211_find_ie(WLAN_EID_COUNTRY, ndev_vif->sta.sta_bss->ies->data,
				      ndev_vif->sta.sta_bss->ies->len);
		if (ie) {
			ie_data = &ie[2];
			memcpy(lls_info->ap_country_str, ie_data, 3);
			iface_stat->peer_info[0].capabilities |= SLSI_LLS_CAPABILITY_COUNTRY;
		}
	}

	peer = ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]; /* connected AP */
	if (peer && peer->valid && peer->assoc_ie && peer->assoc_resp_ie) {
		iface_stat->info.capabilities |= slsi_lls_ie_to_cap(peer->assoc_ie->data, peer->assoc_ie->len);
		if (peer->capabilities & WLAN_CAPABILITY_PRIVACY) {
			iface_stat->peer_info[0].capabilities |= SLSI_LLS_CAPABILITY_PROTECTED;
			iface_stat->info.capabilities |= SLSI_LLS_CAPABILITY_PROTECTED;
		}
		if (peer->qos_enabled) {
			iface_stat->peer_info[0].capabilities |= SLSI_LLS_CAPABILITY_QOS;
			iface_stat->info.capabilities |= SLSI_LLS_CAPABILITY_QOS;
		}
		iface_stat->peer_info[0].capabilities |= slsi_lls_ie_to_cap(peer->assoc_resp_ie->data, peer->assoc_resp_ie->len);

		SLSI_ETHER_COPY(iface_stat->peer_info[0].peer_mac_address, peer->address);
		iface_stat->peer_info[0].type = peer_type;
		iface_stat->num_peers = 1;
	}

	for (i = MAP_AID_TO_QS(SLSI_TDLS_PEER_INDEX_MIN); i <= MAP_AID_TO_QS(SLSI_TDLS_PEER_INDEX_MAX); i++) {
		peer = ndev_vif->peer_sta_record[i];
		if (peer && peer->valid) {
			SLSI_ETHER_COPY(iface_stat->peer_info[iface_stat->num_peers].peer_mac_address, peer->address);
			iface_stat->peer_info[iface_stat->num_peers].type = SLSI_LLS_PEER_TDLS;
			if (peer->qos_enabled)
				iface_stat->peer_info[iface_stat->num_peers].capabilities |= SLSI_LLS_CAPABILITY_QOS;
			iface_stat->peer_info[iface_stat->num_peers].num_rate = 0;
			iface_stat->num_peers++;
		}
	}
}

static void slsi_lls_iface_ap_stats(struct slsi_dev *sdev, struct netdev_vif *ndev_vif, struct slsi_lls_iface_stat *iface_stat)
{
	enum slsi_lls_peer_type peer_type = SLSI_LLS_PEER_INVALID;
	struct slsi_peer        *peer;
	int                     i;
	struct net_device       *dev;

	SLSI_DBG3(sdev, SLSI_GSCAN, "\n");

	/* We are AP/GO, so we advertize our own country. */
	memcpy(iface_stat->info.ap_country_str, iface_stat->info.country_str, 3);

	if (ndev_vif->ifnum == SLSI_NET_INDEX_WLAN) {
		iface_stat->info.mode = SLSI_LLS_INTERFACE_SOFTAP;
		peer_type = SLSI_LLS_PEER_STA;
	} else if (ndev_vif->ifnum == SLSI_NET_INDEX_P2PX_SWLAN) {
		dev = sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN];
		if (SLSI_IS_VIF_INDEX_P2P_GROUP(sdev, ndev_vif)) {
			iface_stat->info.mode = SLSI_LLS_INTERFACE_P2P_GO;
			peer_type = SLSI_LLS_PEER_P2P_CLIENT;
		}
	}

	for (i = MAP_AID_TO_QS(SLSI_PEER_INDEX_MIN); i <= MAP_AID_TO_QS(SLSI_PEER_INDEX_MAX); i++) {
		peer = ndev_vif->peer_sta_record[i];
		if (peer && peer->valid) {
			SLSI_ETHER_COPY(iface_stat->peer_info[iface_stat->num_peers].peer_mac_address, peer->address);
			iface_stat->peer_info[iface_stat->num_peers].type = peer_type;
			iface_stat->peer_info[iface_stat->num_peers].num_rate = 0;
			if (peer->qos_enabled)
				iface_stat->peer_info[iface_stat->num_peers].capabilities = SLSI_LLS_CAPABILITY_QOS;
			iface_stat->num_peers++;
		}
	}

	memcpy(iface_stat->info.ssid, ndev_vif->ap.ssid, ndev_vif->ap.ssid_len);
	iface_stat->info.ssid[ndev_vif->ap.ssid_len] = 0;
	if (ndev_vif->ap.privacy)
		iface_stat->info.capabilities |= SLSI_LLS_CAPABILITY_PROTECTED;
	if (ndev_vif->ap.qos_enabled)
		iface_stat->info.capabilities |= SLSI_LLS_CAPABILITY_QOS;
}

#ifdef CONFIG_SCSC_WLAN_EHT
static void slsi_lls_iface_ml_sta_stats(struct slsi_dev *sdev, struct netdev_vif *ndev_vif,
					struct slsi_lls_iface_ml_stat *iface_ml_stat)
{
	struct slsi_lls_interface_link_layer_info *lls_info = &iface_ml_stat->info;
	const u8                  *ie_data, *ie;
	int                       ie_len;

	SLSI_DBG3(sdev, SLSI_GSCAN, "\n");
	if (ndev_vif->ifnum == SLSI_NET_INDEX_WLAN) {
		lls_info->mode = SLSI_LLS_INTERFACE_STA;
	} else {
		lls_info->mode = SLSI_LLS_INTERFACE_P2P_CLIENT;
	}

	switch (ndev_vif->sta.vif_status) {
	case SLSI_VIF_STATUS_CONNECTING:
		lls_info->state = SLSI_LLS_AUTHENTICATING;
		break;
	case SLSI_VIF_STATUS_CONNECTED:
		lls_info->state = SLSI_LLS_ASSOCIATED;
		break;
	default:
		lls_info->state = SLSI_LLS_DISCONNECTED;
	}
	lls_info->roaming = ndev_vif->sta.roam_in_progress ?
				SLSI_LLS_ROAMING_ACTIVE : SLSI_LLS_ROAMING_IDLE;

	iface_ml_stat->info.capabilities = 0;
	lls_info->ssid[0] = 0;
	lls_info->ap_country_str[0] = 0;
	if (ndev_vif->sta.sta_bss) {
		ie = cfg80211_find_ie(WLAN_EID_SSID, ndev_vif->sta.sta_bss->ies->data,
				      ndev_vif->sta.sta_bss->ies->len);
		if (ie) {
			ie_len = ie[1];
			ie_data = &ie[2];
			memcpy(lls_info->ssid, ie_data, ie_len);
			lls_info->ssid[ie_len] = 0;
		}
		SLSI_ETHER_COPY(lls_info->bssid, ndev_vif->sta.sta_bss->bssid);
		ie = cfg80211_find_ie(WLAN_EID_COUNTRY, ndev_vif->sta.sta_bss->ies->data,
				      ndev_vif->sta.sta_bss->ies->len);
		if (ie) {
			ie_data = &ie[2];
			memcpy(lls_info->ap_country_str, ie_data, 3);
		}
	}
}

static void slsi_lls_iface_ml_ap_stats(struct slsi_dev *sdev, struct netdev_vif *ndev_vif, struct slsi_lls_iface_ml_stat *iface_ml_stat)
{
	struct net_device       *dev;

	SLSI_DBG3(sdev, SLSI_GSCAN, "\n");

	/* We are AP/GO, so we advertize our own country. */
	memcpy(iface_ml_stat->info.ap_country_str, iface_ml_stat->info.country_str, 3);

	if (ndev_vif->ifnum == SLSI_NET_INDEX_WLAN) {
		iface_ml_stat->info.mode = SLSI_LLS_INTERFACE_SOFTAP;
	} else if (ndev_vif->ifnum == SLSI_NET_INDEX_P2PX_SWLAN) {
		dev = sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN];
		if (SLSI_IS_VIF_INDEX_P2P_GROUP(sdev, ndev_vif))
			iface_ml_stat->info.mode = SLSI_LLS_INTERFACE_P2P_GO;
	}

	memcpy(iface_ml_stat->info.ssid, ndev_vif->ap.ssid, ndev_vif->ap.ssid_len);
	iface_ml_stat->info.ssid[ndev_vif->ap.ssid_len] = 0;
	if (ndev_vif->ap.privacy)
		iface_ml_stat->info.capabilities |= SLSI_LLS_CAPABILITY_PROTECTED;
	if (ndev_vif->ap.qos_enabled)
		iface_ml_stat->info.capabilities |= SLSI_LLS_CAPABILITY_QOS;
}

static void slsi_lls_iface_ml_sta_stats_by_link(struct slsi_dev *sdev, struct netdev_vif *ndev_vif,
						struct slsi_lls_interface_link_layer_info *lls_info,
						struct slsi_lls_link_stat *link_stat)
{
	int                            i;
	enum slsi_lls_peer_type        peer_type;
	struct slsi_peer               *peer;
	struct slsi_lls_peer_info      *peer_info = link_stat->peer_info;

	SLSI_DBG3(sdev, SLSI_GSCAN, "\n");
	if (ndev_vif->ifnum == SLSI_NET_INDEX_WLAN)
		peer_type = SLSI_LLS_PEER_AP;
	else
		peer_type = SLSI_LLS_PEER_P2P_GO;

	if (lls_info->ap_country_str[0])
		link_stat->peer_info[0].capabilities |= SLSI_LLS_CAPABILITY_COUNTRY;

	peer = ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]; /* connected AP */
	if (peer && peer->valid && peer->assoc_ie && peer->assoc_resp_ie) {
		lls_info->capabilities |= slsi_lls_ie_to_cap(peer->assoc_ie->data, peer->assoc_ie->len);
		if (peer->capabilities & WLAN_CAPABILITY_PRIVACY) {
			link_stat->peer_info[0].capabilities |= SLSI_LLS_CAPABILITY_PROTECTED;
			lls_info->capabilities |= SLSI_LLS_CAPABILITY_PROTECTED;
		}
		if (peer->qos_enabled) {
			link_stat->peer_info[0].capabilities |= SLSI_LLS_CAPABILITY_QOS;
			lls_info->capabilities |= SLSI_LLS_CAPABILITY_QOS;
		}
		link_stat->peer_info[0].capabilities |= slsi_lls_ie_to_cap(peer->assoc_resp_ie->data, peer->assoc_resp_ie->len);

		SLSI_ETHER_COPY(link_stat->peer_info[0].peer_mac_address, peer->address);
		link_stat->peer_info[0].type = peer_type;
		link_stat->num_peers = 1;
	}

	for (i = MAP_AID_TO_QS(SLSI_TDLS_PEER_INDEX_MIN); i <= MAP_AID_TO_QS(SLSI_TDLS_PEER_INDEX_MAX); i++) {
		peer = ndev_vif->peer_sta_record[i];
		if (peer && peer->valid) {
			SLSI_ETHER_COPY(peer_info->peer_mac_address, peer->address);
			peer_info->type = SLSI_LLS_PEER_TDLS;
			if (peer->qos_enabled)
				peer_info->capabilities |= SLSI_LLS_CAPABILITY_QOS;
			peer_info->num_rate = 0;
			link_stat->num_peers++;
			peer_info = (struct slsi_lls_peer_info *)((u8 *)peer_info + sizeof(struct slsi_lls_peer_info));
		}
	}
}

static void slsi_lls_iface_ml_ap_stats_by_link(struct slsi_dev *sdev, struct netdev_vif *ndev_vif, struct slsi_lls_link_stat *link_stat)
{
	enum slsi_lls_peer_type peer_type = SLSI_LLS_PEER_INVALID;
	struct slsi_peer               *peer;
	struct slsi_lls_peer_info      *peer_info = link_stat->peer_info;
	int                            i;

	SLSI_DBG3(sdev, SLSI_GSCAN, "\n");
	if (ndev_vif->ifnum == SLSI_NET_INDEX_WLAN) {
		peer_type = SLSI_LLS_PEER_STA;
	} else if (ndev_vif->ifnum == SLSI_NET_INDEX_P2PX_SWLAN) {
		if (SLSI_IS_VIF_INDEX_P2P_GROUP(sdev, ndev_vif))
			peer_type = SLSI_LLS_PEER_P2P_CLIENT;
	}

	for (i = MAP_AID_TO_QS(SLSI_PEER_INDEX_MIN); i <= MAP_AID_TO_QS(SLSI_PEER_INDEX_MAX); i++) {
		peer = ndev_vif->peer_sta_record[i];
		if (peer && peer->valid) {
			SLSI_ETHER_COPY(peer_info->peer_mac_address, peer->address);
			peer_info->type = peer_type;
			peer_info->num_rate = 0;
			if (peer->qos_enabled)
				peer_info->capabilities = SLSI_LLS_CAPABILITY_QOS;
			link_stat->num_peers++;
		}
		peer_info = (struct slsi_lls_peer_info *)((u8 *)peer_info + sizeof(struct slsi_lls_peer_info));
	}
}

static void slsi_lls_iface_ml_stat_fill_by_link(struct slsi_dev *sdev,
				     struct net_device *net_dev,
				     struct slsi_lls_link_stat *link_stat)
{
	int                       i;
	struct netdev_vif         *ndev_vif;
	struct slsi_mib_data      mibrsp = { 0, NULL };
	struct slsi_mib_value     *values = NULL;
	struct slsi_mib_get_entry get_values[] = { { SLSI_PSID_UNIFI_AC_SUCCESS, { SLSI_TRAFFIC_Q_VO + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_SUCCESS, { SLSI_TRAFFIC_Q_VI + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_SUCCESS, { SLSI_TRAFFIC_Q_BE + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_SUCCESS, { SLSI_TRAFFIC_Q_BK + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_RETRIES, { SLSI_TRAFFIC_Q_VO + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_RETRIES, { SLSI_TRAFFIC_Q_VI + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_RETRIES, { SLSI_TRAFFIC_Q_BE + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_RETRIES, { SLSI_TRAFFIC_Q_BK + 1, 0 } },
						   { SLSI_PSID_UNIFI_BEACON_RECEIVED, {0, 0} },
						   { SLSI_PSID_UNIFI_PS_LEAKY_AP, {0, 0} },
						   { SLSI_PSID_UNIFI_RSSI, {0, 0} },
						   { SLSI_PSID_UNIFI_AC_NO_ACKS, { SLSI_TRAFFIC_Q_VO + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_NO_ACKS, { SLSI_TRAFFIC_Q_VI + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_NO_ACKS, { SLSI_TRAFFIC_Q_BE + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_NO_ACKS, { SLSI_TRAFFIC_Q_BK + 1, 0 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_VO + 1, 1 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_VI + 1, 1 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_BE + 1, 1 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_BK + 1, 1 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_VO + 1, 2 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_VI + 1, 2 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_BE + 1, 2 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_BK + 1, 2 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_VO + 1, 3 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_VI + 1, 3 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_BE + 1, 3 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_BK + 1, 3 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_VO + 1, 4 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_VI + 1, 4 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_BE + 1, 4 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_BK + 1, 4 } },
						   { SLSI_PSID_UNIFI_LINK_STAT_RADIO_INDEX, {0, 0} } };

	if (!net_dev)
		return;

	ndev_vif = netdev_priv(net_dev);

	mibrsp.dataLength = 10 * sizeof(get_values) / sizeof(get_values[0]);
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);
	if (!mibrsp.data) {
		SLSI_ERR(sdev, "Cannot kmalloc %d bytes for interface MIBs\n", mibrsp.dataLength);
		goto exit;
	}

	values = slsi_read_mibs_by_link(sdev, net_dev, link_stat->link_id, get_values, sizeof(get_values) / sizeof(get_values[0]), &mibrsp);
	if (!values)
		goto exit;

	for (i = 0; i < SLSI_LLS_AC_MAX; i++) {
		if (values[i].type == SLSI_MIB_TYPE_UINT) {
			link_stat->ac[i].ac = slsi_fapi_to_android_traffic_q(i);
			link_stat->ac[i].tx_mpdu = values[i].u.uintValue;
			link_stat->ac[i].rx_mpdu = ndev_vif->rx_packets[i];
			if (values[i + 4].type != SLSI_MIB_TYPE_NONE) {
				SLSI_CHECK_TYPE(sdev, values[i + 4].type, SLSI_MIB_TYPE_UINT);
				if (values[i + 4].type == SLSI_MIB_TYPE_UINT)
					link_stat->ac[i].retries = values[i + 4].u.uintValue;
			} else {
				SLSI_ERR(sdev, "invalid type. iter:%d\n", i);
			}
			if (values[i + 11].type != SLSI_MIB_TYPE_NONE) {
				SLSI_CHECK_TYPE(sdev, values[i + 11].type, SLSI_MIB_TYPE_UINT);
				if (values[i + 11].type == SLSI_MIB_TYPE_UINT) {
					ndev_vif->tx_no_ack[i] = values[i + 11].u.uintValue;
					link_stat->ac[i].mpdu_lost = ndev_vif->tx_no_ack[i];
				}
			} else {
				SLSI_ERR(sdev, "invalid type. iter:%d\n", i);
			}
			if (values[i + 15].type != SLSI_MIB_TYPE_NONE) {
				SLSI_CHECK_TYPE(sdev, values[i + 15].type, SLSI_MIB_TYPE_UINT);
				if (values[i + 15].type == SLSI_MIB_TYPE_UINT)
					link_stat->ac[i].contention_time_max = values[i + 15].u.uintValue;
			} else {
				SLSI_ERR(sdev, "invalid type. iter:%d\n", i);
			}
			if (values[i + 19].type != SLSI_MIB_TYPE_NONE) {
				SLSI_CHECK_TYPE(sdev, values[i + 19].type, SLSI_MIB_TYPE_UINT);
				if (values[i + 19].type == SLSI_MIB_TYPE_UINT)
					link_stat->ac[i].contention_time_min = values[i + 19].u.uintValue;
			} else {
				SLSI_ERR(sdev, "invalid type. iter:%d\n", i);
			}
			if (values[i + 23].type != SLSI_MIB_TYPE_NONE) {
				SLSI_CHECK_TYPE(sdev, values[i + 23].type, SLSI_MIB_TYPE_UINT);
				if (values[i + 23].type == SLSI_MIB_TYPE_UINT)
					link_stat->ac[i].contention_time_avg = values[i + 23].u.uintValue;
			} else {
				SLSI_ERR(sdev, "invalid type. iter:%d\n", i);
			}
			if (values[i + 27].type != SLSI_MIB_TYPE_NONE) {
				SLSI_CHECK_TYPE(sdev, values[i + 27].type, SLSI_MIB_TYPE_UINT);
				if (values[i + 27].type == SLSI_MIB_TYPE_UINT)
					link_stat->ac[i].contention_num_samples = values[i + 27].u.uintValue;
			} else {
				SLSI_ERR(sdev, "invalid type. iter:%d\n", i);
			}

		} else {
			SLSI_WARN(sdev, "Nothing read thru MIB.\n");
			link_stat->ac[i].ac = SLSI_LLS_AC_MAX;
		}
	}

	if (values[8].type == SLSI_MIB_TYPE_UINT)
		link_stat->beacon_rx = values[8].u.uintValue;

	if (values[9].type == SLSI_MIB_TYPE_UINT) {
		link_stat->leaky_ap_detected = values[9].u.uintValue;
		link_stat->leaky_ap_guard_time = 5; /* 5 milli sec. As mentioned in lls document */
	}

	if (values[10].type == SLSI_MIB_TYPE_INT) {
		link_stat->rssi_data = values[10].u.intValue;
		link_stat->rssi_mgmt = values[10].u.intValue;
	}

	if (values[31].type == SLSI_MIB_TYPE_INT)
		link_stat->radio = values[31].u.intValue;

	link_stat->frequency = ndev_vif->sta.links[link_stat->link_id].freq;

	//if this link is being served using time slicing on a radio with one or more links
	//then the duty cycle assigned to this link in %.
	link_stat->time_slicing_duty_cycle_percent = 50;

exit:
	kfree(values);
	kfree(mibrsp.data);
}

static void slsi_lls_iface_ml_stat_fill(struct slsi_dev *sdev,
				     struct net_device *net_dev,
				     struct slsi_lls_iface_ml_stat *iface_ml_stat)
{
	struct netdev_vif         *ndev_vif;
	u8 *buf = (u8 *)iface_ml_stat->links;
	struct slsi_lls_link_stat *link_stat;
	struct mlo_link_info ml_info;
	int link_stat_len = (u8 *)&(iface_ml_stat->links->peer_info) - (u8 *)&(iface_ml_stat->links);
	int i, j, num_links = 0;

	iface_ml_stat->iface = NULL;
	iface_ml_stat->info.mode = SLSI_LLS_INTERFACE_UNKNOWN;
	SLSI_MUTEX_LOCK(sdev->device_config_mutex);
	iface_ml_stat->info.country_str[0] = sdev->device_config.domain_info.regdomain->alpha2[0];
	iface_ml_stat->info.country_str[1] = sdev->device_config.domain_info.regdomain->alpha2[1];
	SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);
	iface_ml_stat->info.country_str[2] = ' '; /* 3rd char of our country code is ASCII<space> */
	/* Set time_slicing_duty_cycle_percent to 100 temporarily until added fw interface */
	iface_ml_stat->info.time_slicing_duty_cycle_percent = 100;

	if (!net_dev)
		return;

	ndev_vif = netdev_priv(net_dev);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	if (!ndev_vif->activated) {
		SLSI_WARN(sdev, "VIF is not activated\n");
		SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
		return;
	}

	if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) {
		slsi_lls_iface_ml_sta_stats(sdev, ndev_vif, iface_ml_stat);
	} else if (ndev_vif->vif_type == FAPI_VIFTYPE_AP) {
		slsi_lls_iface_ml_ap_stats(sdev, ndev_vif, iface_ml_stat);
		SLSI_ETHER_COPY(iface_ml_stat->info.bssid, net_dev->dev_addr);
	}
	SLSI_ETHER_COPY(iface_ml_stat->info.mac_addr, net_dev->dev_addr);

	if ((ndev_vif->vif_type == FAPI_VIFTYPE_STATION) && (ndev_vif->sta.vif_status != SLSI_VIF_STATUS_CONNECTED)) {
		SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
		return;
	}

	if (slsi_mlme_get_mlo_link_state(sdev, net_dev, &ml_info) >= 0)
		num_links = ml_info.num_links;

	for (i = 0; i < MAX_NUM_MLD_LINKS; i++) {
		if (ndev_vif->sta.valid_links & BIT(i)) {
			link_stat = (struct slsi_lls_link_stat *)buf;
			link_stat->link_id = i;

			if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION)
				slsi_lls_iface_ml_sta_stats_by_link(sdev, ndev_vif, &iface_ml_stat->info, link_stat);
			else if (ndev_vif->vif_type == FAPI_VIFTYPE_AP)
				slsi_lls_iface_ml_ap_stats_by_link(sdev, ndev_vif, link_stat);

			slsi_lls_iface_ml_stat_fill_by_link(sdev, net_dev, link_stat);

			for (j = 0; j < num_links; j++) {
				if (ml_info.links[j].link_id == link_stat->link_id) {
					if (ml_info.links[j].link_state == 0)
						link_stat->state = WIFI_LINK_STATE_NOT_IN_USE;
					else if (ml_info.links[j].link_state > 0)
						link_stat->state = WIFI_LINK_STATE_IN_USE;
					else
						link_stat->state = WIFI_LINK_STATE_UNKNOWN;
					break;
				}
			}

			if (link_stat->num_peers > 0)
				buf = (u8 *)(buf + link_stat_len + (sizeof(struct slsi_lls_peer_info) * link_stat->num_peers));	//num_rate is 0.
			else
				buf = (u8 *)(buf + sizeof(struct slsi_lls_link_stat));

			iface_ml_stat->num_links++;
		}
	}

	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
}
#endif

static void slsi_lls_iface_stat_fill(struct slsi_dev *sdev,
				     struct net_device *net_dev,
				     struct slsi_lls_iface_stat *iface_stat)
{
	int                       i;
	struct netdev_vif         *ndev_vif;
	struct slsi_mib_data      mibrsp = { 0, NULL };
	struct slsi_mib_value     *values = NULL;
	struct slsi_mib_get_entry get_values[] = { { SLSI_PSID_UNIFI_AC_SUCCESS, { SLSI_TRAFFIC_Q_VO + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_SUCCESS, { SLSI_TRAFFIC_Q_VI + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_SUCCESS, { SLSI_TRAFFIC_Q_BE + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_SUCCESS, { SLSI_TRAFFIC_Q_BK + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_RETRIES, { SLSI_TRAFFIC_Q_VO + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_RETRIES, { SLSI_TRAFFIC_Q_VI + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_RETRIES, { SLSI_TRAFFIC_Q_BE + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_RETRIES, { SLSI_TRAFFIC_Q_BK + 1, 0 } },
						   { SLSI_PSID_UNIFI_BEACON_RECEIVED, {0, 0} },
						   { SLSI_PSID_UNIFI_PS_LEAKY_AP, {0, 0} },
						   { SLSI_PSID_UNIFI_RSSI, {0, 0} },
						   { SLSI_PSID_UNIFI_AC_NO_ACKS, { SLSI_TRAFFIC_Q_VO + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_NO_ACKS, { SLSI_TRAFFIC_Q_VI + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_NO_ACKS, { SLSI_TRAFFIC_Q_BE + 1, 0 } },
						   { SLSI_PSID_UNIFI_AC_NO_ACKS, { SLSI_TRAFFIC_Q_BK + 1, 0 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_VO + 1, 1 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_VI + 1, 1 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_BE + 1, 1 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_BK + 1, 1 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_VO + 1, 2 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_VI + 1, 2 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_BE + 1, 2 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_BK + 1, 2 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_VO + 1, 3 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_VI + 1, 3 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_BE + 1, 3 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_BK + 1, 3 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_VO + 1, 4 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_VI + 1, 4 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_BE + 1, 4 } },
						   { SLSI_PSID_UNIFI_CONTENTION_TIME, { SLSI_TRAFFIC_Q_BK + 1, 4 } } };

	iface_stat->iface = NULL;
	iface_stat->info.mode = SLSI_LLS_INTERFACE_UNKNOWN;
	SLSI_MUTEX_LOCK(sdev->device_config_mutex);
	iface_stat->info.country_str[0] = sdev->device_config.domain_info.regdomain->alpha2[0];
	iface_stat->info.country_str[1] = sdev->device_config.domain_info.regdomain->alpha2[1];
	SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);
	iface_stat->info.country_str[2] = ' '; /* 3rd char of our country code is ASCII<space> */
	/* Set time_slicing_duty_cycle_percent to 100 temporarily until added fw interface */
	iface_stat->info.time_slicing_duty_cycle_percent = 100;

	for (i = 0; i < SLSI_LLS_AC_MAX; i++)
		iface_stat->ac[i].ac = SLSI_LLS_AC_MAX;

	if (!net_dev)
		return;

	ndev_vif = netdev_priv(net_dev);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	if (!ndev_vif->activated)
		goto exit;

	if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) {
		slsi_lls_iface_sta_stats(sdev, ndev_vif, iface_stat);
	} else if (ndev_vif->vif_type == FAPI_VIFTYPE_AP) {
		slsi_lls_iface_ap_stats(sdev, ndev_vif, iface_stat);
		SLSI_ETHER_COPY(iface_stat->info.bssid, net_dev->dev_addr);
	}
	SLSI_ETHER_COPY(iface_stat->info.mac_addr, net_dev->dev_addr);

	if ((ndev_vif->vif_type == FAPI_VIFTYPE_STATION) && (ndev_vif->sta.vif_status != SLSI_VIF_STATUS_CONNECTED)) {
		SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
		return;
	}

	mibrsp.dataLength = 10 * sizeof(get_values) / sizeof(get_values[0]);
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);
	if (!mibrsp.data) {
		SLSI_ERR(sdev, "Cannot kmalloc %d bytes for interface MIBs\n", mibrsp.dataLength);
		goto exit;
	}

	values = slsi_read_mibs(sdev, net_dev, get_values, sizeof(get_values) / sizeof(get_values[0]), &mibrsp);
	if (!values)
		goto exit;

	for (i = 0; i < SLSI_LLS_AC_MAX; i++) {
		if (values[i].type == SLSI_MIB_TYPE_UINT) {
			iface_stat->ac[i].ac = slsi_fapi_to_android_traffic_q(i);
			iface_stat->ac[i].tx_mpdu = values[i].u.uintValue;
			iface_stat->ac[i].rx_mpdu = ndev_vif->rx_packets[i];
			if (values[i + 4].type != SLSI_MIB_TYPE_NONE) {
				SLSI_CHECK_TYPE(sdev, values[i + 4].type, SLSI_MIB_TYPE_UINT);
				if (values[i + 4].type == SLSI_MIB_TYPE_UINT)
					iface_stat->ac[i].retries = values[i + 4].u.uintValue;
			} else {
				SLSI_ERR(sdev, "invalid type. iter:%d", i);
			}
			if (values[i + 11].type != SLSI_MIB_TYPE_NONE) {
				SLSI_CHECK_TYPE(sdev, values[i + 11].type, SLSI_MIB_TYPE_UINT);
				if (values[i + 11].type == SLSI_MIB_TYPE_UINT) {
					ndev_vif->tx_no_ack[i] = values[i + 11].u.uintValue;
					iface_stat->ac[i].mpdu_lost = ndev_vif->tx_no_ack[i];
				}
			} else {
				SLSI_ERR(sdev, "invalid type. iter:%d", i);
			}
			if (values[i + 15].type != SLSI_MIB_TYPE_NONE) {
				SLSI_CHECK_TYPE(sdev, values[i + 15].type, SLSI_MIB_TYPE_UINT);
				if (values[i + 15].type == SLSI_MIB_TYPE_UINT)
					iface_stat->ac[i].contention_time_max = values[i + 15].u.uintValue;
			} else {
				SLSI_ERR(sdev, "invalid type. iter:%d", i);
			}
			if (values[i + 19].type != SLSI_MIB_TYPE_NONE) {
				SLSI_CHECK_TYPE(sdev, values[i + 19].type, SLSI_MIB_TYPE_UINT);
				if (values[i + 19].type == SLSI_MIB_TYPE_UINT)
					iface_stat->ac[i].contention_time_min = values[i + 19].u.uintValue;
			} else {
				SLSI_ERR(sdev, "invalid type. iter:%d", i);
			}
			if (values[i + 23].type != SLSI_MIB_TYPE_NONE) {
				SLSI_CHECK_TYPE(sdev, values[i + 23].type, SLSI_MIB_TYPE_UINT);
				if (values[i + 23].type == SLSI_MIB_TYPE_UINT)
					iface_stat->ac[i].contention_time_avg = values[i + 23].u.uintValue;
			} else {
				SLSI_ERR(sdev, "invalid type. iter:%d", i);
			}
			if (values[i + 27].type != SLSI_MIB_TYPE_NONE) {
				SLSI_CHECK_TYPE(sdev, values[i + 27].type, SLSI_MIB_TYPE_UINT);
				if (values[i + 27].type == SLSI_MIB_TYPE_UINT)
					iface_stat->ac[i].contention_num_samples = values[i + 27].u.uintValue;
			} else {
				SLSI_ERR(sdev, "invalid type. iter:%d", i);
			}
		}
	}

	if (values[8].type == SLSI_MIB_TYPE_UINT)
		iface_stat->beacon_rx = values[8].u.uintValue;

	if (values[9].type == SLSI_MIB_TYPE_UINT) {
		iface_stat->leaky_ap_detected = values[9].u.uintValue;
		iface_stat->leaky_ap_guard_time = 5; /* 5 milli sec. As mentioned in lls document */
	}

	if (values[10].type == SLSI_MIB_TYPE_INT) {
		iface_stat->rssi_data = values[10].u.intValue;
		iface_stat->rssi_mgmt = values[10].u.intValue;
	}

exit:
	kfree(values);
	kfree(mibrsp.data);
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
}

static void slsi_lls_channel_stat_fill(struct slsi_dev *sdev, struct net_device *dev,
				       struct slsi_lls_channel_stat *channel_stat)
{
	struct slsi_mib_data      mibrsp = { 0, NULL };
	struct slsi_mib_value     *values = NULL;
	struct slsi_mib_get_entry get_values[] = { { SLSI_PSID_UNIFI_RADIO_ON_TIME_STATION, { 0, 0 } },
						   { SLSI_PSID_UNIFI_CCA_BUSY_TIME_STATION, { 0, 0 } } };
	u32                       *channel_data[] = { &channel_stat->on_time, &channel_stat->cca_busy_time };
	int                       mib_count = ARRAY_SIZE(get_values);
	int                       i;

	/* Expect each mib length in response is <= 15 So assume 15 bytes for each MIB */
	mibrsp.dataLength = 15 * mib_count;
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);
	if (!mibrsp.data) {
		SLSI_ERR(sdev, "Cannot kmalloc %d bytes\n", mibrsp.dataLength);
		return;
	}

	values = slsi_read_mibs(sdev, dev, get_values, mib_count, &mibrsp);
	if (!values)
		goto exit_with_mibrsp;

	for (i = 0; i < mib_count; i++) {
		if (values[i].type != SLSI_MIB_TYPE_NONE) {
			SLSI_CHECK_TYPE(sdev, values[i].type, SLSI_MIB_TYPE_UINT);
			if (values[i].type == SLSI_MIB_TYPE_UINT)
				*channel_data[i] = values[i].u.uintValue;
			else
				goto exit_with_values;

		} else {
			SLSI_ERR(sdev, "invalid type. iter:%d", i);
		}
	}

exit_with_values:
	kfree(values);
exit_with_mibrsp:
	kfree(mibrsp.data);
}

static void slsi_lls_radio_stat_fill(struct slsi_dev *sdev, struct net_device *dev,
				     struct slsi_lls_radio_stat *radio_stat,
				     int max_chan_count, int radio_index, int band)
{
	struct netdev_vif         *ndev_vif;
	struct slsi_mib_data      mibrsp = { 0, NULL };
	struct slsi_mib_data      supported_chan_2g_5g = { 0, NULL };
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
	struct slsi_mib_data      supported_chan_6g = { 0, NULL };
#endif
	struct slsi_mib_value     *values = NULL;
	struct slsi_mib_get_entry get_values[] = { { SLSI_PSID_UNIFI_RADIO_SCAN_TIME, { radio_index, 0 } },
						   { SLSI_PSID_UNIFI_RADIO_RX_TIME, { radio_index, 0 } },
						   { SLSI_PSID_UNIFI_RADIO_TX_TIME, { radio_index, 0 } },
						   { SLSI_PSID_UNIFI_RADIO_ON_TIME, { radio_index, 0 } },
						   { SLSI_PSID_UNIFI_RADIO_ON_TIME_NAN, { radio_index, 0 } },
						   { SLSI_PSID_UNIFI_SUPPORTED_CHANNELS, { 1, 0 } }
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
						 , { SLSI_PSID_UNIFI_SUPPORTED_CHANNELS, { 2, 0 } }
#endif
						 };
	u32                       *radio_data[] = { &radio_stat->on_time_scan, &radio_stat->rx_time,
						    &radio_stat->tx_time, &radio_stat->on_time,
						    &radio_stat->on_time_nbd };
	int                       mib_count = ARRAY_SIZE(get_values);
	int                       i, j, chan_count, chan_start, k;

	ndev_vif = netdev_priv(dev);
	radio_stat->radio = radio_index;

	/* Expect each mib length in response is <= 15 So assume 15 bytes for each MIB */
	mibrsp.dataLength = 15 * mib_count;
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);
	if (mibrsp.data == NULL) {
		SLSI_ERR(sdev, "Cannot kmalloc %d bytes\n", mibrsp.dataLength);
		return;
	}

	values = slsi_read_mibs(sdev, NULL, get_values, mib_count, &mibrsp);
	if (!values)
		goto exit_with_mibrsp;

	for (i = 0; i < mib_count; i++) {
		if (values[i].type != SLSI_MIB_TYPE_NONE) {
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
			if (i < mib_count - 2) {
#else
			if (i < mib_count - 1) {
#endif
				SLSI_CHECK_TYPE(sdev, values[i].type, SLSI_MIB_TYPE_UINT);
				if (values[i].type == SLSI_MIB_TYPE_UINT)
					*radio_data[i] = values[i].u.uintValue;
			} else {
				SLSI_CHECK_TYPE(sdev, values[i].type, SLSI_MIB_TYPE_OCTET);
				if (values[i].type != SLSI_MIB_TYPE_OCTET) {
					SLSI_ERR(sdev, "Supported_Chan invalid type.");
					goto exit_with_values;
				}

#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
				if ((i + 2) == mib_count)
					supported_chan_2g_5g = values[i].u.octetValue;
				else if ((i + 1) == mib_count)
					supported_chan_6g = values[i].u.octetValue;
#else
				if ((i + 1) == mib_count)
					supported_chan_2g_5g = values[i].u.octetValue;
#endif
			}
		} else {
			SLSI_ERR(sdev, "invalid type. iter:%d", i);
		}
	}

	if ((band & BIT(0)) || (band & BIT(1))) {
		if (!supported_chan_2g_5g.data) {
			SLSI_ERR(sdev, "No data for 2.4GHz & 5GHz channel!");
			goto exit_with_values;
		}

		for (j = 0; j < supported_chan_2g_5g.dataLength / 2; j++) {
			struct slsi_lls_channel_info *radio_chan;

			chan_start = supported_chan_2g_5g.data[j * 2];
			chan_count = supported_chan_2g_5g.data[j * 2 + 1];
			if (radio_stat->num_channels + chan_count > max_chan_count)
				chan_count = max_chan_count - radio_stat->num_channels;
			if (chan_start == 1 && (band & BIT(0))) { /* for 2.4GHz */
				for (k = 0; k < chan_count; k++) {
					radio_chan = &radio_stat->channels[radio_stat->num_channels + k].channel;
					if (k + chan_start == 14)
						radio_chan->center_freq = 2484;
					else
						radio_chan->center_freq = 2407 + (chan_start + k) * 5;
					radio_chan->width = SLSI_LLS_CHAN_WIDTH_20;
					SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
					if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION &&
					    ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTED &&
					    ndev_vif->chan && ndev_vif->chan->hw_value == (chan_start + k)) {
						slsi_lls_channel_stat_fill(sdev, dev,
									   &radio_stat->channels[radio_stat->num_channels + k]);
					}
					SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
				}
				radio_stat->num_channels += chan_count;

			} else if (chan_start != 1 && (band & BIT(1))) {
				/* for 5GHz */
				for (k = 0; k < chan_count; k++) {
					radio_chan = &radio_stat->channels[radio_stat->num_channels + k].channel;
					radio_chan->center_freq = 5000 + (chan_start + (k * 4)) * 5;
					radio_chan->width = SLSI_LLS_CHAN_WIDTH_20;
					SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
					if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION &&
					    ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTED &&
					    ndev_vif->chan && ndev_vif->chan->hw_value == (chan_start + (k * 4))) {
						slsi_lls_channel_stat_fill(sdev, dev,
									   &radio_stat->channels[radio_stat->num_channels + k]);
					}
					SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
				}
				radio_stat->num_channels += chan_count;
			}
		}
	}

#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
	if (band & BIT(2)) {
		if (!supported_chan_6g.data) {
			SLSI_ERR(sdev, "No data for 6GHz channel!");
			goto exit_with_values;
		}

		for (j = 0; j < supported_chan_6g.dataLength / 2; j++) {
			struct slsi_lls_channel_info *radio_chan;

			chan_start = supported_chan_6g.data[j * 2];
			chan_count = supported_chan_6g.data[j * 2 + 1];
			if (radio_stat->num_channels + chan_count > max_chan_count)
				chan_count = max_chan_count - radio_stat->num_channels;

			/* for 6GHz */
			for (k = 0; k < chan_count; k++) {
				radio_chan = &radio_stat->channels[radio_stat->num_channels + k].channel;
				if (k == 0 && chan_start == 2) {
					radio_chan->center_freq = 5935;
				} else {
					if (k == 1 && chan_start == 2)
						chan_start = -3;
					radio_chan->center_freq = 5950 + (chan_start + (k * 4)) * 5;
				}
				radio_chan->width = SLSI_LLS_CHAN_WIDTH_20;
				SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
				if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION &&
				    ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTED &&
				    ndev_vif->chan && ndev_vif->chan->hw_value == (chan_start + (k * 4))) {
					slsi_lls_channel_stat_fill(sdev, dev,
								   &radio_stat->channels[radio_stat->num_channels + k]);
				}
				SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
			}
			radio_stat->num_channels += chan_count;
		}
	}
#endif
exit_with_values:
	kfree(values);
exit_with_mibrsp:
	kfree(mibrsp.data);
}

static int slsi_lls_fill(struct slsi_dev *sdev, u8 **src_buf)
{
	struct net_device          *net_dev = NULL;
	struct slsi_lls_radio_stat *radio_stat;
#ifdef CONFIG_SCSC_WLAN_DEBUG
	struct slsi_lls_radio_stat *radio_stat_temp;
#endif
	struct slsi_lls_iface_stat *iface_stat;
	int                        buf_len = 0;
	int                        max_chan_count = 0;
	u8                         *buf;
	int                        num_of_radios_supported;
	int i = 0;
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
	int radio_type[4] = {BIT(0), BIT(0), BIT(1) | BIT(2), BIT(1) | BIT(2)};
#else
	int radio_type[2] = {BIT(0), BIT(1)};
#endif

	if (sdev->lls_num_radio == 0) {
		SLSI_ERR(sdev, "Number of radios are zero for this platform\n");
		return -EIO;
	}
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
	num_of_radios_supported = ARRAY_SIZE(radio_type);
	max_chan_count = SLSI_MAX_SUPPORTED_CHANNELS_NUM * 2;
#else
	num_of_radios_supported = sdev->lls_num_radio;
	max_chan_count = SLSI_MAX_SUPPORTED_CHANNELS_NUM;
#endif
	net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	buf_len = (int)((num_of_radios_supported * sizeof(struct slsi_lls_radio_stat))
			+ sizeof(struct slsi_lls_iface_stat)
			+ sizeof(u8)
			+ (sizeof(struct slsi_lls_peer_info) * SLSI_ADHOC_PEER_CONNECTIONS_MAX)
			+ (sizeof(struct slsi_lls_channel_stat) * max_chan_count));

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf) {
		SLSI_ERR(sdev, "No mem. Size:%d\n", buf_len);
		return -ENOMEM;
	}
	buf[0] = num_of_radios_supported;
	*src_buf = buf;
	iface_stat = (struct slsi_lls_iface_stat *)(buf + sizeof(u8));
	slsi_lls_iface_stat_fill(sdev, net_dev, iface_stat);

	radio_stat = (struct slsi_lls_radio_stat *)(buf + sizeof(u8) + sizeof(struct slsi_lls_iface_stat)
						    + (sizeof(struct slsi_lls_peer_info) * iface_stat->num_peers));
#ifdef CONFIG_SCSC_WLAN_DEBUG
	radio_stat_temp = radio_stat;
#endif
	if (num_of_radios_supported == 1) {
		radio_type[0] = BIT(0) | BIT(1);
		slsi_lls_radio_stat_fill(sdev, net_dev, radio_stat, max_chan_count, 0, radio_type[0]);
		radio_stat = (struct slsi_lls_radio_stat *)((u8 *)radio_stat + sizeof(struct slsi_lls_radio_stat)
							    + (sizeof(struct slsi_lls_channel_stat) * radio_stat->num_channels));
	} else {
		for (i = 1; i <= num_of_radios_supported ; i++) {
			slsi_lls_radio_stat_fill(sdev, net_dev, radio_stat, max_chan_count, i, radio_type[i - 1]);
			radio_stat = (struct slsi_lls_radio_stat *)((u8 *)radio_stat + sizeof(struct slsi_lls_radio_stat)
								    + (sizeof(struct slsi_lls_channel_stat) * radio_stat->num_channels));
		}
	}

#ifdef CONFIG_SCSC_WLAN_DEBUG
	if (slsi_dev_llslogs_supported())
		slsi_lls_debug_dump_stats(sdev, radio_stat_temp, iface_stat, NULL, buf, buf_len, num_of_radios_supported);
#endif
	return buf_len;
}

#ifdef CONFIG_SCSC_WLAN_EHT
static int slsi_lls_fill_mlo(struct slsi_dev *sdev, u8 **src_buf)
{
	struct net_device             *net_dev = NULL;
	struct slsi_lls_radio_stat    *radio_stat;
#ifdef CONFIG_SCSC_WLAN_DEBUG
	struct slsi_lls_radio_stat    *radio_stat_temp;
#endif
	struct slsi_lls_iface_ml_stat *iface_ml_stat;
	int                           buf_len = 0;
	int                           max_chan_count = 0;
	u8                            *buf;
	int                           num_of_radios_supported;
	int                           i = 0;
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
	int radio_type[4] = {BIT(0), BIT(0), BIT(1) | BIT(2), BIT(1) | BIT(2)};
#else
	int radio_type[2] = {BIT(0), BIT(1)};
#endif

	if (sdev->lls_num_radio == 0) {
		SLSI_ERR(sdev, "Number of radios are zero for this platform\n");
		return -EIO;
	}
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
	num_of_radios_supported = ARRAY_SIZE(radio_type);
	max_chan_count = SLSI_MAX_SUPPORTED_CHANNELS_NUM * 2;
#else
	num_of_radios_supported = sdev->lls_num_radio;
	max_chan_count = SLSI_MAX_SUPPORTED_CHANNELS_NUM;
#endif
	net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	buf_len = (int)((num_of_radios_supported * sizeof(struct slsi_lls_radio_stat))
			+ (sizeof(struct slsi_lls_iface_ml_stat)
			+ (sizeof(struct slsi_lls_link_stat) * MAX_NUM_MLD_LINKS)
			+ sizeof(u8)
			+ (sizeof(struct slsi_lls_peer_info) * SLSI_ADHOC_PEER_CONNECTIONS_MAX * MAX_NUM_MLD_LINKS)
			+ (sizeof(struct slsi_lls_channel_stat) * max_chan_count)));

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf) {
		SLSI_ERR(sdev, "No mem. Size:%d\n", buf_len);
		return -ENOMEM;
	}
	buf[0] = num_of_radios_supported;
	*src_buf = buf;
	iface_ml_stat = (struct slsi_lls_iface_ml_stat *)(buf + sizeof(u8));
	slsi_lls_iface_ml_stat_fill(sdev, net_dev, iface_ml_stat);

	if (iface_ml_stat->num_links <= 0) {
		SLSI_ERR(sdev, "No links to take stats.\n");
		return 0;
	}

	radio_stat = (struct slsi_lls_radio_stat *)(buf + sizeof(u8) + sizeof(struct slsi_lls_iface_ml_stat)
		     + (sizeof(struct slsi_lls_link_stat) * MAX_NUM_MLD_LINKS)
		     + (sizeof(struct slsi_lls_peer_info) * SLSI_ADHOC_PEER_CONNECTIONS_MAX * MAX_NUM_MLD_LINKS));

#ifdef CONFIG_SCSC_WLAN_DEBUG
	radio_stat_temp = radio_stat;
#endif
	if (num_of_radios_supported == 1) {
		radio_type[0] = BIT(0) | BIT(1);
		slsi_lls_radio_stat_fill(sdev, net_dev, radio_stat, max_chan_count, 0, radio_type[0]);
		radio_stat = (struct slsi_lls_radio_stat *)((u8 *)radio_stat + sizeof(struct slsi_lls_radio_stat)
							    + (sizeof(struct slsi_lls_channel_stat) * radio_stat->num_channels));
	} else {
		for (i = 1; i <= num_of_radios_supported ; i++) {
			slsi_lls_radio_stat_fill(sdev, net_dev, radio_stat, max_chan_count, i, radio_type[i - 1]);
			radio_stat = (struct slsi_lls_radio_stat *)((u8 *)radio_stat + sizeof(struct slsi_lls_radio_stat)
								    + (sizeof(struct slsi_lls_channel_stat) * radio_stat->num_channels));
		}
	}

#ifdef CONFIG_SCSC_WLAN_DEBUG
	if (slsi_dev_llslogs_supported())
		slsi_lls_debug_dump_stats(sdev, radio_stat_temp, NULL, iface_ml_stat, buf, buf_len, num_of_radios_supported);
#endif
	return buf_len;
}
#endif

int slsi_lls_fill_stats(struct slsi_dev *sdev, u8 **src_buf, bool is_mlo)
{
#ifdef CONFIG_SCSC_WLAN_EHT
	if (is_mlo)
		return slsi_lls_fill_mlo(sdev, src_buf);
#endif

	return slsi_lls_fill(sdev, src_buf);
}

