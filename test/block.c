
#include "picocoin-config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <jansson.h>
#include <glib.h>
#include <ccoin/message.h>
#include <ccoin/mbr.h>
#include "libtest.h"

static void runtest(const char *json_fn_base, const char *ser_fn_base)
{
	char *fn = g_strdup_printf("%s/%s", TEST_SRCDIR, json_fn_base);
	json_t *meta = read_json(fn);
	assert(json_is_object(meta));

	char *ser_fn = g_strdup_printf("%s/%s", TEST_SRCDIR, ser_fn_base);
	int fd = open(ser_fn, O_RDONLY);
	if (fd < 0) {
		perror(ser_fn);
		exit(1);
	}

	struct p2p_message msg = {};
	bool read_ok = false;
	bool rc = fread_message(fd, &msg, &read_ok);
	assert(rc);
	assert(read_ok);
	assert(!strncmp(msg.hdr.command, "block", 12));

	const char *hashstr = json_string_value(json_object_get(meta, "hash"));
	assert(hashstr != NULL);

	unsigned int size = json_integer_value(json_object_get(meta, "size"));
	assert((24 + msg.hdr.data_len) == size);

	struct bp_block block;
	bp_block_init(&block);

	struct buffer buf = { msg.data, msg.hdr.data_len };

	rc = deser_bp_block(&block, &buf);
	assert(rc);

	GString *gs = g_string_sized_new(100000);
	ser_bp_block(gs, &block);

	if (gs->len != msg.hdr.data_len) {
		fprintf(stderr, "gs->len %ld, msg.hdr.data_len %u\n",
			(long)gs->len, msg.hdr.data_len);
		assert(gs->len == msg.hdr.data_len);
	}
	assert(memcmp(gs->str, msg.data, msg.hdr.data_len) == 0);

	bp_block_calc_sha256(&block);

	char hexstr[(32 * 2) + 16] = "";
	bu256_hex(hexstr, &block.sha256);

	if (strcmp(hexstr, hashstr)) {
		fprintf(stderr, "block: wanted hash %s,\n       got    hash %s\n",
			hashstr, hexstr);
		assert(!strcmp(hexstr, hashstr));
	}

	bp_block_free(&block);
	g_string_free(gs, TRUE);
	free(msg.data);
	free(fn);
	free(ser_fn);
	json_decref(meta);
}

int main (int argc, char *argv[])
{
	runtest("blk0.json", "blk0.ser");
	runtest("blk120383.json", "blk120383.ser");

	return 0;
}
