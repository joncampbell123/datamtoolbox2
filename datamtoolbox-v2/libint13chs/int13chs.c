#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>

#include <datamtoolbox-v2/libint13chs/int13chs.h>

/* "C/H/S" -> C/H/S */
int int13cnv_parse_chs_geometry(struct chs_geometry_t *chs,const char *sgeo) {
	// cylinder
	if (!isdigit(*sgeo)) return -1;
	chs->cylinders = (uint16_t)strtoul(sgeo,(char**)(&sgeo),0);
	if (!chs->cylinders) return -1;
	if (*sgeo != '/') return -1;
	sgeo++;

	// head
	if (!isdigit(*sgeo)) return -1;
	chs->heads = (uint16_t)strtoul(sgeo,(char**)(&sgeo),0);
	if (!chs->heads) return -1;
	if (*sgeo != '/') return -1;
	sgeo++;

	// sectors
	if (!isdigit(*sgeo)) return -1;
	chs->sectors = (uint16_t)strtoul(sgeo,(char**)(&sgeo),0);
	if (!chs->sectors) return -1;
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

