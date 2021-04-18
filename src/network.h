#pragma once
/**
 * @brief Conversion between host and network format for uint16_t and uint32_t integers to handle differences between little and
 * big endian architectures.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

#define htons(x) x
#define ntohs(x) x
#define htonl(x) x
#define ntohl(x) x

#else
// The Pico is little endian !
#define IS_LITTLE_ENDIAN
#define htons(x) ( ((x)<< 8 & 0xFF00) | ((x)>> 8 & 0x00FF) )
#define ntohs(x) htons(x)
#define htonl(x) ( ((x)<<24 & 0xFF000000UL) | ((x)<< 8 & 0x00FF0000UL) | ((x)>> 8 & 0x0000FF00UL) | ((x)>>24 & 0x000000FFUL) )
#define ntohl(x) htonl(x)

#endif
