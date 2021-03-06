#include "allocate.h"
#include "vector3.h"
#include "hermite8dd.h"
#include "hermite8dd-k.h"

void Gravity::predict_all_rp(
		const int nbody, 
		const double s_tsys, 
		const GParticle * __restrict ptcl,
		GPredictor      * __restrict pred)
{
	const v2r8 tsys(s_tsys);
	const int nb2 = nbody / 2;
#pragma omp parallel for
	for(int i=0; i<nb2; i++){
		const v2r8 dt  = tsys - ptcl[i].tlast;
		const v2r8 dt2 = v2r8(1./2.) * dt;
		const v2r8 dt3 = v2r8(1./3.) * dt;
		const v2r8 dt4 = v2r8(1./4.) * dt;
		const v2r8 dt5 = v2r8(1./5.) * dt;
		const v2r8 dt6 = v2r8(1./6.) * dt;
		const v2r8 dt7 = v2r8(1./7.) * dt;

		pred[i].mass = ptcl[i].mass;
#pragma loop unroll
		for(int k=0; k<3; k++){
			pred[i].posH[k] = ptcl[i].posH[k];
			pred[i].posL[k] = 
				ptcl[i].posL[k] + dt * (
					ptcl[i].vel[k] + dt2 * (
						ptcl[i].acc[k] + dt3 * (
							ptcl[i].jrk[k] + dt4 * (
								ptcl[i].snp[k] + dt5 * (
									ptcl[i].crk[k] + dt6 * (
										ptcl[i].d4a[k] + dt7 * (
											ptcl[i].d5a[k])))))));
			pred[i].vel[k] = 
				ptcl[i].vel[k] + dt * (
					ptcl[i].acc[k] + dt2 * (
						ptcl[i].jrk[k] + dt3 * (
							ptcl[i].snp[k] + dt4 * (
								ptcl[i].crk[k] + dt5 * (
									ptcl[i].d4a[k] + dt6 * (
										ptcl[i].d5a[k]))))));
			pred[i].acc[k] = 
				ptcl[i].acc[k] + dt * (
						ptcl[i].jrk[k] + dt2 * (
							ptcl[i].snp[k] + dt3 * (
								ptcl[i].crk[k] + dt4 * (
									ptcl[i].d4a[k] + dt5 * (
										ptcl[i].d5a[k])))));
			pred[i].jrk[k] = 
				ptcl[i].jrk[k] + dt * (
						ptcl[i].snp[k] + dt2 * (
							ptcl[i].crk[k] + dt3 * (
								ptcl[i].d4a[k] + dt4 * (
									ptcl[i].d5a[k]))));


		}
	}
}


void Gravity::calc_force_in_range(
		const int    is,
		const int    ie,
		const double eps2_s,
		Force * __restrict force )
{
	static GForce fobuf[MAXTHREAD][NIMAX/2];
	int nthreads;
#pragma omp parallel
	{
		const int tid = omp_get_thread_num();
#pragma omp master
		nthreads = omp_get_num_threads();

		const int nj = nbody;
		for(int i=is, ii=0; i<ie; i+=2, ii++){
			v4r8 ax(0.0);
			v4r8 ay(0.0);
			v4r8 az(0.0);
			v4r8 jx(0.0);
			v4r8 jy(0.0);
			v4r8 jz(0.0);
			v4r8 sx(0.0);
			v4r8 sy(0.0);
			v4r8 sz(0.0);
			v4r8 cx(0.0);
			v4r8 cy(0.0);
			v4r8 cz(0.0);
			const v2r8 xHi = pred[i/2].posH[0];
			const v2r8 yHi = pred[i/2].posH[1];
			const v2r8 zHi = pred[i/2].posH[2];
			const v2r8 xLi = pred[i/2].posL[0];
			const v2r8 yLi = pred[i/2].posL[1];
			const v2r8 zLi = pred[i/2].posL[2];
			const v2r8 vxi = pred[i/2].vel[0];
			const v2r8 vyi = pred[i/2].vel[1];
			const v2r8 vzi = pred[i/2].vel[2];
			const v2r8 axi = pred[i/2].acc[0];
			const v2r8 ayi = pred[i/2].acc[1];
			const v2r8 azi = pred[i/2].acc[2];
			const v2r8 jxi = pred[i/2].jrk[0];
			const v2r8 jyi = pred[i/2].jrk[1];
			const v2r8 jzi = pred[i/2].jrk[2];
			const v2r8 eps2(eps2_s);
#pragma omp for // calculate partial force
			for(int j=0; j<nj; j+=2){
				const v2r8 mj  = pred[j/2].mass;
				const v2r8 xHj = pred[j/2].posH[0];
				const v2r8 yHj = pred[j/2].posH[1];
				const v2r8 zHj = pred[j/2].posH[2];
				const v2r8 xLj = pred[j/2].posL[0];
				const v2r8 yLj = pred[j/2].posL[1];
				const v2r8 zLj = pred[j/2].posL[2];
				const v2r8 vxj = pred[j/2].vel[0];
				const v2r8 vyj = pred[j/2].vel[1];
				const v2r8 vzj = pred[j/2].vel[2];
				const v2r8 axj = pred[j/2].acc[0];
				const v2r8 ayj = pred[j/2].acc[1];
				const v2r8 azj = pred[j/2].acc[2];
				const v2r8 jxj = pred[j/2].jrk[0];
				const v2r8 jyj = pred[j/2].jrk[1];
				const v2r8 jzj = pred[j/2].jrk[2];

				const v4r8 dx = (v4r8_llhh(xHj) - v4r8(xHi)) + (v4r8_llhh(xLj) - v4r8(xLi));
				const v4r8 dy = (v4r8_llhh(yHj) - v4r8(yHi)) + (v4r8_llhh(yLj) - v4r8(yLi));
				const v4r8 dz = (v4r8_llhh(zHj) - v4r8(zHi)) + (v4r8_llhh(zLj) - v4r8(zLi));
				const v4r8 dvx = v4r8_llhh(vxj) - v4r8(vxi);
				const v4r8 dvy = v4r8_llhh(vyj) - v4r8(vyi);
				const v4r8 dvz = v4r8_llhh(vzj) - v4r8(vzi);
				const v4r8 dax = v4r8_llhh(axj) - v4r8(axi);
				const v4r8 day = v4r8_llhh(ayj) - v4r8(ayi);
				const v4r8 daz = v4r8_llhh(azj) - v4r8(azi);
				const v4r8 djx = v4r8_llhh(jxj) - v4r8(jxi);
				const v4r8 djy = v4r8_llhh(jyj) - v4r8(jyi);
				const v4r8 djz = v4r8_llhh(jzj) - v4r8(jzi);

				const v4r8 r2    = ((v4r8(eps2) +dx*dx) + dy*dy) + dz*dz;
				const v4r8 rv = (dx *dvx + dy *dvy) + dz *dvz;
				const v4r8 v2 = (dvx*dvx + dvy*dvy) + dvz*dvz;
				const v4r8 ra = (dx *dax + dy *day) + dz *daz;
				const v4r8 va = (dvx*dax + dvy*day) + dvz*daz;
				const v4r8 rj = (dx *djx + dy *djy) + dz *djz;

				const v4r8 rinv   = r2.rsqrta_x8() & (v4r8(eps2) < r2);
				const v4r8 rinv2  = rinv * rinv;
				const v4r8 mrinv  = rinv * v4r8_llhh(mj);
				const v4r8 mrinv3 = mrinv * rinv2;

				const v4r8 alpha = rv * rinv2;
				const v4r8 beta  = (v2 + ra) * rinv2 + (alpha * alpha);
				const v4r8 gamma = (v4r8(3.0) * va + rj) * rinv2
					+ alpha * (v4r8(3.0) * beta - v4r8(4.0) * (alpha * alpha));
				const v4r8 alpha3 = v4r8(3.0) * alpha;
				const v4r8 alpha6 = alpha3 + alpha3;
				const v4r8 alpha9 = v4r8(3.0) * alpha3;
				const v4r8 beta3  = v4r8(3.0) * beta;
				const v4r8 beta9  = v4r8(3.0) * beta3;
				const v4r8 gamma3 = v4r8(3.0) * gamma;
				
				ax += mrinv3 * dx;
				ay += mrinv3 * dy;
				az += mrinv3 * dz;

				const v4r8 tx = dvx - alpha3 * dx;
				const v4r8 ty = dvy - alpha3 * dy;
				const v4r8 tz = dvz - alpha3 * dz;

				jx += mrinv3 * tx;
				jy += mrinv3 * ty;
				jz += mrinv3 * tz;

				const v4r8 ux = dax - alpha6 * tx - beta3 * dx;
				const v4r8 uy = day - alpha6 * ty - beta3 * dy;
				const v4r8 uz = daz - alpha6 * tz - beta3 * dz;

				sx += mrinv3 * ux;
				sy += mrinv3 * uy;
				sz += mrinv3 * uz;

				cx += mrinv3 * (djx - alpha9 * ux - beta9 * tx - gamma3 * dx);
				cy += mrinv3 * (djy - alpha9 * uy - beta9 * ty - gamma3 * dy);
				cz += mrinv3 * (djz - alpha9 * uz - beta9 * tz - gamma3 * dz);
			} // for (j)
			fobuf[tid][ii].save(
					ax.hadd(), ay.hadd(), az.hadd(), 
					jx.hadd(), jy.hadd(), jz.hadd(),
					sx.hadd(), sy.hadd(), sz.hadd(),
					cx.hadd(), cy.hadd(), cz.hadd());
		}
	} // end omp paralle
	// reduction & store
#pragma omp parallel for
	for(int i=is; i<ie; i+=2){
		int ii = (i - is)/2;
		v2r8 ax, ay, az, 
			 jx, jy, jz,
			 sx, sy, sz,
			 cx, cy, cz;
		for(int ith=0; ith<nthreads; ith++){
			// fsum.accumulate(fobuf[ith][ii]);
			ax += fobuf[ith][ii].ax;
			ay += fobuf[ith][ii].ay;
			az += fobuf[ith][ii].az;
			jx += fobuf[ith][ii].jx;
			jy += fobuf[ith][ii].jy;
			jz += fobuf[ith][ii].jz;
			sx += fobuf[ith][ii].sx;
			sy += fobuf[ith][ii].sy;
			sz += fobuf[ith][ii].sz;
			cx += fobuf[ith][ii].cx;
			cy += fobuf[ith][ii].cy;
			cz += fobuf[ith][ii].cz;
		}
		ax.storel(&force[i+0].acc.x);
		ay.storel(&force[i+0].acc.y);
		az.storel(&force[i+0].acc.z);
		jx.storel(&force[i+0].jrk.x);
		jy.storel(&force[i+0].jrk.y);
		jz.storel(&force[i+0].jrk.z);
		sx.storel(&force[i+0].snp.x);
		sy.storel(&force[i+0].snp.y);
		sz.storel(&force[i+0].snp.z);
		cx.storel(&force[i+0].crk.x);
		cy.storel(&force[i+0].crk.y);
		cz.storel(&force[i+0].crk.z);

		ax.storeh(&force[i+1].acc.x);
		ay.storeh(&force[i+1].acc.y);
		az.storeh(&force[i+1].acc.z);
		jx.storeh(&force[i+1].jrk.x);
		jy.storeh(&force[i+1].jrk.y);
		jz.storeh(&force[i+1].jrk.z);
		sx.storeh(&force[i+1].snp.x);
		sy.storeh(&force[i+1].snp.y);
		sz.storeh(&force[i+1].snp.z);
		cx.storeh(&force[i+1].crk.x);
		cy.storeh(&force[i+1].crk.y);
		cz.storeh(&force[i+1].crk.z);
	}
}

void Gravity::calc_potential_rp(
		const int    nbody,
		const double eps2_s,
		const GParticle * __restrict ptcl,
		v2r8           (* __restrict xmbuf)[4], // {{xH,xL}, {yH,yL}, {zH,zL}, {m,m}}
		double          * __restrict potbuf)
{
	// setup cache
#pragma omp parallel for
	for(int ii=0; ii<nbody; ii+=2){
		//const v2r8 x2(ptcl[ii/2].posH[0], ptcl[ii/2].posL[0]);
		//const v2r8 y2(ptcl[ii/2].posH[1], ptcl[ii/2].posL[1]);
		//const v2r8 y2(ptcl[ii/2].posH[2], ptcl[ii/2].posL[2]);
		//const v2r8 m2(ptcl[ii/2].mass,    ptcl[ii/2].mass);
		const v2r8 xH = ptcl[ii/2].posH[0];
		const v2r8 yH = ptcl[ii/2].posH[1];
		const v2r8 zH = ptcl[ii/2].posH[2];
		const v2r8 xL = ptcl[ii/2].posL[0];
		const v2r8 yL = ptcl[ii/2].posL[1];
		const v2r8 zL = ptcl[ii/2].posL[2];
		const v2r8 mm = ptcl[ii/2].mass;

		xmbuf[ii+0][0] = v2r8::unpckl(xH, xL);
		xmbuf[ii+0][1] = v2r8::unpckl(yH, yL);
		xmbuf[ii+0][2] = v2r8::unpckl(zH, zL);
		xmbuf[ii+0][3] = v2r8::unpckl(mm, mm);

		xmbuf[ii+1][0] = v2r8::unpckh(xH, xL);
		xmbuf[ii+1][1] = v2r8::unpckh(yH, yL);
		xmbuf[ii+1][2] = v2r8::unpckh(zH, zL);
		xmbuf[ii+1][3] = v2r8::unpckh(mm, mm);
	}
	// evaluate potential, simple i-parallel
#pragma omp parallel for
	for(int i=0; i<nbody; i+=4){
		double (*xm8)[8] = (double (*)[8])(xmbuf + i);
		const v4r8 xHi(xm8[0][0], xm8[1][0], xm8[2][0], xm8[3][0]);
		const v4r8 xLi(xm8[0][1], xm8[1][1], xm8[2][1], xm8[3][1]);
		const v4r8 yHi(xm8[0][2], xm8[1][2], xm8[2][2], xm8[3][2]);
		const v4r8 yLi(xm8[0][3], xm8[1][3], xm8[2][3], xm8[3][3]);
		const v4r8 zHi(xm8[0][4], xm8[1][4], xm8[2][4], xm8[3][4]);
		const v4r8 zLi(xm8[0][5], xm8[1][5], xm8[2][5], xm8[3][5]);
		const v4r8 eps2(eps2_s);
		v4r8 pot(0.0);
		for(int j=0; j<nbody; j++){
			const v2r8 xj2 = xmbuf[j][0];
			const v2r8 yj2 = xmbuf[j][1];
			const v2r8 zj2 = xmbuf[j][2];
			const v2r8 mj2 = xmbuf[j][3];

			const v4r8 dx = (xHi - v2r8_bcl(xj2)) + (xLi - v2r8_bch(xj2));
			const v4r8 dy = (yHi - v2r8_bcl(yj2)) + (yLi - v2r8_bch(yj2));
			const v4r8 dz = (zHi - v2r8_bcl(zj2)) + (zLi - v2r8_bch(zj2));

			const v4r8 r2    = ((eps2 +dx*dx) + dy*dy) + dz*dz;
			const v4r8 rinv   = (r2.rsqrta_x8()) & (eps2 < r2);

			pot -= v4r8(mj2) * rinv;
		}
		*(v4r8 *)(potbuf + i) = pot;
	}
}
