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

#pragma once

#include <iostream>
#include <iomanip>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <math.h>

#include <general.hh>
#include <config_file.hh>
#include <region_generator.hh>

#include <array>
using index_t = ptrdiff_t;
using index3_t = std::array<index_t, 3>;
using vec3_t = std::array<double, 3>;

class refinement_mask
{
protected:
	std::vector<short> mask_;
	size_t nx_, ny_, nz_;

public:
	refinement_mask(void)
			: nx_(0), ny_(0), nz_(0)
	{
	}

	refinement_mask(size_t nx, size_t ny, size_t nz, short value = 0.)
			: nx_(nx), ny_(ny), nz_(nz)
	{
		mask_.assign(nx_ * ny_ * nz_, value);
	}

	refinement_mask(const refinement_mask &r)
	{
		nx_ = r.nx_;
		ny_ = r.ny_;
		nz_ = r.nz_;
		mask_ = r.mask_;
	}

	refinement_mask &operator=(const refinement_mask &r)
	{
		nx_ = r.nx_;
		ny_ = r.ny_;
		nz_ = r.nz_;
		mask_ = r.mask_;

		return *this;
	}

	void init(size_t nx, size_t ny, size_t nz, short value = 0.)
	{
		nx_ = nx;
		ny_ = ny;
		nz_ = nz;
		mask_.assign(nx_ * ny_ * nz_, value);
	}

	const short &operator()(size_t i, size_t j, size_t k) const
	{
		return mask_[(i * ny_ + j) * nz_ + k];
	}

	short &operator()(size_t i, size_t j, size_t k)
	{
		return mask_[(i * ny_ + j) * nz_ + k];
	}

	size_t count_flagged(void)
	{
		size_t count = 0;
		for (size_t i = 0; i < mask_.size(); ++i)
			if (mask_[i])
				++count;
		return count;
	}

	size_t count_notflagged(void)
	{
		size_t count = 0;
		for (size_t i = 0; i < mask_.size(); ++i)
			if (!mask_[i])
				++count;
		return count;
	}
};

//! base class for all things that have rectangular mesh structure
template <typename T>
class Meshvar
{
public:
	typedef T real_t;

	size_t
			m_nx, //!< x-extent of the rectangular mesh
			m_ny, //!< y-extent of the rectangular mesh
			m_nz; //!< z-extent of the rectangular mesh

	int
			m_offx, //!< x-offset of the grid (just as a helper, not used inside the class)
			m_offy, //!< y-offset of the grid (just as a helper, not used inside the class)
			m_offz; //!< z-offset of the grid (just as a helper, not used inside the class)

	real_t *m_pdata; //!< pointer to the dynamic data array

	//! constructor for cubic mesh
	explicit Meshvar(size_t n, int offx, int offy, int offz)
			: m_nx(n), m_ny(n), m_nz(n), m_offx(offx), m_offy(offy), m_offz(offz)
	{
		m_pdata = new real_t[m_nx * m_ny * m_nz];
	}

	//! constructor for rectangular mesh
	Meshvar(size_t nx, size_t ny, size_t nz, int offx, int offy, int offz)
			: m_nx(nx), m_ny(ny), m_nz(nz), m_offx(offx), m_offy(offy), m_offz(offz)
	{
		m_pdata = new real_t[m_nx * m_ny * m_nz];
	}

	//! variant copy constructor with optional copying of the actual data
	Meshvar(const Meshvar<real_t> &m, bool copy_over = true)
	{
		m_nx = m.m_nx;
		m_ny = m.m_ny;
		m_nz = m.m_nz;

		m_offx = m.m_offx;
		m_offy = m.m_offy;
		m_offz = m.m_offz;

		m_pdata = new real_t[m_nx * m_ny * m_nz];

		if (copy_over){
			#pragma omp parallel for
			for (size_t i = 0; i < m_nx * m_ny * m_nz; ++i)
				m_pdata[i] = m.m_pdata[i];
		}
	}

	//! standard copy constructor
	explicit Meshvar(const Meshvar<real_t> &m)
	{
		m_nx = m.m_nx;
		m_ny = m.m_ny;
		m_nz = m.m_nz;

		m_offx = m.m_offx;
		m_offy = m.m_offy;
		m_offz = m.m_offz;

		m_pdata = new real_t[m_nx * m_ny * m_nz];

		#pragma omp parallel for
		for (size_t i = 0; i < m_nx * m_ny * m_nz; ++i)
			m_pdata[i] = m.m_pdata[i];
	}

	//! destructor
	~Meshvar()
	{
		if (m_pdata != NULL)
			delete[] m_pdata;
	}

	//! deallocate the data, but keep the structure
	inline void deallocate(void)
	{
		if (m_pdata != NULL)
			delete[] m_pdata;
		m_pdata = NULL;
	}

	//! get extent of the mesh along a specified dimension (const)
	inline size_t size(unsigned dim) const
	{
		if (dim == 0)
			return m_nx;
		if (dim == 1)
			return m_ny;
		return m_nz;
	}

	//! get offset of the mesh along a specified dimension  (const)
	inline int offset(unsigned dim) const
	{
		if (dim == 0)
			return m_offx;
		if (dim == 1)
			return m_offy;
		return m_offz;
	}

	//! get extent of the mesh along a specified dimension
	inline int &offset(unsigned dim)
	{
		if (dim == 0)
			return m_offx;
		if (dim == 1)
			return m_offy;
		return m_offz;
	}

	//! set all the data to zero values
	void zero(void)
	{
		#pragma omp parallel for
		for (size_t i = 0; i < m_nx * m_ny * m_nz; ++i)
			m_pdata[i] = 0.0;
	}

	//! direct array random acces to the data block
	inline real_t *operator[](const size_t i)
	{
		return &m_pdata[i];
	}

	//! direct array random acces to the data block (const)
	inline const real_t *operator[](const size_t i) const
	{
		return &m_pdata[i];
	}

	//! 3D random access to the data block via index 3-tuples
	inline real_t &operator()(const int ix, const int iy, const int iz)
	{
#ifdef DEBUG
		if (ix < 0 || ix >= (int)m_nx || iy < 0 || iy >= (int)m_ny || iz < 0 || iz >= (int)m_nz)
			music::elog.Print("Array index (%d,%d,%d) out of bounds", ix, iy, iz);
#endif

		return m_pdata[((size_t)ix * m_ny + (size_t)iy) * m_nz + (size_t)iz];
	}

	//! 3D random access to the data block via index 3-tuples (const)
	inline const real_t &operator()(const int ix, const int iy, const int iz) const
	{
#ifdef DEBUG
		if (ix < 0 || ix >= (int)m_nx || iy < 0 || iy >= (int)m_ny || iz < 0 || iz >= (int)m_nz)
			music::elog.Print("Array index (%d,%d,%d) out of bounds", ix, iy, iz);
#endif

		return m_pdata[((size_t)ix * m_ny + (size_t)iy) * m_nz + (size_t)iz];
	}

	//! direct multiplication of the whole data block with a number
	Meshvar<real_t> &operator*=(real_t x)
	{
		#pragma omp parallel for
		for (size_t i = 0; i < m_nx * m_ny * m_nz; ++i)
			m_pdata[i] *= x;
		return *this;
	}

	//! direct addition of a number to the whole data block
	Meshvar<real_t> &operator+=(real_t x)
	{
		#pragma omp parallel for
		for (size_t i = 0; i < m_nx * m_ny * m_nz; ++i)
			m_pdata[i] += x;
		return *this;
	}

	//! direct element-wise division of the whole data block by a number
	Meshvar<real_t> &operator/=(real_t x)
	{
		#pragma omp parallel for
		for (size_t i = 0; i < m_nx * m_ny * m_nz; ++i)
			m_pdata[i] /= x;
		return *this;
	}

	//! direct subtraction of a number from the whole data block
	Meshvar<real_t> &operator-=(real_t x)
	{
		#pragma omp parallel for
		for (size_t i = 0; i < m_nx * m_ny * m_nz; ++i)
			m_pdata[i] -= x;
		return *this;
	}

	//! direct element-wise multiplication with another compatible mesh
	Meshvar<real_t> &operator*=(const Meshvar<real_t> &v)
	{
		if (v.m_nx * v.m_ny * v.m_nz != m_nx * m_ny * m_nz)
		{
			music::elog.Print("Meshvar::operator*= : attempt to operate on incompatible data");
			throw std::runtime_error("Meshvar::operator*= : attempt to operate on incompatible data");
		}
		#pragma omp parallel for
		for (size_t i = 0; i < m_nx * m_ny * m_nz; ++i)
			m_pdata[i] *= v.m_pdata[i];

		return *this;
	}

	//! direct element-wise division with another compatible mesh
	Meshvar<real_t> &operator/=(const Meshvar<real_t> &v)
	{
		if (v.m_nx * v.m_ny * v.m_nz != m_nx * m_ny * m_nz)
		{
			music::elog.Print("Meshvar::operator/= : attempt to operate on incompatible data");
			throw std::runtime_error("Meshvar::operator/= : attempt to operate on incompatible data");
		}

		#pragma omp parallel for
		for (size_t i = 0; i < m_nx * m_ny * m_nz; ++i)
			m_pdata[i] /= v.m_pdata[i];

		return *this;
	}

	//! direct element-wise addition of another compatible mesh
	Meshvar<real_t> &operator+=(const Meshvar<real_t> &v)
	{
		if (v.m_nx * v.m_ny * v.m_nz != m_nx * m_ny * m_nz)
		{
			music::elog.Print("Meshvar::operator+= : attempt to operate on incompatible data");
			throw std::runtime_error("Meshvar::operator+= : attempt to operate on incompatible data");
		}
		#pragma omp parallel for
		for (size_t i = 0; i < m_nx * m_ny * m_nz; ++i)
			m_pdata[i] += v.m_pdata[i];

		return *this;
	}

	//! direct element-wise subtraction of another compatible mesh
	Meshvar<real_t> &operator-=(const Meshvar<real_t> &v)
	{
		if (v.m_nx * v.m_ny * v.m_nz != m_nx * m_ny * m_nz)
		{
			music::elog.Print("Meshvar::operator-= : attempt to operate on incompatible data");
			throw std::runtime_error("Meshvar::operator-= : attempt to operate on incompatible data");
		}
		#pragma omp parallel for
		for (size_t i = 0; i < m_nx * m_ny * m_nz; ++i)
			m_pdata[i] -= v.m_pdata[i];

		return *this;
	}

	//! assignment operator for rectangular meshes
	Meshvar<real_t> &operator=(const Meshvar<real_t> &m)
	{
		m_nx = m.m_nx;
		m_ny = m.m_ny;
		m_nz = m.m_nz;

		m_offx = m.m_offx;
		m_offy = m.m_offy;
		m_offz = m.m_offz;

		if (m_pdata != NULL)
			delete m_pdata;

		m_pdata = new real_t[m_nx * m_ny * m_nz];

		#pragma omp parallel for
		for (size_t i = 0; i < m_nx * m_ny * m_nz; ++i)
			m_pdata[i] = m.m_pdata[i];

		return *this;
	}

	real_t *get_ptr(void)
	{
		return m_pdata;
	}
};

//! MeshvarBnd derived class adding boundary ghost cell functionality
template <typename T>
class MeshvarBnd : public Meshvar<T>
{
	using Meshvar<T>::m_nx;
	using Meshvar<T>::m_ny;
	using Meshvar<T>::m_nz;
	using Meshvar<T>::m_pdata;

public:
	typedef T real_t;

	//! number of boundary (ghost) cells
	int m_nbnd;

	//! most general constructor
	MeshvarBnd(int nbnd, size_t nx, size_t ny, size_t nz, size_t xoff, size_t yoff, size_t zoff)
			: Meshvar<real_t>(nx + 2 * nbnd, ny + 2 * nbnd, nz + 2 * nbnd, xoff, yoff, zoff), m_nbnd(nbnd)
	{
	}

	//! zero-offset constructor
	MeshvarBnd(size_t nbnd, size_t nx, size_t ny, size_t nz)
			: Meshvar<real_t>(nx + 2 * nbnd, ny + 2 * nbnd, nz + 2 * nbnd, 0, 0, 0), m_nbnd(nbnd)
	{
	}

	//! constructor for cubic meshes
	MeshvarBnd(size_t nbnd, size_t n, size_t xoff, size_t yoff, size_t zoff)
			: Meshvar<real_t>(n + 2 * nbnd, xoff, yoff, zoff), m_nbnd(nbnd)
	{
	}

	//! constructor for cubic meshes with zero offset
	MeshvarBnd(size_t nbnd, size_t n)
			: Meshvar<real_t>(n + 2 * nbnd, 0, 0, 0), m_nbnd(nbnd)
	{
	}

	//! modified copy constructor, allows to avoid copying actual data
	MeshvarBnd(const MeshvarBnd<real_t> &v, bool copyover)
			: Meshvar<real_t>(v, copyover), m_nbnd(v.m_nbnd)
	{
	}

	//! copy constructor
	explicit MeshvarBnd(const MeshvarBnd<real_t> &v)
			: Meshvar<real_t>(v, true), m_nbnd(v.m_nbnd)
	{
	}

	//! get extent of the mesh along a specified dimension
	inline size_t size(unsigned dim = 0) const
	{
		if (dim == 0)
			return m_nx - 2 * m_nbnd;
		if (dim == 1)
			return m_ny - 2 * m_nbnd;
		return m_nz - 2 * m_nbnd;
	}

	//! 3D random access to the data block via index 3-tuples
	inline real_t &operator()(const int ix, const int iy, const int iz)
	{
		size_t iix(ix + m_nbnd), iiy(iy + m_nbnd), iiz(iz + m_nbnd);
		return m_pdata[(iix * m_ny + iiy) * m_nz + iiz];
	}

	//! 3D random access to the data block via index 3-tuples (const)
	inline const real_t &operator()(const int ix, const int iy, const int iz) const
	{
		size_t iix(ix + m_nbnd), iiy(iy + m_nbnd), iiz(iz + m_nbnd);
		return m_pdata[(iix * m_ny + iiy) * m_nz + iiz];
	}

	//! assignment operator for rectangular meshes with ghost zones
	MeshvarBnd<real_t> &operator=(const MeshvarBnd<real_t> &m)
	{
		if (this->m_nx != m.m_nx || this->m_ny != m.m_ny || this->m_nz != m.m_nz)
		{
			this->m_nx = m.m_nx;
			this->m_ny = m.m_ny;
			this->m_nz = m.m_nz;

			if (m_pdata != NULL)
				delete[] m_pdata;

			m_pdata = new real_t[m_nx * m_ny * m_nz];
		}

		#pragma omp parallel for
		for (size_t i = 0; i < m_nx * m_ny * m_nz; ++i)
			this->m_pdata[i] = m.m_pdata[i];

		return *this;
	}

	//! outputs the data, for debugging only, not practical for large datasets
	void print(void) const
	{
		int nbnd = m_nbnd;

		std::cout << "size is [" << this->size(0) << ", " << this->size(1) << ", " << this->size(2) << "]\n";
		std::cout << "ghost region has length of " << nbnd << std::endl;

		std::cout.precision(3);
		for (int i = -nbnd; i < (int)this->size(0) + nbnd; ++i)
		{
			std::cout << "ix = " << i << ": \n";

			for (int j = -nbnd; j < (int)this->size(1) + nbnd; ++j)
			{
				for (int k = -nbnd; k < (int)this->size(2) + nbnd; ++k)
				{
					if (i < 0 || i >= this->size(0) || j < 0 || j >= this->size(1) || k < 0 || k >= this->size(2))
						std::cout << "[" << std::setw(6) << (*this)(i, j, k) << "] ";
					else
						std::cout << std::setw(8) << (*this)(i, j, k) << " ";
				}
				std::cout << std::endl;
			}

			std::cout << std::endl;
		}
	}
};

//! class that subsumes a nested grid collection
template <typename T>
class GridHierarchy
{
public:
	//! number of ghost cells on boundary
	size_t m_nbnd;

	//! highest level without adaptive refinement
	unsigned m_levelmin;

	//! vector of pointers to the underlying rectangular mesh data for each level
	std::vector<MeshvarBnd<T> *> m_pgrids;

	std::vector<int>
			m_xoffabs, //!< vector of x-offsets of a level mesh relative to the coarser level
			m_yoffabs, //!< vector of x-offsets of a level mesh relative to the coarser level
			m_zoffabs; //!< vector of x-offsets of a level mesh relative to the coarser level

	std::vector<refinement_mask *> m_ref_masks;
	bool bhave_refmask;

protected:
	//! check whether a given grid has identical hierarchy, dimensions to this
	bool is_consistent(const GridHierarchy<T> &gh)
	{
		if (gh.levelmax() != levelmax())
			return false;

		if (gh.levelmin() != levelmin())
			return false;

		for (unsigned i = levelmin(); i <= levelmax(); ++i)
			for (int j = 0; j < 3; ++j)
			{
				if (size(i, j) != gh.size(i, j))
					return false;
				if (offset(i, j) != gh.offset(i, j))
					return false;
			}

		return true;
	}

public:
	//! return a pointer to the MeshvarBnd object representing data for one level
	MeshvarBnd<T> *get_grid(unsigned ilevel)
	{

		if (ilevel >= m_pgrids.size())
		{
			music::elog.Print("Attempt to access level %d but maxlevel = %d", ilevel, m_pgrids.size() - 1);
			throw std::runtime_error("Fatal: attempt to access non-existent grid");
		}
		return m_pgrids[ilevel];
	}

	//! return a pointer to the MeshvarBnd object representing data for one level (const)
	const MeshvarBnd<T> *get_grid(unsigned ilevel) const
	{
		if (ilevel >= m_pgrids.size())
		{
			music::elog.Print("Attempt to access level %d but maxlevel = %d", ilevel, m_pgrids.size() - 1);
			throw std::runtime_error("Fatal: attempt to access non-existent grid");
		}

		return m_pgrids[ilevel];
	}

	//! constructor for a collection of rectangular grids representing a multi-level hierarchy
	/*! creates an empty hierarchy, levelmin is initially zero, no grids are stored
	 * @param nbnd number of ghost zones added at the boundary
	 */
	explicit GridHierarchy(size_t nbnd)
			: m_nbnd(nbnd), m_levelmin(0), bhave_refmask(false)
	{
		m_pgrids.clear();
	}

	//! copy constructor
	explicit GridHierarchy(const GridHierarchy<T> &gh)
	{
		for (unsigned i = 0; i <= gh.levelmax(); ++i)
			m_pgrids.push_back(new MeshvarBnd<T>(*gh.get_grid(i)));

		m_nbnd = gh.m_nbnd;
		m_levelmin = gh.m_levelmin;

		m_xoffabs = gh.m_xoffabs;
		m_yoffabs = gh.m_yoffabs;
		m_zoffabs = gh.m_zoffabs;

		// ref_mask   = gh.ref_mask;
		bhave_refmask = gh.bhave_refmask;

		if (bhave_refmask)
		{
			for (size_t i = 0; i < gh.m_ref_masks.size(); ++i)
				m_ref_masks.push_back(new refinement_mask(*(gh.m_ref_masks[i])));
		}
	}

	//! destructor
	~GridHierarchy()
	{
		this->deallocate();
	}

	//! free all memory occupied by the grid hierarchy
	void deallocate()
	{
		for (unsigned i = 0; i < m_pgrids.size(); ++i)
			delete m_pgrids[i];
		m_pgrids.clear();
		std::vector<MeshvarBnd<T> *>().swap(m_pgrids);

		m_xoffabs.clear();
		m_yoffabs.clear();
		m_zoffabs.clear();
		m_levelmin = 0;

		for (size_t i = 0; i < m_ref_masks.size(); ++i)
			delete m_ref_masks[i];
		m_ref_masks.clear();
	}

	// meaning of the mask:
	//  -1  =  outside of mask
	//  0.5 =  in mask and refined (i.e. cell exists also on finer level)
	//  1   =  in mask and not refined (i.e. cell exists only on this level)

	void add_refinement_mask(const double *shift)
	{
		bhave_refmask = false;

		//! generate a mask
		if (m_levelmin != levelmax())
		{
			for (int ilevel = (int)levelmax(); ilevel >= (int)levelmin(); --ilevel)
			{
				vec3_t xq;
				double dx = 1.0 / (1ul << ilevel);

				m_ref_masks[ilevel]->init(size(ilevel, 0), size(ilevel, 1), size(ilevel, 2), 0);

				for (size_t i = 0; i < size(ilevel, 0); i += 2)
				{
					xq[0] = (offset_abs(ilevel, 0) + i) * dx + 0.5 * dx + shift[0];
					for (size_t j = 0; j < size(ilevel, 1); j += 2)
					{
						xq[1] = (offset_abs(ilevel, 1) + j) * dx + 0.5 * dx + shift[1];
						for (size_t k = 0; k < size(ilevel, 2); k += 2)
						{
							xq[2] = (offset_abs(ilevel, 2) + k) * dx + 0.5 * dx + shift[2];

							short mask_val = -1; // outside mask
							if (the_region_generator->query_point(xq, ilevel) || ilevel == (int)levelmin())
								mask_val = 1; // inside mask

							(*m_ref_masks[ilevel])(i + 0, j + 0, k + 0) = mask_val;
							(*m_ref_masks[ilevel])(i + 0, j + 0, k + 1) = mask_val;
							(*m_ref_masks[ilevel])(i + 0, j + 1, k + 0) = mask_val;
							(*m_ref_masks[ilevel])(i + 0, j + 1, k + 1) = mask_val;
							(*m_ref_masks[ilevel])(i + 1, j + 0, k + 0) = mask_val;
							(*m_ref_masks[ilevel])(i + 1, j + 0, k + 1) = mask_val;
							(*m_ref_masks[ilevel])(i + 1, j + 1, k + 0) = mask_val;
							(*m_ref_masks[ilevel])(i + 1, j + 1, k + 1) = mask_val;
						}
					}
				}
			}

			bhave_refmask = true;

			for (int ilevel = (int)levelmin(); ilevel < (int)levelmax(); ++ilevel)
			{
				for (size_t i = 0; i < size(ilevel, 0); i++)
					for (size_t j = 0; j < size(ilevel, 1); j++)
						for (size_t k = 0; k < size(ilevel, 2); k++)
						{
							bool fine_is_flagged = false;

							int ifine[] = {
									2 * (int)i - 2 * (int)offset(ilevel + 1, 0),
									2 * (int)j - 2 * (int)offset(ilevel + 1, 1),
									2 * (int)k - 2 * (int)offset(ilevel + 1, 2),
							};

							if (ifine[0] >= 0 && ifine[0] < (int)size(ilevel + 1, 0) &&
									ifine[1] >= 0 && ifine[1] < (int)size(ilevel + 1, 1) &&
									ifine[2] >= 0 && ifine[2] < (int)size(ilevel + 1, 2))
							{
								fine_is_flagged |= (*m_ref_masks[ilevel + 1])(ifine[0] + 0, ifine[1] + 0, ifine[2] + 0) > 0;
								fine_is_flagged |= (*m_ref_masks[ilevel + 1])(ifine[0] + 0, ifine[1] + 0, ifine[2] + 1) > 0;
								fine_is_flagged |= (*m_ref_masks[ilevel + 1])(ifine[0] + 0, ifine[1] + 1, ifine[2] + 0) > 0;
								fine_is_flagged |= (*m_ref_masks[ilevel + 1])(ifine[0] + 0, ifine[1] + 1, ifine[2] + 1) > 0;
								fine_is_flagged |= (*m_ref_masks[ilevel + 1])(ifine[0] + 1, ifine[1] + 0, ifine[2] + 0) > 0;
								fine_is_flagged |= (*m_ref_masks[ilevel + 1])(ifine[0] + 1, ifine[1] + 0, ifine[2] + 1) > 0;
								fine_is_flagged |= (*m_ref_masks[ilevel + 1])(ifine[0] + 1, ifine[1] + 1, ifine[2] + 0) > 0;
								fine_is_flagged |= (*m_ref_masks[ilevel + 1])(ifine[0] + 1, ifine[1] + 1, ifine[2] + 1) > 0;

								if (fine_is_flagged)
								{
									(*m_ref_masks[ilevel])(i, j, k) = 2; // cell is refined

									(*m_ref_masks[ilevel + 1])(ifine[0] + 0, ifine[1] + 0, ifine[2] + 0) = 1;
									(*m_ref_masks[ilevel + 1])(ifine[0] + 0, ifine[1] + 0, ifine[2] + 1) = 1;
									(*m_ref_masks[ilevel + 1])(ifine[0] + 0, ifine[1] + 1, ifine[2] + 0) = 1;
									(*m_ref_masks[ilevel + 1])(ifine[0] + 0, ifine[1] + 1, ifine[2] + 1) = 1;
									(*m_ref_masks[ilevel + 1])(ifine[0] + 1, ifine[1] + 0, ifine[2] + 0) = 1;
									(*m_ref_masks[ilevel + 1])(ifine[0] + 1, ifine[1] + 0, ifine[2] + 1) = 1;
									(*m_ref_masks[ilevel + 1])(ifine[0] + 1, ifine[1] + 1, ifine[2] + 0) = 1;
									(*m_ref_masks[ilevel + 1])(ifine[0] + 1, ifine[1] + 1, ifine[2] + 1) = 1;
								}
							}
						}
			}
		}
	}

	//! get offset of a grid at specified refinement level
	/*! the offset describes the shift of a refinement grid with respect to its coarser parent grid
	 *  @param ilevel the level for which the offset is to be determined
	 *  @param idim the dimension along which the offset is to be determined
	 *  @return integer value denoting the offset in units of coarse grid cells
	 *  @sa offset_abs
	 */
	int offset(int ilevel, int idim) const
	{
		return m_pgrids[ilevel]->offset(idim);
	}

	//! get size of a grid at specified refinement level
	/*! the size describes the number of cells along one dimension of a grid
	 *  @param ilevel the level for which the size is to be determined
	 *  @param idim the dimension along which the size is to be determined
	 *  @return integer value denoting the size of refinement grid at level ilevel along dimension idim
	 */
	size_t size(int ilevel, int idim) const
	{
		return m_pgrids[ilevel]->size(idim);
	}

	//! get the absolute offset of a grid at specified refinement level
	/*! the absolute offset describes the shift of a refinement grid with respect to the simulation volume
	 *  @param ilevel the level for which the offset is to be determined
	 *  @param idim the dimension along which the offset is to be determined
	 *  @return integer value denoting the absolute offset in units of fine grid cells
	 *  @sa offset
	 */
	int offset_abs(int ilevel, int idim) const
	{
		if (idim == 0)
			return m_xoffabs[ilevel];
		if (idim == 1)
			return m_yoffabs[ilevel];
		return m_zoffabs[ilevel];
	}

	//! get the coordinate posisition of a grid cell
	/*! returns the position of a grid cell at specified level relative to the simulation volume
	 *  @param ilevel the refinement level of the grid cell
	 *  @param i the x-index of the cell in the level grid
	 *  @param j the y-index of the cell in the level grid
	 *  @param k the z-index of the cell in the level grid
	 *  @param ppos pointer to a double[3] array to which the coordinates are written
	 *  @return none
	 */
	void cell_pos(unsigned ilevel, int i, int j, int k, double *ppos) const
	{
		double h = 1.0 / (1 << ilevel); //, htop = h*2.0;
		ppos[0] = h * ((double)offset_abs(ilevel, 0) + (double)i + 0.5);
		ppos[1] = h * ((double)offset_abs(ilevel, 1) + (double)j + 0.5);
		ppos[2] = h * ((double)offset_abs(ilevel, 2) + (double)k + 0.5);

		if (ppos[0] >= 1.0 || ppos[1] >= 1.0 || ppos[2] >= 1.0)
			std::cerr << " - Cell seems outside domain! : (" << ppos[0] << ", " << ppos[1] << ", " << ppos[2] << "\n";
	}

	//! get the bounding box of a grid in code units
	/*! returns the bounding box of a grid at specified level in code units
	 *  @param ilevel the refinement level of the grid
	 *  @param left pointer to a double[3] array to which the left corner is written
	 *  @param right pointer to a double[3] array to which the right corner is written
	 *  @return none
	 */
	void grid_bbox(unsigned ilevel, double *left, double *right) const
	{
		double h = 1.0 / (1 << ilevel); //, htop = h*2.0;
		left[0] = h * ((double)offset_abs(ilevel, 0));
		left[1] = h * ((double)offset_abs(ilevel, 1));
		left[2] = h * ((double)offset_abs(ilevel, 2));

		right[0] = left[0] + h * ((double)size(ilevel, 0));
		right[1] = left[1] + h * ((double)size(ilevel, 1));
		right[2] = left[2] + h * ((double)size(ilevel, 2));
	}

	//! checks whether a given grid cell is refined
	/*! a grid cell counts as refined if it is divided into 8 cells at the next higher level
	 *  @param ilevel the refinement level of the grid cell
	 *  @param i the x-index of the cell in the level grid
	 *  @param j the y-index of the cell in the level grid
	 *  @param k the z-index of the cell in the level grid
	 *  @return true if cell is refined, false otherwise
	 */
	bool is_refined(unsigned ilevel, int i, int j, int k) const
	{
		// meaning of the mask:
		//  -1  =  outside of mask
		//  2 =  in mask and refined (i.e. cell exists also on finer level)
		//  1   =  in mask and not refined (i.e. cell exists only on this level)

		if (bhave_refmask)
		{
			return (*m_ref_masks[ilevel])(i, j, k) == 2;
		}

		if (!bhave_refmask && ilevel == levelmax())
			return false;

		if (i < offset(ilevel + 1, 0) || i >= offset(ilevel + 1, 0) + (int)size(ilevel + 1, 0) / 2 ||
				j < offset(ilevel + 1, 1) || j >= offset(ilevel + 1, 1) + (int)size(ilevel + 1, 1) / 2 ||
				k < offset(ilevel + 1, 2) || k >= offset(ilevel + 1, 2) + (int)size(ilevel + 1, 2) / 2)
			return false;

		return true;
	}

	bool is_in_mask(unsigned ilevel, int i, int j, int k) const
	{
		// meaning of the mask:
		//  -1  =  outside of mask
		//  2 =  in mask and refined (i.e. cell exists also on finer level)
		//  1   =  in mask and not refined (i.e. cell exists only on this level)

		if (bhave_refmask)
		{
			return ((*m_ref_masks[ilevel])(i, j, k) >= 0);
		}

		return true;
	}

	//! sets the values of all grids on all levels to zero
	void zero(void)
	{
		for (unsigned i = 0; i < m_pgrids.size(); ++i)
			m_pgrids[i]->zero();
	}

	//! count the number of cells that are not further refined (=leafs)
	/*! for allocation purposes it is useful to query the number of cells to be expected
	 *  @param lmin the minimum refinement level to consider
	 *  @param lmax the maximum refinement level to consider
	 *  @return the integer number of cells between lmin and lmax that are not further refined
	 */
	size_t count_leaf_cells(unsigned lmin, unsigned lmax) const
	{
		size_t npcount = 0;

		for (int ilevel = lmax; ilevel >= (int)lmin; --ilevel)
			for (unsigned i = 0; i < get_grid(ilevel)->size(0); ++i)
				for (unsigned j = 0; j < get_grid(ilevel)->size(1); ++j)
					for (unsigned k = 0; k < get_grid(ilevel)->size(2); ++k)
						if (is_in_mask(ilevel, i, j, k) && !is_refined(ilevel, i, j, k))
							++npcount;

		return npcount;
	}

	//! count the number of cells that are not further refined (=leafs)
	/*! for allocation purposes it is useful to query the number of cells to be expected
	 *  @return the integer number of cells in the hierarchy that are not further refined
	 */
	size_t count_leaf_cells(void) const
	{
		return count_leaf_cells(levelmin(), levelmax());
	}

	//! creates a hierarchy of coextensive grids, refined by factors of 2
	/*! creates a hierarchy of lmax grids, each extending over the whole simulation volume with
	 *  grid length 2^n for level 0<=n<=lmax
	 *  @param lmax the maximum refinement level to be added (sets the resolution to 2^lmax for each dim)
	 *  @return none
	 */
	void create_base_hierarchy(unsigned lmax)
	{
		size_t n = 1;

		this->deallocate();

		m_pgrids.clear();

		m_xoffabs.clear();
		m_yoffabs.clear();
		m_zoffabs.clear();

		for (unsigned i = 0; i <= lmax; ++i)
		{
			// std::cout << "....adding level " << i << " (" << n << ", " << n << ", " << n << ")" << std::endl;
			m_pgrids.push_back(new MeshvarBnd<T>(m_nbnd, n, n, n, 0, 0, 0));
			m_pgrids[i]->zero();
			n *= 2;

			m_xoffabs.push_back(0);
			m_yoffabs.push_back(0);
			m_zoffabs.push_back(0);
		}

		m_levelmin = lmax;

		for (unsigned i = 0; i <= lmax; ++i)
			m_ref_masks.push_back(new refinement_mask(size(i, 0), size(i, 1), size(i, 2), (short)(i != lmax)));
	}

	//! multiply entire grid hierarchy by a constant
	GridHierarchy<T> &operator*=(T x)
	{
		for (unsigned i = 0; i < m_pgrids.size(); ++i)
			(*m_pgrids[i]) *= x;
		return *this;
	}

	//! divide entire grid hierarchy by a constant
	GridHierarchy<T> &operator/=(T x)
	{
		for (unsigned i = 0; i < m_pgrids.size(); ++i)
			(*m_pgrids[i]) /= x;
		return *this;
	}

	//! add a constant to the entire grid hierarchy
	GridHierarchy<T> &operator+=(T x)
	{
		for (unsigned i = 0; i < m_pgrids.size(); ++i)
			(*m_pgrids[i]) += x;
		return *this;
	}

	//! subtract a constant from the entire grid hierarchy
	GridHierarchy<T> &operator-=(T x)
	{
		for (unsigned i = 0; i < m_pgrids.size(); ++i)
			(*m_pgrids[i]) -= x;
		return *this;
	}

	//! multiply (element-wise) two grid hierarchies
	GridHierarchy<T> &operator*=(const GridHierarchy<T> &gh)
	{
		if (!is_consistent(gh))
		{
			music::elog.Print("GridHierarchy::operator*= : attempt to operate on incompatible data");
			throw std::runtime_error("GridHierarchy::operator*= : attempt to operate on incompatible data");
		}
		for (unsigned i = 0; i < m_pgrids.size(); ++i)
			(*m_pgrids[i]) *= *gh.get_grid(i);
		return *this;
	}

	//! divide (element-wise) two grid hierarchies
	GridHierarchy<T> &operator/=(const GridHierarchy<T> &gh)
	{
		if (!is_consistent(gh))
		{
			music::elog.Print("GridHierarchy::operator/= : attempt to operate on incompatible data");
			throw std::runtime_error("GridHierarchy::operator/= : attempt to operate on incompatible data");
		}
		for (unsigned i = 0; i < m_pgrids.size(); ++i)
			(*m_pgrids[i]) /= *gh.get_grid(i);
		return *this;
	}

	//! add (element-wise) two grid hierarchies
	GridHierarchy<T> &operator+=(const GridHierarchy<T> &gh)
	{
		if (!is_consistent(gh))
			throw std::runtime_error("GridHierarchy::operator+= : attempt to operate on incompatible data");

		for (unsigned i = 0; i < m_pgrids.size(); ++i)
			(*m_pgrids[i]) += *gh.get_grid(i);
		return *this;
	}

	//! subtract (element-wise) two grid hierarchies
	GridHierarchy<T> &operator-=(const GridHierarchy<T> &gh)
	{
		if (!is_consistent(gh))
		{
			music::elog.Print("GridHierarchy::operator-= : attempt to operate on incompatible data");
			throw std::runtime_error("GridHierarchy::operator-= : attempt to operate on incompatible data");
		}
		for (unsigned i = 0; i < m_pgrids.size(); ++i)
			(*m_pgrids[i]) -= *gh.get_grid(i);
		return *this;
	}

	//! assign (element-wise) two grid hierarchies
	GridHierarchy<T> &operator=(const GridHierarchy<T> &gh)
	{
		bhave_refmask = gh.bhave_refmask;

		if (bhave_refmask)
		{
			for (unsigned i = 0; i <= gh.levelmax(); ++i)
				m_ref_masks.push_back(new refinement_mask(*(gh.m_ref_masks[i])));
		}

		if (!is_consistent(gh))
		{
			for (unsigned i = 0; i < m_pgrids.size(); ++i)
				delete m_pgrids[i];
			m_pgrids.clear();

			for (unsigned i = 0; i <= gh.levelmax(); ++i)
				m_pgrids.push_back(new MeshvarBnd<T>(*gh.get_grid(i)));
			m_levelmin = gh.levelmin();
			m_nbnd = gh.m_nbnd;

			m_xoffabs = gh.m_xoffabs;
			m_yoffabs = gh.m_yoffabs;
			m_zoffabs = gh.m_zoffabs;

			return *this;
		} // throw std::runtime_error("GridHierarchy::operator= : attempt to operate on incompatible data");

		for (unsigned i = 0; i < m_pgrids.size(); ++i)
			(*m_pgrids[i]) = *gh.get_grid(i);
		return *this;
	}

	/*
	//! assignment operator
	GridHierarchy& operator=( const GridHierarchy<T>& gh )
	{
		for( unsigned i=0; i<m_pgrids.size(); ++i )
			delete m_pgrids[i];
		m_pgrids.clear();

		for( unsigned i=0; i<=gh.levelmax(); ++i )
			m_pgrids.push_back( new MeshvarBnd<T>( *gh.get_grid(i) ) );
		m_levelmin = gh.levelmin();
		m_nbnd = gh.m_nbnd;

		m_xoffabs = gh.m_xoffabs;
		m_yoffabs = gh.m_yoffabs;
		m_zoffabs = gh.m_zoffabs;


		return *this;
	}
	*/

	/*! add a new refinement patch to the so-far finest level
	 * @param xoff x-offset in units of the coarser grid (finest grid before adding new patch)
	 * @param yoff y-offset in units of the coarser grid (finest grid before adding new patch)
	 * @param zoff z-offset in units of the coarser grid (finest grid before adding new patch)
	 * @param nx x-extent in fine grid cells
	 * @param ny y-extent in fine grid cells
	 * @param nz z-extent in fine grid cells
	 */
	void add_patch(unsigned xoff, unsigned yoff, unsigned zoff, unsigned nx, unsigned ny, unsigned nz)
	{
		m_pgrids.push_back(new MeshvarBnd<T>(m_nbnd, nx, ny, nz, xoff, yoff, zoff));
		m_pgrids.back()->zero();

		//.. add absolute offsets (in units of current level grid cells)
		m_xoffabs.push_back(2 * (m_xoffabs.back() + xoff));
		m_yoffabs.push_back(2 * (m_yoffabs.back() + yoff));
		m_zoffabs.push_back(2 * (m_zoffabs.back() + zoff));

		m_ref_masks.push_back(new refinement_mask(nx, ny, nz, 0));
	}

	/*! cut a refinement patch to the specified size
	 * @param ilevel grid level on which to perform the size adjustment
	 * @param xoff new x-offset in units of the coarser grid (finest grid before adding new patch)
	 * @param yoff new y-offset in units of the coarser grid (finest grid before adding new patch)
	 * @param zoff new z-offset in units of the coarser grid (finest grid before adding new patch)
	 * @param nx new x-extent in fine grid cells
	 * @param ny new y-extent in fine grid cells
	 * @param nz new z-extent in fine grid cells
	 * @param enforce_coarse_mean enforces the average of 8 cells on a fine level to equal the coarse 
	 */
	void cut_patch(unsigned ilevel, unsigned xoff, unsigned yoff, unsigned zoff, unsigned nx, unsigned ny, unsigned nz, bool enforce_coarse_mean)
	{
		unsigned dx, dy, dz, dxtop, dytop, dztop;

		dx = xoff - m_xoffabs[ilevel];
		dy = yoff - m_yoffabs[ilevel];
		dz = zoff - m_zoffabs[ilevel];

		assert(dx % 2 == 0 && dy % 2 == 0 && dz % 2 == 0);

		dxtop = m_pgrids[ilevel]->offset(0) + dx / 2;
		dytop = m_pgrids[ilevel]->offset(1) + dy / 2;
		dztop = m_pgrids[ilevel]->offset(2) + dz / 2;

		MeshvarBnd<T> *mnew = new MeshvarBnd<T>(m_nbnd, nx, ny, nz, dxtop, dytop, dztop);

		//... copy data
		[[maybe_unused]] double coarsesum = 0.0, finesum = 0.0;
    [[maybe_unused]] size_t coarsecount = 0, finecount = 0;

    //... copy data
		#pragma omp parallel for reduction(+:finesum,finecount) collapse(3)
    for( unsigned i=0; i<nx; ++i )
      for( unsigned j=0; j<ny; ++j )
				for( unsigned k=0; k<nz; ++k )
				{
					(*mnew)(i,j,k) = (*m_pgrids[ilevel])(i+dx,j+dy,k+dz);
					finesum += (*mnew)(i,j,k);
					finecount++;
				}

		//... replace in hierarchy
		delete m_pgrids[ilevel];
		m_pgrids[ilevel] = mnew;

		//... update offsets
		m_xoffabs[ilevel] += dx;
		m_yoffabs[ilevel] += dy;
		m_zoffabs[ilevel] += dz;

		if (ilevel < levelmax())
		{
			m_pgrids[ilevel + 1]->offset(0) -= dx;
			m_pgrids[ilevel + 1]->offset(1) -= dy;
			m_pgrids[ilevel + 1]->offset(2) -= dz;
		}

		if( enforce_coarse_mean )
		{
			//... enforce top mean density over same patch
			if (ilevel > levelmin())
			{
				int ox = m_pgrids[ilevel]->offset(0);
				int oy = m_pgrids[ilevel]->offset(1);
				int oz = m_pgrids[ilevel]->offset(2);

				#pragma omp parallel for reduction(+:coarsesum,coarsecount) collapse(3)
				for (unsigned i = 0; i < nx / 2; ++i)
					for (unsigned j = 0; j < ny / 2; ++j)
						for (unsigned k = 0; k < nz / 2; ++k)
						{
							coarsesum += (*m_pgrids[ilevel - 1])(i + ox, j + oy, k + oz);
							coarsecount++;
						}

				coarsesum /= (double)coarsecount;
				finesum /= (double)finecount;

				#pragma omp parallel for collapse(3)
				for (unsigned i = 0; i < nx; ++i)
					for (unsigned j = 0; j < ny; ++j)
						for (unsigned k = 0; k < nz; ++k)
							(*mnew)(i, j, k) += (coarsesum - finesum);

				music::ilog.Print("  .level %d : corrected patch overlap mean value by %f", ilevel, coarsesum - finesum);
			}
		}else{
			//... enforce fine mean density over same patch
			if (ilevel > levelmin())
			{
				int ox = m_pgrids[ilevel]->offset(0);
				int oy = m_pgrids[ilevel]->offset(1);
				int oz = m_pgrids[ilevel]->offset(2);

				#pragma omp parallel for reduction(+:coarsesum,coarsecount) collapse(3)
				for (unsigned i = 0; i < nx / 2; ++i)
					for (unsigned j = 0; j < ny / 2; ++j)
						for (unsigned k = 0; k < nz / 2; ++k)
						{
							coarsesum += (*m_pgrids[ilevel - 1])(i + ox, j + oy, k + oz);
							coarsecount++;
						}

				coarsesum /= (double)coarsecount;
				finesum /= (double)finecount;

				#pragma omp parallel for collapse(3)
				for (unsigned i = 0; i < nx / 2; ++i)
					for (unsigned j = 0; j < ny / 2; ++j)
						for (unsigned k = 0; k < nz / 2; ++k)
							(*m_pgrids[ilevel - 1])(i + ox, j + oy, k + oz) -= (coarsesum - finesum);

				music::ilog.Print("  .level %d : corrected patch overlap mean value by %f", ilevel, coarsesum - finesum);
			}
		}

		find_new_levelmin();
	}

	/*! determine level for which grid extends over entire domain */
	void find_new_levelmin(void)
	{
		for (unsigned i = 0; i <= levelmax(); ++i)
		{
			unsigned n = 1 << i;
			if (m_pgrids[i]->size(0) == n &&
					m_pgrids[i]->size(1) == n &&
					m_pgrids[i]->size(2) == n)
			{
				m_levelmin = i;
			}
		}
	}

	//! return maximum level in refinement hierarchy
	unsigned levelmax(void) const
	{
		return m_pgrids.size() - 1;
	}

	//! return minimum level in refinement hierarchy (the level which extends over the entire domain)
	unsigned levelmin(void) const
	{
		return m_levelmin;
	}
};

//! class that computes the refinement structure given parameters
class refinement_hierarchy
{
	std::vector<double>
			x0_, //!< x-coordinates of grid origins (in [0..1[)
			y0_, //!< y-coordinates of grid origins (in [0..1[)
			z0_, //!< z-coordinates of grid origins (in [0..1[)
			xl_, //!< x-extent of grids (in [0..1[)
			yl_, //!< y-extent of grids (in [0..1[)
			zl_; //!< z-extent of grids (in [0..1[)

	std::vector<index3_t> offsets_;
	std::vector<index3_t> absoffsets_;
	std::vector<index3_t> len_;

	unsigned
			levelmin_,				//!< minimum grid level for Poisson solver
			levelmax_,				//!< maximum grid level for all operations
			levelmin_tf_,			//!< minimum grid level for density calculation
			padding_,					//!< padding in number of coarse cells between refinement levels
			blocking_factor_, //!< blocking factor of grids, necessary fo BoxLib codes such as NyX
			gridding_unit_;		//!< internal blocking factor of grids, necessary for Panphasia

	int margin_; //!< number of cells used for additional padding for convolutions with isolated boundaries (-1 = double padding)

	config_file &cf_; //!< reference to config_file

	bool align_top_,		//!< bool whether to align all grids with coarsest grid cells
			preserve_dims_, //!< bool whether to preserve user-specified grid dimensions
			equal_extent_;	//!< bool whether the simulation code requires squared refinement regions (e.g. RAMSES)

	vec3_t x0ref_; //!< coordinates of refinement region origin (in [0..1[)
	vec3_t lxref_; //!< extent of refinement region (int [0..1[)

	index3_t lnref_;
	bool bhave_nref;

	index3_t xshift_; //!< shift of refinement region in coarse cells (in order to center it in the domain)
	double rshift_[3];

	//! calculates the greatest common divisor
	int gcd(int a, int b) const
	{
		return b == 0 ? a : gcd(b, a % b);
	}

	// calculates the cell shift in units of levelmin grid cells if there is an additional constraint to be
	// congruent with another grid that partitions the same space in multiples of "base_unit"
	int get_shift_unit(int base_unit, int levelmin) const
	{
		/*int Lp = 0;
		while( base_unit * (1<<Lp) < 1<<(levelmin+1) ){
			++Lp;
		}
		int U = base_unit * (1<<Lp);

		return std::max<int>( 1, (1<<(levelmin+1)) / (2*gcd(U,1<<(levelmin+1) )) );*/

		int level_m = 0;
		while (base_unit * (1 << level_m) < (1 << levelmin))
			++level_m;

		return std::max<int>(1, (1 << levelmin) / gcd(base_unit * (1 << level_m), (1 << levelmin)));
	}

public:
	//! copy constructor
	refinement_hierarchy(const refinement_hierarchy &rh)
			: cf_(rh.cf_)
	{
		*this = rh;
	}

	//! constructor from a config_file holding information about the desired refinement geometry
	explicit refinement_hierarchy(config_file &cf)
			: cf_(cf)
	{
		//... query the parameter data we need
		levelmin_ = cf_.get_value<unsigned>("setup", "levelmin");
		levelmax_ = cf_.get_value<unsigned>("setup", "levelmax");
		levelmin_tf_ = cf_.get_value_safe<unsigned>("setup", "levelmin_TF", levelmin_);
		align_top_ = cf_.get_value_safe<bool>("setup", "align_top", false);
		preserve_dims_ = cf_.get_value_safe<bool>("setup", "preserve_dims", false);
		equal_extent_ = cf_.get_value_safe<bool>("setup", "force_equal_extent", false);
		blocking_factor_ = cf.get_value_safe<unsigned>("setup", "blocking_factor", 0);
		margin_ = cf.get_value_safe<int>("setup", "convolution_margin", 4);

		bool bnoshift = cf_.get_value_safe<bool>("setup", "no_shift", false);
		bool force_shift = cf_.get_value_safe<bool>("setup", "force_shift", false);

		gridding_unit_ = cf.get_value_safe<unsigned>("setup", "gridding_unit", 2);

		if (gridding_unit_ != 2 && blocking_factor_ == 0)
		{
			blocking_factor_ = gridding_unit_; // THIS WILL LIKELY CAUSE PROBLEMS WITH NYX
		}
		else if (gridding_unit_ != 2 && blocking_factor_ != 0 && gridding_unit_ != blocking_factor_)
		{
			music::elog.Print("incompatible gridding unit %d and blocking factor specified", gridding_unit_, blocking_factor_);
			throw std::runtime_error("Incompatible gridding unit and blocking factor!");
		}

		//... call the region generator
		if (levelmin_ != levelmax_)
		{

			vec3_t x1ref;
			the_region_generator->get_AABB(x0ref_, x1ref, levelmax_);
			for (int i = 0; i < 3; ++i)
				lxref_[i] = x1ref[i] - x0ref_[i];
			bhave_nref = false;

			std::string region_type = cf.get_value_safe<std::string>("setup", "region", "box");

			music::ilog << "    refinement region is \'" << region_type.c_str() << "\', w/ bounding box" << std::endl;
			music::ilog << "            left = [" << x0ref_[0] << "," << x0ref_[1] << "," << x0ref_[2] << "]" << std::endl;
			music::ilog << "           right = [" << x1ref[0] << "," << x1ref[1] << "," << x1ref[2] << "]" << std::endl;

			bhave_nref = the_region_generator->is_grid_dim_forced(lnref_);
		}

		// if not doing any refinement levels, set extent to full box
		if (levelmin_ == levelmax_)
		{
			x0ref_ = {0.0, 0.0, 0.0};
			lxref_ = {1.0, 1.0, 1.0};
		}

		unsigned ncoarse = 1 << levelmin_;

		//... determine shift

		double xc[3];
		xc[0] = fmod(x0ref_[0] + 0.5 * lxref_[0], 1.0);
		xc[1] = fmod(x0ref_[1] + 0.5 * lxref_[1], 1.0);
		xc[2] = fmod(x0ref_[2] + 0.5 * lxref_[2], 1.0);

		if ((levelmin_ != levelmax_) && (!bnoshift || force_shift))
		{
			int random_base_grid_unit = cf.get_value_safe<int>("random", "base_unit", 1);
			int shift_unit = get_shift_unit(random_base_grid_unit, levelmin_);
			if (shift_unit != 1)
			{
				music::ilog.Print("volume can only be shifted by multiples of %d coarse cells.", shift_unit);
			}
			xshift_[0] = (int)((0.5 - xc[0]) * (double)ncoarse / shift_unit + 0.5) * shift_unit; // ARJ(int)((0.5 - xc[0]) * ncoarse);
			xshift_[1] = (int)((0.5 - xc[1]) * (double)ncoarse / shift_unit + 0.5) * shift_unit; // ARJ(int)((0.5 - xc[1]) * ncoarse);
			xshift_[2] = (int)((0.5 - xc[2]) * (double)ncoarse / shift_unit + 0.5) * shift_unit; // ARJ(int)((0.5 - xc[2]) * ncoarse);

			// xshift_[0] = (int)((0.5 - xc[0]) * ncoarse);
			// xshift_[1] = (int)((0.5 - xc[1]) * ncoarse);
			// xshift_[2] = (int)((0.5 - xc[2]) * ncoarse);
		}
		else
		{
			xshift_[0] = 0;
			xshift_[1] = 0;
			xshift_[2] = 0;
		}

		char strtmp[32];
		snprintf(strtmp, 32, "%ld", xshift_[0]);
		cf_.insert_value("setup", "shift_x", strtmp);
		snprintf(strtmp, 32, "%ld", xshift_[1]);
		cf_.insert_value("setup", "shift_y", strtmp);
		snprintf(strtmp, 32, "%ld", xshift_[2]);
		cf_.insert_value("setup", "shift_z", strtmp);

		rshift_[0] = -(double)xshift_[0] / ncoarse;
		rshift_[1] = -(double)xshift_[1] / ncoarse;
		rshift_[2] = -(double)xshift_[2] / ncoarse;

		x0ref_[0] += (double)xshift_[0] / ncoarse;
		x0ref_[1] += (double)xshift_[1] / ncoarse;
		x0ref_[2] += (double)xshift_[2] / ncoarse;

		//... initialize arrays
		x0_.assign(levelmax_ + 1, 0.0);
		xl_.assign(levelmax_ + 1, 1.0);
		y0_.assign(levelmax_ + 1, 0.0);
		yl_.assign(levelmax_ + 1, 1.0);
		z0_.assign(levelmax_ + 1, 0.0);
		zl_.assign(levelmax_ + 1, 1.0);

		offsets_.assign(levelmax_ + 1, {0, 0, 0});
		absoffsets_.assign(levelmax_ + 1, {0, 0, 0});
		len_.assign(levelmax_ + 1, {0, 0, 0});

		len_[levelmin_] = {ncoarse, ncoarse, ncoarse};

		// set up base hierarchy sizes
		for (unsigned ilevel = 0; ilevel <= levelmin_; ++ilevel)
		{
			unsigned n = 1 << ilevel;

			xl_[ilevel] = yl_[ilevel] = zl_[ilevel] = 1.0;
			len_[ilevel] = {n, n, n};
			// nx_[ilevel] = ny_[ilevel] = nz_[ilevel] = n;
		}

		// if no refinement, we can exit here
		if (levelmax_ == levelmin_)
			return;

		//... determine the position of the refinement region on the finest grid
		int il, jl, kl, ir, jr, kr;
		int nresmax = 1 << levelmax_;

		il = (int)(x0ref_[0] * nresmax);
		jl = (int)(x0ref_[1] * nresmax);
		kl = (int)(x0ref_[2] * nresmax);
		ir = (int)((x0ref_[0] + lxref_[0]) * nresmax); //+ 1.0);
		jr = (int)((x0ref_[1] + lxref_[1]) * nresmax); //+ 1.0);
		kr = (int)((x0ref_[2] + lxref_[2]) * nresmax); //+ 1.0);

		//... align with coarser grids ...
		if (align_top_)
		{
			//... require alignment with top grid
			unsigned nref = 1 << (levelmax_ - levelmin_ + 1);

			if (bhave_nref)
			{
				if (lnref_[0] % (1ul << (levelmax_ - levelmin_)) != 0 ||
						lnref_[1] % (1ul << (levelmax_ - levelmin_)) != 0 ||
						lnref_[2] % (1ul << (levelmax_ - levelmin_)) != 0)
				{
					music::elog.Print("specified ref_dims and align_top=yes but cannot be aligned with coarse grid!");
					throw std::runtime_error("specified ref_dims and align_top=yes but cannot be aligned with coarse grid!");
				}
			}

			il = (int)((double)il / nref) * nref;
			jl = (int)((double)jl / nref) * nref;
			kl = (int)((double)kl / nref) * nref;

			int irr = (int)((double)ir / nref) * nref;
			int jrr = (int)((double)jr / nref) * nref;
			int krr = (int)((double)kr / nref) * nref;

			if (irr < ir)
				ir = (int)((double)ir / nref + 1.0) * nref;
			else
				ir = irr;

			if (jrr < jr)
				jr = (int)((double)jr / nref + 1.0) * nref;
			else
				jr = jrr;

			if (krr < kr)
				kr = (int)((double)kr / nref + 1.0) * nref;
			else
				kr = krr;
		}
		else if (preserve_dims_)
		{
			//... require alignment with coarser grid
			int alx = (xshift_[0] >= 0) - (xshift_[0] < 0);
			int aly = (xshift_[1] >= 0) - (xshift_[1] < 0);
			int alz = (xshift_[2] >= 0) - (xshift_[2] < 0);
			il += alx * (il % 2);
			jl += aly * (jl % 2);
			kl += alz * (kl % 2);
			ir += alx * (ir % 2);
			jr += aly * (jr % 2);
			kr += alz * (kr % 2);
		}
		else
		{
			//... require alignment with coarser grid
			music::ilog.Print("- Internal refinement bounding box: [%d,%d]x[%d,%d]x[%d,%d]", il, ir, jl, jr, kl, kr);

			il -= il % gridding_unit_;
			jl -= jl % gridding_unit_;
			kl -= kl % gridding_unit_;

			ir = ((ir % gridding_unit_) != 0) ? (ir / gridding_unit_ + 1) * gridding_unit_ : ir;
			jr = ((jr % gridding_unit_) != 0) ? (jr / gridding_unit_ + 1) * gridding_unit_ : jr;
			kr = ((kr % gridding_unit_) != 0) ? (kr / gridding_unit_ + 1) * gridding_unit_ : kr;
		}

		// require alighment with coarser block
		if (blocking_factor_)
		{
			unsigned coarse_block = 2 * blocking_factor_;
			il -= il % coarse_block;
			jl -= jl % coarse_block;
			kl -= kl % coarse_block;
			ir += (nresmax - ir) % coarse_block;
			jr += (nresmax - jr) % coarse_block;
			kr += (nresmax - kr) % coarse_block;
		}

		// if doing unigrid, set region to whole box
		if (levelmin_ == levelmax_)
		{
			il = jl = kl = 0;
			ir = jr = kr = nresmax - 1;
		}
		if (bhave_nref)
		{
			ir = il + lnref_[0];
			jr = jl + lnref_[1];
			kr = kl + lnref_[2];
		}

		//... make sure bounding box lies in domain
		il = (il + nresmax) % nresmax;
		ir = (ir + nresmax) % nresmax;
		jl = (jl + nresmax) % nresmax;
		jr = (jr + nresmax) % nresmax;
		kl = (kl + nresmax) % nresmax;
		kr = (kr + nresmax) % nresmax;

		if (il >= ir || jl >= jr || kl >= kr)
		{
			music::elog.Print("Internal refinement bounding box error: [%d,%d]x[%d,%d]x[%d,%d]", il, ir, jl, jr, kl, kr);
			throw std::runtime_error("refinement_hierarchy: Internal refinement bounding box error 1");
		}
		//... determine offsets
		if (levelmin_ != levelmax_)
		{

			absoffsets_[levelmax_] = {(il + nresmax) % nresmax, (jl + nresmax) % nresmax, (kl + nresmax) % nresmax};
			len_[levelmax_] = {ir - il, jr - jl, kr - kl};

			if (equal_extent_)
			{

				if (bhave_nref && (lnref_[0] != lnref_[1] || lnref_[0] != lnref_[2]))
				{
					music::elog.Print("Specified equal_extent=yes conflicting with ref_dims which are not equal.");
					throw std::runtime_error("Specified equal_extent=yes conflicting with ref_dims which are not equal.");
				}
				size_t ilevel = levelmax_;
				index_t nmax = *std::max_element(len_[ilevel].begin(), len_[ilevel].end());

				index3_t dx = {0, 0, 0};
				for (int idim = 0; idim < 3; ++idim)
				{
					dx[idim] = (index_t)((double)(nmax - len_[ilevel][idim]) * 0.5);
					absoffsets_[ilevel][idim] -= dx[idim];
					len_[ilevel][idim] = nmax;
				}

				il = absoffsets_[ilevel][0];
				jl = absoffsets_[ilevel][1];
				kl = absoffsets_[ilevel][2];
				ir = il + nmax;
				jr = jl + nmax;
				kr = kl + nmax;
			}
		}

		padding_ = cf_.get_value_safe<unsigned>("setup", "padding", 8);

		//... determine position of coarser grids
		for (unsigned ilevel = levelmax_ - 1; ilevel > levelmin_; --ilevel)
		{
			il = (int)((double)il * 0.5 - padding_);
			jl = (int)((double)jl * 0.5 - padding_);
			kl = (int)((double)kl * 0.5 - padding_);

			ir = (int)((double)ir * 0.5 + padding_);
			jr = (int)((double)jr * 0.5 + padding_);
			kr = (int)((double)kr * 0.5 + padding_);

			//... align with coarser grids ...
			if (align_top_)
			{
				//... require alignment with top grid
				unsigned nref = 1 << (ilevel - levelmin_);

				il = (int)((double)il / nref) * nref;
				jl = (int)((double)jl / nref) * nref;
				kl = (int)((double)kl / nref) * nref;

				ir = (int)((double)ir / nref + 1.0) * nref;
				jr = (int)((double)jr / nref + 1.0) * nref;
				kr = (int)((double)kr / nref + 1.0) * nref;
			}
			else if (preserve_dims_)
			{
				//... require alignment with coarser grid
				int alx = (xshift_[0] >= 0) - (xshift_[0] < 0);
				int aly = (xshift_[1] >= 0) - (xshift_[1] < 0);
				int alz = (xshift_[2] >= 0) - (xshift_[2] < 0);
				il += alx * (il % 2);
				jl += aly * (jl % 2);
				kl += alz * (kl % 2);
				ir += alx * (ir % 2);
				jr += aly * (jr % 2);
				kr += alz * (kr % 2);
			}
			else
			{
				//... require alignment with coarser grid
				il -= il % gridding_unit_;
				jl -= jl % gridding_unit_;
				kl -= kl % gridding_unit_;

				ir = ((ir % gridding_unit_) != 0) ? (ir / gridding_unit_ + 1) * gridding_unit_ : ir;
				jr = ((jr % gridding_unit_) != 0) ? (jr / gridding_unit_ + 1) * gridding_unit_ : jr;
				kr = ((kr % gridding_unit_) != 0) ? (kr / gridding_unit_ + 1) * gridding_unit_ : kr;
			}

			// require alighment with coarser block
			if (blocking_factor_)
			{
				unsigned coarse_block = 2 * blocking_factor_;
				int nres = 1 << ilevel;
				il -= il % coarse_block;
				jl -= jl % coarse_block;
				kl -= kl % coarse_block;
				ir += (nres - ir) % coarse_block;
				jr += (nres - jr) % coarse_block;
				kr += (nres - kr) % coarse_block;
			}

			if (il >= ir || jl >= jr || kl >= kr || il < 0 || jl < 0 || kl < 0)
			{
				music::elog.Print("Internal refinement bounding box error: [%d,%d]x[%d,%d]x[%d,%d], level=%d", il, ir, jl, jr, kl, kr, ilevel);
				throw std::runtime_error("refinement_hierarchy: Internal refinement bounding box error 2");
			}
			absoffsets_[ilevel] = {il, jl, kl};
			len_[ilevel] = {ir - il, jr - jl, kr - kl};

			if (blocking_factor_)
			{
				for (int idim = 0; idim < 3; ++idim)
					len_[ilevel][idim] += len_[ilevel][idim] % blocking_factor_;
			}

			if (equal_extent_)
			{
				index_t nmax = *std::max_element(len_[ilevel].begin(), len_[ilevel].end());
				for (int idim = 0; idim < 3; ++idim)
				{
					index_t dx = (int)((double)(nmax - len_[ilevel][idim]) * 0.5);
					absoffsets_[ilevel][idim] -= dx;
					len_[ilevel][idim] = nmax;
				}

				il = absoffsets_[ilevel][0];
				jl = absoffsets_[ilevel][1];
				kl = absoffsets_[ilevel][2];
				ir = il + nmax;
				jr = jl + nmax;
				kr = kl + nmax;
			}
		}

		//... determine relative offsets between grids
		for (unsigned ilevel = levelmax_; ilevel > levelmin_; --ilevel)
			for (int idim = 0; idim < 3; ++idim)
			{
				offsets_[ilevel][idim] = (absoffsets_[ilevel][idim] / 2 - absoffsets_[ilevel - 1][idim]);
			}

		//... do a forward sweep to ensure that absolute offsets are also correct now
		for (unsigned ilevel = levelmin_ + 1; ilevel <= levelmax_; ++ilevel)
			for (int idim = 0; idim < 3; ++idim)
			{
				absoffsets_[ilevel][idim] = 2 * absoffsets_[ilevel - 1][idim] + 2 * offsets_[ilevel][idim];
			}

		for (unsigned ilevel = levelmin_ + 1; ilevel <= levelmax_; ++ilevel)
		{
			double h = 1.0 / (1ul << ilevel);

			x0_[ilevel] = h * (double)absoffsets_[ilevel][0];
			y0_[ilevel] = h * (double)absoffsets_[ilevel][1];
			z0_[ilevel] = h * (double)absoffsets_[ilevel][2];

			xl_[ilevel] = h * (double)len_[ilevel][0];
			yl_[ilevel] = h * (double)len_[ilevel][1];
			zl_[ilevel] = h * (double)len_[ilevel][2];
		}

		// do a consistency check that largest subgrid in zoom is not larger than half the box size
		for (unsigned ilevel = levelmin_ + 1; ilevel <= levelmax_; ++ilevel)
		{
			if (len_[ilevel][0] > index_t(1ul << (ilevel - 1)) ||
					len_[ilevel][1] > index_t(1ul << (ilevel - 1)) ||
					len_[ilevel][2] > index_t(1ul << (ilevel - 1)))
			{
				music::elog.Print("On level %d, subgrid is larger than half the box. This is not allowed!", ilevel);
				throw std::runtime_error("Fatal: Subgrid larger than half boxin zoom.");
			}
		}

		// update the region generator with what has been actually created
		vec3_t left = {x0_[levelmax_] + rshift_[0], y0_[levelmax_] + rshift_[1], z0_[levelmax_] + rshift_[2]};
		vec3_t right = {left[0] + xl_[levelmax_], left[1] + yl_[levelmax_], left[2] + zl_[levelmax_]};
		the_region_generator->update_AABB(left, right);
	}

	//! asignment operator
	refinement_hierarchy &operator=(const refinement_hierarchy &o)
	{
		levelmin_ = o.levelmin_;
		levelmax_ = o.levelmax_;
		padding_ = o.padding_;
		cf_ = o.cf_;
		align_top_ = o.align_top_;
		preserve_dims_ = o.preserve_dims_;
		for (int i = 0; i < 3; ++i)
		{
			x0ref_[i] = o.x0ref_[i];
			lxref_[i] = o.lxref_[i];
			xshift_[i] = o.xshift_[i];
			rshift_[i] = o.rshift_[i];
		}

		x0_ = o.x0_;
		y0_ = o.y0_;
		z0_ = o.z0_;
		xl_ = o.xl_;
		yl_ = o.yl_;
		zl_ = o.zl_;
		offsets_ = o.offsets_;
		absoffsets_ = o.absoffsets_;
		len_ = o.len_;
		margin_ = o.margin_;

		return *this;
	}

	/*! cut a grid level to the specified size
	 * @param ilevel grid level on which to perform the size adjustment
	 * @param nx new x-extent in fine grid cells
	 * @param ny new y-extent in fine grid cells
	 * @param nz new z-extent in fine grid cells
	 * @param oax new x-offset in units fine grid units
	 * @param oay new y-offset in units fine grid units
	 * @param oaz new z-offset in units fine grid units

	 */
	void adjust_level(unsigned ilevel, int nx, int ny, int nz, int oax, int oay, int oaz)
	{
		double h = 1.0 / (1 << ilevel);

		int dx, dy, dz;

		dx = absoffsets_[ilevel][0] - oax;
		dy = absoffsets_[ilevel][1] - oay;
		dz = absoffsets_[ilevel][2] - oaz;

		offsets_[ilevel][0] -= dx / 2;
		offsets_[ilevel][1] -= dy / 2;
		offsets_[ilevel][2] -= dz / 2;

		absoffsets_[ilevel][0] = oax;
		absoffsets_[ilevel][1] = oay;
		absoffsets_[ilevel][2] = oaz;

		len_[ilevel][0] = nx;
		len_[ilevel][1] = ny;
		len_[ilevel][2] = nz;

		x0_[ilevel] = h * oax;
		y0_[ilevel] = h * oay;
		z0_[ilevel] = h * oaz;

		xl_[ilevel] = h * nx;
		yl_[ilevel] = h * ny;
		zl_[ilevel] = h * nz;

		if (ilevel < levelmax_)
		{
			offsets_[ilevel + 1][0] += dx;
			offsets_[ilevel + 1][1] += dy;
			offsets_[ilevel + 1][2] += dz;
		}

		find_new_levelmin();
	}

	/*! determine level for which grid extends over entire domain */
	void find_new_levelmin(bool print = false)
	{
		unsigned old_levelmin(levelmin_);

		for (unsigned i = 0; i <= levelmax(); ++i)
		{
			unsigned n = 1 << i;

			if (absoffsets_[i] == index3_t({0, 0, 0}) && len_[i] == index3_t({n, n, n}))
			{
				levelmin_ = i;
			}
		}

		if ((old_levelmin != levelmin_) && print)
			music::ilog.Print("refinement_hierarchy: set new levelmin to %d", levelmin_);
	}

	//! get absolute grid offset for a specified level along a specified dimension (in fine grid units)
	index_t offset_abs(unsigned ilevel, int dim) const
	{
		return absoffsets_[ilevel][dim];
	}

	//! get relative grid offset for a specified level along a specified dimension (in coarser grid units)
	index_t offset(unsigned ilevel, int dim) const
	{
		return offsets_[ilevel][dim];
	}

	//! get grid size for a specified level along a specified dimension
	size_t size(unsigned ilevel, int dim) const
	{
		return len_[ilevel][dim];
	}

	//! get minimum grid level (the level for which the grid covers the entire domain)
	unsigned levelmin(void) const
	{
		return levelmin_;
	}

	//! get maximum grid level
	unsigned levelmax(void) const
	{
		return levelmax_;
	}

	//! get the total shift of the coordinate system in units of coarse cells
	int get_shift(int idim) const
	{
		return xshift_[idim];
	}

	//! get the margin reserved for isolated convolutions (-1=double pad)
	int get_margin() const
	{
		return margin_;
	}

	//! get the total shift of the coordinate system in box coordinates
	const double *get_coord_shift(void) const
	{
		return rshift_;
	}

	//! write refinement hierarchy to stdout
	void output(void) const
	{
		music::ilog << "-------------------------------------------------------------------------------" << std::endl;

		if (xshift_[0] != 0 || xshift_[1] != 0 || xshift_[2] != 0)
			music::ilog << " - Domain will be shifted by (" << xshift_[0] << ", " << xshift_[1] << ", " << xshift_[2] << ")\n"
									<< std::endl;

		music::ilog << " - Grid structure:\n";

		for (unsigned ilevel = levelmin_; ilevel <= levelmax_; ++ilevel)
		{
			music::ilog
					<< "     Level " << std::setw(3) << ilevel << " :   offset = (" << std::setw(5) << offsets_[ilevel][0] << ", " << std::setw(5) << offsets_[ilevel][1] << ", " << std::setw(5) << offsets_[ilevel][2] << ")\n"
					<< "               offset_abs = (" << std::setw(5) << absoffsets_[ilevel][0] << ", " << std::setw(5) << absoffsets_[ilevel][1] << ", " << std::setw(5) << absoffsets_[ilevel][2] << ")\n"
					<< "                   size   = (" << std::setw(5) << len_[ilevel][0] << ", " << std::setw(5) << len_[ilevel][1] << ", " << std::setw(5) << len_[ilevel][2] << ")\n";
		}
		music::ilog << "-------------------------------------------------------------------------------" << std::endl;
	}

	void output_log(void) const
	{
		music::ulog.Print("   Domain shifted by      (%5d,%5d,%5d)", xshift_[0], xshift_[1], xshift_[2]);
		for (unsigned ilevel = levelmin_; ilevel <= levelmax_; ++ilevel)
		{
			music::ulog.Print("   Level %3d :   offset = (%5d,%5d,%5d)", ilevel, offsets_[ilevel][0], offsets_[ilevel][1], offsets_[ilevel][2]);
			music::ulog.Print("                   size = (%5d,%5d,%5d)", len_[ilevel][0], len_[ilevel][1], len_[ilevel][2]);
		}
	}
};

typedef GridHierarchy<real_t> grid_hierarchy;
typedef MeshvarBnd<real_t> meshvar_bnd;
typedef Meshvar<real_t> meshvar;
