#ifndef __BITSY_H__
#define __BITSY_H__
/* Copyright 2012 exMULTI, Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include <bitc/buint.h>                // for bu256_t
#include <bitc/cstr.h>                 // for cstring
#include <bitc/hashtab.h>              // for bitc_hashtab_get

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint64_t


struct wallet;

/* main.c */
extern struct bitc_hashtab *settings;
extern const struct chain_info *chain;
extern bu256_t chain_genesis;
extern uint64_t instance_nonce;
extern struct wallet *cur_wallet;

static inline char *setting(const char *key)
{
	return bitc_hashtab_get(settings, key);
}

#endif /* __BITSY_H__ */
