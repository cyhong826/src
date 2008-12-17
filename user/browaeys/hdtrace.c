/* Multiple arrivals by marching down/up using Hamiltonian dynamics. */
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

#include <math.h>

#include <rsf.h>

#include "hdtrace.h"

#include "enogrid.h"
#include "grid1.h"
#include "symplectrace.h"

#ifndef _hdtrace_h

#define NS 4 /* number of outputs */
/*^*/

#endif

static int snap(float *f, int n);

static float **slow;   /* slowness [nx][nz] */
static int nx, nz, np, npx;
static float dx,dz,dp,x0,z0,p0,xm,zm,pm;
static sf_eno2 cvel;
static enogrid slice;
static grid1 *prev, *curr;
static pqv hvec;


void hdtrace_init (int order        /* interpolation order for velocity */, 
		   int iorder       /* interpolation order for values */,
		   int nx1          /* lateral samples */, 
		   int nz1          /* depth samples */, 
		   int np1          /* horizontal slowness samples */, 
		   float dx1        /* lateral sampling */, 
		   float dz1        /* depth sampling */, 
		   float dp1        /* horizontal slowness sampling */,
		   float x01        /* lateral origin */, 
		   float z01        /* depth origin */, 
		   float p01        /* horizontal slowness origin */,
                   float **vel     /* slowness [nx][nz] */)
/*< Initialize >*/
{
    int ix, kx, kp;
    float p, f[NS];

    slow = vel;

    nx = nx1; nz = nz1; np = np1;
    dx = dx1; dz = dz1; dp = dp1;
    x0 = x01; z0 = z01; p0 = p01;
    npx = np*nx; 
    xm = x0 + (nx-1)*dx;
    zm = z0 + (nz-1)*dz;
    pm = p0 + (np-1)*dp;

    cvel = sf_eno2_init (order, nz, nx);
    sf_eno2_set (cvel, slow);

    prev = (grid1*) sf_alloc(nx,sizeof(grid1));
    curr = (grid1*) sf_alloc(nx,sizeof(grid1));
    for (ix=0; ix < nx; ix++) {
	prev[ix] = grid1_init();
	curr[ix] = grid1_init();
    }

    for (kx=0; kx < nx; kx++) { /* loop in x */
	for (kp=0; kp < np; kp++) { /* loop in horizontal slowness */
	    p = p0+kp*dp;

            /* f is vector (traveltime, x, z, p) for one way dynamic rays */
	    f[0] = 0.;
	    f[1] = x0+kx*dx; 
	    f[2] = z0;
	    f[3] = p;
	    
	    grid1_insert(curr[kx],p,4,f);
	}
    } 

    slice = enogrid_init (iorder, nx, NS, prev);
    hvec = (pqv)malloc(sizeof(pqv));
    nc4_init();
}

void hdtrace_close (void)
/*< Free allocated storage >*/
{
    free(hvec);

    sf_eno2_close (cvel);
    enogrid_close(slice);

    /* close grid */
    free(prev);
    free(curr);
}

void hdtrace_write (sf_file out)
/*< Write it out >*/
{
    int ix;
    for (ix=0; ix < nx; ix++) {
	grid1_write(curr[ix],NS,out);
    }
}

void hdtrace_step (int kz, int up) 
/*< Step in depth >*/
{
    int incell, step;
    int kx, kp, k, ix, iz, ip;
    float x1, z1, p1[2], f[4], ss, ds, fx, fz, fp;
    float x2, z2, p2, t;

    /* grid dimension restrictions */
    /* dx.dp > 1/(4.pi.f) */
    /* dz.dp > 1/(4.pi.f) */

    if (up == -1) sf_warning("marching up in depth");
    if (up ==  1) sf_warning("marching down in depth");

    /* assign the previous slice for interpolation */
    for (ix=0; ix < nx; ix++) {
	grid1_close(prev[ix]);
	prev[ix] = curr[ix];
	curr[ix] = grid1_init();
    }

    for (kx=0; kx < nx; kx++) { /* loop in x */
	
	for (kp=0; kp < np; kp++) { /* loop in horizontal slowness */
	    k = kp + kx*np; /* index in a slice */

            /* initial dimensionless horizontal slowness */
	    p1[1] = p0+kp*dp;

            /* initial dimensionless vertical one-way slowness */
	    p1[0] = up*sqrt(1-p1[1]*p1[1]);

            /* initial position */
	    x1 = x0+kx*dx; 
	    z1 = z0+kz*dz; 
	 
	    /* decide if we are out already */
	    if ((kz == 0    && p1[0] < 0.) ||
	        (kz == nz-1 && p1[0] > 0.) ||
	        (kx == 0    && p1[1] < 0.) ||
	        (kx == nx-1 && p1[1] > 0.)) {
		f[0] = 0.;
		f[1] = x1;
		f[2] = z1;
		f[3] = p1[1];
		grid1_insert(curr[kx],p1[1],4,f);
		incell = 0;
		continue;
	    } else {
		incell = 1;
	    }

            /* Hamiltonian vector */
	    ss = slow_bilininterp(x1,z1,slow,nx,nz,dx,dz,x0,z0);
	    hvec_init(hvec,0.,x1,z1,ss*p1[1],ss*p1[0]);

            /* predict sigma step size to the next cell */
	    ds = nc4_cellstep(hvec,slow,nx,nz,dx,dz,x0,z0,dp,dp);

            /* Loop until previous depth level or exiting from phase space grid */
	    while (incell == 1) {

                /* symplectic cell step and traveltime integration */
		nc4_sigmastep(hvec,fabs(ds),slow,nx,nz,dx,dz,x0,z0);

		value_exitlevel(hvec,&step,&x2,&z2,&p2,&t);

                /* exit at previous/next depth level */
		if (step == 1) {
		    iz = kz-1;
		    fz = 0.;
		    fx = (x2-x0)/dx;
		    fp = (p2-p0)/dp;
		    ix = snap(&fx,nx);
		    ip = snap(&fp,np);
		    incell = 0;
		    break;
		}

                /* update for next sigma step */
		ds = nc4_cellstep(hvec,slow,nx,nz,dx,dz,x0,z0,dp,dp);

		if (step == 2) {
                    /* exit from spatial grid sides */
		    if (((xm-x2) <= dx && p2 > 0.) || (x2 <= dx && p2 < 0.)) {
			f[0] += t;
			f[1] = x2;
			f[2] = z2;
			f[3] = p2;
			grid1_insert(curr[kx],p2,4,f);
			ix = floor((x2-x0)/dx);
			fx = 0.;
			fz = (z2-z0)/dz;
			fp = (p2-p0)/dp;
			iz = snap(&fz,nz);
			ip = snap(&fp,np);
			incell = 0;
			break;
		    }       
		}

		if (step == 3) {
                    /* exit from slowness grid sides (overturning ray) */
		    if (((pm-p2) <= dp && ds > 0.) ||			  
			(p2 <= dp && ds < 0.)) {
			f[0] += t;
			f[1] = x2;
			f[2] = z2;
			f[3] = p2;
			grid1_insert(curr[kx],p2,4,f);
			ip = floor((p2)/dp);
			fp = 0.;
			fz = (z2-z0)/dz;
			fx = (x2-x0)/dx;
			iz = snap(&fz,nz);
			ix = snap(&fx,nx);
			incell = 0;
			break;
		    }
		}
	    }


	    value_exitlevel(hvec,&step,&x2,&z2,&p2,&t);

	    if (step == 1) {/* to previous level */
		/* interpolate */
		enogrid_apply(slice,ix,fx,p2,f);
		if (f[0] < 0.) f[0]=0.;
		f[0] += t;
		if (f[1] < x0) f[1]=x0;
		if (f[1] > xm) f[1]=xm;
		if (f[2] < z0) f[2]=z0;
		if (f[2] > zm) f[2]=zm;
		if (f[3] < p0) f[3]=p0;
		if (f[3] > pm) f[3]=pm;
	    } else {
		f[0] += t;
		f[1] = x2;
		f[2] = z2;
		f[3] = p2;
	    }
	    
	    grid1_insert(curr[kx],p1[1],4,f);

	} /* kp */
    } /* kx */
}


static int snap(float *f, int n)
{
    int i;

    i = floorf(*f);
    if (i < 0) {
	i=0;
	*f=0.;
    } else if (i >= n-1) {
	i=n-1;
	*f=0.;
    } else {
	*f -= i;
    }

    return i;
}
