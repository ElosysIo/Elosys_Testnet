/*
 * Generated by asn1c-0.9.28 (http://lionet.info/asn1c)
 * From ASN.1 module "Crypto-Conditions"
 * 	found in "CryptoConditions.asn"
 */

#ifndef	_Condition_H_
#define	_Condition_H_


#include "asn_application.h"

/* Including external dependencies */
#include "SimpleSha256Condition.h"
#include "CompoundSha256Condition.h"
#include "constr_CHOICE.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dependencies */
typedef enum Condition_PR {
	Condition_PR_NOTHING,	/* No components present */
	Condition_PR_preimageSha256,
	Condition_PR_prefixSha256,
	Condition_PR_thresholdSha256,
	Condition_PR_rsaSha256,
	Condition_PR_ed25519Sha256,
	Condition_PR_secp256k1Sha256,
	Condition_PR_evalSha256
} Condition_PR;

/* Condition */
typedef struct Condition {
	Condition_PR present;
	union Condition_u {
		SimpleSha256Condition_t	 preimageSha256;
		CompoundSha256Condition_t	 prefixSha256;
		CompoundSha256Condition_t	 thresholdSha256;
		SimpleSha256Condition_t	 rsaSha256;
		SimpleSha256Condition_t	 ed25519Sha256;
		SimpleSha256Condition_t	 secp256k1Sha256;
		SimpleSha256Condition_t	 evalSha256;
	} choice;
	
	/* Context for parsing across buffer boundaries */
	asn_struct_ctx_t _asn_ctx;
} Condition_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_Condition;

#ifdef __cplusplus
}
#endif

#endif	/* _Condition_H_ */
#include "asn_internal.h"
