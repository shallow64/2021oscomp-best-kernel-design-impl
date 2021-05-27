#include <type.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <os/mm.h>
#include <os/fat32.h>
#include <os/elf.h>
#include <user_programs.h>
#include <os/sched.h>

#define max(x,y) (((x) > (y)) ? (x) : (y))
#define min(x,y) (((x) > (y)) ? (y) : (x))

#define BUFSIZE (min(NORMAL_PAGE_SIZE, CLUSTER_SIZE))
#define READ_BUF_CNT (BUFSIZE/SECTOR_SIZE)

fat_t fat;
uchar buf[NORMAL_PAGE_SIZE];
uchar filebuf[NORMAL_PAGE_SIZE];
ientry_t cwd_first_clus;
ientry_t cwd_clus;
isec_t cwd_sec;
uchar cwd_path[MAX_PATHLEN];

uchar stdout_buf[NORMAL_PAGE_SIZE];
uchar stdin_buf[NORMAL_PAGE_SIZE];

#define ACCEPT_NUM  1
uchar accept_table[ACCEPT_NUM][10] = {{"MKDIR"}, {"CLONE"}, {"GETPID"}, {"UNAME"},  {"FORK"}, {"GETPPID"}, {"GETTIMEOFDAY"}, 
    {"WAIT"}, {"WAITPID"}, {"EXIT"},{"YIELD"}, {"TIMES"}, {"SLEEP"}};

/* read as many sectors as we can */
void sd_read(char *buf, uint32_t sec)
{
    sd_read_sector(buf, sec, READ_BUF_CNT);
}

/* write as many sectors as we can */
void sd_write(char *buf, uint32_t sec)
{
    for (int i = 0; i < READ_BUF_CNT; ++i)
    {
        sd_write_sector(buf, sec, 1);
        buf += SECTOR_SIZE;
        sec++;
    }    
}

dentry_t *get_next_dentry(dentry_t *p, uchar *dirbuff, ientry_t *now_clus, isec_t *now_sec)
{
    p++;
    if ((uintptr_t)p - (uintptr_t)dirbuff == BUFSIZE){
        // read another
        *now_sec += BUFSIZE / SECTOR_SIZE;
        printk_port("get_now_sec: %d\n", *now_sec);
        if (*now_sec - first_sec_of_clus(*now_clus) == fat.bpb.sec_per_clus){
            *now_clus = get_next_cluster(*now_clus); //another cluster
            *now_sec = first_sec_of_clus(*now_clus);
        }
        sd_read(dirbuff, *now_sec);
        p = dirbuff;
    }
    return p;
}

uint8_t fat32_init()
{
    /* read FAT32 info */
    uint8_t *b = kalloc();
    // read superblock
    sd_read_sector(b, 0, 4);
    // store basic messages
    if (memcmp((char const*)(b + 82), "FAT32", 5))
        printk("not FAT32 volume");
    fat.bpb.byts_per_sec = *(uint8 *)(b + 11) + (*(uint8 *)(b + 12))*256;
    fat.bpb.sec_per_clus = *(b + 13);
    fat.bpb.rsvd_sec_cnt = *(uint16 *)(b + 14);
    fat.bpb.fat_cnt = *(b + 16);
    fat.bpb.hidd_sec = *(uint32 *)(b + 28);
    fat.bpb.tot_sec = *(uint32 *)(b + 32);
    fat.bpb.fat_sz = *(uint32 *)(b + 36);
    fat.bpb.root_clus = *(uint32 *)(b + 44);

    fat.first_data_sec = fat.bpb.rsvd_sec_cnt + fat.bpb.hidd_sec + fat.bpb.fat_cnt * fat.bpb.fat_sz;
    fat.data_sec_cnt = fat.bpb.tot_sec - fat.first_data_sec;
    fat.data_clus_cnt = fat.data_sec_cnt / fat.bpb.sec_per_clus;
    fat.byts_per_clus = fat.bpb.sec_per_clus * fat.bpb.byts_per_sec;

    printk("> [FAT32 init] byts_per_sec: %d\n", fat.bpb.byts_per_sec);
    printk("> [FAT32 init] root_clus: %d\n", fat.bpb.root_clus);
    printk("> [FAT32 init] sec_per_clus: %d\n", fat.bpb.sec_per_clus);
    printk("> [FAT32 init] fat_cnt: %d\n", fat.bpb.fat_cnt);
    printk("> [FAT32 init] fat_sz: %d\n", fat.bpb.fat_sz);
    printk("> [FAT32 init] fat_rsvd: %d\n", fat.bpb.rsvd_sec_cnt);
    printk("> [FAT32 init] fat_hid: %d\n", fat.bpb.hidd_sec);

    /* read root dir and store it for now*/
    cwd_first_clus = fat.bpb.root_clus;
    cwd_clus = fat.bpb.root_clus;
    strcpy(cwd_path, "/");
    cwd_sec = first_sec_of_clus(fat.bpb.root_clus);
    /* from root sector read buffsize */
    sd_read(buf, cwd_sec);
    // printk("root_sec: %d\n\r", root_sec);
    // fat32_read(NULL);
    // printk("success");
    kfree(b);

    return 0;
}

/* read file from sd card and put it into readyqueue*/
/*  1: continue to read
   -1: busy
    0: end
    */ 
int8 fat32_read_test(const char *filename)
{
    fat32_mkdir(AT_FDCWD,"test_mkdir",0);
    while(1);
    // for (int i = 0; i < 40000000; ++i)
    // {
    //     ;
    // }
    static uint32 cnt = 0;
    // printk_port("read %d\n", cnt++);
    // if (cnt > 32){
    //     printk_port("???\n");
    //     assert(0);
    // }

    /* busy */
    for (int i = 1; i < NUM_MAX_TASK; ++i)
    {
        if (pcb[i].status != TASK_EXITED)
            return -1;
    }

    uchar *_elf_binary;
    uint32_t length;

#ifdef K210

    static dentry_t *p = (dentry_t *)buf;
    // printk_port("filename: aa;%s;\n", p->filename);
    // printk_port("extname: ;%s;\n", p->extname);
    // printk_port("attribute: ;%x;\n", p->attribute);
    // printk_port("length: ;%d;\n", p->length);
    // p = get_next_dentry(p);
    // return 1;
    // 0x0f: long dentry
    while (p->attribute == 0x0f || 0xE5 == p->filename[0]){
        // printk_port("<cause 3>\n");
        p = get_next_dentry(p, buf, &cwd_clus, &cwd_sec);
    }
    /* now we are at a real dentry */

    // 0x00: time to end
    uchar *n = (uchar *)p;
    uint8_t index;
    for (index = 0; index < 32; ++index)
    {
        // prints("<n: %d>\n", *n);
        if (*n++ != 0)
            break;
    }
    if (index == 32){
        // printk_port("<return>");
        return 0;
    }

    // printk_port("filename: aa;%s;\n", p->filename);
    // printk_port("extname: ;%s;\n", p->extname);
    // printk_port("attribute: ;%x;\n", p->attribute);
    // printk_port("length: ;%d;\n", p->length);

    // no length or deleted
    if (p->length == 0) {
        // printk_port("<cause 1>\n");
        p = get_next_dentry(p, buf, &cwd_clus, &cwd_sec); 
        return 1;
    }
    // not rw-able
    if (p->attribute != FILE_ATTRIBUTE_GDIR){ 
        // printk_port("<cause 2>\n");
        p = get_next_dentry(p, buf, &cwd_clus, &cwd_sec); return 1;
    }

    // printk_port("filename: bb;%s;\n", p->filename);
    // printk_port("strings: %s %s\n", accept_table[0], accept_table[1]);
    // printk_port("len: %d %d\n", strlen(accept_table[0]), strlen(accept_table[1]));
    // printk_port("I'm here, now %d\n", memcmp(p->filename, accept_table[1], strlen(accept_table[1])));

    /* debug return */
    uint8 i;
    for (i = 0; i < ACCEPT_NUM; ++i)
    {
        uint8 num = (strlen(accept_table[i]) > 6)? 6 : strlen(accept_table[i]);
        if (!memcmp(p->filename, accept_table[i], num)){
            break;
        }
    }
    if (i == ACCEPT_NUM){
        // printk_port("Not match\n");
        // printk_port("filename :%s\n", p->filename);
        // printk_port("table :%s, %s", accept_table[0],accept_table[1]);
        p = get_next_dentry(p, buf, &cwd_clus, &cwd_sec);
        return 1;
    }
    // printk_port("filename: ;%s;\n\r", p->filename);

    // printk("HI: %x\n", p->HI_clusnum);
    // printk("LO: %x\n", p->LO_clusnum);
    // printk("Clusnum: %d %x\n", get_cluster_from_dentry(p), get_cluster_from_dentry(p));

    /* Now we must read file */

    // readfile
    uint32_t cluster = get_cluster_from_dentry(p);
    uint32_t sec = first_sec_of_clus(cluster); // make sure you know the start addr
    uchar *file = allocproc(); // make sure space is enough
    uchar *temp = file; // for future use
    for (uint32_t i = 0; i < p->length; )
    {
        // 1. read 1 cluster
        sd_read(temp, sec);
        i += BUFSIZE;
        temp += BUFSIZE;
        // 2. compute sec number of next buf-size datablock
        if (i % CLUSTER_SIZE == 0){
            cluster = get_next_cluster(cluster);
            sec = first_sec_of_clus(cluster);
        }
        else
            sec += READ_BUF_CNT;
    }

    /* set elf_binary and length */
    _elf_binary = file;
    length = p->length;

#else

    get_elf_file(filename,&_elf_binary,&length);

#endif

    for (uint i = 1; i < NUM_MAX_TASK; ++i)
        if (pcb[i].status == TASK_EXITED)
        {
            // init pcb
            pcb_t *pcb_underinit = &pcb[i];
            ptr_t kernel_stack = allocPage() + NORMAL_PAGE_SIZE;
            ptr_t user_stack = USER_STACK_ADDR;

            init_pcb_default(pcb_underinit, USER_PROCESS);

            // load file to memory
            uintptr_t pgdir = allocPage();
            clear_pgdir(pgdir);
            uint64_t user_stack_page_kva = alloc_page_helper(user_stack - NORMAL_PAGE_SIZE,pgdir,_PAGE_ACCESSED|_PAGE_DIRTY|_PAGE_READ|_PAGE_WRITE|_PAGE_USER);
            uint64_t test_elf = (uintptr_t)load_elf(_elf_binary,length,pgdir,alloc_page_helper);
            share_pgtable(pgdir,pa2kva(PGDIR_PA));

            // copy argc & argv

            // prepare stack
            init_pcb_stack(pgdir, kernel_stack, user_stack, test_elf, 0, user_stack - NORMAL_PAGE_SIZE, pcb_underinit);
            // add to ready_queue
            list_del(&pcb_underinit->list);
            list_add_tail(&pcb_underinit->list,&ready_queue);

            #ifdef K210
            // free resource
            allocfree();
            p = get_next_dentry(p, buf, &cwd_clus, &cwd_sec);
            #endif

            return -1;
        }
    assert(0);
}

/* write count bytes from buff to file in fd */
/* return count: success
          -1: fail
        */
size_t fat32_write(uint32 fd, uchar *buff, uint64_t count)
{
    // for (uint32 i = 0; i < 30000000; ++i)
    // {
    //     ;
    // }
    if (count == 0) return 0;
    if (fd == stdout){
        memcpy(stdout_buf, buff, count);
        stdout_buf[count] = 0;
        return printk_port(stdout_buf);
    }

}

uchar unicode2char(uint16_t unich)
{
    return (unich >= 65 && unich <= 90)? unich - 65 + 'A' :
        (unich >= 97 && unich <= 122)? unich - 97 + 'a' :
        (unich == 95)? '_' : 
        (unich == 46)? '.':
        0;  
}

unicode_t char2unicode(char ch)
{
    return (ch >= 'A' && ch <= 'Z')? 65 + ch - 'A' :
            (ch >= 'a' && ch <= 'z')? 97 + ch - 'a':
            (ch == '_')? 95 :
            (ch == '.')? 46 :
            0;
}

// void get_file_name_from_long_dentry()
// {

// }

/* make a directory */
/* success 0, fail -1 */
uint8_t fat32_mkdir(uint32_t dirfd, const uchar *path_const, uint32_t mode)
{
    uchar buf[BUFSIZE]; // for search
    uchar testbuf[BUFSIZE];
    uint32_t *aaa;

    uchar path[strlen(path_const)+1]; strcpy(path, path_const);

    uint8 isend = 0;

    uchar *temp1, *temp2; // for path parse
    uint32_t now_clus, new_clus; // now cluster num
    uint32_t parent_first_clus;

    dentry_t *p = (dentry_t *)buf; // for loop

    if (path[0] == '/'){
        now_clus = fat.bpb.root_clus;
        temp2 = &path[1], temp1 = &path[1];
    }
    else{
        if (dirfd == AT_FDCWD)
            now_clus = cwd_first_clus;
        else{
            if (current_running->fd[dirfd].used)
                now_clus = current_running->fd[dirfd].first_clus_num;
            else
                return -1;
        }
        temp2 = path; temp1 = path;
    }
    parent_first_clus = now_clus;
    printk_port("path: %s, cwd_first_clus: %d, now_clus: %d\n", path_const, cwd_first_clus, now_clus);


    while (1)
    {
        while (*temp2 != '/' && *temp2 != '\0') temp2++;
        if (*temp2 == '\0' || *(temp2+1) == '\0' ) isend = 1;
        *temp2 = '\0';

        // printk_port("isend: %d\n", isend);

        if (isend){
            // already exists
            if (search(temp1, now_clus, buf, SEARCH_DIR))
                return -1;
            // compute how many entry is needed
            // uint8 no_extname;
            // uchar *temp3 = temp1;
            // while(*temp3 != '.' && *temp3 != 0) temp3++;
            // if (*temp3 == 0) no_extname = 1;
            // else no_extname = 0;
            // *temp3 = 0; temp3++;

            /* now, temp1 point to filename, temp3 point to extname, with '\0' between */

            uint32_t length = strlen(temp1);
            // printk_port("length: %d\n", length);
            // if (!no_extname && length > 8) *(temp3 - 1) = '.';

            /* now, if length > 8 and there is a extname, they hace '. between them*/
            /* cannot strlen */
            /* never move temp1 and temp3 */

            uint32_t demand, sec;
            if (length <= 8)
                demand = 1;
            else
                demand = length / LONG_DENTRY_NAME_LEN + 2; // 1 for /, 1 for short entry
            printk_port("length: %d, demand: %d\n", length, demand);
            // find empty entry
            // printk_port("now_clus: %d\n",now_clus);

            p = search_empty_entry(now_clus, buf, demand, &sec);
            now_clus = clus_of_sec(sec);
            printk_port("sec111: %d, clus11: %d\n",sec,now_clus);

            // sd_read(testbuf, fat.bpb.rsvd_sec_cnt + fat.bpb.hidd_sec);
            // aaa = (uint32_t *)testbuf;
            // for (int i = 0; i < 10; ++i)
            // {
            //     printk_port("test11: %x\n", *aaa++);
            // }
            new_clus = search_empty_fat(buf);
            printk_port("new_clus: %d\n", new_clus);
            // sd_read(testbuf, fat.bpb.rsvd_sec_cnt + fat.bpb.hidd_sec + fat.bpb.fat_sz);
            // aaa = (uint32_t *)testbuf;
            // for (int i = 0; i < 10; ++i)
            // {
            //     printk_port("test21: %x\n", *aaa++);
            // }

            if (!p) assert(0);

            // 1. prepare short entry
            dentry_t new_dentry;
            memset(&new_dentry, 0, sizeof(new_dentry));

            // filename:
            if (demand == 1){
                // only short
                for (uint i = 0; i < length; ++i)
                    new_dentry.filename[i] = *(temp1+i);
                for (uint i = length; i < 8; ++i)
                    new_dentry.filename[i] = ' ';
            }
            else{
                for (uint i = 0; i < 6; ++i)
                    new_dentry.filename[i] = *(temp1+i);
                new_dentry.filename[6] = '~'; new_dentry.filename[7] = '1'; // dont think about 2 or more
            }
            // printk_port("name : %s\n",new_dentry.filename);
            // extname:
            for (uint i = 0; i < SHORT_DENTRY_EXTNAME_LEN; ++i)
                new_dentry.extname[i] = ' ';
            // attribute
            new_dentry.attribute = FILE_ATTRIBUTE_CHDIR;
            // reserved
            new_dentry.reserved = 0;
            // create time
            new_dentry.create_time_ms = 0;
            new_dentry.create_time = 0x53D4; //10:30:20
            new_dentry.create_date = 0x52BB; // 2021/5/27
            new_dentry.last_visited = 0x52BB;
            new_dentry.HI_clusnum = (new_clus >> 16) & ((1lu << 16) - 1);      //21:20
            new_dentry.last_modified_time = 0x53D3;   //23:22
            new_dentry.last_modified_date = 0x52BB;     //25:24
            new_dentry.LO_clusnum = new_clus & ((1lu << 16) - 1);      //27:26
            new_dentry.length = 0x0000;     //31:28

            // for (int i = 0; i < 32; ++i)
            // {
            //     printk_port("num %d is %d\n", i, *(((char*)new_dentry) + i));
            // }
            
            // 2. prepare long entry
            long_dentry_t long_entries[demand - 1];
            if (demand > 1){
                uint now_len;
                printk_port("length: %d\n", length);
                for (uint j = 0; j < demand - 1; ++j)
                {
                    // name
                    for (uint i = 0; i < LONG_DENTRY_NAME1_LEN; ++i)
                    {
                        printk_port("1\n");
                        now_len = j*LONG_DENTRY_NAME_LEN + i;
                        printk_port("nowlength: %d\n", now_len);
                        long_entries[j].name1[i] = (now_len < length)? char2unicode(*(temp1+now_len)) :
                                                    (now_len == length)? 0x00 :
                                                    0xff;
                        printk_port("is %x\n", long_entries[j].name1[i]);
                    }
                    for (uint i = 0; i < LONG_DENTRY_NAME2_LEN; ++i)
                    {
                        printk_port("2\n");
                        now_len = j*LONG_DENTRY_NAME_LEN + LONG_DENTRY_NAME1_LEN + i;
                        printk_port("nowlength: %d\n", now_len);
                        long_entries[j].name2[i] = (now_len < length)? char2unicode(*(temp1+now_len)) :
                                                    (now_len == length)? 0x00 :
                                                    0xff;
                        printk_port("is %x\n", long_entries[j].name2[i]);
                    }
                    for (uint i = 0; i < LONG_DENTRY_NAME3_LEN; ++i)
                    {
                        printk_port("3\n");
                        now_len = j*LONG_DENTRY_NAME_LEN + LONG_DENTRY_NAME1_LEN + LONG_DENTRY_NAME2_LEN + i;
                        printk_port("nowlength: %d\n", now_len);
                        long_entries[j].name3[i] = (now_len < length)? char2unicode(*(temp1+now_len)) :
                                                    (now_len == length)? 0x00 :
                                                    0xff;
                        printk_port("is %x\n", long_entries[j].name3[i]);
                    }
                    long_entries[j].sequence = (0x40 | (j + 1)) & 0x4f;   
                    long_entries[j].attribute = FILE_ATTRIBUTE_LONG;
                    long_entries[j].reserved = 0;
                    // checksum
                    uint8 sum = 0; uchar *fcb = &new_dentry;
                    for (int i = 0; i < 11; ++i)
                        sum = (sum & 0x1 ? 0x80 : 0) + (sum >> 1) + *fcb++;
                    long_entries[j].checksum = sum;
                    printk_port("sum: %d\n", sum);
                    long_entries[j].reserved = 0lu;
                }
            }
            printk_port("name1: %d %d %d %d %d\n", long_entries[0].name1[0], long_entries[0].name1[1], 
                long_entries[0].name1[2], long_entries[0].name1[3], long_entries[0].name1[4]);
            printk_port("name2: %d %d %d %d %d %d\n", long_entries[0].name2[0], long_entries[0].name2[1], 
                long_entries[0].name2[2], long_entries[0].name2[3], long_entries[0].name2[4], long_entries[0].name2[5]);
            printk_port("name3: %d %d\n", long_entries[0].name3[0], long_entries[0].name3[1]);
            printk_port("sequence: %x\n", long_entries[0].sequence);
            printk_port("attribute: %d\n", long_entries[0].attribute);
            // 3. write entry
            // printk_port("sec: %d, sec2: %d\n", sec, sec - (sec % (BUFSIZE/SECTOR_SIZE)));
            sd_read(buf, sec); // if no sec then p is meaningless
            for (uint i = 0; i < demand - 1; ++i)
            {
                memcpy(p, &long_entries[demand - 2 - i], sizeof(dentry_t));
                printk_port("p: %lx, buf: %lx\n", p + 1, buf + BUFSIZE);
                printk_port("attribute: %d\n", p->attribute);

                if (p + 1 == buf + BUFSIZE) // boundary
                {
                    printk_port("Oops\n");
                    sd_write(buf, sec);
                    if (clus_of_sec(sec + 1) != clus_of_sec(sec) && get_next_cluster(clus_of_sec(sec)) == 0x0fffffff){
                        uint32_t dir_new_clus = search_empty_fat(buf);
                        write_fat_table(now_clus, dir_new_clus, buf);
                    }
                }
                p = get_next_dentry(p, buf, &now_clus, &sec);
            }
            memcpy(p, &new_dentry, sizeof(new_dentry));
            // printk_port("p: %lx, buf: %lx, sec :%d, offset: %x\n", p, buf, sec, (void*)p - (void*)buf);

            // sd_read(testbuf, sec);
            // aaa = (uint32_t *)&testbuf[128];
            // for (int i = 0; i < 10; ++i)
            // {
            //     printk_port("test55: %x\n", *aaa++);
            // }

            sd_write(buf, sec);

            // sd_read(testbuf, sec);
            // aaa = (uint32_t *)&testbuf[128];
            // for (int i = 0; i < 10; ++i)
            // {
            //     printk_port("test66: %x\n", *aaa++);
            // }



            //4. write dir itself
            sec = first_sec_of_clus(new_clus);
            memset(buf, 0, BUFSIZE);
            p = buf;
            // printk_port("newsec: %d\n", sec);

            //"."
            p->filename[0] = '.'; for (uint i = 1; i < SHORT_DENTRY_FILENAME_LEN; i++) p->filename[i] = ' ';
            for (uint i = 0; i < SHORT_DENTRY_EXTNAME_LEN; i++) p->extname[i] = ' ';
            // attribute
            p->attribute = FILE_ATTRIBUTE_CHDIR;
            // reserved
            p->reserved = 0;
            // create time
            p->create_time_ms = 0;
            p->create_time = 0x53D4; //10:30:20
            p->create_date = 0x52BB; // 2021/5/27
            p->last_visited = 0x52BB;
            p->HI_clusnum = (new_clus >> 16) & ((1lu << 16) - 1);      //21:20
            p->last_modified_time = 0x53D3;   //23:22
            p->last_modified_date = 0x52BB;     //25:24
            p->LO_clusnum = new_clus & ((1lu << 16) - 1);      //27:26
            p->length = 0x0000;     //31:28

            p++;
            //".."
            p->filename[0] = '.'; p->filename[1] = '.'; for (uint i = 2; i < SHORT_DENTRY_FILENAME_LEN; i++) p->filename[i] = ' ';
            for (uint i = 0; i < SHORT_DENTRY_EXTNAME_LEN; i++) p->extname[i] = ' ';
            // attribute
            p->attribute = FILE_ATTRIBUTE_CHDIR;
            // reserved
            p->reserved = 0;
            // create time
            p->create_time_ms = 0;
            p->create_time = 0x53D4; //10:30:20
            p->create_date = 0x52BB; // 2021/5/27
            p->last_visited = 0x52BB;
            p->HI_clusnum = (parent_first_clus >> 16) & ((1lu << 16) - 1);      //21:20
            p->last_modified_time = 0x53D3;   //23:22
            p->last_modified_date = 0x52BB;     //25:24
            p->LO_clusnum = parent_first_clus & ((1lu << 16) - 1);      //27:26
            p->length = 0x0000;     //31:28

            sd_write(buf, sec);

            // 5. write cluster
            write_fat_table(0, new_clus, buf);

            return 0;
        }
        else{
            // search dir until fail or goto search file
            while (p = search(temp1, now_clus, buf, SEARCH_DIR)){
                now_clus = get_cluster_from_dentry(p);
                ++temp2;
                temp1 = temp2;
            }
            return -1;
        }
    }

}

/* write clus */
/* old pt new */
void write_fat_table(uint32_t old_clus, uint32_t new_clus, uchar *buff)
{
    uint32_t *clusat;
    if (old_clus){
        /* TABLE 1*/
        sd_read(buff, fat_sec_of_clus(old_clus));
        clusat = (uint32_t *)((char*)buff + fat_offset_of_clus(old_clus));
        *clusat = 0xffffffff & new_clus;
        sd_write(buff, fat_sec_of_clus(old_clus));
        /* TABLE 0*/
        sd_read(buff, fat_sec_of_clus(old_clus) - fat.bpb.fat_sz);
        clusat = (uint32_t *)((char*)buff + fat_offset_of_clus(old_clus));
        *clusat = 0xffffffff & new_clus;
        sd_write(buff, fat_sec_of_clus(old_clus) - fat.bpb.fat_sz);
    }
    /* TABLE 1*/
    sd_read(buff, fat_sec_of_clus(new_clus));
    clusat = (uint32_t *)((char*)buff + fat_offset_of_clus(new_clus));
    *clusat = 0x0fffffff;
    sd_write(buff, fat_sec_of_clus(new_clus));
    /* TABLE 0*/
    sd_read(buff, fat_sec_of_clus(new_clus) - fat.bpb.fat_sz);
    clusat = (uint32_t *)((char*)buff + fat_offset_of_clus(new_clus));
    *clusat = 0x0fffffff;
    sd_write(buff, fat_sec_of_clus(new_clus) - fat.bpb.fat_sz);
}

/* return empty clus num */
uint32_t search_empty_fat(uchar *buf)
{
    uint32_t now_sec = fat.first_data_sec - fat.bpb.fat_sz;
    uint32_t now_clus = clus_of_sec(now_sec);
    sd_read(buf, now_sec);
    uchar *temp = buf;
    uint j = 0;
    while (1){
        uint i;
        for (i = 0; i < 4; ++i)
        {
            if (*(temp + i + j))
                break;
        }
        if (i == 4) break;
        else j += 4;
    }
    printk_port("j = %d\n", j);
    write_fat_table(0, j/4, buf);
    return j/4;
}

/* make a directory */
/* success 0, fail -1 */
// uint8_t fat32_mkdir22(uint32_t dirfd, const uchar *path_const, uint32_t mode)
// {
//     uchar path[strlen(path_const)+1]; strcpy(path, path_const);

//     uint8 isend = 0;

//     uchar *temp1, *temp2; // for path parse
//     uint32_t now_clus; // now cluster num
//     uchar buf[BUFSIZE]; // for search

//     dentry_t *p = (dentry_t *)buf; // for loop

//     if (path[0] == '/'){
//         now_clus = fat.bpb.root_clus;
//         temp2 = &path[1], temp1 = &path[1];
//     }
//     else{
//         if (dirfd == AT_FDCWD)
//             now_clus = cwd_first_clus;
//         else{
//             if (current_running->fd[dirfd].used)
//                 now_clus = current_running->fd[dirfd].first_clus_num;
//             else
//                 return -1;
//         }
//         temp2 = path; temp1 = path;
//     }
//     printk_port("path: %s, cwd_first_clus: %d, now_clus: %d\n", path_const, cwd_first_clus, now_clus);


//     while (1)
//     {
//         while (*temp2 != '/' && *temp2 != '\0') temp2++;
//         if (*temp2 == '\0' || *(temp2+1) == '\0' ) isend = 1;
//         *temp2 = '\0';

//         printk_port("isend: %d\n", isend);
        
//         if (isend){
//             // already exists
//             if (search(temp1, now_clus, buf, SEARCH_DIR))
//                 return -1;
//             // compute how many entry is needed
//             uint8 no_extname;
//             uchar *temp3 = temp1;
//             while(*temp3 != '.' && *temp3 != 0) temp3++;
//             if (*temp3 == 0) no_extname = 1;
//             else no_extname = 0;
//             *temp3 = 0; temp3++;

//             /* now, temp1 point to filename, temp3 point to extname, with '\0' between */

//             uint32_t length = strlen(temp1);
//             if (!no_extname && length > 8) *(temp3 - 1) = '.';

//             /* now, if length > 8 and there is a extname, they hace '. between them*/
//             /* cannot strlen */
//             /* never move temp1 and temp3 */

//             uint32_t demand, sec;
//             if (length <= 8)
//                 demand = 1;
//             else
//                 demand = length / LONG_DENTRY_NAME_LEN + 2; // 1 for /, 1 for short entry
//             // find empty entry
//             p = search_empty_entry(now_clus, buf, demand, &sec);
//             now_clus = clus_of_sec(sec);
//             new_clus = search_empty_fat(buf);
//             if (!p) assert(0);

//             // 1. prepare short entry
//             dentry_t new_dentry;
//             memset(&new_dentry, 0, sizeof(new_dentry));

//             // filename:
//             if (demand = 1){
//                 // only short
//                 for (uint i = 0; i < length; ++i)
//                     new_dentry.filename[i] = (*(temp1+i) <= 'z' && *(temp1+i) >= 'a') ? *(temp1+i) - 'a' + 'A' : *(temp1+i);
//                 for (uint i = length; i < 8; ++i)
//                     new_dentry.filename[i] = ' ';
//             }
//             else{
//                 for (uint i = 0; i < 6; ++i)
//                     new_dentry.filename[i] = (*(temp1+i) <= 'z' && *(temp1+i) >= 'a') ? *(temp1+i) - 'a' + 'A' : *(temp1+i);
//                 new_dentry.filename[6] = '~'; new_dentry.filename[7] = '1'; // dont think about 2 or more
//             }
//             // extname:
//             for (uint i = 0; i < 3; ++i){
//                 new_dentry.extname[i] = (*(temp3+i) <= 'z' && *(temp3+i) >= 'a') ? *(temp3+i) - 'a' + 'A' : 
//                                             (*(temp3+i)) ? *(temp3+i) :
//                                             ' ';
//             }
//             // attribute
//             new_dentry.attribute = FILE_ATTRIBUTE_CHDIR;
//             // reserved
//             new_dentry.reserved = 0;
//             // create time
//             new_dentry.create_time_ms = 0;
//             new_dentry.create_time = 0x53D4; //10:30:20
//             new_dentry.create_date = 0x52BB; // 2021/5/27
//             new_dentry.last_visited = 0x52BB;
//             new_dentry.HI_clusnum = (new_clus >> 16) & ((1lu << 16) - 1);      //21:20
//             new_dentry.last_modified_time = 0x53D3;   //23:22
//             new_dentry.last_modified_date = 0x52BB;     //25:24
//             new_dentry.LO_clusnum = new_clus & ((1lu << 16) - 1);      //27:26
//             new_dentry.length = 0x0000;     //31:28
            
//             sd_write(buf, sec - (sec % 4));

//             // 2. prepare long entry
//             long_dentry_t long_entries[demand - 1];
//             uint setf = 0;
//             uint now_len;
//             for (uint j = 0; j < demand - 1; ++j)
//             {
//                 // name
//                 for (uint i = 0; i < LONG_DENTRY_NAME1_LEN; ++i)
//                 {
//                     now_len = j*LONG_DENTRY_NAME_LEN + i + 1;
//                     long_entries[j].name1[i] = (now_len <= length)? char2unicode(*(temp+now_len)) :
//                                                 (now_len - length == 1)? 0x00 :
//                                                 0xff;
//                 }
//                 for (uint i = 0; i < LONG_DENTRY_NAME2_LEN; ++i)
//                 {
//                     now_len = j*LONG_DENTRY_NAME_LEN + LONG_DENTRY_NAME1_LEN + i + 1;
//                     long_entries[j].name2[i] = (now_len <= length)? char2unicode(*(temp+now_len)) :
//                                                 (now_len - length == 1)? 0x00 :
//                                                 0xff;
//                 }
//                 for (uint i = 0; i < LONG_DENTRY_NAME3_LEN; ++i)
//                 {
//                     now_len = j*LONG_DENTRY_NAME_LEN + LONG_DENTRY_NAME1_LEN + LONG_DENTRY_NAME2_LEN + i + 1;
//                     long_entries[j].name3[i] = (now_len <= length)? char2unicode(*(temp+now_len)) :
//                                                 (now_len - length == 1)? 0x00 :
//                                                 0xff;
//                 }
//                 long_entries[j].sequence = (0x40 | (j + 1)) & 0x40;   
//                 long_entries[j].attribute = FILE_ATTRIBUTE_LONG;
//                 long_entries[j].reserved = 0;
//                 // checksum
//                 uint8 sum = 0; uchar *fcb = &long_entries[j];
//                 for (int i = 0; i < count; ++i)
//                     sum = (sum & 0x1 ? 0x80 : 0) + (sum >> 1) + *fcb++;
//                 long_entries[j].checksum = sum;
//                 long_entries[j].reserved = 0lu;
//             }
//             // 3. write entry
//             sd_read(buf, sec - (sec % (BUFSIZE/SECTOR_SIZE))); // if no % then p is meaningless
//             for (uint i = 0; i < demand - 1; ++i)
//             {
//                 memcpy(p, &long_entries[demand - 2 - i], sizeof(p));
//                 if (p + 1 == buf + BUFSIZE) // boundary
//                 {
//                     sd_write(buf, sec - (sec % (BUFSIZE/SECTOR_SIZE)));
//                     if (((sec + 1) % fat.bpb.sec_per_clus == 0) && get_next_cluster(clus_of_sec(sec)) == 0x0ffffff8){
//                         assert(0);
//                     }
//                 }
//                 p = get_next_dentry(p, buf, &now_clus, &sec);
//             }
//             memcpy(p, &new_dentry, sizeof(new_dentry));
//             sd_write(buf, sec - (sec % (BUFSIZE/SECTOR_SIZE) ));
//         }
//         else{
//             // search dir until fail or goto search file
//             while (p = search(temp1, now_clus, buf, SEARCH_DIR)){
//                 now_clus = get_cluster_from_dentry(p);
//                 ++temp2;
//                 temp1 = temp2;
//             }
//             return -1;
//         }
//     }

// }


/* search for demand number of CONTINUOUS empty entry for mkdir */
/* make sure demand != 0 */
/* suggest we can find a result, or endless loop */
/* return top and modify sec as top*/
dentry_t *search_empty_entry(uint32_t dir_first_clus, uchar *buf, uint32_t demand, uint32_t *sec)
{
    uint32_t now_clus = dir_first_clus;
    *sec = first_sec_of_clus(dir_first_clus);
    printk_port("sec: %d\n", *sec);
    sd_read(buf, *sec);
    dentry_t *p = (dentry_t *)buf;
    uint32_t cnt = 0;
    dentry_t *ret_p = p;
    uint32_t ret_sec = *sec;

    for (uint i = 0; i < 255; i++){
        if (!is_zero_dentry(p) && 0xE5 != p->filename[0]){
            cnt = 0;
            p = get_next_dentry(p, buf, &now_clus, *sec);
            printk_port("sec4: %d\n", *sec);
            ret_p = p; ret_sec = *sec;
        }
        else{
            cnt++;
            if (cnt == demand) {*sec = ret_sec; return ret_p;}
            else{
                p = get_next_dentry(p, buf, &now_clus, *sec);
                printk_port("sec3: %d\n", *sec);
            }
        }
    }
    return NULL;
}



/* search if name.file exists in dir now_clus */
/* make sure buf size is BUFSIZE */
/* return dentry point if success, NULL if fail */
dentry_t *search(const uchar *name, uint32_t dir_first_clus, uchar *buf, search_mode_t mode)
{
    // printk_port("p addr: %lx, buf: %lx\n", *pp, buf);
    uint32_t now_clus = dir_first_clus, now_sec = first_sec_of_clus(dir_first_clus);
    // printk_port("buf: %lx, p: %lx, sec: %d\n", buf, *pp, now_sec);
    sd_read(buf, now_sec);
    // printk_port("5\n");
    dentry_t *p = (dentry_t *)buf;
    // dont care deleted dir
    while (!is_zero_dentry(p)){

        while (0xE5 == p->filename[0]){
            p = get_next_dentry(p, buf, &now_clus, &now_sec);
        }

        uchar *filename;
        long_dentry_t *q = (long_dentry_t *)p;
        // if long dentry
        if (q->attribute == 0x0f && (q->sequence & 0x40) == 0x40){
            uint8 item_num = q->sequence & 0x0f; // entry num
            filename = kmalloc((LONG_DENTRY_NAME1_LEN + LONG_DENTRY_NAME2_LEN + LONG_DENTRY_NAME3_LEN)*item_num); // filename

            /* get filename */
            uint8 isbreak = 0;
            uint16_t unich; // unicode

            while (item_num--){
                uint8 name_cnt = 0;
                for (uint8 i = 0; i < LONG_DENTRY_NAME1_LEN; ++i){
                    // name1
                    unich = q->name1[i];
                    if (unich == 0x0000 || unich == 0xffff){
                        filename[item_num*LONG_DENTRY_NAME_LEN + name_cnt] = 0;
                        break;
                    }
                    else filename[item_num*LONG_DENTRY_NAME_LEN + name_cnt] = unicode2char(unich);   
                    name_cnt++;           
                }
                for (uint8 i = 0; i < LONG_DENTRY_NAME2_LEN; ++i){
                    // name1
                    unich = q->name2[i];
                    if (unich == 0x0000 || unich == 0xffff){
                        filename[item_num*LONG_DENTRY_NAME_LEN + name_cnt] = 0;
                        break;
                    }
                    else filename[item_num*LONG_DENTRY_NAME_LEN + name_cnt] = unicode2char(unich);   
                    name_cnt++;            
                }
                for (uint8 i = 0; i < LONG_DENTRY_NAME3_LEN; ++i){
                    // name1
                    unich = q->name3[i];
                    if (unich == 0x0000 || unich == 0xffff){
                        filename[item_num*LONG_DENTRY_NAME_LEN + name_cnt] = 0;
                        break;
                    }
                    else filename[item_num*LONG_DENTRY_NAME_LEN + name_cnt] = unicode2char(unich);  
                    name_cnt++;             
                }
                p = get_next_dentry(p, buf, &now_clus, &now_sec);
                q = (long_dentry_t *)p;
            }
        }
        // short dentry
        else{
            /* filename */
            uint8 name_cnt = 0;
            filename = kmalloc(SHORT_DENTRY_FILENAME_LEN + SHORT_DENTRY_EXTNAME_LEN + 2); //'.' and addition 0 in the end
            for (uint8 i = 0; i < SHORT_DENTRY_FILENAME_LEN; ++i)
            {
                if (p->filename[i] == ' ') break;
                else filename[name_cnt++] = p->filename[i];
            }
            if (p->extname[0] != ' ') filename[name_cnt++] = '.';
            for (uint8 i = 0; i < SHORT_DENTRY_EXTNAME_LEN; ++i)
            {
                if (p->extname[i] == ' ') break;
                else filename[name_cnt++] = p->filename[i];
            }
            filename[name_cnt++] = 0;
        }
        // printk_port("filename: %s\n", filename);
        // printk_port("attri: %x %x\n", p->attribute, p->attribute & 0x10);
        // printk_port("1:%s, 2:%s, %d, %d\n", name, filename, strcmp(name, filename), filenamecmp(name, filename));

        if ((p->attribute & 0x10) != 0 && mode == SEARCH_DIR && !filenamecmp(filename, name))
            return p;
        else if ((p->attribute & 0x10) == 0 && mode == SEARCH_FILE && !filenamecmp(filename, name))
            return p;
        else
            p = get_next_dentry(p, buf, &now_clus, &now_sec);
    }
    
    return NULL;
}

/* write count bytes from buff to file in fd */
/* return count: success
          -1: fail
        */
uint16 fat32_open(uint32 fd, const uchar *path_const, uint32 flags, uint32 mode)
{
    uchar path[strlen(path_const)+1];strcpy(path, path_const);

    uint8 isend = 0;

    uchar *temp1, *temp2; // for path parse
    uint32_t now_clus; // now cluster num
    uchar buf[BUFSIZE]; // for search

    dentry_t *p = (dentry_t *)buf; // for loop

    if (path[0] == '/'){
        now_clus = fat.bpb.root_clus;
        temp2 = &path[1], temp1 = &path[1];
    }
    else{
        now_clus = cwd_first_clus;
        temp2 = path, temp1 = path;
    }

    while (1)
    {
        while (*temp2 != '/' && *temp2 != '\0') temp2++;
        if (*temp2 == '\0' || *(temp2+1) == '\0' ) isend = 1;
        *temp2 = '\0';

        if (isend){
            // success
            if (p = search(temp1, now_clus, buf, SEARCH_FILE)){
                for (uint8 i = 0; i < NUM_FD; ++i)
                {
                    if (!current_running->fd[i].used){
                        current_running->fd[i].dev = 0;
                        current_running->fd[i].first_clus_num = get_cluster_from_dentry(p);
                        // printk_port("clus: %d\n", get_cluster_from_dentry(p));
                        current_running->fd[i].flags = flags;
                        current_running->fd[i].pos = 0;
                        current_running->fd[i].used = 1;
                        return i;
                    }
                }
                return -1;
            }
            //fail
            else return -1;
        }
        else{
            // search dir until fail or goto search file
            while (p = search(temp1, now_clus, buf, SEARCH_DIR)){
                now_clus = get_cluster_from_dentry(p);
                ++temp2;
                temp1 = temp2;
            }
            return -1;
        }
    }
}

/* close fd */
/* success return 0, fail return 1*/
uint8 fat32_close(uint32 fd)
{
    if (current_running->fd[fd].used)
    {
        current_running->fd[fd].used = FD_UNUSED;
        return 0;
    }
    else
        return 1;
}

uint8 is_zero_dentry(dentry_t *p)
{
    uchar *n = (uchar *)p;
    uint8_t index;
    for (index = 0; index < 32; ++index)
    {
        if (*n++ != 0)
            break;
    }
    printk_port("index: %d\n",index);
    if (index == 32)
        return 1;
    else
        return 0;
}

/* filename compare */
/* same return 0, other return 0*/
/* capital-ignore */
/* safe cmp */
uint8_t filenamecmp(const char *name1, const char *name2)
{
    uchar name1_t[strlen(name1)+1], name2_t[strlen(name2)+1];
    strcpy(name1_t, name1); strcpy(name2_t, name2);
    for (int i = 0; i < strlen(name1); ++i)
        if (name1_t[i] >= 'A' && name1_t[i] <= 'Z')
            name1_t[i] = 'a' + name1_t - 'A';
    for (int i = 0; i < strlen(name1); ++i)
        if (name2_t[i] >= 'A' && name2_t[i] <= 'Z')
            name2_t[i] = 'a' + name2_t - 'A';
    return strcmp(name1_t, name2_t);
}



/* go to child directory */
