/* clinit.c
 *
 * Copyright (c) 2009 Brown Deer Technology, LLC.  All Rights Reserved.
 *
 * This software was developed by Brown Deer Technology, LLC.
 * For more information contact info@browndeertechnology.com
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3 (LGPLv3)
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* DAR */

/* XXX to do, add err code checks, other safety checks * -DAR */
/* XXX to do, clvplat_destroy should automatically release all txts -DAR */

#ifdef _WIN64
#include "fix_windows.h"
#else
#include <unistd.h>
#include <sys/mman.h>
#include <elf.h>
#endif

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <CL/cl.h>

#include "util.h"

#define __STDCL__
#include "clinit.h"
#include "clcontext.h"


#ifdef DEFAULT_OPENCL_PLATFORM
#define DEFAULT_PLATFORM_NAME DEFAULT_OPENCL_PLATFORM
#else
#define DEFAULT_PLATFORM_NAME "ATI"
#endif


/* 
 * global CONTEXT structs 
 */

LIBSTDCL_API CONTEXT* stddev = 0;
LIBSTDCL_API CONTEXT* stdcpu = 0;
LIBSTDCL_API CONTEXT* stdgpu = 0;
LIBSTDCL_API CONTEXT* stdrpu = 0;

int procelf_fd = -1;
void* procelf = 0;
size_t procelf_sz = 0;

//struct _proc_cl_struct _proc_cl = { 0,0, 0,0, 0,0, 0,0, 0,0 };
struct clelf_sect_struct* _proc_clelf_sect = 0;

char* __log_automatic_kernels_filename = 0;

#define min(a,b) ((a<b)?a:b)

//static int __getenv_token( 
//	const char* name, const char* token, char* value, size_t n
//);

//static cl_platform_id _select_platformid( 
//	cl_uint nplatforms, cl_platform_id* platforms, const char* env_var
//);


/* 
 * libstdcl initialization ctor and dtor 
 */

#ifdef _WIN64
LIBSTDCL_API void _libstdcl_init()
//void _libstdcl_init()
#else
void __attribute__((__constructor__)) _libstdcl_init()
#endif
{

	int i;
	int n;

	cl_platform_id platformid;

	int enable;
	cl_uint ndev;
	char env_max_ndev[256];
	int lock_key;


	DEBUG(__FILE__,__LINE__,"_libstdcl_init() called");

	/*
	 * set _proc_cl struct
 	 */

#ifndef _WIN64

	pid_t pid = getpid();
	DEBUG(__FILE__,__LINE__,"_libstdcl_init: pid=%d\n",pid);

	char procexe[256];
	snprintf(procexe,256,"/proc/%d/exe",pid);

	struct stat st;
	if (stat(procexe,&st)) ERROR(__FILE__,__LINE__,"stat procexe failed");

	procelf_fd = open(procexe,O_RDONLY);

	if (procelf_fd < 0) { 

		ERROR(__FILE__,__LINE__,"opening procexe failed");

	} else {

		procelf = mmap(0,st.st_size,PROT_READ,MAP_PRIVATE,procelf_fd,0);
		procelf_sz = st.st_size;

		DEBUG(__FILE__,__LINE__,"_libstdcl_init: procelf size %d bytes\n",
			st.st_size);

		_proc_clelf_sect 
			= (struct clelf_sect_struct*)malloc(sizeof(struct clelf_sect_struct));

		clelf_load_sections(procelf,_proc_clelf_sect);

	}

	struct clelf_sect_struct* sect = _proc_clelf_sect;

	if (sect) {

		DEBUG(__FILE__,__LINE__,"_libstdcl_init: proc clelf sections:"
			" %p %p %p %p %p %p %p\n",
			sect->clprgtab,
			sect->clkrntab,
			sect->clprgsrc,
			sect->cltextsrc,
			sect->clprgbin,
			sect->cltextbin,
			sect->clstrtab
		);

	}

#endif



	/*
	 * initialize stddev (all CL devices)
	 */

	DEBUG(__FILE__,__LINE__,"clinit: initialize stddev");


	stddev = 0;
	ndev = 0; /* this is a special case that implies all available -DAR */
	enable = 1;
	lock_key = 0;

	if (getenv("STDDEV")) enable = atoi(getenv("STDDEV"));

	if (enable) {

		char name[256];
		if (getenv("STDDEV_PLATFORM_NAME"))
			strncpy(name,getenv("STDDEV_PLATFORM_NAME"),256);
#ifdef DEFAULT_OPENCL_PLATFORM
//		else name[0]='\0';
		else strncpy(name,DEFAULT_OPENCL_PLATFORM,256);
#else
		else name[0]='\0';
#endif

		if (getenv("STDDEV_MAX_NDEV"))
			ndev = atoi(getenv("STDDEV_MAX_NDEV"));

		if (getenv("STDDEV_LOCK"))
			lock_key = atoi(getenv("STDDEV_LOCK"));

		stddev = clcontext_create(name,CL_DEVICE_TYPE_ALL,ndev,0,lock_key);

	}

	DEBUG(__FILE__,__LINE__,"back from clcontext_create\n");



	/*
	 * initialize stdcpu (all CPU CL devices)
	 */

	DEBUG(__FILE__,__LINE__,"clinit: initialize stdcpu");


	stdcpu = 0;
	ndev = 0; /* this is a special case that implies all available -DAR */
	enable = 1;
	lock_key = 0;

	if (getenv("STDCPU")) enable = atoi(getenv("STDCPU"));

	if (enable) {

		char name[256];
		if (getenv("STDCPU_PLATFORM_NAME"))
			strncpy(name,getenv("STDCPU_PLATFORM_NAME"),256);
#ifdef DEFAULT_OPENCL_PLATFORM
//		else name[0]='\0';
		else strncpy(name,DEFAULT_OPENCL_PLATFORM,256);
#else
		else name[0]='\0';
#endif

		if (getenv("STDCPU_MAX_NDEV"))
			ndev = atoi(getenv("STDCPU_MAX_NDEV"));

		if (getenv("STDCPU_LOCK"))
			lock_key = atoi(getenv("STDCPU_LOCK"));

		stdcpu = clcontext_create(name,CL_DEVICE_TYPE_CPU,ndev,0,lock_key);

	}

	DEBUG(__FILE__,__LINE__,"back from clcontext_create\n");




	/*
	 * initialize stdgpu (all GPU CL devices)
	 */

	DEBUG(__FILE__,__LINE__,"clinit: initialize stdgpu");

	stdgpu = 0;
	ndev = 0; /* this is a special case that implies all available -DAR */
	enable = 1;
	lock_key = 0;

	if (getenv("STDGPU")) enable = atoi(getenv("STDGPU"));

	if (enable) {

		char name[256];
		if (getenv("STDGPU_PLATFORM_NAME"))
			strncpy(name,getenv("STDGPU_PLATFORM_NAME"),256);
#ifdef DEFAULT_OPENCL_PLATFORM
//		else name[0]='\0';
		else strncpy(name,DEFAULT_OPENCL_PLATFORM,256);
#else
		else name[0]='\0';
#endif

		if (getenv("STDGPU_MAX_NDEV"))
			ndev = atoi(getenv("STDGPU_MAX_NDEV"));

		if (getenv("STDGPU_LOCK"))
			lock_key = atoi(getenv("STDGPU_LOCK"));

		stdgpu = clcontext_create(name,CL_DEVICE_TYPE_GPU,ndev,0,lock_key);

	}

	DEBUG(__FILE__,__LINE__,"back from clcontext_create\n");



	/*
	 * initialize stdrpu (all RPU CL devices)
	 */

	DEBUG(__FILE__,__LINE__,"clinit: initialize stdrpu");

	stdrpu = 0;
	ndev = 0; /* this is a special case that implies all available -DAR */
	enable = 1;
	lock_key = 0;

	if (getenv("STDRPU")) enable = atoi(getenv("STDRPU"));

	if (enable) {

		char name[256];
		if (getenv("STDRPU_PLATFORM_NAME"))
			strncpy(name,getenv("STDRPU_PLATFORM_NAME"),256);
		else name[0]='\0';

		if (getenv("STDRPU_MAX_NDEV"))
			ndev = atoi(getenv("STDRPU_MAX_NDEV"));

		if (getenv("STDRPU_LOCK"))
			lock_key = atoi(getenv("STDRPU_LOCK"));

		stdrpu = clcontext_create(name,CL_DEVICE_TYPE_RPU,ndev,0,lock_key);

	}

	DEBUG(__FILE__,__LINE__,"back from clcontext_create\n");



	/*
	 * initialize autokern logging
	 */

	if (getenv("COPRTHR_LOG_AUTOKERN")) {
		__log_automatic_kernels_filename = (char*)malloc(256+6);
			snprintf(
				__log_automatic_kernels_filename,256+6,
				"coprthr.autokern.log.%d",getpid());
		DEBUG(__FILE__,__LINE__,"log_automatic_kernels written to %s",
			__log_automatic_kernels_filename);
	}

//	clUnloadCompiler();	

}


#ifdef _WIN64
void _libstdcl_fini()
#else
void __attribute__((__destructor__)) _libstdcl_fini()
#endif
{
	DEBUG(__FILE__,__LINE__,"_libstdcl_fini() called");

	if (stdgpu) clcontext_destroy(stdgpu);

#ifndef _WIN64
	munmap(procelf,procelf_sz);
	close(procelf_fd);
#endif

/* Dangerous, order of destructors not well-controled, just let them die -DAR 
	if (stddev) clcontext_destroy(stddev);
	if (stdcpu) clcontext_destroy(stdcpu);
	if (stdgpu) clcontext_destroy(stdgpu);
//	if (stdrpu) clcontext_destroy(stdrpu);
*/

}

