
#include <datamtoolbox-v2/libmsfat/libmsfat.h>

void libmsfat_dirent_lfn_to_str_utf8(char *buf,size_t buflen,const struct libmsfat_lfn_assembly_t *lfn_name);
void libmsfat_dirent_lfn_to_str_utf16le(char *buf,size_t buflen,const struct libmsfat_lfn_assembly_t *lfn_name);

int libmsfat_file_io_ctx_find_in_dir(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_context_t *msfatctx,struct libmsfat_dirent_t *dirent,struct libmsfat_lfn_assembly_t *lfn_name,const char *name,unsigned int flags);

