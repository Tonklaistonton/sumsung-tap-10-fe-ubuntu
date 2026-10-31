// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-common.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-mgt.h"
#include "kunit-mock-sap_mlme.h"
#include "kunit-mock-ba.h"
#include "kunit-mock-rx.h"
#include "../fw_test.c"

static struct slsi_fw_test *fwtest;

/* unit test function definition*/
static void test_slsi_fw_test_save_frame(struct kunit *test)
{
	struct slsi_dev     *sdev = TEST_TO_SDEV(test);
	struct sk_buff      *skb = fapi_alloc(ma_unitdata_ind, MLME_CONNECT_REQ, SLSI_NET_INDEX_WLAN, 128);

	skb_pull(skb, sizeof(struct udi_msg_t));
	((struct fapi_vif_signal_header *)(skb)->data)->vif = cpu_to_le16(SLSI_NET_INDEX_WLAN);
	((struct fapi_signal *)(skb)->data)->id = MLME_CONNECT_REQ;
	skb_push(skb, sizeof(struct udi_msg_t));
	slsi_fw_test_save_frame(sdev, fwtest, fwtest->mlme_connect_req, skb, true);

	//will be freed while deinit
	skb = fapi_alloc(mlme_connect_req, MLME_CONNECT_REQ, SLSI_NET_INDEX_WLAN, 128);
	slsi_fw_test_save_frame(sdev, fwtest, fwtest->mlme_connect_req, skb, false);
}

static void test_slsi_fw_test_process_frame(struct kunit *test)
{
	struct slsi_dev     *sdev = TEST_TO_SDEV(test);
	struct net_device   *dev = TEST_TO_DEV(test);
	struct sk_buff      *skb = fapi_alloc(ma_unitdata_ind, MA_UNITDATA_IND, SLSI_NET_INDEX_WLAN, 128);

	sdev->netdev[SLSI_NET_INDEX_WLAN] = dev;
	skb_pull(skb, sizeof(struct udi_msg_t));
	((struct fapi_vif_signal_header *)(skb)->data)->vif = cpu_to_le16(SLSI_NET_INDEX_WLAN);
	((struct fapi_signal *)(skb)->data)->id = MA_UNITDATA_IND;
	skb_push(skb, sizeof(struct udi_msg_t));

	slsi_fw_test_process_frame(sdev, fwtest, skb, true);

	//will be freed by callback
	skb = fapi_alloc(ma_unitdata_ind, MA_UNITDATA_IND, SLSI_NET_INDEX_WLAN, 128);
	slsi_fw_test_process_frame(sdev, fwtest, skb, false);
}

static void test_slsi_fw_test_signal(struct kunit *test)
{
	struct slsi_dev     *sdev = TEST_TO_SDEV(test);
	struct net_device   *dev = TEST_TO_DEV(test);
	struct sk_buff      *skb;

	sdev->netdev[SLSI_NET_INDEX_WLAN] = dev;

	//default
	skb = fapi_alloc(ma_unitdata_ind, MA_UNITDATA_IND, SLSI_NET_INDEX_WLAN, 0);
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal(sdev, fwtest, skb));
	kfree_skb(skb);

	//save & process frame
	skb = fapi_alloc(mlme_add_vif_req, MLME_ADD_VIF_REQ, SLSI_NET_INDEX_WLAN, 0);
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal(sdev, fwtest, skb));
	//to prevent from being freed twice
	fwtest->mlme_add_vif_req[SLSI_NET_INDEX_WLAN] = NULL;

	//save frame
	skb = fapi_alloc(mlme_connect_req, MLME_CONNECT_REQ, SLSI_NET_INDEX_WLAN, 0);
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal(sdev, fwtest, skb));

	//process frame
	skb = fapi_alloc(mlme_del_vif_req, MLME_DEL_VIF_REQ, SLSI_NET_INDEX_WLAN, 0);
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal(sdev, fwtest, skb));
}

static void test_slsi_fw_test_signal_with_udi_header(struct kunit *test)
{
	struct slsi_dev               *sdev = TEST_TO_SDEV(test);
	struct net_device             *dev = TEST_TO_DEV(test);
	struct netdev_vif             *ndev_vif = netdev_priv(dev);
	struct sk_buff                *skb;
	struct udi_msg_t              *msg = kunit_kzalloc(test, sizeof(struct udi_msg_t), GFP_KERNEL);
	struct fapi_vif_signal_header *fapi_header = kunit_kzalloc(test, sizeof(struct fapi_vif_signal_header),
								   GFP_KERNEL);

	fwtest->sdev = sdev;
	fwtest->fw_test_enabled = true;
	msg->direction = SLSI_LOG_DIRECTION_TO_HOST;
	sdev->netdev[1] = dev;
	ndev_vif->is_fw_test = false;

	//process frame
	skb = fapi_alloc(mlme_disconnected_ind, MLME_DISCONNECTED_IND, 1, 128);
	memcpy(skb->data, msg, sizeof(struct udi_msg_t));
	fapi_header->id = cpu_to_le16(MLME_DISCONNECTED_IND);
	fapi_header->vif = cpu_to_le16(SLSI_NET_INDEX_WLAN);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));

	//process frame
	skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 1, 128);
	memcpy(skb->data, msg, sizeof(struct udi_msg_t));
	fapi_header->id = cpu_to_le16(MLME_CONNECT_IND);
	fapi_header->vif = cpu_to_le16(SLSI_NET_INDEX_WLAN);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));

	//process frame
	skb = fapi_alloc(mlme_connected_ind, MLME_CONNECTED_IND, 1, 128);
	memcpy(skb->data, msg, sizeof(struct udi_msg_t));
	fapi_header->id = cpu_to_le16(MLME_CONNECTED_IND);
	fapi_header->vif = cpu_to_le16(SLSI_NET_INDEX_WLAN);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));

	//process frame
	skb = fapi_alloc(mlme_roamed_ind, MLME_ROAMED_IND, 1, 128);
	memcpy(skb->data, msg, sizeof(struct udi_msg_t));
	fapi_header->id = cpu_to_le16(MLME_ROAMED_IND);
	fapi_header->vif = cpu_to_le16(SLSI_NET_INDEX_WLAN);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));

	//process frame
	skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, 1, 128);
	memcpy(skb->data, msg, sizeof(struct udi_msg_t));
	fapi_header->id = cpu_to_le16(MLME_TDLS_PEER_IND);
	fapi_header->vif = cpu_to_le16(SLSI_NET_INDEX_WLAN);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));

	//save frame
	skb = fapi_alloc(mlme_connect_cfm, MLME_CONNECT_CFM, 1, 128);
	memcpy(skb->data, msg, sizeof(struct udi_msg_t));
	fapi_header->id = cpu_to_le16(MLME_CONNECT_CFM);
	fapi_header->vif = cpu_to_le16(SLSI_NET_INDEX_WLAN);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));

	//save & process frame
	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 1, 128);
	memcpy(skb->data, msg, sizeof(struct udi_msg_t));
	fapi_header->id = cpu_to_le16(MLME_PROCEDURE_STARTED_IND);
	fapi_header->vif = cpu_to_le16(SLSI_NET_INDEX_WLAN);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));
	//to prevent from being freed twice
	fwtest->mlme_procedure_started_ind[1] = NULL;

	//process frame
	skb = fapi_alloc(mlme_start_cfm, MLME_START_CFM, 1, 128);
	memcpy(skb->data, msg, sizeof(struct udi_msg_t));
	fapi_header->id = cpu_to_le16(MLME_START_CFM);
	fapi_header->vif = cpu_to_le16(SLSI_NET_INDEX_WLAN);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));

	//process frame
	skb = fapi_alloc(ma_blockackreq_ind, MA_BLOCKACKREQ_IND, 1, 128);
	memcpy(skb->data, msg, sizeof(struct udi_msg_t));
	fapi_header->id = cpu_to_le16(MA_BLOCKACKREQ_IND);
	fapi_header->vif = cpu_to_le16(SLSI_NET_INDEX_WLAN);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));

	//default
	skb = fapi_alloc(ma_spare_1_ind, MA_SPARE_1_IND, 1, 128);
	memcpy(skb->data, msg, sizeof(struct udi_msg_t));
	fapi_header->id = cpu_to_le16(MA_SPARE_1_IND);
	fapi_header->vif = cpu_to_le16(SLSI_NET_INDEX_WLAN);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));
	kfree_skb(skb);
}

static void test_slsi_fw_test_connect_station_roam(struct kunit *test)
{
	struct net_device       *dev = TEST_TO_DEV(test);
	struct netdev_vif       *ndev_vif = netdev_priv(dev);
	struct slsi_dev         *sdev = ndev_vif->sdev;
	struct sk_buff          *skb = fapi_alloc(mlme_roamed_ind, MLME_ROAMED_IND, 0, 128);
	struct ieee80211_mgmt   *mgmt = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt), GFP_KERNEL);
	u16                     flow_id = 0;

	ndev_vif->is_fw_test = true;
	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	fwtest->mlme_procedure_started_ind[ndev_vif->ifnum] = fapi_alloc(mlme_procedure_started_ind,
									 MLME_PROCEDURE_STARTED_IND,
									 0, 128);

	memcpy(((struct fapi_signal *)(skb)->data) + fapi_get_siglen(skb), mgmt, sizeof(*mgmt));
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = true;

	slsi_fw_test_connect_station_roam(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_connect_start_station(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff      *skb = fapi_alloc(mlme_connect_req, MLME_CONNECT_REQ, 0, 128);

	ndev_vif->activated = false;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->is_fw_test = true;

	fwtest->mlme_connect_req[ndev_vif->ifnum] = fapi_alloc(mlme_connect_req, MLME_CONNECT_REQ, 0, 128);
	fwtest->mlme_connect_cfm[ndev_vif->ifnum] = fapi_alloc(mlme_connect_cfm, MLME_CONNECT_CFM, 0, 128);

	dev->ieee80211_ptr = kunit_kzalloc(test, sizeof(struct wireless_dev), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = NULL;
	slsi_fw_test_connect_start_station(sdev, dev, fwtest, skb);

	ndev_vif->activated = false;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	fapi_set_memcpy(skb, u.mlme_connect_req.bssid, "00:11:22:33:44:55");
	slsi_fw_test_connect_start_station(sdev, dev, fwtest, skb);

	kfree_skb(fwtest->mlme_connect_req[ndev_vif->ifnum]);
	fwtest->mlme_connect_req[ndev_vif->ifnum] = NULL;
	kfree_skb(fwtest->mlme_connect_cfm[ndev_vif->ifnum]);
	fwtest->mlme_connect_cfm[ndev_vif->ifnum] = NULL;
}

static void test_slsi_fw_test_connect_station(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff *skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 0, 128);
	u16 flow_id = 0;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->activated = true;
	ndev_vif->is_fw_test = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = true;

	fapi_set_u16(skb, u.mlme_connect_ind.result_code, FAPI_RESULTCODE_SUCCESS);
	fapi_set_u16(skb, u.mlme_connect_ind.flow_id, flow_id);

	fwtest->mlme_connect_req[ndev_vif->ifnum] = fapi_alloc(mlme_connect_req, MLME_CONNECT_REQ, 0, 128);
	fwtest->mlme_connect_cfm[ndev_vif->ifnum] = fapi_alloc(mlme_connect_cfm, MLME_CONNECT_CFM, 0, 128);
	fwtest->mlme_procedure_started_ind[ndev_vif->ifnum] = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 128);

	slsi_fw_test_connect_station(sdev, dev, fwtest, skb);
	kfree_skb(skb);
}

static void test_slsi_fw_test_started_network(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff *skb = fapi_alloc(mlme_start_cfm, MLME_START_CFM, 0, 128);

	ndev_vif->is_fw_test = true;
	ndev_vif->activated = false;
	dev->ieee80211_ptr = &ndev_vif->wdev;

	fapi_set_u16(skb, u.mlme_start_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	slsi_fw_test_started_network(sdev, dev, fwtest, skb);
	kfree_skb(skb);
}

static void test_slsi_fw_test_stop_network(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	ndev_vif->is_fw_test = true;
	ndev_vif->activated = true;

	slsi_fw_test_stop_network(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_connect_start_ap(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff *skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 128);
	struct ieee80211_mgmt *mgmt = fapi_get_mgmt(skb);
	u16 flow_id = 256;

	ndev_vif->is_fw_test = true;
	ndev_vif->activated = true;
	fapi_set_u16(skb, u.mlme_procedure_started_ind.flow_id, flow_id);
	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);
	slsi_fw_test_connect_start_ap(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_connected_network(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 flow_id = 256;

	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_connected_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_connected_ind.flow_id, flow_id);

	slsi_fw_test_connected_network(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_procedure_started_ind(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff *skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 128);
	u16 procedure_type = FAPI_PROCEDURETYPE_CONNECTION_STARTED;
	u16 virtual_interface_type = FAPI_VIFTYPE_STATION;
	u16 flow_id = 256;
	ndev_vif->is_fw_test = true;

	fapi_set_u16(skb, u.mlme_procedure_started_ind.procedure_type, procedure_type);

	ndev_vif->ifnum = 0;
	fwtest->mlme_add_vif_req[ndev_vif->ifnum] = fapi_alloc(mlme_add_vif_req, MLME_ADD_VIF_REQ, 0, 128);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);

	slsi_fw_test_procedure_started_ind(sdev, dev, fwtest, skb);

	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 128);
	fapi_set_u16(skb, u.mlme_procedure_started_ind.procedure_type, procedure_type);
	virtual_interface_type = FAPI_VIFTYPE_AP;
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	slsi_fw_test_procedure_started_ind(sdev, dev, fwtest, skb);

	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 128);
	procedure_type = FAPI_PROCEDURETYPE_UNKNOWN;
	fapi_set_u16(skb, u.mlme_procedure_started_ind.procedure_type, procedure_type);
	slsi_fw_test_procedure_started_ind(sdev, dev, fwtest, skb);

	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 128);
	ndev_vif->is_fw_test = false;
	slsi_fw_test_procedure_started_ind(sdev, dev, fwtest, skb);

	kfree_skb(fwtest->mlme_add_vif_req[ndev_vif->ifnum]);
}

static void test_slsi_fw_test_connect_ind(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff *skb = fapi_alloc(mlme_add_vif_req, MLME_ADD_VIF_REQ, 0, 128);
	u16 virtual_interface_type = FAPI_VIFTYPE_STATION;
	u16 viftype;

	ndev_vif->ifnum = 0;
	ndev_vif->is_fw_test = true;
	fwtest->mlme_add_vif_req[ndev_vif->ifnum] = fapi_alloc(mlme_add_vif_req, MLME_ADD_VIF_REQ, 0, 128);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	
	slsi_fw_test_connect_ind(sdev, dev, fwtest, skb);

	skb = fapi_alloc(mlme_add_vif_req, MLME_ADD_VIF_REQ, 0, 128);
	virtual_interface_type = FAPI_VIFTYPE_AP;
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	slsi_fw_test_connect_ind(sdev, dev, fwtest, skb);

	kfree_skb(fwtest->mlme_add_vif_req[ndev_vif->ifnum]);
}

static void test_slsi_fw_test_connected_ind(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff *skb = fapi_alloc(mlme_connected_ind, MLME_CONNECTED_IND, 0, 128);
	u16 virtual_interface_type = FAPI_VIFTYPE_STATION;

	ndev_vif->ifnum = 0;
	ndev_vif->is_fw_test = true;
	fwtest->mlme_add_vif_req[ndev_vif->ifnum] = fapi_alloc(mlme_add_vif_req, MLME_ADD_VIF_REQ, 0, 128);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);

	slsi_fw_test_connected_ind(sdev, dev, fwtest, skb);

	skb = fapi_alloc(mlme_connected_ind, MLME_CONNECTED_IND, 0, 128);
	virtual_interface_type = FAPI_VIFTYPE_AP;
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	slsi_fw_test_connected_ind(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_roamed_ind(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff *skb = fapi_alloc(mlme_roamed_ind, MLME_ROAMED_IND, 0, 128);
	u16 virtual_interface_type = FAPI_VIFTYPE_STATION;

	ndev_vif->ifnum = 0;
	ndev_vif->is_fw_test = true;
	fwtest->mlme_add_vif_req[ndev_vif->ifnum] = fapi_alloc(mlme_add_vif_req, MLME_ADD_VIF_REQ, 0, 128);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);

	slsi_fw_test_roamed_ind(sdev, dev, fwtest, skb);

	skb = fapi_alloc(mlme_roamed_ind, MLME_ROAMED_IND, 0, 128);
	virtual_interface_type = FAPI_VIFTYPE_AP;
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	slsi_fw_test_roamed_ind(sdev, dev, fwtest, skb);
	kfree_skb(fwtest->mlme_add_vif_req[ndev_vif->ifnum]);
}

static void test_slsi_fw_test_disconnect_station(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	ndev_vif->is_fw_test = true;
	ndev_vif->ifnum = 0;
	ndev_vif->activated = true;

	slsi_fw_test_disconnect_station(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_disconnect_network(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff *skb = fapi_alloc(mlme_disconnected_ind, MLME_DISCONNECTED_IND, 0, 128);

	fapi_set_memcpy(skb, u.mlme_disconnected_ind.peer_sta_address, "00:11:22:33:44:55");
	ndev_vif->is_fw_test = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.tdls_enabled = false;
	ndev_vif->peer_sta_record[0] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[0]->valid = true;
	memcpy(ndev_vif->peer_sta_record[0]->address,
	       ((struct fapi_signal *)(skb)->data)->u.mlme_disconnected_ind.peer_sta_address, ETH_ALEN);

	slsi_fw_test_disconnect_network(sdev, dev, fwtest, skb);
	kfree_skb(skb);
}

static void test_slsi_fw_test_disconnected_ind(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff *skb = fapi_alloc(mlme_disconnected_ind, MLME_DISCONNECTED_IND, 0, 128);
	u16 virtual_interface_type = FAPI_VIFTYPE_STATION;

	ndev_vif->ifnum = 0;
	fwtest->mlme_add_vif_req[ndev_vif->ifnum] = fapi_alloc(mlme_add_vif_req, MLME_ADD_VIF_REQ, 0, 128);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);

	ndev_vif->is_fw_test = true;
	ndev_vif->ifnum = 0;
	slsi_fw_test_disconnected_ind(sdev, dev, fwtest, skb);

	skb = fapi_alloc(mlme_disconnected_ind, MLME_DISCONNECTED_IND, 0, 128);
	virtual_interface_type = FAPI_VIFTYPE_AP;
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	slsi_fw_test_disconnected_ind(sdev, dev, fwtest, skb);

	skb = fapi_alloc(mlme_disconnected_ind, MLME_DISCONNECTED_IND, 0, 128);
	virtual_interface_type = FAPI_VIFTYPE_NAN;
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	slsi_fw_test_disconnected_ind(sdev, dev, fwtest, skb);

	kfree_skb(fwtest->mlme_add_vif_req[ndev_vif->ifnum]);
}

static void test_slsi_fw_test_tdls_event_connected(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct slsi_dev     *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, 0, 128);
	u16 flow_id = 512;

	fapi_set_u16(skb, u.mlme_tdls_peer_ind.flow_id, flow_id);
	fapi_set_memcpy(skb, u.mlme_tdls_peer_ind.peer_sta_address, "00:11:22:33:44:55");

	slsi_fw_test_tdls_event_connected(sdev, dev, skb);
	kfree_skb(skb);
}

static void test_slsi_fw_test_tdls_event_disconnected(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff *skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, 0, 128);

	fapi_set_memcpy(skb, u.mlme_tdls_peer_ind.peer_sta_address, "00:11:22:33:44:55");
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.tdls_enabled = false;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = true;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->aid = 1;
	memcpy(ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->address,
	       ((struct fapi_signal *)(skb)->data)->u.mlme_tdls_peer_ind.peer_sta_address, ETH_ALEN);

	slsi_fw_test_tdls_event_disconnected(sdev, dev, skb);
	kfree_skb(skb);
}

static void test_slsi_fw_test_tdls_peer_ind(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff *skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, 0, 128);
	u16 virtual_interface_type = FAPI_VIFTYPE_STATION;
	u16 tdls_event = FAPI_TDLSEVENT_DISCONNECTED;

	ndev_vif->is_fw_test = true;
	ndev_vif->activated = true;
	ndev_vif->ifnum = 0;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = true;
	ndev_vif->sta.tdls_enabled = false;

	fwtest->mlme_add_vif_req[ndev_vif->ifnum] = fapi_alloc(mlme_add_vif_req, MLME_ADD_VIF_REQ, 0, 128);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);

	fapi_set_u16(skb, u.mlme_tdls_peer_ind.tdls_event, tdls_event);
	slsi_fw_test_tdls_peer_ind(sdev, dev, fwtest, skb);

	tdls_event = FAPI_TDLSEVENT_CONNECTED;
	skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, 0, 128);
	fapi_set_u16(skb, u.mlme_tdls_peer_ind.tdls_event, tdls_event);
	slsi_fw_test_tdls_peer_ind(sdev, dev, fwtest, skb);

	tdls_event = FAPI_TDLSEVENT_DISCOVERED;
	skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, 0, 128);
	fapi_set_u16(skb, u.mlme_tdls_peer_ind.tdls_event, tdls_event);
	slsi_fw_test_tdls_peer_ind(sdev, dev, fwtest, skb);

	tdls_event += 1;
	skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, 0, 128);
	fapi_set_u16(skb, u.mlme_tdls_peer_ind.tdls_event, tdls_event);
	slsi_fw_test_tdls_peer_ind(sdev, dev, fwtest, skb);

	kfree_skb(fwtest->mlme_add_vif_req[ndev_vif->ifnum]);
}

static void test_slsi_fw_test_start_cfm(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff *skb = fapi_alloc(mlme_start_cfm, MLME_START_CFM, 0, 128);
	u16 virtual_interface_type = FAPI_VIFTYPE_AP;
	u16 tdls_event = FAPI_TDLSEVENT_CONNECTED;

	ndev_vif->is_fw_test = true;
	ndev_vif->ifnum = 0;
	fwtest->mlme_add_vif_req[ndev_vif->ifnum] = fapi_alloc(mlme_add_vif_req, MLME_ADD_VIF_REQ, 0, 128);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	slsi_fw_test_start_cfm(sdev, dev, fwtest, skb);

	skb = fapi_alloc(mlme_start_cfm, MLME_START_CFM, 0, 128);
	virtual_interface_type = FAPI_VIFTYPE_STATION;
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	slsi_fw_test_start_cfm(sdev, dev, fwtest, skb);

	skb = fapi_alloc(mlme_start_cfm, MLME_START_CFM, 0, 128);
	ndev_vif->is_fw_test = false;
	slsi_fw_test_start_cfm(sdev, dev, fwtest, skb);

	kfree_skb(fwtest->mlme_add_vif_req[ndev_vif->ifnum]);
}

static void test_slsi_fw_test_add_vif_req(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff      *skb = fapi_alloc(mlme_add_vif_req, MLME_ADD_VIF_REQ, 0, 128);
	u16 vif = 0;

	fapi_set_u16(skb, u.mlme_add_vif_req.vif, vif);
	slsi_fw_test_add_vif_req(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_del_vif_req(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff      *skb = fapi_alloc(mlme_del_vif_req, MLME_DEL_VIF_REQ, 0, 128);
	u16                 virtual_interface_type = FAPI_VIFTYPE_AP;
	u16                 vif = 0;

	fapi_set_u16(skb, u.mlme_del_vif_req.vif, vif);
	ndev_vif->ifnum = 0;

	fwtest->mlme_add_vif_req[ndev_vif->ifnum] = fapi_alloc(mlme_add_vif_req, MLME_ADD_VIF_REQ, 0, 128);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	fwtest->mlme_connect_req[ndev_vif->ifnum] = NULL;
	fwtest->mlme_connect_cfm[ndev_vif->ifnum] = NULL;
	fwtest->mlme_procedure_started_ind[ndev_vif->ifnum] = NULL;
	slsi_fw_test_del_vif_req(sdev, dev, fwtest, skb);

	skb = fapi_alloc(mlme_del_vif_req, MLME_DEL_VIF_REQ, 0, 128);
	virtual_interface_type = FAPI_VIFTYPE_STATION;
	ndev_vif->is_fw_test = true;
	ndev_vif->activated = true;
	fwtest->mlme_add_vif_req[ndev_vif->ifnum] = fapi_alloc(mlme_add_vif_req, MLME_ADD_VIF_REQ, 0, 128);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	fwtest->mlme_connect_req[ndev_vif->ifnum] = NULL;
	fwtest->mlme_connect_cfm[ndev_vif->ifnum] = NULL;
	fwtest->mlme_procedure_started_ind[ndev_vif->ifnum] = NULL;
	slsi_fw_test_del_vif_req(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_ma_blockackreq_ind(struct kunit *test)
{
	struct net_device   *dev = TEST_TO_DEV(test);
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
	struct slsi_dev     *sdev = ndev_vif->sdev;
	struct sk_buff      *skb = fapi_alloc(ma_blockackreq_ind, MA_BLOCKACKREQ_IND, 0, 128);

	ndev_vif->is_fw_test = true;
	slsi_fw_test_ma_blockackreq_ind(sdev, dev, fwtest, skb);

	skb = fapi_alloc(ma_blockackreq_ind, MA_BLOCKACKREQ_IND, 0, 128);
	ndev_vif->is_fw_test = false;
	slsi_fw_test_ma_blockackreq_ind(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_work(struct kunit *test)
{
	struct slsi_dev     *sdev = TEST_TO_SDEV(test);
	struct net_device   *dev = TEST_TO_DEV(test);
	struct sk_buff      *skb;
	u16                 vif = 1;

	fwtest->sdev = sdev;
	sdev->netdev[0] = dev;
	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 128);
	fapi_set_u16(skb, u.mlme_procedure_started_ind.vif, vif);
	fapi_set_u16(skb, id, MLME_PROCEDURE_STARTED_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 0, 128);
	fapi_set_u16(skb, u.mlme_connect_ind.vif, vif);
	fapi_set_u16(skb, id, MLME_CONNECT_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	skb = fapi_alloc(mlme_roamed_ind, MLME_ROAMED_IND, 0, 128);
	fapi_set_u16(skb, u.mlme_roamed_ind.vif, vif);
	fapi_set_u16(skb, id, MLME_ROAMED_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	skb = fapi_alloc(mlme_connected_ind, MLME_CONNECTED_IND, 0, 128);
	fapi_set_u16(skb, u.mlme_connected_ind.vif, vif);
	fapi_set_u16(skb, id, MLME_CONNECTED_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	skb = fapi_alloc(mlme_disconnected_ind, MLME_DISCONNECTED_IND, 0, 128);
	fapi_set_u16(skb, u.mlme_disconnected_ind.vif, vif);
	fapi_set_u16(skb, id, MLME_DISCONNECTED_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, 0, 128);
	fapi_set_u16(skb, u.mlme_tdls_peer_ind.vif, vif);
	fapi_set_u16(skb, id, MLME_TDLS_PEER_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	skb = fapi_alloc(mlme_start_cfm, MLME_START_CFM, 0, 128);
	fapi_set_u16(skb, u.mlme_start_cfm.vif, vif);
	fapi_set_u16(skb, id, MLME_START_CFM);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	skb = fapi_alloc(mlme_add_vif_req, MLME_ADD_VIF_REQ, 0, 128);
	fapi_set_u16(skb, u.mlme_add_vif_req.vif, vif);
	fapi_set_u16(skb, id, MLME_ADD_VIF_REQ);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	skb = fapi_alloc(mlme_del_vif_req, MLME_DEL_VIF_REQ, 0, 128);
	fapi_set_u16(skb, u.mlme_del_vif_req.vif, vif);
	fapi_set_u16(skb, id, MLME_DEL_VIF_REQ);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	skb = fapi_alloc(ma_blockackreq_ind, MA_BLOCKACKREQ_IND, 0, 128);
	fapi_set_u16(skb, u.ma_blockackreq_ind.vif, vif);
	fapi_set_u16(skb, id, MA_BLOCKACKREQ_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

#ifdef CONFIG_SCSC_WLAN_TX_API
	skb = fapi_alloc(mlme_frame_transmission_ind, MLME_FRAME_TRANSMISSION_IND, 0, 128);
	fapi_set_u16(skb, u.mlme_frame_transmission_ind.vif, vif);
	fapi_set_u16(skb, id, MLME_FRAME_TRANSMISSION_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	skb = fapi_alloc(mlme_send_frame_cfm, MLME_SEND_FRAME_CFM, 0, 128);
	fapi_set_u16(skb, u.mlme_send_frame_cfm.vif, vif);
	fapi_set_u16(skb, id, MLME_SEND_FRAME_CFM);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);
#endif

	//Unhanlded signal case
	skb = fapi_alloc(ma_spare_1_ind, MA_SPARE_1_IND, 0, 128);
	fapi_set_u16(skb, u.ma_spare_1_ind.vif, vif);
	fapi_set_u16(skb, id, MA_SPARE_1_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	vif = CONFIG_SCSC_WLAN_MAX_INTERFACES + 1;
	skb = fapi_alloc(ma_spare_1_ind, MA_SPARE_1_IND, 0, 128);
	fapi_set_u16(skb, u.ma_spare_1_ind.vif, vif);
	fapi_set_u16(skb, id, MA_SPARE_1_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);
}

/* Test fictures */
static int fw_test_test_init(struct kunit *test)
{
	struct slsi_dev     *sdev;

	test_dev_init(test);
	sdev = TEST_TO_SDEV(test);
	fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);

	slsi_fw_test_init(sdev, fwtest);
	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void fw_test_test_exit(struct kunit *test)
{
	struct slsi_dev     *sdev = TEST_TO_SDEV(test);

	slsi_fw_test_deinit(sdev, fwtest);
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case fw_test_test_cases[] = {
	KUNIT_CASE(test_slsi_fw_test_save_frame),
	KUNIT_CASE(test_slsi_fw_test_process_frame),
	KUNIT_CASE(test_slsi_fw_test_signal),
	KUNIT_CASE(test_slsi_fw_test_signal_with_udi_header),
	KUNIT_CASE(test_slsi_fw_test_connect_station_roam),
	KUNIT_CASE(test_slsi_fw_test_connect_start_station),
	KUNIT_CASE(test_slsi_fw_test_connect_station),
	KUNIT_CASE(test_slsi_fw_test_started_network),
	KUNIT_CASE(test_slsi_fw_test_stop_network),
	KUNIT_CASE(test_slsi_fw_test_connect_start_ap),
	KUNIT_CASE(test_slsi_fw_test_connected_network),
	KUNIT_CASE(test_slsi_fw_test_procedure_started_ind),
	KUNIT_CASE(test_slsi_fw_test_connect_ind),
	KUNIT_CASE(test_slsi_fw_test_connected_ind),
	KUNIT_CASE(test_slsi_fw_test_roamed_ind),
	KUNIT_CASE(test_slsi_fw_test_disconnect_station),
	KUNIT_CASE(test_slsi_fw_test_disconnect_network),
	KUNIT_CASE(test_slsi_fw_test_disconnected_ind),
	KUNIT_CASE(test_slsi_fw_test_tdls_event_connected),
	KUNIT_CASE(test_slsi_fw_test_tdls_event_disconnected),
	KUNIT_CASE(test_slsi_fw_test_tdls_peer_ind),
	KUNIT_CASE(test_slsi_fw_test_start_cfm),
	KUNIT_CASE(test_slsi_fw_test_add_vif_req),
	KUNIT_CASE(test_slsi_fw_test_del_vif_req),
	KUNIT_CASE(test_slsi_fw_test_ma_blockackreq_ind),
	KUNIT_CASE(test_slsi_fw_test_work),
	{}
};

static struct kunit_suite fw_test_test_suite[] = {
	{
		.name = "fw_test-test",
		.test_cases = fw_test_test_cases,
		.init = fw_test_test_init,
		.exit = fw_test_test_exit,
	}
};

kunit_test_suites(fw_test_test_suite);
