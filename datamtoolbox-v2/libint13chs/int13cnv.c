#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>

#include <datamtoolbox-v2/libint13chs/int13chs.h>

static struct int13h_packed_geometry_t int13_pck;
static struct chs_geometry_t chs_geo;
static struct chs_geometry_t chs_res;

static int do_lba2chs(const char *s_geometry,const char *s_lba) {
	uint32_t lba;

	if (s_lba == NULL) {
		fprintf(stderr,"You must specify a LBA sector\n");
		return 1;
	}

	if (s_geometry == NULL)
		s_geometry = int13cnv_default_large_chs;

	if (int13cnv_parse_chs_geometry(&chs_geo,s_geometry)) {
		fprintf(stderr,"Failed to parse C/H/S pair\n");
		return 1;
	}
	if (!int13cnv_is_chs_geometry_valid(&chs_geo)) {
		fprintf(stderr,"Invalid C/H/S pair given\n");
		return 1;
	}

	lba = (uint32_t)strtoul(s_lba,NULL,0);

	printf("LBA given:      %lu (0x%08lx)\n",(unsigned long)lba,(unsigned long)lba);
	printf("Geometry (CHS): %u/%u/%u\n",(unsigned int)chs_geo.cylinders,(unsigned int)chs_geo.heads,(unsigned int)chs_geo.sectors);

	if (int13cnv_lba_to_chs(&chs_res,&chs_geo,lba)) {
		fprintf(stderr,"Failed to convert LBA to CHS\n");
		return 1;
	}

	printf("CHS result:     %u/%u/%u\n",(unsigned int)chs_res.cylinders,(unsigned int)chs_res.heads,(unsigned int)chs_res.sectors);
	return 0;
}

static int do_chs2lba(const char *s_geometry,const char *s_chs) {
	uint32_t lba;

	if (s_chs == NULL) {
		fprintf(stderr,"You must specify a CHS pair\n");
		return 1;
	}

	if (s_geometry == NULL)
		s_geometry = int13cnv_default_large_chs;

	if (int13cnv_parse_chs_geometry(&chs_geo,s_geometry)) {
		fprintf(stderr,"Failed to parse C/H/S pair\n");
		return 1;
	}
	if (!int13cnv_is_chs_geometry_valid(&chs_geo)) {
		fprintf(stderr,"Invalid C/H/S pair given\n");
		return 1;
	}

	if (int13cnv_parse_chs_geometry(&chs_res,s_chs)) {
		fprintf(stderr,"Failed to parse C/H/S pair for source\n");
		return 1;
	}
	if (!int13cnv_is_chs_location_valid(&chs_res)) {
		fprintf(stderr,"Invalid C/H/S pair given for source\n");
		return 1;
	}

	printf("CHS given:      %u/%u/%u\n",(unsigned int)chs_res.cylinders,(unsigned int)chs_res.heads,(unsigned int)chs_res.sectors);
	printf("Geometry (CHS): %u/%u/%u\n",(unsigned int)chs_geo.cylinders,(unsigned int)chs_geo.heads,(unsigned int)chs_geo.sectors);

	if (int13cnv_chs_to_lba(&lba,&chs_geo,&chs_res)) {
		fprintf(stderr,"Failed to convert CHS to LBA\n");
		return 1;
	}

	printf("LBA result:     %lu (0x%08lx)\n",(unsigned long)lba,(unsigned long)lba);
	return 0;
}

static int do_chs2int13(const char *s_chs) {
	if (s_chs == NULL) {
		fprintf(stderr,"You must specify a CHS pair\n");
		return 1;
	}

	if (int13cnv_parse_chs_geometry(&chs_res,s_chs)) {
		fprintf(stderr,"Failed to parse C/H/S pair for source\n");
		return 1;
	}
	if (!int13cnv_is_chs_location_valid(&chs_res)) {
		fprintf(stderr,"Invalid C/H/S pair given for source\n");
		return 1;
	}

	if (int13cnv_chs_to_int13(&int13_pck,&chs_res)) {
		fprintf(stderr,"Invalid C/H/S to INT 13h conversion\n");
		return 1;
	}

	printf("CHS given:      %u/%u/%u\n",(unsigned int)chs_res.cylinders,(unsigned int)chs_res.heads,(unsigned int)chs_res.sectors);
	printf("INT 13:         CL=%02xh CH=%02xh DH=0x%02x\n",int13_pck.CL,int13_pck.CH,int13_pck.DH);
	return 0;
}

static int do_int132chs(const char *s_int13) {
	if (s_int13 == NULL) {
		fprintf(stderr,"You must specify INT 13h registers in CL,CH,DH\n");
		return 1;
	}

	if (int13cnv_parse_int13_pair(&int13_pck,s_int13)) {
		fprintf(stderr,"Failed to parse INT 13h pair\n");
		return 1;
	}

	printf("INT 13 given:   CL=%02xh CH=%02xh DH=0x%02x\n",int13_pck.CL,int13_pck.CH,int13_pck.DH);

	if (int13cnv_int13_to_chs(&chs_res,&int13_pck)) {
		fprintf(stderr,"Invalid INT 13h to C/H/S conversion\n");
		return 1;
	}

	printf("CHS result:     %u/%u/%u\n",(unsigned int)chs_res.cylinders,(unsigned int)chs_res.heads,(unsigned int)chs_res.sectors);
	return 0;
}

int main(int argc,char **argv) {
	const char *s_geometry = NULL;
	const char *s_command = NULL;
	const char *s_int13 = NULL;
	const char *s_chs = NULL;
	const char *s_lba = NULL;
	int i;

	for (i=1;i < argc;) {
		const char *a = argv[i++];

		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"c") || !strcmp(a,"command")) {
				s_command = argv[i++];
			}
			else if (!strcmp(a,"geometry")) {
				s_geometry = argv[i++];
			}
			else if (!strcmp(a,"int13")) {
				s_int13 = argv[i++];
			}
			else if (!strcmp(a,"chs")) {
				s_chs = argv[i++];
			}
			else if (!strcmp(a,"lba")) {
				s_lba = argv[i++];
			}
			else {
				fprintf(stderr,"Unknown switch '%s'\n",a);
				return 1;
			}
		}
		else {
			fprintf(stderr,"Unexpected arg '%s'\n",a);
			return 1;
		}
	}

	if (s_command == NULL) {
		fprintf(stderr,"int13cnv -c <command> ...\n");
		fprintf(stderr,"\n");

		fprintf(stderr,"int13cnv -c lba2chs --geometry <cyl/head/sect> --lba <lba>\n");
		fprintf(stderr," ^ convert LBA sector number to C/H/S given the geometry\n");
		fprintf(stderr,"\n");

		fprintf(stderr,"int13cnv -c chs2lba --geometry <cyl/head/sect> --chs <cyl,head,sect>\n");
		fprintf(stderr," ^ convert C/H/S sector number to LBA given the geometry\n");
		fprintf(stderr,"\n");

		fprintf(stderr,"int13cnv -c chs2int13 --chs <cyl/head/sect>\n");
		fprintf(stderr," ^ convert C/H/S sector number to packed form for use with INT 13h\n");
		fprintf(stderr,"\n");

		fprintf(stderr,"int13cnv -c int132chs --int13 <cl,ch,dh>\n");
		fprintf(stderr," ^ convert INT 13h packed form to C/H/S sector number\n");
		fprintf(stderr,"\n");

		return 1;
	}

	if (!strcmp(s_command,"lba2chs"))
		return do_lba2chs(s_geometry,s_lba);
	else if (!strcmp(s_command,"chs2lba"))
		return do_chs2lba(s_geometry,s_chs);
	else if (!strcmp(s_command,"chs2int13"))
		return do_chs2int13(s_chs);
	else if (!strcmp(s_command,"int132chs"))
		return do_int132chs(s_int13);
	else
		fprintf(stderr,"Unknown command '%s'\n",s_command);

	return 1;
}

