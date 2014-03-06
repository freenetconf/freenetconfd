#include "dmconfig.h"

/*
 * dm_init() - init libev and dmconfig parts
 *
 * @event_base*:
 * @DMCONTEXT:
 * DM_AVPGRP*:
 *
 * NOTE: should we move this to freenetconfd.c, and use extern?
 */
int dm_init(struct event_base **base, DMCONTEXT *ctx, DM_AVPGRP **grp)
{
	int rc = -1;

	if (!(*base = event_init())) {
		fprintf(stderr,"dmconfig: event init failed\n");
		return rc;
	}

	dm_context_init(ctx, *base);

	rc = dm_init_socket(ctx, AF_INET);
	if (rc) {
		fprintf(stderr, "dmconfig: init socket failed\n");
		goto exit;
	}

	rc = dm_send_start_session(ctx, CMD_FLAG_READWRITE, NULL, NULL);
	if (rc) {
		fprintf(stderr, "dmconfig: send start session failed\n");
		goto exit;
	}

	*grp = dm_grp_new();
	if (*grp) rc = 0;

exit:
	return rc;
}

/*
 * dm_shutdown() - deinit libev and dmconfig parts
 *
 * @event_base*:
 * @DMCONTEXT:
 * DM_AVPGRP*:
 *
 */
void dm_shutdown(struct event_base **base, DMCONTEXT *ctx, DM_AVPGRP **grp)
{
	if (*grp) dm_grp_free(*grp);
	if (dm_context_get_sessionid(ctx))
		dm_send_end_session(ctx);
	dm_shutdown_socket(ctx);
	event_base_free(*base);
}
/*
 * dm_get_parameter() - get parameter from mand
 *
 * @char*:	key we are searching for
 *
 * Returns NULL on error or value if found. Must be freed.
 */
char* dm_get_parameter(char *key)
{
	if (!key) return NULL;

	DMCONTEXT ctx;
	DM_AVPGRP *grp = NULL, *ret_grp;
	struct event_base *base;

	uint32_t type, vendor_id;
	uint8_t	flags;
	void *data;
	size_t len;
	char *rp = NULL;
	int rc;

	rc = dm_init(&base, &ctx, &grp);
	if (rc) goto exit;

	rc = dm_grp_get_unknown(&grp, key);
	if (rc) goto exit;

	rc = dm_send_packet_get(&ctx, grp, &ret_grp);
	if (rc) goto exit;

	rc = dm_avpgrp_get_avp(ret_grp, &type, &flags, &vendor_id, &data, &len);
	if (rc) goto exit;

	dm_decode_unknown_as_string(type, data, len, &rp);

exit:
	dm_shutdown(&base, &ctx, &grp);
	return rp;
}

/*
 * dm_set_parameter() - set parameter through mand
 *
 * @char*:	key we are setting
 * @char*:	value to be set
 *
 * Returns zero on success.
 */
int dm_set_parameter(char *key, char *value)
{
	if(!key || !value) return -1;

	DMCONTEXT ctx;
	DM_AVPGRP *grp = NULL;
	struct event_base *base;
	int rc = -1;

	rc = dm_init(&base, &ctx, &grp);
	if (rc) goto exit;

	rc = dm_grp_set_unknown(&grp, key, value);
	if (rc) goto exit;

	rc = dm_send_packet_set(&ctx, grp);
	if (rc) goto exit;

	rc = 0;

exit:
	dm_shutdown(&base, &ctx, &grp);

	return rc;
}

/*
 * dm_commit() - commit pending changes
 *
 * Commits pending changes (set_parameter, add object...).
 * Returns zero on success.
 */
int dm_commit()
{
	DMCONTEXT ctx;
	DM_AVPGRP *grp = NULL;
	struct event_base *base;
	int rc = -1;

	rc = dm_init(&base, &ctx, &grp);
	if (rc) goto exit;

	rc = dm_send_commit(&ctx);
	if (rc) {
		fprintf(stderr, "dmconfig: couldn't commit changes\n");
		goto exit;
	}

exit:
	dm_shutdown(&base, &ctx, &grp);

	return rc;
}

/*
 * dm_dump() - dump (part of) config
 *
 * @char*:	NULL or path (for example "system.ntp.1")
 *
 * NULL as path equals to "". Returns NULL on error or value (char*) on
 * success. Must be freed.
 */
char* dm_dump(char *path)
{
	DMCONTEXT ctx;
	DM_AVPGRP *grp = NULL;
	struct event_base *base;
	char *rp = NULL;
	int rc = -1;

	if(!path) path = "";

	rc = dm_init(&base, &ctx, &grp);
	if (rc) goto exit;

	rc = dm_send_cmd_dump(&ctx, path, &rp);
	if (rc) {
		fprintf(stderr, "dmconfig: couldn't dump config\n");
		goto exit;
	}

exit:
	dm_shutdown(&base, &ctx, &grp);

	return rp;
}
