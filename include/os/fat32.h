#include <type.h>
#include <pgtable.h>

typedef struct fat{
    uint32_t  first_data_sec;
    uint32_t  data_sec_cnt;
    uint32_t  data_clus_cnt;
    uint32_t  byts_per_clus;

    struct {
        uint16_t  byts_per_sec;
        uint8_t   sec_per_clus;
        uint16_t  rsvd_sec_cnt;
        uint8_t   fat_cnt;            /* count of FAT regions */
        uint32_t  hidd_sec;           /* count of hidden sectors */
        uint32_t  tot_sec;            /* total count of sectors including all regions */
        uint32_t  fat_sz;             /* count of sectors for a FAT region */
        uint32_t  root_clus;
    } bpb;

}fat_t;

typedef struct dentry{

    uchar filename[8];  //7:0
    uchar extname[3];   //10:8
    uchar attribute;    //11
    uchar reserved;     //12
    uint8 create_time_ms;   //13
    uint16 create_time;     //15:14
    uint16 create_date;     //17:16
    uint16 last_visited;    //19:18
    uint16 HI_clusnum;      //21:20
    uint16 last_modified_time;   //23:22
    uint16 last_modified_date;      //25:24
    uint16 LO_clusnum;      //27:26
    uint32 length;          //31:28

}dentry_t;

#define SECTOR_SIZE (fat.bpb.byts_per_sec)
#define CLUSTER_SIZE (fat.byts_per_clus)
#define READ_PAGE_CNT (NORMAL_PAGE_SIZE/fat.bpb.byts_per_sec)
#define READ_CLUS_CNT (CLUSTER_SIZE/fat.bpb.byts_per_sec)

#define FILE_ATTRIBUTE_RW 0x00
#define FILE_ATTRIBUTE_RO 0x01
#define FILE_ATTRIBUTE_HID 0x02
#define FILE_ATTRIBUTE_SYS 0x04
#define FILE_ATTRIBUTE_VOL 0x08
#define FILE_ATTRIBUTE_CHDIR 0x10
#define FILE_ATTRIBUTE_GDIR 0x20

extern fat_t fat;

int8 fat32_read(const char *filename);
uint32_t get_next_cluster(uint32_t cluster);

/* get the first sector num of this cluster */
static inline uint32 first_sec_of_clus(uint32 cluster)
{
    return ((cluster - 2) * fat.bpb.sec_per_clus) + fat.first_data_sec;
}

/* get the sector num of this cluster in TABLE 1*/
static inline uint32 fat_sec_of_clus(uint32 cluster)
{
    return fat.bpb.rsvd_sec_cnt + fat.bpb.hidd_sec + \
        (cluster * 4) / fat.bpb.byts_per_sec + fat.bpb.fat_sz;
}

static inline uint32 get_cluster_from_dentry(dentry_t *p)
{
    return ((uint32_t)p->HI_clusnum << 16) + p->LO_clusnum;
}

static inline uint32 fat_offset_of_clus(uint32 cluster)
{
    return ((cluster) * 4) % fat.bpb.byts_per_sec;
}
