
#ifndef DATAMTOOLBOXV2_LIBINT13CHS_INT13CHS_H
#define DATAMTOOLBOXV2_LIBINT13CHS_INT13CHS_H

#if defined(WIN32)
# include <windows.h>
#endif

# include <stdint.h>

# ifdef __cplusplus
extern "C" {
# endif

/* structure to hold disk geometry, NOT in INT 13h format.
 * no provision is made for disks larger than 16384 tracks
 * because by that time the C/H/S geometry mode had been
 * abandoned for LBA disk access. */
struct chs_geometry_t {
	uint16_t	cylinders;
	uint16_t	heads;
	uint16_t	sectors;
};

#pragma pack(push,1)
/* structure to hold packed INT 13h format (for use with AH=2 and AH=3 for disk access) */
/* this exists in the same form as it exists in the MBR record of the partition table */
struct int13h_packed_geometry_t {
	uint8_t		DH;
	uint8_t		CL;
	uint8_t		CH;
// CL[7:6] = cylinder bits 9-8
// CL[5:0] = sector
// CH      = cylinder bits 7-0
// DH      = head
};
#pragma pack(pop)

extern const char *int13cnv_default_large_chs;

int int13cnv_parse_chs_geometry(struct chs_geometry_t *chs,const char *sgeo);
int int13cnv_is_chs_geometry_valid(const struct chs_geometry_t *chs);
int int13cnv_is_chs_location_valid(const struct chs_geometry_t *chs);
int int13cnv_lba_to_chs(struct chs_geometry_t *chs_res,const struct chs_geometry_t *chs_geo,uint32_t lba);
int int13cnv_chs_to_lba(uint32_t *lba,const struct chs_geometry_t *chs_geo,const struct chs_geometry_t *chs_res);
int int13cnv_chs_to_int13(struct int13h_packed_geometry_t *pck,const struct chs_geometry_t *chs);
int int13cnv_parse_int13_pair(struct int13h_packed_geometry_t *pck,const char *s);
int int13cnv_int13_to_chs(struct chs_geometry_t *chs,const struct int13h_packed_geometry_t *pck);

# ifdef __cplusplus
}
# endif

#endif //DATAMTOOLBOXV2_LIBINT13CHS_INT13CHS_H

