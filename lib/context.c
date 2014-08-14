#include <string.h>
#include <sys/time.h>

#include <common/sockaddr.h>
#include "lib/context.h"

int kr_context_init(struct kr_context *ctx, mm_ctx_t *mm)
{
	memset(ctx, 0, sizeof(struct kr_context));

	ctx->pool = mm;
	kr_slist_init(ctx);

	return 0;
}

int kr_context_reset(struct kr_context *ctx)
{
	kr_slist_clear(ctx);
	kr_slist_init(ctx);

	return 0;
}

int kr_context_deinit(struct kr_context *ctx)
{
	kr_slist_clear(ctx);

	return -1;
}

int kr_result_init(struct kr_context *ctx, struct kr_result *result)
{
	memset(result, 0, sizeof(struct kr_result));

	knot_pkt_t *ans = knot_pkt_new(NULL, KNOT_WIRE_MAX_PKTSIZE, ctx->pool);
	if (ans == NULL) {
		return -1;
	}

	knot_pkt_put_question(ans, ctx->sname, ctx->sclass, ctx->stype);
	knot_wire_set_rcode(ans->wire, KNOT_RCODE_SERVFAIL);
	knot_wire_set_qr(ans->wire);

	result->ans = ans;
	gettimeofday(&result->t_start, NULL);

	return 0;
}

int kr_result_deinit(struct kr_result *result)
{
	knot_pkt_free(&result->ans);

	return 0;
}
