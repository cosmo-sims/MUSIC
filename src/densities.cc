// This file is part of MUSIC
// A software package to generate ICs for cosmological simulations
// Copyright (C) 2010-2024 by Oliver Hahn
// 
// monofonIC is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// monofonIC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <cstring>

#include "math/special.hh"

#include "densities.hh"
#include "random.hh"
#include "convolution_kernel.hh"

//TODO: this should be a larger number by default, just to maintain consistency with old default
#define DEF_RAN_CUBE_SIZE 32


/* interpolate upwards in the hierarchy */
template <typename m1, typename m2>
void fft_coarsen(m1 &v, m2 &V)
{
	size_t nxf = v.size(0), nyf = v.size(1), nzf = v.size(2), nzfp = nzf + 2;
	size_t nxF = V.size(0), nyF = V.size(1), nzF = V.size(2), nzFp = nzF + 2;

	real_t *rcoarse = new real_t[nxF * nyF * nzFp];
	complex_t *ccoarse = reinterpret_cast<complex_t *>(rcoarse);

	real_t *rfine = new real_t[nxf * nyf * nzfp];
	complex_t *cfine = reinterpret_cast<complex_t *>(rfine);

	fftw_plan_t
		pf = FFTW_API(plan_dft_r2c_3d)(nxf, nyf, nzf, rfine, cfine, FFTW_ESTIMATE),
		ipc = FFTW_API(plan_dft_c2r_3d)(nxF, nyF, nzF, ccoarse, rcoarse, FFTW_ESTIMATE);

#pragma omp parallel for
	for (int i = 0; i < (int)nxf; i++)
		for (int j = 0; j < (int)nyf; j++)
			for (int k = 0; k < (int)nzf; k++)
			{
				size_t q = ((size_t)i * nyf + (size_t)j) * nzfp + (size_t)k;
				rfine[q] = v(i, j, k);
			}

	FFTW_API(execute)(pf);

	double fftnorm = 1.0 / ((double)nxF * (double)nyF * (double)nzF);

#pragma omp parallel for
	for (int i = 0; i < (int)nxF; i++)
		for (int j = 0; j < (int)nyF; j++)
			for (int k = 0; k < (int)nzF / 2 + 1; k++)
			{
				int ii(i), jj(j), kk(k);

				if (i > (int)nxF / 2)
					ii += (int)nxf / 2;
				if (j > (int)nyF / 2)
					jj += (int)nyf / 2;

				size_t qc, qf;

				double kx = (i <= (int)nxF / 2) ? (double)i : (double)(i - (int)nxF);
				double ky = (j <= (int)nyF / 2) ? (double)j : (double)(j - (int)nyF);
				double kz = (k <= (int)nzF / 2) ? (double)k : (double)(k - (int)nzF);

				qc = ((size_t)i * nyF + (size_t)j) * (nzF / 2 + 1) + (size_t)k;
				qf = ((size_t)ii * nyf + (size_t)jj) * (nzf / 2 + 1) + (size_t)kk;

				std::complex<double> val_fine(RE(cfine[qf]), IM(cfine[qf]));
				double phase = (kx / nxF + ky / nyF + kz / nzF) * 0.5 * M_PI;

				std::complex<double> val_phas(cos(phase), sin(phase));

				val_fine *= val_phas * fftnorm / 8.0; 

				double blend_coarse_x = Meyer_scaling_function(kx, nxF / 2);
				double blend_coarse_y = Meyer_scaling_function(ky, nyF / 2);
				double blend_coarse_z = Meyer_scaling_function(kz, nzF / 2);

				double blend_coarse = blend_coarse_x*blend_coarse_y*blend_coarse_z;

				RE(ccoarse[qc]) = val_fine.real() * blend_coarse;
				IM(ccoarse[qc]) = val_fine.imag() * blend_coarse;
			}

	delete[] rfine;

	FFTW_API(execute)(ipc);

#pragma omp parallel for
	for (int i = 0; i < (int)nxF; i++)
		for (int j = 0; j < (int)nyF; j++)
			for (int k = 0; k < (int)nzF; k++)
			{
				size_t q = ((size_t)i * nyF + (size_t)j) * nzFp + (size_t)k;
				V(i, j, k) = rcoarse[q];
			}

	delete[] rcoarse;

	FFTW_API(destroy_plan)(pf);
	FFTW_API(destroy_plan)(ipc);
}

/* interpolate downwards in the hierarchy */
template <typename m1, typename m2>
void fft_interpolate(m1 &V, m2 &v, int margin, bool from_basegrid = false)
{
	int oxf = v.offset(0), oyf = v.offset(1), ozf = v.offset(2);
	size_t nxf = v.size(0), nyf = v.size(1), nzf = v.size(2), nzfp = nzf + 2;
	size_t mxf = v.margin(0), myf = v.margin(1), mzf = v.margin(2);

	// adjust offsets to respect margins, all grids have 'margins' except basegrid (which is periodic)
	if (!from_basegrid)
	{
		oxf +=  mxf/2;
		oyf +=  myf/2;
		ozf +=  mzf/2;
	}
	else
	{
		oxf -= mxf/2;
		oyf -= myf/2;
		ozf -= mzf/2;
	}

	music::ulog.Print("FFT interpolate: offset=%d,%d,%d size=%d,%d,%d", oxf, oyf, ozf, nxf, nyf, nzf);

	// cut out piece of coarse grid that overlaps the fine:
	assert(nxf % 2 == 0 && nyf % 2 == 0 && nzf % 2 == 0);

	size_t nxc = nxf / 2, nyc = nyf / 2, nzc = nzf / 2, nzcp = nzf / 2 + 2;

	real_t *rcoarse = new real_t[nxc * nyc * nzcp];
	complex_t *ccoarse = reinterpret_cast<complex_t *>(rcoarse);

	real_t *rfine = new real_t[nxf * nyf * nzfp];
	complex_t *cfine = reinterpret_cast<complex_t *>(rfine);

	// copy coarse data to rcoarse[.]
	memset(rcoarse, 0, sizeof(real_t) * nxc * nyc * nzcp);

#pragma omp parallel for
	for (int i = 0; i < (int)nxc; ++i)
		for (int j = 0; j < (int)nyc; ++j)
			for (int k = 0; k < (int)nzc; ++k)
			{
				int ii(i);
				int jj(j);
				int kk(k);
				size_t q = ((size_t)ii * nyc + (size_t)jj) * nzcp + (size_t)kk;
				rcoarse[q] = V(oxf + i, oyf + j, ozf + k);
			}

#pragma omp parallel for
	for (int i = 0; i < (int)nxf; ++i)
		for (int j = 0; j < (int)nyf; ++j)
			for (int k = 0; k < (int)nzf; ++k)
			{
				size_t q = ((size_t)i * nyf + (size_t)j) * nzfp + (size_t)k;
				rfine[q] = v(i, j, k);
			}


	fftw_plan_t
		pc = FFTW_API(plan_dft_r2c_3d)(nxc, nyc, nzc, rcoarse, ccoarse, FFTW_ESTIMATE),
		pf = FFTW_API(plan_dft_r2c_3d)(nxf, nyf, nzf, rfine, cfine, FFTW_ESTIMATE),
		ipf = FFTW_API(plan_dft_c2r_3d)(nxf, nyf, nzf, cfine, rfine, FFTW_ESTIMATE);
	FFTW_API(execute)(pc);
	FFTW_API(execute)(pf);

	/*************************************************/
	//.. perform actual interpolation
	double fftnorm = 1.0 / ((double)nxf * (double)nyf * (double)nzf);

	// this enables filtered splicing of coarse and fine modes
	#pragma omp parallel for
	for (int i = 0; i < (int)nxc; i++)
		for (int j = 0; j < (int)nyc; j++)
			for (int k = 0; k < (int)nzc / 2 + 1; k++)
			{
				int ii(i), jj(j), kk(k);

				if (i > (int)nxc / 2)
					ii += (int)nxf / 2;
				if (j > (int)nyc / 2)
					jj += (int)nyf / 2;
				if (k > (int)nzc / 2)
					kk += (int)nzf / 2;

				size_t qc, qf;
				qc = ((size_t)i * (size_t)nyc + (size_t)j) * (nzc / 2 + 1) + (size_t)k;
				qf = ((size_t)ii * (size_t)nyf + (size_t)jj) * (nzf / 2 + 1) + (size_t)kk;

				double kx = (i <= (int)nxc / 2) ? (double)i : (double)(i - (int)nxc);
				double ky = (j <= (int)nyc / 2) ? (double)j : (double)(j - (int)nyc);
				double kz = (k <= (int)nzc / 2) ? (double)k : (double)(k - (int)nzc);

				double phase = -0.5 * M_PI * (kx / nxc + ky / nyc + kz / nzc);

				std::complex<double> val_phas(cos(phase), sin(phase));

				std::complex<double> val(RE(ccoarse[qc]), IM(ccoarse[qc]));
				val *= val_phas * 8.0;

				double blend_coarse_x = Meyer_scaling_function(kx, nxc / 4);
				double blend_coarse_y = Meyer_scaling_function(ky, nyc / 4);
				double blend_coarse_z = Meyer_scaling_function(kz, nzc / 4);
				double blend_coarse = blend_coarse_x*blend_coarse_y*blend_coarse_z;
				double blend_fine = 1.0-blend_coarse;

				RE(cfine[qf]) = blend_fine * RE(cfine[qf]) + blend_coarse * val.real();
				IM(cfine[qf]) = blend_fine * IM(cfine[qf]) + blend_coarse * val.imag();
			}

	delete[] rcoarse;

	/*************************************************/

	FFTW_API(execute)(ipf);

	FFTW_API(destroy_plan)(pf);
	FFTW_API(destroy_plan)(pc);
	FFTW_API(destroy_plan)(ipf);

// copy back and normalize
#pragma omp parallel for
	for (int i = 0; i < (int)nxf; ++i)
		for (int j = 0; j < (int)nyf; ++j)
			for (int k = 0; k < (int)nzf; ++k)
			{
				size_t q = ((size_t)i * nyf + (size_t)j) * nzfp + (size_t)k;
				v(i, j, k) = rfine[q] * fftnorm;
			}

	delete[] rfine;
}

/*******************************************************************************************/
/*******************************************************************************************/
/*******************************************************************************************/

void GenerateDensityUnigrid(config_file &cf, const cosmology::calculator* cc, tf_type type,
							refinement_hierarchy &refh, noise_generator &rand, grid_hierarchy &delta, bool smooth, bool shift)
{
	auto ptf = cc->transfer_function_.get();
	unsigned levelmin, levelmax, levelminPoisson;

	levelminPoisson = cf.get_value<unsigned>("setup", "levelmin");
	levelmin = cf.get_value_safe<unsigned>("setup", "levelmin_TF", levelminPoisson);
	levelmax = cf.get_value<unsigned>("setup", "levelmax");

	bool fix  = cf.get_value_safe<bool>("setup","fix_mode_amplitude",false);
	bool flip = cf.get_value_safe<bool>("setup","flip_mode_amplitude",false);

	unsigned nbase = 1 << levelmin;

	music::ilog.Print("- Running unigrid density convolution...");

	//... select the transfer function to be used
	convolution::kernel_creator *the_kernel_creator = convolution::get_kernel_map()["tf_kernel_k"];

	
	//... initialize convolution kernel
	convolution::kernel *the_tf_kernel = the_kernel_creator->create(cf, ptf, refh, type);

	//...
	music::ulog.Print("- Performing noise convolution on level %3d", levelmax);

	//... create convolution mesh
	DensityGrid<real_t> *top = new DensityGrid<real_t>(nbase, nbase, nbase);

	//... fill with random numbers
	rand.load(*top, levelmin);

	//... load convolution kernel
	the_tf_kernel->fetch_kernel(levelmin, false);

	//... perform convolution
	convolution::perform(the_tf_kernel, reinterpret_cast<void *>(top->get_data_ptr()), shift, fix, flip);

	//... clean up kernel
	delete the_tf_kernel;

	//... create multi-grid hierarchy
	delta.create_base_hierarchy(levelmin);

	//... copy convolved field to multi-grid hierarchy
	top->copy(*delta.get_grid(levelmin));

	//... delete convolution grid
	delete top;
}

/*******************************************************************************************/
/*******************************************************************************************/
/*******************************************************************************************/

void GenerateDensityHierarchy(config_file &cf, const cosmology::calculator* cc, tf_type type,
							  refinement_hierarchy &refh, noise_generator &rand,
							  grid_hierarchy &delta, bool smooth, bool shift)
{
	auto ptf = cc->transfer_function_.get();

	std::vector<long> rngseeds;
	std::vector<std::string> rngfnames;

	double tstart, tend;
#if defined(_OPENMP)
	tstart = omp_get_wtime();
#else
	tstart = (double)clock() / CLOCKS_PER_SEC;
#endif

	unsigned levelminPoisson = cf.get_value<unsigned>("setup", "levelmin");
	unsigned levelmin = cf.get_value_safe<unsigned>("setup", "levelmin_TF", levelminPoisson);
	unsigned levelmax = cf.get_value<unsigned>("setup", "levelmax");

	unsigned margin =  cf.get_value_safe<unsigned>("setup", "convolution_margin", 4);

	bool fix  = cf.get_value_safe<bool>("setup","fix_mode_amplitude",false);
	bool flip = cf.get_value_safe<bool>("setup","flip_mode_amplitude",false);
	bool fourier_splicing = true; //cf.get_value_safe<bool>("setup","fourier_splicing",true);

	if( fix && levelmin != levelmax ){
		music::wlog.Print("You have chosen mode fixing for a zoom. This is not well tested,\n please proceed at your own risk...");
	}

	unsigned nbase = 1 << levelmin;

	convolution::kernel_creator *the_kernel_creator  = convolution::get_kernel_map()["tf_kernel_k"];
	convolution::kernel *the_tf_kernel = the_kernel_creator->create(cf, ptf, refh, type);

	/***** PERFORM CONVOLUTIONS *****/
	{
		//... create and initialize density grids with white noise
		DensityGrid<real_t> *top(NULL);
		PaddedDensitySubGrid<real_t> *coarse(NULL), *fine(NULL);
		int nlevels = (int)levelmax - (int)levelmin + 1;

		// do coarse level
		top = new DensityGrid<real_t>(nbase, nbase, nbase);
		music::ilog.Print("Performing noise convolution on level %3d", levelmin);
		rand.load(*top, levelmin);
		convolution::perform(the_tf_kernel->fetch_kernel(levelmin, false), reinterpret_cast<void *>(top->get_data_ptr()), shift, fix, flip);

		delta.create_base_hierarchy(levelmin);

		top->copy(*delta.get_grid(levelmin));

		for (int i = 1; i < nlevels; ++i)
		{
			music::ilog.Print("Performing noise convolution on level %3d...", levelmin + i);
			/////////////////////////////////////////////////////////////////////////
			//... add new refinement patch
			music::ilog.Print("Allocating refinement patch");
			music::ilog.Print("   offset=(%5d,%5d,%5d)", refh.offset(levelmin + i, 0),
					refh.offset(levelmin + i, 1), refh.offset(levelmin + i, 2));
			music::ilog.Print("   size  =(%5d,%5d,%5d)", refh.size(levelmin + i, 0),
					refh.size(levelmin + i, 1), refh.size(levelmin + i, 2));

			if( refh.get_margin() > 0 ){
				fine = new PaddedDensitySubGrid<real_t>( refh.offset(levelmin + i, 0), refh.offset(levelmin + i, 1), refh.offset(levelmin + i, 2),
																								 refh.size(levelmin + i, 0), refh.size(levelmin + i, 1), refh.size(levelmin + i, 2),
																								 refh.get_margin(), refh.get_margin(), refh.get_margin() );
				music::ilog.Print("    margin = %d",refh.get_margin());
			}else{
				fine = new PaddedDensitySubGrid<real_t>( refh.offset(levelmin + i, 0), refh.offset(levelmin + i, 1), refh.offset(levelmin + i, 2),
																								 refh.size(levelmin + i, 0), refh.size(levelmin + i, 1), refh.size(levelmin + i, 2));
				music::ilog.Print("    margin = %d",refh.size(levelmin + i, 0)/2);
			}
			/////////////////////////////////////////////////////////////////////////

			// load white noise for patch
			rand.load(*fine, levelmin + i);

			convolution::perform(the_tf_kernel->fetch_kernel(levelmin + i, true),
										 reinterpret_cast<void *>(fine->get_data_ptr()), shift, fix, flip);

			if( fourier_splicing ){
				if (i == 1)
					fft_interpolate(*top, *fine, margin, true);
				else
					fft_interpolate(*coarse, *fine, margin, false);
			}

			delta.add_patch(refh.offset(levelmin + i, 0),
							refh.offset(levelmin + i, 1),
							refh.offset(levelmin + i, 2),
							refh.size(levelmin + i, 0),
							refh.size(levelmin + i, 1),
							refh.size(levelmin + i, 2));

			fine->copy_unpad(*delta.get_grid(levelmin + i));

			if (i == 1)
				delete top;
			else
				delete coarse;

			coarse = fine;
		}

		delete coarse;
	}

	delete the_tf_kernel;

#if defined(_OPENMP)
	tend = omp_get_wtime();
	if (true) //verbosity > 1 )
		music::ulog << " - Density calculation took " << tend - tstart << "s with " << omp_get_max_threads() << " threads." << std::endl;
#else
	tend = (double)clock() / CLOCKS_PER_SEC;
	if (true) //verbosity > 1 )
		music::ulog << " - Density calculation took " << tend - tstart << "s." << std::endl;
#endif

	if( !fourier_splicing ){
		coarsen_density(refh,delta,false);
	}
	music::ulog.Print("Finished computing the density field in %fs", tend - tstart);
}

/*******************************************************************************************/
/*******************************************************************************************/
/*******************************************************************************************/

void normalize_density(grid_hierarchy &delta)
{
	//return;

	long double sum = 0.0;
	unsigned levelmin = delta.levelmin(), levelmax = delta.levelmax();

	{
		size_t nx, ny, nz;

		nx = delta.get_grid(levelmin)->size(0);
		ny = delta.get_grid(levelmin)->size(1);
		nz = delta.get_grid(levelmin)->size(2);

#pragma omp parallel for reduction(+ : sum)
		for (int ix = 0; ix < (int)nx; ++ix)
			for (size_t iy = 0; iy < ny; ++iy)
				for (size_t iz = 0; iz < nz; ++iz)
					sum += (*delta.get_grid(levelmin))(ix, iy, iz);

		sum /= (double)(nx * ny * nz);
	}

	music::ilog << "- Top grid mean density is off by " << sum << ", correcting..." << std::endl;

	for (unsigned i = levelmin; i <= levelmax; ++i)
	{
		size_t nx, ny, nz;
		nx = delta.get_grid(i)->size(0);
		ny = delta.get_grid(i)->size(1);
		nz = delta.get_grid(i)->size(2);

#pragma omp parallel for
		for (int ix = 0; ix < (int)nx; ++ix)
			for (size_t iy = 0; iy < ny; ++iy)
				for (size_t iz = 0; iz < nz; ++iz)
					(*delta.get_grid(i))(ix, iy, iz) -= sum;
	}
}

void normalize_levelmin_density(grid_hierarchy &delta)
{
	//return;

	long double sum = 0.0;
	const unsigned levelmin = delta.levelmin();

	{
		size_t nx, ny, nz;

		nx = delta.get_grid(levelmin)->size(0);
		ny = delta.get_grid(levelmin)->size(1);
		nz = delta.get_grid(levelmin)->size(2);

#pragma omp parallel for reduction(+ : sum)
		for (int ix = 0; ix < (int)nx; ++ix)
			for (size_t iy = 0; iy < ny; ++iy)
				for (size_t iz = 0; iz < nz; ++iz)
					sum += (*delta.get_grid(levelmin))(ix, iy, iz);

		sum /= (double)(nx * ny * nz);
	}

	music::ilog << "- Top grid mean density is off by " << sum << ", correcting..." << std::endl;

	{
		size_t nx, ny, nz;
		nx = delta.get_grid(levelmin)->size(0);
		ny = delta.get_grid(levelmin)->size(1);
		nz = delta.get_grid(levelmin)->size(2);

#pragma omp parallel for
		for (int ix = 0; ix < (int)nx; ++ix)
			for (size_t iy = 0; iy < ny; ++iy)
				for (size_t iz = 0; iz < nz; ++iz)
					(*delta.get_grid(levelmin))(ix, iy, iz) -= sum;
	}
}


void coarsen_density(const refinement_hierarchy &rh, GridHierarchy<real_t> &u, bool bfourier_coarsening )
{
	const unsigned levelmin_TF = u.levelmin();

	if (bfourier_coarsening)
	{
		for (int i = levelmin_TF; i >= (int)rh.levelmin(); --i)
			fft_coarsen(*(u.get_grid(i)), *(u.get_grid(i - 1)));
	}
	else
	{
		mg_straight().restrict(*(u.get_grid(u.levelmax())), *(u.get_grid(u.levelmax() - 1)));
		for (int i = levelmin_TF; i >= (int)rh.levelmin(); --i)
			mg_straight().restrict(*(u.get_grid(i)), *(u.get_grid(i - 1)));
	}

	for (unsigned i = 1; i <= rh.levelmax(); ++i)
	{
		if (rh.offset(i, 0) != u.get_grid(i)->offset(0) || rh.offset(i, 1) != u.get_grid(i)->offset(1) || rh.offset(i, 2) != u.get_grid(i)->offset(2) 
			|| rh.size(i, 0) != u.get_grid(i)->size(0) || rh.size(i, 1) != u.get_grid(i)->size(1) || rh.size(i, 2) != u.get_grid(i)->size(2))
		{
			u.cut_patch(i, rh.offset_abs(i, 0), rh.offset_abs(i, 1), rh.offset_abs(i, 2),
						rh.size(i, 0), rh.size(i, 1), rh.size(i, 2), !bfourier_coarsening );
		}
	}
	if( !bfourier_coarsening ){
		normalize_levelmin_density( u );
	}
}
