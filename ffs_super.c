// FSO 2022-2023

#include <stdio.h>
#include <string.h>

#include "disk_driver.h"
extern struct disk_operations disk_ops;

#include "ffs_super.h"
#include "ffs_bytemap.h"
extern struct bytemap_operations bmap_ops;

#include "ffs_inode.h"
#define SUPER_BLOCK_START 0
#define BMAPINODES_START 1
#define INODES_START 2
#define INODES_PER_BLOCKS 32

struct IMsuper ffs_IMsb; // one in-memory SB only


void super_debug(struct IMsuper *imSB, unsigned int dbg);


/***
  create: To be called by format(), requires NOT mounted as it
          overwrites the in-memory SB structure.
	  DOES NOT UPDATE the disk superblock, that must be an explicit
	  call to the disk driver write
    parameters:
     @in: pointer to superblock structure, disk size (blocks),
	  number of blocks to allocate for inodes (sizeInArea),
	  size of a cluster (clusterSize)
     @out: none
 ***/
static void super_create(struct super *sb, unsigned int nblocks, \
                  unsigned int sizeInArea, unsigned int clusterSize) {
    //sb->fsmagic = 
    //  etc
    /*** TODO ***/
    
    //total number of blocks minus the Inode area, the inode bitmaps and the super
    int dt_blocks = nblocks - 2 - sizeInArea;

    //The number of clusters in data field
    int dt_cluster = dt_blocks/clusterSize;

    //The number of blocks needed to bitmap for dt_cluster 
    int bmap_dt_size = dt_cluster/DISK_BLOCK_SIZE + 1;                
    
    //The number of blocks left 
    int resto = dt_blocks - dt_cluster * clusterSize;

    //calculo de bit map de blocos de dados
    if(resto < bmap_dt_size){     
        bmap_dt_size = dt_cluster/DISK_BLOCK_SIZE + 1;                        
        dt_cluster =  dt_cluster - (bmap_dt_size%clusterSize +1);
    }        

    int st_dt_bmap = INODES_START + sizeInArea; 

    sb->fsmagic = FS_MAGIC;
    sb->nblocks = nblocks;
    sb->startInBmap = 1;
    sb->sizeInBmap = 1;
    sb->startInArea = INODES_START;
    sb->sizeInArea = sizeInArea;
    sb->ninodes = sizeInArea * INODES_PER_BLOCKS;
    sb->startDtBmap = st_dt_bmap;
    sb->sizeDtBmap = bmap_dt_size;
    sb->clusterSize = clusterSize;
    sb->startDtArea = st_dt_bmap + bmap_dt_size;
    sb-> nclusters = dt_cluster;
    sb -> mounted = 0;

}


/***
  read: reads in a SB from disk, overwrites the in-mem SB structure.
	Requires disk open at the driver level.
    parameters:
     @out: pointer to superblock structure
***/
static int super_read(struct super *sb) {
    union u_sbBlk sb_u;
    int ercode;

    ercode = disk_ops.read(0,sb_u.data,1);
    if (ercode < 0) return ercode;

    memcpy(sb, &sb_u.sb, sizeof(struct super));
    return 0;
}


/***
  write: writes the in-mem SB structure to disk.
        Requires disk open at the driver level.
    parameters:
     @in: pointer to superblock structure
***/
static int super_write(struct super *sb) {
    union u_sbBlk sb_u;
    int ercode;

    memset(sb_u.data, 0, DISK_BLOCK_SIZE);    // clean...
    memcpy(sb_u.data,sb,sizeof(struct super));

    ercode = disk_ops.write(0,sb_u.data,1);
    if (ercode < 0) return ercode;

    return 0;
}



/***
  mount: mount the superblock and optionally print its info
	 NO other disk should be mounted
    parameters:
     @in: disk filename, pointer to in-mem superblock,
	  debug(1) or not (0)
    errors:
     those from disk driver
***/
static int super_mount(char *diskname, struct IMsuper *imSB, int debug) {
    int ercode;

    ercode = disk_ops.open(diskname,0);
    if (ercode < 0) return ercode;

    ercode = super_read(&imSB->sb);
    if (ercode < 0) return ercode;

    if (debug) super_debug(imSB, 1); // Debug before mounting

    imSB->sb.mounted = 1;
    ercode = super_write(&imSB->sb);
    if (ercode < 0) return ercode;

    if (debug) super_debug(imSB, 1); // Debug after mounting

// *** TODO *** In future these should be in the mount() library call, not here...
    bmap_ops.mount(&imSB->sb);    // in-mem bmap infos: start, size, n.entries

    return 0;
}


/***
  umount: umount the superblock, wipe the in-mem image
    parameters:
      pointer to in-mem superblock
    errors:
     those from disk driver
***/
static int super_umount(struct IMsuper *imSB) {
    int ercode;

    imSB->sb.mounted = 0;
    ercode = super_write(&imSB->sb);
    if (ercode < 0) return ercode;

    ercode = disk_ops.close();
    if (ercode < 0) return ercode;

    memset(imSB,0,sizeof(struct IMsuper));

    return 0;
}


/***
  get*: gets the relevant BFS info from the in-mem SB
    parameters: none
    returns: unsigned int value or address of region
***/
unsigned int super_getStartInBmap(struct super *sb) {
    return sb->startInBmap;
}

unsigned int super_getSizeInBmap(struct super *sb) {
    return sb->sizeInBmap;
}

unsigned int super_getStartInArea(struct super *sb) {
    return sb->startInArea;
}

unsigned int super_getSizeInArea(struct super *sb) {
    return sb->sizeInArea;
}

unsigned int super_getTotalInodes(struct super *sb) {
    return sb->ninodes;
}

unsigned int super_getStartDtBmap(struct super *sb) {
    return sb->startDtBmap;
}

unsigned int super_getSizeDtBmap(struct super *sb) {
    return sb->sizeDtBmap;
}

unsigned int super_getClusterSize(struct super *sb) {
    return sb->clusterSize;
}

unsigned int super_getStartDtArea(struct super *sb) {
    return sb->startDtArea;
}

unsigned int super_getNclusters(struct super *sb) {
    return sb->nclusters;
}

unsigned int super_getMounted(struct super *sb) {
    return sb->mounted;
}


/* Helper functions */

void super_debug(struct IMsuper *imSB, unsigned int dbg) {
    struct super *sb = &imSB->sb;

    if (!dbg) return;

    printf("In-Memory Superblock:\n");
    printf("  dirty            = %s\n", (imSB->dirty) ? "yes" : "no");
    printf("  fsmagic          = 0x%x\n", sb->fsmagic);
    printf("  nblocks          = %u\n", sb->nblocks);
    printf("  startInBmap      = %u\n", sb->startInBmap);
    printf("  sizeInBmap       = %u\n", sb->sizeInBmap);
    printf("  startInArea      = %u\n", sb->startInArea);
    printf("  sizeInArea       = %u\n", sb->sizeInArea);
    printf("  ninodes          = %u\n", sb->ninodes);
    printf("  startDtBmap      = %u\n", sb->startDtBmap);
    printf("  sizeDtBmap       = %u\n", sb->sizeDtBmap);
    printf("  clusterSize      = %u\n", sb->clusterSize);
    printf("  startDtArea      = %u\n", sb->startDtArea);
    printf("  nclusters        = %u\n", sb->nclusters);
    printf("  mounted          = %s\n", (sb->mounted) ? "yes" : "no");
    fflush(stdout);
}

struct super_operations super_ops = {
        .create= super_create,
        .read= super_read,
        .write= super_write,
        .mount= super_mount,
        .umount= super_umount,
        .getStartInBmap= super_getStartInBmap,
        .getSizeInBmap= super_getSizeInBmap,
        .getStartInArea= super_getStartInArea,
        .getSizeInArea= super_getSizeInArea,
        .getTotalInodes= super_getTotalInodes,
        .getStartDtBmap= super_getStartDtBmap,
        .getSizeDtBmap= super_getSizeDtBmap,
        .getClusterSize= super_getClusterSize,
        .getStartDtArea= super_getStartDtArea,
        .getNclusters= super_getNclusters,
        .getMounted= super_getMounted,
        .debug= super_debug
};
