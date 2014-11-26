/*
 * solowan_rolling.h
 * API of the Solowan deduplication module
 *
 *     Copyright 2014 Universidad Politécnica de Madrid - Center for Open Middleware
 *     (http://www.centeropenmiddleware.com)
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 *         Email: javier.ruiz@centeropenmiddleware.com
 *         This file contains common functions to all deduplication library files.
 *
 * The implementation of De-duplication is based on the algorithm described in:
 * A Protocol-Independent Technique for Eliminating Redundant Network Traffic.
 * Neil T. Spring and David Wetherall.
 * Proceedings SIGCOMM '00
 * Proceedings of the conference on Applications, Technologies, Architectures, and Protocols for Computer Communication.
 * READ THAT ARTICLE BEFORE TRYING TO UNDERSTAND THIS CODE

 */

#ifndef SOLOWAN_ROLLING
#define SOLOWAN_ROLLING

#include <stdint.h>
#include <pthread.h>
// This is the header file for the de-duplicator code (dedup.c) and the uncompressor code (uncomp.c)
// De-duplication is based on the algorithm described in:
// A Protocol-Independent Technique for Eliminating Redundant Network Traffic. 
// Neil T. Spring and David Wetherall. 
// Proceedings SIGCOMM '00 
// Proceedings of the conference on Applications, Technologies, Architectures, and Protocols for Computer Communication.
// READ THAT ARTICLE BEFORE TRYING TO UNDERSTAND THIS CODE

// Fingerpint store has a size FPS_FACTOR greater than the maximum number of fingerprints (FP) stored.
// A hash table should not be too crowded
#define FPS_FACTOR 4

// We calculate MAX_FP_PER_PKT Rabin fingerprints in each packet
// At present, the fingerprint selection algorithm is very simple: 
// The first MAX_FP_PER_PKT fingerprints which have the lowest GAMMA (see below) bits equal to 0 are chosen
// This algirithm is very quick, but performs poorly when there are too many repeated bytes on the packet, as then there will be many identical consecuive fingerprints
// Winnowing should be a much better algortihm for chosing fingerprints. FUTURE WORK.

#define MAX_FP_PER_PKT 32

// The algorithm is agnostic on packet contents. They may include protocol headers, BUT:
// (SEE BELOW) There must be an external indicator in a received packet that allows the receiver to know if the packet is optimized or not
// This indicator may be a flag in the TCP or IP headers
// OUR CODE CANNOT TELL IF A PACKET IS OPTIMIZED, OR NOT, BASED SOLELY ON ITS CONTENTS

// 2^M is the base of the fingerprint modular arithmetic (see section 4.1 of article)
#define M 60

// Factor used in calculations of Rabin fingerprints (a large prime number) (see section 4.1 of article)
#define P 1048583

// Fingerprints are calculated on fingerprints of length = BETA (see figure 4.2 of article for justification of this value)
#define BETA 64

// See above: fingerprints are selected using a simple rule: their lowest GAMMA bits equal to 0 (see figure 4.2 of article for justification of this value)
#define GAMMA 5

// Auxiliary masks depending on M and GAMMA
#define SELECT_FP_MASK ((1<<GAMMA) -1)
#define MOD_MASK ((1L<<M)-1)

#define SEED 0x8BADF00D
#define BYTE_RANGE 256

// Each packet is given an identifier, pktId
// According to article, a 64 bit integer is used to keep pktId
// 2^63 bits is a very, very large number
// No need for _modular_ increment of pktId
// At 1 Gpps (approx. 2^30 pps), we would need 2^33 seconds to wrap pktId values
// That is, centuries before wrapping values
// This software won't run so much time
#define MAX_PKT_ID INT64_MAX

// Type definition for the generic fingerprint entries
typedef struct {
	// Fingerprint value
	uint64_t fp;
	// pktId of the packet holding the string with the previous fingerprint
	int64_t pktId;
	// offset (from the beginning of the packet) where the string begins
        uint16_t offset;
} FPEntryB;

// Type definition for the generic fingerprint entries in the fingerprint store of the compressor
#define PKTS_PER_FP 3
typedef struct {
	FPEntryB *pkts;
} FPEntry;

// Type definition for the fingerprint store
// The host machine should have enough RAM
// AND SWAP SHOULD BE DISABLED 
typedef FPEntry *FPStore;

// Type definition for a data packet
typedef unsigned char *Pkt;

// Type definition for the entries in the packet store
typedef struct {
	// Data packet
	Pkt pkt;
	// Actual packet length
        uint16_t len;
	// Packet hash
        uint32_t hash;
} PktEntry;


// Packet store
typedef struct {
	PktEntry *pkts;
	int64_t pktId;
} PktStore;

typedef struct {
	uint64_t inputBytes;
	uint64_t outputBytes;
	uint64_t processedPackets;
	uint64_t compressedPackets;
	uint64_t numOfFPEntries;
	uint64_t lastPktId;
	uint64_t numberOfFPHashCollisions;
	uint64_t numberOfFPCollisions;
	uint64_t numberOfShortPkts;
	uint64_t numberOfPktsProcessedInPutInCacheCall;
	uint64_t numberOfShortPktsInPutInCacheCall;
	uint64_t numberOfRMObsoleteOrBadFP;
	uint64_t numberOfRMLinearSearches;
	uint64_t numberOfRMCannotFind;
	uint64_t uncompressedPackets;
	uint64_t errorsMissingFP;
	uint64_t errorsMissingPacket;
	uint64_t errorsPacketFormat;
	uint64_t errorsPacketHash;
} Statistics;


// Calculate relevant fingerprints of a given packet
#define MAX_ITER 4
unsigned int calculateRelevantFPs(FPEntryB *pktFps, unsigned char *packet, uint16_t pktlen);

// Read and write bytes from/to network
void hton16(unsigned char *p, uint16_t n) ;
void hton32(unsigned char *p, uint32_t n) ;
void hton64(unsigned char *p, uint64_t n) ;
uint16_t ntoh16(unsigned char *p) ;
uint32_t ntoh32(unsigned char *p) ;
uint64_t ntoh64(unsigned char *p) ;


inline unsigned int MAX_PKT_SIZE(void);
inline unsigned int PKT_STORE_SIZE(void);
inline unsigned int FP_STORE_SIZE(void);


// Dictionary (PacketStore and FPStore) API
// UNSAFE FUNCTIONS, must be called inside code with locks

inline FPEntryB *getFPhash(FPStore fpStore, PktStore *pktStore, uint64_t fp, uint32_t pktHash);
inline FPEntryB *getFPcontent(FPStore fpStore, PktStore *pktStore, uint64_t fp, unsigned char *chunk);
inline PktEntry *getPkt(PktStore *pktStore, int64_t pktId);
inline PktEntry *getPktHash(PktStore *pktStore, uint32_t pktHash);
inline int64_t putPkt(PktStore *pktStore, unsigned char *pkt, uint16_t pktlen, uint32_t pktHash);
inline void putFP(FPStore fpStore, PktStore *pktStore, uint64_t fp, int64_t pktId, uint16_t offset, Statistics *st);

// Common API functions 

// Statistics handling
// Deduplicator object definition
// It can hold state for both compresion and decompression
typedef struct {
  pthread_mutex_t cerrojo;
  Statistics compStats;
  FPStore fps;
  PktStore ps;
} Deduplicator, *pDeduplicator;

void getStatistics(pDeduplicator pd, Statistics *cs);
void resetStatistics(pDeduplicator pd);

// Common initialization function
extern void init_common(unsigned int pktStoreSize, unsigned int pktSize);

// Deduplicator object creation
extern pDeduplicator newDeduplicator(void);
// Deduplication API 

// Uncompression return
#define	UNCOMP_OK			0
#define	UNCOMP_FP_NOT_FOUND		1
#define	UNCOMP_BAD_PACKET_FORMAT	2
#define	UNCOMP_BAD_PACKET_HASH		3
#define	UNCOMP_ERROR			255

typedef struct {
	unsigned char code;
	uint64_t fp;
	uint32_t hash;
} UncompReturnStatus;

// De-duplication function
// Input parameter: packet (pointer to an array of unsigned char holding the packet to be optimized)
// Input parameter: pktlen (actual length of packet -- 16 bit unsigned integer)
// Output parameter: optpkt (pointer to an array of unsigned char where the optimized packet contents are copied). VERY IMPORTANT: THE CALLER MUST ALLOCATE THIS ARRAY.
// Output parameter: optlen (actual length of optimized packet -- pointer to a 16 bit unsigned integer). 
// VERY IMPORTANT: IF OPTLEN IS THE SAME AS PKTLEN, NO OPTIMIZATION IS POSSIBLE AND OPTPKT HAS NO VALID CONTENTS
extern void dedup(pDeduplicator pd, unsigned char *packet, uint16_t pktlen, unsigned char *optpkt, uint16_t *optlen);

// update cache in compressor function
// Input parameter: packet (pointer to an array of unsigned char holding the packet to be optimized)
// Input parameter: pktlen (actual length of packet -- 16 bit unsigned integer)
extern void put_in_cache(pDeduplicator pd, unsigned char *packet, uint16_t pktlen);

// Uncompression API (implemented in uncomp.c)

// Update uncompressor packet cache
// Input parameter: packet (pointer to an array of unsigned char holding an uncompressed received packet)
// Input parameter: pktlen (actual length of packt -- 16 bit unsigned integer)
extern void update_caches(pDeduplicator pd, unsigned char *packet, uint16_t pktlen);

// Uncompress received optimized packet
// Input parameter: optpkt (pointer to an array of unsigned char holding an optimized packet). 
// Input parameter: optlen (actual length of optimized packet -- 16 bit unsigned integer). 
// Output parameter: packet (pointer to an array of unsigned char holding the uncompressed packet). VERY IMPORTANT: THE CALLER MUST ALLOCATE THIS ARRAY
// Output parameter: pktlen (actual length of uncompressed packet -- pointer to a 16 bit unsigned integer)
// Output parameter: status (pointer to UncompReturnStatus, holding the return status of the call. Must be allocated by caller.)
//		status.code == UNCOMP_OK		packet successfully uncompressed, packet and pktlen output parameters updated 
//		status.code == UNCOMP_FP_NOT_FOUND	packet unsuccessfully uncompressed due to a fingerprint not found or not matching stored hash
//								packet and pktlen not modified
//								status.fp and status.hash hold the missed values
//		status.code == UNCOMP_BAD_PACKET_HASH	packet cannot be uncompressed because packet hash validation failed
//		status.code == UNCOMP_BAD_PACKET_FORMAT	packet cannot be uncompressed because of erroneous format
// This function also calls update_caches when the packet is successfully uncompressed, no need to call update_caches externally
extern void uncomp(pDeduplicator pd, unsigned char *packet, uint16_t *pktlen, unsigned char *optpkt, uint16_t optlen, UncompReturnStatus *status);

#endif

