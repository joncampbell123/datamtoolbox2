
#include <datamtoolbox-v2/libmsfat/libmsfat.h>

void libmsfat_dirent_lfn_to_str_utf8(char *buf,size_t buflen,const struct libmsfat_lfn_assembly_t *lfn_name);
void libmsfat_dirent_lfn_to_str_utf16le(char *buf,size_t buflen,const struct libmsfat_lfn_assembly_t *lfn_name);
int libmsfat_dirent_utf8_str_to_lfn(struct libmsfat_dirent_t *dirent,struct libmsfat_lfn_assembly_t *lfn_name,const char *name,unsigned int hash);
int libmsfat_name_needs_lfn_utf8(const char *name);

int libmsfat_file_io_ctx_find_in_dir(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_context_t *msfatctx,struct libmsfat_dirent_t *dirent,struct libmsfat_lfn_assembly_t *lfn_name,const char *name,unsigned int flags);

int libmsfat_file_io_ctx_path_lookup(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_file_io_ctx_t *fioctx_parent,
	struct libmsfat_context_t *msfatctx,struct libmsfat_dirent_t *dirent,struct libmsfat_lfn_assembly_t *lfn_name,
	const char *path,unsigned int flags);
#define libmsfat_path_lookup_CREATE		(1U << 0U)
#define libmsfat_path_lookup_DIRECTORY		(1U << 1U)

