#include "libbitc-config.h"

#include <assert.h>
#include <stdio.h>
#include <openssl/err.h>
#include <bitc/buffer.h>
#include <bitc/coredefs.h>
#include <bitc/cstr.h>
#include <bitc/hdkeys.h>
#include <bitc/key.h>
#include <bitc/wallet.h>

static bool key_eq(const struct bitc_key *key1,
		   const struct bitc_key *key2)
{
	void *data1, *data2;
	size_t len1, len2;
	int ret;

	if (!bitc_privkey_get(key1, &data1, &len1))
		return false;

	if (!bitc_privkey_get(key2, &data2, &len2)){
		free(data1);
		return false;
	}

	if (len1 != len2) {
		free(data2);
		free(data1);
		return false;
	}

	ret = memcmp(data1, data2, len1);

	free(data1);
	free(data2);

	return ret == 0;
}

static bool wallet_eq(const struct wallet *wlt1,
		      const struct wallet *wlt2)
{
	unsigned int i;

	if (wlt1->version != wlt2->version)
		return false;

	if (wlt1->chain != wlt2->chain)
		return false;

	if (wlt1->keys->len != wlt2->keys->len)
		return false;

	for (i = 0; i < wlt1->keys->len; i++) {
		const struct bitc_key *key1, *key2;

		key1 = parr_idx(wlt1->keys, i);
		key2 = parr_idx(wlt2->keys, i);

		if (!key_eq(key1, key2))
			return false;
	}

	return true;
}

/*
 * Given a wallet, wlt, ensure the following condition holds:
 *
 *   deser(ser(wlt)) == wlt
 *
 * Note that this implies ensuring that the key order is preserved
 * during serialization/deserialization, which may be a more strict
 * than required.
 */
static void check_serialization(const struct wallet *wlt)
{
	struct wallet deser;
	cstring *ser = ser_wallet(wlt);
	struct const_buffer buf;

	assert(wallet_init(&deser, wlt->chain));
	assert(ser != NULL);

	buf.p = ser->str;
	buf.len = ser->len;

	assert(deser_wallet(&deser, &buf));
	assert(wallet_eq(wlt, &deser));

	cstr_free(ser, true);
	wallet_free(&deser);
}

// Seed (hex): 000102030405060708090a0b0c0d0e0f
static const uint8_t test_seed[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};

static void check_with_chain(const struct chain_info *chain)
{
	struct wallet wlt;
	unsigned int i;

	assert(wallet_init(&wlt, chain));

	struct hd_extended_key *hdkey;
	hdkey = calloc(1, sizeof(*hdkey));
	assert(hd_extended_key_init(hdkey) == true);
	assert(hd_extended_key_generate_master(hdkey, test_seed, sizeof(test_seed)) == true);

	parr_add(wlt.hdmaster, hdkey);

	for (i = 0; i < 100; i++) {
		cstring *addr;

		addr = wallet_new_address(&wlt);
		assert(addr != NULL);

		cstr_free(addr, true);
	}

	check_serialization(&wlt);

	wallet_free(&wlt);

}

int main(int argc, char *argv[])
{
	unsigned int i;

	for (i = 0; i < CHAIN_LAST; i++)
		check_with_chain(&chain_metadata[i]);

	ERR_remove_state(0);
	bitc_key_static_shutdown();
	return 0;
}
