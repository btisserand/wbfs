#if defined( __linux__) || defined(__APPLE__)
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __linux__
#include <linux/fs.h>
#else
#include <sys/disk.h>
#endif
#include <fcntl.h>
#include <unistd.h>

#include "libwbfs.h"

static int wbfs_fread_sector(void *_fp,u32 lba,u32 count,void*buf)
{
	FILE*fp =_fp;
	u64 off = lba;
	off*=512ULL;
	if (fseeko(fp, off, SEEK_SET))
	{
		fprintf(stderr,"\n\n%lld %d %p\n",off,count,_fp);
		wbfs_error("error seeking in disc partition");
		return 1;
	}
	if (fread(buf, count*512ULL, 1, fp) != 1){
		wbfs_error("error reading disc");
		return 1;
	}
	return 0;
  
}
static int wbfs_fwrite_sector(void *_fp,u32 lba,u32 count,void*buf)
{
	FILE*fp =_fp;
	u64 off = lba;
	off*=512ULL;
	if (fseeko(fp, off, SEEK_SET))
	{
		wbfs_error("error seeking in disc file");
		return 1;
	}
	if (fwrite(buf, count*512ULL, 1, fp) != 1){
		wbfs_error("error writing disc");
		return 1;
	}
	return 0;
  
}
static int get_capacity(char *file,u32 *sector_size,u32 *n_sector)
{
	int fd = open(file,O_RDONLY);
	int ret;
	if(fd<0){
		return 0;
	}
#ifdef __linux__
	ret = ioctl(fd,BLKSSZGET,sector_size);
#else //__APPLE__
	ret = ioctl(fd,DKIOCGETBLOCKSIZE,sector_size);
#endif
	if(ret<0)
	{
		FILE *f;
		close(fd);
		f = fopen(file,"r");
		fseeko(f,0,SEEK_END);
		*n_sector = ftello(f)/512;
		*sector_size = 512;
		fclose(f);
		return 1;
	}
#ifdef __linux__
	ret = ioctl(fd,BLKGETSIZE,n_sector);
#else //__APPLE__
	long long my_n_sector;
	ret = ioctl(fd,DKIOCGETBLOCKCOUNT,&my_n_sector);
	*n_sector = (long)my_n_sector;
#endif
	close(fd);
	if(*sector_size>512)
		*n_sector*=*sector_size/512;
	if(*sector_size<512)
		*n_sector/=512/ *sector_size;
	return 1;
}
wbfs_t *wbfs_try_open_hd(char *fn,int reset)
{
	u32 sector_size, n_sector;
	if(!get_capacity(fn,&sector_size,&n_sector))
		return NULL;
	FILE *f = fopen(fn,"r+");
	if (!f)
		return NULL;
	return wbfs_open_hd(wbfs_fread_sector,wbfs_fwrite_sector,f,
			    sector_size ,n_sector,reset);
}
wbfs_t *wbfs_try_open_partition(char *fn,int reset)
{
	u32 sector_size, n_sector;
	if(!get_capacity(fn,&sector_size,&n_sector))
		return NULL;
	FILE *f = fopen(fn,"r+");
	if (!f)
		return NULL;
	return wbfs_open_partition(wbfs_fread_sector,wbfs_fwrite_sector,f,
				   sector_size ,n_sector,0,reset);
}
wbfs_t *wbfs_try_open(char *disc,char *partition, int reset)
{
	wbfs_t *p = 0;
	if(partition)
		p = wbfs_try_open_partition(partition,reset);
	if (!p && !reset && disc)
		p = wbfs_try_open_hd(disc,0);
	else if(!p && !reset){
		char buffer[32];
		int i;
#ifdef __linux__
		for (i='b';i<'z';i++)
		{
			snprintf(buffer,32,"/dev/sd%c",i);
			p = wbfs_try_open_hd(buffer,0);
			if (p)
			{
				fprintf(stderr,"using %s\n",buffer);
				return p;
			}
			snprintf(buffer,32,"/dev/hd%c",i);
			p = wbfs_try_open_hd(buffer,0);
			if (p)
			{
				fprintf(stderr,"using %s\n",buffer);
				return p;
			}
		}			 
#else
		int j;
		for (i=0;i<10;i++)
			for (j=0;j<10;j++){
				snprintf(buffer,32,"/dev/disk%ds%d",i,j);
				p = wbfs_try_open_partition(buffer,0);
				if (p)
				{
					fprintf(stderr,"using %s\n",buffer);
					return p;
				}	    
			}
#endif
		wbfs_error("cannot find any wbfs partition (verify permissions))");
	}
	return p;
	
}

#endif //__linux__ or __APPLE__
