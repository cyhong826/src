/* 1-D FFT wave extrapolation */
/*
  Copyright (C) 2008 University of Texas at Austin
  
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
#include <rsf.h>

#include "fft1.h"

int main(int argc, char* argv[]) 
{
    int nx, nx2, nk, nt, m1, m2, ix, ik, it, im, n2;
    float dt, f, *curr, *prev, **lft, **rht, **mid, **wave;
    sf_complex **mat, *cwave, *cwavem;
    sf_file inp, out, prop, left, right;

    sf_init(argc,argv);
    inp = sf_input("in");
    out = sf_output("out");

    if (SF_FLOAT != sf_gettype(inp)) sf_error("Need float input");
    
    if (!sf_histint(inp,"n1",&nx)) sf_error("No n1= in input");

    if (!sf_getint("nt",&nt)) sf_error("No nt= in input");
    if (!sf_getfloat("dt",&dt)) sf_error("No dt= in input");

    sf_putint(out,"n2",nt);
    sf_putfloat(out,"d2",dt);
    sf_putfloat(out,"o2",0.);
    sf_putstring(out,"label2","Time");
    sf_putstring(out,"unit2","s");

    nk = fft1_init(nx,&nx2);

    curr = sf_floatalloc(nx2);
    prev = sf_floatalloc(nx);

    cwave = sf_complexalloc(nk);
    prop = sf_input("prop");

    if (NULL != sf_getstring("left")) {
	if (SF_FLOAT != sf_gettype(prop)) sf_error("Need float prop");

	left = sf_input("left");
	right = sf_input("right");

	if (!sf_histint(prop,"n1",&m1)) sf_error("No n1= in prop");
	if (!sf_histint(prop,"n2",&m2)) sf_error("No n2= in prop");

	if (!sf_histint(left,"n1",&n2) || n2 != nk) sf_error("Need n1=%d in left",nk);
	if (!sf_histint(left,"n2",&n2) || n2 != m1) sf_error("Need n2=%d in left",m1);
	
	if (!sf_histint(right,"n1",&n2) || n2 != m2) sf_error("Need n1=%d in right",m2);
	if (!sf_histint(right,"n2",&n2) || n2 != nx) sf_error("Need n2=%d in right",nx);

	lft = sf_floatalloc2(nk,m1);
	mid = sf_floatalloc2(m1,m2);
	rht = sf_floatalloc2(m2,nx);

	sf_floatread(lft[0],nk*m1,left);
	sf_floatread(rht[0],nx*m2,right);
	sf_floatread(mid[0],m1*m2,prop);

	sf_fileclose(left);
	sf_fileclose(right);
	sf_fileclose(prop);

	mat = NULL;

	cwavem = sf_complexalloc(nk);
	wave = sf_floatalloc2(nx,m1);
    } else {
	if (SF_COMPLEX != sf_gettype(prop)) sf_error("Need complex prop");

	lft = NULL;
	rht = NULL;
	mid = NULL;

	mat = sf_complexalloc2(nk,nx);
	sf_complexread(mat[0],nx*nk,prop);
	sf_fileclose(prop);

	cwavem = NULL;
	wave = NULL;
    }
    
    sf_floatread (curr,nx,inp);
    for (ix = 0; ix < nx; ix++) {
	prev[ix] = 0;
    }
    for (ix = nx; ix < nx2; ix++) {
	curr[ix] = 0.;
    }

    /* propagation in time */
    for (it=0; it < nt; it++) {
	fft1(curr,cwave);

	if (NULL == mat) {
	    for (im = 0; im < m1; im++) {
		for (ik = 0; ik < nk; ik++) {
#ifdef SF_HAS_COMPLEX_H
		    cwavem[ik] = cwave[ik]*lft[im][ik]/nx2;
#else
		    cwavem[ik] = sf_crmul(cwave[ik],lft[im][ik]/nx2);
#endif
		}
		ifft1(wave[im],cwavem);
	    }
	}

	for (ix = 0; ix < nx; ix++) {
	    f = curr[ix];
	    curr[ix] = -prev[ix];
	    prev[ix] = f;

	    if (NULL == mat) {
		for (im = 0; im < m1; im++) {
		    for (ik = 0; ik < m2; ik++) {
			curr[ix] += rht[ix][ik]*mid[ik][im]*wave[im][ix];
		    }
		}
	    } else {
		for (ik = 0; ik < nk; ik++) {
#ifdef SF_HAS_COMPLEX_H
		    curr[ix] += crealf(mat[ix][ik] * cwave[ik]);
#else
		    curr[ix] += crealf(sf_cmul(mat[ix][ik],cwave[ik]));
#endif
		}
	    }
	} 

	sf_floatwrite(curr,nx,out);
    }
    
    exit(0);
}
