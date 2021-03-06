#include <cassert>
#include "pownth.h"

template <typename FLOAT>
__attribute__((noinline))
void dd_accumulate(FLOAT &xh, FLOAT &xl, const FLOAT &dx){
#if 0
	FLOAT yl  = xl + dx;
	FLOAT yh  = xh + yl;
	FLOAT res = yl - (yh - xh);
	xh = yh;
	xl = res;
#else
	FLOAT yh  = xh + dx;
	FLOAT res = dx - (yh - xh);
	FLOAT yl  = xl + res;

	FLOAT zh  = yh + yl;
	FLOAT zl  = yl - (zh - yh);

	xh = zh;
	xl = zl;
#endif
}

template <typename FLOAT>
__attribute__((noinline))
void dd_twosum(FLOAT &a, FLOAT &b){ // Knuth, 1969
	FLOAT x = a+b;
	FLOAT c = x-a;
	FLOAT y = (a-(x-c)) + (b-c);
	
	a = x;
	b = y;
}

struct Force{
	dvec3 acc;
	dvec3 jrk;
	dvec3 snp;
	dvec3 crk;
};

struct Particle{
	enum {
		order = 8,
		flops = 144,
	};
	long  id;
	double mass;
	double tlast;
	double dt;
	dvec3 posH;
	dvec3 posL;
	dvec3 vel;
	dvec3 acc;
	dvec3 jrk;
	dvec3 snp;
	dvec3 crk;
	dvec3 d4a;
	dvec3 d5a;
	dvec3 d6a; // save all the derivatives
	dvec3 d7a;
	double pad[3]; // 40 DP words

	Particle(
			const long    _id, 
			const double  _mass,
			const dvec3 & _pos,
			const dvec3 & _vel)
		: id(_id), mass(_mass), tlast(0.0), dt(0.0), 
		  posH(_pos), posL(0.0), vel(_vel), 
		  acc(0.0), jrk(0.0), 
		  snp(0.0), crk(0.0),
		  d4a(0.0), d5a(0.0)
	{
		assert(sizeof(Particle) == 320);
	}

	void dump(FILE *fp){
		fprintf(fp, "%8ld %A", id, mass);
		fprintf(fp, "   %A %A %A", posH.x, posH.y, posH.z);
		fprintf(fp, "   %A %A %A", posL.x, posL.y, posL.z);
		fprintf(fp, "   %A %A %A", vel.x, vel.y, vel.z);
		fprintf(fp, "   %A %A %A", acc.x, acc.y, acc.z);
		fprintf(fp, "   %A %A %A", jrk.x, jrk.y, jrk.z);
		fprintf(fp, "   %A %A %A", snp.x, snp.y, snp.z);
		fprintf(fp, "   %A %A %A", crk.x, crk.y, crk.z);
		fprintf(fp, "   %A %A %A", d4a.x, d4a.y, d4a.z);
		fprintf(fp, "   %A %A %A", d5a.x, d5a.y, d5a.z);
		fprintf(fp, "   %A %A %A", d6a.x, d6a.y, d6a.z);
		fprintf(fp, "   %A %A %A", d7a.x, d7a.y, d7a.z);
		fprintf(fp, "\n");
	}
	void restore(FILE *fp){
		int nread = 0;
		nread += fscanf(fp, "%ld %lA", &id, &mass);
		nread += fscanf(fp, "%lA %lA %lA", &posH.x, &posH.y, &posH.z);
		nread += fscanf(fp, "%lA %lA %lA", &posL.x, &posL.y, &posL.z);
		nread += fscanf(fp, "%lA %lA %lA", &vel.x, &vel.y, &vel.z);
		nread += fscanf(fp, "%lA %lA %lA", &acc.x, &acc.y, &acc.z);
		nread += fscanf(fp, "%lA %lA %lA", &jrk.x, &jrk.y, &jrk.z);
		nread += fscanf(fp, "%lA %lA %lA", &snp.x, &snp.y, &snp.z);
		nread += fscanf(fp, "%lA %lA %lA", &crk.x, &crk.y, &crk.z);
		nread += fscanf(fp, "%lA %lA %lA", &d4a.x, &d4a.y, &d4a.z);
		nread += fscanf(fp, "%lA %lA %lA", &d5a.x, &d5a.y, &d5a.z);
		nread += fscanf(fp, "%lA %lA %lA", &d6a.x, &d6a.y, &d6a.z);
		nread += fscanf(fp, "%lA %lA %lA", &d7a.x, &d7a.y, &d7a.z);
		assert(35 == nread);
	}

	void assign_force(const Force &f){
		acc = f.acc;
		jrk = f.jrk;
		snp = f.snp;
		crk = f.crk;
	}

	void init_dt(const double eta_s, const double dtmax){
		double s0 = acc.norm2();
		double s1 = jrk.norm2();
		double s2 = snp.norm2();
		double s3 = crk.norm2();

		double u = sqrt(s0*s2) + s1;
		double l = sqrt(s1*s3) + s2;

		double dtnat =  eta_s * sqrt(u/l);
		dt = 0.25 * dtmax;
		while(dt > dtnat) dt *= 0.5;
	}

	static double aarseth_step_quant(
			const dvec3 &a0, 
			const dvec3 &a1, 
			const dvec3 &a2, 
			const dvec3 &a3, // not used 
			const dvec3 &a4, // not used
			const dvec3 &a5, 
			const dvec3 &a6, 
			const dvec3 &a7, 
			const double etapow)
	{
		double s0 = a0.norm2();
		double s1 = a1.norm2();
		double s2 = a2.norm2();

		double s5 = a5.norm2();
		double s6 = a6.norm2();
		double s7 = a7.norm2();

		double u = sqrt(s0*s2) + s1;
		double l = sqrt(s5*s7) + s6;
		const double dtpow = etapow * (u/l);
		return pow_one_nth_quant<10>(dtpow);
	}
	static double aarseth_step(
			const dvec3 &a0, 
			const dvec3 &a1, 
			const dvec3 &a2, 
			const dvec3 &a3, // not used 
			const dvec3 &a4, // not used
			const dvec3 &a5, 
			const dvec3 &a6, 
			const dvec3 &a7, 
			const double eta)
	{
		double s0 = a0.norm2();
		double s1 = a1.norm2();
		double s2 = a2.norm2();

		double s5 = a5.norm2();
		double s6 = a6.norm2();
		double s7 = a7.norm2();

		double u = sqrt(s0*s2) + s1;
		double l = sqrt(s5*s7) + s6;
		return eta * pow(u/l, 1./10.);
	}

	void recalc_dt(const double eta, const double dtmax){
		const double dta = aarseth_step(acc, jrk, snp, crk, d4a, d5a, d6a, d7a, eta);
		dt = dtmax;
		while(dt > dta) dt *= 0.5;
	}

	void correct(const Force &f, const double eta, const double etapow, const double dtlim){
		const double h = 0.5 * dt;
		const double hinv = 2.0/dt;

		const dvec3 Ap = (f.acc + acc);
		const dvec3 Am = (f.acc - acc);
		const dvec3 Jp = (f.jrk + jrk)*h;
		const dvec3 Jm = (f.jrk - jrk)*h;
		const dvec3 Sp = (f.snp + snp)*(h*h);
		const dvec3 Sm = (f.snp - snp)*(h*h);
		const dvec3 Cp = (f.crk + crk)*(h*h*h);
		const dvec3 Cm = (f.crk - crk)*(h*h*h);

		// do correct
		dvec3 vel1 = vel + h*(Ap +  (-3./7.)*Jm + (2./21)*Sp - (1./105.)*Cm);
		dvec3 dpos = h*((vel + vel1) + h*((-3./7.)*Am + (2./21)*Jp - (1./105.)*Sm));
		dd_accumulate(posH, posL, dpos);

		vel = vel1;
		acc = f.acc;
		jrk = f.jrk;
		snp = f.snp;
		crk = f.crk;
		tlast += dt;

		// taylor series
		double hinv2 = hinv*hinv; 
		double hinv3 = hinv2*hinv; 
		double hinv4 = hinv2*hinv2;
		double hinv5 = hinv2*hinv3;
		double hinv6 = hinv3*hinv3;
		double hinv7 = hinv3*hinv4;

		d4a = (hinv4 *   24./32.)*(         - 5.*Jm + 5.*Sp - Cm);
		d5a = (hinv5 *  120./32.)*( 21.*Am - 21.*Jp + 8.*Sm - Cp);
		d6a = (hinv6 *  720./32.)*(              Jm -    Sp + Cm/3.);
		d7a = (hinv7 * 5040./32.)*( -5.*Am +  5.*Jp - 2.*Sm + Cp/3.);

		double h2 = 0.5*h, h3 = (1./3.)*h;
		d4a += h*(d5a + h2*(d6a + h3*d7a));
		d5a += h*(d6a + h2*d7a);
		d6a += h*d7a;

		// update timestep
#if 0
		const double dta = aarseth_step(acc, jrk, snp, crk, d4a, d5a, d6a, d7a, eta);
		dt = dtlim;
		while(dt > dta) dt *= 0.5;
#else
		const double dtq = aarseth_step_quant(acc, jrk, snp, crk, d4a, d5a, d6a, d7a, etapow);
		dt = dtq<dtlim ? dtq : dtlim;
#endif
	}

#ifdef __AVX__
	static void copy_particle(Particle &dst, const Particle &src){
		const v4df *vsrc = (const v4df *)&src;
		const v4df ym0 = vsrc[0];
		const v4df ym1 = vsrc[1];
		const v4df ym2 = vsrc[2];
		const v4df ym3 = vsrc[3];
		const v4df ym4 = vsrc[4];
		const v4df ym5 = vsrc[5];
		const v4df ym6 = vsrc[6];
		const v4df ym7 = vsrc[7];
		const v4df ym8 = vsrc[8];
		const v4df ym9 = vsrc[9];

		v4df *vdst = (v4df *)&dst;
		vdst[0] = ym0;
		vdst[1] = ym1;
		vdst[2] = ym2;
		vdst[3] = ym3;
		vdst[4] = ym4;
		vdst[5] = ym5;
		vdst[6] = ym6;
		vdst[7] = ym7;
		vdst[8] = ym8;
		vdst[9] = ym9;
	}
	Particle(const Particle &p){
		copy_particle(*this, p);
	}
	const Particle &operator=(const Particle &p){
		copy_particle(*this, p);
		return (*this);
	}
#endif
} __attribute__ ((aligned(32)));

#ifdef AVX_GRAVITY
#include "hermite8dd-avx.h"
#elif defined HPC_ACE_GRAVITY
#include "hermite8dd-k.h"
#else
struct Gravity{
	typedef Particle GParticle;
	struct GPredictor{
		dvec3  posH;
		double mass;
		dvec3  posL;
		long   id;
		dvec3  vel;
		dvec3  acc;
		dvec3  jrk; // 17 DP

		GPredictor(const GParticle &p, const double tsys){
			assert(sizeof(GPredictor) == 136);

			const double dt = tsys - p.tlast;
			const double dt2 = (1./2.) * dt;
			const double dt3 = (1./3.) * dt;
			const double dt4 = (1./4.) * dt;
			const double dt5 = (1./5.) * dt;
			const double dt6 = (1./6.) * dt;
			const double dt7 = (1./7.) * dt;

			posH = p.posH;
			mass = p.mass;
			posL = p.posL + dt * (p.vel + dt2 * (p.acc + dt3 * (p.jrk + dt4 * (p.snp + dt5 * (p.crk + dt6 * (p.d4a + dt7 * (p.d5a)))))));
			vel  = p.vel + dt * (p.acc + dt2 * (p.jrk + dt3 * (p.snp + dt4 * (p.crk + dt5 * (p.d4a + dt6 * (p.d5a))))));
			acc  = p.acc + dt * (p.jrk + dt2 * (p.snp + dt3 * (p.crk + dt4 * (p.d4a + dt5 * (p.d5a)))));
			jrk  = p.jrk + dt * (p.snp + dt2 * (p.crk + dt3 * (p.d4a + dt4 * (p.d5a))));
			id   = p.id;
		}
	};

	const int  nbody;
	GParticle  *ptcl;
	GPredictor *pred;


	Gravity(const int _nbody) : nbody(_nbody) {
		ptcl = allocate<GParticle,  64> (nbody);
		pred = allocate<GPredictor, 64> (nbody);
	}
	~Gravity(){
		free(ptcl);
		free(pred);
	}

	void set_jp(const int addr, const Particle &p){
		ptcl[addr] = p;
	}

	void predict_all(const double tsys){
#pragma omp parallel for
		for(int i=0; i<nbody; i++){
			pred[i] = GPredictor(ptcl[i], tsys);
		}
	}

	void calc_force_on_first_nact(
			const int    nact,
			const double eps2,
			Force        force[] )
	{
		const int ni = nact;
		const int nj = nbody;
#pragma omp parallel for
		for(int i=0; i<ni; i++){
			const dvec3 posHi = pred[i].posH;
			const dvec3 posLi = pred[i].posL;
			const dvec3 veli = pred[i].vel;
			const dvec3 acci = pred[i].acc;
			const dvec3 jrki = pred[i].jrk;
			dvec3 acc(0.0);
			dvec3 jrk(0.0);
			dvec3 snp(0.0);
			dvec3 crk(0.0);
			for(int j=0; j<nj; j++){
				const dvec3  dr    = (pred[j].posH - posHi) + (pred[j].posL - posLi);
				const dvec3  dv    = pred[j].vel - veli;
				const dvec3  da    = pred[j].acc - acci;
				const dvec3  dj    = pred[j].jrk - jrki;

				const double r2    = eps2 + dr*dr;
				if(r2 == eps2) continue;
				const double drdv  = dr*dv;
				const double dvdv  = dv*dv;
				const double drda  = dr*da;
				const double dvda  = dv*da;
				const double drdj  = dr*dj;

				const double ri2   = 1.0 / r2;
				const double mri3  = pred[j].mass * ri2 * sqrt(ri2);
				const double alpha = drdv * ri2;
				const double beta  = (dvdv + drda)*ri2 + alpha*alpha;
				const double gamma = (3.0*dvda + drdj)*ri2 + alpha*(3.0*beta - 4.0*alpha*alpha);
				
				acc += mri3 * dr;
				dvec3 tmp1 = dv + (-3.0*alpha) * dr;
				jrk += mri3 * tmp1;
				dvec3 tmp2 = da + (-6.0*alpha) * tmp1 + (-3.0*beta) * dr;
				snp += mri3 * tmp2;
				dvec3 tmp3 = dj + (-9.0*alpha) * tmp2 + (-9.0*beta) * tmp1 + (-3.0*gamma) * dr;
				crk += mri3 * tmp3;
			}
			force[i].acc = acc;
			force[i].jrk = jrk;
			force[i].snp = snp;
			force[i].crk = crk;
		}
	}

	void calc_potential(
			const double eps2,
			double       potbuf[] )
	{
		const int ni = nbody;
		const int nj = nbody;
#pragma omp parallel for
		for(int i=0; i<ni; i++){
			double pot = 0.0;
			const dvec3 posHi = ptcl[i].posH;
			const dvec3 posLi = ptcl[i].posL;
			for(int j=0; j<nj; j++){
				if(j == i) continue;
				const dvec3  posHj = ptcl[j].posH;
				const dvec3  posLj = ptcl[j].posL;
				const dvec3  dr   = (posHj - posHi) + (posLj - posLi);;
				const double r2   = eps2 + dr*dr;
				pot -= ptcl[j].mass / sqrt(r2);
			}
			potbuf[i] = pot;
		}
	}
};
#endif // !defined AVX_GRAVITY

