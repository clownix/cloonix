#include "flowtable.h"
#include "config.h"

const char *resource_path = RESOURCE_PATH;

static void dump_handshake_info(struct lws *wsi)
{
	int n = 0;
	char buf[256];
	const unsigned char *c;

	printf("\n");
	do {
		c = lws_token_to_string(n);
		if (!c) {
			n++;
			continue;
		}

		if (!lws_hdr_total_length(wsi, n)) {
			n++;
			continue;
		}

		lws_hdr_copy(wsi, buf, sizeof(buf), n);

		RTE_LOG(INFO, WEBSOCKETS, "%s = %s\n", c, buf);
		n++;
	} while (c);
}

static const char *get_mimetype(const char *file)
{
	int n = strlen(file);

	if (n < 5)
		return NULL;

	if (!strcmp(&file[n - 3], ".js"))
		return "application/javascript";

	if (!strcmp(&file[n - 4], ".ico"))
		return "image/x-icon";

	if (!strcmp(&file[n - 4], ".png"))
		return "image/png";

	if (!strcmp(&file[n - 5], ".html"))
		return "text/html";

	return NULL;
}

int callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	const char *mimetype;
	char buf[256];
	int n = 0;

	switch (reason) {
		case LWS_CALLBACK_HTTP:

			dump_handshake_info(wsi);

			if (len < 1) {
				lws_return_http_status(wsi, HTTP_STATUS_BAD_REQUEST, NULL);
				return -1;
			}

			/* no concept of directories */
			if (strchr((const char *)in + 1, '/')) {
				lws_return_http_status(wsi, HTTP_STATUS_FORBIDDEN, NULL);
				return -1;
			}

			/* send a file the easy way */
			strcpy(buf, resource_path);
			if (strcmp(in, "/")) {
				if (*((const char *)in) != '/')
					strcat(buf, "/");
				strncat(buf, in, sizeof(buf) - strlen(resource_path));
			} else /* default */
				strcat(buf, "/index.html");
			buf[sizeof(buf) - 1] = '\0';

			/* refuse to serve files we don't understand */
			mimetype = get_mimetype(buf);
			if (!mimetype) {
				RTE_LOG(WARNING, WEBSOCKETS, "Unknown mimetype for %s\n", buf);
				lws_return_http_status(wsi, HTTP_STATUS_FORBIDDEN, NULL);
				return -1;
			}

			n = lws_serve_http_file(wsi, buf, mimetype, NULL, n);
			if (n < 0 || ((n > 0) && lws_http_transaction_completed(wsi)))
				return -1; /* error or can't reuse connection */

			break;

		default:
			break;
	}

	return 0;
}
