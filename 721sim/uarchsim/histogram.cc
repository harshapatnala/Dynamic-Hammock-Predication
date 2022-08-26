/*--------------------------------------------------------------------------*\
 | histogram.cc
 | Timothy Heil
 | 1/23/96
 |
 | Provides generic histogram class.
\*--------------------------------------------------------------------------*/

#include <cstdlib>
#include <cstdio>
//#include <stream.h>
#include <cassert>
#include <cmath>

#include "histogram.h"

HistogramClass::HistogramClass(int bins)
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
{
	int i;

	assert(bins>0);

	hist = new int[bins];
	assert(hist);

	len = bins;
	sum = 0;
	sumSq = 0;

	for (i=0; i<len; i++) {
		hist[i]=0;
	}
}

HistogramClass::~HistogramClass()
/*------------------------------------------------------------------------*\
 | Destructor.  Frees memory allocate by histogram.
\*------------------------------------------------------------------------*/
{
	delete [] hist;

	len =0;
}

void HistogramClass::Increment(int bin)
/*------------------------------------------------------------------------*\
 | Adds one sample to a specified bin.  The bin must be non-negative,
 |  but can be of any size.
\*------------------------------------------------------------------------*/
{
	assert (bin >= 0);

	sum = sum + bin;
	sumSq = sumSq + bin*bin;

	if (bin >=len) {
		bin = len -1;
	}

	hist[bin]++;
}

void HistogramClass::Add(int bin, int amount)
/*------------------------------------------------------------------------*\
 | Adds multiple samples to a specified bin.  The bin must be non-negative,
 |  but can be of any size.
\*------------------------------------------------------------------------*/
{
	unsigned long long lbin = bin;
	assert (bin >= 0);

	sum = sum + lbin*amount;
	sumSq = sumSq + lbin*lbin*amount;

	if (bin >=len) {
		bin = len -1;
	}

	hist[bin]+=amount;
}

void HistogramClass::Clear()
/*------------------------------------------------------------------------*\
 | Clears out the histogram for reuse.
\*------------------------------------------------------------------------*/
{
	int i;

	sum =0;
	sumSq = 0;
	for (i=0; i<len; i++) {
		hist[i]=0;
	}
}

int HistogramClass::Bin(int bin)
/*------------------------------------------------------------------------*\
 | Returns the number of samples added to a bin.  The bin must be
 |  non-negative, but can be of any size.
\*------------------------------------------------------------------------*/
{
	assert (bin>=0);

	if (bin >= len) {
		bin = len -1;
	}
	return(hist[bin]);
}

int HistogramClass::Samples()
/*------------------------------------------------------------------------*\
 | Returns the total number of samples added to all bins.
\*------------------------------------------------------------------------*/
{
	int samp, i;

	for(samp = 0,i=0; i<len; samp = samp + hist[i], i++);

	return(samp);
}

unsigned long long HistogramClass::Sum()
/*------------------------------------------------------------------------*\
 | Returns the sum of all sample bin values added to the histogram.
 | Mathematically, this is the sum of the bin times the number of samples
 | in the bin.  However, due to histogram truncation, this does not work
 | computationally.  The histogram class specifically sums the values
 | of all samples added to the histogram, to avoid this problem.
 |
 | The average of the sample values is Sum()/Samples();
\*------------------------------------------------------------------------*/
{
	return(sum);
}

unsigned long long HistogramClass::SumSq()
/*------------------------------------------------------------------------*\
 | Like Sum(), returns the sum of all the sample values squared.  This is
 | useful in calculating the variance and standard deviation of the
 | sample values.  Again, the sum of the squares is totaled as the histogram
 | is updated, to avoid histogram truncation problems.
 |
 | The variance is ( Samples()*SumSq()-Sum()*Sum() ) / Samples()/Samples();
 | The standard deviation is the square root of the variance.
\*------------------------------------------------------------------------*/
{
	return(sumSq);
}

double HistogramClass::Average()
/*------------------------------------------------------------------------*\
 | Returns the average of the sample values, calculated as explained under
 |  Sum().
\*------------------------------------------------------------------------*/
{
	return( ((double) sum)/Samples() );
}

double HistogramClass::Variance()
/*------------------------------------------------------------------------*\
 | Returns the variance of the sample values, calculated as explained under
 | SumSq().
\*------------------------------------------------------------------------*/
{
	double samp;

	samp = Samples();

	return( ( samp * sumSq - ((double) sum) * sum ) / (samp * samp) );
}

void HistogramClass::Print(FILE* fp, unsigned int norm_value)
/*------------------------------------------------------------------------*\
 | Prints out the histogram data to the output stream specified.
\*------------------------------------------------------------------------*/
{
	int i;

	if (norm_value) {
		for (i=0; i<len; i++) {
			fprintf(fp, "%d\t%f\n", i, (double)hist[i]/(double)norm_value);
		}
	}
	else {
		for (i=0; i<len; i++) {
			fprintf(fp, "%d\t%d\n", i, hist[i]);
		}
	}
}

#if 0
void HistogramClass::PrintStat(ostream& out)
/*------------------------------------------------------------------------*\
 | Prints out Samples(), Sum(), SumSq(), Average(), and standard deviation
 |  to the output stream specified.
\*------------------------------------------------------------------------*/
{
	out << "Samples           : " << Samples()        << "\n";
	out << "Sum               : " << Sum()            << "\n";
	out << "Sum of Squares    : " << SumSq()          << "\n";
	out << "Average           : " << Average()        << "\n";
	out << "Standard Deviation: " << sqrt(Variance()) << "\n";
}
#endif
