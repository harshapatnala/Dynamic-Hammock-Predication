#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include <iostream>

/*--------------------------------------------------------------------------*\
 | histogram.h
 | Timothy Heil
 | 1/23/96
 |
 | Provides generic histogram class.
\*--------------------------------------------------------------------------*/

class HistogramClass
{
public:
	HistogramClass(int bins);
	/*------------------------------------------------------------------------*\
	 | Constructor.  Creates a histogram with bins number of bins.
	 |  Bins are all initially zero.  The last bin is used for all higher
	 |  bins.  Thus, HistogramClass(5) will create a histogram with bins for
	 |  0,1,2,3, and 4 and higher sample values.
	 |
	 | Bin numbers should NOT be reduced to the number of bins in the
	 |  histogram before adding sample values to the histogram.  The
	 |  HistogramClass will do this for you.
	\*------------------------------------------------------------------------*/

	~HistogramClass();
	/*------------------------------------------------------------------------*\
	 | Destructor.  Frees memory allocate by histogram.
	\*------------------------------------------------------------------------*/

	void Increment(int bin);
	/*------------------------------------------------------------------------*\
	 | Adds one sample to a specified bin.  The bin must be non-negative,
	 |  but can be of any size.
	\*------------------------------------------------------------------------*/

	void Add(int bin, int amount);
	/*------------------------------------------------------------------------*\
	 | Adds multiple samples to a specified bin.  The bin must be non-negative,
	 |  but can be of any size.
	\*------------------------------------------------------------------------*/

	void Clear();
	/*------------------------------------------------------------------------*\
	 | Clears out the histogram for reuse.
	\*------------------------------------------------------------------------*/

	int Bin(int bin);
	/*------------------------------------------------------------------------*\
	 | Returns the number of samples added to a bin.  The bin must be
	 |  non-negative, but can be of any size.
	\*------------------------------------------------------------------------*/

	int Samples();
	/*------------------------------------------------------------------------*\
	 | Returns the total number of samples added to all bins.
	\*------------------------------------------------------------------------*/

	unsigned long long Sum();
	/*------------------------------------------------------------------------*\
	 | Returns the sum of all sample bin values added to the histogram.
	 | Mathematically, this is the sum of the bin times the number of samples
	 | in the bin.  However, due to histogram truncation, this does not work
	 | computationally.  The histogram class specifically sums the values
	 | of all samples added to the histogram, to avoid this problem.
	 |
	 | The average of the sample values is Sum()/Samples();
	\*------------------------------------------------------------------------*/

	unsigned long long SumSq();
	/*------------------------------------------------------------------------*\
	 | Like Sum(), returns the sum of all the sample values squared.  This is
	 | useful in calculating the variance and standard deviation of the
	 | sample values.  Again, the sum of the squares is totaled as the histogram
	 | is updated, to avoid histogram truncation problems.
	 |
	 | The variance is ( Samples()*SumSq()-Sum()*Sum() ) / Samples()/Samples();
	 | The standard deviation is the square root of the variance.
	\*------------------------------------------------------------------------*/

	double Average();
	/*------------------------------------------------------------------------*\
	 | Returns the average of the sample values, calculated as explained under
	 |  Sum().
	\*------------------------------------------------------------------------*/

	double Variance();
	/*------------------------------------------------------------------------*\
	 | Returns the variance of the sample values, calculated as explained under
	 | SumSq().
	\*------------------------------------------------------------------------*/

	void Print(FILE* fp, unsigned int norm_value = 0);
	/*------------------------------------------------------------------------*\
	 | Prints out the histogram data to the output stream specified.
	\*------------------------------------------------------------------------*/

#if 0
	void PrintStat(ostream& out);
	/*------------------------------------------------------------------------*\
	 | Prints out Samples(), Sum(), SumSq(), Average(), and standard deviation
	 |  to the output stream specified.
	\*------------------------------------------------------------------------*/
#endif


private:
	int* hist;   /* Actually contains the histogram data.                   */
	int len;     /* Number of bins in the histogram data.                   */
	unsigned long long sum;    /* Keeps a running sum of the sampled data.  */
	unsigned long long sumSq;  /* Keeps a running sum of the squares of the
			      * sampled data. */

};


#endif //HISTOGRAM_H
