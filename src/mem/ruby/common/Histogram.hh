
/*
 * Copyright (c) 1999-2008 Mark D. Hill and David A. Wood
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * $Id$
 *
 * Description: The histogram class implements a simple histogram
 *
 */

#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include "mem/ruby/common/Global.hh"
#include "mem/gems_common/Vector.hh"

class Histogram {
public:
  // Constructors
  Histogram(int binsize = 1, int bins = 50);

  // Destructor
  ~Histogram();

  // Public Methods

  void add(int64 value);
  void add(const Histogram& hist);
  void clear() { clear(m_bins); }
  void clear(int bins);
  void clear(int binsize, int bins);
  int64 size() const { return m_count; }
  int getBins() const { return m_bins; }
  int getBinSize() const { return m_binsize; }
  int64 getTotal() const { return m_sumSamples; }
  int64 getData(int index) const { return m_data[index]; }

  void printWithMultiplier(ostream& out, double multiplier) const;
  void printPercent(ostream& out) const;
  void print(ostream& out) const;
private:
  // Private Methods

  // Private copy constructor and assignment operator
  //  Histogram(const Histogram& obj);
  //  Histogram& operator=(const Histogram& obj);

  // Data Members (m_ prefix)
  Vector<int64> m_data;
  int64 m_max;          // the maximum value seen so far
  int64 m_count;                // the number of elements added
  int m_binsize;                // the size of each bucket
  int m_bins;           // the number of buckets
  int m_largest_bin;      // the largest bin used

  int64 m_sumSamples;   // the sum of all samples
  int64 m_sumSquaredSamples; // the sum of the square of all samples

  double getStandardDeviation() const;
};

bool node_less_then_eq(const Histogram* n1, const Histogram* n2);

// Output operator declaration
ostream& operator<<(ostream& out, const Histogram& obj);

// ******************* Definitions *******************

// Output operator definition
extern inline
ostream& operator<<(ostream& out, const Histogram& obj)
{
  obj.print(out);
  out << flush;
  return out;
}

#endif //HISTOGRAM_H
