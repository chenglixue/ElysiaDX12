#pragma once
#include "stdafx.h"

namespace ElysiaHelper
{
	class SobolSequenceGenerator
	{
	public:
		SobolSequenceGenerator() = default;
        explicit SobolSequenceGenerator(uint32_t dimensions);
		SobolSequenceGenerator(const SobolSequenceGenerator&) = delete;
		SobolSequenceGenerator& operator=(const SobolSequenceGenerator&) = delete;
		SobolSequenceGenerator(SobolSequenceGenerator&&) = delete;
		SobolSequenceGenerator& operator=(SobolSequenceGenerator&&) = delete;


		static SobolSequenceGenerator& GetInstance()
		{
			std::call_once(m_initInstanceFlag, []() {
				m_instance.reset(new SobolSequenceGenerator());
				});

			return *m_instance;
		}

        uint32_t* rightmostZeroBit(uint32_t N) const;

        std::vector<double> nextPoint(uint32_t changed_bit);

        void reset();

        uint64_t getCurrentIndex() const;
        uint32_t getDimensions() const;

        void printDirectionNumbers(uint32_t dim, uint32_t count = 10) const;

	private:
		static std::unique_ptr<SobolSequenceGenerator> m_instance;
		static std::once_flag m_initInstanceFlag;

		// Maximum supported dimensions
		static constexpr uint32_t MAX_DIMENSIONS = 10; // Simplified for this example
		static constexpr uint32_t MAX_BITS = 32;

		/**
		 * @struct PrimitivePolynomial
		 * @brief Stores primitive polynomial coefficients and degree
		 */
		struct PrimitivePolynomial 
		{
			uint32_t degree;        // Degree of the polynomial
			uint32_t coefficients;  // Binary coefficients (bit-packed)
			std::vector<uint32_t> initial_m; // Initial direction numbers
		};

		/**
		 * Pre-computed primitive polynomials for first 10 dimensions
		 * Format: {degree, coefficients, {initial_m_values...}}
		 *
		 * Coefficients are stored as binary numbers where bit i represents
		 * the coefficient of x^i term (excluding the leading x^degree term)
		 */
		static const std::vector<PrimitivePolynomial> primitive_polynomials;

		uint32_t dimensions_;           // Number of dimensions
		uint64_t current_index_;       // Current sequence index
		std::vector<std::vector<uint32_t>> direction_numbers_; // Direction numbers for each dimension
		std::vector<uint32_t> current_point_;  // Current point (as integers)

        /**
         * @brief Initializes direction numbers for all dimensions
         *
         * Direction numbers are computed using the recurrence relation based on
         * primitive polynomials. The first few direction numbers are given as
         * initial values, and the rest are computed using the recurrence.
         */
        void initializeDirectionNumbers() 
        {
            direction_numbers_.resize(dimensions_);

            for (uint32_t dim = 0; dim < dimensions_; ++dim) 
            {
                direction_numbers_[dim].resize(MAX_BITS);

                if (dim == 0) 
                {
                    // First dimension is special - uses Van der Corput sequence in base 2
                    // Direction numbers are powers of 2: 2^31, 2^30, 2^29, ...
                    for (uint32_t i = 0; i < MAX_BITS; ++i) 
                    {
                        direction_numbers_[dim][i] = 1U << (31 - i);
                    }
                }
                else 
                {
                    // For other dimensions, use primitive polynomials
                    const auto& poly = primitive_polynomials[dim - 1];

                    // Set initial direction numbers (given values, left-shifted appropriately)
                    // The initial direction numbers are scaled by 2^(32-i)
                    // effectively, the recurrence relation that was:
                    // m_i = 2*a_1*m_{i-1} XOR 2^2*a_2*m_{i-2} XOR ... XOR 2^{k-1}*a_{k-1}*m_{i-(k-1)} XOR 2^k*m_{i-k} XOR m_{i-k}
                    // is now converted to:
                    // m_i = a_1*m_{i-1} XOR a_2*m_{i-2} XOR ... XOR m_{i-k} XOR m_{i-k}/2^k
                    // k is the degree of the polynomial
                    // Therefore, m_i = u_i * 2^(32-i) where u_i is the original direction number
                    // we skip the step v_i = m_i / 2^i so that we can efficiently store
                    // the direction numbers as integers in [0, 2^32)
                    // Once the real points are computed, we scale them back to [0, 1) {see line 167}
                    for (uint32_t i = 0; i < poly.initial_m.size() && i < MAX_BITS; ++i) 
                    {
                        direction_numbers_[dim][i] = poly.initial_m[i] << (31 - i);
                    }

                    // Compute remaining direction numbers using recurrence relation
                    // m_i = a_1*m_{i-1} XOR a_2*m_{i-2} XOR ... XOR m_{i-k} XOR m_{i-k}/2^k
                    // k is the degree of the polynomial
                    for (uint32_t i = poly.degree; i < MAX_BITS; ++i) 
                    {
                        // m_{i-k} XOR m_{i-k}/2^k
                        uint32_t m = direction_numbers_[dim][i - poly.degree] ^ (direction_numbers_[dim][i - poly.degree] >> poly.degree);

                        for (uint32_t j = 1; j < poly.degree; ++j) 
                        {
                            if (poly.coefficients & (1U << (j - 1))) 
                            {
                                // XOR m_{i-1} ... XOR m_{i-(k-1)}
                                m ^= direction_numbers_[dim][i - j];
                            }
                        }

                        direction_numbers_[dim][i] = m;
                    }
                }
            }
        }
	};

    /// <summary>
    /// 
    /// </summary>
    /// <param name="count"> Number of points to generate </param>
    inline std::vector<Vector2> Create2DSobolSqeuence(UINT32 count)
    {
        std::vector<Vector2> o(count);
        SobolSequenceGenerator sobol(2);

#if _DEBUG
        std::cout << "First 16 points of 2D Sobol sequence:" << std::endl;
        std::cout << "Index\tX\t\tY" << std::endl;
        std::cout << "-----\t--------\t--------" << std::endl;
#endif

        uint32_t* C = sobol.rightmostZeroBit(count);
        for (int i = 0; i < count; ++i)
        {
            auto point = sobol.nextPoint(C[i]);

#if _DEBUG
            std::cout << i << "\t" << std::fixed << std::setprecision(6)
                << point[0] << "\t" << point[1] << std::endl;
#endif
            o[i].x = point[0];
            o[i].y = point[1];
        }

        return o;

    }
}