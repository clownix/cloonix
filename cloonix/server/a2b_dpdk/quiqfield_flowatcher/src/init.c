#include "flowtable.h"
#include "config.h"

volatile uint8_t app_quit_signal = 0;

struct app_params app;

struct app_rx_flow_stats app_stat;
struct app_rx_exp_stats exp_stat;

struct lws_context *wss_context = NULL;
struct lws_protocols wss_protocols[] = {
	{
		"http-only",
		callback_http,
		0,
		0, 0, 0
	},
	{
		"flow-export-protocol",
		callback_flowexp,
		sizeof(struct per_session_data__flowexp),
		10, 0, 0,
	},
	{ 0 }
};


static void app_init_flowtables(void)
{
	char s[32];

	RTE_LOG(INFO, FLOWATCHER, "Creating flow_table ...\n");

	snprintf(s, sizeof(s), "flow_table_v4");
	app.flow_table_v4 = create_flow_table(s, FLOW_TABLE_SIZE, PKT_IP_TYPE_IPV4);

	if (app.flow_table_v4 == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create %s\n", s);
	else
		RTE_LOG(INFO, FLOWATCHER, "Created %s\n", s);

	snprintf(s, sizeof(s), "flow_table_v6");
	app.flow_table_v6 = create_flow_table(s, FLOW_TABLE_SIZE, PKT_IP_TYPE_IPV6);

	if (app.flow_table_v6 == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create %s\n", s);
	else
		RTE_LOG(INFO, FLOWATCHER, "Created %s\n", s);
}



static void app_init_lws_context(void)
{
	struct lws_context_creation_info info;

	memset(&info, 0, sizeof(info));
	info.port = FLOWEXP_PORT;
	info.iface = NULL;
	info.protocols = wss_protocols;
	info.ssl_cert_filepath = NULL;
	info.ssl_private_key_filepath = NULL;
	info.gid = -1;
	info.uid = -1;
	info.max_http_header_pool = 1;
	info.options = 0;

	wss_context = lws_create_context(&info);
	if (wss_context == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create lws_context\n");
	else
		RTE_LOG(INFO, FLOWATCHER, "Created lws_context\n");
}

static void app_init_status(void)
{
	get_app_status_init();
}

void app_init(void)
{
	app_init_flowtables();
	app_init_lws_context();
	app_init_status();
	get_app_status();
	RTE_LOG(INFO, FLOWATCHER, "Initialization completed\n\n");
}
