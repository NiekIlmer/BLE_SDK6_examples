#include "lecb.h"

void LECB_add_rx_credit(lecb_conn *connection, uint8_t credits){
		struct gapc_lecb_add_cmd *req = KE_MSG_ALLOC(GAPC_LECB_ADD_CMD,
							KE_BUILD_ID(TASK_GAPC, connection->connection_id), TASK_APP,
							gapc_lecb_add_cmd);
		
		req->operation = GAPC_LE_CB_ADDITION;
		req->le_psm = connection->le_psm;
		req->credit = credits;
		ke_msg_send(req);	
		connection->rx_credits += credits;
}

void LECB_create_channel(lecb_conn *connection, uint16_t psm, uint16_t cid, uint16_t initial_credits){
	  connection->le_psm = psm;
	  connection->cid = cid;
	  struct gapc_lecb_create_cmd *req = KE_MSG_ALLOC(GAPC_LECB_CREATE_CMD,
            KE_BUILD_ID(TASK_GAPC, connection->connection_id), TASK_APP,
            gapc_lecb_create_cmd);
    req->operation = GAPC_LE_CB_CREATE;
    req->sec_lvl = 0;
    req->le_psm = connection->le_psm;
    req->cid = connection->cid;
    req->intial_credit = initial_credits;
		ke_msg_send(req);
}

void gapc_lecb_connect_req_handler(lecb_conn *connection){
		struct gapc_lecb_connect_cfm *req = KE_MSG_ALLOC(GAPC_LECB_CONNECT_CFM,
								KE_BUILD_ID(TASK_GAPC, connection->connection_id), TASK_APP,
								gapc_lecb_connect_cfm);
		
		req->le_psm = connection->le_psm;
		req->status = L2C_CB_CON_SUCCESS;
		ke_msg_send(req);	
}

void LECB_send_pdu(lecb_conn *connection, uint8_t* data, uint16_t length){
		struct l2cc_pdu_send_req *pkt = KE_MSG_ALLOC_DYN(L2CC_PDU_SEND_REQ,
                                                     KE_BUILD_ID(TASK_L2CC, connection->connection_id),
                                                     TASK_APP, l2cc_pdu_send_req,
                                                     length);

    pkt->pdu.chan_id   = connection->cid;
    pkt->pdu.data.code = 0;
		pkt->pdu.data.send_lecb_data_req.code = 0;
		pkt->pdu.data.send_lecb_data_req.sdu_data_len = length;
    memcpy(&(pkt->pdu.data.send_lecb_data_req.sdu_data), data, length);

    // Send message to L2CAP
    ke_msg_send(pkt);

}
