#include "libxl_osdeps.h"
#include "libxl_internal.h"
#include "libxl_arch.h"

#define SSHM_PATH(id) GCSPRINTF("/libxl/static_shm/%s", id)

#define SSHM_ERROR(domid, sshmid, f, ...)                               \
    LOGD(ERROR, domid, "static_shm id = %s: " f, sshmid, ##__VA_ARGS__)


/* Set default values for libxl_static_shm */
int libxl__sshm_setdefault(libxl__gc *gc, uint32_t domid,
                           libxl_static_shm *sshm)
{
    int rc;

    if (sshm->role != LIBXL_SSHM_ROLE_SLAVE &&
        sshm->role != LIBXL_SSHM_ROLE_MASTER)
        return ERROR_INVAL;
    if (sshm->begin & ~XC_PAGE_MASK ||
        sshm->size & ~XC_PAGE_MASK ||
        (sshm->offset != LIBXL_SSHM_RANGE_UNKNOWN &&
        sshm->offset & ~XC_PAGE_MASK)) {
        SSHM_ERROR(domid, sshm->id,
                   "begin/size/offset is not a multiple of 4K");
        return ERROR_INVAL;
    }

    /* role-specific checks */
    if (sshm->role == LIBXL_SSHM_ROLE_SLAVE) {
        if (sshm->offset == LIBXL_SSHM_RANGE_UNKNOWN)
            sshm->offset = 0;
        if (sshm->cache_policy != LIBXL_SSHM_CACHEPOLICY_UNKNOWN) {
            SSHM_ERROR(domid, sshm->id,
                       "cache_policy is only applicable to master domains");
            rc = ERROR_INVAL;
            goto out;
        }
    } else {
        if (sshm->offset != LIBXL_SSHM_RANGE_UNKNOWN) {
            SSHM_ERROR(domid, sshm->id,
                       "offset is only applicable to slave domains");
            rc = ERROR_INVAL;
            goto out;
        }

        rc = libxl__arch_domain_sshm_cachepolicy_setdefault(sshm);
        if (rc) {
            SSHM_ERROR(domid, sshm->id,
                       "cache policy not supported on this platform");
            goto out;
        }
    }

    rc = 0;
out:
    return rc;
}

/* Comparator for sorting sshm ranges by sshm->begin */
static int sshm_range_cmp(const void *a, const void *b)
{
    libxl_static_shm *const *sshma = a, *const *sshmb = b;
    return (*sshma)->begin > (*sshmb)->begin ? 1 : -1;
}

/* Check if the sshm slave configs in @sshm overlap */
int libxl__sshm_check_overlap(libxl__gc *gc, uint32_t domid,
                                     libxl_static_shm *sshms, int len)
{

    const libxl_static_shm **slave_sshms = NULL;
    int num_slaves;
    int i;

    if (!len) return 0;

    slave_sshms = libxl__calloc(gc, len, sizeof(slave_sshms[0]));
    num_slaves = 0;
    for (i = 0; i < len; ++i) {
        if (sshms[i].role == LIBXL_SSHM_ROLE_SLAVE)
            slave_sshms[num_slaves++] = sshms + i;
    }
    qsort(slave_sshms, num_slaves, sizeof(slave_sshms[0]), sshm_range_cmp);

    for (i = 0; i < num_slaves - 1; ++i) {
        if (slave_sshms[i+1]->begin <
            slave_sshms[i]->begin + slave_sshms[i]->size) {
            SSHM_ERROR(domid, slave_sshms[i+1]->id, "slave ranges overlap.");
            return ERROR_INVAL;
        }
    }

    return 0;
}

/*
 * Decrease the refcount of an sshm. When refcount reaches 0,
 * clean up the whole sshm path.
 * Xenstore operations are done within the same transaction.
 */
static void libxl__sshm_decref(libxl__gc *gc, xs_transaction_t xt,
                               const char *sshm_path)
{
    int count;
    const char *count_path, *count_string;

    count_path = GCSPRINTF("%s/usercnt", sshm_path);
    if (libxl__xs_read_checked(gc, xt, count_path, &count_string))
        return;
    count = atoi(count_string);

    if (--count == 0) {
        libxl__xs_path_cleanup(gc, xt, sshm_path);
        return;
    }

    count_string = GCSPRINTF("%d", count);
    libxl__xs_write_checked(gc, xt, count_path, count_string);

    return;
}

static void libxl__sshm_do_unmap(libxl__gc *gc, uint32_t domid, const char *id,
                                 uint64_t begin, uint64_t size)
{
    uint64_t end;
    begin >>= XC_PAGE_SHIFT;
    size >>= XC_PAGE_SHIFT;
    end = begin + size;
    for (; begin < end; ++begin) {
        if (xc_domain_remove_from_physmap(CTX->xch, domid, begin)) {
            SSHM_ERROR(domid, id,
                       "unable to unmap shared page at 0x%"PRIx64".",
                       begin);
        }
    }
}

static void libxl__sshm_del_slave(libxl__gc *gc, xs_transaction_t xt,
                                  uint32_t domid, const char *id, bool isretry)
{
    const char *slave_path, *begin_str, *size_str;
    uint64_t begin, size;

    slave_path = GCSPRINTF("%s/slaves/%"PRIu32, SSHM_PATH(id), domid);

    begin_str = libxl__xs_read(gc, xt, GCSPRINTF("%s/begin", slave_path));
    size_str = libxl__xs_read(gc, xt, GCSPRINTF("%s/size", slave_path));
    begin = strtoull(begin_str, NULL, 16);
    size = strtoull(size_str, NULL, 16);

    libxl__sshm_do_unmap(gc, domid, id, begin, size);
    libxl__xs_path_cleanup(gc, xt, slave_path);
}

/* Delete static_shm entries in the xensotre. */
int libxl__sshm_del(libxl__gc *gc,  uint32_t domid)
{
    int rc, i;
    bool isretry;
    xs_transaction_t xt = XBT_NULL;
    const char *dom_path, *dom_sshm_path, *role;
    char **sshm_ents;
    unsigned int sshm_num;

    dom_path = libxl__xs_get_dompath(gc, domid);
    dom_sshm_path = GCSPRINTF("%s/static_shm", dom_path);

    isretry = false;
    for (;;) {
        rc = libxl__xs_transaction_start(gc, &xt);
        if (rc) goto out;

        if (libxl__xs_read(gc, xt, dom_sshm_path)) {
            sshm_ents = libxl__xs_directory(gc, xt, dom_sshm_path, &sshm_num);
            if (!sshm_ents) continue;

            for (i = 0; i < sshm_num; ++i) {
                role = libxl__xs_read(gc, xt,
                                      GCSPRINTF("%s/%s/role",
                                                dom_sshm_path,
                                                sshm_ents[i]));
                assert(role);
                if (!strncmp(role, "slave", 5))
                    libxl__sshm_del_slave(gc, xt, domid, sshm_ents[i], isretry);

                libxl__sshm_decref(gc, xt, SSHM_PATH(sshm_ents[i]));
            }
        }

        rc = libxl__xs_transaction_commit(gc, &xt);
        if (!rc) break;
        if (rc < 0) goto out;
        isretry = true;
    }

    rc = 0;
out:
    libxl__xs_transaction_abort(gc, &xt);
    return rc;
}

/*   libxl__sshm_do_map -- map pages into slave's physmap
 *
 *   This functions maps
 *     master gfn: [@msshm->begin + @sshm->offset,
 *                  @msshm->begin + @msshm->size + @sshm->offset)
 *   into
 *     slave gfn: [@sshm->begin, @sshm->begin + @sshm->size)
 *
 *   The gfns of the pages that are successfully mapped will be stored
 *   in @mapped, and the number of the gfns will be stored in @nmapped.
 *
 *   The caller has to guarantee that all the values are page-aligned.
 */
static int libxl__sshm_do_map(libxl__gc *gc, uint32_t mid, uint32_t sid,
                              libxl_static_shm *sshm, libxl_static_shm *msshm,
                              xen_pfn_t *mapped, unsigned int *nmapped)
{
    int rc;
    int i;
    xen_pfn_t num_mpages, num_spages, num_success, offset;
    int *errs;
    xen_ulong_t *idxs;
    xen_pfn_t *gpfns;

    num_mpages = (msshm->size) >> XC_PAGE_SHIFT;
    num_spages = (sshm->size) >> XC_PAGE_SHIFT;
    offset = sshm->offset >> XC_PAGE_SHIFT;

    /* Check range. Test offset < mpages first to avoid overflow */
    if ((offset >= num_mpages) || (num_mpages - offset < num_spages)) {
        SSHM_ERROR(sid, sshm->id, "exceeds master's address space.");
        rc = ERROR_INVAL;
        goto out;
    }

    /* fill out the gfn's and do the mapping */
    errs = libxl__calloc(gc, num_spages, sizeof(int));
    idxs = libxl__calloc(gc, num_spages, sizeof(xen_ulong_t));
    gpfns = libxl__calloc(gc, num_spages, sizeof(xen_pfn_t));
    for (i = 0; i < num_spages; i++) {
        idxs[i] = (msshm->begin >> XC_PAGE_SHIFT) + offset + i;
        gpfns[i]= (sshm->begin >> XC_PAGE_SHIFT) + i;
    }
    rc = xc_domain_add_to_physmap_batch(CTX->xch,
                                        sid, mid,
                                        XENMAPSPACE_gmfn_share,
                                        num_spages,
                                        idxs, gpfns, errs);

    num_success = 0;
    for (i = 0; i < num_spages; i++) {
        if (errs[i]) {
            SSHM_ERROR(sid, sshm->id,
                       "can't map at address 0x%"PRIx64".",
                       gpfns[i] << XC_PAGE_SHIFT);
            rc = ERROR_FAIL;
        } else {
            mapped[num_success++] = gpfns[i];
        }
    }
    *nmapped = num_success;
    if (rc) goto out;

    rc = 0;
out:
    return rc;
}

/* Xenstore ops are protected by a transaction */
static int libxl__sshm_incref(libxl__gc *gc, xs_transaction_t xt,
                              const char *sshm_path)
{
    int rc, count;
    const char *count_path, *count_string;

    count_path = GCSPRINTF("%s/usercnt", sshm_path);
    rc = libxl__xs_read_checked(gc, xt, count_path, &count_string);
    if (rc) goto out;
    count = atoi(count_string);

    count_string = GCSPRINTF("%d", count+1);
    rc = libxl__xs_write_checked(gc, xt, count_path, count_string);
    if (rc) goto out;

    rc = 0;
out:
    return rc;
}

static int libxl__sshm_add_slave(libxl__gc *gc, uint32_t domid,
                                 libxl_static_shm *sshm)
{
    int rc, i;
    const char *sshm_path, *slave_path;
    const char *dom_path, *dom_sshm_path, *dom_role_path;
    const char *xs_value;
    char *ents[9];
    libxl_static_shm master_sshm;
    uint32_t master_domid;
    xen_pfn_t *mapped;
    unsigned int nmapped = 0;
    xs_transaction_t xt = XBT_NULL;
    bool isretry;

    sshm_path = SSHM_PATH(sshm->id);
    slave_path = GCSPRINTF("%s/slaves/%"PRIu32, sshm_path, domid);
    dom_path = libxl__xs_get_dompath(gc, domid);
    /* the domain should be in xenstore by now */
    assert(dom_path);
    dom_sshm_path = GCSPRINTF("%s/static_shm/%s", dom_path, sshm->id);
    dom_role_path = GCSPRINTF("%s/role", dom_sshm_path);

    /* prepare the slave xenstore entries */
    ents[0] = "begin";
    ents[1] = GCSPRINTF("0x%"PRIx64, sshm->begin);
    ents[2] = "size";
    ents[3] = GCSPRINTF("0x%"PRIx64, sshm->size);
    ents[4] = "offset";
    ents[5] = GCSPRINTF("0x%"PRIx64, sshm->offset);
    ents[6] = "prot";
    ents[7] = libxl__strdup(gc, libxl_sshm_prot_to_string(sshm->prot));
    ents[8] = NULL;

    mapped = libxl__calloc(gc, sshm->size >> XC_PAGE_SHIFT, sizeof(xen_pfn_t));

    isretry = false;
    for (;;) {
        rc = libxl__xs_transaction_start(gc, &xt);
        if (rc) goto out;

        if (!libxl__xs_read(gc, xt, sshm_path)) {
            SSHM_ERROR(domid, sshm->id, "no master found.");
            rc = ERROR_FAIL;
            goto out;
        }

        /* every ID can appear in each domain at most once */
        if (libxl__xs_read(gc, xt, dom_sshm_path)) {
            SSHM_ERROR(domid, sshm->id,
                       "domain tried to map the same ID twice.");
            rc = ERROR_FAIL;
            goto out;
        }

        /* look at the master info and see if we could do the mapping */
        rc = libxl__xs_read_checked(gc, xt,
                                    GCSPRINTF("%s/prot", sshm_path),
                                    &xs_value);
        if (rc) goto out;
        libxl_sshm_prot_from_string(xs_value, &master_sshm.prot);

        rc = libxl__xs_read_checked(gc, xt,
                                    GCSPRINTF("%s/begin", sshm_path),
                                    &xs_value);
        if (rc) goto out;
        master_sshm.begin = strtoull(xs_value, NULL, 16);

        rc = libxl__xs_read_checked(gc, xt,
                                    GCSPRINTF("%s/size", sshm_path),
                                    &xs_value);
        if (rc) goto out;
        master_sshm.size = strtoull(xs_value, NULL, 16);

        rc = libxl__xs_read_checked(gc, xt,
                                    GCSPRINTF("%s/master", sshm_path),
                                    &xs_value);
        if (rc) goto out;
        master_domid = strtoull(xs_value, NULL, 16);

        if (sshm->prot == LIBXL_SSHM_PROT_UNKNOWN)
            sshm->prot = master_sshm.prot;

        /* check if the slave is asking too much permission */
        if (master_sshm.prot < sshm->prot) {
            SSHM_ERROR(domid, sshm->id, "slave is asking too much permission.");
            rc = ERROR_INVAL;
            goto out;
        }

        /* all checks passed, do the job */
        if (!isretry) {
            rc = libxl__sshm_do_map(gc, master_domid, domid,
                                    sshm, &master_sshm,
                                    mapped, &nmapped);
            if (rc) goto out;
        }

        /* write the result to xenstore and commit */
        rc = libxl__xs_write_checked(gc, xt, dom_role_path, "slave");
        if (rc) goto out;
        rc = libxl__xs_writev(gc, xt, slave_path, ents);
        if (rc) goto out;
        rc = libxl__sshm_incref(gc, xt, sshm_path);
        if (rc) goto out;

        rc = libxl__xs_transaction_commit(gc, &xt);
        if (!rc) break;
        if (rc < 0) goto out;
        isretry = true;
    }

    rc = 0;
out:
    if (rc) {
        /* roll back successfully mapped pages */
        SSHM_ERROR(domid, sshm->id, "failed to map some pages, cancelling.");
        for (i = 0; i < nmapped; i++) {
            xc_domain_remove_from_physmap(CTX->xch, domid, mapped[i]);
        }
    }

    libxl__xs_transaction_abort(gc, &xt);

    return rc;
}

static int libxl__sshm_add_master(libxl__gc *gc, uint32_t domid,
                                  libxl_static_shm *sshm)
{
    int rc;
    const char *sshm_path, *dom_path, *dom_role_path;
    char *ents[13];
    struct xs_permissions noperm;
    xs_transaction_t xt = XBT_NULL;

    sshm_path = SSHM_PATH(sshm->id);
    dom_path = libxl__xs_get_dompath(gc, domid);
    /* the domain should be in xenstore by now */
    assert(dom_path);
    dom_role_path = GCSPRINTF("%s/static_shm/%s/role", dom_path, sshm->id);

    /* prepare the xenstore entries */
    ents[0] = "master";
    ents[1] = GCSPRINTF("%"PRIu32, domid);
    ents[2] = "begin";
    ents[3] = GCSPRINTF("0x%"PRIx64, sshm->begin);
    ents[4] = "size";
    ents[5] = GCSPRINTF("0x%"PRIx64, sshm->size);
    ents[6] = "prot";
    ents[7] = libxl__strdup(gc, libxl_sshm_prot_to_string(sshm->prot));
    ents[8] = "cache_policy";
    ents[9] = libxl__strdup(gc,
                            libxl_sshm_cachepolicy_to_string(sshm->cache_policy));
    ents[10] = "usercnt";
    ents[11] = "1";
    ents[12] = NULL;

    /* could only be accessed by Dom0 */
    noperm.id = 0;
    noperm.perms = XS_PERM_NONE;

    for (;;) {
        rc = libxl__xs_transaction_start(gc, &xt);
        if (rc) goto out;

        if (!libxl__xs_read(gc, xt, sshm_path)) {
            /* every ID can appear in each domain at most once */
            if (libxl__xs_read(gc, xt, dom_role_path)) {
                SSHM_ERROR(domid, sshm->id,
                           "domain tried to map the same ID twice.");
                rc = ERROR_FAIL;
                goto out;
            }
            rc = libxl__xs_write_checked(gc, xt, dom_role_path, "master");
            if (rc) goto out;;

            libxl__xs_mknod(gc, xt, sshm_path, &noperm, 1);
            libxl__xs_writev(gc, xt, sshm_path, ents);
        } else {
            SSHM_ERROR(domid, sshm->id, "can only have one master.");
            rc = ERROR_FAIL;
            goto out;
        }

        rc = libxl__xs_transaction_commit(gc, &xt);
        if (!rc) break;
        if (rc < 0) goto out;
    }

    rc = 0;
out:
    libxl__xs_transaction_abort(gc, &xt);
    return rc;
}

int libxl__sshm_add(libxl__gc *gc,  uint32_t domid,
                    libxl_static_shm *sshms, int len)
{
    int rc, i;

    for (i = 0; i < len; ++i) {
        if (sshms[i].role == LIBXL_SSHM_ROLE_SLAVE) {
            rc = libxl__sshm_add_slave(gc, domid, sshms+i);
        } else {
            rc = libxl__sshm_add_master(gc, domid, sshms+i);
        }
        if (rc)  return rc;
    }

    return 0;
}

/*
 * Local variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */