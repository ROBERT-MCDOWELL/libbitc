/* Copyright 2012 exMULTI, Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#include "libbitc-config.h"

#include <bitc/core.h>                  // for bitc_tx, bitc_txin, etc
#include <bitc/net/version.h>           // for CADDR_TIME_VERSION
#include <bitc/serialize.h>             // for deser_u32, ser_u32, etc
#include <bitc/util.h>                  // for bu_Hash
#include <bitc/compat.h>                // for parr_new

#include <stdbool.h>                    // for false, true, bool
#include <stdint.h>                     // for uint32_t
#include <stdlib.h>                     // for free, calloc, malloc
#include <string.h>                     // for memset, NULL


bool deser_bitc_addr(unsigned int protover,
		struct bitc_address *addr, struct const_buffer *buf)
{
	bitc_addr_free(addr);

	if (protover >= CADDR_TIME_VERSION)
		if (!deser_u32(&addr->nTime, buf)) return false;
	if (!deser_u64(&addr->nServices, buf)) return false;
	if (!deser_bytes(&addr->ip, buf, 16)) return false;
	if (!deser_u16(&addr->port, buf)) return false;
	return true;
}

void ser_bitc_addr(cstring *s, unsigned int protover, const struct bitc_address *addr)
{
	if (protover >= CADDR_TIME_VERSION)
		ser_u32(s, addr->nTime);
	ser_u64(s, addr->nServices);
	ser_bytes(s, addr->ip, 16);
	ser_u16(s, addr->port);
}

void bitc_addr_freep(void *p)
{
	struct bitc_address *addr = p;

	if (!addr)
		return;

	bitc_addr_free(addr);

	memset(addr, 0, sizeof(*addr));
	free(addr);
}

void bitc_inv_init(struct bitc_inv *inv)
{
	memset(inv, 0, sizeof(*inv));
}

bool deser_bitc_inv(struct bitc_inv *inv, struct const_buffer *buf)
{
	bitc_inv_free(inv);

	if (!deser_u32(&inv->type, buf)) return false;
	if (!deser_u256(&inv->hash, buf)) return false;
	return true;
}

void ser_bitc_inv(cstring *s, const struct bitc_inv *inv)
{
	ser_u32(s, inv->type);
	ser_u256(s, &inv->hash);
}

void bitc_inv_freep(void *bitc_inv_p)
{
	struct bitc_inv *inv = bitc_inv_p;
	if (!inv)
		return;

	bitc_inv_free(inv);

	memset(inv, 0, sizeof(*inv));
	free(inv);
}

bool deser_bitc_locator(struct bitc_locator *locator, struct const_buffer *buf)
{
	bitc_locator_free(locator);

	if (!deser_u32(&locator->nVersion, buf)) return false;
	if (!deser_u256_array(&locator->vHave, buf)) return false;

	return true;
}

void ser_bitc_locator(cstring *s, const struct bitc_locator *locator)
{
	ser_u32(s, locator->nVersion);
	ser_u256_array(s, locator->vHave);
}

void bitc_locator_free(struct bitc_locator *locator)
{
	if (!locator)
		return;

	if (locator->vHave) {
		parr_free(locator->vHave, true);
		locator->vHave = NULL;
	}
}

void bitc_locator_push(struct bitc_locator *locator, const bu256_t *hash_in)
{
	/* TODO: replace '16' with number based on real world usage */
	if (!locator->vHave)
		locator->vHave = parr_new(16, bu256_freep);

	bu256_t *hash = bu256_new(hash_in);
	parr_add(locator->vHave, hash);
}

void bitc_outpt_init(struct bitc_outpt *outpt)
{
	memset(outpt, 0, sizeof(*outpt));
}

bool deser_bitc_outpt(struct bitc_outpt *outpt, struct const_buffer *buf)
{
	bitc_outpt_free(outpt);

	if (!deser_u256(&outpt->hash, buf)) return false;
	if (!deser_u32(&outpt->n, buf)) return false;
	return true;
}

void ser_bitc_outpt(cstring *s, const struct bitc_outpt *outpt)
{
	ser_u256(s, &outpt->hash);
	ser_u32(s, outpt->n);
}

void bitc_txin_init(struct bitc_txin *txin)
{
	memset(txin, 0, sizeof(*txin));
	bitc_outpt_init(&txin->prevout);
}

bool deser_bitc_txin(struct bitc_txin *txin, struct const_buffer *buf)
{
	bitc_txin_free(txin);

	if (!deser_bitc_outpt(&txin->prevout, buf)) return false;
	if (!deser_varstr(&txin->scriptSig, buf)) return false;
	if (!deser_u32(&txin->nSequence, buf)) return false;
	return true;
}

void ser_bitc_txin(cstring *s, const struct bitc_txin *txin)
{
	ser_bitc_outpt(s, &txin->prevout);
	ser_varstr(s, txin->scriptSig);
	ser_u32(s, txin->nSequence);
}

void bitc_txin_free(struct bitc_txin *txin)
{
	if (!txin)
		return;

	bitc_outpt_free(&txin->prevout);

	if (txin->scriptWitness) {
		parr_free(txin->scriptWitness, true);
		txin->scriptWitness = NULL;
	}

	if (txin->scriptSig) {
		cstr_free(txin->scriptSig, true);
		txin->scriptSig = NULL;
	}
}

void bitc_txin_freep(void *data)
{
	if (!data)
		return;

	struct bitc_txin *txin = data;
	bitc_txin_free(txin);

	memset(txin, 0, sizeof(*txin));
	free(txin);
}

void bitc_txin_copy(struct bitc_txin *dest, const struct bitc_txin *src)
{
	bitc_outpt_copy(&dest->prevout, &src->prevout);
	dest->nSequence = src->nSequence;

	if (!src->scriptWitness)
		dest->scriptWitness = NULL;
	else {
	    dest->scriptWitness = parr_new(src->scriptWitness->len, buffer_freep);
	    unsigned int i;
	    for (i = 0; i < src->scriptWitness->len; i++) {
	        struct buffer *witness_src = parr_idx(src->scriptWitness, i);
	        parr_add(dest->scriptWitness, buffer_copy(witness_src->p, witness_src->len));
	    }
	}
	if (!src->scriptSig)
		dest->scriptSig = NULL;
	else {
		dest->scriptSig = cstr_new_sz(src->scriptSig->len);
		cstr_append_buf(dest->scriptSig,
				    src->scriptSig->str, src->scriptSig->len);
	}
}

void bitc_txout_init(struct bitc_txout *txout)
{
	memset(txout, 0, sizeof(*txout));
}

bool deser_bitc_txout(struct bitc_txout *txout, struct const_buffer *buf)
{
	bitc_txout_free(txout);

	if (!deser_s64(&txout->nValue, buf)) return false;
	if (!deser_varstr(&txout->scriptPubKey, buf)) return false;
	return true;
}

void ser_bitc_txout(cstring *s, const struct bitc_txout *txout)
{
	ser_s64(s, txout->nValue);
	ser_varstr(s, txout->scriptPubKey);
}

void bitc_txout_free(struct bitc_txout *txout)
{
	if (!txout)
		return;

	if (txout->scriptPubKey) {
		cstr_free(txout->scriptPubKey, true);
		txout->scriptPubKey = NULL;
	}
}

void bitc_txout_freep(void *data)
{
	if (!data)
		return;

	struct bitc_txout *txout = data;
	bitc_txout_free(txout);

	memset(txout, 0, sizeof(*txout));
	free(txout);
}

void bitc_txout_set_null(struct bitc_txout *txout)
{
	bitc_txout_free(txout);

	txout->nValue = -1;
	txout->scriptPubKey = cstr_new("");
}

void bitc_txout_copy(struct bitc_txout *dest, const struct bitc_txout *src)
{
	dest->nValue = src->nValue;

	if (!src->scriptPubKey)
		dest->scriptPubKey = NULL;
	else {
		dest->scriptPubKey = cstr_new_sz(src->scriptPubKey->len);
		cstr_append_buf(dest->scriptPubKey,
				    src->scriptPubKey->str,
				    src->scriptPubKey->len);
	}
}

void bitc_tx_init(struct bitc_tx *tx)
{
	memset(tx, 0, sizeof(*tx));
	tx->nVersion = 1;
}

bool deser_bitc_tx(struct bitc_tx *tx, struct const_buffer *buf)
{
	bitc_tx_free(tx);

	if (!deser_u32(&tx->nVersion, buf)) return false;

	unsigned char flags = 0;
	tx->vin = parr_new(8, bitc_txin_freep);
	tx->vout = parr_new(8, bitc_txout_freep);

	/* Try to read the vin. In case the dummy is there, this will be read as an empty vector. */
	uint32_t vlen;
	if (!deser_varlen(&vlen, buf)) return false;

	unsigned int i;
	for (i = 0; i < vlen; i++) {
		struct bitc_txin *txin;

		txin = calloc(1, sizeof(*txin));
		bitc_txin_init(txin);
		if (!deser_bitc_txin(txin, buf)) {
			free(txin);
			goto err_out;
		}

		parr_add(tx->vin, txin);
	}
    if (tx->vin->len == 0) {
        /* We read a dummy or an empty vin. */
        deser_bytes(&flags, buf, 1);
        if (flags != 0) {
            if (!deser_varlen(&vlen, buf)) return false;
            for (i = 0; i < vlen; i++) {
                struct bitc_txin *txin;

                txin = calloc(1, sizeof(*txin));
                bitc_txin_init(txin);
                if (!deser_bitc_txin(txin, buf)) {
                    free(txin);
                    goto err_out;
                }

            	parr_add(tx->vin, txin);
            }

            if (!deser_varlen(&vlen, buf)) return false;

        	for (i = 0; i < vlen; i++) {
        	    struct bitc_txout *txout;

        	    txout = calloc(1, sizeof(*txout));
        	    bitc_txout_init(txout);
        	    if (!deser_bitc_txout(txout, buf)) {
        	        free(txout);
        	        goto err_out;
        	    }

        	    parr_add(tx->vout, txout);
        	}
        }
    } else {
        /* We read a non-empty vin. Assume a normal vout follows. */
        if (!deser_varlen(&vlen, buf)) return false;
        for (i = 0; i < vlen; i++) {
            struct bitc_txout *txout;

            txout = calloc(1, sizeof(*txout));
            bitc_txout_init(txout);
            if (!deser_bitc_txout(txout, buf)) {
                free(txout);
                goto err_out;
            }

            parr_add(tx->vout, txout);;
        }
    }
    if (flags & 1) {
        /* The witness flag is present, and we support witnesses. */
        flags ^= 1;
        for (i = 0; i < tx->vin->len; i++) {
            struct bitc_txin *txin = parr_idx(tx->vin, i);
            if (!deser_varlen_array(&txin->scriptWitness, buf))
                goto err_out;
        }
    }
    if (flags) {
        /* Unknown flag in the serialization */
        goto err_out;
    }
	if (!deser_u32(&tx->nLockTime, buf)) return false;
	return true;

err_out:
	bitc_tx_free(tx);
	return false;
}

void ser_bitc_tx(cstring *s, const struct bitc_tx *tx)
{
	ser_u32(s, tx->nVersion);

	ser_varlen(s, tx->vin ? tx->vin->len : 0);

	unsigned int i;
	if (tx->vin) {
		for (i = 0; i < tx->vin->len; i++) {
			struct bitc_txin *txin;

			txin = parr_idx(tx->vin, i);
			ser_bitc_txin(s, txin);
		}
	}

	ser_varlen(s, tx->vout ? tx->vout->len : 0);

	if (tx->vout) {
		for (i = 0; i < tx->vout->len; i++) {
			struct bitc_txout *txout;

			txout = parr_idx(tx->vout, i);
			ser_bitc_txout(s, txout);
		}
	}

	ser_u32(s, tx->nLockTime);
}

void bitc_tx_free_vout(struct bitc_tx *tx)
{
	if (!tx || !tx->vout)
		return;

	parr_free(tx->vout, true);
	tx->vout = NULL;
}

void bitc_tx_free(struct bitc_tx *tx)
{
	if (!tx)
		return;

	if (tx->vin) {
		parr_free(tx->vin, true);
		tx->vin = NULL;
	}

	bitc_tx_free_vout(tx);

	tx->sha256_valid = false;
}

void bitc_tx_freep(void *p)
{
	struct bitc_tx *tx = p;
	if (!tx)
		return;

	bitc_tx_free(tx);

	memset(tx, 0, sizeof(*tx));
	free(tx);
}

void bitc_tx_calc_sha256(struct bitc_tx *tx)
{
	if (tx->sha256_valid)
		return;

	/* TODO: introduce hashing-only serialization mode */

	cstring *s = cstr_new_sz(512);
	ser_bitc_tx(s, tx);

	bu_Hash((unsigned char *) &tx->sha256, s->str, s->len);
	tx->sha256_valid = true;

	cstr_free(s, true);
}

unsigned int bitc_tx_ser_size(const struct bitc_tx *tx)
{
	unsigned int tx_ser_size;

	/* TODO: introduce a counting-only serialization mode */

	cstring *s = cstr_new_sz(512);
	ser_bitc_tx(s, tx);

	tx_ser_size = s->len;

	cstr_free(s, true);

	return tx_ser_size;
}

void bitc_tx_copy(struct bitc_tx *dest, const struct bitc_tx *src)
{
	dest->nVersion = src->nVersion;
	dest->nLockTime = src->nLockTime;
	dest->sha256_valid = src->sha256_valid;
	bu256_copy(&dest->sha256, &src->sha256);

	if (!src->vin)
		dest->vin = NULL;
	else {
		unsigned int i;

		dest->vin = parr_new(src->vin->len, bitc_txin_freep);

		for (i = 0; i < src->vin->len; i++) {
			struct bitc_txin *txin_old, *txin_new;

			txin_old = parr_idx(src->vin, i);
			txin_new = malloc(sizeof(*txin_new));
			bitc_txin_copy(txin_new, txin_old);
			parr_add(dest->vin, txin_new);
		}
	}

	if (!src->vout)
		dest->vout = NULL;
	else {
		unsigned int i;

		dest->vout = parr_new(src->vout->len, bitc_txout_freep);

		for (i = 0; i < src->vout->len; i++) {
			struct bitc_txout *txout_old, *txout_new;

			txout_old = parr_idx(src->vout, i);
			txout_new = malloc(sizeof(*txout_new));
			bitc_txout_copy(txout_new, txout_old);
			parr_add(dest->vout, txout_new);
		}
	}
}

void bitc_block_init(struct bitc_block *block)
{
	memset(block, 0, sizeof(*block));
}

bool deser_bitc_block(struct bitc_block *block, struct const_buffer *buf)
{
	bitc_block_free(block);

	if (!deser_u32(&block->nVersion, buf)) return false;
	if (!deser_u256(&block->hashPrevBlock, buf)) return false;
	if (!deser_u256(&block->hashMerkleRoot, buf)) return false;
	if (!deser_u32(&block->nTime, buf)) return false;
	if (!deser_u32(&block->nBits, buf)) return false;
	if (!deser_u32(&block->nNonce, buf)) return false;

	/* permit header-only blocks */
	if (buf->len == 0)
		return true;

	block->vtx = parr_new(512, bitc_tx_freep);

	uint32_t vlen;
	if (!deser_varlen(&vlen, buf)) return false;

	unsigned int i;
	for (i = 0; i < vlen; i++) {
		struct bitc_tx *tx;

		tx = calloc(1, sizeof(*tx));
		bitc_tx_init(tx);
		if (!deser_bitc_tx(tx, buf)) {
			free(tx);
			goto err_out;
		}

		parr_add(block->vtx, tx);
	}

	return true;

err_out:
	bitc_block_free(block);
	return false;
}

static void ser_bitc_block_hdr(cstring *s, const struct bitc_block *block)
{
	ser_u32(s, block->nVersion);
	ser_u256(s, &block->hashPrevBlock);
	ser_u256(s, &block->hashMerkleRoot);
	ser_u32(s, block->nTime);
	ser_u32(s, block->nBits);
	ser_u32(s, block->nNonce);
}

void ser_bitc_block(cstring *s, const struct bitc_block *block)
{
	ser_bitc_block_hdr(s, block);

	unsigned int i;
	if (block->vtx) {
		ser_varlen(s, block->vtx->len);

		for (i = 0; i < block->vtx->len; i++) {
			struct bitc_tx *tx;

			tx = parr_idx(block->vtx, i);
			ser_bitc_tx(s, tx);
		}
	}
}
void bitc_block_vtx_free(struct bitc_block *block)
{
	if (!block || !block->vtx)
		return;

	parr_free(block->vtx, true);
	block->vtx = NULL;
}

void bitc_block_free(struct bitc_block *block)
{
	if (!block)
		return;

	bitc_block_vtx_free(block);
}

void bitc_block_freep(void *p)
{
	struct bitc_block *block = p;
	if (!block)
		return;

	bitc_block_free(block);

	memset(block, 0, sizeof(*block));
	free(block);
}

void bitc_block_free_cb(void *data)
{
	if (!data)
		return;

	struct bitc_block *block = data;
	bitc_block_free(block);

	memset(block, 0, sizeof(*block));
	free(block);
}

void bitc_block_calc_sha256(struct bitc_block *block)
{
	if (block->sha256_valid)
		return;

	/* TODO: introduce hashing-only serialization mode */

	cstring *s = cstr_new_sz(10 * 1024);
	ser_bitc_block_hdr(s, block);

	bu_Hash((unsigned char *)&block->sha256, s->str, s->len);
	block->sha256_valid = true;

	cstr_free(s, true);
}

unsigned int bitc_block_ser_size(const struct bitc_block *block)
{
	unsigned int block_ser_size;

	/* TODO: introduce a counting-only serialization mode */

	cstring *s = cstr_new_sz(200 * 1024);
	ser_bitc_block(s, block);

	block_ser_size = s->len;

	cstr_free(s, true);

	return block_ser_size;
}

