#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>

#include <datamtoolbox-v2/libint13chs/int13chs.h>

/* useful */
const char *int13cnv_default_large_chs = "16384/255/63";

/* "C/H/S" -> C/H/S */
int int13cnv_parse_chs_geometry(struct chs_geometry_t *chs,const char *sgeo) {
	// cylinder
	if (!isdigit(*sgeo)) return -1;
	chs->cylinders = (uint16_t)strtoul(sgeo,(char**)(&sgeo),0);
	if (*sgeo != '/') return -1;
	sgeo++;

	// head
	if (!isdigit(*sgeo)) return -1;
	chs->heads = (uint16_t)strtoul(sgeo,(char**)(&sgeo),0);
	if (*sgeo != '/') return -1;
	sgeo++;

	// sectors
	if (!isdigit(*sgeo)) return -1;
	chs->sectors = (uint16_t)strtoul(sgeo,(char**)(&sgeo),0);
	if (*sgeo != 0) return -1;

	return 0;
}

int int13cnv_is_chs_geometry_valid(const struct chs_geometry_t *chs) {
	if (chs == NULL) return 0;
	if (chs->cylinders == 0) return 0;
	if (chs->heads == 0 || chs->heads > 256) return 0;
	if (chs->sectors == 0 || chs->sectors > 63) return 0;
	return 1;
}

int int13cnv_is_chs_location_valid(const struct chs_geometry_t *chs) {
	if (chs == NULL) return 0;
	if (chs->sectors == 0) return 0;
	return 1;
}

int int13cnv_lba_to_chs(struct chs_geometry_t *chs_res,const struct chs_geometry_t *chs_geo,uint32_t lba) {
	if (chs_res == NULL || chs_geo == NULL) return -1;

	/* this code assumes you validated the geometry, and that heads <= 256 and sectors <= 63 and none are zero */
	/* it also assumes the LBA is in range. */

	chs_res->sectors = 1U + ((uint16_t)(lba % (uint32_t)chs_geo->sectors));
	lba /= (uint32_t)chs_geo->sectors;

	chs_res->heads = ((uint16_t)(lba % (uint32_t)chs_geo->heads));
	lba /= (uint32_t)chs_geo->heads;

	chs_res->cylinders = (uint16_t)lba;
	return 0;
}

int int13cnv_chs_to_lba(uint32_t *lba,const struct chs_geometry_t *chs_geo,const struct chs_geometry_t *chs_res) {
	uint32_t res;

	if (chs_res == NULL || chs_geo == NULL) return -1;
	if (chs_res->sectors == 0) return -1;

	/* this code assumes you validated the geometry, and that heads <= 256 and sectors <= 63 and none are zero */
	/* it also assumes the LBA is in range. */
	res  = (uint32_t)chs_res->sectors - (uint32_t)1;
	res += (uint32_t)chs_res->heads * (uint32_t)chs_geo->sectors;
	res += (uint32_t)chs_res->cylinders * (uint32_t)chs_geo->sectors * (uint32_t)chs_geo->heads;
	*lba = res;
	return 0;
}

int int13cnv_chs_to_int13(struct int13h_packed_geometry_t *pck,const struct chs_geometry_t *chs) {
	if (chs == NULL || pck == NULL) return -1;
	if (chs->sectors == 0) return -1;
	if (chs->cylinders >= 1024) return -1;
	if (chs->sectors >= 64) return -1;
	if (chs->heads >= 256) return -1;

	pck->CL = (uint8_t)chs->sectors + (uint8_t)((chs->cylinders >> 8) << 6);
	pck->CH = (uint8_t)chs->cylinders;
	pck->DH = (uint8_t)chs->heads;
	return 0;
}

int int13cnv_int13_to_chs(struct chs_geometry_t *chs,const struct int13h_packed_geometry_t *pck) {
	if (chs == NULL || pck == NULL) return -1;

	chs->sectors = (pck->CL & 0x3F);
	chs->cylinders = pck->CH + ((pck->CL >> 6) << 8);
	chs->heads = pck->DH;
	return 0;
}

int int13cnv_parse_int13_pair(struct int13h_packed_geometry_t *pck,const char *s) {
	if (!isdigit(*s)) return -1;
	pck->CL = (uint8_t)strtoul(s,(char**)(&s),0);
	if (*s != ',') return -1;
	s++;

	if (!isdigit(*s)) return -1;
	pck->CH = (uint8_t)strtoul(s,(char**)(&s),0);
	if (*s != ',') return -1;
	s++;

	if (!isdigit(*s)) return -1;
	pck->DH = (uint8_t)strtoul(s,(char**)(&s),0);
	if (*s != 0) return -1;

	return 0;
}

