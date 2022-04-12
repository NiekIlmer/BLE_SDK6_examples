#include "rwip_config.h"             // SW configuration
#include "gap.h"
#include "co_bt.h"
#include "llm.h"
#include "l2cc_task.h"
#include "app.h"
#include "app_easy_timer.h"
#include "app_prf_perm_types.h"
#include "app_easy_security.h"
#include "app_security.h"
#include "app_task.h"
#include "app_utils.h"

typedef struct {
	/* Connection ID */
	uint8_t connection_id;
	/* BLE Protocol/Service Multiplexer */
	uint16_t le_psm;
	/* BLE CID */
	uint16_t cid;
  /* Destination Credit for the LE Credit Based Connection */
  uint16_t rx_credits;
  /* Local Credit for the LE Credit Based Connection */
  uint16_t tx_credits;
	/* Maximum Transfer Unit */
	uint16_t MTU;
	/* Maximum Payload Size */
	uint16_t MPS;
} lecb_conn;

void LECB_add_rx_credit(lecb_conn *connection, uint8_t credits);
void LECB_create_channel(lecb_conn *connection, uint16_t psm, uint16_t cid, uint16_t initial_credits);
void gapc_lecb_connect_req_handler(lecb_conn *connection);
void LECB_send_pdu(lecb_conn *connection, uint8_t* data, uint16_t length);


