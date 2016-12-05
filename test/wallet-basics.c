
#include "libbitc-config.h"

#include <assert.h>
#include <jansson.h>
#include <bitc/core.h>
#include <bitc/util.h>
#include <bitc/parr.h>
#include <bitc/buffer.h>
#include <bitc/buint.h>
#include <bitc/hexcode.h>
#include <bitc/key.h>
#include <bitc/address.h>
#include <bitc/addr_match.h>
#include "libtest.h"

static void load_json_key(json_t *wallet, struct bitc_key *key)
{
	json_t *keys_a = json_object_get(wallet, "keys");
	assert(json_is_array(keys_a));

	json_t *key_o = json_array_get(keys_a, 0);
	assert(json_is_object(key_o));

	const char *address_str = json_string_value(json_object_get(key_o, "address"));
	assert(address_str != NULL);

	const char *privkey_address_str = json_string_value(json_object_get(key_o, "privkey_address"));
	assert(privkey_address_str);

	const char *pubkey_str = json_string_value(json_object_get(key_o, "pubkey"));
	assert(pubkey_str != NULL);

	const char *privkey_str = json_string_value(json_object_get(key_o, "privkey"));
	assert(privkey_str != NULL);

	char rawbuf[strlen(privkey_str)];
	size_t buf_len = 0;

	/* decode privkey */
	assert(decode_hex(rawbuf, sizeof(rawbuf), privkey_str, &buf_len) == true);

	assert(bitc_privkey_set(key, rawbuf, buf_len) == true);

	/* decode pubkey */
	assert(decode_hex(rawbuf, sizeof(rawbuf), pubkey_str, &buf_len) == true);

	void *pk = NULL;
	size_t pk_len = 0;

	/* verify pubkey matches expected */
	assert(bitc_pubkey_get(key, &pk, &pk_len) == true);
	assert(pk_len == buf_len);
	assert(memcmp(rawbuf, pk, pk_len) == 0);

	free(pk);

	/* verify pubkey hash (bitcoin address) matches expected */
	cstring *btc_addr = bitc_pubkey_get_address(key, PUBKEY_ADDRESS_TEST);
	assert(strlen(address_str) == btc_addr->len);
	assert(memcmp(address_str, btc_addr->str, btc_addr->len) == 0);

	/* verify the private key address (WIF) */
	cstring *privkey_addr = bitc_privkey_get_address(key, PRIVKEY_ADDRESS_TEST);
	assert(strlen(privkey_address_str) == privkey_addr->len);
	assert(memcmp(privkey_address_str, privkey_addr->str, privkey_addr->len) == 0);

	cstr_free(privkey_addr, true);
	cstr_free(btc_addr, true);
}

static void runtest(const char *json_base_fn, const char *ser_in_fn,
		    const char *block_in_hash, const char *tx_in_hash)
{
	/* read wallet data */
	char *json_fn = test_filename(json_base_fn);
	json_t *wallet = read_json(json_fn);
	assert(wallet != NULL);

	/* read block data containing incoming payment */
	char *fn = test_filename(ser_in_fn);
	void *data;
	size_t data_len;
	assert(bu_read_file(fn, &data, &data_len, 1 * 1024 * 1024) == true);

	struct bitc_block block_in;
	bitc_block_init(&block_in);
	struct const_buffer buf = { data, data_len };

	assert(deser_bitc_block(&block_in, &buf) == true);
	bitc_block_calc_sha256(&block_in);

	/* verify block-in data matches expected block-in hash */
	bu256_t check_hash;
	assert(hex_bu256(&check_hash, block_in_hash) == true);

	assert(bu256_equal(&block_in.sha256, &check_hash) == true);

	/* load key that has received an incoming payment */
	struct bitc_key key;
	bitc_key_init(&key);

	load_json_key(wallet, &key);

	/* load key into keyset */
	struct bitc_keyset ks;
	bitc_keyset_init(&ks);

	assert(bitc_keyset_add(&ks, &key) == true);

	/* find key matches in block */
	parr *matches;
	matches = bitc_block_match(&block_in, &ks);
	assert(matches != NULL);
	assert(matches->len == 1);

	struct bitc_block_match *match = parr_idx(matches, 0);
	assert(match->n == 1);			/* match 2nd tx, index 1 */

	/* get matching transaction */
	struct bitc_tx *tx = parr_idx(block_in.vtx, match->n);
	bitc_tx_calc_sha256(tx);

	/* verify txid matches expected */
	char tx_hexstr[BU256_STRSZ];
	bu256_hex(tx_hexstr, &tx->sha256);
	assert(strcmp(tx_hexstr, tx_in_hash) == 0);

	/* verify mask matches 2nd txout (1 << 1) */
	mpz_t tmp_mask;
	mpz_init_set_ui(tmp_mask,1);
	mpz_mul_2exp(tmp_mask, tmp_mask, 1);
	assert(mpz_cmp(tmp_mask, match->mask) == 0);

	/* build merkle tree, tx's branch */
	parr *mtree = bitc_block_merkle_tree(&block_in);
	assert(mtree != NULL);
	parr *mbranch = bitc_block_merkle_branch(&block_in, mtree, match->n);
	assert(mbranch != NULL);

	/* verify merkle branch for tx matches expected */
	bu256_t mrk_check;
	bitc_check_merkle_branch(&mrk_check, &tx->sha256, mbranch, match->n);
	assert(bu256_equal(&mrk_check, &block_in.hashMerkleRoot) == true);

	/* release resources */
	parr_free(mtree, true);
	parr_free(mbranch, true);
	mpz_clear(tmp_mask);
	parr_free(matches, true);
	bitc_keyset_free(&ks);
	bitc_key_free(&key);
	bitc_block_free(&block_in);
	json_decref(wallet);
	free(data);
	free(fn);
	free(json_fn);
}

int main (int argc, char *argv[])
{
	runtest("wallet-basics.json", "tn_blk35133.ser",
	    "00000000003bf8f8f24e0c5f592a38bb7c18352745ef7192f1a576d855fd6b2d",
	    "bf1938abc33cc0b4cde7d94002412b17e35e3c657689e1be7ff588f3fda8d463");

	bitc_key_static_shutdown();

	return 0;
}
