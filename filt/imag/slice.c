/* Read slices from files */
/*
  Copyright (C) 2004 University of Texas at Austin
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#define _LARGEFILE_SOURCE
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>

#include <rsf.h>
/*^*/

#include "slice.h"

#ifndef _slice_h

typedef struct Slice *slice;
/* abstract data type */
/*^*/

typedef struct FSlice *fslice;
/* abstract data type */
/*^*/

#endif

struct Slice {
    off_t start;
    sf_file file;
    int n12, n3;
};

struct FSlice {
    FILE* file;
    char* name;
    off_t n12;
    int n3;
};


slice slice_init(sf_file file, int n1, int n2, int n3)
/*< initialize a sliceable file object >*/
{
    slice sl;

    sl = (slice) sf_alloc(1,sizeof(*sl));
    sl->file = file;
    sl->n12 = n1*n2;
    sl->n3 = n3;

    sf_unpipe(sl->file,n1*n2*n3*sizeof(float));
    sl->start = sf_tell(sl->file);
    
    return sl;
}

void slice_get(slice sl, int i3, float* data)
/*< get a slice at level i3 >*/
{
    sf_seek(sl->file,sl->start+i3*(sl->n12)*sizeof(float),SEEK_SET);
    sf_floatread(data,sl->n12,sl->file);
}

void slice_put(slice sl, int i3, float* data)
/*< put a slice at level i3 >*/
{
    sf_seek(sl->file,sl->start+i3*(sl->n12)*sizeof(float),SEEK_SET);
    sf_floatwrite(data,sl->n12,sl->file);
}

void cslice_get(slice sl, int i3, complex float* data)
/*< get a slice at level i3 >*/
{
    sf_seek(sl->file,sl->start+i3*(sl->n12)*sizeof(complex float),SEEK_SET);
    sf_complexread(data,sl->n12,sl->file);
}

void cslice_put(slice sl, int i3, complex float* data)
/*< put a slice at level i3 >*/
{
    sf_seek(sl->file,sl->start+i3*(sl->n12)*sizeof(complex float),SEEK_SET);
    sf_complexwrite(data,sl->n12,sl->file);
}

/*------------------------------------------------------------*/

fslice fslice_init(int n1, int n2, int n3, size_t size)
/*< initialize a sliceable file object >*/
{
    fslice sl;

    sl = (fslice) sf_alloc(1,sizeof(*sl));
    sl->file = sf_tempfile(&(sl->name), "w+b");
    sl->n12 = n1*n2*size;
    sl->n3 = n3;
    
    return sl;
}

void fslice_get(fslice sl, int i3, void* data)
/*< get a slice at level i3 >*/
{
    fseeko(sl->file,i3*(sl->n12),SEEK_SET);
    fread(data,sl->n12,1,sl->file);
}

void fslice_put(fslice sl, int i3, void* data)
/*< put a slice at level i3 >*/
{
    fseeko(sl->file,i3*(sl->n12),SEEK_SET);
    fwrite(data,sl->n12,1,sl->file);
}

void fslice_close(fslice sl)
/*< remove the file and free allocated storage >*/
{
    unlink(sl->name);
    free(sl);
}


/*------------------------------------------------------------*/
