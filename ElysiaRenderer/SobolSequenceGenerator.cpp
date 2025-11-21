#include "stdafx.h"
#include "SobolSequenceGenerator.h"

namespace ElysiaHelper
{
	std::unique_ptr<SobolSequenceGenerator> SobolSequenceGenerator::m_instance;
	std::once_flag SobolSequenceGenerator::m_initInstanceFlag;

    /**
    * @brief Constructor
    * @param dimensions Number of dimensions (must be <= MAX_DIMENSIONS)
    * @throws std::invalid_argument if dimensions is 0 or exceeds maximum
    */
    SobolSequenceGenerator::SobolSequenceGenerator(uint32_t dimensions)
        : dimensions_(dimensions), current_index_(0)
    {
        if (dimensions == 0) {
            throw std::invalid_argument("Dimensions must be at least 1");
        }
        if (dimensions > MAX_DIMENSIONS) {
            throw std::invalid_argument("Too many dimensions requested");
        }

        initializeDirectionNumbers();
        current_point_.resize(dimensions_, 0);
    }

    /**
         * @brief Function to provide Rightmost zero bit
         * @param N Number of points to generate
         * @return Array of uint32_t containing the rightmost zero bit for each index
        */
    uint32_t* SobolSequenceGenerator::rightmostZeroBit(uint32_t N) const
    {
        // Count trailing ones to find the rightmost zero
        uint32_t* C = new uint32_t[N];
        C[0] = 1;
        for (uint32_t i = 1; i <= N - 1; i++)
        {
            C[i] = 1;
            uint32_t value = i;
            while (value & 1)
            {
                value >>= 1;
                C[i]++;
            }
        }
        return C;
    }

    /**
     * @brief Generates the next point in the Sobol sequence
     * @return Vector of doubles in [0, 1) representing the next point
     *
     * Uses Gray code optimization: instead of computing the entire point from scratch,
     * we XOR the previous point with a single direction number corresponding to
     * the bit that changed in the Gray code representation.
     */
    std::vector<double> SobolSequenceGenerator::nextPoint(uint32_t changed_bit) 
    {
        if (current_index_ == 0) 
        {
            // First point is always the origin
            current_index_++;
            return std::vector<double>(dimensions_, 0.0);
        }

        // Update each dimension by XORing with the appropriate direction number
        std::vector<double> point(dimensions_);
        for (uint32_t dim = 0; dim < dimensions_; ++dim)
        {
            current_point_[dim] ^= direction_numbers_[dim][changed_bit];
            // Convert to double in [0, 1) by dividing by 2^32
            point[dim] = static_cast<double>(current_point_[dim]) / (1ULL << 32);
        }

        current_index_++;
        return point;
    }

    /**
     * @brief Resets the sequence to the beginning
     */
    void SobolSequenceGenerator::reset() 
    {
        current_index_ = 0;
        std::fill(current_point_.begin(), current_point_.end(), 0);
    }

    /**
    * @brief Gets the current index in the sequence
    * @return Current index (0-based)
    */
    uint64_t SobolSequenceGenerator::getCurrentIndex() const
    {
        return current_index_;
    }

    /**
     * @brief Gets the number of dimensions
     * @return Number of dimensions
     */
    uint32_t SobolSequenceGenerator::getDimensions() const
    {
        return dimensions_;
    }

    /**
     * @brief Utility function to print direction numbers (for debugging)
     * @param dim Dimension to print (0-indexed)
     * @param count Number of direction numbers to print
     */
    void SobolSequenceGenerator::printDirectionNumbers(uint32_t dim, uint32_t count) const
    {
        if (dim >= dimensions_) 
        {
            std::cout << "Invalid dimension" << std::endl;
            return;
        }

        /*std::cout << "Direction numbers for dimension " << dim << ":" << std::endl;
        for (uint32_t i = 0; i < std::fmin(count, MAX_BITS); ++i) 
        {
            std::cout << "v[" << i << "] = " << std::hex << std::setw(8) << std::setfill('0')
                << direction_numbers_[dim][i] << std::dec << std::endl;
        }*/
    }

    // Static member definition - primitive polynomials for dimensions 2-11
    // Each entry contains: {degree, coefficients, {initial direction numbers}}
    const std::vector<SobolSequenceGenerator::PrimitivePolynomial> SobolSequenceGenerator::primitive_polynomials =
    {
        // Dimension 2: x^1 + 1 (coefficients = 0, since no middle terms)
        {1, 0, {1}},

        // Dimension 3: x^2 + x + 1 (coefficients = 1, representing the x term)
        {2, 1, {1, 3}},

        // Dimension 4: x^3 + x + 1
        {3, 1, {1, 3, 1}},

        // Dimension 5: x^3 + x^2 + 1
        {3, 2, {1, 1, 1}},

        // Dimension 6: x^4 + x + 1
        {4, 1, {1, 1, 3, 3}},

        // Dimension 7: x^4 + x^3 + 1
        {4, 4, {1, 3, 5, 13}},

        // Dimension 8: x^5 + x^2 + 1
        {5, 2, {1, 1, 5, 5, 17}},

        // Dimension 9: x^5 + x^4 + x^2 + x + 1
        {5, 11, {1, 1, 5, 5, 5}},

        // Dimension 10: x^5 + x^4 + x^3 + x + 1
        {5, 13, {1, 3, 15, 17, 63}}
    };
}