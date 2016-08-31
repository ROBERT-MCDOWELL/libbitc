/* Copyright 2012 exMULTI, Inc.
 * Copyright 2015 Josh Cartwright <joshc@eso.teric.us>
 *
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#include "libbitc-config.h"

#include <bitc/address.h>
#include <bitc/coredefs.h>
#include <bitc/key.h>
#include <bitc/mbr.h>
#include <bitc/message.h>
#include <bitc/wallet.h>
#include <bitc/hdkeys.h>
#include <bitc/serialize.h>

#include <assert.h>
#include <stdio.h>

struct hd_extended_key_serialized {
	uint8_t data[78 + 1];	// 78 + NUL (the latter not written)
};

static void wallet_free_key(void *p)
{
	struct bitc_key *key = p;

	if (!key)
		return;

	bitc_key_free(key);
	memset(key, 0, sizeof(*key));
	free(key);
}

static void wallet_free_hdkey(void *p)
{
	struct hd_extended_key *hdkey = p;

	if (!hdkey)
		return;

	hd_extended_key_free(hdkey);
	memset(hdkey, 0, sizeof(*hdkey));
	free(hdkey);
}

bool wallet_init(struct wallet *wlt, const struct chain_info *chain)
{
	wlt->version = 1;
	wlt->chain = chain;
	wlt->keys = parr_new(1000, wallet_free_key);
	wlt->hdmaster = parr_new(10, wallet_free_hdkey);

	return ((wlt->keys != NULL) && (wlt->hdmaster != NULL));
}

void wallet_free(struct wallet *wlt)
{
	struct bitc_key *key;
	struct hd_extended_key *hdkey;

	if (!wlt)
		return;

	wallet_for_each_key(wlt, key)
		bitc_key_free(key);
	wallet_for_each_mkey(wlt, hdkey)
		hd_extended_key_free(hdkey);

	parr_free(wlt->keys, true);
	parr_free(wlt->hdmaster, true);
	memset(wlt, 0, sizeof(*wlt));
}

cstring *wallet_new_address(struct wallet *wlt)
{
	struct bitc_key *key;

	key = calloc(1, sizeof(*key));
	if (!bitc_key_init(key)) {
		free(key);
		fprintf(stderr, "wallet: key init failed\n");
		return NULL;
	}

	if (!bitc_key_generate(key)) {
		bitc_key_free(key);
		free(key);
		fprintf(stderr, "wallet: key gen failed\n");
		return NULL;
	}

	parr_add(wlt->keys, key);

	return bitc_pubkey_get_address(key, wlt->chain->addr_pubkey);
}

static cstring *ser_wallet_root(const struct wallet *wlt)
{
	cstring *rs = cstr_new_sz(8);

	ser_u32(rs, wlt->version);
	ser_bytes(rs, &wlt->chain->netmagic[0], 4);

	return rs;
}

static bool write_ek_ser_prv(struct hd_extended_key_serialized *out,
			     const struct hd_extended_key *ek)
{
	cstring s = { (char *)(out->data), 0, sizeof(out->data) };
	return hd_extended_key_ser_priv(ek, &s);
}

cstring *ser_wallet(const struct wallet *wlt)
{
	struct bitc_key *key;

	cstring *rs = cstr_new_sz(20 * 1024);

	/*
	 * ser "root" record
	 */
	{
	cstring *s_root = ser_wallet_root(wlt);
	cstring *recdata = message_str(wlt->chain->netmagic,
				       "root", s_root->str, s_root->len);
	cstr_append_buf(rs, recdata->str, recdata->len);
	cstr_free(recdata, true);
	cstr_free(s_root, true);
	}

	/* ser "privkey" records */
	wallet_for_each_key(wlt, key) {
		void *privkey = NULL;
		size_t pk_len = 0;

		bitc_privkey_get(key, &privkey, &pk_len);

		cstring *recdata = message_str(wlt->chain->netmagic,
					       "privkey",
					       privkey, pk_len);
		free(privkey);

		cstr_append_buf(rs, recdata->str, recdata->len);
		cstr_free(recdata, true);
	}

	/* ser "hdmaster" records */
	struct hd_extended_key *hdkey;
	wallet_for_each_mkey(wlt, hdkey) {

		struct hd_extended_key_serialized hdraw;
		bool rc = write_ek_ser_prv(&hdraw, hdkey);
		assert(rc == true);

		cstring *recdata = message_str(wlt->chain->netmagic,
					       "hdmaster",
					       hdraw.data,
					       sizeof(hdraw.data) - 1);

		assert(recdata != NULL);

		cstr_append_buf(rs, recdata->str, recdata->len);
		cstr_free(recdata, true);
	}

	return rs;
}

static bool deser_wallet_root(struct wallet *wlt, struct const_buffer *buf)
{
	unsigned char netmagic[4];

	if (!deser_u32(&wlt->version, buf))
		return false;

	if (!deser_bytes(&netmagic[0], buf, 4))
		return false;

	wlt->chain = chain_find_by_netmagic(netmagic);
	if (!wlt->chain)
		return false;

	return true;
}

static bool load_rec_privkey(struct wallet *wlt, const void *privkey, size_t pk_len)
{
	struct bitc_key *key;

	key = calloc(1, sizeof(*key));
	if (!bitc_key_init(key))
		goto err_out;
	if (!bitc_privkey_set(key, privkey, pk_len))
		goto err_out_kf;

	parr_add(wlt->keys, key);

	return true;

err_out_kf:
	bitc_key_free(key);
err_out:
	free(key);
	return false;
}

static bool load_rec_hdmaster(struct wallet *wlt, const void *mkey, size_t mlen)
{
	struct hd_extended_key *hdkey;

	hdkey = calloc(1, sizeof(*hdkey));
	if (!hd_extended_key_init(hdkey)) {
		fprintf(stderr, "hdmaster fail 1\n");
		goto err_out;
	}
	if (!hd_extended_key_deser(hdkey, mkey, mlen)) {
		fprintf(stderr, "hdmaster fail 2\n");
		goto err_out_kf;
	}

	parr_add(wlt->hdmaster, hdkey);

	return true;

err_out_kf:
	hd_extended_key_free(hdkey);
err_out:
	free(hdkey);
	return false;
}

static bool load_rec_root(struct wallet *wlt, const void *data, size_t data_len)
{
	struct const_buffer buf = { data, data_len };

	if (!deser_wallet_root(wlt, &buf)) return false;

	if (wlt->version != 1) {
		fprintf(stderr, "wallet root: unsupported wallet version %u\n",
			wlt->version);
		return false;
	}

	return true;
}

static bool load_record(struct wallet *wlt, const struct p2p_message *msg)
{
	if (!strncmp(msg->hdr.command, "hdmaster", sizeof(msg->hdr.command)))
		return load_rec_hdmaster(wlt, msg->data, msg->hdr.data_len);

	else if (!strncmp(msg->hdr.command, "privkey", sizeof(msg->hdr.command)))
		return load_rec_privkey(wlt, msg->data, msg->hdr.data_len);

	else if (!strncmp(msg->hdr.command, "root", sizeof(msg->hdr.command)))
		return load_rec_root(wlt, msg->data, msg->hdr.data_len);

	return true;	/* ignore unknown records */
}

bool deser_wallet(struct wallet *wlt, struct const_buffer *buf)
{
	struct mbuf_reader mbr;

	mbr_init(&mbr, buf);

	while (mbr_read(&mbr)) {
		if (!load_record(wlt, &mbr.msg)) {
			mbr_free(&mbr);
			goto err_out;
		}
	}

	if (mbr.error) {
		mbr_free(&mbr);
		goto err_out;
	}

	return true;

err_out:
	return false;
}
