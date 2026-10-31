/******************************************************************************
 *                                                                            *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * S.LSI Bluetooth HCI UART driver                                            *
 *                                                                            *
 ******************************************************************************/
#ifndef __SLSI_HCI_UART_BT_H_
#define __SLSI_HCI_UART_BT_H_
#include "hci_trans.h"

int hci_slsi_init(void);
int hci_slsi_deinit(void);

void slsi_hci_uart_resume(struct hci_trans *htr);
void slsi_hci_uart_block(struct hci_trans *htr);

int slsi_hci_uart_open_io(void);
int slsi_hci_uart_close_io(void);

#endif /* __SLSI_HCI_UART_BT_H_ */
